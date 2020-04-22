//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"
#include "vportalleaderboard.h"
#include "vportalleaderboardhud.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "vgui/ilocalize.h"
#include "vgui_controls/scrollbar.h"
#include "vgui_controls/imagepanel.h"
#include "gameui/portal2/vdialoglistbutton.h"
#include "vpvplobby.h"
#include "vgui/portal_leaderboard_graph_panel.h"
#include "gameui/portal2/vportalchallengestatspanel.h"
#include "GameUI_Interface.h"
#include "c_user_message_register.h"
#include "c_portal_player.h"
#include "vgui/IInput.h"

#include "portal_mp_gamerules.h"
#include "gamerules.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#ifdef XBX_GetPrimaryUserId
	#undef XBX_GetPrimaryUserId
#endif


// Uncomment this to generate global leaderboard Steam config data
//#define BUILD_GLOBAL_LEADERBOARD_STEAM_CONFIG "D:/dev/portal2/rel_pc/portal2_leaderboard_steamconfig.txt"

#ifdef BUILD_GLOBAL_LEADERBOARD_STEAM_CONFIG
	#include "filesystem.h"
#endif // BUILD_GLOBAL_LEADERBOARD_STEAM_CONFIG


#define AVATAR_PIC_MOUSE_HOVER_ALPHA 255
#define AVATAR_PIC_MOUSE_NO_HOVER_ALPHA 225

// holds the score players achieve at the end of a level
int g_nPortalScoreTempUpdate = -1;
int g_nTimeScoreTempUpdate = -1;
int g_nPortalScoreTempMostRecent = -1;
int g_nTimeScoreTempMostRecent = -1;

ConVar cl_leaderboard_fake_offline( "cl_leaderboard_fake_offline", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_leaderboard_fake_no_data( "cl_leaderboard_fake_no_data", false, FCVAR_DEVELOPMENTONLY );
ConVar cl_leaderboard_fake_io_error( "cl_leaderboard_fake_io_error", false, FCVAR_DEVELOPMENTONLY );

// Destroying the lobby without any confirmations
static void LeaveLobbyImpl()
{
	g_pMatchFramework->CloseSession();
	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}

//=============================================================================
static void LeaveGameOkCallback()
{
	COM_TimestampedLog( "Exit Game" );

	BaseModUI::WINDOW_TYPE windowType;
	
	if ( GameRules() && GameRules()->IsMultiplayer() )
	{
		windowType = WT_PORTALCOOPLEADERBOARD;
	}
	else
	{
		windowType = WT_PORTALLEADERBOARD;
	}

	CPortalLeaderboardPanel* self = 
		static_cast< CPortalLeaderboardPanel* >( CBaseModPanel::GetSingleton().GetWindow( windowType ) );

	if ( self )
	{
		self->Close();
	}

	GameUI().HideGameUI();

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

	GameUI().ActivateGameUI();
	GameUI().AllowEngineHideGameUI();

	LeaveLobbyImpl();
}

//=============================================================================
static void LeaveGameCancelCallback()
{
	CBaseModFrame *pInGameMenu = CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_INGAMEMAINMENU );
	KeyValues *pLeaderboardValues = new KeyValues( "leaderboard" );
	KeyValues::AutoDelete autodelete_pLeaderboardValues( pLeaderboardValues );
	pLeaderboardValues->SetInt("state", STATE_END_OF_LEVEL);
	
	BaseModUI::WINDOW_TYPE windowType;

	if ( GameRules() && GameRules()->IsMultiplayer() )
	{
		windowType = WT_PORTALCOOPLEADERBOARD;
	}
	else
	{
		windowType = WT_PORTALLEADERBOARD;
	}

	BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( windowType, pInGameMenu, true, pLeaderboardValues );
}

//=============================================================================
static void LeaveGameHubCancelCallback()
{
	CBaseModFrame *pInGameMenu = CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_INGAMEMAINMENU );
	KeyValues *pLeaderboardValues = new KeyValues( "leaderboard" );
	KeyValues::AutoDelete autodelete_pLeaderboardValues( pLeaderboardValues );
	pLeaderboardValues->SetInt("state", STATE_START_OF_LEVEL);

	BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( WT_PORTALCOOPLEADERBOARD, pInGameMenu, true, pLeaderboardValues );
}



namespace BaseModUI
{

//=============================================================================
CLeaderboardMapItem::CLeaderboardMapItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName ),
	m_pListCtrlr( ( GenericPanelList * )pParent )
{
	m_pLeaderboard = dynamic_cast< CPortalLeaderboardPanel * >( m_pListCtrlr->GetParent() );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_nChapterNumber = 0;
	m_nMapIndex = -1;
	m_nMapNumber = 0;
	m_bLocked = false;
	m_hTextFont = vgui::INVALID_FONT;

	m_nTextOffsetY = 0;

	m_bSelected = false;
	m_bHasMouseover = false;
}


//=============================================================================	
void CLeaderboardMapItem::SetChapterAndMapInfo( int nChapterNumber, int nMapNumber, int nMapIndex, int nUnlockedChapters /*= -1*/ )
{
	// make sure all numbers are at least valid on the low end
	if ( nChapterNumber <= 0 || nMapNumber <= 0 || nMapIndex < 0 )
		return;

	m_nChapterNumber = nChapterNumber;
	m_nMapNumber = nMapNumber;
	m_nMapIndex = nMapIndex;

	Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblChapterName" ) );
	if( !pLabel )
		return;

	wchar_t *pMapTitle;
	
	if ( m_pLeaderboard->IsSinglePlayer() )	  // single player
	{
		if ( nChapterNumber >= nUnlockedChapters )
		{
			pMapTitle = g_pVGuiLocalize->Find( "#GameUI_Achievement_Locked" );
			m_bLocked = true;
		}
		else
		{
			pMapTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_MAP_NAME_CH%d_MAP%d", m_nChapterNumber, m_nMapNumber ) );
		}
		
	}
	else   // multi player
	{
		const char* szStatName = CFmtStr( "MP.complete.%s", CBaseModPanel::GetSingleton().GetMapName( nChapterNumber, nMapNumber, false ) );
		bool bUnlocked = false;

		// If we can use the game rules, then we're in-game and compare the maps complete for both players.
		CPortalMPGameRules* pMPGameRules = PortalMPGameRules();
		if( pMPGameRules && engine->IsConnected() )
		{
			// IsLevelInBranchComplete checks if either player has the specified map unlocked
			bUnlocked = pMPGameRules->IsLevelInBranchComplete( nChapterNumber-1, nMapNumber-1 );
		}
		else
		{
			for ( DWORD iUserSlot = 0; iUserSlot < XBX_GetNumGameUsers() && !bUnlocked; ++ iUserSlot )
			{
				int iCtrlr = XBX_GetUserId( iUserSlot );
				IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );

				TitleDataFieldsDescription_t const *pFields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

				if ( TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( pFields, szStatName ) )
				{
					bUnlocked |= TitleDataFieldsDescriptionGetBit( pField, pPlayerLocal );
				}
			}
		}

		if ( bUnlocked )
		{
			pMapTitle= g_pVGuiLocalize->Find( CFmtStr( "#COOP_MAP_NAME_TR%d_MAP%d", m_nChapterNumber, m_nMapNumber ) );
		}
		else
		{
			pMapTitle = g_pVGuiLocalize->Find( "#GameUI_Achievement_Locked" );
			m_bLocked = true;
		}
	}

	pLabel->SetText( pMapTitle );
}

//=============================================================================
void CLeaderboardMapItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/newgame_chapteritem.res" );

	const char *pDefaultFontString = pScheme->GetResourceString( "HybridButton.Font" );
	const char *pStyle = "DialogListButton";
	const char *pFontString = pScheme->GetResourceString( CFmtStr( "%s.Font", pStyle ) );
	m_hTextFont = pScheme->GetFont( pFontString[0] ? pFontString : pDefaultFontString, true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
	m_LockedColor = GetSchemeColor( "HybridButton.LockedColor", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColor", pScheme );
	m_LostFocusColor = Color( 120, 120, 120, 255 );
	m_BaseColor = Color( 255, 255, 255, 0 );

	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "NewGameDialog.TextOffsetY" ) ) );
}

//=============================================================================
void CLeaderboardMapItem::PaintBackground()
{
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're highlighted, background
	if ( HasFocus() )
	{
		surface()->DrawSetColor( m_CursorColor );
	}
	else if ( IsSelected() )
	{
		surface()->DrawSetColor( m_LostFocusColor );
	}
	else if ( HasMouseover() )
	{
		surface()->DrawSetColor( m_MouseOverCursorColor );
	}
	else
	{
		surface()->DrawSetColor( m_BaseColor );
	}
	surface()->DrawFilledRect( 0, 0, wide, tall );

	DrawListItemLabel( dynamic_cast< vgui::Label * >( FindChildByName( "LblChapterName" ) ) );
}


//=============================================================================
void CLeaderboardMapItem::OnCursorEntered()
{ 
	SetHasMouseover( true );

	if ( IsPC() )
		return;

	if ( GetParent() )
		GetParent()->NavigateToChild( this );
	else
		NavigateTo();
}


//=============================================================================
void CLeaderboardMapItem::NavigateTo( void )
{
	m_pListCtrlr->SelectPanelItemByPanel( this );

	if ( m_pLeaderboard )
	{
		m_pLeaderboard->SelectPanel( SELECT_MAP );
	}

	SetHasMouseover( true );
	RequestFocus();
	int nNumPanels = m_pListCtrlr->GetPanelItemCount();
	for ( int i = 0; i < nNumPanels; ++i )
	{
		CLeaderboardMapItem *pPanel = static_cast< CLeaderboardMapItem *>( m_pListCtrlr->GetPanelItem( i ) );
		if ( pPanel )
		{
			pPanel->SetSelected( false );
		}
	}
	SetSelected( true );
	m_pLeaderboard->SetMapIndex( m_nMapIndex );
	m_pLeaderboard->UpdateLeaderboards();

	BaseClass::NavigateTo();
}


//=============================================================================
void CLeaderboardMapItem::NavigateFrom( void )
{
	SetHasMouseover( false );
	// get the parent
	if ( m_pLeaderboard )
	{
		if ( m_pLeaderboard->GetCurrentChapterNumber() == m_nChapterNumber && m_pLeaderboard->GetCurrentMapIndex() == m_nMapIndex )
			SetSelected( true );
	}

	BaseClass::NavigateFrom();

	if ( IsGameConsole() )
	{
		OnClose();
	}
}


//=============================================================================
void CLeaderboardMapItem::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_UP:
	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
		// always clear the current selected stat
		m_pLeaderboard->ClearAvatarSelection();
		// if the top one, go to the chapter selection gizmo
		if ( m_nMapIndex == 0 )
		{
			m_pLeaderboard->SelectPanel( SELECT_CHAPTER );
			return;
		}
		break;

	case KEY_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
		// always clear the current selected stat
		m_pLeaderboard->ClearAvatarSelection();
		// if not the bottom map, act normally
		if ( m_nMapIndex != m_pListCtrlr->GetPanelItemCount() - 1 )
			break;
		// the bottom map will not navigate further
		return;

	case KEY_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
		if ( m_pLeaderboard && IsPC() )
		{
			m_pLeaderboard->SelectPanel( SELECT_LEADERBOARD );
		}
		else if ( m_pLeaderboard && IsX360() )
		{
			m_pLeaderboard->SelectPanel( SELECT_AVATAR, true );
		}
		return;

	case KEY_ENTER:
	case KEY_XBUTTON_A:
		if ( m_pLeaderboard )
		{
			m_pLeaderboard->OnKeyCodePressed( code );
			return;
		}
		break;
	}

	m_pLeaderboard->UpdateFooter();

	BaseClass::OnKeyCodePressed( code );
}


//=============================================================================
void CLeaderboardMapItem::OnMousePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();

		Assert( m_pLeaderboard );
		if( m_pLeaderboard )
		{
			m_pLeaderboard->UpdateFooter();
		}
		return;
	}
	BaseClass::OnMousePressed( code );
	m_pLeaderboard->UpdateFooter();
}


//=============================================================================
void CLeaderboardMapItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		CLeaderboardMapItem* pListItem = static_cast< CLeaderboardMapItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}

		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}


//=============================================================================
void CLeaderboardMapItem::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );
		panel->SetMouseInputEnabled( false );
	}
}


//=============================================================================
void CLeaderboardMapItem::DrawListItemLabel( vgui::Label *pLabel )
{
	if ( !pLabel )
		return;

	bool bHasFocus = HasFocus() || IsSelected();

	// set whether locked or normal text color
	Color textColor = m_bLocked ? m_LockedColor : m_TextColor;
	// keep same color already set if not in focus
	textColor = bHasFocus ? m_FocusColor : textColor;

	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	int x, y, labelWide, labelTall;
	pLabel->GetBounds( x, y, labelWide, labelTall );

	wchar_t szUnicode[512];
	pLabel->GetText( szUnicode, sizeof( szUnicode ) );
	int len = V_wcslen( szUnicode );

	int textWide, textTall;
	surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

	// vertical center
	y += ( labelTall - textTall ) / 2 + m_nTextOffsetY;

	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( textColor );
	vgui::surface()->DrawPrintText( szUnicode, len );
}


//=============================================================================
void CLeaderboardMapItem::SetHasMouseover( bool bHasMouseover )
{
	if ( bHasMouseover )
	{
		for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
		{
			CLeaderboardMapItem *pItem = dynamic_cast< CLeaderboardMapItem* >( m_pListCtrlr->GetPanelItem( i ) );
			if ( pItem && pItem != this )
			{
				pItem->SetHasMouseover( false );
			}
		}
	}
	m_bHasMouseover = bHasMouseover;
}

//=============================================================================
CPortalLeaderboardPanel::CPortalLeaderboardPanel( Panel *pParent, const char *pPanelName, bool bSinglePlayer ):
BaseClass( pParent, pPanelName ), m_autodelete_pResourceLoadConditions( (KeyValues*)NULL )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	m_pMapList = new GenericPanelList( this, "MapList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pMapList->SetPaintBackgroundEnabled( false );

	m_pStatList = new GenericPanelList( this, "StatList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pStatList->SetPaintBackgroundEnabled( false );

	m_pChallengeStatsPanel = new CPortalChallengeStatsPanel( this, "PortalChallengeStatsPanel" );
	m_pChallengeStatsPanel->SetVisible( false );

	V_strncpy( m_ResourceName, "Resource/UI/BaseModUI/portalleaderboard.res",sizeof( m_ResourceName ) );

	Assert( !m_pResourceLoadConditions );
	m_pResourceLoadConditions = new KeyValues( m_ResourceName );
	m_autodelete_pResourceLoadConditions.Assign( m_pResourceLoadConditions );

	V_snprintf( m_szNextMap, sizeof(m_szNextMap), "" );

	m_nCurrentChapterNumber = 1;
	m_nMapIndex = 0;
	m_nUnlockedSPChapters = 0;
	m_pLeaderboard = NULL;
	m_pChapterListButton = NULL;
	m_pLeaderboardListButton = NULL;
	m_pPortalGraph = NULL;
	m_pTimeGraph = NULL;
	m_pInvalidLabel = NULL;
	m_pInvalidLabel2 = NULL;
	m_pWorkingAnim = NULL;
	m_pEveryoneLabel = NULL;
	m_CurrentLeaderboardType = LEADERBOARD_PORTAL;
	m_bMapPanelSelected = true;
	m_currentSelection = SELECT_CHAPTER;

	m_bNeedsUpdate = false;
	m_bNeedsMapItemSelect = false;
	m_leaderboardState = STATE_PAUSE_MENU;
	m_bSinglePlayerMode = bSinglePlayer;
	m_bCheated = false;
	m_bOnline = false;
	m_bCommittedAction = false;

	if ( !m_bSinglePlayerMode )
	{
		m_pResourceLoadConditions->SetInt( "?online", 1 );
	}

	SetDialogTitle( "#L4D360UI_MainMenu_SurvivalLeaderboards", NULL, false, 0, 0, 4 );

	SetFooterEnabled( true );
	UpdateFooter();

	m_pPortalGraph = new CPortalLeaderboardGraphPanel( this, "PortalGraph", LEADERBOARD_PORTAL );
	if ( m_pPortalGraph )
	{
		m_pPortalGraph->SetVisible( false );
		m_pPortalGraph->SetKeyBoardInputEnabled( false );
	}

	m_pTimeGraph = new CPortalLeaderboardGraphPanel( this, "TimeGraph", LEADERBOARD_TIME );
	if ( m_pTimeGraph )
	{
		m_pTimeGraph->SetVisible( false );
		m_pTimeGraph->SetKeyBoardInputEnabled( false );
	}
}

//=============================================================================
CPortalLeaderboardPanel::~CPortalLeaderboardPanel()
{
	if ( m_pMapList )
	{
		delete m_pMapList;
	}

	if ( m_pStatList )
	{
		delete m_pStatList;
	}
	
	if ( m_pChallengeStatsPanel )
	{
		delete m_pChallengeStatsPanel;
	}
}


//=============================================================================
void CPortalLeaderboardPanel::MsgPreChangeLevel()
{
	engine->ServerCmd( "pre_go_to_hub");
	PostMessage( this, new KeyValues( "MsgChangeLevel" ), 1.0f );
}

//=============================================================================
void CPortalLeaderboardPanel::MsgChangeLevel()
{
	char szCmd[64];
	V_snprintf( szCmd, sizeof(szCmd), "mp_select_level %s", GetCurrentMapName() );
	engine->ServerCmd( szCmd );
}

void CPortalLeaderboardPanel::MsgGoToNext()
{
	if ( m_szNextMap[ 0 ] != '\0')
	{
		engine->ServerCmd( VarArgs( "select_map %s\n", m_szNextMap ) );
		GameUI().AllowEngineHideGameUI();
		engine->ExecuteClientCmd("gameui_hide");
		CBaseModPanel::GetSingleton().CloseAllWindows();
	}
}

void CPortalLeaderboardPanel::MsgRetryMap()
{
	if( m_bSinglePlayerMode )
	{
		engine->ServerCmd( "restart_level" );
	}
	else
	{
		engine->ServerCmd( "mp_restart_level" );
	}
}


//=============================================================================
void CPortalLeaderboardPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_nUnlockedSPChapters = BaseModUI::CBaseModPanel::GetSingleton().GetChapterProgress();

	bool bInHub = IsInHub();

	m_pChapterListButton = static_cast< CDialogListButton * >( FindChildByName( "ListBtnChapters" ) );
	if ( m_pChapterListButton )
	{
		m_pChapterListButton->SetCanWrap( false );

		// if single player, make sure locked chapters are set correctly
		if ( m_bSinglePlayerMode && m_nUnlockedSPChapters >= 1 )
		{
			// loop thru locked chapter names
			int nListCount = m_pChapterListButton->GetListItemCount();
			for ( int i = m_nUnlockedSPChapters - 1; i < nListCount; ++i )
			{
				// get the full chapter name
				wchar_t *pChapterTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_PRESENCE_TEXT_CH%d", i+1 ) );
				char szChapterString[64];
				g_pVGuiLocalize->ConvertUnicodeToANSI( pChapterTitle, szChapterString, sizeof( szChapterString ) );
				char *pHeaderPrefix = V_strstr( szChapterString, "\n" );
				if ( pHeaderPrefix )
				{
					// truncate the title, want to preserve "Chapter ?"
					*pHeaderPrefix = 0;
				}
				// set the new text
				m_pChapterListButton->SetListItemText( i, szChapterString );
			}
		}
		
		// set the correct active chapter, but not from Hub in coop
		m_nCurrentChapterNumber = 1;
		if ( m_leaderboardState == STATE_END_OF_LEVEL || ( m_leaderboardState == STATE_PAUSE_MENU && !bInHub ) )
		{
			m_nCurrentChapterNumber = CBaseModPanel::GetSingleton().MapNameToChapter( engine->GetLevelNameShort(), m_bSinglePlayerMode );	
		}
		m_pChapterListButton->SetCurrentSelectionIndex( m_nCurrentChapterNumber - 1 );
		m_pChapterListButton->SetArrowsAlwaysVisible( true );
	}

	SetMapList();
	SetNextMap();
	// if end of level or from pause menu, select current map ( but not in Hub )
	if ( m_leaderboardState == STATE_END_OF_LEVEL || ( m_leaderboardState == STATE_PAUSE_MENU && !bInHub ) )
	{
		m_nMapIndex = GetIndexOfMap(  CBaseModPanel::GetSingleton().GetMapNumInChapter( m_nCurrentChapterNumber, 
																	engine->GetLevelNameShort(), m_bSinglePlayerMode ) );
		m_bNeedsMapItemSelect = true;
	}
	else
	{
		m_pMapList->SelectPanelItem(0);

		SelectPanel( SELECT_CHAPTER );
	}

	if ( m_pChallengeStatsPanel )
	{
		if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			m_pChallengeStatsPanel->SetVisible( true );
			m_pChallengeStatsPanel->SetTitleText( "#PORTAL2_FinalScore" );
			UpdateStatPanel();
		}
		else if ( m_leaderboardState == STATE_PAUSE_MENU && !IsInHub() )
		{
			m_pChallengeStatsPanel->SetVisible( true );
			m_pChallengeStatsPanel->SetTitleText( "#PORTAL2_CurrentScore" );
			UpdateStatPanel();
		}
		else
		{
			m_pChallengeStatsPanel->SetVisible( false );
		}
	}

	m_pLeaderboardListButton = static_cast< CDialogListButton * >( FindChildByName( "ListBtnLeaderboards" ) );
	if ( m_pLeaderboardListButton )
	{
		m_pLeaderboardListButton->SetCurrentSelectionIndex( 0 );
		m_pLeaderboardListButton->SetArrowsAlwaysVisible( IsPC() );
		m_pLeaderboardListButton->SetCanWrap( !IsPC() );
		m_pLeaderboardListButton->SetDrawAsDualStateButton( true );
	}

	m_pInvalidLabel = static_cast< Label *>( FindChildByName( "LblInvalidLeaderboard" ) );
	if ( m_pInvalidLabel )
	{
		m_pInvalidLabel->SetVisible( false );
	}

	m_pInvalidLabel2 = static_cast< Label *>( FindChildByName( "LblInvalidLeaderboard2" ) );
	if ( m_pInvalidLabel2 )
	{
		m_pInvalidLabel2->SetVisible( false );
	}

	m_pEveryoneLabel = static_cast< Label *>( FindChildByName( "LblEveryone" ) );

	m_pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) );
	if ( m_pWorkingAnim )
	{
		m_pWorkingAnim->SetVisible( false );
	}

	m_pMapList->SetScrollBarVisible( !IsGameConsole() );
	m_pMapList->SetScrollArrowsVisible( IsGameConsole() );

	m_pStatList->SetScrollBarVisible( false );
	m_pStatList->SetScrollArrowsVisible( false );

	UpdateFooter();
}

//=============================================================================
void CPortalLeaderboardPanel::Activate()
{
	BaseClass::Activate();

	if ( m_leaderboardState == STATE_MAIN_MENU )
	{
		SelectPanel( SELECT_CHAPTER );
	}
	else
	{
		if ( m_pMapList )
		{
			m_pMapList->RequestFocus();
		}
	}

	m_bCommittedAction = false;

	GameUI().PreventEngineHideGameUI();
	
	
	UpdateFooter();
}


//=============================================================================
void CPortalLeaderboardPanel::OnKeyCodePressed( KeyCode code )
{
	int iSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iSlot );

	if ( m_bCommittedAction )
	{
		// Already decided to do something else... don't accept input
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// sp/coop and pc/console all use B as main menu on EOL
		if ( m_leaderboardState == STATE_END_OF_LEVEL || m_leaderboardState == STATE_START_OF_LEVEL )
		{
			ReturnToMainMenu();

			m_bCommittedAction = true;
			return;
		}
		else // all layouts use B as Back in all other states
		{
			GameUI().AllowEngineHideGameUI();
			
			if ( m_leaderboardState == STATE_MAIN_MENU )
			{
				PortalLeaderboardManager()->CleanupLeaderboard();

				m_bCommittedAction = true;
			}
		}
		
		break;

	case KEY_ENTER:
	case KEY_XBUTTON_A:

		// no matter what state, if an avatar is selected
		// allow for viewing the ID
		if ( m_currentSelection == SELECT_AVATAR )
		{
			CAvatarPanelItem *pPanelItem = static_cast< CAvatarPanelItem *>( m_pStatList->GetSelectedPanelItem() );
			if ( pPanelItem )
			{
				pPanelItem->ActivateSelectedItem();
			}
			break;
		}

		// handle main menu
		if ( m_leaderboardState == STATE_MAIN_MENU )
		{
			if ( !IsCurrentMapLocked() )
			{
				if ( m_bSinglePlayerMode )
				{
					StartSPGame();
				}
				else
				{
					OpenCoopLobby();
				}

				m_bCommittedAction = true;
			}
			// hide the game UI
			GameUI().AllowEngineHideGameUI();
		}
		// all other cases of full leaderboard allow standard map selection
		else 
		{
			if ( StartSelectedMap() )
			{
				m_bCommittedAction = true;
			}
		}
		break;

	case KEY_XBUTTON_X:
		// replay current map on consoles at end of level
		if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			if ( PortalMPGameRules() )
			{
				GameUI().AllowEngineHideGameUI();
				engine->ServerCmd( "pre_go_to_hub" );
				PostMessage( this, new KeyValues( "MsgRetryMap" ), 1.0f );
			}
			else
			{
				PostMessage( this, new KeyValues( "MsgRetryMap" ), 0.0f );
			}
			
			m_bCommittedAction = true;
		}
		break;

	case KEY_XBUTTON_Y:
		// on consoles, allow leaderboard switching
		if ( !IsPC() && m_pLeaderboardListButton && m_pLeaderboardListButton->IsVisible() )
		{
			m_pLeaderboardListButton->SetNextSelection();
		}
		else if ( IsPC() && m_leaderboardState == STATE_END_OF_LEVEL )
		{
			// on PC/Mac play the next map in order
			if ( PortalMPGameRules() )
			{
				if ( m_szNextMap[0] != '\0' )
				{
					GameUI().AllowEngineHideGameUI();
					engine->ServerCmd( "pre_go_to_hub" );
					PostMessage( this, new KeyValues( "MsgGoToNext" ), 1.0f );
					m_bCommittedAction = true;
				}
			}
			else
			{
				PostMessage( this, new KeyValues( "MsgGoToNext" ), 0.0f );
				m_bCommittedAction = true;
			}
		}
		return;

	case KEY_UP:
	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
#if defined( _X360 )
		if ( m_pLeaderboardListButton->HasFocus() )
		{
			// select the bottom available avatar item
			m_pStatList->SelectPanelItem( m_pStatList->GetLastVisibleItemNumber() );
			SetPanelSelection( SELECT_AVATAR );
		}
		return;
#endif
		break;

	case KEY_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
#if defined( _X360 )
		if ( m_pLeaderboardListButton->HasFocus() )
		{
			// select the first available avatar item
			m_pStatList->SelectPanelItem( m_pStatList->GetFirstVisibleItemNumber()	);
			SetPanelSelection( SELECT_AVATAR );
		}
		else
#endif
		if ( m_pChapterListButton->HasFocus() )
		{
			m_pMapList->RequestFocus();
			m_pMapList->SelectPanelItem( 0 );
			m_pMapList->GetSelectedPanelItem()->NavigateTo();
			SetPanelSelection( SELECT_MAP );
			UpdateFooter();
			return;
		}
		break;

	case KEY_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
		if ( m_pChapterListButton->HasFocus() )
		{
			m_pMapList->SelectPanelItem(0);
			SelectPanel( SELECT_CHAPTER );
		}
		else if ( m_pLeaderboardListButton && m_pLeaderboardListButton->HasFocus() )
		{
			if( m_pLeaderboardListButton->GetCurrentSelectionIndex() == 0 )
			{
				SelectPanel( SELECT_MAP, true );
			}
		}
		break;

	case KEY_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
		if ( m_pChapterListButton->HasFocus() )
		{
			m_pMapList->SelectPanelItem(0);
			SelectPanel( SELECT_CHAPTER );
		}
	}
	BaseClass::OnKeyCodePressed(code);
}


//=============================================================================
void CPortalLeaderboardPanel::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );

		m_bCommittedAction = true;
		return;
	}
	else if ( !V_stricmp( "Leaderboard_Portals", command ) )
	{
		m_CurrentLeaderboardType = LEADERBOARD_PORTAL;
		SetPanelStats();
		// consoles use a key to switch and don't want to change focus
		if ( IsPC() ) 
		{
			SetPanelSelection( SELECT_LEADERBOARD );
		}
		else if ( m_currentSelection == SELECT_AVATAR )
		{
			m_currentSelection = SELECT_NONE;
			SelectPanel( SELECT_AVATAR, true );
		}
	}
	else if ( !V_stricmp( "Leaderboard_Time", command ) )
	{
		m_CurrentLeaderboardType = LEADERBOARD_TIME;
		SetPanelStats();
		// consoles use a key to switch and don't want to change focus
		if ( IsPC() )
		{
			SetPanelSelection( SELECT_LEADERBOARD );
		}
		else if ( m_currentSelection == SELECT_AVATAR )
		{
			m_currentSelection = SELECT_NONE;
			SelectPanel( SELECT_AVATAR, true );
		}
	}
	else
	{
		const char *pChapterCmd = Q_stristr( command, ":" );
		if ( pChapterCmd )
		{
			SetPanelSelection( SELECT_CHAPTER );
			// get the index
			pChapterCmd++;
			m_nCurrentChapterNumber = V_atoi( pChapterCmd );
			// get the list of maps
			m_nMapIndex = 0;
			SetMapList();
			// select the top item in the map list
			m_pMapList->SelectPanelItem(0);
			SelectPanel( SELECT_CHAPTER );
		}
	}
	UpdateFooter();

	BaseClass::OnCommand( command );
}

//=============================================================================
void CPortalLeaderboardPanel::OnThink()
{
	bool bOnline = false;

	if ( !cl_leaderboard_fake_offline.GetBool() )
	{
#if !defined( NO_STEAM )
		if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUser()->BLoggedOn() && steamapicontext->SteamMatchmaking() )
#elif defined( _X360 )
		if ( CUIGameData::Get()->AnyUserConnectedToLIVE() ) // If either player is online with LIVE, that's good enough because the session is still established
#else
		if ( 0 )
#endif
		{
			bOnline = true;
		}
	}

	if ( !m_bOnline && bOnline )
	{
		m_bNeedsUpdate = true;
	}

	if ( m_bOnline != bOnline )
	{
		m_bOnline = bOnline;

		if ( !IsPC() )
		{
			UpdateFooter();
		}
	}


	if ( m_bNeedsMapItemSelect )   
	{
		if ( m_pMapList && m_nMapIndex >= 0 )
		{
			// hacky check to make sure panels are all set up
			int min, max;
			m_pMapList->GetScrollBar()->GetRange( min, max );
			if ( max > 0 )
			{
				m_pMapList->SelectPanelItem( m_nMapIndex );
				m_bNeedsMapItemSelect = false;
			}
			
		}
	}

	if ( m_bOnline )
	{
		if( m_bNeedsUpdate )
		{
			const char *pMapName = GetCurrentMapName();
			bool bViewSpinner = true;
			bool bHideInvalidLabels = true;

			if ( !pMapName )
				return;

			// if we need an update, clear all previous data
			ClearStats();
			m_pPortalGraph->ClearData();
			m_pTimeGraph->ClearData();

			// get the leaderboard manager
			if ( !m_pLeaderboard )
			{
				m_pLeaderboard = PortalLeaderboardManager()->GetLeaderboard( pMapName ); 
			}

			if ( m_pLeaderboard )
			{
				if ( m_pLeaderboard->IsInvalid() || cl_leaderboard_fake_io_error.GetBool() || cl_leaderboard_fake_no_data.GetBool() )
				{
					if ( m_pInvalidLabel )
					{
						if ( m_pLeaderboard->WasIOError() || cl_leaderboard_fake_io_error.GetBool() )
						{
							m_pInvalidLabel->SetText( "#PORTAL2_Leaderboards_Invalid" );
						}
						else
						{
							m_pInvalidLabel->SetText( "#PORTAL2_Leaderboards_Empty" );
						}

						m_pInvalidLabel->SetVisible( true );

						if( m_pInvalidLabel2 )
						{
							m_pInvalidLabel2->SetVisible( false );
						}
					}

					UpdateFooter();
					m_bNeedsUpdate = false;
					bViewSpinner = false;
					bHideInvalidLabels = false;
				}
				else if( !m_pLeaderboard->IsQuerying() )
				{
					SetGraphData( m_pPortalGraph, LEADERBOARD_PORTAL );
					SetGraphData( m_pTimeGraph, LEADERBOARD_TIME );
					SetPanelStats();

					// It's time for glados to speak
					if( m_leaderboardState == STATE_END_OF_LEVEL )
					{
						// If we haven't cheated, let Glados speak
						if( !m_bCheated )
						{
							m_pLeaderboard->SetGladosIsAllowedToSpeak( true );
						}
						else
						{
							// If we have cheated, then fake that glados has spoken so that she won't be able to speak
							// so long as we have this leaderboard
							m_pLeaderboard->SetGladosHasSpoken();
						}
					}

#ifdef BUILD_GLOBAL_LEADERBOARD_STEAM_CONFIG
					char szBuff[ 1024 ];
					char szFriendlyName[ 256 ];
					CLeaderboardMapItem *pPanel = static_cast< CLeaderboardMapItem *>( m_pMapList->GetSelectedPanelItem() );
					Label *pLabel = dynamic_cast< Label* >( pPanel->FindChildByName( "LblChapterName" ) );

					for ( int i = 0; i < NUM_LEADERBOARDS; ++i )
					{
						pLabel->GetText( szFriendlyName, sizeof( szFriendlyName ) );
						V_strncat( szFriendlyName, i == LEADERBOARD_PORTAL ? ": Portals" : ": Time", sizeof(szFriendlyName) );

						SteamLeaderboard_t hLeaderboard = m_pLeaderboard->GetLeaderboardHandle( (LeaderboardType)i );
						int nHandle = static_cast< int >( hLeaderboard );

						V_snprintf( szBuff, sizeof(szBuff), "%i => array( 'name' => '%s', 'valueType' => '%s', 'globalLimit' => 200 ),\n", 
									nHandle, szFriendlyName, ( i == LEADERBOARD_PORTAL ? "qty" : "time_ms" ) );

						FileHandle_t hFile = g_pFullFileSystem->Open( BUILD_GLOBAL_LEADERBOARD_STEAM_CONFIG, "a" );
						if ( hFile )
						{
							g_pFullFileSystem->Write( szBuff, V_strlen( szBuff ), hFile );
							g_pFullFileSystem->Close( hFile );
						}
					}
#endif // BUILD_GLOBAL_LEADERBOARD_STEAM_CONFIG

					// display the improved leaderboard and force its stat update
					if ( V_strcmp( engine->GetLevelNameShort(), m_pLeaderboard->GetMapName() ) == 0 && m_pLeaderboardListButton )
					{
						// If Glados is allowed to speak, and this is the end of the level, and cheats weren't detected
						if( m_pLeaderboard->IsGladosAllowedToSpeak() && m_leaderboardState == STATE_END_OF_LEVEL && !m_bCheated )
						{
							char szGladosLine [256];
							int nImprovedLeaderboardIndex;
							m_pLeaderboard->DoGladosSpokenReaction(szGladosLine, &nImprovedLeaderboardIndex);
							vgui::surface()->PlaySound( szGladosLine );

							m_pLeaderboardListButton->SetCurrentSelectionIndex( nImprovedLeaderboardIndex );
							m_pLeaderboardListButton->ForceCurrentSelectionCommand();
						}
					}
				
					InvalidateLayout( false );
					m_bNeedsUpdate = false;
					bViewSpinner = false;

					UpdateFooter();
				}
			}
	
			if( bHideInvalidLabels )
			{
				if ( m_pInvalidLabel )
				{
					m_pInvalidLabel->SetVisible( false );
				}
				if ( m_pInvalidLabel2 )
				{
					m_pInvalidLabel2->SetVisible( false );
				}
			}

			ClockSpinner( bViewSpinner );
		}
	}
	else
	{
		ClearStats();
		m_pPortalGraph->ClearData();
		m_pTimeGraph->ClearData();

		if ( m_pInvalidLabel )
		{
			m_pInvalidLabel->SetVisible( true );

			if ( IsX360() )
			{
				m_pInvalidLabel->SetText( "#PORTAL2_LeaderboardOnlineWarning_X360" );
			}
			else if ( IsPS3() )
			{
				m_pInvalidLabel->SetText( "#PORTAL2_LeaderboardOnlineWarning_PSN_Steam" );
			}
			else
			{
				m_pInvalidLabel->SetText( "#PORTAL2_LeaderboardOnlineWarning_Steam" );
			}
		}

		// X360 can queue up leaderboard scores while offline
		if ( !IsX360() )
		{
			if ( m_pInvalidLabel2 )
			{
				m_pInvalidLabel2->SetVisible( true );
			}
		}

		if ( m_pWorkingAnim && m_pWorkingAnim->IsVisible() )
		{
			ClockSpinner( false );
		}
	}

	BaseClass::OnThink();
}


//=============================================================================
void CPortalLeaderboardPanel::OnClose()
{
	GameUI().AllowEngineHideGameUI();
	BaseClass::OnClose();
}


//=============================================================================
void CPortalLeaderboardPanel::SetDataSettings( KeyValues *pSettings )
{
	m_leaderboardState = static_cast< LeaderboardState_t >( pSettings->GetInt( "state", STATE_PAUSE_MENU ) );
	m_bCheated = static_cast< LeaderboardState_t >( pSettings->GetBool( "cheated", false ) );

	BaseClass::SetDataSettings( pSettings );
}


//=============================================================================
void CPortalLeaderboardPanel::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;

		// set buttons that are specific to state
		if ( m_leaderboardState == STATE_MAIN_MENU )
		{
			if ( !IsCurrentMapLocked() )
			{
				if ( m_bSinglePlayerMode )
				{
					visibleButtons |= FB_ABUTTON;
					pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PlaySelectedMap" );
				}
				else
				{
					visibleButtons |= FB_ABUTTON;
					pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_ContinueToCoop" );
				}
			}
		}
		else if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			if ( IsPC() )
			{
				if ( !IsCurrentMapLocked() )
				{
					visibleButtons |= FB_ABUTTON;
					pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PlaySelectedMap" );					
				}
				visibleButtons |= FB_XBUTTON | FB_YBUTTON;
				pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_ReplayMap");
				pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_PlayNextMap");
				pFooter->SetButtonText( FB_BBUTTON, "#PORTAL2UI_ExitToMainMenu" );
			}
			else
			{
				visibleButtons |= FB_ABUTTON;
				pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_ContinueToCoop"); // says "Continue"
				visibleButtons |= FB_XBUTTON;
				pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_ReplayMap");
			}
		}
		else // allow map selection
		{
			if ( !IsCurrentMapLocked() )
			{
				// set up coop HUB to allow selection
				visibleButtons |= FB_ABUTTON;
				pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PlaySelectedMap" );
			}

			// special case when in the hub
			if ( m_leaderboardState == STATE_START_OF_LEVEL )
			{
				pFooter->SetButtonText( FB_BBUTTON, "#PORTAL2UI_ExitToMainMenu" );
			}
			else
			{
				pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
			}
		}
		

		// set the leaderboard switching button for consoles
		if ( !IsPC() && m_bOnline && m_pLeaderboard && !m_pLeaderboard->IsInvalid() && !m_pLeaderboard->IsQuerying()
			&& !cl_leaderboard_fake_io_error.GetBool() && !cl_leaderboard_fake_no_data.GetBool() )
		{
			visibleButtons |= FB_YBUTTON;
			const char* pLeaderboardText = m_CurrentLeaderboardType == LEADERBOARD_PORTAL ? "#PORTAL2_ViewTime" : "#PORTAL2_ViewPortals";
			pFooter->SetButtonText( FB_YBUTTON, pLeaderboardText );
		}
		// set the avatar selection button irrelevant of state
		if ( m_currentSelection == SELECT_AVATAR )
		{
			visibleButtons |= FB_ABUTTON;
#ifndef NO_STEAM
			//pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_ViewSteamID" );
#else
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_GameInfo" );
#endif
		}

		pFooter->SetButtons( visibleButtons );
	}
}


//=============================================================================
void CPortalLeaderboardPanel::SetMapList()
{
	KeyValues *pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( m_nCurrentChapterNumber, !m_bSinglePlayerMode );

	if ( !pChallengeMapList )
		return;

	if ( m_pMapList )
	{
		m_pMapList->RemoveAllPanelItems();
		m_nMapIndex = 0;
		// reset the scrollbar to the top
		m_pMapList->GetScrollBar()->SetValue( m_nMapIndex );

		// loop thru key values - chapter names
		// for each chapter
		int nCurrentIndex = 0;
		for ( KeyValues *pCurrentMap = pChallengeMapList->GetFirstSubKey(); pCurrentMap != NULL; pCurrentMap = pCurrentMap->GetNextKey() )
		{
			// get the actual map number of that map
			int nMapNumber = BaseModUI::CBaseModPanel::GetSingleton().GetMapNumInChapter( m_nCurrentChapterNumber, 
																		pCurrentMap->GetString(), m_bSinglePlayerMode );

			// add an item using the current chapter number, real map number, and map index
			CLeaderboardMapItem *pItem = m_pMapList->AddPanelItem< CLeaderboardMapItem >( "newgame_chapteritem" );
			if( pItem )
			{
				int nUnlockedChapters = m_bSinglePlayerMode ? m_nUnlockedSPChapters : -1;
				pItem->SetChapterAndMapInfo( m_nCurrentChapterNumber, nMapNumber, nCurrentIndex++, nUnlockedChapters );
			}
		}

		m_bNeedsUpdate = true;
	}
}

void CPortalLeaderboardPanel::SetNextMap()
{
	int nCurrentChapterNumber = CBaseModPanel::GetSingleton().MapNameToChapter( engine->GetLevelNameShort(), m_bSinglePlayerMode );

	KeyValues *pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nCurrentChapterNumber, !m_bSinglePlayerMode );
	if ( pChallengeMapList )
	{
		int nChapter = nCurrentChapterNumber;

		for ( KeyValues *pCurrentMap = pChallengeMapList->GetFirstSubKey(); pCurrentMap != NULL; pCurrentMap = pCurrentMap->GetNextKey() )
		{
			if ( V_strcmp( engine->GetLevelNameShort(), pCurrentMap->GetString() ) != 0 )
			{
				continue;
			}

			KeyValues *pNextMap = pCurrentMap->GetNextKey();
			int nNextMapNumber;

			if ( !pNextMap )
			{
				// Next chapter
				nChapter++;
				pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapter, !m_bSinglePlayerMode );

				if ( !pChallengeMapList )
				{
					// Wrap to first chapter
					nChapter = 1;
					pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapter, !m_bSinglePlayerMode );
				}

				// there should always be another map, or another (or the first) chapter
				Assert( pChallengeMapList );

				if ( pChallengeMapList )
				{
					// First map
					pNextMap = pChallengeMapList->GetFirstSubKey();
				}

				// single player locked map should wrap to first map of first chapter
				nNextMapNumber = CBaseModPanel::GetSingleton().GetMapNumInChapter( nChapter, pNextMap->GetString(), m_bSinglePlayerMode );
				Assert( nNextMapNumber != -1 ); // should always have a valid map number
				if ( m_bSinglePlayerMode && IsMapLocked(nChapter, nNextMapNumber, m_bSinglePlayerMode) )
				{
					// go to first map of first chapter
					nChapter = 1;
					pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapter, !m_bSinglePlayerMode );
					pNextMap = pChallengeMapList->GetFirstSubKey(); // should always be unlocked ( to be able to play in the 1st place)
				}
			}

			// there should always be another map, or a first map of another (or the first) chapter
			Assert( pNextMap );

			// get the map number
			nNextMapNumber = CBaseModPanel::GetSingleton().GetMapNumInChapter( nChapter, pNextMap->GetString(), m_bSinglePlayerMode );
			Assert( nNextMapNumber != -1 ); // should always have a valid map number

			if ( pNextMap && !IsMapLocked( nChapter, nNextMapNumber, m_bSinglePlayerMode ) )
			{
				V_strncpy( m_szNextMap, pNextMap->GetString(), sizeof(m_szNextMap) );
			}
			else // if the next map is locked
			{
				V_strncpy( m_szNextMap, "", sizeof(m_szNextMap) );
			}
		}
	}
}


void CPortalLeaderboardPanel::ResetTempScoreUpdates( void )
{
	const char *pMap = CPortalLeaderboardManager::GetTempScoresMap();
	if ( pMap && pMap[ 0 ] != '\0' && V_strcmp( pMap, engine->GetLevelNameShort() ) == 0 )
	{
		return;
	}

	g_nPortalScoreTempUpdate = -1;
	g_nTimeScoreTempUpdate = -1;
	g_nPortalScoreTempMostRecent = -1;
	g_nTimeScoreTempMostRecent = -1;
}

static void __MsgFunc_ScoreboardTempUpdate( bf_read &msg )
{
	int nPortalScore = msg.ReadLong();
	int nTimeScore = msg.ReadLong();

	if ( g_nPortalScoreTempUpdate == -1 || g_nPortalScoreTempUpdate > nPortalScore )
	{
		g_nPortalScoreTempUpdate = nPortalScore;
	}

	g_nPortalScoreTempMostRecent = nPortalScore;

	if ( g_nTimeScoreTempUpdate == -1 || g_nTimeScoreTempUpdate > nTimeScore )
	{
		g_nTimeScoreTempUpdate = nTimeScore;
	}

	g_nTimeScoreTempMostRecent = nTimeScore;

	CPortalLeaderboardManager::SetTempScoresMap( engine->GetLevelNameShort() );
}
USER_MESSAGE_REGISTER( ScoreboardTempUpdate );


static void ChallengeModeCheatsOKCallback()
{
	// PC's not crossplaying with a PS3
	if ( (IsPC() && !ClientIsCrossplayingWithConsole())  )
	{
		engine->ClientCmd( "leaderboard_open 1 1" );
	}
	else
	{
		engine->ClientCmd( "+leaderboard 2 1" );
	}
}


static void __MsgFunc_ChallengeModeCheatSession( bf_read& /*msg*/ )
{
	//Bring up a dialog telling the player that their scores weren't recorded as cheats were enabled
	GameUI().PreventEngineHideGameUI();
	GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, CBaseModPanel::GetSingleton().GetWindow( WT_PORTALLEADERBOARD ), true ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#PORTAL2_ChallengeMode_Cheat_Header";
	data.pMessageText = "#PORTAL2_ChallengeMode_Cheat_Explanation";

	data.bOkButtonEnabled = true;
	data.pOkButtonText = "#L4D360UI_Ok";
	data.pfnOkCallback = &ChallengeModeCheatsOKCallback;

	confirmation->SetUsageData(data);

	GameUI().ActivateGameUI();
}
USER_MESSAGE_REGISTER( ChallengeModeCheatSession );


static void __MsgFunc_ChallengeModeCloseAllUI( bf_read& /*msg*/ )
{
	GenericConfirmation* pConfirmation = static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
	if ( pConfirmation )
	{
		// make sure the cheat window gets closed - others, like memory card notice should remain
		if ( V_strcmp( pConfirmation->GetUsageData().pWindowTitle, "#PORTAL2_ChallengeMode_Cheat_Header") == 0 ||
			V_strcmp( pConfirmation->GetUsageData().pWindowTitle, "#L4D360UI_LeaveMultiplayerConf") == 0 ||
			V_strcmp( pConfirmation->GetUsageData().pWindowTitle, "#Portal2UI_GoToHubQ") == 0 )
		{
			pConfirmation->Close();
		}
	}

	GameUI().AllowEngineHideGameUI();
	GameUI().HideGameUI();
	CBaseModPanel::GetSingleton().CloseAllWindows();
}
USER_MESSAGE_REGISTER( ChallengeModeCloseAllUI );



//=============================================================================
void CPortalLeaderboardPanel::SetPanelStats()
{
	if ( m_CurrentLeaderboardType == LEADERBOARD_PORTAL )
	{
		if ( m_pPortalGraph )
		{
			m_pPortalGraph->SetVisible( true );
		}

		if ( m_pTimeGraph )
		{
			m_pTimeGraph->SetVisible( false );
		}
	}
	else if ( m_CurrentLeaderboardType == LEADERBOARD_TIME )
	{
		if ( m_pTimeGraph )
		{
			m_pTimeGraph->SetVisible( true );
		}

		if ( m_pPortalGraph )
		{
			m_pPortalGraph->SetVisible( false );
		}
	}

	if ( m_pLeaderboardListButton )
	{
		m_pLeaderboardListButton->SetVisible( true );
	}

	if ( m_pEveryoneLabel )
	{
		m_pEveryoneLabel->SetVisible( true );
	}
	

	if ( m_pStatList )
	{
		m_pStatList->RemoveAllPanelItems();

		if ( !m_pLeaderboard )
			return;

		m_pLeaderboard->UpdateXUIDs();

		int nPlayerIndex = m_pLeaderboard->GetCurrentPlayerIndex( m_CurrentLeaderboardType );
		int nOldIndex = -2;
		int nPlacementIndex = nPlayerIndex;
		int nIndexList[ 3 ] = { 0, 1, 2 };

		// get this player's score
		const PortalLeaderboardItem_t *pPlayerData[ 2 ];
		pPlayerData[ 0 ] = m_pLeaderboard->GetCurrentBest( m_CurrentLeaderboardType );
		pPlayerData[ 1 ] = m_pLeaderboard->GetCurrentBest( m_CurrentLeaderboardType, 1 );
		const PortalLeaderboardItem_t *pData;

		int nSpecialScore = -1;
		PortalLeaderboardItem_t tempUpdate;

		if ( V_strcmp( engine->GetLevelNameShort(), m_pLeaderboard->GetMapName() ) == 0 )
		{
			if ( g_nPortalScoreTempUpdate != -1 && m_CurrentLeaderboardType == LEADERBOARD_PORTAL )
			{
				nSpecialScore = g_nPortalScoreTempUpdate;
			}
			else if ( g_nTimeScoreTempUpdate != -1 && m_CurrentLeaderboardType == LEADERBOARD_TIME )
			{
				nSpecialScore = g_nTimeScoreTempUpdate;
			}
		}

		int nOffset = 0;
		bool bFirstValidPlayer = true;

		for ( DWORD iSlot = 0; iSlot < XBX_GetNumGameUsers(); ++iSlot )
		{
#ifdef _GAMECONSOLE
			if ( CUIGameData::Get()->IsGuestOrOfflinePlayerWhenSomePlayersAreOnline( iSlot ) )
			{
				nOffset = 1;
				continue;
			}
#endif //#ifdef _GAMECONSOLE

			if ( nSpecialScore != -1 && ( !pPlayerData[ iSlot - nOffset ] || nSpecialScore < pPlayerData[ iSlot - nOffset ]->m_iScore ) )
			{
				V_strncpy( tempUpdate.m_szName, "Fixme: no name", sizeof(tempUpdate.m_szName) );

				if ( pPlayerData[ iSlot - nOffset ] )
				{
					tempUpdate = *(pPlayerData[ iSlot - nOffset ]);
				}
			
				tempUpdate.m_iScore = nSpecialScore;
#if !defined( NO_STEAM )
				tempUpdate.m_steamIDUser = 0ull;
#elif defined( _X360 )
				tempUpdate.m_xuid = 0ull;
#endif

				pPlayerData[ iSlot - nOffset ] = &tempUpdate;

				if ( bFirstValidPlayer )
				{
					bFirstValidPlayer = false;

					nPlacementIndex = -1;  // mark the position with a -1 to use the new score
					nOldIndex = nPlayerIndex;

					// if the player had no previous score and just got a new score
					if ( nOldIndex == -1 )
					{
						int nNewIndex = 0;
						pData = m_pLeaderboard->GetPlayerAtIndex( nNewIndex, m_CurrentLeaderboardType );
						// loop thru existing scores to find player's new index
						while ( pData )
						{
							if ( pData->m_iScore >= pPlayerData[ iSlot - nOffset ]->m_iScore )
								break;

							++nNewIndex;
							pData = m_pLeaderboard->GetPlayerAtIndex( nNewIndex, m_CurrentLeaderboardType );
						}
						nPlayerIndex = nNewIndex;
					}

					// otherwise start at the player's old score index
					// and loop until we find where the new score fits
					for ( int i = nOldIndex; i >= 0; --i )
					{
						// get the player at each position beneath the old score's position
						pData = m_pLeaderboard->GetPlayerAtIndex( i, m_CurrentLeaderboardType );
						// if the new score is better or equal to the lower position
						if ( pData && pData->m_iScore >= pPlayerData[ iSlot - nOffset ]->m_iScore )
						{
							// set the player's new position
							nPlayerIndex = i; 
						}
						else
						{
							// if we've found a better score
							break; // go no further
						}
					}
				}
			}
		}

		if ( pPlayerData[ 0 ] )
		{
			// set the proper index if the player is at #0
			if ( nPlayerIndex < 1 )
			{
				nIndexList[ 0 ] = nPlacementIndex;
				// if we moved up into top place, we need to put the old top
				// place beneath us
				if ( nOldIndex >= -1 )
				{
					nIndexList[ 1 ] = 0;
					nIndexList[ 2 ] = 1;
				}
			}
			else if ( nPlayerIndex == 1 ) // if player is #1 check for a tie w/ #0
			{
				pData = m_pLeaderboard->GetPlayerAtIndex( 0, m_CurrentLeaderboardType );
				Assert( pData );
				if ( pPlayerData[ 0 ]->m_iScore <= pData->m_iScore )
				{
					nIndexList[ 0 ] = nPlacementIndex;
					nIndexList[ 1 ] = 0;
				}
				else
				{
					nIndexList[ 1 ] = nPlacementIndex;
				}

				if( nOldIndex >= -1 )
				{
					nIndexList[ 2 ] = 1;
				}
			}
			else if ( nPlayerIndex > 1 )
			{
				// check for a tie w/ #0
				pData = m_pLeaderboard->GetPlayerAtIndex( 0, m_CurrentLeaderboardType );
				Assert( pData );
				if ( pPlayerData[ 0 ]->m_iScore <= pData->m_iScore )
				{
					nIndexList[ 0 ] = nPlacementIndex;
					nIndexList[ 1 ] = 0;
					nIndexList[ 2 ] = 1;
				}
				else  // check for a tie w/ #1
				{
					pData = m_pLeaderboard->GetPlayerAtIndex( 1, m_CurrentLeaderboardType );
					Assert( pData );
					if ( pPlayerData[ 0 ]->m_iScore <= pData->m_iScore )
					{
						nIndexList[ 1 ] = nPlacementIndex;
						nIndexList[ 2 ] = 1;
					}
					else  // no tie w/ #0 or #1, just do #0, target, me
					{
						// if we have a new score
						if ( nOldIndex >= -1 )
						{
							// find the correct new target
							// ( due to previous logic nPlayerIndex can't be tied with index - 1
							//   and nPlayerIndex must be at least > 1 to reach this point
							nIndexList[ 1 ] = nPlayerIndex - 1;
						}
						else // just use the standard target
						{
							nIndexList[ 1 ] = m_pLeaderboard->GetNextTargetIndex( m_CurrentLeaderboardType );
						}
						nIndexList[ 2 ] = nPlacementIndex;
					}
				}

			}
			
			// if the player has earned a better score
			if ( nOldIndex >= 0 )
			{
				// make sure we don't display the player's old slot too
				for ( int i = 0; i < 3; ++i )
				{
					if ( nIndexList[i] >= nOldIndex )
					{
						// bump everyone at and below the player's old rank
						// down by one
						nIndexList[i] += 1;
					}
				}
			}

			if ( m_CurrentLeaderboardType == LEADERBOARD_PORTAL )
			{
				m_pPortalGraph->SetPlayerScore( pPlayerData[ 0 ]->m_iScore );
			}
			else
			{
				m_pTimeGraph->SetPlayerScore( pPlayerData[ 0 ]->m_iScore );
			}
		}
		else // no player data for map
		{
			// set second as worst
			int nWorstRank = m_pLeaderboard->GetWorstPlayerIndex( m_CurrentLeaderboardType );
			if ( nWorstRank >= 0 )
			{
				nIndexList[ 1 ] = nWorstRank;
			}

			if ( nIndexList[ 0 ] == nIndexList[ 1 ] )
			{
				nIndexList[ 1 ] = -1;// make the second rank a ? score for local player
				nIndexList[ 2 ] = -2;// don't show third
			}
			else
			{
				nIndexList[ 2 ] = -1; // make the third rank a ? score for local player
			}

			if ( m_CurrentLeaderboardType == LEADERBOARD_PORTAL )
			{
				m_pPortalGraph->SetPlayerScore( -1 );
			}
			else
			{
				m_pTimeGraph->SetPlayerScore( -1 );
			}
		}
		
		int nCurrentAvatarIndex = 0;
		bool bUsedTarget = false;

#if defined( _X360 )
		int nNumNonGuestPlayers = m_pLeaderboard->NumXUIDs();

		if ( nNumNonGuestPlayers > 1 )
		{
			int nScore = ( pPlayerData[ 0 ] ? pPlayerData[ 0 ]->m_iScore : -1 );
			AddAvatarPanelItem( m_pLeaderboard, m_pStatList, pPlayerData[ 0 ], nScore, m_CurrentLeaderboardType, 0, nCurrentAvatarIndex, m_nStatHeight, 0 );
			nCurrentAvatarIndex++;

			nScore = ( pPlayerData[ 1 ] ? pPlayerData[ 1 ]->m_iScore : -1 );
			AddAvatarPanelItem( m_pLeaderboard, m_pStatList, pPlayerData[ 1 ], nScore, m_CurrentLeaderboardType, 0, nCurrentAvatarIndex, m_nStatHeight, 1 );
			nCurrentAvatarIndex++;

			return;
		}
#endif

		for ( int i = 0; i < 3; ++i )
		{
			if ( nIndexList[ i ] == -1 )
			{
				bUsedTarget = true; // no targets after the player

				int nScore = ( pPlayerData[ 0 ] ? pPlayerData[ 0 ]->m_iScore : -1 );
				AddAvatarPanelItem( m_pLeaderboard, m_pStatList, pPlayerData[ 0 ], nScore, m_CurrentLeaderboardType, 0, nCurrentAvatarIndex, m_nStatHeight, -1 );
				nCurrentAvatarIndex++;
			}
			else if ( nIndexList[ i ] != -2 )
			{
				int nPlayerType = 3;
				if ( nIndexList[ i ] == nPlayerIndex && nOldIndex == -2 )
				{
					pData = pPlayerData[ 0 ];
					nPlayerType = 0;
					bUsedTarget = true;
				}
				else
				{
					pData = m_pLeaderboard->GetPlayerAtIndex( nIndexList[ i ], m_CurrentLeaderboardType );

					if ( i == 0 )
					{
						if ( nIndexList[ i ] != nPlayerIndex )
						{
							nPlayerType = 1;
						}
					}
					else if ( !bUsedTarget )
					{
						nPlayerType = 2;
						bUsedTarget = true;
					}
				}

				if  ( pData )
				{
					int nScore = ( pData ? pData->m_iScore : -1 );

					AddAvatarPanelItem( m_pLeaderboard, m_pStatList, pData, nScore, m_CurrentLeaderboardType, nPlayerType, nCurrentAvatarIndex, m_nStatHeight, -1 );
					nCurrentAvatarIndex++;
				}
			}
		}
	}
}


//=============================================================================
void CPortalLeaderboardPanel::SetGraphData( CPortalLeaderboardGraphPanel *pGraphPanel, LeaderboardType graphType )
{
	if ( !pGraphPanel )
		return;

	if ( !m_pLeaderboard || m_pLeaderboard->IsInvalid() )
	{
		return;
	}

	pGraphPanel->UpdateGraph( m_pLeaderboard, graphType );
}


void CPortalLeaderboardPanel::SelectPanel( PanelSelection_t selectedPanel, bool bForceItemSelect /*= false*/ )
{
	switch( selectedPanel )
	{
	case SELECT_AVATAR:
		if ( bForceItemSelect && m_pStatList )
		{
			if ( m_pStatList->GetSelectedPanelItem() )
			{
				m_pStatList->GetSelectedPanelItem()->RequestFocus();
			}
			else
			{
				if ( !m_pStatList->SelectPanelItem( m_pStatList->GetFirstVisibleItemNumber() ) )
				{
					m_bMapPanelSelected = true;
					SetPanelSelection( SELECT_MAP );
					return;
				}
			}
		}
		break;
	case SELECT_CHAPTER:
		if ( m_pChapterListButton )
		{
			m_pChapterListButton->RequestFocus();
			m_pChapterListButton->SetSelected( true );
			m_pChapterListButton->SetArmed( true );
		}
		break;
	case SELECT_LEADERBOARD:
		if ( m_pLeaderboardListButton )
		{
			m_pLeaderboardListButton->RequestFocus();
			m_pLeaderboardListButton->SetSelected( true );
			m_pLeaderboardListButton->SetArmed( true );
		}
		break;
	case SELECT_MAP:
		if ( bForceItemSelect && m_pMapList )
		{
			m_pMapList->RequestFocus();
			Panel *pPanel =	m_pMapList->GetSelectedPanelItem(); 
			if ( pPanel )
			{
				pPanel->RequestFocus();
			}
			else
			{
				m_pMapList->SelectPanelItem( 0 );
			}
		}
		break;
	}

	SetPanelSelection( selectedPanel );
	UpdateFooter();
}



//=============================================================================
void CPortalLeaderboardPanel::ClearAvatarSelection()
{
	if ( m_pStatList )
	{
		CAvatarPanelItem *pPanelItem = static_cast< CAvatarPanelItem *>( m_pStatList->GetSelectedPanelItem() );
		if ( pPanelItem )
		{
			pPanelItem->SetSelected( false );
			m_pStatList->ClearPanelSelection();
		}
	}
}


//=============================================================================
void CPortalLeaderboardPanel::ClearStats()
{
	if ( m_pStatList )
	{
		m_pStatList->RemoveAllPanelItems();
	}
	
	if ( m_pPortalGraph )
	{
		m_pPortalGraph->SetVisible( false );
	}
	
	if ( m_pTimeGraph )
	{
		m_pTimeGraph->SetVisible( false );
	}
	
	if ( m_pLeaderboardListButton )
	{
		m_pLeaderboardListButton->SetVisible( false );
	}
	
	if ( m_pEveryoneLabel )
	{
		m_pEveryoneLabel->SetVisible( false );
	}
}


//=============================================================================
const char* CPortalLeaderboardPanel::GetCurrentMapName()
{
	// get the map name from the chapter and map number
	if ( m_pMapList )
	{
		// get the map item and its actual map number
		CLeaderboardMapItem *pMapItem = static_cast< CLeaderboardMapItem *>( m_pMapList->GetPanelItem( m_nMapIndex ) );
		if ( pMapItem )
		{
			return CBaseModPanel::GetSingleton().GetMapName( m_nCurrentChapterNumber, pMapItem->GetMapNumber(), m_bSinglePlayerMode );	
		}
	}

	return NULL;
}


//=============================================================================
void CPortalLeaderboardPanel::SetPanelSelection( PanelSelection_t selectedPanel )
{
	if ( m_currentSelection != selectedPanel )
	{
		m_currentSelection = selectedPanel;
		
		if ( m_pLeaderboardListButton && m_currentSelection != SELECT_LEADERBOARD )
		{
			m_pLeaderboardListButton->SetSelected( false );
			m_pLeaderboardListButton->SetArmed( false );
		}

		if ( m_pChapterListButton && m_currentSelection != SELECT_CHAPTER )
		{
			m_pChapterListButton->SetSelected( false );
			m_pChapterListButton->SetArmed( false );
		}

		UpdateFooter();
	}
}


//=============================================================================
int CPortalLeaderboardPanel::GetIndexOfMap( int nMapNumber )
{
	if ( !m_pMapList )
		return -1;

	// loop thru map list until we find the one set to this map number
	int nMapCount = m_pMapList->GetPanelItemCount();
	for ( int i = 0; i < nMapCount; ++i )
	{
		CLeaderboardMapItem *pMapItem = static_cast< CLeaderboardMapItem *>( m_pMapList->GetPanelItem( i ) );
		if ( pMapItem->GetMapNumber() == nMapNumber )
		{
			return i;
		}
	}

	return -1;
}


//=============================================================================
void CPortalLeaderboardPanel::OpenCoopLobby()
{
#ifdef _GAMECONSOLE
	if ( XBX_GetPrimaryUserIsGuest() )
		CUIGameData::Get()->InitiateSplitscreenPartnerDetection( "coop_challenge" );
	else
	{
		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			"mode coop_challenge"
			);
		pSettings->SetString( "challenge_map", GetCurrentMapName() );
		KeyValues::AutoDelete autodelete( pSettings );
		CBaseModPanel::GetSingleton().OpenWindow( WT_STARTCOOPGAME, this, true, pSettings );
	}
#else
	CUIGameData::Get()->InitiateOnlineCoopPlay( this, "playonline", "coop_challenge", GetCurrentMapName() );
#endif
}

void CPortalLeaderboardPanel::ClockSpinner( bool bVisible )
{
	if ( m_pWorkingAnim )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pWorkingAnim->SetFrame( nAnimFrame );
		m_pWorkingAnim->SetVisible( bVisible );
	}
}

bool CPortalLeaderboardPanel::IsCurrentMapLocked()
{
	if ( m_pMapList )
	{
		CLeaderboardMapItem *pMapItem = static_cast< CLeaderboardMapItem* >( m_pMapList->GetSelectedPanelItem() );
		if ( pMapItem && pMapItem->IsLocked() )
			return true;
	}
	return false;
}

void CPortalLeaderboardPanel::ReturnToMainMenu()
{
	// main menu confirmation panel
	GameUI().PreventEngineHideGameUI();
	GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#L4D360UI_LeaveMultiplayerConf";
	data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsg";
	if ( GameRules() && GameRules()->IsMultiplayer() )
		data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsgOnline";
#ifdef _GAMECONSOLE
	if ( XBX_GetNumGameUsers() > 1 )
		data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsgSS";
#endif
	data.bOkButtonEnabled = true;
	data.pOkButtonText = "#PORTAL2_ButtonAction_Exit";
	data.pfnOkCallback = &LeaveGameOkCallback;
	data.bCancelButtonEnabled = true;
	data.pfnCancelCallback = (m_leaderboardState == STATE_START_OF_LEVEL) ? &LeaveGameHubCancelCallback : &LeaveGameCancelCallback;

	confirmation->SetUsageData(data);
}

void CPortalLeaderboardPanel::StartSPGame()
{
	KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetString( "map", GetCurrentMapName() );
	pSettings->SetString( "reason", "challenge" );
	CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
}

bool CPortalLeaderboardPanel::StartSelectedMap()
{
	if ( IsCurrentMapLocked() )
		return false;

	if ( m_bSinglePlayerMode )
	{
		char szCommand[64];
		Q_strncpy( szCommand, "select_map ", sizeof(szCommand) );
		Q_strncat( szCommand, GetCurrentMapName(), sizeof(szCommand) );
		GameUI().HideGameUI();
		engine->ServerCmd( szCommand );
		GameUI().AllowEngineHideGameUI();
	}
	else
	{
		bool bWaitScreen = CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_ChallengeMode_LevelTransition", 0.0f, NULL );
		PostMessage( this, new KeyValues( "MsgPreChangeLevel" ), bWaitScreen ? 2.0f : 0.0f );
	}

	return true;
}
bool CPortalLeaderboardPanel::IsMapLocked( int nChapterNumber, int nMapNumber, bool bSinglePlayer )
{
	if ( bSinglePlayer )
	{
		int nUnlockedChapters = BaseModUI::CBaseModPanel::GetSingleton().GetChapterProgress();

		if ( nChapterNumber >= nUnlockedChapters )
		{
			return true;
		}

		return false;
	}
	else  // coop
	{
		// should always be valid
		Assert( PortalMPGameRules() );

		return !PortalMPGameRules()->IsLevelInBranchComplete( nChapterNumber-1, nMapNumber-1 );
	}
}


bool CPortalLeaderboardPanel::IsInHub()
{
	return V_strcmp( engine->GetLevelNameShort(), "mp_coop_lobby_3" ) ? false : true;
}

void CPortalLeaderboardPanel::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	if ( code == MOUSE_LEFT )
	{
		PanelSelection_t prevSelection = m_currentSelection;
		m_currentSelection = SELECT_NONE;

		SelectPanel( prevSelection, true );
	}
}

void CPortalLeaderboardPanel::UpdateStatPanel()
{
	if ( m_pChallengeStatsPanel == NULL )
		return;

	if ( m_leaderboardState == STATE_END_OF_LEVEL )
	{
		// just use the temp capture stats
		m_pChallengeStatsPanel->SetPortalScore( g_nPortalScoreTempMostRecent );
		m_pChallengeStatsPanel->SetTimeScore( g_nTimeScoreTempMostRecent );
	}
	else
	{
		// get the stats, update the labels
		C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPlayer();
		if( pPlayer )
		{
			// update number of portals
			int nNumPortals = ( PortalMPGameRules() ? PortalMPGameRules()->GetNumPortalsPlaced() : pPlayer->m_StatsThisLevel.iNumPortalsPlaced.Get() );
			float flTotalSeconds = pPlayer->m_StatsThisLevel.fNumSecondsTaken.Get();

			m_pChallengeStatsPanel->SetPortalScore( nNumPortals );
			m_pChallengeStatsPanel->SetTimeScore( flTotalSeconds );
		}
	}
}

//=============================================================================
CAvatarPanelItem::CAvatarPanelItem( vgui::Panel *pParent, const char *pPanelName ) : CLeaderboardMapItem( pParent, pPanelName )
{
	 m_pHUDLeaderboard = 0;
	 m_nSteamID = 0;
	 m_nXUID = 0;

	 m_pGamerName = new Label( this, "LblGamerTag", "" );
	 m_pGamerScore = new Label( this, "LblGamerScore", "" );
	 m_pGamerAvatar = new ImagePanel( this, "PnlGamerPic" );
	 m_pScoreLegend = new Label( this, "LblScoreLegend", "" );
}


CAvatarPanelItem::~CAvatarPanelItem()
{
#ifndef NO_STEAM
	if ( m_pGamerAvatar )
	{
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID , BaseModUI::CUIGameData::kAvatarImageRelease );
		m_pGamerAvatar->SetImage( (IImage*)NULL );
		m_pGamerAvatar->SetVisible( false );
	}
#endif

}

//=============================================================================
void CAvatarPanelItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/portal_leaderboard_avatar_panel.res" );

	m_hTextFont = pScheme->GetFont( "NewGameChapterName", true );
	m_hFriendsListFont = pScheme->GetFont( "FriendsList", true );
	m_hFriendsListSmallFont = pScheme->GetFont( "FriendsListSmall", true );
	m_hFriendsListVerySmallFont = pScheme->GetFont( "ControllerLayout", true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
	m_LockedColor = GetSchemeColor( "HybridButton.LockedColor", pScheme );
	m_BaseColor = IsPC() ? GetSchemeColor( "HybridButton.MouseOverCursorColor", pScheme ) : Color(0, 0, 0, 40);
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColor", pScheme );
	m_LostFocusColor = m_BaseColor;

	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "NewGameDialog.TextOffsetY" ) ) );

	if( IsX360() )
	{
		SetKeyBoardInputEnabled( true );
	}
	else
	{
		SetKeyBoardInputEnabled( false );
	}
}


//=============================================================================
void CAvatarPanelItem::SetPlayerData( const PortalLeaderboardItem_t *pData, LeaderboardType type )
{
	if ( !pData )
		return;

	m_leaderboardType = type;

	const char *pPlayerName = pData->m_szName;

	if ( m_pGamerName )
	{
		m_pGamerName->SetText( pPlayerName );

		wchar_t wchName[ 128 ];
		m_pGamerName->GetText( wchName, sizeof( wchName) );
		
		int nWide, nTall;
		vgui::surface()->GetTextSize( m_hFriendsListFont, wchName, nWide, nTall );

		if ( nWide + 1 >= m_pGamerName->GetWide() || nTall + 1 >= m_pGamerName->GetTall() )
		{
			vgui::surface()->GetTextSize( m_hFriendsListSmallFont, wchName, nWide, nTall );

			// See if we can get away with small font before resorting to very small
			if ( nWide + 1 >= m_pGamerName->GetWide() || nTall + 1 >= m_pGamerName->GetTall() )
			{
				m_pGamerName->SetFont( m_hFriendsListVerySmallFont );
			}
			else
			{
				m_pGamerName->SetFont( m_hFriendsListSmallFont );
			}
		}
		else
		{
			m_pGamerName->SetFont( m_hFriendsListFont );
		}
	}

	// set the score
	if ( m_pGamerScore )
	{
		m_pGamerScore->SetText( ScoreToString( pData->m_iScore, m_leaderboardType ) );
	}

#if !defined( NO_STEAM )
	m_nSteamID = pData->m_steamIDUser.ConvertToUint64();
#elif defined( _X360 )
	m_nXUID = pData->m_xuid;
#endif

	if ( m_pGamerAvatar )
	{
		vgui::IImage *pImage = NULL;

#if !defined( NO_STEAM )
		pImage = BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID , BaseModUI::CUIGameData::kAvatarImageRequest );
#elif defined( _X360 )
		pImage = CUIGameData::Get()->AccessAvatarImage( m_nXUID, CUIGameData::kAvatarImageRequest );
#endif

		m_pGamerAvatar->SetImage( pImage );
		m_pGamerAvatar->SetVisible( true );
		m_pGamerAvatar->SetAlpha( AVATAR_PIC_MOUSE_NO_HOVER_ALPHA );
	}
}

#if !defined( NO_STEAM ) || defined( _X360 )
#if !defined( NO_STEAM )
void CAvatarPanelItem::SetPlayerData( CSteamID playerID, int nScore, LeaderboardType type )
#elif defined( _X360 )
void CAvatarPanelItem::SetPlayerData( XUID playerID, int nScore, LeaderboardType type, int nController )
#endif
{
	m_leaderboardType = type;

	const char *pPlayerName = 0;



#if !defined( NO_STEAM )
	if ( steamapicontext->SteamFriends() )
	{
		pPlayerName = steamapicontext->SteamFriends()->GetPersonaName();
	}
#elif defined( _X360 )
	char szName[ MAX_PLAYER_NAME_LENGTH ] = "Guest";
	if ( nController != -1 )
	{
		XUserGetName( nController, szName, ARRAYSIZE( szName ) );
	}

	pPlayerName = szName;
#endif

	if ( m_pGamerName )
	{
		m_pGamerName->SetText( pPlayerName );

		wchar_t wchName[ 128 ];
		m_pGamerName->GetText( wchName, sizeof( wchName) );

		int nWide, nTall;
		vgui::surface()->GetTextSize( m_hFriendsListFont, wchName, nWide, nTall );

		if ( nWide + 1 >= m_pGamerName->GetWide() || nTall + 1 >= m_pGamerName->GetTall() )
		{
			vgui::surface()->GetTextSize( m_hFriendsListSmallFont, wchName, nWide, nTall );

			// See if we can get away with small font before resorting to very small
			if ( nWide + 1 >= m_pGamerName->GetWide() || nTall + 1 >= m_pGamerName->GetTall() )
			{
				m_pGamerName->SetFont( m_hFriendsListVerySmallFont );
			}
			else
			{
				m_pGamerName->SetFont( m_hFriendsListSmallFont );
			}
		}
		else
		{
			m_pGamerName->SetFont( m_hFriendsListFont );
		}
	}

	// set the score
	if ( m_pGamerScore )
	{
		m_pGamerScore->SetText( ScoreToString( nScore, m_leaderboardType ) );
	}

#if !defined( NO_STEAM )
	m_nSteamID = playerID.ConvertToUint64();
#elif defined( _X360 )
	m_nXUID = playerID;
#endif

	if ( m_pGamerAvatar )
	{
		vgui::IImage *pImage = NULL;
		
#if !defined( NO_STEAM )
		pImage = BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID , BaseModUI::CUIGameData::kAvatarImageRequest );
#elif defined( _X360 )
		pImage = CUIGameData::Get()->AccessAvatarImage( m_nXUID, CUIGameData::kAvatarImageRequest );
#endif

		m_pGamerAvatar->SetImage( pImage );
		m_pGamerAvatar->SetVisible( true );
		m_pGamerAvatar->SetAlpha( AVATAR_PIC_MOUSE_NO_HOVER_ALPHA );
	}
}
#endif

//=============================================================================
void CAvatarPanelItem::NavigateTo()
{
	if( IsPC() )
	{
		return;
	}

	m_pListCtrlr->SelectPanelItemByPanel( this );
	if ( m_pLeaderboard )
	{
		m_pLeaderboard->SelectPanel( SELECT_AVATAR );
	}

	SetHasMouseover( true );
	RequestFocus();
	int nNumPanels = m_pListCtrlr->GetPanelItemCount();
	for ( int i = 0; i < nNumPanels; ++i )
	{
		CLeaderboardMapItem *pPanel = static_cast< CLeaderboardMapItem *>( m_pListCtrlr->GetPanelItem( i ) );
		if ( pPanel )
		{
			pPanel->SetSelected( false );
		}
	}
	SetSelected( true );
	m_pScoreLegend->SetFgColor( m_FocusColor );
	m_pGamerName->SetFgColor( m_FocusColor );
	m_pGamerScore->SetFgColor( m_FocusColor );
}


//=============================================================================
void CAvatarPanelItem::NavigateFrom()
{
	m_pScoreLegend->SetFgColor( m_TextColor );
	m_pGamerName->SetFgColor( m_TextColor );
	m_pGamerScore->SetFgColor( m_TextColor );
	BaseClass::NavigateFrom();
}


//=============================================================================
void CAvatarPanelItem::OnCursorMoved( int x, int y )
{ 
	if( m_pGamerAvatar )
	{
		if( IsMouseOnGamerPicture() )
		{
			m_pGamerAvatar->SetAlpha( AVATAR_PIC_MOUSE_HOVER_ALPHA );
		}
		else
		{
			m_pGamerAvatar->SetAlpha( AVATAR_PIC_MOUSE_NO_HOVER_ALPHA );
		}
	}

	BaseClass::OnCursorMoved( x, y );
}


//=============================================================================
void CAvatarPanelItem::OnCursorExited( void )
{
	if( m_pGamerAvatar )
	{
		m_pGamerAvatar->SetAlpha( AVATAR_PIC_MOUSE_NO_HOVER_ALPHA );
	}

	BaseClass::OnCursorExited();
}


//=============================================================================
bool CAvatarPanelItem::IsMouseOnGamerPicture( void )
{
	bool bMouseOnPic = false;
	int nCursorPosX, nCursorPosY;
	vgui::input()->GetCursorPos( nCursorPosX, nCursorPosY );

	if( m_pGamerAvatar )
	{
		int picX, picY, picWidth, picHeight;
		m_pGamerAvatar->GetBounds( picX, picY, picWidth, picHeight );
		LocalToScreen( picX, picY );

		if( nCursorPosX >= picX && nCursorPosX <= ( picX + picWidth ) &&
			nCursorPosY >= picY && nCursorPosY <= ( picY + picHeight ) )
		{
			bMouseOnPic = true;
		}
	}

	return bMouseOnPic;
}


//=============================================================================
void CAvatarPanelItem::OnMousePressed( vgui::MouseCode code )
{
	// view the avatar
	if ( code == MOUSE_LEFT )
	{
		if( IsMouseOnGamerPicture() )
		{
			ActivateSelectedItem();
		}
	}
}


//=============================================================================
void CAvatarPanelItem::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	if ( m_pHUDLeaderboard )
	{
		m_pHUDLeaderboard->OnKeyCodePressed( code );
		return;
	}

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		ActivateSelectedItem();
		return;

	case KEY_LEFT:
	case KEY_XBUTTON_LEFT:
	//case KEY_XBUTTON_B:
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
		if ( m_pLeaderboard )
		{
			m_pLeaderboard->SelectPanel( SELECT_MAP, true );
			m_pListCtrlr->ClearPanelSelection();
			NavigateFrom();
		}
		return;

	case KEY_UP:
	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
		if ( m_nAvatarIndex == 0 )
		{
			// select the leaderboard button
			m_pListCtrlr->SelectPanelItem( m_pListCtrlr->GetPanelItemCount() - 1 );
		}
		else
		{
			m_pListCtrlr->SelectPanelItem( m_nAvatarIndex - 1 );
		}
		return;
		
	case KEY_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
		if ( m_nAvatarIndex == m_pListCtrlr->GetPanelItemCount() - 1 )
		{
			m_pListCtrlr->SelectPanelItem( 0 );
		}
		else
		{
			m_pListCtrlr->SelectPanelItem( m_nAvatarIndex + 1 );
		}
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}


//=============================================================================
bool CAvatarPanelItem::ActivateSelectedItem()
{
	if ( IsPS3() )
	{
		return false; // gamercards not supported on PS3
	}

	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr;
#ifdef _X360
	if ( XBX_GetUserIsGuest( iSlot ) )
	{
		iCtrlr = XBX_GetPrimaryUserId();
	}
	else
#endif
	{
		iCtrlr = XBX_GetUserId( iSlot );
	}

	if ( IsX360() ) // don't worry about these checks on PC
	{
		IMatchSystem *pIMatchSystem = g_pMatchFramework->GetMatchSystem();

		if ( !pIMatchSystem )
			return false;

		IPlayerLocal *pPlayerLocal = pIMatchSystem->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( !pPlayerLocal || ( pPlayerLocal->GetOnlineState() == IPlayer::STATE_OFFLINE ) )
			return false;
	}
	
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
#ifdef _X360
	if ( m_nXUID )
	{
		XShowGamerCardUI( iCtrlr, m_nXUID );
		return true;
	}
#else
	if ( m_nSteamID )
	{
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "steamid/%llu", m_nSteamID );
		BaseModUI::CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
		return true;
	}
#endif

	return false;
}

void CAvatarPanelItem::SetScoreLegend( const char *pLegend )
{
	if ( m_pScoreLegend && pLegend != NULL )
	{
		m_pScoreLegend->SetText( pLegend );

		wchar_t wchName[ 128 ];
		m_pScoreLegend->GetText( wchName, sizeof( wchName) );

		int nWide, nTall;
		vgui::surface()->GetTextSize( m_hFriendsListSmallFont, wchName, nWide, nTall );

		if ( nWide + 1 >= m_pScoreLegend->GetWide() || nTall + 1 >= m_pScoreLegend->GetTall() )
		{
			m_pScoreLegend->SetFont( m_hFriendsListVerySmallFont );
		}
		else
		{
			m_pScoreLegend->SetFont( m_hFriendsListSmallFont );
		}
	}
}

void CAvatarPanelItem::SetAsHUDElement( bool bHudElement )
{
	if ( bHudElement )
	{
		m_pLeaderboard = NULL;
		m_pHUDLeaderboard = dynamic_cast< CPortalHUDLeaderboard* >( m_pListCtrlr->GetParent() );
		// move the legend label over
		int nWidth = m_pScoreLegend->GetWide();
		nWidth += m_pHUDLeaderboard->GetAvatarLegendOffset();
		m_pScoreLegend->SetWide( nWidth );
	}
	else
	{
		m_pHUDLeaderboard = NULL;
		m_pLeaderboard = dynamic_cast< CPortalLeaderboardPanel* >( m_pListCtrlr->GetParent() );
	}
}

void AddAvatarPanelItem( CPortalLeaderboard *pLeaderboard, BaseModUI::GenericPanelList *pStatLists, const PortalLeaderboardItem_t *pData, int nScore, LeaderboardType nType, int nPlayerType, int nAvatarIndex, int nHeight, int nSlot, bool bHUDElement /*= false*/ )
{
	CAvatarPanelItem *pItem = pStatLists->AddPanelItem< CAvatarPanelItem >( "portal_leaderboard_avatar_panel" );
	if ( pItem )
	{
		if ( !pData 
#if !defined( NO_STEAM )
			|| pData->m_steamIDUser.ConvertToUint64() == 0
#elif defined( _X360 )
			|| pData->m_xuid == 0
#endif
			)
		{
#if defined( _X360 )
			if ( nSlot == -1 )
			{
				// Find a player who's not a guest
				for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
				{
					if ( !CUIGameData::Get()->IsGuestOrOfflinePlayerWhenSomePlayersAreOnline( k ) )
					{
						nSlot = k;
						break;
					}
				}
			}
#endif

			// get the player's avatar and name
#if !defined( NO_STEAM )
			if ( steamapicontext->SteamUser() )
			{
				pItem->SetPlayerData( steamapicontext->SteamUser()->GetSteamID(), nScore, nType );
			}
#elif defined( _X360 )
			if ( nSlot != -1 )
			{
				pItem->SetPlayerData( pLeaderboard->GetLocalPlayerXUID( nSlot ), nScore, nType, nSlot );
			}
#endif
		}
		else
		{
			pItem->SetPlayerData( pData, nType );
		}

		pItem->SetAvatarIndex( nAvatarIndex );

		if ( nPlayerType == 0 )
		{
			if ( nScore == -1 )
			{
				pItem->SetScoreLegend( "#PORTAL2_Challenge_NoScore" );
			}
			else
			{
				pItem->SetScoreLegend( "#PORTAL2_Challenge_YourBest" );
			}
		}
		else if ( nPlayerType == 1 )
		{
			pItem->SetScoreLegend( "#PORTAL2_Challenge_TopFriend" );
		}
		else if ( nPlayerType == 2 )
		{
			pItem->SetScoreLegend( "#PORTAL2_Challenge_FriendToBeat" );
		}

		pItem->SetTall( nHeight );		
		pItem->SetAsHUDElement( bHUDElement );
	}
}

} // end namespace BaseModUI


void cc_leaderboard_open( const CCommand &args )
{
	if ( args.ArgC() <= 1 )
		return;


	bool bLevelEnd = V_atoi( args.Arg( 1 ) ) != 0; //event->GetBool( "endoflevel" );
	bool bCheated = V_atoi( args.Arg( 2 ) ) != 0;
	if ( bLevelEnd )
	{
		GameUI().ActivateGameUI();
		KeyValues *pLeaderboardValues = new KeyValues( "leaderboard" );
		pLeaderboardValues->SetInt("state", STATE_END_OF_LEVEL);
		pLeaderboardValues->SetBool("cheated", bCheated );
		BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );
		if ( GameRules() && GameRules()->IsMultiplayer() )
		{
			BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_PORTALCOOPLEADERBOARD, pInGameMenu, true, pLeaderboardValues );
		}
		else
		{
			BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_PORTALLEADERBOARD, pInGameMenu, true, pLeaderboardValues );
		}
		pLeaderboardValues->deleteThis();
	}
	else
	{
		CPortalLeaderboardPanel::ResetTempScoreUpdates();
	}
}
static ConCommand leaderboard_open("leaderboard_open", cc_leaderboard_open, "Activate main leaderboard");

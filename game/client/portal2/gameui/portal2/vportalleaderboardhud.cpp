//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"
#include "vportalleaderboardhud.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "vgui/ilocalize.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "vgenericwaitscreen.h"
#include "vgui/portal_leaderboard_graph_panel.h"
#include "gameui/portal2/vgenericpanellist.h"
#include "portal2_leaderboard_manager.h"
#include "vportalleaderboard.h"
#include "vportalchallengestatspanel.h"
#include "portal_gamerules.h"
#include "portal_mp_gamerules.h"
#include "c_portal_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern int g_nPortalScoreTempUpdate;
extern int g_nTimeScoreTempUpdate;
extern int g_nPortalScoreTempMostRecent;
extern int g_nTimeScoreTempMostRecent;

ConVar move_during_ui( "move_during_ui", "false", FCVAR_ARCHIVE, "Allows player movement while UI is visible." );
extern ConVar cl_leaderboard_fake_offline;
extern ConVar cl_leaderboard_fake_io_error;
extern ConVar cl_leaderboard_fake_no_data;

//=============================================================================
static void LeaveGameOkCallback()
{
	COM_TimestampedLog( "Exit Game" );

	CUIGameData::Get()->GameStats_ReportAction( "challenge_quit", engine->GetLevelNameShort(), g_nTimeScoreTempUpdate );

	CPortalLeaderboardPanel* self = 
		static_cast< CPortalLeaderboardPanel* >( CBaseModPanel::GetSingleton().GetWindow( WT_PORTALLEADERBOARDHUD ) );

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


	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}


//=============================================================================
static void GoToHubOkCallback()
{
	GameUI().AllowEngineHideGameUI();
	CPortalLeaderboardPanel* pSelf = 
		static_cast< CPortalLeaderboardPanel* >( CBaseModPanel::GetSingleton().GetWindow( WT_PORTALLEADERBOARDHUD ) );

	if ( pSelf )
	{
		bool bWaitScreen =  CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_GoingToHub", 0.0f, NULL );
		pSelf->PostMessage( pSelf, new KeyValues( "MsgPreGoToHub" ), bWaitScreen ? 2.0f : 0.0f );
	}
}


CPortalHUDLeaderboard::CPortalHUDLeaderboard( Panel *pParent, const char *pPanelName ):
BaseClass( pParent, pPanelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#L4D360UI_MainMenu_SurvivalLeaderboards" );
	
	m_bNeedsUpdate = false;
	m_bEnabled = false;
	m_bOnline = false;
	m_pLeaderboard = NULL;
	m_nSelectedPanelIndex = 0;
	m_leaderboardState = STATE_MAIN_MENU;
	m_bCheated = false;
	m_bCommittedAction = false;

	V_snprintf( m_szNextMap, sizeof(m_szNextMap), "" );

	m_pGraphPanels[LEADERBOARD_PORTAL] = new CPortalLeaderboardGraphPanel( this, "PortalGraph", LEADERBOARD_PORTAL );
	m_pGraphPanels[LEADERBOARD_TIME] = new CPortalLeaderboardGraphPanel( this, "TimeGraph", LEADERBOARD_TIME );

	m_pStatLists[ LEADERBOARD_PORTAL ] = new GenericPanelList( this, "PortalStatList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pStatLists[ LEADERBOARD_PORTAL ]->SetPaintBackgroundEnabled( false );

	m_pStatLists[ LEADERBOARD_TIME ] = new GenericPanelList( this, "TimeStatList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pStatLists[ LEADERBOARD_TIME ]->SetPaintBackgroundEnabled( false );

	m_pChallengeStatsPanel = new CPortalChallengeStatsPanel( this, "PortalChallengeStatsPanel" );
	m_pChallengeStatsPanel->SetVisible( false );

	m_pInvalidLabel = new Label( this, "LblInvalidLeaderboard", "" );
	m_pInvalidLabel2 = new Label( this, "LblInvalidLeaderboard2", "" );
	m_pWorkingAnim = NULL;

	m_pFewestPortalsLabel = new Label( this, "LblFewestPortals", "" );
	m_pFastestTimeLabel = new Label( this, "LblFastestTime", "" );
	m_pEveryoneLabel = new Label( this, "LblEveryone", "" );

	SetFooterEnabled( true );
	UpdateFooter();
}

CPortalHUDLeaderboard::~CPortalHUDLeaderboard()
{
	if ( m_leaderboardState == STATE_START_OF_LEVEL )
	{
		engine->ClientCmd_Unrestricted( "unpause nomsg" );
		engine->ClientCmd_Unrestricted( "sv_pausable 0" );
	}
}

void CPortalHUDLeaderboard::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	// get the chapter and map
	SetMapTitle();
	SetNextMap();

	BaseClass::ApplySchemeSettings( pScheme );

	for ( int i = 0; i < NUM_LEADERBOARDS; ++i )
	{
		m_pStatLists[ i ]->SetScrollBarVisible( false );
		m_pStatLists[ i ]->SetScrollArrowsVisible( false );
	}

	m_bNeedsUpdate = true;

	if ( m_pChallengeStatsPanel )
	{
		if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			m_pChallengeStatsPanel->SetVisible( true );
			m_pChallengeStatsPanel->SetTitleText( "#PORTAL2_FinalScore" );
			UpdateStatPanel();
		}
		// Show the stats panel if we're paused, triggered, or it's the start of a coop map
		else if ( m_leaderboardState == STATE_PAUSE_MENU || m_leaderboardState == STATE_TRIGGERED ||
			( m_leaderboardState == STATE_START_OF_LEVEL && PortalMPGameRules() && PortalMPGameRules()->IsCoOp() ) )
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

	m_pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) );
	if ( m_pWorkingAnim )
	{
		m_pWorkingAnim->SetVisible( false );
	}

	UpdateFooter();

	if ( m_leaderboardState == STATE_START_OF_LEVEL )
	{
		// pause the server in case it is pausable
		engine->ClientCmd_Unrestricted( "sv_pausable 1" );
		engine->ClientCmd_Unrestricted( "setpause nomsg" );
	}
}

void CPortalHUDLeaderboard::Activate()
{
	BaseClass::Activate();
	move_during_ui.SetValue( 1  );
	m_bNeedsUpdate = true;
	
	UpdateFooter();

	m_bCommittedAction = false;

	GameUI().PreventEngineHideGameUI();
}


void CPortalHUDLeaderboard::SetDataSettings( KeyValues *pSettings )
{
	m_leaderboardState = static_cast< LeaderboardState_t >( pSettings->GetInt( "LevelState", (int)STATE_TRIGGERED ) );
	m_bCheated = pSettings->GetBool( "Cheated", false );
	UpdateFooter();

	BaseClass::SetDataSettings( pSettings );
}


void CPortalHUDLeaderboard::OnKeyCodePressed( KeyCode code )
{
	int joystick = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( joystick );

	if ( m_bCommittedAction )
	{
		// Already decided to do something else... don't accept input
		return;
	}

	// predeclare variables to make the switch statement happy
	CAvatarPanelItem *pPanel = NULL;
	unsigned short nSelectedIndex;
	int nOldSelectionIndex;
	bool bMovingLeft = false;

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_UP:
	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
		if ( !IsX360() )
			return;

		pPanel = static_cast< CAvatarPanelItem* >( m_pStatLists[ m_nSelectedPanelIndex ]->GetSelectedPanelItem() );
		if ( pPanel )
		{
			int nCurrentIndex = pPanel->GetAvatarIndex();
			nCurrentIndex = (nCurrentIndex == 0) ? m_pStatLists[ m_nSelectedPanelIndex ]->GetPanelItemCount() - 1 : nCurrentIndex - 1;
			m_pStatLists[ m_nSelectedPanelIndex ]->SelectPanelItem( nCurrentIndex );
		}
		return;

	case KEY_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
		if ( !IsX360() )
			return;
		pPanel = static_cast< CAvatarPanelItem* >( m_pStatLists[ m_nSelectedPanelIndex ]->GetSelectedPanelItem() );
		if ( pPanel )
		{
			int nCurrentIndex = pPanel->GetAvatarIndex();
			nCurrentIndex = (nCurrentIndex + 1) % m_pStatLists[ m_nSelectedPanelIndex ]->GetPanelItemCount();
			m_pStatLists[ m_nSelectedPanelIndex ]->SelectPanelItem( nCurrentIndex );
		}
		return;

	case KEY_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
		bMovingLeft = true;
		// fall through to right
	case KEY_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
		if ( !IsX360() )
			return;
		// don't wrap
		if ( (!bMovingLeft && m_nSelectedPanelIndex == 1) || 
			 (bMovingLeft && m_nSelectedPanelIndex == 0) )
			 return;

		// get the currently selected avatar panel
		pPanel = static_cast< CAvatarPanelItem* >( m_pStatLists[ m_nSelectedPanelIndex ]->GetSelectedPanelItem() );
		Assert( pPanel );
		// set the text/bg color of the exiting selection
		if ( pPanel )
		{
			pPanel->NavigateFrom();
		}
		// get the index of the currently selected item
		// if the panel isn't found for some reason, just default to selecting the top item
		if ( !m_pStatLists[ m_nSelectedPanelIndex ]->GetPanelItemIndex(pPanel, nSelectedIndex ) )
		{
			nSelectedIndex = 0;
		}
		// capture the old panel list and determine the new panel list
		nOldSelectionIndex = m_nSelectedPanelIndex;
		m_nSelectedPanelIndex = (m_nSelectedPanelIndex + 1) % 2;
		// clear the old panel list and focus/select the new panel list
		m_pStatLists[ nOldSelectionIndex ]->ClearPanelSelection();
		m_pStatLists[ m_nSelectedPanelIndex ]->RequestFocus();
		m_pStatLists[ m_nSelectedPanelIndex ]->SelectPanelItem( nSelectedIndex );
		return;

	// exit leaderboard / continue to next map
	case KEY_ENTER:
	case KEY_XBUTTON_A:
		if ( m_leaderboardState == STATE_TRIGGERED || m_leaderboardState == STATE_START_OF_LEVEL )
		{
			GameUI().AllowEngineHideGameUI();
			GameUI().HideGameUI();
			CBaseModPanel::GetSingleton().CloseAllWindows();
			BaseClass::OnKeyCodePressed( KEY_XBUTTON_B );

			m_bCommittedAction = true;
			return;
		}
		else if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			CUIGameData::Get()->GameStats_ReportAction( "challenge_continue", engine->GetLevelNameShort(), g_nTimeScoreTempUpdate );

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
		else
		{
			GameUI().AllowEngineHideGameUI();
			BaseClass::OnKeyCodePressed( KEY_XBUTTON_B );

			m_bCommittedAction = true;
		}
		break;

	// exit to main menu or hub
	case KEY_XBUTTON_B:
		if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			// check for exit to main menu
			ReturnToMainMenu();

			m_bCommittedAction = true;
			return;
		}
		else if ( m_leaderboardState == STATE_TRIGGERED || m_leaderboardState == STATE_START_OF_LEVEL )
		{
			GameUI().AllowEngineHideGameUI();
			GameUI().HideGameUI();
			CBaseModPanel::GetSingleton().CloseAllWindows();

			m_bCommittedAction = true;
		}
		else
		{
			GameUI().AllowEngineHideGameUI();
		}
		break;

	// restart map
	case KEY_XBUTTON_X:
		if ( m_leaderboardState == STATE_END_OF_LEVEL )
		{
			CUIGameData::Get()->GameStats_ReportAction( "challenge_retry", engine->GetLevelNameShort(), g_nTimeScoreTempUpdate );

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

	// view gamertag on xbox, go to hub on PS3
	case KEY_XBUTTON_Y:
		if ( IsX360() )
		{
			// get the selected item
			CAvatarPanelItem *pSelectedPanel = static_cast< CAvatarPanelItem* >( m_pStatLists[ m_nSelectedPanelIndex ]->GetSelectedPanelItem() );
			if ( pSelectedPanel )
			{
				pSelectedPanel->ActivateSelectedItem();
			}
		}
		else if ( GameRules() && GameRules()->IsMultiplayer() && m_leaderboardState == STATE_END_OF_LEVEL )
		{
			CUIGameData::Get()->GameStats_ReportAction( "challenge_hub", engine->GetLevelNameShort(), g_nTimeScoreTempUpdate );
			GoToHub();

			m_bCommittedAction = true;
		}

		break;
	}
	BaseClass::OnKeyCodePressed( code );
}

void CPortalHUDLeaderboard::OnCommand(const char *command )
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );

		m_bCommittedAction = false;
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CPortalHUDLeaderboard::OnThink()
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


	// Make the stats panel update at the start and when paused
	if ( m_pChallengeStatsPanel && ( m_leaderboardState == STATE_START_OF_LEVEL || m_leaderboardState == STATE_PAUSE_MENU ) )
	{
		UpdateStatPanel();
	}

	if ( m_bOnline )
	{
		if ( m_bNeedsUpdate )
		{
			bool bViewSpinner = true;
			bool bHideInvalidLabels = true;

			ClearData();

			if ( m_pLeaderboard == NULL )
			{
				m_pLeaderboard = PortalLeaderboardManager()->GetLeaderboard( engine->GetLevelNameShort() );
			}

			if ( m_pLeaderboard )
			{
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
					}

					if( m_pInvalidLabel2 )
					{
						m_pInvalidLabel2->SetVisible( false );
					}

					UpdateFooter();

					m_bEnabled = false; // don't show the HUD
					m_bNeedsUpdate = false;
					bViewSpinner = false;
					bHideInvalidLabels = false;
				}
				else if ( !m_pLeaderboard->IsQuerying() )
				{
					m_bNeedsUpdate = false;
					Update();

					if( m_pLeaderboard->IsGladosAllowedToSpeak() )
					{
						char szGladosLine [256];
						m_pLeaderboard->DoGladosSpokenReaction(szGladosLine);
						vgui::surface()->PlaySound( szGladosLine );
					}

					bViewSpinner = false;
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
		ClearData();

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


void CPortalHUDLeaderboard::OnClose()
{
	GameUI().AllowEngineHideGameUI();
	BaseClass::OnClose();
}


void CPortalHUDLeaderboard::UpdateFooter()
{	
	if ( m_leaderboardState == STATE_TRIGGERED && IsPC() )
	{
		SetFooterEnabled( false );
	}
	else
	{
		SetFooterEnabled( true );
		CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
		if ( pFooter )
		{
			int visibleButtons = 0;
			if ( m_leaderboardState == STATE_START_OF_LEVEL )
			{
				visibleButtons |= FB_ABUTTON;
				pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_ContinueToCoop" );
				if ( IsX360() )
				{
					if ( m_bOnline )
					{
						visibleButtons |= FB_YBUTTON;
					}
					pFooter->SetButtonText( FB_YBUTTON, "#L4D360UI_GameInfo" );
				}
				pFooter->SetButtons( visibleButtons );
				
			}
			else if ( m_leaderboardState != STATE_END_OF_LEVEL )
			{
				visibleButtons |= FB_BBUTTON;
				pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
				if ( IsX360() )
				{
					if ( m_bOnline )
					{
						visibleButtons |= FB_YBUTTON;
						pFooter->SetButtonText( FB_YBUTTON, "#L4D360UI_GameInfo" );
					}
				}
			}
			else // STATE_END_OF_LEVEL
			{
				// if coop and the next map is locked, no A button
				if ( PortalMPGameRules() && m_szNextMap[0] == '\0' )
				{
					visibleButtons = FB_BBUTTON | FB_XBUTTON;
				}
				else
				{
					visibleButtons = FB_ABUTTON | FB_BBUTTON | FB_XBUTTON;
					pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PlayNextMap" );
				}

				pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_ReplayMap" );
				pFooter->SetButtonText( FB_BBUTTON, "#PORTAL2_ButtonAction_Exit" );

				// if multiplayer allow going to the hub
				if ( PortalMPGameRules() && PortalMPGameRules()->IsCoOp() )
				{
					if ( m_bOnline )
					{
						visibleButtons |= FB_YBUTTON;
						pFooter->SetButtonText( FB_YBUTTON, "#Portal2UI_GoToHubStdCase" );
					}

					// coop Xbox just says "Exit" on B button
					if ( IsX360() )
					{
						pFooter->SetButtonText( FB_BBUTTON, "#PORTAL2_ButtonAction_Exit" );
					}
				}
				
				// if Xbox, always use Y button as Gamertag access
				if ( IsX360() )
				{
					if ( m_bOnline )
					{
						visibleButtons |= FB_YBUTTON;
						pFooter->SetButtonText( FB_YBUTTON, "#L4D360UI_GameInfo" );
					}
				}
			}

			pFooter->SetButtons( visibleButtons );
		}
	}
}

void CPortalHUDLeaderboard::Update()
{
	m_pFewestPortalsLabel->SetVisible( true );
	m_pFastestTimeLabel->SetVisible( true );
	m_pEveryoneLabel->SetVisible( true );

	for ( int i = 0; i < NUM_LEADERBOARDS; ++i )
	{
		m_pGraphPanels[ i ]->SetVisible( true );
		UpdateLeaderboard( (LeaderboardType)i );
	}

	// select the top left stat item by default
	if ( IsX360() && m_pStatLists[0] && m_pStatLists[0]->GetPanelItemCount() > 0 )
	{
		m_pStatLists[0]->RequestFocus();
		m_pStatLists[0]->SelectPanelItem( 0 );
		m_nSelectedPanelIndex = 0;
	}

	InvalidateLayout( false );
}

void CPortalHUDLeaderboard::UpdateLeaderboard( LeaderboardType type )
{
	if ( !m_pLeaderboard )
		return;

	m_pLeaderboard->UpdateXUIDs();

	m_pGraphPanels[type]->UpdateGraph( m_pLeaderboard, type );

	if ( m_pStatLists[ type ] )
	{
		m_pStatLists[ type ]->RemoveAllPanelItems();

		if ( !m_pLeaderboard )
			return;

		int nPlayerIndex = m_pLeaderboard->GetCurrentPlayerIndex( type );
		int nOldIndex = -2;
		int nPlacementIndex = nPlayerIndex;
		int nIndexList[ 3 ] = { 0, 1, 2 };

		// get this player's score
		const PortalLeaderboardItem_t *pPlayerData[ 2 ];
		pPlayerData[ 0 ] = m_pLeaderboard->GetCurrentBest( type );
		pPlayerData[ 1 ] = m_pLeaderboard->GetCurrentBest( type, 1 );
		const PortalLeaderboardItem_t *pData;

		int nSpecialScore = -1;
		PortalLeaderboardItem_t tempUpdate;

		if (  V_strcmp( engine->GetLevelNameShort(), m_pLeaderboard->GetMapName() ) == 0 )
		{
			if ( g_nPortalScoreTempUpdate != -1 && type == LEADERBOARD_PORTAL )
			{
				nSpecialScore = g_nPortalScoreTempUpdate;
			}
			else if ( g_nTimeScoreTempUpdate != -1 && type == LEADERBOARD_TIME )
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
						pData = m_pLeaderboard->GetPlayerAtIndex( nNewIndex, type );
						// loop thru existing scores to find player's new index
						while ( pData )
						{
							if ( pData->m_iScore >= pPlayerData[ iSlot - nOffset ]->m_iScore )
								break;

							++nNewIndex;
							pData = m_pLeaderboard->GetPlayerAtIndex( nNewIndex, type );
						}
						nPlayerIndex = nNewIndex;
					}

					// otherwise start at the player's old score index
					// and loop until we find where the new score fits
					for ( int i = nOldIndex; i >= 0; --i )
					{
						// get the player at each position beneath the old score's position
						pData = m_pLeaderboard->GetPlayerAtIndex( i, type );
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
				pData = m_pLeaderboard->GetPlayerAtIndex( 0, type );
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
				pData = m_pLeaderboard->GetPlayerAtIndex( 0, type );
				Assert( pData );
				if ( pPlayerData[ 0 ]->m_iScore <= pData->m_iScore )
				{
					nIndexList[ 0 ] = nPlacementIndex;
					nIndexList[ 1 ] = 0;
					nIndexList[ 2 ] = 1;
				}
				else  // check for a tie w/ #1
				{
					pData = m_pLeaderboard->GetPlayerAtIndex( 1, type );
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
							nIndexList[ 1 ] = m_pLeaderboard->GetNextTargetIndex( type );
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

			m_pGraphPanels[ type ]->SetPlayerScore( pPlayerData[ 0 ]->m_iScore );
		}
		else // no player data for map
		{
			// set second as worst
			int nWorstRank = m_pLeaderboard->GetWorstPlayerIndex( type );
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

			m_pGraphPanels[ type ]->SetPlayerScore( -1 );
		}
		
		int nCurrentAvatarIndex = 0;
		bool bUsedTarget = false;

#if defined( _X360 )
		int nNumNonGuestPlayers = m_pLeaderboard->NumXUIDs();

		if ( nNumNonGuestPlayers > 1 )
		{
			int nScore = ( pPlayerData[ 0 ] ? pPlayerData[ 0 ]->m_iScore : -1 );
			AddAvatarPanelItem( m_pLeaderboard, m_pStatLists[ type ], pPlayerData[ 0 ], nScore, type, 0, nCurrentAvatarIndex, m_nStatHeight, 0, true );

			nCurrentAvatarIndex++;

			nScore = ( pPlayerData[ 1 ] ? pPlayerData[ 1 ]->m_iScore : -1 );
			AddAvatarPanelItem( m_pLeaderboard, m_pStatLists[ type ], pPlayerData[ 1 ], nScore, type, 0, nCurrentAvatarIndex, m_nStatHeight, 1, true );

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
				AddAvatarPanelItem( m_pLeaderboard, m_pStatLists[ type ], pPlayerData[ 0 ], nScore, type, 0, nCurrentAvatarIndex, m_nStatHeight, -1, true );
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
					pData = m_pLeaderboard->GetPlayerAtIndex( nIndexList[ i ], type );

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

					AddAvatarPanelItem( m_pLeaderboard, m_pStatLists[ type ], pData, nScore, type, nPlayerType, nCurrentAvatarIndex, m_nStatHeight, -1, true );
					nCurrentAvatarIndex++;
				}
			}
		}
	}
}

void CPortalHUDLeaderboard::ClearData()
{
	// clear the graphs	and player stats
	for ( int i = 0; i < NUM_LEADERBOARDS; ++i )
	{
		m_pGraphPanels[ i ]->ClearData();
		m_pStatLists[ i ]->RemoveAllPanelItems();
	}

	m_pGraphPanels[LEADERBOARD_PORTAL]->SetVisible( false );
	m_pGraphPanels[LEADERBOARD_TIME]->SetVisible( false );

	m_pFewestPortalsLabel->SetVisible( false );
	m_pFastestTimeLabel->SetVisible( false );
	m_pEveryoneLabel->SetVisible( false );
}

void CPortalHUDLeaderboard::SetMapTitle()
{
	// set the map name
	bool bInCoop = GameRules()->IsMultiplayer();
	int nChapterNum = CBaseModPanel::GetSingleton().MapNameToChapter( engine->GetLevelNameShort(), !bInCoop );
	int nMapNum = CBaseModPanel::GetSingleton().GetMapNumInChapter( nChapterNum, engine->GetLevelNameShort(), !bInCoop );

	if ( nMapNum > 0 )
	{
		wchar_t *pChapterTitle = NULL;
		wchar_t *pMapTitle = NULL;

		pChapterTitle = bInCoop ? g_pVGuiLocalize->Find( CFmtStr( "#COOP_PRESENCE_TRACK_TRACK%d", nChapterNum ) )
								: g_pVGuiLocalize->Find( CFmtStr( "#SP_PRESENCE_TEXT_CH%d", nChapterNum ) );
		pMapTitle = bInCoop ? g_pVGuiLocalize->Find( CFmtStr("#COOP_MAP_NAME_TR%d_MAP%d", nChapterNum, nMapNum ) ) 
			                : g_pVGuiLocalize->Find( CFmtStr("#SP_MAP_NAME_CH%d_MAP%d", nChapterNum, nMapNum ) );

		if ( !pChapterTitle || !pMapTitle )
			return;

		char szChapterTitle[64];
		char szMapTitle[64];
		char szTotalString[256];
		wchar_t szFinalTitle[256];

		// convert the chapter and map titles to ANSI for ease of handling
		g_pVGuiLocalize->ConvertUnicodeToANSI( pChapterTitle, szChapterTitle, sizeof( szChapterTitle ) );
		g_pVGuiLocalize->ConvertUnicodeToANSI( pMapTitle, szMapTitle, sizeof( szMapTitle ) );

		// find the actual name of the chapter
		char *pHeaderPrefix = V_strstr( szChapterTitle, "\n" );
		if ( pHeaderPrefix )
		{
			++pHeaderPrefix;
		}
		else
		{
			pHeaderPrefix = szChapterTitle;
		}

		// set up the actual string we want on screen
		V_snprintf( szTotalString, sizeof( szTotalString ), "%s - %s", pHeaderPrefix, szMapTitle );
		// convert back to Unicode so we can set the subtitle
		g_pVGuiLocalize->ConvertANSIToUnicode( szTotalString, szFinalTitle, sizeof( szFinalTitle ) );
		// set the panel's subtitle
		SetDialogSubTitle(0, szFinalTitle );
	}

	
}

void CPortalHUDLeaderboard::SetNextMap()
{
	bool bSinglePlayerMode = !GameRules()->IsMultiplayer();
	int nCurrentChapterNumber = CBaseModPanel::GetSingleton().MapNameToChapter( engine->GetLevelNameShort(), bSinglePlayerMode );

	KeyValues *pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nCurrentChapterNumber, !bSinglePlayerMode );
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
				pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapter, !bSinglePlayerMode );

				if ( !pChallengeMapList )
				{
					// Wrap to first chapter
					nChapter = 1;
					pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapter, !bSinglePlayerMode );
				}

				// there should always be another map, or another (or the first) chapter
				Assert( pChallengeMapList );

				if ( pChallengeMapList )
				{
					// First map
					pNextMap = pChallengeMapList->GetFirstSubKey();
				}

				// single player locked map should wrap to first map of first chapter
				nNextMapNumber = CBaseModPanel::GetSingleton().GetMapNumInChapter( nChapter, pNextMap->GetString(), bSinglePlayerMode );
				Assert( nNextMapNumber != -1 ); // should always have a valid map number
				if ( bSinglePlayerMode && IsMapLocked(nChapter, nNextMapNumber, bSinglePlayerMode) )
				{
					// go to first map of first chapter
					nChapter = 1;
					pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapter, !bSinglePlayerMode );
					pNextMap = pChallengeMapList->GetFirstSubKey(); // should always be unlocked ( to be able to play in the 1st place)
				}
			}

			// there should always be another map, or a first map of another (or the first) chapter
			Assert( pNextMap );

			// get the map number
			nNextMapNumber = CBaseModPanel::GetSingleton().GetMapNumInChapter( nChapter, pNextMap->GetString(), bSinglePlayerMode );
			Assert( nNextMapNumber != -1 ); // should always have a valid map number

			if ( pNextMap && !IsMapLocked( nChapter, nNextMapNumber, bSinglePlayerMode ) )
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


void CPortalHUDLeaderboard::UpdateStatPanel()
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

void CPortalHUDLeaderboard::ReturnToMainMenu()
{
	if ( PortalMPGameRules() && PortalMPGameRules()->IsCoOp() && IsX360() )
	{
		GameUI().AllowEngineHideGameUI();
		CBaseModPanel::GetSingleton().OpenWindow( WT_COOPEXITCHOICE, this, true );
		return;
	}

	// main menu confirmation panel
	if ( IsPC() )
	{
		GameUI().PreventEngineHideGameUI();
	}
	else
	{
		GameUI().AllowEngineHideGameUI();
	}
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

	confirmation->SetUsageData(data);
}

void CPortalHUDLeaderboard::GoToHub()
{
	GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#Portal2UI_GoToHubQ";
	data.pMessageText = "#Portal2UI_GoToHubConfMsg";
	data.pfnOkCallback = &GoToHubOkCallback;

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	confirmation->SetUsageData(data);
}

void CPortalHUDLeaderboard::MsgPreGoToHub()
{
	engine->ServerCmd( "pre_go_to_hub" );
	PostMessage( this, new KeyValues( "MsgGoToHub" ), 1.0f );
}

void CPortalHUDLeaderboard::MsgGoToHub()
{
	engine->ServerCmd( "go_to_hub" );
}

void CPortalHUDLeaderboard::MsgRetryMap()
{
	//replay this map
	if( GameRules() && GameRules()->IsMultiplayer() )
	{
		engine->ServerCmd( "mp_restart_level" );
	}
	else
	{
		engine->ServerCmd( "restart_level" );
	}
}

void CPortalHUDLeaderboard::MsgGoToNext()
{
	if ( m_szNextMap[ 0 ] != '\0')
	{
		engine->ServerCmd( VarArgs( "select_map %s\n", m_szNextMap ) );
		GameUI().AllowEngineHideGameUI();
		engine->ExecuteClientCmd("gameui_hide");
		CBaseModPanel::GetSingleton().CloseAllWindows();
	}
}

void CPortalHUDLeaderboard::ClockSpinner( bool bVisible )
{
	if ( m_pWorkingAnim )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pWorkingAnim->SetFrame( nAnimFrame );
		m_pWorkingAnim->SetVisible( bVisible );
	}
}

bool IsChallengeMap( const char *pLevelName )
{
	if ( !pLevelName )
		return false;

	// check whether we're singleplayer or coop
	bool bMultiplayer = GameRules() ? GameRules()->IsMultiplayer() : false;
	// get the chapter/track number
	int nChapterNum = BaseModUI::CBaseModPanel::GetSingleton().MapNameToChapter( pLevelName, !bMultiplayer );
	// retrieve the list of challenge maps
	KeyValues *pChallengeMapList = PortalLeaderboardManager()->GetChallengeMapsFromChapter( nChapterNum, bMultiplayer );

	if ( !pChallengeMapList )
		return false;

	// find our map in the list of challenge maps
	for ( KeyValues *pCurrentMap = pChallengeMapList->GetFirstSubKey(); pCurrentMap != NULL; pCurrentMap = pCurrentMap->GetNextKey() )
	{
		if( !V_stricmp( pLevelName, pCurrentMap->GetString() ) )
		{
			// we found the map name, so it is a challenge map
			return true;
		}
	}
	// name wasn't found, not a challenge map
	return false;
}


void OpenPortalHUDLeaderboard( LeaderboardState_t leaderboardState = STATE_MAIN_MENU )
{
	bool bOpenHUD = false;

	if ( GameRules() )
	{
		if ( GameRules()->IsMultiplayer() )
		{
			if ( PortalMPGameRules()->IsChallengeMode() && IsChallengeMap( engine->GetLevelNameShort() ) )
			{
				bOpenHUD = true;
			}
		}
		else
		{
			if ( PortalGameRules()->IsChallengeMode() && IsChallengeMap( engine->GetLevelNameShort() ) )
			{
				bOpenHUD = true;
			}
		}
	}

	if ( bOpenHUD )
	{
		GameUI().ActivateGameUI();
		BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );
		if ( IsPC() )
		{
			GameUI().PreventEngineHideGameUI();
		}
		KeyValues *pLeaderboardValues = new KeyValues( "leaderboard" );
		pLeaderboardValues->SetInt( "LevelState", (int)leaderboardState );
		BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_PORTALLEADERBOARDHUD, pInGameMenu, true, pLeaderboardValues );
	}
}


bool CPortalHUDLeaderboard::IsMapLocked( int nChapterNumber, int nMapNumber, bool bSinglePlayer )
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

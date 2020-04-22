//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGameSettings.h"
#include "KeyValues.h"

#include <ctype.h>
#include <vstdlib/random.h>

#include "VDropDownMenu.h"
#include "VHybridButton.h"
#include "VFooterPanel.h"
#include "vgui/ISurface.h"
#include "EngineInterface.h"
#include "VLoadingProgress.h"
#include "VGenericConfirmation.h"

#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"

#include "fmtstr.h"
#include "smartptr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar ui_game_allow_create_public( "ui_game_allow_create_public", IsPC() ? "1" : "0", FCVAR_DEVELOPMENTONLY, "When set user can create public lobbies instead of matching" );
ConVar ui_game_allow_create_random( "ui_game_allow_create_random", "1", FCVAR_DEVELOPMENTONLY, "When set, creating a game will pick a random mission" );

namespace BaseModUI
{

	FlyoutMenu * UpdateChapterFlyout( KeyValues *pSettings, FlyoutMenuListener *pListener, DropDownMenu *pChapterDropDown )
	{
		if ( !pSettings || !pChapterDropDown )
			return NULL;

		KeyValues *pAllMissions = g_pMatchExt->GetAllMissions();
		if ( !pAllMissions )
			return NULL;

		char const *szMode = pSettings->GetString( "game/mode", "coop" );
		if ( !szMode || !*szMode )
			return NULL;
		
		char const *szMission = pSettings->GetString( "game/campaign", NULL );
		if ( !szMission || !*szMission )
			return NULL;

		KeyValues *pMission = pAllMissions->FindKey( szMission );
		if ( !pMission )
			return NULL;

		KeyValues *pMode = pMission->FindKey( CFmtStr( "modes/%s", szMode ) );
		if ( !pMode )
			return NULL;

		// Set the proper flyout
		int nMaxChapters = pMode->GetInt( "chapters" );
		if ( nMaxChapters <= 0 )
			return NULL;

		int nAnyChapter = !!pSettings->GetInt( "game/any_chapter_allowed", 0 );
		
		pChapterDropDown->SetFlyout( CFmtStr( "FlmChapter%d", MIN( MAX( nMaxChapters + nAnyChapter, 2 ), FlyoutMenu::s_numSupportedChaptersFlyout ) ) );
		FlyoutMenu *flyout = pChapterDropDown->GetCurrentFlyout();
		if ( !flyout )
			return NULL;

		// Set listener
		flyout->SetListener( pListener );

		// Set chapters
		for ( int jj = 1; jj <= FlyoutMenu::s_numSupportedChaptersFlyout; ++ jj )
		{
			if ( vgui::Button *pChapterBtn = flyout->FindChildButtonByCommand( CFmtStr( "#L4D360UI_Chapter_%d", jj ) ) )
			{
				if ( char const *szName = pMode->GetString( CFmtStr( "%d/displayname", jj ), NULL ) )
				{
					pChapterBtn->SetText( szName );
					pChapterBtn->SetEnabled( true );
				}
				else if ( nAnyChapter )
				{
					pChapterBtn->SetText( "#L4D360UI_Chapter_Any" );
					pChapterBtn->SetEnabled( true );
				}
				else
				{
					pChapterBtn->SetText( "#L4D360UI_Chapter_Only1Avail" );
					pChapterBtn->SetEnabled( false );
				}
			}
		}

		// pChapterDropDown->SetEnabled( nMaxChapters + nAnyChapter > 1 );

		return flyout;
	}

	int GetNumChaptersForMission( KeyValues *pSettings )
	{
		if ( !pSettings )
			return -1;

		KeyValues *pAllMissions = g_pMatchExt->GetAllMissions();
		if ( !pAllMissions )
			return -1;

		char const *szMode = pSettings->GetString( "game/mode", "coop" );
		if ( !szMode || !*szMode )
			return -1;

		char const *szMission = pSettings->GetString( "game/campaign", NULL );
		if ( !szMission || !*szMission )
			return -1;

		KeyValues *pMission = pAllMissions->FindKey( szMission );
		if ( !pMission )
			return -1;

		KeyValues *pMode = pMission->FindKey( CFmtStr( "modes/%s", szMode ) );
		if ( !pMode )
			return -1;

		// Get num chapters
		int nMaxChapters = pMode->GetInt( "chapters" );
		if ( nMaxChapters <= 0 )
			return -1;

		return nMaxChapters;
	}

	KeyValues *GetMapInfoAllowingAnyChapter( KeyValues *pSettings, KeyValues **ppMissionInfo )
	{
		KeyValues *kvMapInfo = pSettings;
		if ( kvMapInfo && !kvMapInfo->GetInt( "game/chapter", 0 ) )
		{
			kvMapInfo = kvMapInfo->MakeCopy();
			kvMapInfo->SetInt( "game/chapter", 1 );
		}

		KeyValues *pMapInfoResult = g_pMatchExt->GetMapInfo( kvMapInfo, ppMissionInfo );

		if ( kvMapInfo && kvMapInfo != pSettings )
			kvMapInfo->deleteThis();

		return pMapInfoResult;
	}

	KeyValues *GetAnyMissionKeyValues()
	{
		static KeyValues *s_pAnyMission = NULL;
		if ( !s_pAnyMission )
		{
			s_pAnyMission = new KeyValues( "" );
			s_pAnyMission->SetString( "name", "#L4D360UI_Campaign_Any" );
			s_pAnyMission->SetString( "displaytitle", "#L4D360UI_Campaign_Any" );
			s_pAnyMission->SetString( "version", "" );
			s_pAnyMission->SetInt( "builtin", 1 );
		}
		return s_pAnyMission;
	}

	KeyValues *GetAnyChapterKeyValues()
	{
		static KeyValues *s_arrAnyChapters[] = { NULL, NULL };
		static int s_idxAnyChapter = 0;
		
		KeyValues *s_pAnyChapter = s_arrAnyChapters[ ( s_idxAnyChapter ++ ) % ARRAYSIZE( s_arrAnyChapters ) ];
		if ( !s_pAnyChapter )
		{
			s_pAnyChapter = new KeyValues( "0" );
		}
		s_pAnyChapter->SetString( "displayname", "#L4D360UI_Chapter_Any" );
		s_pAnyChapter->SetString( "image", "maps/any" );
		s_pAnyChapter->SetInt( "chapter", 0 );
		return s_pAnyChapter;
	}

	KeyValues *GetMapInfoRespectingAnyChapter( KeyValues *pSettings, KeyValues **ppMissionInfo )
	{
		char const *szMission = pSettings->GetString( "game/campaign", "" );
		if ( !szMission || !*szMission )
		{
			// ANY MISSION
			if ( ppMissionInfo )
				*ppMissionInfo = GetAnyMissionKeyValues();
			return GetAnyChapterKeyValues();
		}

		// Mission is set, check if zero chapter
		int nChapter = pSettings->GetInt( "game/chapter", 0 );

		KeyValues *pMissionInfo = NULL;
		KeyValues *pResult = GetMapInfoAllowingAnyChapter( pSettings, &pMissionInfo );
		if ( ppMissionInfo )
			*ppMissionInfo = pMissionInfo;
		if ( !pResult )
			return NULL;

		if ( !nChapter )
		{
			KeyValues *pAnyChapter = GetAnyChapterKeyValues();
			if ( pMissionInfo )
			{
				const char *szMissionImage = pMissionInfo->GetString( "image", NULL );
				if ( szMissionImage )
				{
					pAnyChapter->SetString( "image", pMissionInfo->GetString( "image" ) );
				}
				else
				{
					// Use the first chapter image
					if ( pResult )
					{
						pAnyChapter->SetString( "image", pResult->GetString( "image" ) );
					}
				}
			}
			pResult = pAnyChapter;
		}

		return pResult;
	}
};

using namespace vgui;
using namespace BaseModUI;


//=============================================================================
GameSettings::GameSettings( vgui::Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, false ),
	m_pSettings( NULL ),
	m_autodelete_pSettings( (KeyValues *)NULL ),
	m_drpDifficulty( NULL ),
	m_drpRoundsLimit( NULL ),
	m_drpMission( NULL ),
	m_drpChapter( NULL ),
	m_drpCharacter( NULL ),
	m_drpGameAccess( NULL ),
	m_drpServerType( NULL ),
	m_bEditingSession( false ),
	m_bAllowChangeToCustomCampaign( true ),
	m_bPreventSessionModifications( false )
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );
	SetFooterEnabled( true );
	SetCancelButtonEnabled( true );
}

//=============================================================================
GameSettings::~GameSettings()
{
}

void GameSettings::SetDataSettings( KeyValues *pSettings )
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	KeyValues *pMatchSettings = pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;

	if ( pMatchSettings && ( !pSettings || pSettings == pMatchSettings ) )
	{
		m_pSettings = pMatchSettings;
		m_bEditingSession = true;
	}
	else
	{
		Assert( !m_pSettings );

		m_pSettings = pSettings ? pSettings->MakeCopy() : new KeyValues( "settings" );
		m_autodelete_pSettings.Assign( m_pSettings );
		m_bEditingSession = false;
	}
}

void GameSettings::UpdateSessionSettings( KeyValues *pUpdate )
{
	if ( m_bEditingSession )
	{
		if ( m_bPreventSessionModifications )
			return;

		IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pIMatchSession )
		{
			pIMatchSession->UpdateSessionSettings( pUpdate );
		}
	}
	else
	{
		m_pSettings->MergeFrom( pUpdate );
	}
}

void GameSettings::PaintBackground()
{
	char chBufferTitle[128];
	const char *pTitle = "";

	char chBufferSubTitle[128];
	const char *pSubtitle = "#L4D360UI_GameSettings_Description";

	char const *szNetwork = m_pSettings->GetString( "system/network", "offline" );

	if ( !Q_stricmp( "offline", szNetwork ) )
	{
		char const *szOptionsPlay = m_pSettings->GetString( "options/play", "" );

		if ( !Q_stricmp( "commentary", szOptionsPlay ) )
			pTitle = "#L4D360UI_GameSettings_Commentary";
		else if ( m_pSettings->GetInt( "members/numPlayers" ) > 1 )
			pTitle = "#L4D360UI_GameSettings_OfflineCoop";
		else
			pTitle = "#L4D360UI_GameSettings_Solo";
	}
	else
	{
		Q_snprintf( chBufferTitle, sizeof( chBufferTitle ), "#L4D360UI_GameSettings_MP_%s", m_pSettings->GetString( "game/mode", "coop" ) );
		pTitle = chBufferTitle;

		char const *szAccess = m_pSettings->GetString( "system/access", "public" );
		if ( !Q_stricmp( "lan", szNetwork ) )
			szAccess = szNetwork;

		Q_snprintf( chBufferSubTitle, sizeof( chBufferSubTitle ), "#L4D360UI_Access_%s", szAccess );
		pSubtitle = chBufferSubTitle;
	}

	BaseClass::DrawDialogBackground( pTitle, NULL, pSubtitle, NULL );
}

//=============================================================================
void GameSettings::GenerateDefaultMissionAndChapter( char const *&szMission, int &nChapter )
{
	char const *szGameMode = m_pSettings->GetString( "game/mode" );
	CFmtStr sModeChapters( "modes/%s/chapters", szGameMode );
	bool bSingleChapter = GameModeIsSingleChapter( szGameMode );

	CUtlVector< KeyValues * > arrBuiltinMissions;

	for ( KeyValues *pMission = g_pMatchExt->GetAllMissions()->GetFirstTrueSubKey(); pMission; pMission = pMission->GetNextTrueSubKey() )
	{
		if ( !pMission->GetInt( "builtin" ) )
			continue;

		if ( !Q_stricmp( "credits", pMission->GetString( "name" ) ) )
			continue;

		int numChapters = pMission->GetInt( sModeChapters );
		if ( !numChapters )
			continue;

		// Weigh missions proportionally to the number of chapters
		// if the game mode is a single chapter
		arrBuiltinMissions.AddToTail( pMission );
		if ( bSingleChapter )
		{
			for ( int k = 1; k < numChapters; ++ k )
				arrBuiltinMissions.AddToTail( pMission );
		}
	}

	if ( !arrBuiltinMissions.Count() )
		return;

	if ( !Q_stricmp( "coop", szGameMode ) ||
		 !Q_stricmp( "realism", szGameMode ) ||
		 !ui_game_allow_create_random.GetBool() )
	{
		// Prefer to create Campaign and Realism games on first mission
		szMission = arrBuiltinMissions[0]->GetString( "name" );
		nChapter = 1;
		return;
	}

	// Select a random mission
	int iRandom = RandomInt( 0, arrBuiltinMissions.Count() - 1 );
	KeyValues *pRandomMission = arrBuiltinMissions[ iRandom ];
	szMission = pRandomMission->GetString( "name" );

	// Select a random chapter for single-chapter games
	nChapter = bSingleChapter ? RandomInt( 1, pRandomMission->GetInt( sModeChapters ) ) : 1;
}

//=============================================================================
void GameSettings::Activate()
{
	BaseClass::Activate();

	CAutoPushPop< bool > autoPreventSessionModification( m_bPreventSessionModifications, true );

	char const *szMission = m_pSettings->GetString( "game/campaign", "" );
	int nChapterIndex = m_pSettings->GetInt( "game/chapter", 1 );

	if ( !*szMission && !IsCustomMatchSearchCriteria() && !IsAnyChapterAllowed() )
	{
		GenerateDefaultMissionAndChapter( szMission, nChapterIndex );
	}

	KeyValues *pMissionInfo = NULL;
	KeyValues *pChapterInfo = GetMapInfoAllowingAnyChapter( m_pSettings, &pMissionInfo );
	pChapterInfo;

	if ( szMission && *szMission && !pChapterInfo )
	{
		// Bad case, this would mean that we inherited settings on a mission
		// that we don't support, fall back to the supported mission
		GenerateDefaultMissionAndChapter( szMission, nChapterIndex );
	}

	char const *szNetwork = m_pSettings->GetString( "system/network", "offline" );

 	if ( m_drpCharacter )
	{
		m_drpCharacter->SetFlyout( "FlmCharacterFlyout" );
		m_drpCharacter->SetCurrentSelection( "character_" );

		FlyoutMenu* flyout = m_drpCharacter->GetCurrentFlyout();
		if( flyout )
		{
			flyout->CloseMenu( NULL );
		}
	}

	bool showGameAccess = !Q_stricmp( "create", m_pSettings->GetString( "options/action", "" ) ) &&
							!IsEditingExistingLobby();

	bool showServerType = !Q_stricmp( "LIVE", szNetwork );
	
	// On X360 we cannot allow selecting server type until the
	// session is actually created
	if ( IsX360() && showServerType )
		showServerType = IsEditingExistingLobby();

	bool showSearchControls = IsCustomMatchSearchCriteria();

#ifdef _X360
	bool bPlayingSplitscreen = XBX_GetNumGameUsers() > 1;
#else
	bool bPlayingSplitscreen = false;
#endif

	bool showSinglePlayerControls = !Q_stricmp( "offline", szNetwork ) && !bPlayingSplitscreen;

	m_bBackButtonMeansDone = ( !showSearchControls && !showSinglePlayerControls && !showGameAccess );
	m_bCloseSessionOnClose = showSinglePlayerControls;

	if ( m_drpDifficulty )
	{
		m_drpDifficulty->SetCurrentSelection( CFmtStr( "#L4D360UI_Difficulty_%s",
			m_pSettings->GetString( "game/difficulty", "normal" ) ) );

		if ( FlyoutMenu* flyout = m_drpDifficulty->GetCurrentFlyout() )
			flyout->CloseMenu( NULL );
	}

	if ( m_drpRoundsLimit )
	{
		m_drpRoundsLimit->SetCurrentSelection( CFmtStr( "#L4D360UI_RoundLimit_%d",
			m_pSettings->GetInt( "game/maxrounds", 3 ) ) );

		if ( FlyoutMenu* flyout = m_drpRoundsLimit->GetCurrentFlyout() )
			flyout->CloseMenu( NULL );
	}

	//
	// Drop down for mission selection
	//
	DropDownMenu* hiddenControl = NULL;
	if ( showSearchControls || IsAnyChapterAllowed() )
	{
		m_drpMission = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMissionExtended" ) );
		hiddenControl = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMission" ) );
	}
	else
	{
		m_drpMission = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMission" ) );
		hiddenControl = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMissionExtended" ) );
	}

	if ( m_drpMission )
		m_drpMission->SetVisible( true );
	if ( hiddenControl )
		hiddenControl->SetVisible( false );

	// If we have an active control, navigate from it since we'll be setting a new one
	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateFrom();
	}

	if ( IsX360() && m_bBackButtonMeansDone )
	{
		// No special button needed in these menus as backing out is considered "Done"
		// There's effectively no way to "Cancel"

		if ( m_drpMission && m_drpMission->IsVisible() )
		{
			m_drpMission->NavigateTo();
		}
	}

	BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnStartGame" ) );
	if( button )
	{
		button->SetVisible( showSinglePlayerControls );
		SetControlVisible( "IconForwardArrow", showSinglePlayerControls );

		if ( IsX360() && button->IsVisible() )
		{
			button->NavigateTo();
		}
	}

	button = dynamic_cast< BaseModHybridButton* > ( FindChildByName( "BtnJoinStart" ) );
	if ( button )
	{
		if ( IsX360() && button->IsVisible() )
		{
			button->NavigateTo();
		}
	}

	button = dynamic_cast< BaseModHybridButton* > ( FindChildByName( "BtnStartLobby" ) );
	if ( button )
	{
		button->SetVisible( showGameAccess );
		if ( IsX360() && button->IsVisible() )
		{
			button->NavigateTo();
		}
	}

	if ( IsPC() )
	{
		SetControlVisible( "BtnCancel", true );
		
		if ( m_drpMission && m_drpMission->IsVisible() )
		{
			m_drpMission->NavigateTo();
		}
	}

	if ( m_drpServerType )
	{
		m_drpServerType->SetEnabled( showServerType );
	}

	if ( m_drpMission )
	{
		if ( pMissionInfo && !pMissionInfo->GetInt( "builtin" ) )
		{
			KeyValues *pEvent = new KeyValues( "OnCustomCampaignSelected" );
			pEvent->SetString( "campaign", pMissionInfo->GetString( "name" ) );
			pEvent->SetInt( "chapter", -10 );
			PostMessage( this, pEvent );
			m_drpMission->SetCurrentSelection( pMissionInfo->GetString( "displayname" ) );
		}
		else
		{
			m_drpMission->SetCurrentSelection( CFmtStr( "cmd_campaign_%s", szMission ) );
		}

		if( m_drpChapter )
		{
			KeyValues *kvUpdateChapterFlyout( m_pSettings ? m_pSettings->MakeCopy() : NULL );
			KeyValues::AutoDelete autodelete_kvUpdateChapterFlyout( kvUpdateChapterFlyout );

			if ( kvUpdateChapterFlyout )
			{
				kvUpdateChapterFlyout->SetInt( "game/any_chapter_allowed", IsAnyChapterAllowed() );
			}

			if ( FlyoutMenu* flyout = UpdateChapterFlyout( kvUpdateChapterFlyout, this, m_drpChapter ) )
			{
				// Determine first chapter to set as active
				char chChapterBuffer[32] = {0};

				if ( !nChapterIndex )
					nChapterIndex = GetNumChaptersForMission( m_pSettings ) + 1;

				Q_snprintf( chChapterBuffer, ARRAYSIZE( chChapterBuffer ), "#L4D360UI_Chapter_%d", nChapterIndex );
				m_drpChapter->SetCurrentSelection( chChapterBuffer );

				flyout->CloseMenu( NULL );
			}
		}
	}

	UpdateFooter();

	if ( m_drpServerType && m_drpServerType->IsVisible() )
	{
		char const *szDefaultServerToCreate = IsX360() ? "official" : "dedicated";
		szDefaultServerToCreate = "listen"; // force listen servers by default since we don't have dedicated servers for now
		char const *szServerType = m_pSettings->GetString( "options/server", szDefaultServerToCreate );
		char chServerType[64];
		Q_snprintf( chServerType, sizeof( chServerType ), "#L4D360UI_ServerType_%s", szServerType );
		m_drpServerType->SetCurrentSelection( chServerType );
	}
}

void GameSettings::OnThink()
{
	m_bAllowChangeToCustomCampaign = true;

	if ( m_drpGameAccess )
	{
		bool bWasEnabled = m_drpGameAccess->IsEnabled();

		m_drpGameAccess->SetEnabled( CUIGameData::Get()->SignedInToLive() );

		if ( bWasEnabled && !m_drpGameAccess->IsEnabled() )
		{
			m_drpGameAccess->SetCurrentSelection( "GameNetwork_lan" );
			m_drpGameAccess->CloseDropDown();

			// If we are creating a team game and lost connection to LIVE then
			// we need to bail to main menu (if we have an active session, session will do it)
			if ( !g_pMatchFramework->GetMatchSession() )
			{
				char const *szGameMode = m_pSettings->GetString( "game/mode", "" );
				if ( StringHasPrefix( szGameMode, "team" ) )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
						"OnEngineDisconnectReason", "reason", "Lost connection to LIVE" ) );
				}
			}
		}
	}

	BaseClass::OnThink();
}

void GameSettings::OnKeyCodePressed(KeyCode code)
{
	int iUserSlot = GetJoystickForCode( code );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );
	
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );

			if ( IsEditingExistingLobby() )
			{
				NavigateBack();
			}
			else
			{
				if ( m_bCloseSessionOnClose )
				{
					g_pMatchFramework->CloseSession();
					m_bCloseSessionOnClose = false;
				}
				m_pSettings = NULL;
				CBaseModPanel::GetSingleton().OpenFrontScreen();
			}
			break;
		}

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void GameSettings::SelectNetworkAccess( char const *szNetworkType, char const *szAccessType )
{
	KeyValues *pSettings = new KeyValues( "update" );
	KeyValues::AutoDelete autodelete( pSettings );
	pSettings->SetString( "update/system/network", szNetworkType );
	pSettings->SetString( "update/system/access", szAccessType );

	UpdateSessionSettings( pSettings );

	if ( BaseModHybridButton* button = dynamic_cast< BaseModHybridButton* > ( FindChildByName( "BtnStartLobby" ) ) )
	{
		if ( !Q_stricmp( "public", szAccessType ) && !Q_stricmp( "LIVE", szNetworkType ) && !ui_game_allow_create_public.GetBool() )
		{
			button->SetText( "#L4D360UI_Join_At_Start" );
		}
		else
		{
			button->SetText( "#L4D360UI_GameSettings_Create_Lobby" );
		}
	}
}

void GameSettings::DoCustomMatch( char const *szGameState )
{
	KeyValues *pSettings = KeyValues::FromString(
		"update",
		" update { "
			" game { "
				" state = "
			" } "
			" options { "
				" action custommatch "
			" } "
		" } "
		);
	KeyValues::AutoDelete autodelete( pSettings );

	pSettings->SetString( "update/game/state", szGameState );

	const char *pszGameMode = m_pSettings->GetString( "game/mode", "" );
	if ( !GameModeIsSingleChapter( pszGameMode ) )
	{
		pSettings->SetInt( "update/game/chapter", 1 );
	}

	UpdateSessionSettings( pSettings );

	Navigate();
}

static char g_chSwitchToNewGameMode[64];
static char g_chSwitchToNewGameModeDone[64];
static void OnSwitchToGameModeConfirmed()
{
	if ( GameSettings * pWnd = ( GameSettings * )CBaseModPanel::GetSingleton().GetWindow( WT_GAMESETTINGS ) )
		pWnd->SwitchToGameMode( g_chSwitchToNewGameMode, true );
}

void GameSettings::SwitchToGameMode( char const *szNewGameMode, bool bConfirmed )
{
	if ( !IsEditingExistingLobby() )
		return;

	if ( !bConfirmed )
	{
		// Show them a confirmation prompt
		GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		CFmtStr strTitle( "#L4D360UI_Lobby_SwitchTo_%s", szNewGameMode );
		data.pWindowTitle = strTitle;

		CFmtStr strText( "#L4D360UI_Lobby_SwitchConf_%s", szNewGameMode );
		data.pMessageText = strText;

		Q_strncpy( g_chSwitchToNewGameMode, szNewGameMode, sizeof( g_chSwitchToNewGameMode ) );
		data.pfnOkCallback = OnSwitchToGameModeConfirmed;
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
		return;
	}

	// Build our game settings update package
	KeyValues *pAllMissions = g_pMatchExt->GetAllMissions();
	KeyValues *kvUpdate = new KeyValues( "update" );
	kvUpdate->SetString( "game/mode", szNewGameMode );

	if ( !Q_stricmp( szNewGameMode, "scavenge" ) ||
		 !Q_stricmp( szNewGameMode, "teamscavenge" ) )
		 kvUpdate->SetInt( "game/maxrounds", 3 );

	// Check if ANY campaign
	char const *szMission = m_pSettings->GetString( "game/campaign" );
	if ( !*szMission )
	{
		// ANY campaign is always valid
	}
	else
	{
		// Campaign is selected, make sure there is such campaign in
		// the new game mode too
		int numChaptersInNewMode = pAllMissions->GetInt( CFmtStr( "%s/modes/%s/chapters", szMission, szNewGameMode ) );
		if ( numChaptersInNewMode )
		{
			// New game mode supports the same camapaign, make sure the chapter index is good
			int nChapter = m_pSettings->GetInt( "game/chapter" );
			if ( !nChapter )
			{
				// ANY chapter is always a valid option
			}
			else if ( IsAnyChapterAllowed() )
			{
				// Prefer to use ANY chapter if possible
				kvUpdate->SetInt( "game/chapter", 0 );
			}
			else if ( nChapter > numChaptersInNewMode ||
				( GameModeIsSingleChapter( m_pSettings->GetString( "game/mode" ) ) && !GameModeIsSingleChapter( szNewGameMode ) ) )
			{
				// Drop to chapter #1
				kvUpdate->SetInt( "game/chapter", 1 );
			}
		}
		else
		{
			// New game mode does not exist for this campaign
			// our options are: pick ANY campaign / ANY chapter if possible
			if ( IsAnyChapterAllowed() )
			{
				kvUpdate->SetString( "game/campaign", "" );
				kvUpdate->SetInt( "game/chapter", 0 );
			}
			// Find a first valid built-in campaign supporting the new game mode
			else for ( KeyValues *kvMission = pAllMissions->GetFirstTrueSubKey(); kvMission; kvMission->GetNextTrueSubKey() )
			{
				if ( !kvMission->GetInt( "builtin" ) )
					continue;
				if ( !kvMission->GetInt( CFmtStr( "modes/%s/chapters", szNewGameMode ) ) )
					continue;

				// Wonderful - found a mission that supports new game mode
				kvUpdate->SetString( "game/campaign", kvMission->GetString( "name" ) );
				kvUpdate->SetInt( "game/chapter", 1 );
				break;
			}
		}
	}

	// Push the updated package into the matchmaking session
	KeyValues *kvModification = new KeyValues( "settings" );
	kvModification->AddSubKey( kvUpdate );
	UpdateSessionSettings( KeyValues::AutoDeleteInline( kvModification ) );

	// Now after the update has been pushed, we need to reopen ourselves
	{
		Panel *pLobby = NavigateBack();
		pLobby->SetVisible( false );

		// Show them a confirmation about successful switch
		Q_snprintf( g_chSwitchToNewGameModeDone, sizeof( g_chSwitchToNewGameModeDone ),
			"#L4D360UI_Lobby_SwitchDone_%s", szNewGameMode );
		CUIGameData::Get()->OpenWaitScreen( g_chSwitchToNewGameModeDone, 1.5f );
		CUIGameData::Get()->CloseWaitScreen( pLobby, "ChangeGameSettings" );
	}
}

//=============================================================================
void GameSettings::OnCommand(const char *command)
{
	if( V_strcmp( command, "JoinAny" ) == 0 )	// Join game in progress
	{
		DoCustomMatch( "game" );
	}
	else if( V_strcmp( command, "Done" ) == 0 )	// Join lobby / done / start game - awful!
	{
		if ( IsCustomMatchSearchCriteria() )
		{
			DoCustomMatch( "lobby" );
		}
		else
		{
			Navigate();
		}
	}
	else if ( !Q_strcmp( command, "Create" ) )
	{
		Assert( !IsEditingExistingLobby() );
		g_pMatchFramework->CreateSession( m_pSettings );
	}
	else if ( char const *szNetworkType = StringAfterPrefix( command, "GameNetwork_" ) )
	{
		SelectNetworkAccess( szNetworkType, "public" );
	}
	else if ( char const *szAccessType = StringAfterPrefix( command, "GameAccess_" ) )
	{
		SelectNetworkAccess( "LIVE", szAccessType );
	}
	else if( V_strcmp( command, "StartLobby" ) == 0 )
	{
		// safety check if we aren't on live
		if( !CUIGameData::Get()->SignedInToLive() )
			SelectNetworkAccess( "lan", "public" );

		char const *szNetwork = m_pSettings->GetString( "system/network", "offline" );
		char const *szAccess = m_pSettings->GetString( "system/access", "public" );

		if ( !Q_stricmp( "LIVE", szNetwork ) &&
			 !Q_stricmp( "public", szAccess ) )
		{
			if ( ui_game_allow_create_public.GetBool() )
			{
				OnCommand( "Create" );
				return;
			}

			// Instead of creating a new public lobby we're going to search
			// for any existing public lobbies that match these params!
			// This way people who take this path to a public lobby will still
			// be matched with humans (they might not end up a lobby leader).
			DoCustomMatch( "lobby" );
		}
		else
		{
			Navigate();
		}
	}
	else if( V_strcmp( command, "Back" ) == 0 )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( char const *szChapterSelected = StringAfterPrefix( command, "#L4D360UI_Chapter_" ) )
	{
		const char *pszGameMode = m_pSettings->GetString( "game/mode", "" );
		if ( !IsCustomMatchSearchCriteria() ||
			 ( m_pSettings && GameModeIsSingleChapter( pszGameMode ) ) )
			// Don't need to react to chapter selections when setting
			// up LIVE custom match search filter (unless in survival or scavenge)
		{
			KeyValues *pSettings = KeyValues::FromString(
				"update",
				" update { "
					" game { "
						" chapter #int#1 "
					" } "
				" } "
				);
			KeyValues::AutoDelete autodelete( pSettings );

			int nChapterIndex = atoi( szChapterSelected );
			if ( nChapterIndex == GetNumChaptersForMission( m_pSettings ) + 1 )
				nChapterIndex = 0;

			pSettings->SetInt( "update/game/chapter", nChapterIndex );

			UpdateSessionSettings( pSettings );
			UpdateChapterImage();
		}
	}
	else if ( char const *pszCharacter = StringAfterPrefix( command, "character_" ) )
	{
		KeyValues *pRequest = new KeyValues( "Game::Avatar" );
		KeyValues::AutoDelete autodelete( pRequest );

		int iController = 0;
#ifdef _X360
		int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
		iController = XBX_GetUserId( iSlot );
#endif
		XUID xuid = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()
			->GetLocalPlayer( iController )->GetXUID();

		pRequest->SetString( "run", "host" );
		pRequest->SetUint64( "xuid", xuid );
		pRequest->SetString( "avatar", pszCharacter );

		g_pMatchFramework->GetMatchSession()->Command( pRequest );

		// Manually update the character flyout since we know that we are the host

		FlyoutMenu* characterFlyout = m_drpCharacter->GetCurrentFlyout();
		if( characterFlyout )
		{
			characterFlyout->SetListener( this );

			vgui::Button *newCharacter = characterFlyout->FindChildButtonByCommand( command );
			OnNotifyChildFocus( newCharacter );
		}
	}
	else if ( V_strcmp( command, "cmd_addoncampaign" ) == 0 )
	{
		// If we have an active control, navigate from it since we'll be setting a new one
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		if ( m_drpMission )
		{
			m_drpMission->NavigateFrom();
		}

		// Open the custom campaign window
		CBaseModPanel::GetSingleton().OpenWindow( WT_CUSTOMCAMPAIGNS, this, true, m_pSettings );
	}
	else if ( const char* szMissionItem = StringAfterPrefix( command, "cmd_campaign_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" campaign = "
					" chapter #int#1 "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		int nChapterSet = ( !*szMissionItem || IsAnyChapterAllowed() ) ? 0 : 1;
		pSettings->SetString( "update/game/campaign", szMissionItem );
		pSettings->SetInt( "update/game/chapter", nChapterSet );

		UpdateSessionSettings( pSettings );

		if( m_drpMission ) //we should become a listener for events pertaining to the mission flyout
		{
			FlyoutMenu* missionFlyout = m_drpMission->GetCurrentFlyout();
			if( missionFlyout )
			{
				missionFlyout->SetListener( this );
			}
		}
		if( m_drpChapter ) //missions change what the available campaigns are, we should listen on that flyout as well
		{
			//set the new flyout, depending on what mission we've selected and become a listener
			if ( !*szMissionItem )
			{
				vgui::ImagePanel* imgLevelImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgLevelImage" ) );
				if( imgLevelImage )
				{
					char buffer[ MAX_PATH ];
					Q_snprintf( buffer, MAX_PATH, "maps/any" );
					imgLevelImage->SetImage( buffer );
				}

				KeyValues *pSettings = KeyValues::FromString(
					"update",
					" update { "
						" game { "
							" campaign #empty# "
							" chapter #int#0 "
						" } "
					" } "
					);
				KeyValues::AutoDelete autodelete( pSettings );

				m_drpChapter->SetEnabled( false );
				m_drpChapter->SetCurrentSelection( "   " );
	
				UpdateSessionSettings( pSettings );
			}
			else
			{
				//
				// Update the flyout to display chapter map names
				//
				KeyValues *pFlyoutChapterDescription = KeyValues::FromString(
					"settings",
					" game { "
						" mode = "
						" campaign = "
						" chapter #int#0 "
						" any_chapter_allowed #int#0 "
					" } "
					);
				KeyValues::AutoDelete autodelete_pFlyoutChapterDescription( pFlyoutChapterDescription );
				pFlyoutChapterDescription->SetString( "game/mode", m_pSettings->GetString( "game/mode", "" ) );
				pFlyoutChapterDescription->SetString( "game/campaign", szMissionItem );
				pFlyoutChapterDescription->SetInt( "game/any_chapter_allowed", IsAnyChapterAllowed() );
				
				m_drpChapter->SetEnabled( true );
				FlyoutMenu* flyout = UpdateChapterFlyout( pFlyoutChapterDescription, this, m_drpChapter );

				//
				// Now select Chapter 1
				//we will need to select the default chapter for this mission ( the first mission )
				if ( !nChapterSet )
					nChapterSet = GetNumChaptersForMission( m_pSettings ) + 1;

				m_drpChapter->SetCurrentSelection( CFmtStr( "#L4D360UI_Chapter_%d", nChapterSet ) );

				if ( flyout )
				{
					//we will need to find the child button that is normally triggered when we make this new selection
					//and run the same code path as if the user had selected that mission from the list
					vgui::Button *newChapter = flyout->FindChildButtonByCommand( CFmtStr( "#L4D360UI_Chapter_%d", nChapterSet ) );
					OnNotifyChildFocus( newChapter );
				}
			}
		}
	}
	else if ( const char *szDifficultyValue = StringAfterPrefix( command, "#L4D360UI_Difficulty_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" difficulty = "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "update/game/difficulty", szDifficultyValue );

		UpdateSessionSettings( pSettings );

		if( m_drpDifficulty )
		{
			if ( FlyoutMenu* pFlyout = m_drpDifficulty->GetCurrentFlyout() )
				pFlyout->SetListener( this );
		}
	}
	else if ( const char *szRoundLimitValue = StringAfterPrefix( command, "#L4D360UI_RoundLimit_" ) )
	{
		KeyValues *pSettings = new KeyValues( "update" );
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetInt( "update/game/maxrounds", atoi( szRoundLimitValue ) );

		UpdateSessionSettings( pSettings );

		if( m_drpRoundsLimit )
		{
			if ( FlyoutMenu* pFlyout = m_drpRoundsLimit->GetCurrentFlyout() )
				pFlyout->SetListener( this );
		}
	}
	else if ( const char *szServerTypeValue = StringAfterPrefix( command, "#L4D360UI_ServerType_" ) )
	{
	KeyValues *pSettings = KeyValues::FromString(
			"update",
			" update { "
				" options { "
					" server x "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		char chBuffer[64];
		Q_snprintf( chBuffer, sizeof( chBuffer ), "%s", szServerTypeValue );
		for ( char *pszUpper = chBuffer; *pszUpper; ++ pszUpper )
		{
			if ( isupper( *pszUpper ) )
				*pszUpper = tolower( *pszUpper );
		}

		pSettings->SetString( "update/options/server", chBuffer );

		UpdateSessionSettings( pSettings );

		if ( m_drpServerType )
		{
			if ( FlyoutMenu *pFlyout = m_drpServerType->GetCurrentFlyout() )
				pFlyout->SetListener( this );
		}
	}
	else if ( char const *szNewGameMode = StringAfterPrefix( command, "SwitchGameModeTo_" ) )
	{
		// User wants to switch our game to a different game mode, let's confirm that first
		SwitchToGameMode( szNewGameMode, false );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void GameSettings::MsgOnCustomCampaignSelected( int chapter, const char *campaign )
{
	if ( !m_bAllowChangeToCustomCampaign )
	{
		Assert( chapter == -10 );
		return;
	}
	m_bAllowChangeToCustomCampaign = false;

	if ( chapter <= -10 )
		chapter = m_pSettings->GetInt( "game/chapter", 0 );

	if ( !IsAnyChapterAllowed() && !chapter )
		chapter = 1;

	// Update the session settings
	KeyValues *pSettings = KeyValues::FromString(
		"update",
		" update { "
			" game { "
				" campaign = "
			" } "
		" } "
		);
	KeyValues::AutoDelete autodelete( pSettings );
	pSettings->SetString( "update/game/campaign", campaign );
	if ( chapter >= 0 )
	{
		pSettings->SetInt( "update/game/chapter", chapter );
	}
	UpdateSessionSettings( pSettings );

	// m_pSettings will be updated now
	KeyValues *pMissionInfo = NULL;
	if ( GetMapInfoAllowingAnyChapter( m_pSettings, &pMissionInfo ) && pMissionInfo )
	{
		m_drpMission->SetCurrentSelection( pMissionInfo->GetString( "displaytitle" ) );
	}

	// If we have an active control, navigate from it since we'll be setting a new one
	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateFrom();
	}

	// OnCommand will actually cascade the update to other controls and
	// will update session settings
	OnCommand( CFmtStr( "cmd_campaign_%s", campaign ) );

	if ( chapter <= 0 )
		chapter = GetNumChaptersForMission( m_pSettings ) + 1;

	OnCommand( CFmtStr( "#L4D360UI_Chapter_%d", chapter ) );
	m_drpChapter->SetCurrentSelection( CFmtStr( "#L4D360UI_Chapter_%d", chapter ) );
}

void GameSettings::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );
	char const *szAccess = m_pSettings->GetString( "system/access", "public" );
	char const *szPlayOptions = m_pSettings->GetString( "options/play", "" );

	char const *szGameMode = m_pSettings->GetString( "game/mode", "coop" );

	const char *pSuffix1 = "";
	const char *pSuffix2 = "";
	const char *pSuffix3 = "";
	
	if ( !Q_stricmp( "offline", szNetwork ) )
	{
		if ( !Q_stricmp( "commentary", szPlayOptions ) )
		{
			pSuffix1 = "_Commentary";
		}
	}
	else
	{
		pSuffix1 = "_";
		pSuffix2 = szGameMode;

		if ( IsCustomMatchSearchCriteria() )
		{
			if ( StringHasPrefix( szGameMode, "team" ) )
			{
				Assert( !"no support for search in teamxxx" );
				Error( "no support for search in teamxxx" );
			}
			pSuffix3 = "Search";
		}
		else if ( IsEditingExistingLobby() )
		{
			// edit game settings
			pSuffix3 = "Edit";
		}
		else
		{
			pSuffix3 = "Create";
		}
	}

	char szPath[MAX_PATH];
	V_snprintf( szPath, sizeof( szPath ), "Resource/UI/BaseModUI/GameSettings%s%s%s.res", pSuffix1, pSuffix2, pSuffix3 );

	LoadControlSettings( szPath );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();

	m_drpDifficulty = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpDifficulty" ) );
	m_drpRoundsLimit = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpRoundLimit" ) );
	m_drpMission = dynamic_cast< DropDownMenu* >( FindChildByName( ( IsCustomMatchSearchCriteria() || IsAnyChapterAllowed() ) ? "DrpMissionExtended" : "DrpMission" ) );
	m_drpChapter = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpChapter" ) );
	m_drpCharacter = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpCharacter" ) );

	m_drpGameAccess = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGameAccess" ) );
	if ( m_drpGameAccess )
	{		
		if( !CUIGameData::Get()->SignedInToLive() )
		{
			m_drpGameAccess->SetCurrentSelection( "GameNetwork_lan" );
		}
		else
		{
			if ( !Q_stricmp( "lan", szNetwork ) )
				m_drpGameAccess->SetCurrentSelection( "GameNetwork_lan" );
			else if ( !Q_stricmp( "LIVE", szNetwork ) )
				m_drpGameAccess->SetCurrentSelection( CFmtStr( "GameAccess_%s", szAccess ) );
		}
	}
	m_drpServerType = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpServerType" ) );

	// can now be invoked as controls exist
	Activate();
}

bool GameSettings::IsEditingExistingLobby()
{
	vgui::Panel * backPanel = GetNavBack();
	CBaseModFrame *pLobby = CBaseModPanel::GetSingleton().GetWindow( WT_GAMELOBBY );
	if ( pLobby && backPanel == pLobby )
	{
		return true;
	}

	return false;
}

bool GameSettings::IsCustomMatchSearchCriteria()
{
	if ( IsEditingExistingLobby() )
		return false;

	if ( Q_stricmp( "custommatch", m_pSettings->GetString( "options/action", "" ) ) )
		return false;

	return true;
}

bool GameSettings::IsAnyChapterAllowed()
{
	char const *szGameMode = m_pSettings->GetString( "game/mode", "" );

	// Custom match allows "ANY" if chapter is a valid option
	if ( IsCustomMatchSearchCriteria() )
	{
		return GameModeIsSingleChapter( szGameMode );
	}

	// Team modes always allow "ANY"
	if ( StringHasPrefix( szGameMode, "team" ) )
		return true;

	return false;
}

void GameSettings::OnClose()
{
	BaseClass::OnClose();

	if( m_drpCharacter)
		m_drpCharacter->CloseDropDown();

	if( m_drpChapter )
		m_drpChapter->CloseDropDown();

	if( m_drpMission )
		m_drpMission->CloseDropDown();

	if( m_drpDifficulty )
		m_drpDifficulty->CloseDropDown();

	if( m_drpRoundsLimit )
		m_drpRoundsLimit->CloseDropDown();

	m_pSettings = NULL;	// NULL out settings in case we get some calls
	// after we are closed
	if ( m_bCloseSessionOnClose )
	{
		g_pMatchFramework->CloseSession();
	}
}

void GameSettings::Navigate()
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	char const *szNetwork = m_pSettings->GetString( "system/network", "offline" );
	char const *szAccess = m_pSettings->GetString( "system/access", "public" );

	if ( IsEditingExistingLobby() )
	{
		NavigateBack();//if we were opened from the game lobby nav back to the game lobby.
	}
	else
	{
		if ( !Q_stricmp( "LIVE", szNetwork ) &&
			 !Q_stricmp( "public", szAccess ) )
		{
			g_pMatchFramework->MatchSession( m_pSettings );
			return;
		}

		if ( !Q_stricmp( "lan", szNetwork ) || (
			 !Q_stricmp( "LIVE", szNetwork ) &&
				( !Q_stricmp( "friends", szAccess ) ||
				  !Q_stricmp( "private", szAccess ) ) ) )
		{
			g_pMatchFramework->CreateSession( m_pSettings );
			return;
		}

		if ( !Q_stricmp( "offline", szNetwork ) )
		{
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			Assert( pIMatchSession );
			if ( pIMatchSession )
			{
				m_bCloseSessionOnClose = false;
				pIMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
				return;
			}
		}

		Assert( !"unreachable" );
		NavigateBack();
	}
}

void GameSettings::OnNotifyChildFocus( vgui::Panel* child )
{
	if ( !child )
	{
		return;
	}

	BaseModHybridButton* button = dynamic_cast< BaseModHybridButton* >( child );
	if ( button )
	{
		KeyValues* command = button->GetCommand();
		if ( command )
		{
			const char* commandValue = command->GetString( "command", NULL );
			if ( char const *szChapterName = StringAfterPrefix( commandValue, "#L4D360UI_Chapter_" ) )
			{
				UpdateChapterImage( atoi( szChapterName ) );
			}
			else if ( const char *szMissionIdx = StringAfterPrefix( commandValue, "cmd_campaign_" ) )
			{
				if ( !*szMissionIdx )
				{
					if ( vgui::ImagePanel* imgLevelImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgLevelImage" ) ) )
					{
						imgLevelImage->SetImage( "maps/any" );
					}
				}
				else
				{
					UpdateChapterImage( 0, szMissionIdx );
				}
			}
			else if ( V_strcmp( commandValue, "cmd_addoncampaign" ) == 0 )
			{
				if ( vgui::ImagePanel* imgLevelImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgLevelImage" ) ) )
				{
					imgLevelImage->SetImage( "maps/unknown" );
				}
			}
			else if ( char const *pszCharacter = StringAfterPrefix( commandValue, "character_" ) )
			{
				const char *pszPortrait = s_characterPortraits->GetTokenI( pszCharacter );
				if ( pszPortrait )
				{
					vgui::ImagePanel *pPanel = dynamic_cast< vgui::ImagePanel* >( child->FindSiblingByName( "HeroPortrait" ) );
					if ( pPanel )
					{
						pPanel->SetVisible( true );
						pPanel->SetImage( pszPortrait );
					}
				}
			}

		}
	}
}

void GameSettings::UpdateChapterImage( int nChapterIdx /* = -1 */, char const *szCampaign /* = NULL */ )
{
	char const *szMapImagePath = "maps/any";

	if ( !m_drpChapter || !m_drpChapter->IsVisible() )
		nChapterIdx = 0;	// always show campaign image if no chapter access

	KeyValues *pInfoMission = NULL;
	KeyValues *pMapInfo = NULL;
	if ( nChapterIdx >= 0 && m_pSettings )
	{
		KeyValues *pSettings = m_pSettings->MakeCopy();
		KeyValues::AutoDelete autodelete( pSettings );

		int numChapters = GetNumChaptersForMission( m_pSettings );

		if ( numChapters > 0 && nChapterIdx == numChapters + 1 )
			nChapterIdx = 0;

		pSettings->SetInt( "game/chapter", nChapterIdx );
		if ( szCampaign )
			pSettings->SetString( "game/campaign", szCampaign );

		pMapInfo = GetMapInfoRespectingAnyChapter( pSettings, &pInfoMission );
	}

	if ( !pMapInfo )
		pMapInfo = GetMapInfoRespectingAnyChapter( m_pSettings, &pInfoMission );

	// Resolve to actual image path
	if ( pMapInfo )
		szMapImagePath = pMapInfo->GetString( "image" );

	vgui::ImagePanel* imgLevelImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgLevelImage" ) );
	if( imgLevelImage )
	{
		imgLevelImage->SetImage( szMapImagePath );
	}
}

void GameSettings::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );

		if ( m_bBackButtonMeansDone )
		{
			// No special button needed in these menus as backing out is considered "Done"
			// There's effectively no way to "Cancel"
			footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
		}
		else
		{
			footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
		}
	}
}

void GameSettings::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter();
	UpdateChapterImage();
}

void GameSettings::OnFlyoutMenuCancelled()
{
	//we need to make sure that if the user cancelled and closed the flyout without making a selection
	//we update the image so that it reflects the current selected map.
	if( m_drpChapter )
	{
		const char* chapterCurSel = m_drpChapter->GetCurrentSelection( );
		FlyoutMenu* chapterFlyout = m_drpChapter->GetCurrentFlyout();
		if( chapterFlyout && chapterCurSel )
		{
			vgui::Button* curSelButton = chapterFlyout->FindChildButtonByCommand( chapterCurSel );
			OnNotifyChildFocus( curSelButton );
		}
	}

	if ( m_drpCharacter )
	{
		const char* characterCurSel = m_drpCharacter->GetCurrentSelection();
		FlyoutMenu* characterFlyout = m_drpCharacter->GetCurrentFlyout();
		if( characterFlyout && characterCurSel )
		{
			vgui::Button* curSelButton = characterFlyout->FindChildButtonByCommand( characterCurSel );
			OnNotifyChildFocus( curSelButton );
		}
	}
}

static void ShowGameSettings()
{
	CBaseModFrame* mainMenu = CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU );
	CBaseModPanel::GetSingleton().OpenWindow( WT_GAMESETTINGS, mainMenu );
}

ConCommand showGameSettings( "showGameSettings", ShowGameSettings );

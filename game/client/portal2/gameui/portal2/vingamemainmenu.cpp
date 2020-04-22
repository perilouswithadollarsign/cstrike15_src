//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"

#include "VInGameMainMenu.h"
#include "VGenericConfirmation.h"
#include "vportalleaderboard.h"
#include "VFooterPanel.h"
#include "VFlyoutMenu.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "vpuzzlemakersavedialog.h"

#include "fmtstr.h"

#include "game/client/IGameClientExports.h"
#include "GameUI_Interface.h"

#include "vgui/ILocalize.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"

#include "vratemapdialog.h"
#include "VGenericWaitScreen.h"

#include "materialsystem/materialsystem_config.h"
#include "portal_gamerules.h"
#include "portal_mp_gamerules.h"

#include "puzzlemaker/puzzlemaker.h"

#include "gameui_util.h"

#ifdef PORTAL2_PUZZLEMAKER

#include "c_community_coop.h"

#endif // PORTAL2_PUZZLEMAKER

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern class IMatchSystem *matchsystem;
extern IVEngineClient *engine;

extern void ExitPuzzleMaker();
extern ConVar cm_community_debug_spew;
extern Color rgbaCommunityDebug;

void Demo_DisableButton( Button *pButton );
void OpenGammaDialog( VPANEL parent );

enum BusyOperation_e
{
	BUSYOP_UNKNOWN = 0,
	BUSYOP_SAVING_GAME,
	BUSYOP_SAVING_PROFILE,
	BUSYOP_READING_DATA
};

//=============================================================================
InGameMainMenu::InGameMainMenu( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, false, true ),
	m_autodelete_pResourceLoadConditions( (KeyValues*)NULL )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );
	SetTitle( "", false );

	SetDialogTitle( NULL );

	// set the correct resource file descriptor
	const char *pResourceType = ( IsInCoopGame() ) ? "_MP" : "";
	
	V_snprintf( m_ResourceName, sizeof( m_ResourceName ), "Resource/UI/BaseModUI/%s%s.res", panelName, pResourceType );
		//( GameRules() && GameRules()->IsMultiplayer() ) ? "_MP" : "" );

	Assert( !m_pResourceLoadConditions );
	m_pResourceLoadConditions = new KeyValues( m_ResourceName );
	m_autodelete_pResourceLoadConditions.Assign( m_pResourceLoadConditions );

	m_bCanViewGamerCard = false;

	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
	if ( pSession )
	{
		KeyValues *pGameSettings = pSession ? pSession->GetSessionSettings() : NULL;
		if ( pGameSettings )
		{
			const char *pNetworkString = pGameSettings->GetString( "system/network" );
			const char *szGameMode = pGameSettings->GetString( "game/mode", "coop" );
			m_bCanViewGamerCard = ( V_stricmp( pNetworkString, "live" ) == 0 && V_stricmp( szGameMode, "sp" ) != 0  );
		}
	}

	if ( GameRules() && GameRules()->IsMultiplayer() && ( XBX_GetNumGameUsers() == 1 ) )
	{
		m_pResourceLoadConditions->SetInt( "?online", 1 );
	}

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->GetBonusChallenge() > 0 )
	{
		m_pResourceLoadConditions->SetInt( "?challenge", 1 );
	}

	if ( GameRules() && GameRules()->IsMultiplayer() && ( XBX_GetNumGameUsers() == 1 )  && ( pPlayer && pPlayer->GetBonusChallenge() > 0 ))
	{
		m_pResourceLoadConditions->SetInt( "?onlinechallenge", 1 );
	}

#if defined( PORTAL2_PUZZLEMAKER )

	PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
	if ( nMapID != 0 )
	{
		m_pResourceLoadConditions->SetInt( "?communitymap", 1 );

		if( engine->IsClientLocalToActiveServer() )
		{
			if( BASEMODPANEL_SINGLETON.IsQuickplay() )
			{
				if( BASEMODPANEL_SINGLETON.GetNextQuickPlayMapInQueue() != NULL )
				{
					m_pResourceLoadConditions->SetInt( "?communitymap_hasnextmap", 1 );
				}
			}
			else if ( BASEMODPANEL_SINGLETON.GetNextSubscribedMapInQueue() != NULL )
			{
				m_pResourceLoadConditions->SetInt( "?communitymap_hasnextmap", 1 );
			}
		}
	}

	if ( g_pPuzzleMaker->GetActive() )
	{
		m_pResourceLoadConditions->SetInt( "?puzzlemaker_active", 1 );

		static ConVarRef puzzlemaker_show( "puzzlemaker_show" );
		if ( puzzlemaker_show.GetBool() )
		{
			m_pResourceLoadConditions->SetInt( "?puzzlemaker_in_view", 1 );
		}
	}

	if ( !g_pPuzzleMaker->HasUncompiledChanges() )
	{
		m_pResourceLoadConditions->SetInt( "?no_uncompiled_changes", 1 );
	}

#endif // PORTAL2_PUZZLEMAKER

	SetFooterEnabled( true );
	SetFooterState();
}

//=============================================================================
InGameMainMenu::~InGameMainMenu()
{
	GameUI().AllowEngineHideGameUI();
}

static void LeaveGameConfirm()
{
	COM_TimestampedLog( "Exit Game" );

	InGameMainMenu* self = 
		static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );

	if ( self )
	{
		self->Close();
	}

	engine->ExecuteClientCmd( "gameui_hide" );

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

	engine->ExecuteClientCmd( "gameui_activate" );

	BASEMODPANEL_SINGLETON.CloseAllWindows();
	BASEMODPANEL_SINGLETON.OpenFrontScreen();
}

void InGameMainMenu::MsgLeaveGameConfirm()
{
	LeaveGameConfirm();
}

#if defined( PORTAL2_PUZZLEMAKER )
void InGameMainMenu::MsgRequestMapRating( KeyValues *pParams )
{
	pParams->SetBool( "options/allowskiptonextlevel", true );
	BASEMODPANEL_SINGLETON.OpenWindow( WT_RATEMAP, this, true, pParams );
}
#endif // PORTAL2_PUZZLEMAKER

void InGameMainMenu::MsgWaitingForExit( KeyValues *pParams )
{
	bool bSaveInProgress = engine->IsSaveInProgress();
	bool bPS3SaveUtilBusy = false;
#if defined( _PS3 )
	bPS3SaveUtilBusy = ps3saveuiapi->IsSaveUtilBusy();
#endif

	if ( bSaveInProgress || bPS3SaveUtilBusy )
	{
		// try again
		PostMessage( this, pParams->MakeCopy(), 0.001f );
		return;
	}

	const char *pFinalMessage = NULL;
	int nOperation = pParams->GetInt( "operation", BUSYOP_UNKNOWN );
	switch ( nOperation )
	{
	case BUSYOP_SAVING_GAME:
		pFinalMessage = "#PORTAL2_MsgBx_SaveCompletedTxt";
		break;

	case BUSYOP_SAVING_PROFILE:
		pFinalMessage = "#PORTAL2_MsgBx_SaveProfileCompleted";
		break;

	default:
		break;
	}

	if ( pFinalMessage )
	{
		CUIGameData::Get()->OpenWaitScreen( pFinalMessage, 1.0f, NULL );
	}

	CUIGameData::Get()->CloseWaitScreen( this, "MsgLeaveGameConfirm" );
}

static void LeaveGameOkCallback()
{
	bool bSaveInProgress = engine->IsSaveInProgress();
	bool bPS3SaveUtilBusy = false;
#if defined( _PS3 )
	bPS3SaveUtilBusy = ps3saveuiapi->IsSaveUtilBusy();
#endif

	if ( bSaveInProgress || bPS3SaveUtilBusy )
	{
		InGameMainMenu* pSelf = 
			static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );

		BusyOperation_e nBusyOperation = BUSYOP_SAVING_GAME;
		const char *pMessage = IsGameConsole() ? "#PORTAL2_WaitScreen_SavingGame" : "#PORTAL2_Hud_SavingGame";
#if defined( _PS3 )
		if ( !bSaveInProgress )
		{
			if ( bPS3SaveUtilBusy )
			{
				uint32 nOpTag = ps3saveuiapi->GetCurrentOpTag();
				if ( nOpTag == kSAVE_TAG_WRITE_STEAMINFO )
				{
					nBusyOperation = BUSYOP_SAVING_PROFILE;
					pMessage = "#PORTAL2_WaitScreen_SavingProfile";
				}
				else
				{
					nBusyOperation = BUSYOP_READING_DATA;
					pMessage = "#PORTAL2_WaitScreen_ReadingData";
				}
			}
		}
#endif
		if ( pSelf && CUIGameData::Get()->OpenWaitScreen( pMessage, 2.0f, NULL ) )
		{
			KeyValues *pParams = new KeyValues( "MsgWaitingForExit" );
			pParams->SetInt( "operation", nBusyOperation );
			pSelf->PostMessage( pSelf, pParams );
			return;
		}
	}

	LeaveGameConfirm();
}

void InGameMainMenu::MsgPreGoToHub()
{
	engine->ServerCmd( "pre_go_to_hub" );
	PostMessage( this, new KeyValues( "MsgGoToHub" ), 1.0f );
}

void InGameMainMenu::MsgGoToHub()
{
	engine->ServerCmd( "go_to_hub" );
}

static void GoToHubOkCallback()
{
	InGameMainMenu *pSelf = 
		static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );
	if ( pSelf )
	{
		bool bWaitScreen =  CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_GoingToHub", 0.0f, NULL );
		pSelf->PostMessage( pSelf, new KeyValues( "MsgPreGoToHub" ), bWaitScreen ? 2.0f : 0.0f );
	}
}

static void LoadLastSaveOkCallback()
{
	InGameMainMenu *pSelf = 
		static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );
	if ( pSelf )
	{
		if ( pSelf )
		{
			pSelf->Close();
		}

		const char *szMostRecentSave = engine->GetMostRecentSaveGame( true );
		CUIGameData::Get()->GameStats_ReportAction( "loadlast", engine->GetLevelNameShort(), !!( szMostRecentSave && szMostRecentSave[0] ) );

		engine->ExecuteClientCmd( "load_recent_checkpoint" );
	}
}

void InGameMainMenu::MsgPreRestartLevel()
{
	engine->ServerCmd( "pre_go_to_hub" );
	PostMessage( this, new KeyValues( "MsgRestartLevel" ), 1.0f );
}

void InGameMainMenu::MsgRestartLevel()
{
	engine->ServerCmd( "mp_restart_level" );
}

static void RestartLevelOkCallback()
{
	if ( GameRules() && GameRules()->IsMultiplayer() )
	{
		InGameMainMenu *pSelf = 
			static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );
		if ( pSelf )
		{
			bool bWaitScreen =  CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_RestartLevel", 0.0f, NULL );
			pSelf->PostMessage( pSelf, new KeyValues( "MsgPreRestartLevel" ), bWaitScreen ? 2.0f : 0.0f );
		}
	}
	else
	{
		engine->ClientCmd( "gameui_hide" );
		engine->ServerCmd( "restart_level" );
	}	
}

void InGameMainMenu::MsgPreGoToCalibration()
{
	engine->ServerCmd( "pre_go_to_calibration" );
	PostMessage( this, new KeyValues( "MsgGoToCalibration" ), 1.0f );
}

void InGameMainMenu::MsgGoToCalibration()
{
	engine->ServerCmd( "go_to_calibration" );
}

static void GoToCalibrationOkCallback()
{
	InGameMainMenu *pSelf = 
		static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );
	if ( pSelf )
	{
		bool bWaitScreen = CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_GoingToCalibration", 0.0f, NULL );
		pSelf->PostMessage( pSelf, new KeyValues( "MsgPreGoToCalibration" ), bWaitScreen ? 2.0f : 0.0f );
	}
}

static void ReturnToQueueOkCallback()
{
	// All done!
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

	BASEMODPANEL_SINGLETON.CloseAllWindows();
	BASEMODPANEL_SINGLETON.MoveToCommunityMapQueue();
	BASEMODPANEL_SINGLETON.OpenFrontScreen();
}

static void SkipToNextLevelOkCallback()
{
	InGameMainMenu *pSelf = 
		static_cast< InGameMainMenu* >( BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU ) );

	Assert( pSelf );
	if ( pSelf )
	{
		pSelf->PostMessage( pSelf, new KeyValues( "MsgPreSkipToNextLevel" ), 0.0f );
	}
}


#if defined( PORTAL2_PUZZLEMAKER )
class CInGameMenu_WaitForDownloadOperation : public IMatchAsyncOperation
{
public:
	virtual bool IsFinished() { return false; }
	virtual AsyncOperationState_t GetState() { return AOS_RUNNING; }
	virtual uint64 GetResult() { return 0ull; }
	virtual void Abort();
	virtual void Release() { Assert( 0 ); }

public:
	CInGameMenu_WaitForDownloadOperation() {}
	IMatchAsyncOperation * Prepare();
}
g_InGameMenu_WaitForDownloadOperation;

class CInGameMenuDownloadFileCallback : public IWaitscreenCallbackInterface
{
public:
	CInGameMenuDownloadFileCallback() : bFinished( false )
	{}

	virtual void OnThink()
	{
		// All done!
		if( bFinished )
			return;

		if( BASEMODPANEL_SINGLETON.IsQuickplay() )
		{
			PublishedFileId_t nID = BASEMODPANEL_SINGLETON.GetNextCommunityMapID();

			// Did we run out of maps?
			if( nID == 0 )
			{
				Assert( 0 );

				// Handle the error case
				InGameMainMenu *pInGameMenu = (InGameMainMenu *) BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU );
				if ( !pInGameMenu )
					return;

				pInGameMenu->PostMessage( pInGameMenu, new KeyValues( "MapDownloadFailed", "msg", "" ) );
			}
		}

		const PublishedFileInfo_t* pInfo = BASEMODPANEL_SINGLETON.GetNextCommunityMapInQueueBasedOnQueueMode();

		// Keep spinning if we dont have our info yet
		if ( !pInfo )
			return;

		UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( pInfo->m_hFile );
		if ( status == UGCFILEREQUEST_FINISHED )
		{
			// Handle completion
			InGameMainMenu *pInGameMenu = (InGameMainMenu *) BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU );
			if ( !pInGameMenu )
				return;

			pInGameMenu->PostMessage( pInGameMenu, new KeyValues( "MapDownloadComplete", "msg", "" ) );
			bFinished = true;
		} 
		else if ( status == UGCFILEREQUEST_ERROR )
		{
			if( BASEMODPANEL_SINGLETON.IsQuickplay() )
			{
				// Remove the current map and get the ball rolling towards getting a valid map
				BASEMODPANEL_SINGLETON.RemoveQuickPlayMapFromQueue( BASEMODPANEL_SINGLETON.GetNextCommunityMapID() );

				return;
			}

			// Handle the error case
			InGameMainMenu *pInGameMenu = (InGameMainMenu *) BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU );
			if ( !pInGameMenu )
				return;

			pInGameMenu->PostMessage( pInGameMenu, new KeyValues( "MapDownloadFailed", "msg", "" ) );
			bFinished = true;
		}
	}

public:
	IWaitscreenCallbackInterface *Prepare() { bFinished = false; return this; }
private:
	bool bFinished;
}
g_InGameMenu_DownloadFileCallback;

IMatchAsyncOperation *CInGameMenu_WaitForDownloadOperation::Prepare()
{
	return this;
}

void CInGameMenu_WaitForDownloadOperation::Abort()
{
	InGameMainMenu *pInGameMenu = (InGameMainMenu *) BASEMODPANEL_SINGLETON.GetWindow( WT_INGAMEMAINMENU );
	if ( !pInGameMenu )
		return;

	pInGameMenu->PostMessage( pInGameMenu, new KeyValues( "MapDownloadAborted", "msg", "" ) );
}

void InGameMainMenu::MapDownloadAborted( const char *msg )
{
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

void InGameMainMenu::MapDownloadComplete( const char *msg )
{
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
	ProceedToNextMap();
}

void InGameMainMenu::MapDownloadFailed( const char *msg )
{
	BASEMODPANEL_SINGLETON.OpenMessageDialog( "#PORTAL2_WorkshopError_DownloadError_Title", "#PORTAL2_WorkshopError_DownloadError" );

	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

void InGameMainMenu::MsgPreSkipToNextLevel()
{
	PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
	if ( nMapID == 0 )
	{
		// TODO: this means that we arrived here without a current community map idea -- huh?!
		Assert( nMapID != 0 );
		return;
	}

	const PublishedFileInfo_t *pNextMapFileInfo = BASEMODPANEL_SINGLETON.GetNextCommunityMapInQueueBasedOnQueueMode();
	if ( pNextMapFileInfo == NULL )
	{
		// TODO: this means we have an invalid map ID set as our next map!
		Assert( pNextMapFileInfo != NULL );
		return;
	}

	// Check the current status of the file and either start a request or just load if it's done
	switch ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() )
	{
	case QUEUEMODE_USER_QUEUE:
	case QUEUEMODE_QUICK_PLAY:
		OnSkipSinglePlayer( pNextMapFileInfo );
		break;
	case QUEUEMODE_USER_COOP_QUEUE:
	case QUEUEMODE_COOP_QUICK_PLAY:
		OnSkipCoop( pNextMapFileInfo );
		break;
	}
}


void InGameMainMenu::OnSkipSinglePlayer( const PublishedFileInfo_t *pNextMapFileInfo )
{
	if ( !WorkshopManager().UGCFileRequestExists( pNextMapFileInfo->m_hFile ) )
	{
		// This means that the file request was never submitted and we're not actively trying to download it.
		// So we need to get the ball rolling, then wait
		BASEMODPANEL_SINGLETON.CreateMapFileRequest( *pNextMapFileInfo );				
	}

	// Move this query to the top of the list
	WorkshopManager().PromoteUGCFileRequestToTop( pNextMapFileInfo->m_hFile );

	// Throw up a "waiting for file download" wait screen
	KeyValues *pSettings = new KeyValues( "WaitScreen" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetUint64( "options/filehandle", pNextMapFileInfo->m_hFile );
	pSettings->SetPtr( "options/asyncoperation", g_InGameMenu_WaitForDownloadOperation.Prepare() );		
	pSettings->SetPtr( "options/waitscreencallback", g_InGameMenu_DownloadFileCallback.Prepare() );

	// We're still downloading, so stall while it works
	CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_CommunityPuzzle_WaitForFileDownload", 1.0f, pSettings );
}


void InGameMainMenu::OnSkipCoop( const PublishedFileInfo_t *pNextMapFileInfo )
{
	g_CommunityCoopManager.BroadcastStartDownloadingMap( pNextMapFileInfo, "SkipMapDialog" );
}

#endif // PORTAL2_PUZZLEMAKER

void InGameMainMenu::Activate()
{
	BaseClass::Activate();

	// the root ingame menu is the only menu allowed that ESC will return to game
	GameUI().AllowEngineHideGameUI();	
}

//=============================================================================
void InGameMainMenu::OnCommand( const char *command )
{
	int iUserSlot = BASEMODPANEL_SINGLETON.GetLastActiveUserId();

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling ingame menu command %s from user%d ctrlr%d\n",
			command, iUserSlot, XBX_GetUserId( iUserSlot ) );
	}

	int iOldSlot = GetGameUIActiveSplitScreenPlayerSlot();

	SetGameUIActiveSplitScreenPlayerSlot( iUserSlot );

	GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iUserSlot );

	if ( !V_stricmp( command, "ReturnToGame" ) )
	{
		engine->ExecuteClientCmd( "gameui_hide" );
		SetGameUIActiveSplitScreenPlayerSlot( iOldSlot );
		return;
	}
	else if ( !V_stricmp( command, "GoIdle" ) )
	{
		engine->ClientCmd(  "gameui_hide" );
		engine->ClientCmd("go_away_from_keyboard" );
	}
	else if ( !V_stricmp( command, "BootPlayer" ) )
	{
#if defined ( _GAMECONSOLE )
		OnCommand( "ReturnToGame" );
		engine->ClientCmd( "togglescores" );
#else
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_INGAMEKICKPLAYERLIST, this, true );
#endif
	}
	else if ( !V_stricmp(command, "ChangeScenario" ) && !demo_ui_enable.GetString()[0] )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_INGAMECHAPTERSELECT, this, true );
	}
	else if ( !V_stricmp( command, "ChangeChapter" ) && !demo_ui_enable.GetString()[0] )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_INGAMECHAPTERSELECT, this, true );
	}
	else if ( !V_stricmp( command, "ChangeDifficulty" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_INGAMEDIFFICULTYSELECT, this, true );
	}
	else if ( !V_stricmp( command, "RestartScenario" ) )
	{
		engine->ClientCmd( "gameui_hide" );
		engine->ClientCmd( "callvote RestartGame;" );
	}
	else if ( !V_stricmp( command, "ReturnToLobby" ) )
	{
		engine->ClientCmd( "gameui_hide" );
		engine->ClientCmd( "callvote ReturnToLobby;" );
	}
	else if ( char const *szInviteType = StringAfterPrefix( command, "InviteUI_" ) )
	{
		if ( IsGameConsole() )
		{
			CUIGameData::Get()->OpenInviteUI( szInviteType );
		}
		else
		{
			CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
	}
	else if ( char const *szLeaderboards = StringAfterPrefix( command, "Leaderboards_" ) )
	{
#ifdef PORTAL2
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfOffline( this,
			"#PORTAL2_LeaderboardOnlineWarning" ) )
			return;
		// check if in the hub
		bool bInHub = false;
		bool bMultiplayer = GameRules() && GameRules()->IsMultiplayer();
		if ( bMultiplayer && V_stricmp("mp_coop_lobby_3", engine->GetLevelNameShort() ) == 0 )
		{
			bInHub = true;
		}
		// PC's not crossplaying with a PS3 and in the Hub gets full map selection leaderboard
		if ( (IsPC() && !ClientIsCrossplayingWithConsole()) || bInHub )
		{
			KeyValues *pLeaderboardValues = new KeyValues( "leaderboard" );
			if ( bInHub || IsPC() && bMultiplayer )
			{
				pLeaderboardValues->SetInt( "state", STATE_PAUSE_MENU );
				BASEMODPANEL_SINGLETON.OpenWindow( WT_PORTALCOOPLEADERBOARD, this, true, pLeaderboardValues );
			}
			else
			{
				pLeaderboardValues->SetInt( "state", STATE_PAUSE_MENU );
				BASEMODPANEL_SINGLETON.OpenWindow( WT_PORTALLEADERBOARD, this, true, pLeaderboardValues );
			}
			pLeaderboardValues->deleteThis();
		}
		else
		{
			// use the limited HUD
			KeyValues *pLeaderboardValues = new KeyValues( "leaderboard" );
			pLeaderboardValues->SetInt( "LevelState", (int)STATE_PAUSE_MENU );
			BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_PORTALLEADERBOARDHUD, this, true, pLeaderboardValues );
		}

		
#else

		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfOffline( this,
			"#L4D360UI_MainMenu_SurvivalLeaderboards_Tip_Disabled" ) )
			return;

		KeyValues *pSettings = NULL;
		if ( *szLeaderboards )
		{
			pSettings = KeyValues::FromString(
				"settings",
				" game { "
					" mode = "
				" } "
				);
			pSettings->SetString( "game/mode", szLeaderboards );
		}
		else
		{
			pSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
		}
		
		if ( !pSettings )
			return;
		
		KeyValues::AutoDelete autodelete( pSettings );
		
		m_ActiveControl->NavigateFrom();

		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_LEADERBOARD, this, true, pSettings );
#endif
	}
	else if ( !V_stricmp( command, "AudioVideo" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_AUDIOVIDEO, this, true );
	}
	else if ( !V_stricmp( command, "Controller" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_CONTROLLER, this, true,
			KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", iUserSlot ) ) );
	}
	else if ( !V_stricmp( command, "Storage" ) )
	{
#ifdef _GAMECONSOLE
		if ( XBX_GetUserIsGuest( iUserSlot ) )
		{
			BASEMODPANEL_SINGLETON.PlayUISound( UISOUND_INVALID );
			return;
		}
#endif
		// Trigger storage device selector
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( XBX_GetUserId( iUserSlot ) ) );
	}
	else if ( !V_stricmp( command, "Audio" ) )
	{
		// audio options dialog, PC only
		m_ActiveControl->NavigateFrom();
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_AUDIO, this, true );
	}
	else if ( !V_stricmp( command, "Video" ) )
	{
		// video options dialog, PC only
		m_ActiveControl->NavigateFrom();
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_VIDEO, this, true );
	}
	else if ( !V_stricmp( command, "Brightness" ) )
	{
		// brightness options dialog, PC only
		OpenGammaDialog( GetVParent() );
	}
	else if ( !V_stricmp( command, "KeyboardMouse" ) )
	{
		// standalone keyboard/mouse dialog, PC only
		m_ActiveControl->NavigateFrom();
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_KEYBOARDMOUSE, this, true );
	}
	else if ( !V_stricmp( command, "MultiplayerSettings" ) )
	{
		// standalone multiplayer settings dialog, PC only
		m_ActiveControl->NavigateFrom();
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_MULTIPLAYER, this, true );
	}
	else if ( !V_stricmp( command, "CloudSettings" ) )
	{
		// standalone cloud settings dialog, PC only
		m_ActiveControl->NavigateFrom();
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_CLOUD, this, true );
	}
	else if ( !V_stricmp( command, "EnableSplitscreen" ) || !Q_strcmp( command, "DisableSplitscreen" ) )
	{
		GameUI().PreventEngineHideGameUI();
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_LeaveMultiplayerConf";
		data.pMessageText = "#L4D360UI_MainMenu_SplitscreenChangeConfMsg";

		data.bOkButtonEnabled = true;
		data.pOkButtonText = "#PORTAL2_ButtonAction_Exit";
		data.pfnOkCallback = &LeaveGameOkCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !V_stricmp( command, "ExitToMainMenu" ) )
	{
		GameUI().PreventEngineHideGameUI();
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

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
	else if ( !V_stricmp( command, "Addons" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_ADDONS, this, true );
	}
	else if ( !V_stricmp( command, "GoToHub" ) )
	{
		GameUI().PreventEngineHideGameUI();
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		if ( PortalMPGameRules() && PortalMPGameRules()->IsLobbyMap() )
		{
			data.pWindowTitle = "#Portal2UI_GoToCalibrationQ";
			data.pMessageText = "#Portal2UI_GoToCalibrationConfMsg";
			data.pfnOkCallback = &GoToCalibrationOkCallback;
		}
		else
		{
			data.pWindowTitle = "#Portal2UI_GoToHubQ";
			data.pMessageText = "#Portal2UI_GoToHubConfMsg";
			data.pfnOkCallback = &GoToHubOkCallback;
		}

		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !V_stricmp( command, "OpenLoadGameDialog" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_LOADGAME, this, true );
	}
	else if ( !V_stricmp( command, "OpenSaveGameDialog" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_SAVEGAME, this, true );
	}
	else if ( !V_stricmp( command, "LoadLastSave" ) )
	{
		GameUI().PreventEngineHideGameUI();
		GenericConfirmation *pConfirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#Portal2UI_LoadLastSaveQ";
		data.pMessageText = "#Portal2UI_LoadLastSaveConfMsg";
		data.pfnOkCallback = &LoadLastSaveOkCallback;

		data.pOkButtonText = "#PORTAL2_ButtonAction_Load";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		pConfirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( command, "RestartLevel" ) )
	{
		GenericConfirmation *pConfirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#Portal2UI_RestartLevelQ";
		data.pMessageText = "#Portal2UI_RestartLevelConfMsg";
		data.pfnOkCallback = &RestartLevelOkCallback;

		data.pOkButtonText = "#PORTAL2_ButtonAction_Restart";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		pConfirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( command, "Options" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_OPTIONS, this, true );
	}
#if defined( PORTAL2_PUZZLEMAKER )
	else if ( !V_stricmp( command, "RateMap" ) )
	{
		GameUI().PreventEngineHideGameUI();
		BASEMODPANEL_SINGLETON.OpenWindow( WT_RATEMAP, this, true );
	}
	else if ( !V_stricmp( command, "EndPlaytest" ) )
	{
		// Depricated -- jdw
	}
	else if ( !V_stricmp( command, "ReturnToQueue" ) )
	{
		GameUI().PreventEngineHideGameUI();
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#PORTAL2_CommunityPuzzle_ReturnToQueue";
		data.pMessageText = "#Portal2UI_ReturnToQueueConfMsg";
		data.bOkButtonEnabled = true;
		data.pOkButtonText = "#PORTAL2_CommunityPuzzle_ReturnToQueue";
		data.pfnOkCallback = &ReturnToQueueOkCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !V_stricmp( command, "SkipToNextLevel" ) )
	{
		GenericConfirmation *pConfirmation = 
			static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#Portal2UI_SkipToNextLevelQ";
		data.pMessageText = "#Portal2UI_SkipToNextLevelConfMsg";
		data.pfnOkCallback = &SkipToNextLevelOkCallback;

		data.pOkButtonText = "#PORTAL2_CommunityPuzzle_SkipToNextLevel";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		pConfirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( command, "SwitchToGameView" ) )
	{
		engine->ExecuteClientCmd( "gameui_hide" );
		engine->ExecuteClientCmd( "puzzlemaker_show 0" );
		SetGameUIActiveSplitScreenPlayerSlot( iOldSlot );
		return;
	}
	else if ( !V_stricmp( command, "SwitchToPuzzleMakerView" ) )
	{
		engine->ExecuteClientCmd( "gameui_hide" );
		engine->ExecuteClientCmd( "puzzlemaker_show 1" );
		SetGameUIActiveSplitScreenPlayerSlot( iOldSlot );
		return;
	}
	else if ( !V_stricmp( command, "ExitPuzzleMaker" ) )
	{
		if ( g_pPuzzleMaker->HasUnsavedChanges() )
		{
			GameUI().PreventEngineHideGameUI();
			BASEMODPANEL_SINGLETON.OpenWindow( WT_PUZZLEMAKEREXITCONRFIRMATION, this, true );
		}
		else
		{
			ExitPuzzleMaker();
		}
	}
	else if ( !V_stricmp( command, "SavePuzzle" ) )
	{
		GameUI().PreventEngineHideGameUI();
		CPuzzleMakerSaveDialog *pSaveDialog = static_cast<CPuzzleMakerSaveDialog*>(BASEMODPANEL_SINGLETON.OpenWindow( WT_PUZZLEMAKERSAVEDIALOG, this, true ));
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_FROMPAUSEMENU );
	}
	else if ( !V_stricmp( command, "RestartLeveLPuzzle" ) )
	{
		engine->ClientCmd( "gameui_hide" );
		engine->ServerCmd( "restart_level" );
	}
	else if ( !V_stricmp( command, "RebuildPuzzle" ) )
	{
		if ( g_pPuzzleMaker->HasErrors() )
		{
			GenericConfirmation *pConfirmation = static_cast<GenericConfirmation*>( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_PuzzleMaker_CannotCompileTitle";
			data.pMessageText = "#PORTAL2_PuzzleMaker_CannotCompileMsg";
			data.pCancelButtonText = "#L4D360UI_Back";
			data.bCancelButtonEnabled = true;

			pConfirmation->SetUsageData( data );
		}
		else
		{
			GameUI().PreventEngineHideGameUI();
			BASEMODPANEL_SINGLETON.OpenWindow( WT_PUZZLEMAKERCOMPILEDIALOG, this, true );
			g_pPuzzleMaker->CompilePuzzle();
		}
	}
	else if ( !V_stricmp( command, "PublishPuzzle" ) )
	{
		GameUI().PreventEngineHideGameUI();
		CPuzzleMakerSaveDialog *pSaveDialog = static_cast<CPuzzleMakerSaveDialog*>(BASEMODPANEL_SINGLETON.OpenWindow( WT_PUZZLEMAKERSAVEDIALOG, this, true ));
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE );
	}
#endif // PORTAL2_PUZZLEMAKER
	else
	{
		const char *pchCommand = command;
#ifdef _GAMECONSOLE
		{
			if ( !Q_strcmp(command, "FlmOptionsFlyout") )
			{
				if ( XBX_GetPrimaryUserIsGuest() )
				{
					pchCommand = "FlmOptionsGuestFlyout";
				}
			}
		}
#endif
		if ( !V_stricmp( command, "FlmVoteFlyout" ) )
		{
			static ConVarRef mp_gamemode( "mp_gamemode" );
			if ( mp_gamemode.IsValid() )
			{
				char const *szGameMode = mp_gamemode.GetString();
				if ( char const *szNoTeamMode = StringAfterPrefix( szGameMode, "team" ) )
					szGameMode = szNoTeamMode;

				if ( !Q_strcmp( szGameMode, "versus" ) || !Q_strcmp( szGameMode, "scavenge" ) )
				{
					pchCommand = "FlmVoteFlyoutVersus";
				}
				else if ( !Q_strcmp( szGameMode, "survival" ) )
				{
					pchCommand = "FlmVoteFlyoutSurvival";
				}
			}
		}

		// does this command match a flyout menu?
		BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( pchCommand ) );
		if ( flyout )
		{
			// If so, enumerate the buttons on the menu and find the button that issues this command.
			// (No other way to determine which button got pressed; no notion of "current" button on PC.)
			for ( int iChild = 0; iChild < GetChildCount(); iChild++ )
			{
				BaseModHybridButton *hybrid = dynamic_cast<BaseModHybridButton *>( GetChild( iChild ) );
				if ( hybrid && hybrid->GetCommand() && !Q_strcmp( hybrid->GetCommand()->GetString( "command"), command ) )
				{
#ifdef _GAMECONSOLE
					hybrid->NavigateFrom( );
#endif //_GAMECONSOLE
					// open the menu next to the button that got clicked
					flyout->OpenMenu( hybrid );
					break;
				}
			}
		}
	}

	SetGameUIActiveSplitScreenPlayerSlot( iOldSlot );
}

//=============================================================================
void InGameMainMenu::OnKeyCodePressed( KeyCode code )
{
	int userId = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( userId );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_START:
	case KEY_XBUTTON_B:
		BASEMODPANEL_SINGLETON.PlayUISound( UISOUND_BACK );
		OnCommand( "ReturnToGame" );
		return;

#if defined( _X360 )
	case KEY_XBUTTON_X:
		{
			CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
			if ( pFooter && ( pFooter->GetButtons() & FB_XBUTTON ) )
			{
				XUID partnerXUID = BASEMODPANEL_SINGLETON.GetPartnerXUID();
				if ( partnerXUID )
				{
					BASEMODPANEL_SINGLETON.PlayUISound( UISOUND_ACCEPT );
					XShowGamerCardUI( XBX_GetActiveUserId(), partnerXUID );
					return;
				}
			}
		}
		break;
#endif

#if !defined( _GAMECONSOLE )
	case KEY_XBUTTON_LEFT_SHOULDER:
		engine->ExecuteClientCmd( "open_econui" );
		return;
#endif
	}

	BaseClass::OnKeyCodePressed( code );
}

void InGameMainMenu::MsgWaitingForOpen( KeyValues *pParams )
{
	bool bSaveInProgress = engine->IsSaveInProgress();
	bool bPS3SaveUtilBusy = false;
#if defined( _PS3 )
	bPS3SaveUtilBusy = ps3saveuiapi->IsSaveUtilBusy();
#endif

	if ( bSaveInProgress || bPS3SaveUtilBusy )
	{
		// try again
		PostMessage( this, pParams->MakeCopy(), 0.001f );
		return;
	}

	const char *pFinalMessage = NULL;
	int nOperation = pParams->GetInt( "operation", BUSYOP_UNKNOWN );
	switch ( nOperation )
	{
	case BUSYOP_SAVING_GAME:
		pFinalMessage = "#PORTAL2_MsgBx_SaveCompletedTxt";
		break;

	case BUSYOP_SAVING_PROFILE:
		pFinalMessage = "#PORTAL2_MsgBx_SaveProfileCompleted";
		break;

	default:
		break;
	}

	if ( pFinalMessage )
	{
		CUIGameData::Get()->OpenWaitScreen( pFinalMessage, 1.0f, NULL );
	}

	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

void InGameMainMenu::OnOpen()
{
	BaseClass::OnOpen();

	SetFooterState();
	UpdateSaveState();

	bool bSaveInProgress = engine->IsSaveInProgress();
	bool bPS3SaveUtilBusy = false;
#if defined( _PS3 )
	bPS3SaveUtilBusy = ps3saveuiapi->IsSaveUtilBusy();
#endif

	if ( bSaveInProgress || bPS3SaveUtilBusy )
	{
		BusyOperation_e nBusyOperation = BUSYOP_SAVING_GAME;
		const char *pMessage = IsGameConsole() ? "#PORTAL2_WaitScreen_SavingGame" : "#PORTAL2_Hud_SavingGame";
#if defined( _PS3 )
		if ( !bSaveInProgress )
		{
			if ( bPS3SaveUtilBusy )
			{
				uint32 nOpTag = ps3saveuiapi->GetCurrentOpTag();
				if ( nOpTag == kSAVE_TAG_WRITE_STEAMINFO )
				{
					nBusyOperation = BUSYOP_SAVING_PROFILE;
					pMessage = "#PORTAL2_WaitScreen_SavingProfile";
				}
				else
				{
					nBusyOperation = BUSYOP_READING_DATA;
					pMessage = "#PORTAL2_WaitScreen_ReadingData";
				}
			}
		}
#endif
		if ( CUIGameData::Get()->OpenWaitScreen( pMessage, 2.0f, NULL ) )
		{
			KeyValues *pParams = new KeyValues( "MsgWaitingForOpen" );
			pParams->SetInt( "operation", nBusyOperation );
			PostMessage( this, pParams );
			return;
		}
	}
}

void InGameMainMenu::OnClose()
{
	Unpause();

	// During shutdown this calls delete this, so Unpause should occur before this call
	BaseClass::OnClose();
}

void InGameMainMenu::OnThink()
{
	int iSlot = GetGameUIActiveSplitScreenPlayerSlot();

	GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iSlot );

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	KeyValues *pGameSettings = pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;
	
	char const *szNetwork = pGameSettings->GetString( "system/network", "offline" );
	char const *szGameMode = pGameSettings->GetString( "game/mode", "coop" );
	char const *szGameState = pGameSettings->GetString( "game/state", "lobby" );

	bool bCanInvite = !Q_stricmp( "LIVE", szNetwork );
	bool bInFinale = !Q_stricmp( "finale", szGameState );

	if ( bCanInvite && pIMatchSession )
	{
		bCanInvite = !bInFinale;
	}

	SetControlEnabled( "BtnInviteFriends", bCanInvite );
	SetControlEnabled( "BtnLeaderboard", CUIGameData::Get()->SignedInToLive() && !Q_stricmp( szGameMode, "survival" ) );

	{
		BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnOptions" ) );
		if ( button )
		{
			BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );

			if ( flyout )
			{
#ifdef _GAMECONSOLE
				bool bIsSplitscreen = ( XBX_GetNumGameUsers() > 1 );
#else
				bool bIsSplitscreen = false;
#endif

				Button *pButton = flyout->FindChildButtonByCommand( "EnableSplitscreen" );
				if ( pButton )
				{
					pButton->SetVisible( !bIsSplitscreen );
#ifdef _GAMECONSOLE
					pButton->SetEnabled( !XBX_GetPrimaryUserIsGuest() && Q_strcmp( engine->GetLevelName(), "maps/credits" PLATFORM_EXT ".bsp" ) != 0 );
#endif
				}

				pButton = flyout->FindChildButtonByCommand( "DisableSplitscreen" );
				if ( pButton )
				{
					pButton->SetVisible( bIsSplitscreen );
				}
			}
		}
	}

	bool bCanGoIdle = !Q_stricmp( "coop_community", szGameMode ) || !Q_stricmp( "coop_challenge", szGameMode ) || !Q_stricmp( "coop", szGameMode ) || !Q_stricmp( "realism", szGameMode ) || !Q_stricmp( "survival", szGameMode );

	// TODO: determine if player can go idle
#if 0
	if ( bCanGoIdle )
	{
		int iLocalPlayerTeam;
		if ( !GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) || iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() )
		{
			bCanGoIdle = false;
		}
		else
		{
			int iNumAliveHumanPlayersOnTeam = GameClientExports()->GetNumPlayersAliveHumanPlayersOnTeam( iLocalPlayerTeam );

			if ( iNumAliveHumanPlayersOnTeam <= 1 )
			{
				bCanGoIdle = false;
			}
		}
	}
#endif

	SetControlEnabled( "BtnGoIdle", bCanGoIdle );

	if ( IsPC() )
	{
		FlyoutMenu *pFlyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
		if ( pFlyout )
		{
			const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
			pFlyout->SetControlEnabled( "BtnBrightness", !config.Windowed() );
		}
	}

	BaseClass::OnThink();

	if ( IsVisible() )
	{
		// Yield to generic wait screen or message box if one of those is present
		WINDOW_TYPE arrYield[] = { WT_GENERICWAITSCREEN, WT_GENERICCONFIRMATION };
		for ( int j = 0; j < ARRAYSIZE( arrYield ); ++ j )
		{
			CBaseModFrame *pYield = BASEMODPANEL_SINGLETON.GetWindow( arrYield[j] );
			if ( pYield && pYield->IsVisible() && !pYield->HasFocus() )
			{
				pYield->Activate();
				pYield->RequestFocus();
			}
		}
	}

#if defined( PORTAL2_PUZZLEMAKER )
	PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
	if ( nMapID != 0 )
	{
		m_pResourceLoadConditions->SetInt( "?communitymap", 1 );

		if( BASEMODPANEL_SINGLETON.IsQuickplay() && engine->IsClientLocalToActiveServer() )
		{
			int nPreValue = m_pResourceLoadConditions->GetInt( "?communitymap_hasnextmap" );

			const PublishedFileInfo_t* pInfo = BASEMODPANEL_SINGLETON.GetNextQuickPlayMapInQueue();
			int nNewValue = pInfo != NULL ? 1 : 0;

			if( nPreValue != nNewValue )
			{
				m_pResourceLoadConditions->SetInt( "?communitymap_hasnextmap", 1 );
			}
		}
	}
#endif // PORTAL2_PUZZLEMAKER
}


//=============================================================================
void InGameMainMenu::PerformLayout( void )
{
	BaseClass::PerformLayout();

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	KeyValues *pGameSettings = pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;

	char const *szNetwork = pGameSettings->GetString( "system/network", "offline" );

	bool bPlayOffline = !Q_stricmp( "offline", szNetwork );

	bool bCanInvite = !Q_stricmp( "LIVE", szNetwork );
	SetControlEnabled( "BtnInviteFriends", bCanInvite );


	if ( PortalMPGameRules() )
	{
		if ( PortalMPGameRules()->IsLobbyMap() )
		{
			BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton * >( FindChildByName( "BtnGoToHub" ) );
			if ( pButton )
			{
				pButton->SetText( "#Portal2UI_GoToCalibration" );
			}

			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			if ( !PortalMPGameRules()->IsAnyLevelComplete() || ( pPlayer && pPlayer->GetBonusChallenge() > 0 ) )
			{
				SetControlEnabled( "BtnGoToHub", false );
			}

			SetControlEnabled( "BtnRestartLevel", false );
		}
		else if ( PortalMPGameRules()->IsStartMap() )
		{
			if ( !PortalMPGameRules()->IsAnyLevelComplete() )
			{
				SetControlEnabled( "BtnGoToHub", false );
			}

			if ( PortalMPGameRules()->IsChallengeMode() )
			{
				BaseModHybridButton *pButton = static_cast< BaseModHybridButton *>( FindChildByName("BtnLeaderboards") );
				if ( pButton )
				{
					pButton->SetEnabled( false );
				}
			}
		}
	}

	UpdateSaveState();

	BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
	}

	flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsGuestFlyout" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
	}

	flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmVoteFlyout" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
		
		bool bSinglePlayer = true;

#ifdef _GAMECONSOLE
		bSinglePlayer = ( XBX_GetNumGameUsers() == 1 );
#endif

		Button *pButton = flyout->FindChildButtonByCommand( "ReturnToLobby" );
		if ( pButton )
		{
			static CGameUIConVarRef r_sv_hosting_lobby( "sv_hosting_lobby", true );
			bool bEnabled = r_sv_hosting_lobby.IsValid() && r_sv_hosting_lobby.GetBool() &&
				// Don't allow return to lobby if playing local singleplayer (it has no lobby)
				!( bPlayOffline && bSinglePlayer );
			pButton->SetEnabled( bEnabled );
		}

		pButton = flyout->FindChildButtonByCommand( "BootPlayer" );
		if ( pButton )
		{
			// Don't allow kick player in local games (nobody to kick)
			pButton->SetEnabled( !bPlayOffline );
		}
	}

	flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmVoteFlyoutVersus" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
	}

	//Figure out which is the top button and navigate to it
	BaseModHybridButton *pReturnToGameButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnReturnToGame" ) );
	BaseModHybridButton *pSwitchToGameButton = dynamic_cast<BaseModHybridButton*>( FindChildByName( "BtnSwitchToGameView" ) );
	BaseModHybridButton *pSwitchToPuzzleMakerButton = dynamic_cast<BaseModHybridButton*>( FindChildByName( "BtnSwitchToPuzzleMakerView" ) );
	BaseModHybridButton *pReturnToChamberView = dynamic_cast<BaseModHybridButton*>( FindChildByName( "BtnReturnToChamberCreator" ) );
	BaseModHybridButton *pTopButton = NULL;
	if ( pReturnToGameButton && pReturnToGameButton->IsEnabled() )
	{
		pTopButton = pReturnToGameButton;
	}
	else if ( pSwitchToGameButton && pSwitchToGameButton->IsEnabled() )
	{
		pTopButton = pSwitchToGameButton;
	}
	else if ( pSwitchToPuzzleMakerButton && pSwitchToPuzzleMakerButton->IsEnabled() )
	{
		pTopButton = pSwitchToPuzzleMakerButton;
	}
	else if ( pReturnToChamberView && pReturnToChamberView->IsEnabled() )
	{
		pTopButton = pReturnToChamberView;
	}

	//If a top button was found, navigate to it
	if ( pTopButton )
	{
		if( m_ActiveControl )
			m_ActiveControl->NavigateFrom();

		pTopButton->NavigateTo();
	}
}

void InGameMainMenu::Unpause( void )
{
}

//=============================================================================
void InGameMainMenu::OnNotifyChildFocus( vgui::Panel* child )
{
}

void InGameMainMenu::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	SetFooterState();
}

void InGameMainMenu::OnFlyoutMenuCancelled()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when the GameUI is hidden
//-----------------------------------------------------------------------------
void InGameMainMenu::OnGameUIHidden()
{
	Unpause();
	Close();
}

//=============================================================================
void InGameMainMenu::SetFooterState()
{
	CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( IsX360() )
		{
			if ( m_bCanViewGamerCard )
			{
				visibleButtons |= FB_XBUTTON;
			}
		}

		if ( IsGameConsole() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		if ( !IsGameConsole() && IsInCoopGame() )
		{
			visibleButtons |= FB_LSHOULDER;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
		pFooter->SetButtonText( FB_XBUTTON, "#L4D360UI_ViewGamerCard" );

		if ( visibleButtons & FB_LSHOULDER )
		{
			pFooter->SetButtonText( FB_LSHOULDER, "#PORTAL2_ItemManagement" );
		}
	}
}

void InGameMainMenu::UpdateSaveState()
{
	// save is disabled in commentary mode
	bool bInCommentary = engine->IsInCommentaryMode();
	bool bNoSavesAllowed = bInCommentary || 
		( V_stristr( engine->GetLevelNameShort(), "sp_a5_credits" ) != NULL );
#if defined( _GAMECONSOLE )
	if ( XBX_GetPrimaryUserIsGuest() )
	{
		bNoSavesAllowed = true;
	}
#endif
	static ConVarRef map_wants_save_disable( "map_wants_save_disable" );
	SetControlEnabled( "BtnSaveGame", !bNoSavesAllowed && !map_wants_save_disable.GetBool() );
	SetControlEnabled( "BtnLoadLastSave", !bNoSavesAllowed );
}

void InGameMainMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	bool bNeedsAvatar = m_pResourceLoadConditions->GetInt( "?online", 0 ) != 0;
	if ( bNeedsAvatar )
	{
		BASEMODPANEL_SINGLETON.SetupPartnerInScience();
		SetupPartnerInScience();
	}
}

void InGameMainMenu::SetupPartnerInScience()
{
	vgui::ImagePanel *pPnlGamerPic = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
	if ( pPnlGamerPic )
	{
		vgui::IImage *pAvatarImage = BASEMODPANEL_SINGLETON.GetPartnerImage();
		if ( pAvatarImage )
		{
			pPnlGamerPic->SetImage( pAvatarImage );
		}
		else
		{
			pPnlGamerPic->SetImage( "icon_lobby" );
		}		

		pPnlGamerPic->SetVisible( true );
	}

	vgui::Label *pLblGamerTag = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTag" ) );
	if ( pLblGamerTag )
	{
		CUtlString partnerName = BASEMODPANEL_SINGLETON.GetPartnerName();
		pLblGamerTag->SetText( partnerName.Get() );
		pLblGamerTag->SetVisible( true );
	}

	vgui::Label *pLblGamerTagStatus = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTagStatus" ) );
	if ( pLblGamerTagStatus )
	{
		pLblGamerTagStatus->SetVisible( true );
		pLblGamerTagStatus->SetText( BASEMODPANEL_SINGLETON.GetPartnerDescKey() );
	}
}

bool InGameMainMenu::IsInCoopGame() const
{
#if defined( PORTAL2_PUZZLEMAKER )
	if( g_pPuzzleMaker->GetActive() )
	{
		return false;
	}

	if ( BASEMODPANEL_SINGLETON.IsCommunityCoop() )
	{
		return false;
	}
#endif
	return (GameRules() && GameRules()->IsMultiplayer());
}

#if defined( PORTAL2_PUZZLEMAKER )

//-----------------------------------------------------------------------------
// Purpose:	Move on to the next map in the queue
//-----------------------------------------------------------------------------
void InGameMainMenu::ProceedToNextMap()
{
	const PublishedFileInfo_t *pMapInfo = BASEMODPANEL_SINGLETON.GetNextCommunityMapInQueueBasedOnQueueMode();
	Assert( pMapInfo );
	if ( pMapInfo == NULL )
		return;

	if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug, "Proceeding to next map ID: %llu\n", pMapInfo->m_nPublishedFileId );

	const char *lpszFilename = WorkshopManager().GetUGCFilename( pMapInfo->m_hFile );
	const char *lpszDirectory = WorkshopManager().GetUGCFileDirectory( pMapInfo->m_hFile );

	char szFilenameNoExtension[MAX_PATH];
	Q_FileBase( lpszFilename, szFilenameNoExtension, sizeof(szFilenameNoExtension) );

	char szMapName[MAX_PATH];
	V_SafeComposeFilename( lpszDirectory, szFilenameNoExtension, szMapName, sizeof(szMapName) );

	// Move past the "maps" folder, it's implied by the following call to load
	const char *lpszUnbasedDirectory = V_strnchr( szMapName, CORRECT_PATH_SEPARATOR, sizeof(szMapName) );
	lpszUnbasedDirectory++; // Move past the actual path separator character

	// Save this for later reference
	BASEMODPANEL_SINGLETON.SetCurrentCommunityMapID( pMapInfo->m_nPublishedFileId );

	// Increment the number of maps we've played so far
	int nNumMapsPlayedThisSession = BASEMODPANEL_SINGLETON.GetNumCommunityMapsPlayedThisSession();
	BASEMODPANEL_SINGLETON.SetNumCommunityMapsPlayedThisSession( nNumMapsPlayedThisSession+1 );

	KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetString( "map", lpszUnbasedDirectory );
	pSettings->SetString( "reason", "newgame" );
	BASEMODPANEL_SINGLETON.OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
}

#endif // PORTAL2_PUZZLEMAKER

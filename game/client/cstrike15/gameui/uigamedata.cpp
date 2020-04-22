//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"

#include "basepanel.h"
#include "uigamedata.h"

#include <ctype.h>


#include "./GameUI/IGameUI.h"
#include "ienginevgui.h"
#include "icommandline.h"
#include "vgui/ISurface.h"
#include "engineinterface.h"
#include "tier0/dbg.h"
#include "ixboxsystem.h"
#include "gameui_interface.h"
#include "game/client/IGameClientExports.h"
#include "fmtstr.h"
#include "vstdlib/random.h"
#include "utlbuffer.h"
#include "filesystem/IXboxInstaller.h"
#include "tier1/tokenset.h"
#include "filesystem.h"
#include "filesystem/IXboxInstaller.h"
#include "inputsystem/iinputsystem.h"
#include <time.h>
#include "messagebox_scaleform.h"


#if defined( _PS3 )
#include <cell/camera.h> // PS3 eye camera
#endif

// BaseModUI High-level windows

//#include "VFoundGames.h"
//#include "VFoundGroupGames.h"
//#include "VGameLobby.h"
//#include "VGenericConfirmation.h"
//#include "VGenericWaitScreen.h"
//#include "VInGameMainMenu.h"
//#include "VMainMenu.h"
//#include "VFooterPanel.h"
//#include "VAttractScreen.h"
//#include "VPasswordEntry.h"
// vgui controls
#include "vgui/ILocalize.h"

#include "netmessages.h"

#ifndef _GAMECONSOLE
#include "steam/steam_api.h"
#endif

#include "gameui_util.h"

#include "cdll_client_int.h"

#include "cstrike15_gcmessages.pb.h"
#include "cstrike15_gcconstants.h"
#include "engine/inetsupport.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
// DWenger - Pulled out temporarily - extern const char *COM_GetModDirectory( void );

// DWenger - Pulled out temporarily - ConVar x360_audio_english("x360_audio_english", "0", 0, "Keeps track of whether we're forcing english in a localized language." );

ConVar demo_ui_enable( "demo_ui_enable", "", FCVAR_DEVELOPMENTONLY, "Suffix for the demo UI" );
ConVar demo_connect_string( "demo_connect_string", "", FCVAR_DEVELOPMENTONLY, "Connect string for demo UI" );

///Asyncronous Operations

ConVar mm_ping_max_green( "ping_max_green", "70" );
ConVar mm_ping_max_yellow( "ping_max_yellow", "140" );
ConVar mm_ping_max_red( "ping_max_red", "250" );

//=============================================================================

const tokenset_t< const char * > BaseModUI::s_characterPortraits[] =
{
	{ "",			"select_Random" },
	{ "random",		"select_Random" },

	//{ "BtnNamVet",	"select_Bill" },
	//{ "BtnTeenGirl",	"select_Zoey" },
	//{ "BtnBiker",		"select_Francis" },
	//{ "BtnManager",	"select_Louis" },

	{ "coach",		"s_panel_lobby_coach" },
	{ "producer",	"s_panel_lobby_producer" },
	{ "gambler",	"s_panel_lobby_gambler" },
	{ "mechanic",	"s_panel_lobby_mechanic" },

	{ "infected",	"s_panel_hand" },

	{ NULL, "" }
};

//=============================================================================
// Xbox 360 Marketplace entry point
//=============================================================================
struct X360MarketPlaceEntryPoint
{
	DWORD dwEntryPoint;
	uint64 uiOfferID;
};
static X360MarketPlaceEntryPoint g_MarketplaceEntryPoint;

#ifdef _GAMECONSOLE
struct X360MarketPlaceQuery
{
	uint64 uiOfferID;
	HRESULT hResult;
	XOVERLAPPED xOverlapped;
};
static CUtlVector< X360MarketPlaceQuery * > g_arrMarketPlaceQueries;
#endif

static void GoToMarketplaceForOffer()
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
#ifdef _X360
	// Stop installing to the hard drive, otherwise STFC fragmentation hazard, as multiple non sequential HDD writes will occur.
	// This needs to be done before the DLC might be downloaded to the HDD, otherwise it could be fragmented.
	// We restart the installer on DLC download completion. We do not handle the cancel/abort case. The installer
	// will restart through the pre-dlc path, i.e. after attract or exiting a map back to the main menu.
	if ( g_pXboxInstaller )
		g_pXboxInstaller->Stop();

	// See if we need to free some of the queries
	for ( int k = 0; k < g_arrMarketPlaceQueries.Count(); ++ k )
	{
		X360MarketPlaceQuery *pQuery = g_arrMarketPlaceQueries[k];
		if ( XHasOverlappedIoCompleted( &pQuery->xOverlapped ) )
		{
			delete pQuery;
			g_arrMarketPlaceQueries.FastRemove( k -- );
		}
	}

	// Allocate a new query
	X360MarketPlaceQuery *pQuery = new X360MarketPlaceQuery;
	memset( pQuery, 0, sizeof( *pQuery ) );
	pQuery->uiOfferID = g_MarketplaceEntryPoint.uiOfferID;
	g_arrMarketPlaceQueries.AddToTail( pQuery );

	// Open the marketplace entry point
	//int iSlot = ;
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	xonline->XShowMarketplaceDownloadItemsUI( XBX_GetActiveUserId(),
											  g_MarketplaceEntryPoint.dwEntryPoint,
											  &pQuery->uiOfferID,
											  1,
											  &pQuery->hResult,
											  &pQuery->xOverlapped );
#endif
}

static void ShowMarketplaceUiForOffer()
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
#ifdef _X360
	// Stop installing to the hard drive, otherwise STFC fragmentation hazard, as multiple non sequential HDD writes will occur.
	// This needs to be done before the DLC might be downloaded to the HDD, otherwise it could be fragmented.
	// We restart the installer on DLC download completion. We do not handle the cancel/abort case. The installer
	// will restart through the pre-dlc path, i.e. after attract or exiting a map back to the main menu.
	if ( g_pXboxInstaller )
		g_pXboxInstaller->Stop();

	// Open the marketplace entry point
	// DWenger - Pulled out temporarily - int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	// DWenger - Pulled out temporarily - int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	// DWenger - Pulled out temporarily - DWORD ret = xonline->XShowMarketplaceUI( iCtrlr, g_MarketplaceEntryPoint.dwEntryPoint, g_MarketplaceEntryPoint.uiOfferID, DWORD( -1 ) );
	// DWenger - Pulled out temporarily - DevMsg( "XShowMarketplaceUI for offer %llx entry point %d ctrlr%d returned %d\n",
		// DWenger - Pulled out temporarily - g_MarketplaceEntryPoint.uiOfferID, g_MarketplaceEntryPoint.dwEntryPoint, iCtrlr, ret );
#endif
}

#ifdef _GAMECONSOLE
CON_COMMAND_F( x360_marketplace_offer, "Get a known offer from x360 marketplace", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() != 4 )
	{
		Warning( "Usage: x360_marketplace_offer type 0xOFFERID ui|dl\n" );
		return;
	}

	int iEntryPoint = Q_atoi( args.Arg( 1 ) );
	char const *szArg2 = args.Arg( 2 );
	uint64 uiOfferId = 0ull;
	if ( 1 != sscanf( szArg2, "0x%llx", &uiOfferId ) )
		uiOfferId = 0ull;

	// Go to marketplace
	g_MarketplaceEntryPoint.dwEntryPoint = iEntryPoint;
	g_MarketplaceEntryPoint.uiOfferID = uiOfferId;
	if ( !Q_stricmp( args.Arg( 3 ), "ui" ) )
		ShowMarketplaceUiForOffer();
	else
		GoToMarketplaceForOffer();
}
#endif

// Console command that's fired from the destructive action confirmation for joining a new session while already in a previous session.
CON_COMMAND_F( confirm_join_new_session_exit_current, "Confirm that we wish to join a new session, destroying a previous session", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( 
		new KeyValues( "OnInvite", "action", "join" ) );
}

//=============================================================================
//
//=============================================================================
CUIGameData* CUIGameData::m_Instance = 0;
bool CUIGameData::m_bModuleShutDown = false;

//=============================================================================
CUIGameData::CUIGameData() :

#if !defined( NO_STEAM )
	m_CallbackUserStatsStored( NULL, NULL ),
	m_CallbackUserStatsReceived( NULL, NULL ),
#endif

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )

	m_CallbackPersonaStateChanged( NULL, NULL ),

#endif

	m_CGameUIPostInit( false )
{
	// It's very dangerous to use "this" in initializer lists.  Do it this way for safety and to kill some warnings.
#if !defined( NO_STEAM )
	m_CallbackUserStatsStored.Register(this, &CUIGameData::Steam_OnUserStatsStored);
	m_CallbackUserStatsReceived.Register(this, &CUIGameData::Steam_OnUserStatsReceived);
#endif
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	m_CallbackPersonaStateChanged.Register(this, &CUIGameData::Steam_OnPersonaStateChanged);
#endif

	m_LookSensitivity = 1.0f;

	m_flShowConnectionProblemTimer = 0.0f;
	m_flTimeLastFrame = Plat_FloatTime();
	m_bShowConnectionProblemActive = false;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_bXUIOpen = false;

	m_bWaitingForStorageDeviceHandle = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = NULL;

	m_pSelectStorageClient = NULL;

	SetDefLessFunc( m_mapUserXuidToAvatar );
	SetDefLessFunc( m_mapUserXuidToName );
}

//=============================================================================
CUIGameData::~CUIGameData()
{
	// Unsubscribe from events system
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//=============================================================================
CUIGameData* CUIGameData::Get()
{
	if ( !m_Instance && !m_bModuleShutDown )
	{
		m_Instance = new CUIGameData();
	}

	return m_Instance;
}

void CUIGameData::Shutdown()
{
	if ( !m_bModuleShutDown )
	{
		m_bModuleShutDown = true;
		delete m_Instance;
		m_Instance = NULL;
	}
}

#ifdef _GAMECONSOLE
CON_COMMAND( ui_fake_connection_problem, "" )
{
	int numMilliSeconds = 1000;
	if ( args.ArgC() > 1 )
	{
		numMilliSeconds = Q_atoi( args.Arg( 1 ) );
	}
	
	float flTime = Plat_FloatTime();
	DevMsg( "ui_fake_connection_problem %d @%.2f\n", numMilliSeconds, flTime );

	int numTries = 2;
	while ( ( 1000 * ( Plat_FloatTime() - flTime ) < numMilliSeconds ) &&
		numTries --> 0 )
	{
		ThreadSleep( numMilliSeconds + 50 );
	}

	flTime = Plat_FloatTime();
	DevMsg( "ui_fake_connection_problem finished @%.2f\n", flTime );
}
#endif

//=============================================================================
void CUIGameData::RunFrame()
{

	// DWenger - Pulled out temporarily - RunFrame_Storage();

	// DWenger - Pulled out temporarily - RunFrame_Invite();

	// msmith - Put in the RunFrame for PS3.
#if defined( _PS3 )

	GetPs3SaveSteamInfoProvider()->RunFrame();

#endif


	if ( m_flShowConnectionProblemTimer > 0.0f )
	{
		float flCurrentTime = Plat_FloatTime();
		float flTimeElapsed = ( flCurrentTime - m_flTimeLastFrame );

		m_flTimeLastFrame = flCurrentTime;

		if ( flTimeElapsed > 0.0f )
		{
			m_flShowConnectionProblemTimer -= flTimeElapsed;
		}

		if ( m_flShowConnectionProblemTimer > 0.0f )
		{
			// DWenger - Pulled out temporarily
			/*
			if ( !m_bShowConnectionProblemActive &&
				 !CBaseModPanel::GetSingleton().IsVisible() )
			{
				GameUI().ActivateGameUI();
				OpenWaitScreen( "#GameUI_RetryingConnectionToServer", 0.0f );
				m_bShowConnectionProblemActive = true;
			}
			*/
		}
		else
		{
			// DWenger - Pulled out temporarily
			/*
			if ( m_bShowConnectionProblemActive )
			{
				// Before closing this particular waitscreen we need to establish
				// a correct navback, otherwise it will not close - Vitaliy (bugbait #51272)
				if ( CBaseModFrame *pWaitScreen = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
				{
					if ( !pWaitScreen->GetNavBack() )
					{
						if ( CBaseModFrame *pIngameMenu = CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) )
							pWaitScreen->SetNavBack( pIngameMenu );
					}

					if ( !pWaitScreen->GetNavBack() )
					{
						// This waitscreen will fail to close, force the close!
						pWaitScreen->Close();
					}
				}

				CloseWaitScreen( NULL, "Connection Problems" );
				GameUI().HideGameUI();
				m_bShowConnectionProblemActive = false;
			}
			*/
		}
	}
}

// DWenger - Pulled out temporarily
/*
void CUIGameData::OnSetStorageDeviceId( int iController, uint nDeviceId )
{
	// Check to see if there is enough room on this storage device
	if ( nDeviceId == XBX_STORAGE_DECLINED || nDeviceId == XBX_INVALID_STORAGE_ID )
	{
		CloseWaitScreen( NULL, "ReportNoDeviceSelected" );
		m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_NOT_SELECTED );
		m_pSelectStorageClient = NULL;
	}
	else if ( xboxsystem->DeviceCapacityAdequate( iController, nDeviceId, COM_GetModDirectory() ) == false )
	{
		CloseWaitScreen( NULL, "ReportDeviceFull" );
		m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_FULL );
		m_pSelectStorageClient = NULL;
	}
	else
	{
		// Set the storage device
		XBX_SetStorageDeviceId( iController, nDeviceId );
		OnDeviceAttached();
		m_pSelectStorageClient->OnDeviceSelected();
	}
}
*/

//=============================================================================
void CUIGameData::OnGameUIPostInit()
{
	m_CGameUIPostInit = true;
}

//=============================================================================
bool CUIGameData::CanPlayer2Join()
{
	if ( demo_ui_enable.GetString()[0] )
		return false;

#ifdef _GAMECONSOLE
	if ( XBX_GetNumGameUsers() != 1 )
		return false;

	if ( XBX_GetPrimaryUserIsGuest() )
		return false;

	// DWenger - Pulled out temporarily
	/*
	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() != WT_MAINMENU )
		return false;
	*/

	return true;
#else
	return false;
#endif
}

//=============================================================================
void CUIGameData::OpenFriendRequestPanel(int index, uint64 playerXuid)
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
#ifdef _X360 
	XShowFriendRequestUI(index, playerXuid);
#endif
}

//=============================================================================
void CUIGameData::OpenInviteUI( char const *szInviteUiType )
{
#ifdef _GAMECONSOLE 
	// DWenger - Pulled out temporarily - int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	// DWenger - Pulled out temporarily - int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	
	// DWenger - Pulled out temporarily - if ( !Q_stricmp( szInviteUiType, "friends" ) )
		// DWenger - Pulled out temporarily - ::XShowFriendsUI( iCtrlr );
	// DWenger - Pulled out temporarily - else if ( !Q_stricmp( szInviteUiType, "players" ) )
		// DWenger - Pulled out temporarily - xonline->XShowGameInviteUI( iCtrlr, NULL, 0, 0 );
	// DWenger - Pulled out temporarily - else if ( !Q_stricmp( szInviteUiType, "party" ) )
		// DWenger - Pulled out temporarily - xonline->XShowPartyUI( iCtrlr );
	// DWenger - Pulled out temporarily - else if ( !Q_stricmp( szInviteUiType, "inviteparty" ) )
		// DWenger - Pulled out temporarily - xonline->XPartySendGameInvites( iCtrlr, NULL );
	// DWenger - Pulled out temporarily - else if ( !Q_stricmp( szInviteUiType, "community" ) )
		// DWenger - Pulled out temporarily - xonline->XShowCommunitySessionsUI( iCtrlr, XSHOWCOMMUNITYSESSION_SHOWPARTY );
	// DWenger - Pulled out temporarily - else if ( !Q_stricmp( szInviteUiType, "voiceui" ) )
		// DWenger - Pulled out temporarily - ::XShowVoiceChannelUI( iCtrlr );
	// DWenger - Pulled out temporarily - else if ( !Q_stricmp( szInviteUiType, "gamevoiceui" ) )
		// DWenger - Pulled out temporarily - ::XShowGameVoiceChannelUI();
	// DWenger - Pulled out temporarily - else
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - DevWarning( "OpenInviteUI with wrong parameter `%s`!\n", szInviteUiType );
		// DWenger - Pulled out temporarily - Assert( 0 );
	// DWenger - Pulled out temporarily - }
#endif
}

void CUIGameData::ExecuteOverlayCommand( char const *szCommand )
{
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamFriends() &&
		 steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
	{
		steamapicontext->SteamFriends()->ActivateGameOverlay( szCommand );
	}
	else
	{
		DisplayOkOnlyMsgBox( NULL, "#SFUI_SteamOverlay_Title", "#SFUI_SteamOverlay_Text" );
	}
#else
	ExecuteNTimes( 5, DevWarning( "ExecuteOverlayCommand( %s ) is unsupported\n", szCommand ) );
	Assert( !"ExecuteOverlayCommand" );
#endif
}

//=============================================================================
bool CUIGameData::SignedInToLive()
{
#ifdef _GAMECONSOLE

	if ( XBX_GetNumGameUsers() <= 0 ||
		 XBX_GetPrimaryUserIsGuest() )
		 return false;

	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iController = XBX_GetUserId( k );
		IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( !player )
			return false;
		if ( player->GetOnlineState() != IPlayer::STATE_ONLINE )
			return false;
	}
#endif
	
	return true;
}

bool CUIGameData::AnyUserSignedInToLiveWithMultiplayerDisabled()
{
#ifdef _GAMECONSOLE
	if ( XBX_GetNumGameUsers() <= 0 ||
		XBX_GetPrimaryUserIsGuest() )
		return false;

	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iController = XBX_GetUserId( k );
		IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( player && player->GetOnlineState() == IPlayer::STATE_NO_MULTIPLAYER )
			return true;
	}
#endif

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg )
{
#ifdef _GAMECONSOLE
	bool bOnlineFound = false;
	if ( XBX_GetNumGameUsers() > 0 &&
		!XBX_GetPrimaryUserIsGuest() )
	{
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			int iController = XBX_GetUserId( k );
			IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
			if ( player && player->GetOnlineState() > IPlayer::STATE_OFFLINE )
				return false;
		}
	}

	if ( bOnlineFound )
		return false;

	DisplayOkOnlyMsgBox( pCallerFrame, "#SFUI_XboxLive", szMsg );
	return true;
#endif

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame )
{
	if ( !IsGameConsole() )
		return false;

	if ( SignedInToLive() )
		return false;

	char const *szMsg = "";

	if ( AnyUserSignedInToLiveWithMultiplayerDisabled() )
	{
		szMsg = "#SFUI_MsgBx_NeedLiveNonGoldMsg";

#ifdef _GAMECONSOLE
		// Show the splitscreen version if there are 2 non-guest accounts
		if ( XBX_GetNumGameUsers() > 1 && XBX_GetUserIsGuest( 0 ) == false && XBX_GetUserIsGuest( 1 ) == false )
		{
			szMsg = "#SFUI_MsgBx_NeedLiveNonGoldSplitscreenMsg";
		}
#endif
	}
	else
	{
		szMsg = "#SFUI_MsgBx_NeedLiveSinglescreenMsg";

#ifdef _GAMECONSOLE
		// Show the splitscreen version if there are 2 non-guest accounts
		if ( XBX_GetNumGameUsers() > 1 && XBX_GetUserIsGuest( 0 ) == false && XBX_GetUserIsGuest( 1 ) == false )
		{
			szMsg = "#SFUI_MsgBx_NeedLiveSplitscreenMsg";
		}
#endif
	}

	DisplayOkOnlyMsgBox( pCallerFrame, "#SFUI_XboxLive", szMsg );

	return true;
}

void CUIGameData::DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg )
{
	// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = 
		// DWenger - Pulled out temporarily - static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pCallerFrame, false ) );
	// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;
	// DWenger - Pulled out temporarily - data.pWindowTitle = szTitle;
	// DWenger - Pulled out temporarily - data.pMessageText = szMsg;
	// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
	// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
}

const char *CUIGameData::GetLocalPlayerName( int iController )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

	IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !player )
	{
		return "";
	}

	return player->GetName();
}


//////////////////////////////////////////////////////////////////////////


//=============================================================================
void CUIGameData::SetLookSensitivity(float sensitivity)
{
	m_LookSensitivity = sensitivity;

	static CGameUIConVarRef joy_yawsensitivity("joy_yawsensitivity");
	if(joy_yawsensitivity.IsValid())
	{
		float defaultValue = atof(joy_yawsensitivity.GetDefault());
		joy_yawsensitivity.SetValue(defaultValue * sensitivity);
	}

	static CGameUIConVarRef joy_pitchsensitivity("joy_pitchsensitivity");
	if(joy_pitchsensitivity.IsValid())
	{
		float defaultValue = atof(joy_pitchsensitivity.GetDefault());
		joy_pitchsensitivity.SetValue(defaultValue * sensitivity);
	}
}

//=============================================================================
float CUIGameData::GetLookSensitivity()
{
	return m_LookSensitivity;
}

bool CUIGameData::IsXUIOpen()
{
	return m_bXUIOpen;
}

void CUIGameData::OpenWaitScreen( const char * messageText, float minDisplayTime, KeyValues *pSettings )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] OpenWaitScreen( %s )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// DWenger - Pulled out temporarily - CBaseModFrame * backFrame = CBaseModPanel::GetSingleton().GetWindow( wtActive );
	// DWenger - Pulled out temporarily - if ( wtActive == WT_GENERICWAITSCREEN && backFrame )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - backFrame = backFrame->GetNavBack();
		// DWenger - Pulled out temporarily - DevMsg( "CUIGameData::OpenWaitScreen - setting navback to %s instead of waitscreen\n", backFrame ? backFrame->GetName() : "NULL" );
	// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - if ( wtActive == WT_GENERICCONFIRMATION && backFrame )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - DevWarning( "Cannot display waitscreen! Active window of higher priority: %s\n", backFrame->GetName() );
		// DWenger - Pulled out temporarily - return;
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICWAITSCREEN, backFrame, false, pSettings ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->SetNavBack( backFrame );
		// DWenger - Pulled out temporarily - waitScreen->ClearData();
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::UpdateWaitPanel( const char * messageText, float minDisplayTime )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] UpdateWaitPanel( %s )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] UpdateWaitPanel( %S )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::CloseWaitScreen( vgui::Panel * callbackPanel, const char * message )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] CloseWaitScreen( %s )\n", message );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->SetCloseCallback( callbackPanel, message );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::NeedConnectionProblemWaitScreen ( void )
{
	m_flShowConnectionProblemTimer = 1.0f;
}

static void PasswordEntered()
{
	CUIGameData::Get()->FinishPasswordUI( true );
}

static void PasswordNotEntered()
{
	CUIGameData::Get()->FinishPasswordUI( false );
}

void CUIGameData::ShowPasswordUI( char const *pchCurrentPW )
{
	// DWenger - Pulled out temporarily - PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().OpenWindow( WT_PASSWORDENTRY, NULL, false ) );
	// DWenger - Pulled out temporarily - if ( pwEntry )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - PasswordEntry::Data_t data;
		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_PasswordEntry_Title";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_PasswordEntry_Prompt";
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
		// DWenger - Pulled out temporarily - data.bCancelButtonEnabled = true;
		// DWenger - Pulled out temporarily - data.m_szCurrentPW = pchCurrentPW;
		// DWenger - Pulled out temporarily - data.pfnOkCallback = PasswordEntered;
		// DWenger - Pulled out temporarily - data.pfnCancelCallback = PasswordNotEntered;
		// DWenger - Pulled out temporarily - pwEntry->SetUsageData(data);
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::FinishPasswordUI( bool bOk )
{
	// DWenger - Pulled out temporarily - PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().GetWindow( WT_PASSWORDENTRY ) );
	// DWenger - Pulled out temporarily - if ( pwEntry )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - if ( bOk )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - char pw[ 256 ];
			// DWenger - Pulled out temporarily - pwEntry->GetPassword( pw, sizeof( pw ) );
			// DWenger - Pulled out temporarily - engine->SetConnectionPassword( pw );
		// DWenger - Pulled out temporarily - }
		// DWenger - Pulled out temporarily - else
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->SetConnectionPassword( "" );
		// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - }
}

IImage *CUIGameData::GetAvatarImage( XUID playerID )
{
#ifdef _GAMECONSOLE
	return NULL;
#else
	if ( !playerID )
		return NULL;

	// do we already have this image cached?
	// DWenger - Pulled out temporarily - CAvatarImage *pImage = NULL;
	int iIndex = m_mapUserXuidToAvatar.Find( playerID );
	
	if ( iIndex == m_mapUserXuidToAvatar.InvalidIndex() )
	{
		// cache a new image
		// DWenger - Pulled out temporarily - pImage = new CAvatarImage();

		// We may fail to set the steam ID - if the player is not our friend and we are not in a lobby or game, eg
		// DWenger - Pulled out temporarily - if ( !pImage->SetAvatarSteamID( playerID ) )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - delete pImage;
			// DWenger - Pulled out temporarily - return NULL;
		// DWenger - Pulled out temporarily - }

		// DWenger - Pulled out temporarily - iIndex = m_mapUserXuidToAvatar.Insert( playerID, pImage );
	}
	// DWenger - Pulled out temporarily - else
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - pImage = m_mapUserXuidToAvatar.Element( iIndex );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - return pImage;
	return NULL; // DWenger - Added temporarily
#endif // !_GAMECONSOLE
}

char const * CUIGameData::GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamUtils() &&
		steamapicontext->SteamFriends() && steamapicontext->SteamUser() )
	{
		int iIndex = m_mapUserXuidToName.Find( playerID );
		if ( iIndex == m_mapUserXuidToName.InvalidIndex() )
		{
			char const *szName = steamapicontext->SteamFriends()->GetFriendPersonaName( playerID );
			if ( szName && *szName )
			{
				iIndex = m_mapUserXuidToName.Insert( playerID, szName );
			}
		}

		if ( iIndex != m_mapUserXuidToName.InvalidIndex() )
			return m_mapUserXuidToName.Element( iIndex ).Get();
	}
#endif

	return szPlayerNameSpeculative;
}

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
void CUIGameData::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( !pParam->m_ulSteamID )
		return;

	if ( pParam->m_nChangeFlags & k_EPersonaChangeName )
	{
		int iIndex = m_mapUserXuidToName.Find( pParam->m_ulSteamID );
		if ( iIndex != m_mapUserXuidToName.InvalidIndex() )
		{
			CUtlString utlName = m_mapUserXuidToName.Element( iIndex );
			m_mapUserXuidToName.RemoveAt( iIndex );
			GetPlayerName( pParam->m_ulSteamID, utlName.Get() );
		}
	}

	if ( pParam->m_nChangeFlags & k_EPersonaChangeAvatar )
	{
		// DWenger - Pulled out temporarily - CAvatarImage *pImage = NULL;
		// DWenger - Pulled out temporarily - int iIndex = m_mapUserXuidToAvatar.Find( pParam->m_ulSteamID );
		// DWenger - Pulled out temporarily - if ( iIndex != m_mapUserXuidToAvatar.InvalidIndex() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - pImage = m_mapUserXuidToAvatar.Element( iIndex );
		// DWenger - Pulled out temporarily - }

		// Re-fetch the image if we have it cached
		// DWenger - Pulled out temporarily - if ( pImage )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - pImage->SetAvatarSteamID( pParam->m_ulSteamID );
		// DWenger - Pulled out temporarily - }
	}
}
#endif

CON_COMMAND_F( ui_reloadscheme, "Reloads the resource files for the active UI window", 0 )
{
	g_pFullFileSystem->SyncDvdDevCache();
	CUIGameData::Get()->ReloadScheme();
}

void CUIGameData::ReloadScheme()
{
	// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().ReloadScheme();
	// DWenger - Pulled out temporarily - CBaseModFrame *window = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	// DWenger - Pulled out temporarily - if( window )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - window->ReloadSettings();
	// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
	// DWenger - Pulled out temporarily - if( footer )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - footer->ReloadSettings();
	// DWenger - Pulled out temporarily - }
}

CBaseModFrame * CUIGameData::GetParentWindowForSystemMessageBox()
{
	// DWenger - Pulled out temporarily - WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// DWenger - Pulled out temporarily - WINDOW_PRIORITY wPriority = CBaseModPanel::GetSingleton().GetActiveWindowPriority();

	// DWenger - Pulled out temporarily - CBaseModFrame *pCandidate = CBaseModPanel::GetSingleton().GetWindow( wtActive );

	// DWenger - Pulled out temporarily - if ( pCandidate )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - if ( wPriority >= WPRI_WAITSCREEN && wPriority <= WPRI_MESSAGE )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - DevMsg( "GetParentWindowForSystemMessageBox: using navback of %s\n", pCandidate->GetName() );
			// DWenger - Pulled out temporarily - }

			// Message would not be able to nav back to waitscreen or another message
			// DWenger - Pulled out temporarily - pCandidate = pCandidate->GetNavBack();
		// DWenger - Pulled out temporarily - }
		// DWenger - Pulled out temporarily - else if ( wPriority > WPRI_MESSAGE )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - DevMsg( "GetParentWindowForSystemMessageBox: using NULL since a higher priority window is open %s\n", pCandidate->GetName() );
			// DWenger - Pulled out temporarily - }

			// Message would not be able to nav back to a higher level priority window
			// DWenger - Pulled out temporarily - pCandidate = NULL;
		// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - return pCandidate;
	return NULL;	// DWenger - temporary code
}

bool CUIGameData::IsActiveSplitScreenPlayerSpectating( void )
{
// 	int iLocalPlayerTeam;
// 	if ( GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) )
// 	{
// 		if ( iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() &&
// 			 iLocalPlayerTeam != GameClientExports()->GetTeamId_Infected() )
// 			return true;
// 	}

	return false;
}

struct ServerCookie_t
{
	uint64 m_uiCookie;
	double m_dblTimeCached;
};
CUtlMap< uint64, ServerCookie_t, int32, CDefLess< uint64 > > g_mapServerCookies;
static uint64 Helper_GetServerCookie( uint64 gsid )
{
	int32 i = g_mapServerCookies.Find( gsid );
	if ( i == g_mapServerCookies.InvalidIndex() )
		return 0;
	if ( Plat_FloatTime() - g_mapServerCookies.Element( i ).m_dblTimeCached > 60.0f )
		return 0;
	return g_mapServerCookies.Element( i ).m_uiCookie;
}

void CUIGameData::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnSysXUIEvent", szEvent ) )
	{
		m_bXUIOpen = !Q_stricmp( "opening", pEvent->GetString( "action", "" ) );
	}
	else if ( !Q_stricmp( "OnProfileUnavailable", szEvent ) )
	{
#if defined( _DEMO ) && defined( _GAMECONSOLE )
		return;
#endif
		// Activate game ui to see the dialog
		// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
		// DWenger - Pulled out temporarily - }

		// Pop a message dialog if their storage device was changed
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_AchievementNotWrittenTitle";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_AchievementNotWritten"; 
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

		// DWenger - Pulled out temporarily - confirmation->SetUsageData( data );
	}
	else if ( !Q_stricmp( "OnInvite", szEvent ) )
	{
		// Check if the user just accepted invite
		if ( !Q_stricmp( "accepted", pEvent->GetString( "action" ) ) )
		{
			// Check if we have an outstanding session
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( !pIMatchSession )
			{
				// DWenger - Pulled out temporarily - Invite_Connecting();
				return;
			}

			// User is accepting an invite and has an outstanding
			// session, TCR requires confirmation of destructive actions
			if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
			{
				*pnConfirmed = 0;
			}

			// Show the prompt to confirm they wish to join a new session
			GameUI().CreateCommandMsgBoxInSlot(
				CMB_SLOT_FULL_SCREEN, 
				"#SFUI_Confirm_JoinAnotherGameTitle", 
				"#SFUI_Confirm_JoinAnotherGameText", 
				true, 
				true, 
				"confirm_join_new_session_exit_current\n", 
				NULL, 
				NULL, 
				NULL );
		}
		else if ( !Q_stricmp( "storage", pEvent->GetString( "action" ) ) )
		{
			// DWenger - Pulled out temporarily - if ( !Invite_IsStorageDeviceValid() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
				// DWenger - Pulled out temporarily - {
					// DWenger - Pulled out temporarily - *pnConfirmed = 0;	// make the invite accepting code wait
				// DWenger - Pulled out temporarily - }
			// DWenger - Pulled out temporarily - }
		}
		else if ( !Q_stricmp( "error", pEvent->GetString( "action" ) ) )
		{
			char const *szReason = pEvent->GetString( "error", "" );

			if ( XBX_GetNumGameUsers() < 2 )
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline1", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer1", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole1", RemapText_t::MATCH_FULL },
					{ NULL, NULL, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}
			else
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline2", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer2", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole2", RemapText_t::MATCH_FULL },
					{ NULL, NULL, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}

			// Show the message box
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );

			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_XboxLive";
			// DWenger - Pulled out temporarily - data.pMessageText = szReason;
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		}
	}
	else if ( !Q_stricmp( "OnSysStorageDevicesChanged", szEvent ) )
	{
#if defined( _DEMO ) && defined( _GAMECONSOLE )
		return;
#endif

		// If a storage device change is in progress, the simply ignore
		// the notification callback, but pop the dialog
		if ( m_pSelectStorageClient )
		{
			DevWarning( "Ignored OnSysStorageDevicesChanged while the storage selection was in progress...\n" );
		}

		// Activate game ui to see the dialog
		// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
		// DWenger - Pulled out temporarily - }

		// Pop a message dialog if their storage device was changed
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#GameUI_Console_StorageRemovedTitle";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_StorageDeviceRemoved"; 
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
		
		// DWenger - Pulled out temporarily - extern void OnStorageDevicesChangedSelectNewDevice();
		// DWenger - Pulled out temporarily - data.pfnOkCallback = m_pSelectStorageClient ? NULL : &OnStorageDevicesChangedSelectNewDevice;	// No callback if already in the middle of selecting a storage device

		// DWenger - Pulled out temporarily - confirmation->SetUsageData( data );
	}
#if defined( _PS3 )
	else if ( !Q_stricmp( "OnPSMoveOutOfViewChanged", szEvent ) )
	{
		int inViewStatus = pEvent->GetInt( "OutOfViewBool" );
		if ( inViewStatus == 0 )
		{	
			if ( g_pInputSystem->MotionControllerActive() )
			{
			// here is where we open the "move out of view" message box!
				PopupManager::ShowSingleUsePopup( POPUP_TYPE_PSMOVE_OUT_OF_VIEW );
			}
		}
		else
		{
			PopupManager::HideSingleUsePopup( POPUP_TYPE_PSMOVE_OUT_OF_VIEW );
		}
	}
	else if ( !Q_stricmp( "OnPSEyeChangedStatus", szEvent ) )
	{
		int32 camStatus = pEvent->GetInt( "CamStatus" );
		if ( camStatus == CELL_OK )
		{		
				// remove message box either way.
				PopupManager::HideSingleUsePopup( POPUP_TYPE_PSEYE_DISCONNECTED );
		}
		else
		{
			// only show this warning if the camera is removed AND we're using the move or sharpshooter
			// otherwise it's not important
			if ( g_pInputSystem->MotionControllerActive() )
			{
				PopupManager::ShowSingleUsePopup( POPUP_TYPE_PSEYE_DISCONNECTED );
			}
		}
	}
#endif
	else if ( !Q_stricmp( "OnSysInputDevicesChanged", szEvent ) )
	{
		unsigned int nInactivePlayers = 0;  // Number of users on the spectating team (ie. idle), or disconnected in this call
		int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
		int nDisconnectedDevices = pEvent->GetInt( "mask" );
		for ( unsigned int nSlot = 0; nSlot < XBX_GetNumGameUsers(); ++nSlot, nDisconnectedDevices >>= 1 )
		{
			engine->SetActiveSplitScreenPlayerSlot( nSlot );
			
			// See if this player is spectating (ie. idle)
			bool bSpectator = IsActiveSplitScreenPlayerSpectating();
			if ( bSpectator )
			{
				nInactivePlayers++;
			}
		
			if ( nDisconnectedDevices & 0x1 )
			{
				// Only count disconnections if that player wasn't idle
				if ( !bSpectator )
				{
					nInactivePlayers++;
				}

				engine->ClientCmd( "go_away_from_keyboard" );
			}
		}
		engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

		// If all the spectators and all the disconnections account for all possible users, we need to pop a message
		// Also, if the GameUI is up, always show the disconnection message
		// DWenger - Pulled out temporarily - if ( CBaseModPanel::GetSingleton().IsVisible() || nInactivePlayers == XBX_GetNumGameUsers() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
			// DWenger - Pulled out temporarily - }

			// Pop a message if a valid controller was removed!
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
			
			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;
			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_ControllerUnpluggedTitle";
			// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_ControllerUnplugged";
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		// DWenger - Pulled out temporarily - }
	}
	else if ( !Q_stricmp( "OnMatchPlayerMgrReset", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );
		bool bShowDisconnectedMsgBox = true;
		if ( !Q_stricmp( szReason, "GuestSignedIn" ) )
		{
			char const *szDestroyedSessionState = pEvent->GetString( "settings/game/state", "lobby" );
			if ( !Q_stricmp( "lobby", szDestroyedSessionState ) )
				bShowDisconnectedMsgBox = false;
		}

		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the attract screen
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = bShowDisconnectedMsgBox ? static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) ) : NULL;
		// DWenger - Pulled out temporarily - CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL );

		// DWenger - Pulled out temporarily - if ( confirmation )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_SignInChangeC";
			// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_SignInChange";
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - if ( !Q_stricmp( szReason, "GuestSignedIn" ) )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_DisconnectedFromSession";	// "Disconnect"
				// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_SignInChange";				// "Sign-in change has occured."
			// DWenger - Pulled out temporarily - }

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);

#ifdef _GAMECONSOLE
			// When a confirmation shows up it prevents attract screen from opening, so reset user slots here:
			// DWenger - Pulled out temporarily - XBX_ResetUserIdSlots();
			// DWenger - Pulled out temporarily - XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
			// DWenger - Pulled out temporarily - XBX_SetPrimaryUserIsGuest( 0 );	
			// DWenger - Pulled out temporarily - XBX_SetNumGameUsers( 0 ); // users not selected yet
#endif
		// DWenger - Pulled out temporarily - }
	}
	else if ( !Q_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );

		if ( char const *szDisconnectHdlr = pEvent->GetString( "disconnecthdlr", NULL ) )
		{
			// If a disconnect handler was set during the event, then we don't interfere with
			// the dialog explaining disconnection, just let the disconnect handler do everything.
			return;
		}

		RemapText_t arrText[] = {
			{ "", "#DisconnectReason_Unknown", RemapText_t::MATCH_FULL },
			{ "Lost connection to LIVE", "#DisconnectReason_LostConnectionToLIVE", RemapText_t::MATCH_FULL },
			{ "Player removed from host session", "#DisconnectReason_PlayerRemovedFromSession", RemapText_t::MATCH_SUBSTR },
			{ "Connection to server timed out", "#SFUI_MsgBx_DisconnectedFromServer", RemapText_t::MATCH_SUBSTR },
			{ "Server shutting down", "#SFUI_MsgBx_DisconnectedServerShuttingDown", RemapText_t::MATCH_SUBSTR },
			{ "Added to banned list", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "Kicked and banned", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "You have been voted off", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "All players idle", "#L4D_ServerShutdownIdle", RemapText_t::MATCH_SUBSTR },
#ifdef _GAMECONSOLE
			{ "", "#DisconnectReason_Unknown", RemapText_t::MATCH_START },	// Catch all cases for X360
#endif
			{ NULL, NULL, RemapText_t::MATCH_FULL }
		};

		szReason = RemapText_t::RemapRawText( arrText, szReason );

		//
		// Go back to main menu and display the disconnection reason
		//
		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the main menu
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_DisconnectedFromSession";	// "Disconnect"
		// DWenger - Pulled out temporarily - data.pMessageText = szReason;
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

		// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
	}
	else if ( !Q_stricmp( "OnEngineEndGame", szEvent ) )
	{
		// If we are connected and there was no session object to handle the event
		if ( !g_pMatchFramework->GetMatchSession() )
		{
			// Issue the disconnect command
			engine->ExecuteClientCmd( "disconnect" );
		}
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !Q_stricmp( "error", pEvent->GetString( "state", "" ) ) )
		{
			g_pMatchFramework->CloseSession();

			char chErrorMsgBuffer[128] = {0};
			char chErrorTitleBuffer[128] = {0};
			char const *szError = pEvent->GetString( "error", "" );
			char const *szErrorTitle = "#SFUI_MsgBx_DisconnectedFromSession";

			RemapText_t arrText[] = {
				{ "", "#SessionError_Unknown", RemapText_t::MATCH_FULL },
				{ "n/a", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "create", "#SessionError_Create", RemapText_t::MATCH_FULL },
				{ "createclient", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "connect", "#SessionError_Connect", RemapText_t::MATCH_FULL },
				{ "full", "#SessionError_Full", RemapText_t::MATCH_FULL },
				{ "lock", "#SessionError_Lock", RemapText_t::MATCH_FULL },
				{ "kicked", "#SessionError_Kicked", RemapText_t::MATCH_FULL },
				{ "migrate", "#SessionError_Migrate", RemapText_t::MATCH_FULL },
				{ "nomap", "#SessionError_NoMap", RemapText_t::MATCH_FULL },
				{ "SteamServersDisconnected", "#SessionError_SteamServersDisconnected", RemapText_t::MATCH_FULL },
				{ NULL, NULL, RemapText_t::MATCH_FULL }
			};

			szError = RemapText_t::RemapRawText( arrText, szError );

			if ( !Q_stricmp( "turequired", szError ) )
			{
				// Special case for TU required message
				// If we have a localization string for the TU message then this means that the other box
				// is running and older version of the TU
				char const *szTuRequiredCode = pEvent->GetString( "turequired" );
				CFmtStr strLocKey( "#SessionError_TU_Required_%s", szTuRequiredCode );
				if ( g_pVGuiLocalize->Find( strLocKey ) )
				{
					Q_strncpy( chErrorMsgBuffer, strLocKey, sizeof( chErrorMsgBuffer ) );
					szError = chErrorMsgBuffer;
				}
				else
				{
					szError = "#SessionError_TU_RequiredMessage";
				}
				szErrorTitle = "#SessionError_TU_RequiredTitle";
			}

			// Go to the main menu
			// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

			// Show the message box
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
			// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = szErrorTitle;
			// DWenger - Pulled out temporarily - data.pMessageText = szError;
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			if ( !Q_stricmp( "dlcrequired", szError ) )
			{
				// Special case for DLC required message
				uint64 uiDlcRequiredMask = pEvent->GetUint64( "dlcrequired" );
				int iDlcRequired = 0;

				// Find the first DLC in the reported missing mask that is required
				for ( int k = 1; k < sizeof( uiDlcRequiredMask ); ++ k )
				{
					if ( uiDlcRequiredMask & ( 1ull << k ) )
					{
						iDlcRequired = k;
						break;
					}
				}

				CFmtStr strLocKey( "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				if ( !g_pVGuiLocalize->Find( strLocKey ) )
					iDlcRequired = 0;

				// Try to figure out if this DLC is paid/free/unknown
				KeyValues *kvDlcDetails = new KeyValues( "" );
				KeyValues::AutoDelete autodelete_kvDlcDetails( kvDlcDetails );
				if ( !kvDlcDetails->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/dlcdetailsinfo.res", "MOD" ) )
					kvDlcDetails = NULL;

				// Determine the DLC offer ID
				uint64 uiDlcOfferID = 0ull;
				if ( 1 != sscanf( kvDlcDetails->GetString( CFmtStr( "dlc%d/offerid", iDlcRequired ) ), "0x%llx", &uiDlcOfferID ) )
					uiDlcOfferID = 0ull;
				
				// Format the strings
				bool bKicked = !Q_stricmp( pEvent->GetString( "action" ), "kicked" );
				wchar_t const *wszLine1 = g_pVGuiLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", bKicked ? "Kicked" : "Join", iDlcRequired ) );
				wchar_t const *wszLine2 = g_pVGuiLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", uiDlcOfferID ? "Offer" : "Message", iDlcRequired ) );
				
				int numBytesTwoLines = ( Q_wcslen( wszLine1 ) + Q_wcslen( wszLine2 ) + 4 ) * sizeof( wchar_t );
				wchar_t *pwszTwoLines = ( wchar_t * ) stackalloc( numBytesTwoLines );
				Q_snwprintf( pwszTwoLines, numBytesTwoLines, L"%s%s", wszLine1, wszLine2 );
				// DWenger - Pulled out temporarily - data.pMessageTextW = pwszTwoLines;
				// DWenger - Pulled out temporarily - data.pMessageText = NULL;

				Q_snprintf( chErrorTitleBuffer, sizeof( chErrorTitleBuffer ), "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				// DWenger - Pulled out temporarily - data.pWindowTitle = chErrorTitleBuffer;

				if ( uiDlcOfferID )
				{
					// DWenger - Pulled out temporarily - data.bCancelButtonEnabled = true;
					// DWenger - Pulled out temporarily - data.pfnOkCallback = GoToMarketplaceForOffer;
					
					g_MarketplaceEntryPoint.uiOfferID = uiDlcOfferID;
					g_MarketplaceEntryPoint.dwEntryPoint = kvDlcDetails->GetInt( CFmtStr( "dlc%d/type", iDlcRequired ) );
				}
			}
			
			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		}
	}
#if ENGINE_CONNECT_VIA_MMS
#else
	else if ( !Q_strcmp( "OnEngineLevelLoadingSession", szEvent ) )
	{
		/* Removed for partner depot */
	}
#endif
}

class ClientJob_EMsgGCCStrike15_v2_ClientRequestJoinFriendData : public GCSDK::CGCClientJob
{
public:
	explicit ClientJob_EMsgGCCStrike15_v2_ClientRequestJoinFriendData( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_ClientRequestJoinFriendData> msg( pNetPacket );

		if ( msg.Body().has_errormsg() )
		{
			RemapText_t arrText[] = {
				{ "Game is full", "#SFUI_DisconnectReason_LobbyFull", RemapText_t::MATCH_FULL },
				{ "Certified server required", "#SFUI_DisconnectReason_CertifiedServerRequired", RemapText_t::MATCH_FULL },
				{ "Certified server denied", "#SFUI_DisconnectReason_CertifiedServerDenied", RemapText_t::MATCH_FULL },
				{ "PW server required", "#SFUI_DisconnectReason_PWServerRequired", RemapText_t::MATCH_FULL },
				{ "PW server denied", "#SFUI_DisconnectReason_PWServerDenied", RemapText_t::MATCH_FULL },
				{ NULL, NULL, RemapText_t::MATCH_FULL }
			};
			char const *szReason = RemapText_t::RemapRawText( arrText, msg.Body().errormsg().c_str() );

			g_pMatchFramework->CloseSession();
			CMessageBoxScaleform::UnloadAllDialogs( true );
			BasePanel()->RestoreMainMenuScreen();
			CCommandMsgBox::CreateAndShow( "#SFUI_Disconnect_Title", szReason, true );
			return false;
		}

		if ( msg.Body().res().serverid() )
		{
			ServerCookie_t sc = { msg.Body().res().reservationid(), Plat_FloatTime() };
			g_mapServerCookies.InsertOrReplace( msg.Body().res().serverid(), sc );
		}

		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_ClientRequestJoinFriendData, k_EMsgGCCStrike15_v2_ClientRequestJoinFriendData );

class ClientJob_EMsgGCCStrike15_v2_ClientRequestJoinServerData : public GCSDK::CGCClientJob
{
public:
	explicit ClientJob_EMsgGCCStrike15_v2_ClientRequestJoinServerData( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_ClientRequestJoinServerData> msg( pNetPacket );

		if ( msg.Body().has_errormsg() )
		{
			RemapText_t arrText[] = {
				{ "Game is full", "#SFUI_DisconnectReason_LobbyFull", RemapText_t::MATCH_FULL },
				{ "Certified server required", "#SFUI_DisconnectReason_CertifiedServerRequired", RemapText_t::MATCH_FULL },
				{ "Certified server denied", "#SFUI_DisconnectReason_CertifiedServerDenied", RemapText_t::MATCH_FULL },
				{ "PW server required", "#SFUI_DisconnectReason_PWServerRequired", RemapText_t::MATCH_FULL },
				{ "PW server denied", "#SFUI_DisconnectReason_PWServerDenied", RemapText_t::MATCH_FULL },
				{ NULL, NULL, RemapText_t::MATCH_FULL }
			};
			char const *szReason = RemapText_t::RemapRawText( arrText, msg.Body().errormsg().c_str() );

			g_pMatchFramework->CloseSession();
			CMessageBoxScaleform::UnloadAllDialogs( true );
			BasePanel()->RestoreMainMenuScreen();
			GameUI().CreateCommandMsgBox( "#SFUI_Disconnect_Title", szReason, true );
			return false;
		}

		ServerCookie_t sc = { msg.Body().res().reservationid(), Plat_FloatTime() };
		g_mapServerCookies.InsertOrReplace( msg.Body().serverid(), sc );

		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_ClientRequestJoinServerData, k_EMsgGCCStrike15_v2_ClientRequestJoinServerData );



#if !defined( NO_STEAM )

void CUIGameData::Steam_OnUserStatsStored( UserStatsStored_t *pParam )
{

#if defined( _PS3 )

	GetPs3SaveSteamInfoProvider()->WriteSteamStats();

#endif

}

void CUIGameData::Steam_OnUserStatsReceived( UserStatsReceived_t *pParam )
{

#if defined( _PS3 )

	GetPs3SaveSteamInfoProvider()->WriteSteamStats();

#endif

}

#endif

//////////////////////////////////////////////////////////////////////////
//
//
// A bunch of helper KeyValues hierarchy readers
//
//
//////////////////////////////////////////////////////////////////////////


bool GameModeHasDifficulty( char const *szGameMode )
{
	return !Q_stricmp( szGameMode, "coop" ) || !Q_stricmp( szGameMode, "realism" );
}

char const * GameModeGetDefaultDifficulty( char const *szGameMode )
{
	if ( !GameModeHasDifficulty( szGameMode ) )
		return "normal";

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetActiveUserId() );
	if ( !pProfile )
		return "normal";

	UserProfileData const &upd = pProfile->GetPlayerProfileData();
	switch ( upd.difficulty )
	{
	case 1: return "easy";
	case 2: return "hard";
	default: return "normal";
	}
}

bool GameModeHasRoundLimit( char const *szGameMode )
{
	return !Q_stricmp( szGameMode, "scavenge" ) || !Q_stricmp( szGameMode, "teamscavenge" );
}

bool GameModeIsSingleChapter( char const *szGameMode )
{
	return !Q_stricmp( szGameMode, "survival" ) || !Q_stricmp( szGameMode, "scavenge" ) || !Q_stricmp( szGameMode, "teamscavenge" );
}


// DWenger - Pulled out temporarily
/*
const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( Q_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		Q_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			Q_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = Q_strlen( modDir );
			Q_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}
*/

uint64 GetDlcInstalledMask()
{
	static ConVarRef mm_dlcs_mask_fake( "mm_dlcs_mask_fake" );
	char const *szFakeDlcsString = mm_dlcs_mask_fake.GetString();
	if ( *szFakeDlcsString )
		return atoi( szFakeDlcsString );

	static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	uint64 uiDLCmask = ( unsigned ) mm_dlcs_mask_extras.GetInt();

	bool bSearchPath = false;
	int numDLCs = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPath );

	for ( int j = 0; j < numDLCs; ++ j )
	{
		unsigned int uiDlcHeader = 0;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( j, &uiDlcHeader, NULL, 0 ) )
			continue;

		int idDLC = DLC_LICENSE_ID( uiDlcHeader );
		if ( idDLC < 1 || idDLC >= 31 )
			continue;	// unsupported DLC id

		uiDLCmask |= ( 1ull << idDLC );
	}

	return uiDLCmask;
}

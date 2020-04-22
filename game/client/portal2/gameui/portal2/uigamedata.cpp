//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "basemodpanel.h"
#include "basemodframe.h"
#include "UIGameData.h"

#include <ctype.h>


#include "./GameUI/IGameUI.h"
#include "ienginevgui.h"
#include "icommandline.h"
#include "vgui/ISurface.h"
#include "EngineInterface.h"
#include "tier0/dbg.h"
#include "ixboxsystem.h"
#include "GameUI_Interface.h"
#include "game/client/IGameClientExports.h"
#include "fmtstr.h"
#include "vstdlib/random.h"
#include "utlbuffer.h"
#include "filesystem/IXboxInstaller.h"
#include "tier1/tokenset.h"
#include "FileSystem.h"
#include "filesystem/IXboxInstaller.h"

#include <time.h>

// BaseModUI High-level windows

#include "VFoundGames.h"
#include "VFoundGroupGames.h"
#include "VGameLobby.h"
#include "VGenericConfirmation.h"
#include "VGenericWaitScreen.h"
#include "VInGameMainMenu.h"
#include "VMainMenu.h"
#include "VFooterPanel.h"
#include "VAttractScreen.h"
#include "VPasswordEntry.h"
// vgui controls
#include "vgui/ILocalize.h"
#include "vstartcoopgame.h"
#include "gameconsole.h"

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	#include "econ_ui.h"
#endif

#include "netmessages.h"
#include "cegclientwrapper.h"

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

#ifdef _PS3
#include "inputsystem/iinputsystem.h"
#include <netex/libnetctl.h>
#include <cell/sysmodule.h>
#endif

#include "gameui_util.h"

#include "steamcloudsync.h"
#include "transitionpanel.h"

#if defined (PORTAL2_PUZZLEMAKER)
#include "puzzlemaker/puzzlemaker.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
extern void OnStorageDevicesChangedSelectNewDevice();

ConVar demo_ui_enable( "demo_ui_enable", "", FCVAR_DEVELOPMENTONLY, "Suffix for the demo UI" );
ConVar demo_connect_string( "demo_connect_string", "", FCVAR_DEVELOPMENTONLY, "Connect string for demo UI" );

///Asyncronous Operations

ConVar mm_ping_max_green( "ping_max_green", "70" );
ConVar mm_ping_max_yellow( "ping_max_yellow", "140" );
ConVar mm_ping_max_red( "ping_max_red", "250" );

#ifndef _CERT
extern ConVar ui_coop_map_default;
#endif
extern ConVar ui_coop_ss_fadeindelay;

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

#ifdef _X360
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
	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	xonline->XShowMarketplaceDownloadItemsUI( iCtrlr,
		g_MarketplaceEntryPoint.dwEntryPoint, &pQuery->uiOfferID, 1,
		&pQuery->hResult, &pQuery->xOverlapped );
#endif
}

static void ShowMarketplaceUiForOffer()
{
#ifdef _X360
	// Stop installing to the hard drive, otherwise STFC fragmentation hazard, as multiple non sequential HDD writes will occur.
	// This needs to be done before the DLC might be downloaded to the HDD, otherwise it could be fragmented.
	// We restart the installer on DLC download completion. We do not handle the cancel/abort case. The installer
	// will restart through the pre-dlc path, i.e. after attract or exiting a map back to the main menu.
	if ( g_pXboxInstaller )
		g_pXboxInstaller->Stop();

	// Open the marketplace entry point
	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	DWORD ret;
	ret = xonline->XShowMarketplaceUI( iCtrlr, g_MarketplaceEntryPoint.dwEntryPoint, g_MarketplaceEntryPoint.uiOfferID, DWORD( -1 ) );
	DevMsg( "XShowMarketplaceUI for offer %llx entry point %d ctrlr%d returned %d\n",
		g_MarketplaceEntryPoint.uiOfferID, g_MarketplaceEntryPoint.dwEntryPoint, iCtrlr, ret );
#endif
}

#ifdef _X360
CON_COMMAND( x360_marketplace_offer, "Get a known offer from x360 marketplace" )
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

//=============================================================================
//
//=============================================================================
CUIGameData* CUIGameData::m_Instance = 0;
bool CUIGameData::m_bModuleShutDown = false;

//=============================================================================
CUIGameData::CUIGameData() :
#if !defined( NO_STEAM )
	m_CallbackGameOverlayActivated( this, &CUIGameData::Steam_OnGameOverlayActivated ),
	m_CallbackPersonaStateChanged( this, &CUIGameData::Steam_OnPersonaStateChanged ),
	m_CallbackAvatarImageLoaded( this, &CUIGameData::Steam_OnAvatarImageLoaded ),
	m_CallbackUserStatsStored( this, &CUIGameData::Steam_OnUserStatsStored ),
	m_CallbackUserStatsReceived( this, &CUIGameData::Steam_OnUserStatsReceived ),
#endif
	m_CGameUIPostInit( false )
{
	m_LookSensitivity = 1.0f;

	m_flShowConnectionProblemTimer = 0.0f;
	m_flTimeLastFrame = Plat_FloatTime();
	m_bShowConnectionProblemActive = false;
	m_bNeedUpdateMatchMutelist = false;
	m_bPendingMapVoteRequest = false;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_bXUIOpen = false;
	m_bSteamOverlayActive = false;

	m_bWaitingForStorageDeviceHandle = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = NULL;

	m_pSelectStorageClient = NULL;

	SetDefLessFunc( m_mapUserXuidToAvatar );
	SetDefLessFunc( m_mapUserXuidToName );

	Q_memset( m_chNotificationMode, 0, sizeof( m_chNotificationMode ) );

	m_numOnlineFriends = 0;
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

void CUIGameData::OnSetStorageDeviceId( int iController, uint nDeviceId )
{
	// Check to see if there is enough room on this storage device
	if ( nDeviceId == XBX_STORAGE_DECLINED || nDeviceId == XBX_INVALID_STORAGE_ID )
	{
		CloseWaitScreen( NULL, "ReportNoDeviceSelected" );
		m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_NOT_SELECTED );
		m_pSelectStorageClient = NULL;
	}
	else if ( IsX360() && xboxsystem && !xboxsystem->DeviceCapacityAdequate( iController, nDeviceId, engine->GetModDirectory() ) )
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
		if ( SelectStorageDevicePolicy() & STORAGE_DEVICE_ASYNC )
			m_pSelectStorageClient->OnDeviceSelected();
	}
}

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

	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() != WT_MAINMENU )
		return false;

	if ( !AllowSplitscreenMainMenu() )
		return false;

	return true;
#else
	return false;
#endif
}

//=============================================================================
void CUIGameData::OpenFriendRequestPanel(int index, uint64 playerXuid)
{
#ifdef _X360 
	XShowFriendRequestUI(index, playerXuid);
#endif
}

//=============================================================================
void CUIGameData::OpenInviteUI( char const *szInviteUiType )
{
#ifdef _X360 
	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	
	if ( !Q_stricmp( szInviteUiType, "friends" ) )
		::XShowFriendsUI( iCtrlr );
	else if ( !Q_stricmp( szInviteUiType, "players" ) )
		xonline->XShowGameInviteUI( iCtrlr, NULL, 0, 0 );
	else if ( !Q_stricmp( szInviteUiType, "party" ) )
		xonline->XShowPartyUI( iCtrlr );
	else if ( !Q_stricmp( szInviteUiType, "inviteparty" ) )
		xonline->XPartySendGameInvites( iCtrlr, NULL );
	else if ( !Q_stricmp( szInviteUiType, "community" ) )
		xonline->XShowCommunitySessionsUI( iCtrlr, XSHOWCOMMUNITYSESSION_SHOWPARTY );
	else if ( !Q_stricmp( szInviteUiType, "voiceui" ) )
		::XShowVoiceChannelUI( iCtrlr );
	else if ( !Q_stricmp( szInviteUiType, "gamevoiceui" ) )
		::XShowGameVoiceChannelUI();
	else
	{
		DevWarning( "OpenInviteUI with wrong parameter `%s`!\n", szInviteUiType );
		Assert( 0 );
	}
#endif
}

void CUIGameData::ExecuteOverlayCommand( char const *szCommand, char const *szErrorText )
{
#if !defined( NO_STEAM ) && !defined( _PS3 )
	// PS3 doesn't support Steam overlay commands
	if ( steamapicontext && steamapicontext->SteamFriends() &&
		 steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
	{
		steamapicontext->SteamFriends()->ActivateGameOverlay( szCommand );
	}
	else
	{
		DisplayOkOnlyMsgBox( NULL, "#L4D360UI_SteamOverlay_Title", szErrorText ? szErrorText : "#L4D360UI_SteamOverlay_Text" );
	}
#else
	ExecuteNTimes( 5, DevWarning( "ExecuteOverlayCommand( %s ) is unsupported\n", szCommand ) );
	Assert( !"ExecuteOverlayCommand" );
#endif
}

bool CUIGameData::IsNetworkCableConnected()
{
	return true;
	//
	// NOTE: this request was raised by EA CERT NET team, but
	// later SCEA didn't request a fix for network cable error
	// messages. Additionally, this condition apparently triggers
	// a lot more often than expected leading to cable error msg
	// appearing for error text and misleading.
	//
#ifdef _PS3
	int netState = CELL_NET_CTL_STATE_Disconnected;
	if ( cellNetCtlGetState( &netState ) < 0 )
		return false;
	if ( netState < CELL_NET_CTL_STATE_IPObtained )
		return false;
#endif
	return true;
}

//=============================================================================
bool CUIGameData::SignedInToLive()
{
#ifdef _X360

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
#ifdef _X360
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

bool CUIGameData::IsUserLIVEEnabled( int nUserID )
{
#ifdef _X360
	return ( eXUserSigninState_SignedInToLive == XUserGetSigninState( nUserID ) );
#endif // _X360

	return false;
}


bool CUIGameData::AnyUserConnectedToLIVE()
{
#ifdef _X360
	for ( uint idx = 0; idx < XBX_GetNumGameUsers(); ++ idx )
	{
		if ( IsUserLIVEEnabled( XBX_GetUserId( idx ) ) )
		{
			return true;
		}
	}
#endif

	return false;
}

bool CUIGameData::IsGuestOrOfflinePlayerWhenSomePlayersAreOnline( int nSlot )
{
#ifdef _GAMECONSOLE
	if ( XBX_GetUserIsGuest( nSlot ) )
	{
		return true;
	}
#endif

#ifdef _X360
	if ( AnyUserConnectedToLIVE() && !IsUserLIVEEnabled( XBX_GetUserId( nSlot ) ) )
	{
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

	if ( IsX360() )
	{
		DisplayOkOnlyMsgBox( pCallerFrame, "#L4D360UI_XboxLive", szMsg );
	}
	else
	{
		DisplayOkOnlyMsgBox( pCallerFrame, "#L4D360UI_PSN", szMsg );
	}
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
		szMsg = "#L4D360UI_MsgBx_NeedLiveNonGoldMsg";

#ifdef _GAMECONSOLE
		// Show the splitscreen version if there are 2 non-guest accounts
		if ( XBX_GetNumGameUsers() > 1 && XBX_GetUserIsGuest( 0 ) == false && XBX_GetUserIsGuest( 1 ) == false )
		{
			szMsg = "#L4D360UI_MsgBx_NeedLiveNonGoldSplitscreenMsg";
		}
#endif
	}
	else
	{
		szMsg = "#L4D360UI_MsgBx_NeedLiveSinglescreenMsg";

#ifdef _GAMECONSOLE
		// Show the splitscreen version if there are 2 non-guest accounts
		if ( XBX_GetNumGameUsers() > 1 && XBX_GetUserIsGuest( 0 ) == false && XBX_GetUserIsGuest( 1 ) == false )
		{
			szMsg = "#L4D360UI_MsgBx_NeedLiveSplitscreenMsg";
		}
#endif
	}

	DisplayOkOnlyMsgBox( pCallerFrame, "#L4D360UI_XboxLive", szMsg );

	return true;
}

bool CUIGameData::CanSendLiveGameInviteToUser( XUID xuid )
{
#ifdef _X360
	BOOL bHasPrivileges;
	DWORD dwResult = XUserCheckPrivilege( XBX_GetPrimaryUserId(), XPRIVILEGE_COMMUNICATIONS, &bHasPrivileges );
	if ( dwResult == ERROR_SUCCESS )
	{
		if ( !bHasPrivileges )
		{
			// Second call checks for friends-only
			XUserCheckPrivilege( XBX_GetPrimaryUserId(), XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY, &bHasPrivileges );
			if ( bHasPrivileges )
			{
				if ( !xuid )
					return true;

				// Privileges are set to friends-only. See if the remote player is on our friends list.
				BOOL bIsFriend;
				dwResult = XUserAreUsersFriends( XBX_GetPrimaryUserId(), &xuid, 1, &bIsFriend, NULL );
				if ( dwResult != ERROR_SUCCESS || !bIsFriend )
				{
					return false;
				}
			}
			else
			{
				// Privilege is nobody
				return false;
			}
		}
	}
	return true;
#else
	return true;
#endif
}

void CUIGameData::DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg )
{
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pCallerFrame, false ) );
	GenericConfirmation::Data_t data;
	data.pWindowTitle = szTitle;
	data.pMessageText = szMsg;
	data.bOkButtonEnabled = true;
	confirmation->SetUsageData(data);
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

bool CUIGameData::IsSteamOverlayActive()
{
	return m_bSteamOverlayActive;
}

bool CUIGameData::OpenWaitScreen( const char * messageText, float minDisplayTime, KeyValues *pSettings )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] OpenWaitScreen( %s )\n", messageText );
	}

	WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	CBaseModFrame * backFrame = CBaseModPanel::GetSingleton().GetWindow( wtActive );
	if ( wtActive == WT_GENERICWAITSCREEN && backFrame )
	{
		backFrame = backFrame->GetNavBack();
		DevMsg( "CUIGameData::OpenWaitScreen - setting navback to %s instead of waitscreen\n", backFrame ? backFrame->GetName() : "NULL" );
	}
	if ( wtActive == WT_GENERICCONFIRMATION && backFrame )
	{
		DevWarning( "Cannot display waitscreen! Active window of higher priority: %s\n", backFrame->GetName() );
		return false;
	}

	GenericWaitScreen* waitScreen = dynamic_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICWAITSCREEN, backFrame, false, pSettings ) );

	Assert( waitScreen != NULL );
	if ( !waitScreen )
		return false;

	waitScreen->SetNavBack( backFrame );
	waitScreen->ClearData();
	// hack for opening waitscreen with a pre-localized message
	if ( char const *szPtrValue = StringAfterPrefix( messageText, "$ptr" ) )
	{
		uint32 uiPtrValue = 0;
		sscanf( szPtrValue, "%u", &uiPtrValue );
		waitScreen->AddMessageText( uiPtrValue ? reinterpret_cast< wchar_t const * >( uiPtrValue ) : L"*", minDisplayTime );
	}
	else
	{
		waitScreen->AddMessageText( messageText, minDisplayTime );
	}

	return true;
}

void CUIGameData::UpdateWaitPanel( const char * messageText, float minDisplayTime )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] UpdateWaitPanel( %s )\n", messageText );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->AddMessageText( messageText, minDisplayTime );
	}
}

void CUIGameData::UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] UpdateWaitPanel( %S )\n", messageText );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->AddMessageText( messageText, minDisplayTime );
	}
}

#if defined( PORTAL2_PUZZLEMAKER )
void CUIGameData::UpdateWaitPanel( UGCHandle_t hFileHandle, float flCustomProgress /*= -1.f*/ )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] UpdateWaitPanel( %llu )\n", hFileHandle );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->SetTargetFileHandle( hFileHandle, flCustomProgress );
	}
}
#endif // PORTAL2_PUZZLEMAKER

bool CUIGameData::CloseWaitScreen( vgui::Panel * callbackPanel, const char * message )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CloseWaitScreen( %s )\n", message );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if ( waitScreen )
	{
		waitScreen->SetCloseCallback( callbackPanel, message );
	}

	return ( waitScreen != NULL );
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
	PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().OpenWindow( WT_PASSWORDENTRY, NULL, false ) );
	if ( pwEntry )
	{
		PasswordEntry::Data_t data;
		data.pWindowTitle = "#L4D360UI_PasswordEntry_Title";
		data.pMessageText = "#L4D360UI_PasswordEntry_Prompt";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;
		data.m_szCurrentPW = pchCurrentPW;
		data.pfnOkCallback = PasswordEntered;
		data.pfnCancelCallback = PasswordNotEntered;
		pwEntry->SetUsageData(data);
	}
}

void CUIGameData::FinishPasswordUI( bool bOk )
{
	PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().GetWindow( WT_PASSWORDENTRY ) );
	if ( pwEntry )
	{
		if ( bOk )
		{
			char pw[ 256 ];
			pwEntry->GetPassword( pw, sizeof( pw ) );
			engine->SetConnectionPassword( pw );
		}
		else
		{
			engine->SetConnectionPassword( "" );
		}
	}
}

IImage *CUIGameData::AccessAvatarImage( XUID playerID, UiAvatarImageAccessType_t eAccess, CGameUiAvatarImage::AvatarSize_t nAvatarSize /*= CGameUiAvatarImage::MEDIUM*/ )
{
	if ( eAccess == kAvatarImageNull )
		return NULL;

	if ( !playerID )
		return NULL;

	// do we already have this image cached?
	CGameUiAvatarImage *pImage = NULL;
	int iIndex = m_mapUserXuidToAvatar.Find( playerID );
	
	if ( iIndex == m_mapUserXuidToAvatar.InvalidIndex() )
	{
		// We don't have this image cached, so obviously we can only request a new instance
		if ( eAccess != kAvatarImageRequest )
		{
			Assert( 0 );
			return NULL;
		}

		// cache a new image
		pImage = new CGameUiAvatarImage();

		// We may fail to set the steam ID - if the player is not our friend and we are not in a lobby or game, eg
		if ( !pImage->SetAvatarXUID( playerID, nAvatarSize ) )
		{
			delete pImage;
			return NULL;
		}

		iIndex = m_mapUserXuidToAvatar.Insert( playerID, pImage );
	}
	else
	{
		// Locate an existing image
		pImage = m_mapUserXuidToAvatar.Element( iIndex );
		// make sure it is the requested size
		if ( pImage && pImage->GetAvatarSize() != nAvatarSize )
		{
			if ( !pImage->SetAvatarXUID( playerID, nAvatarSize ) )
			{
				m_mapUserXuidToAvatar.Remove( playerID );
				delete pImage;
				return NULL;
			}
		}
	}

	Assert( pImage );
	if ( eAccess == kAvatarImageRequest )
	{
		int nRefcount = pImage->AdjustRefCount( +1 ); nRefcount;
		DevMsg( "Avatar image for user %llX cached [refcount=%d]\n", playerID, nRefcount );
	}
	else
	{
		int nRemainingRefCount = pImage->AdjustRefCount( -1 );
		if ( nRemainingRefCount <= 0 )
		{
			pImage = NULL;
			m_mapUserXuidToAvatar.RemoveAt( iIndex );
		}
		DevMsg( "Avatar image for user %llX released [refcount=%d]\n", playerID, nRemainingRefCount );
	}

	return pImage;
}

char const * CUIGameData::GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

#if !defined( NO_STEAM )
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

#if !defined( NO_STEAM )
void CUIGameData::Steam_OnUserStatsStored( UserStatsStored_t *pParam )
{
#ifdef _PS3
	GetPs3SaveSteamInfoProvider()->WriteSteamStats();
#endif
}

void CUIGameData::Steam_OnUserStatsReceived( UserStatsReceived_t *pParam )
{
#ifdef _PS3
	GetPs3SaveSteamInfoProvider()->WriteSteamStats();
#endif
}

void CUIGameData::Steam_OnGameOverlayActivated( GameOverlayActivated_t *pParam )
{
	m_bSteamOverlayActive = !!pParam->m_bActive;
}

void CUIGameData::Steam_OnAvatarImageLoaded( AvatarImageLoaded_t *pParam )
{
	if ( !pParam->m_steamID.IsValid() )
		return;

	CGameUiAvatarImage *pImage = NULL;
	int iIndex = m_mapUserXuidToAvatar.Find( pParam->m_steamID.ConvertToUint64() );
	if ( iIndex != m_mapUserXuidToAvatar.InvalidIndex() )
	{
		pImage = m_mapUserXuidToAvatar.Element( iIndex );
	}

	// Re-fetch the image if we have it cached
	if ( pImage )
	{
		pImage->SetAvatarXUID( pParam->m_steamID.ConvertToUint64() );
	}
}

void CUIGameData::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( !pParam->m_ulSteamID )
		return;

	if ( pParam->m_nChangeFlags & k_EPersonaChangeRelationshipChanged )
		m_bNeedUpdateMatchMutelist = true;

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
		CGameUiAvatarImage *pImage = NULL;
		int iIndex = m_mapUserXuidToAvatar.Find( pParam->m_ulSteamID );
		if ( iIndex != m_mapUserXuidToAvatar.InvalidIndex() )
		{
			pImage = m_mapUserXuidToAvatar.Element( iIndex );
		}

		// Re-fetch the image if we have it cached
		if ( pImage )
		{
			pImage->SetAvatarXUID( pParam->m_ulSteamID );
		}
	}

	if ( pParam->m_nChangeFlags )
	{
		// count friends who are online
		int numFriends = steamapicontext->SteamFriends()->GetFriendCount( k_EFriendFlagImmediate );
		int numOnlineFriends = 0;
		for ( int jj = 0; jj < numFriends; ++ jj )
		{
			CSteamID sID = steamapicontext->SteamFriends()->GetFriendByIndex( jj, k_EFriendFlagImmediate );
			if ( !sID.IsValid() )
				continue;
#ifdef _PS3
			if ( sID.BConsoleUserAccount() )
				continue;
#endif
			if ( steamapicontext->SteamFriends()->GetFriendPersonaState( sID ) == k_EPersonaStateOffline )
				continue;
			++ numOnlineFriends;
		}
		m_numOnlineFriends = numOnlineFriends;
	}
}

#ifdef _PS3
static char const *g_szConnectionToSteamReason = NULL;
static char const *g_szConnectionToSteamGameMode = NULL;
static int g_nConnectionToSteamReasonResetMsgId = 0;
uint64 g_uiConnectionToSteamToJoinLobbyId = 0ull;
static void ResetConnectToSteamReason()
{
	g_szConnectionToSteamReason = NULL;
	g_szConnectionToSteamGameMode = NULL;
	g_nConnectionToSteamReasonResetMsgId = 0;

	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() == WT_ATTRACTSCREEN )
	{
		CBaseModPanel::GetSingleton().CloseAllWindows();
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
	}
}
static void ProcessConnectToSteamReason()
{
	char const *szReason = g_szConnectionToSteamReason;
	char const *szGameMode = g_szConnectionToSteamGameMode;
	ResetConnectToSteamReason();
	
	if ( !Q_stricmp( "invitejoin", szReason ) )
	{
		KeyValues *kvEvent = new KeyValues( "OnSteamOverlayCall::LobbyJoin" );
		kvEvent->SetUint64( "sessionid", g_uiConnectionToSteamToJoinLobbyId );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
	}
	else if ( !Q_stricmp( "playonline", szReason ) || !Q_stricmp( "playlan", szReason ) )
	{
		CUIGameData::Get()->InitiateOnlineCoopPlay( NULL, szReason, szGameMode );
	}
}
class CSteamLogonAsyncOperation : public IMatchAsyncOperation
{
public:
	// Poll if operation has completed
	virtual bool IsFinished() { return m_eState != AOS_RUNNING; }

	// Operation state
	virtual AsyncOperationState_t GetState() { return m_eState; }

	// Retrieve a generic completion result for simple operations
	// that return simple results upon success,
	// results are operation-specific, may result in undefined behavior
	// if operation is still in progress.
	virtual uint64 GetResult() { return 0ull; }

	// Request operation to be aborted
	virtual void Abort();

	// Release the operation interface and all resources
	// associated with the operation. Operation callbacks
	// will not be called after Release. Operation object
	// cannot be accessed after Release.
	virtual void Release() { Assert( 0 ); }

public:
	CSteamLogonAsyncOperation() { m_eState = AOS_FAILED; }
	IMatchAsyncOperation * Prepare();
	void LogonFinished( AsyncOperationState_t eState, CBaseModFrame *pWnd = NULL );

public:
	static KeyValues * PrepareToPerformAnySortOfLogon();
	static void PerformAutomaticLogon();
	static void PerformAnonymousLogon();
	static void PerformUserPwdLogon( char const *szUserName, char const *szPwd );
	static void LogonCongratulationsCallback();

public:
	STEAM_CALLBACK_MANUAL( CSteamLogonAsyncOperation, OnSteamServersConnected, SteamServersConnected_t, m_SteamServersConnected );
	STEAM_CALLBACK_MANUAL( CSteamLogonAsyncOperation, OnSteamServerConnectFailure, SteamServerConnectFailure_t, m_SteamServerConnectFailure );

public:
	AsyncOperationState_t m_eState;
	CBaseModFrame *m_pWaitScreenNavBack;
	enum LogonType_t
	{
		LOGON_AUTOMATIC,
		LOGON_ANONYMOUS,
		LOGON_USERPWD
	};
	LogonType_t m_eLogonType;
}
g_SteamLogonAsyncOperation;

class CSteamLogonAsyncOperationWaitCallback : public IWaitscreenCallbackInterface
{
protected:
	virtual void OnThink()
	{
		if ( !m_bListening ||
			!steamapicontext->SteamUser()->BLoggedOn() )
			return;

		m_bListening = false;
		g_SteamLogonAsyncOperation.LogonFinished( AOS_SUCCEEDED );
		ProcessConnectToSteamReason();
	}

public:
	IWaitscreenCallbackInterface * Prepare() { m_bListening = true; return this; }

public:
	bool m_bListening;
}
g_SteamLogonAsyncOperationWaitCallback;

IMatchAsyncOperation * CSteamLogonAsyncOperation::Prepare()
{
	m_SteamServersConnected.Register( this, &CSteamLogonAsyncOperation::OnSteamServersConnected );
	m_SteamServerConnectFailure.Register( this, &CSteamLogonAsyncOperation::OnSteamServerConnectFailure );

	m_eState = AOS_RUNNING;

	return this;
}

void CSteamLogonAsyncOperation::LogonFinished( AsyncOperationState_t eState, CBaseModFrame *pWnd )
{
	if ( m_eState != AOS_RUNNING )
		return;

	m_eState = eState;

	m_SteamServersConnected.Unregister();
	m_SteamServerConnectFailure.Unregister();
	g_SteamLogonAsyncOperationWaitCallback.m_bListening = false;

	// CUIGameData::Get()->CloseWaitScreen( &CBaseModPanel::GetSingleton(), "CSteamLogonAsyncOperationLogonFinished" );
	CBaseModFrame *pSpinner = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN );
	if ( pSpinner && pWnd )
	{
		pSpinner->SetNavBack( pWnd );
	}
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

void CSteamLogonAsyncOperation::Abort()
{
	LogonFinished( AOS_ABORTED );
}

KeyValues * CSteamLogonAsyncOperation::PrepareToPerformAnySortOfLogon()
{
	if ( steamapicontext->SteamUser()->BLoggedOn() )
	{
		g_SteamLogonAsyncOperation.Prepare();
		g_SteamLogonAsyncOperation.LogonFinished( AOS_SUCCEEDED );
		ProcessConnectToSteamReason();
		return NULL;
	}
	
	KeyValues *pSettings = new KeyValues( "WaitScreen" );
	pSettings->SetPtr( "options/asyncoperation", g_SteamLogonAsyncOperation.Prepare() );
	pSettings->SetPtr( "options/waitscreencallback", g_SteamLogonAsyncOperationWaitCallback.Prepare() );
	return pSettings;
}

void CSteamLogonAsyncOperation::PerformAutomaticLogon()
{
	g_nConnectionToSteamReasonResetMsgId = 0;

	KeyValues *pSettings = PrepareToPerformAnySortOfLogon();
	if ( !pSettings )
		return;
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	
	CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_Steam_Connecting", 0.0f, pSettings );
	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	
	g_SteamLogonAsyncOperation.m_pWaitScreenNavBack = waitScreen ? waitScreen->GetNavBack() : NULL;
	g_SteamLogonAsyncOperation.m_eLogonType = LOGON_AUTOMATIC;
	
	steamapicontext->SteamUser()->LogOn( true );
}

void CSteamLogonAsyncOperation::PerformAnonymousLogon()
{
	KeyValues *pSettings = PrepareToPerformAnySortOfLogon();
	if ( !pSettings )
		return;
	KeyValues::AutoDelete autodelete_pSettings( pSettings );

	CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_Steam_LinkingAnonymous", 0.0f, pSettings );
	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );

	g_SteamLogonAsyncOperation.m_pWaitScreenNavBack = waitScreen ? waitScreen->GetNavBack() : NULL;
	g_SteamLogonAsyncOperation.m_eLogonType = LOGON_ANONYMOUS;

	steamapicontext->SteamUser()->LogOnAndCreateNewSteamAccountIfNeeded( true );
}

void CSteamLogonAsyncOperation::PerformUserPwdLogon( char const *szUserName, char const *szPwd )
{
	KeyValues *pSettings = PrepareToPerformAnySortOfLogon();
	if ( !pSettings )
		return;
	KeyValues::AutoDelete autodelete_pSettings( pSettings );

	CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_Steam_LinkingUserPwd", 0.0f, pSettings );
	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );

	g_SteamLogonAsyncOperation.m_pWaitScreenNavBack = waitScreen ? waitScreen->GetNavBack() : NULL;
	g_SteamLogonAsyncOperation.m_eLogonType = LOGON_USERPWD;

	steamapicontext->SteamUser()->LogOnAndLinkSteamAccountToPSN( true, szUserName, szPwd );
}

void CSteamLogonAsyncOperation::OnSteamServersConnected( SteamServersConnected_t *pParam )
{
	DevMsg( "CSteamLogonAsyncOperation::OnSteamServersConnected: %d\n", steamapicontext->SteamUser()->BLoggedOn() );

	if ( steamapicontext->SteamUser()->BLoggedOn() )
	{
		LogonFinished( AOS_SUCCEEDED );

		// Check what's the reason for steam servers got connected
		ProcessConnectToSteamReason();
		return;
	}
	LogonFinished( AOS_FAILED );

	GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, m_pWaitScreenNavBack, true ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#L4D360UI_Steam";
	data.pMessageText = "#L4D360UI_Steam_Error_Unexpected";
#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
		data.pMessageText = "#L4D360UI_Steam_Error_PSN";
#endif
	if ( !CUIGameData::Get()->IsNetworkCableConnected() )
		data.pMessageText = "#L4D360UI_Steam_Error_NetworkCable";

	data.bOkButtonEnabled = true;
	data.pfnOkCallback = &PerformAutomaticLogon;
	data.bCancelButtonEnabled = true;
	data.pfnCancelCallback = ResetConnectToSteamReason;

	g_nConnectionToSteamReasonResetMsgId = confirmation->SetUsageData(data);
}

void CSteamLogonAsyncOperation::OnSteamServerConnectFailure( SteamServerConnectFailure_t *pParam )
{
	DevMsg( "CSteamLogonAsyncOperation::OnSteamServerConnectFailure: %d\n", pParam->m_eResult );

	if ( m_eLogonType == LOGON_AUTOMATIC )
	{
		switch ( pParam->m_eResult )
		{
#if defined( _PS3 ) && !defined( NO_STEAM )
		case k_EResultExternalAccountUnlinked:
			if ( steamapicontext && steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->BIsPSNOnline() )
			{
				// Need more decisions from user
				CBaseModFrame *pWnd = CBaseModPanel::GetSingleton().OpenWindow( WT_STEAMLINKDIALOG, m_pWaitScreenNavBack, true );
				LogonFinished( AOS_FAILED, pWnd );
				break;
			}
			// Otherwise fall through, show PSN error
#endif

		default:
			{
				LogonFinished( AOS_FAILED );
				GenericConfirmation* confirmation = 
					static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, m_pWaitScreenNavBack, true ) );

				GenericConfirmation::Data_t data;

				data.pWindowTitle = "#L4D360UI_Steam";
				data.pMessageText = "#L4D360UI_Steam_Error_Unexpected";
				bool bCanRetry = true;
				if ( pParam->m_eResult == k_EResultLoggedInElsewhere )
				{
					data.pMessageText = "#L4D360UI_Steam_Error_LinkLoggedInElsewhere";
					bCanRetry = false;
				}
				else if ( pParam->m_eResult == k_EResultAccountDisabled )
				{
					data.pMessageText = "#L4D360UI_Steam_Error_LinkAccountDisabled";
					bCanRetry = false;
				}
				else if ( pParam->m_eResult == k_EResultParentalControlRestricted )
				{
					data.pMessageText = "#L4D360UI_Steam_Error_ParentalControl";
					bCanRetry = false;
				}
#if defined( _PS3 ) && !defined( NO_STEAM )
				if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
					data.pMessageText = "#L4D360UI_Steam_Error_PSN";
#endif
				if ( !CUIGameData::Get()->IsNetworkCableConnected() )
					data.pMessageText = "#L4D360UI_Steam_Error_NetworkCable";

				data.bOkButtonEnabled = true;
				if ( bCanRetry )
				{
					data.pfnOkCallback = &PerformAutomaticLogon;
					data.bCancelButtonEnabled = true;
					data.pfnCancelCallback = ResetConnectToSteamReason;
				}
				else
				{
					data.pfnOkCallback = ResetConnectToSteamReason;
				}

				g_nConnectionToSteamReasonResetMsgId = confirmation->SetUsageData(data);
			}
			break;
		}
	}
	else if ( m_eLogonType == LOGON_ANONYMOUS )
	{
		LogonFinished( AOS_FAILED );
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, m_pWaitScreenNavBack, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_Steam";
		data.pMessageText = "#L4D360UI_Steam_Error_Unexpected";
		bool bCanRetry = true;
		if ( pParam->m_eResult == k_EResultParentalControlRestricted )
		{
			data.pMessageText = "#L4D360UI_Steam_Error_ParentalControl";
			bCanRetry = false;
		}
#if defined( _PS3 ) && !defined( NO_STEAM )
		if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
			data.pMessageText = "#L4D360UI_Steam_Error_PSN";
#endif
		if ( !CUIGameData::Get()->IsNetworkCableConnected() )
			data.pMessageText = "#L4D360UI_Steam_Error_NetworkCable";

		data.bOkButtonEnabled = true;
		if ( bCanRetry )
		{
			data.pfnOkCallback = &PerformAutomaticLogon;
			data.bCancelButtonEnabled = true;
			data.pfnCancelCallback = ResetConnectToSteamReason;
		}
		else
		{
			data.pfnOkCallback = ResetConnectToSteamReason;
		}

		g_nConnectionToSteamReasonResetMsgId = confirmation->SetUsageData(data);
	}
	else if ( m_eLogonType == LOGON_USERPWD )
	{
#if defined( _PS3 ) && !defined( NO_STEAM )
		if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
		{
			LogonFinished( AOS_FAILED );
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, m_pWaitScreenNavBack, true ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_Steam";
			data.pMessageText = "#SessionError_PSN";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = ResetConnectToSteamReason;

			g_nConnectionToSteamReasonResetMsgId = confirmation->SetUsageData(data);
			return;
		}
#endif

		KeyValues *kv = new KeyValues( "SteamLink" );
		KeyValues::AutoDelete autodelete_kv( kv );
		switch ( pParam->m_eResult )
		{
		case k_EResultInvalidPassword:
		case k_EResultInvalidName:
			kv->SetString( "error", "#L4D360UI_Steam_Error_LinkAuth" );
			break;
		case k_EResultLoggedInElsewhere:
			kv->SetString( "error", "#L4D360UI_Steam_Error_LinkLoggedInElsewhere" );
			break;
		case k_EResultAccountDisabled:
			kv->SetString( "error", "#L4D360UI_Steam_Error_LinkAccountDisabled" );
			break;
		case k_EResultExternalAccountAlreadyLinked:
			kv->SetString( "error", "#L4D360UI_Steam_Error_LinkAlreadyLinked" );
			break;
		case k_EResultParentalControlRestricted:
			kv->SetString( "error", "#L4D360UI_Steam_Error_ParentalControl" );
			break;
		default:
			kv->SetString( "error", "#L4D360UI_Steam_Error_LinkUnexpected" );
			break;
		}
		CBaseModFrame *pWnd = CBaseModPanel::GetSingleton().OpenWindow( WT_STEAMLINKDIALOG, m_pWaitScreenNavBack, true, kv );
		LogonFinished( AOS_FAILED, pWnd );
	}
}
#endif

bool CUIGameData::CanInitiateConnectionToSteam()
{
#ifndef _PS3
	return false;
#else
	return steamapicontext && steamapicontext->SteamUser() && !steamapicontext->SteamUser()->BLoggedOn() && g_SteamLogonAsyncOperation.IsFinished()
		&& !CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN )
		&& !CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION )
		&& !CBaseModPanel::GetSingleton().GetWindow( WT_STEAMLINKDIALOG );
#endif
}

bool CUIGameData::InitiateConnectionToSteam( char const *szUserName /* = NULL */, char const *szPwd /* = NULL */ )
{
	if ( !CanInitiateConnectionToSteam() )
		return false;

#ifndef _PS3
	return false;
#else
	// Force Steam to do an automatic log on
	if ( !szUserName || !szPwd )
		g_SteamLogonAsyncOperation.PerformAutomaticLogon();
	else if ( !szUserName[0] || !szPwd[0] )
		g_SteamLogonAsyncOperation.PerformAnonymousLogon();
	else
		g_SteamLogonAsyncOperation.PerformUserPwdLogon( szUserName, szPwd );
	return true;
#endif
}

void CUIGameData::SetConnectionToSteamReason( char const *szReason /* = NULL */, char const *szGameMode /* = NULL */ )
{
#ifdef _PS3
	g_szConnectionToSteamReason = szReason;
	g_szConnectionToSteamGameMode = szGameMode;
	if ( !szReason )
		ResetConnectToSteamReason();
#endif
}
#endif

class CSplitscreenPartnerDetection : public IMatchAsyncOperation
{
public:
	// Poll if operation has completed
	virtual bool IsFinished() { return m_eState != AOS_RUNNING; }

	// Operation state
	virtual AsyncOperationState_t GetState() { return m_eState; }

	// Retrieve a generic completion result for simple operations
	// that return simple results upon success,
	// results are operation-specific, may result in undefined behavior
	// if operation is still in progress.
	virtual uint64 GetResult() { return 0ull; }

	// Request operation to be aborted
	virtual void Abort();

	// Release the operation interface and all resources
	// associated with the operation. Operation callbacks
	// will not be called after Release. Operation object
	// cannot be accessed after Release.
	virtual void Release() { Assert( 0 ); }

public:
	CSplitscreenPartnerDetection() { m_eState = AOS_FAILED; }
	IMatchAsyncOperation * Prepare();

public:
	AsyncOperationState_t m_eState;
}
g_SplitscreenPartnerDetection;

IMatchAsyncOperation * CSplitscreenPartnerDetection::Prepare()
{
	m_eState = AOS_RUNNING;

	return this;
}

void CSplitscreenPartnerDetection::Abort()
{
	m_eState = AOS_FAILED;

	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

class CSplitscreenPartnerDetectionCallback : public IWaitscreenCallbackInterface
{
public:
	CSplitscreenPartnerDetectionCallback()
	{
		m_idUser = 0;
		m_flUserTimestamp = 0;
	}
public:
	// Allows a callback interface get a first crack at keycode
	// return true to allow processing by waitscreen
	// return false to swallow the keycode
	virtual bool OnKeyCodePressed( vgui::KeyCode code )
	{
		switch ( GetBaseButtonCode( code ) )
		{
		case KEY_XBUTTON_INACTIVE_START:
#ifdef _X360
			{
				int userId = GetJoystickForCode( code );
				if ( // !XBX_GetPrimaryUserIsGuest() && -- guests will have to sign in
					userId != (int) XBX_GetPrimaryUserId() &&
					userId >= 0 )
				{
					// Pass the index of controller which wanted to join splitscreen
					CBaseModPanel::GetSingleton().CloseAllWindows();
					CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GOSPLITSCREEN, userId );
					CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
				}
			}
#endif
#ifdef _PS3
			{
				int userId = GetJoystickForCode( code );
				if ( userId < 0 || userId >= XUSER_MAX_COUNT )
					return false;
				// This user pressed START within timeslice from another user,
				// see if we have both guys ready?
				if ( ( ( Plat_FloatTime() - m_flUserTimestamp ) < 3.0f ) && ( userId != m_idUser ) )
				{
					CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->PreventTransitions( true );
					CBaseModPanel::GetSingleton().AddFadeinDelayAfterOverlay( ui_coop_ss_fadeindelay.GetFloat(), true );

					// both are ready, go-go-go
					// Pass the index of controller which wanted to join splitscreen
					CBaseModPanel::GetSingleton().CloseAllWindows();
					XBX_SetPrimaryUserId( m_idUser );
					CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GOSPLITSCREEN, userId );
					CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
				}
				else
				{
					m_idUser = userId;
					m_flUserTimestamp = Plat_FloatTime();
				}
			}
#endif
			return false;
		}
		return true;
	}

	virtual void OnThink()
	{
#ifdef _PS3
		// Keep PS3 input system in START button identification mode
		// until both splitscreen users are detected
		g_pInputSystem->SetPS3StartButtonIdentificationMode();
#endif
	}

	// Splitscreen users and their key timestamps
	int m_idUser;
	float m_flUserTimestamp;
}
g_SplitscreenPartnerDetectionCallback;

void CUIGameData::InitiateSplitscreenPartnerDetection( const char* szGameMode, char const *szMapName /*= NULL*/ )
{
	if ( !IsGameConsole() )
		return;

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	KeyValues *pSettings = new KeyValues( "WaitScreen" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetPtr( "options/asyncoperation", g_SplitscreenPartnerDetection.Prepare() );
	pSettings->SetPtr( "options/waitscreencallback", &g_SplitscreenPartnerDetectionCallback );
	pSettings->SetString( "game/mode", szGameMode );
	if ( !V_stricmp( szGameMode, "coop_challenge" ) )
	{
		pSettings->SetString( "game/map", szMapName );
	}
	OpenWaitScreen( IsPS3() ? "#L4D360UI_PressStartBothForSplitscreen" : "#L4D360UI_PressStartToBeginSplitscreen", 0.0f, pSettings );
}

bool CUIGameData::AllowSplitscreenMainMenu()
{
	// Portal 2 can only be ingame splitscreen
	return false;
}

CEG_NOINLINE void CUIGameData::InitiateOnlineCoopPlay( CBaseModFrame *pCaller, char const *szType, char const *szGameMode, char const *szMapName /*= NULL*/ )
{
	bool bShouldSetMapName = !V_stricmp( "coop_challenge", szGameMode ) || !V_stricmp( "coop_community", szGameMode );
	if ( !Q_stricmp( szType, "quickmatch" ) )
	{
		g_pMatchFramework->CloseSession();
		CBaseModPanel::GetSingleton().CloseAllWindows();

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			PORTAL2_LOBBY_CONFIG_COOP( "LIVE", "public" )
			" options { "
				" action quickmatch "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );
	#ifndef _CERT
		if ( ui_coop_map_default.GetString()[0] )
			pSettings->SetString( "game/map", ui_coop_map_default.GetString() );
	#endif
		pSettings->SetString( "game/type", "quickmatch" );
		pSettings->SetString( "game/mode", szGameMode );
		if ( bShouldSetMapName && szMapName )
		{
			pSettings->SetString( "game/map", szMapName );
		}
		pSettings->SetString( "game/platform", IsPS3() ? "ps3" : "default" );
		g_pMatchFramework->MatchSession( pSettings );

		return;
	}

	// Used from UI calls
	bool bOnline = !Q_stricmp( "playonline", szType );
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

#if defined( _X360 )
	if ( bOnline && CheckAndDisplayErrorIfNotSignedInToLive( pCaller ) )
		return;
#else
	bOnline = true;
#endif

	CEG_PROTECT_MEMBER_FUNCTION( CUIGameData_InitiateOnlineCoopPlay );

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	if ( !( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamMatchmaking() ) )
	{
		GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pCaller, false ) );
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
		return;
	}
#endif

#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( CanInitiateConnectionToSteam() )
	{
		SetConnectionToSteamReason( bOnline ? "playonline" : "playlan", szGameMode );
		InitiateConnectionToSteam();
		return;
	}
#endif

	//
	// All conditions seem ok to go ahead and create a lobby
	//
	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		PORTAL2_LOBBY_CONFIG_COOP( "LIVE", "private" )
		" options { "
			" action create "
		" } "
		);
	KeyValues::AutoDelete autodelete_kv( pSettings );

	pSettings->SetString( "game/mode", szGameMode );
	if ( bShouldSetMapName )
	{
		pSettings->SetString( "game/map", szMapName );
	}

#ifndef _CERT
	if ( ui_coop_map_default.GetString()[0] )
		pSettings->SetString( "game/map", ui_coop_map_default.GetString() );
#endif
#ifdef _X360
	if ( !bOnline )
	{
		pSettings->SetString( "system/network", "lan" );
		pSettings->SetString( "system/access", "public" );
	}
#endif

	g_pMatchFramework->CreateSession( pSettings );
}

CEG_NOINLINE void CUIGameData::InitiateSinglePlayerPlay( const char *pMapName, const char *pSaveName, const char *szPlayType )
{
	// Portal 2 single player server is still driven by session
	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		PORTAL2_LOBBY_CONFIG_COOP( "offline", "public" )
		" options { "
			" action create "
		" } "
		);

	bool bLan = false;
	if ( !V_stricmp( szPlayType, "commentary" ) )
	{
		pSettings->SetString( "options/play", "commentary" );
	}
	else if ( !V_stricmp( szPlayType, "challenge" ) )
	{
		pSettings->SetString( "options/play", "challenge" );
#ifdef _X360
		// must be "lan" so Silver account can play challenge mode
		pSettings->SetString( "system/network", "lan" );
		bLan = true;
#endif
	}

	pSettings->SetString( "game/mode", "sp" );
	pSettings->SetString( "game/map", pMapName );
	if ( pSaveName && *pSaveName )
	{
		pSettings->SetString( "game/save", pSaveName );
	}

	KeyValues::AutoDelete autodelete( pSettings );
	g_pMatchFramework->CreateSession( pSettings );
	
	CEG_PROTECT_MEMBER_FUNCTION( CUIGameData_InitiateSinglePlayerPlay );

	// Submit stats about that
	GameStats_ReportAction( szPlayType, pMapName, 0 );

	if ( bLan )
	{
		return;
	}

	// Automatically start sp session, no configuration required
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
	}
}

CEG_NOINLINE void CUIGameData::InitiateSplitscreenPlay()
{
	CEG_PROTECT_MEMBER_FUNCTION( CUIGameData_InitiateSplitscreenPlay );

	// Start a splitscreen game
	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		PORTAL2_LOBBY_CONFIG_COOP( "offline", "public" )
		" options { "
		" action create "
		" } "
		);

	bool bImmediateStart = true;
	const char *pchGameMode = CStartCoopGame::CoopGameMode();
	if ( pchGameMode && pchGameMode[ 0 ] != '\0' )
	{
		pSettings->SetString( "game/mode", pchGameMode );
		const char *pchChallengeMap = CStartCoopGame::CoopChallengeMap();
		if ( pchGameMode && !V_stricmp( pchGameMode, "coop_challenge" ) )
		{
			if ( IsX360() )
			{
				// must be "lan" so Silver account can play challenge mode
				pSettings->SetString( "system/network", "lan" );
				bImmediateStart = false;
			}
			pSettings->SetString( "game/map", pchChallengeMap );
		}
	}

#ifndef _CERT
	if ( ui_coop_map_default.GetString()[0] )
		pSettings->SetString( "game/map", ui_coop_map_default.GetString() );
#endif

	if ( bImmediateStart )
	{
		CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->PreventTransitions( true );
	}

	KeyValues::AutoDelete autodelete( pSettings );
	g_pMatchFramework->CreateSession( pSettings );

	// Automatically start the splitscreen session, no configuration required
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		if ( bImmediateStart )
		{
			pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
		}
	}
}

CON_COMMAND_F( ui_reloadscheme, "Reloads the resource files for the active UI window", 0 )
{
	g_pFullFileSystem->SyncDvdDevCache();
	CUIGameData::Get()->ReloadScheme();
}

void CUIGameData::ReloadScheme()
{
	CBaseModPanel::GetSingleton().ReloadScheme();
	CBaseModFrame *window = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	if( window )
	{
		window->ReloadSettings();
	}
	CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
	if( footer )
	{
		footer->ReloadSettings();
	}
}

CBaseModFrame * CUIGameData::GetParentWindowForSystemMessageBox()
{
	WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	WINDOW_PRIORITY wPriority = CBaseModPanel::GetSingleton().GetActiveWindowPriority();

	CBaseModFrame *pCandidate = CBaseModPanel::GetSingleton().GetWindow( wtActive );

	if ( pCandidate )
	{
		if ( wPriority >= WPRI_WAITSCREEN && wPriority <= WPRI_MESSAGE )
		{
			if ( UI_IsDebug() )
			{
				DevMsg( "GetParentWindowForSystemMessageBox: using navback of %s\n", pCandidate->GetName() );
			}

			// Message would not be able to nav back to waitscreen or another message
			pCandidate = pCandidate->GetNavBack();
		}
		else if ( wPriority > WPRI_MESSAGE )
		{
			if ( UI_IsDebug() )
			{
				DevMsg( "GetParentWindowForSystemMessageBox: using NULL since a higher priority window is open %s\n", pCandidate->GetName() );
			}

			// Message would not be able to nav back to a higher level priority window
			pCandidate = NULL;
		}
	}

	return pCandidate;
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


static const char* GetDLCSessionErrorMessage( int iDlcRequired )
{
	static char szErrorTitleBuffer[128];
	Q_snprintf( szErrorTitleBuffer, sizeof( szErrorTitleBuffer ), "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
	return szErrorTitleBuffer;
}


void CUIGameData::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( g_pGameSteamCloudSync )
		g_pGameSteamCloudSync->OnEvent( pEvent );

	if ( !Q_stricmp( "OnSysXUIEvent", szEvent ) )
	{
		m_bXUIOpen = !Q_stricmp( "opening", pEvent->GetString( "action", "" ) );
	}
	else if ( !Q_stricmp( "OnProfileUnavailable", szEvent ) )
	{
#if defined( _DEMO )
		return;
#endif
		// Activate game ui to see the dialog
		if ( !CBaseModPanel::GetSingleton().IsVisible() )
		{
			engine->ExecuteClientCmd( "gameui_activate" );
		}

		// Pop a message dialog if their storage device was changed
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			GetParentWindowForSystemMessageBox(), false ) );
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MsgBx_AchievementNotWrittenTitle";
		data.pMessageText = "#L4D360UI_MsgBx_AchievementNotWritten"; 
		data.bOkButtonEnabled = true;

		confirmation->SetUsageData( data );
	}
	else if ( !Q_stricmp( "OnInvite", szEvent ) )
	{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
		// Close the EconUI
		EconUI()->CloseEconUI();
#endif

		// Check if the user just accepted invite
		if ( !Q_stricmp( "accepted", pEvent->GetString( "action" ) ) )
		{
			int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" );
#if defined( _PS3 ) && !defined( NO_STEAM )
			g_uiConnectionToSteamToJoinLobbyId = pEvent->GetUint64( "sessionid" );
#endif

#if defined (PORTAL2_PUZZLEMAKER)
			if( g_pPuzzleMaker->GetActive() )
			{
				// Not confirmed just yet!
				if ( pnConfirmed )
					*pnConfirmed = 0;

				Invite_Confirm();

				return;
			}
#endif

			// Check if we have an outstanding session
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( !pIMatchSession )
			{
				if ( !Invite_Connecting() && pnConfirmed )
					*pnConfirmed = 0;
				return;
			}

			// In Portal2 we don't care to warn a user if he is in a lobby (Portal2 lobby state is meaningless really)
			if ( !Q_stricmp( pIMatchSession->GetSessionSettings()->GetString( "game/state" ), "lobby" ) )
			{
				if ( !Invite_Connecting() && pnConfirmed )
					*pnConfirmed = 0;
				return;
			}

			// User is accepting an invite and has an outstanding
			// session, TCR requires confirmation of destructive actions
			if ( pnConfirmed )
				*pnConfirmed = 0;

			// Show the dialog
			Invite_Confirm();
		}
		else if ( !Q_stricmp( "storage", pEvent->GetString( "action" ) ) )
		{
			if ( ( CUIGameData::Get()->SelectStorageDevicePolicy() & STORAGE_DEVICE_NEED_INVITE ) &&
				!Invite_IsStorageDeviceValid() )
			{
				if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
				{
					*pnConfirmed = 0;	// make the invite accepting code wait
				}
			}
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
			GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				GetParentWindowForSystemMessageBox(), false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#InviteErrorTitle";
			data.pMessageText = szReason;
			data.bOkButtonEnabled = true;

			confirmation->SetUsageData(data);
		}
	}
	else if ( !Q_stricmp( "OnSysStorageDevicesChanged", szEvent ) )
	{
#if defined( _DEMO )
		return;
#endif

		// If a storage device change is in progress, the simply ignore
		// the notification callback, but pop the dialog
		if ( m_pSelectStorageClient )
		{
			DevWarning( "Ignored OnSysStorageDevicesChanged while the storage selection was in progress...\n" );
		}

		// Activate game ui to see the dialog
		if ( !CBaseModPanel::GetSingleton().IsVisible() )
		{
			engine->ExecuteClientCmd( "gameui_activate" );
		}

		// Pop a message dialog if their storage device was changed
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			GetParentWindowForSystemMessageBox(), false ) );
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#GameUI_Console_StorageRemovedTitle";
		data.pMessageText = "#L4D360UI_MsgBx_StorageDeviceRemoved"; 
		data.bOkButtonEnabled = true;
		
		data.pfnOkCallback = m_pSelectStorageClient ? NULL : &OnStorageDevicesChangedSelectNewDevice;	// No callback if already in the middle of selecting a storage device

		confirmation->SetUsageData( data );
	}
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
		if ( CBaseModPanel::GetSingleton().IsVisible() || nInactivePlayers == XBX_GetNumGameUsers() )
		{
			if ( !CBaseModPanel::GetSingleton().IsVisible() )
			{
				engine->ExecuteClientCmd( "gameui_activate" );
			}

			// Pop a message if a valid controller was removed!
			GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				GetParentWindowForSystemMessageBox(), false ) );
			
			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_MsgBx_ControllerUnpluggedTitle";
			data.pMessageText = "#L4D360UI_MsgBx_ControllerUnplugged";
			data.bOkButtonEnabled = true;

			confirmation->SetUsageData(data);
		}
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
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		GenericConfirmation* confirmation = bShowDisconnectedMsgBox ? static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) ) : NULL;
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL );

		if ( confirmation )
		{
			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MsgBx_SignInChangeC";
			data.pMessageText = "#L4D360UI_MsgBx_SignInChange";
			data.bOkButtonEnabled = true;

			if ( !Q_stricmp( szReason, "GuestSignedIn" ) )
			{
				data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";	// "Disconnect"
				data.pMessageText = "#L4D360UI_MsgBx_SignInChange";				// "Sign-in change has occured."
			}

			confirmation->SetUsageData(data);

#ifdef _GAMECONSOLE
			// When a confirmation shows up it prevents attract screen from opening, so reset user slots here:
			XBX_ResetUserIdSlots();
			XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
			XBX_SetPrimaryUserIsGuest( 0 );	
			XBX_SetNumGameUsers( 0 ); // users not selected yet
#endif
		}
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
			{ "Connection to server timed out", "#L4D360UI_MsgBx_DisconnectedFromServer", RemapText_t::MATCH_SUBSTR },
			{ "Added to banned list", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "Kicked and banned", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "You have been voted off", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "All players idle", "#L4D_ServerShutdownIdle", RemapText_t::MATCH_SUBSTR },
			{ "Partner disconnected", "#SessionError_NoPartner", RemapText_t::MATCH_SUBSTR },
#ifndef _GAMECONSOLE
			{ "Connection failed after ", "#SessionError_ConnectionFailedAfter", RemapText_t::MATCH_SUBSTR },
			{ "Server shutting down", "#SessionError_ServerShuttingDown", RemapText_t::MATCH_SUBSTR },
#endif
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
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Open front screen
		CBaseModPanel::GetSingleton().OpenFrontScreen( true );
#ifdef _GAMECONSOLE
		CBaseModFrame* pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN );
#else
		CBaseModFrame* pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU );
#endif
		// Show the message box
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pFrame, true ) );


		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";	// "Disconnect"
		data.pMessageText = szReason;
		data.bOkButtonEnabled = true;

#if defined( _PS3 ) && !defined( NO_STEAM )
		if ( !Q_strcmp( "#SessionError_NoPartner", data.pMessageText ) ||
			!Q_strcmp( "#DisconnectReason_Unknown", data.pMessageText ) ||
			!Q_strcmp( "#DisconnectReason_LostConnectionToLIVE", data.pMessageText ) ||
			!Q_strcmp( "#SessionError_ConnectionFailedAfter", data.pMessageText ) ||
			!Q_strcmp( "#SessionError_ServerShuttingDown", data.pMessageText ) ||
			!Q_strcmp( "#L4D360UI_MsgBx_DisconnectedFromServer", data.pMessageText )
			)
		{
			if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
				data.pMessageText = "#SessionError_PSN";
			if ( !CUIGameData::Get()->IsNetworkCableConnected() )
				data.pMessageText = "#SessionError_EthernetCable";
		}
#endif

		confirmation->SetUsageData(data);
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
			char const *szError = pEvent->GetString( "error", "" );
			char const *szErrorTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";

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
				{ "nopuzzle", "#PORTAL2_QuickPlay_NoPuzzles", RemapText_t::MATCH_FULL },
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
			CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

			// Open front screen
			CBaseModPanel::GetSingleton().OpenFrontScreen( true );
#ifdef _GAMECONSOLE
			CBaseModFrame* pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN );
#else
			CBaseModFrame* pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU );
#endif
			// Show the message box
			GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pFrame, true ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = szErrorTitle;
			data.pMessageText = szError;
			data.bOkButtonEnabled = true;

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
				uint64 uiDlcOfferID = kvDlcDetails->GetUint64( CFmtStr( "dlc%d/offerid", iDlcRequired ) );
				
				// Format the strings
				bool bKicked = !Q_stricmp( pEvent->GetString( "action" ), "kicked" );
				wchar_t const *wszLine1 = g_pVGuiLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", bKicked ? "Kicked" : "Join", iDlcRequired ) );
				wchar_t const *wszLine2 = g_pVGuiLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", uiDlcOfferID ? "Offer" : "Message", iDlcRequired ) );
				
				int numBytesTwoLines = ( Q_wcslen( wszLine1 ) + Q_wcslen( wszLine2 ) + 4 ) * sizeof( wchar_t );
				wchar_t *pwszTwoLines = ( wchar_t * ) stackalloc( numBytesTwoLines );
				Q_snwprintf( pwszTwoLines, numBytesTwoLines, PRI_WS_FOR_WS PRI_WS_FOR_WS, wszLine1, wszLine2 );
				data.pMessageTextW = pwszTwoLines;
				data.pMessageText = NULL;

				data.pWindowTitle = GetDLCSessionErrorMessage( iDlcRequired );

				if ( uiDlcOfferID )
				{
					data.bCancelButtonEnabled = true;
					data.pfnOkCallback = GoToMarketplaceForOffer;
					
					g_MarketplaceEntryPoint.uiOfferID = uiDlcOfferID;
					g_MarketplaceEntryPoint.dwEntryPoint = kvDlcDetails->GetInt( CFmtStr( "dlc%d/type", iDlcRequired ) );
				}
			}

#if defined( _PS3 ) && !defined( NO_STEAM )
			if ( data.pMessageText && ( !Q_strcmp( "#SessionError_SteamServersDisconnected", data.pMessageText ) ||
					!Q_strcmp( "#SessionError_Create", data.pMessageText ) ||
					!Q_strcmp( "#SessionError_NotAvailable", data.pMessageText ) ||
					!Q_strcmp( "#SessionError_Connect", data.pMessageText )
				) )
			{
				if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
				{
					data.pMessageTextW = NULL;
					data.pMessageText = "#SessionError_PSN";
				}
				if ( !CUIGameData::Get()->IsNetworkCableConnected() )
				{
					data.pMessageTextW = NULL;
					data.pMessageText = "#SessionError_EthernetCable";
				}
			}
#endif
			
			confirmation->SetUsageData(data);
		}
	}
	else if ( !Q_stricmp( "OnAttractModeWaitNotification", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state" );
		if ( IsX360() && !Q_stricmp( szState, "splitscreen" ) )
		{
			InitiateSplitscreenPlay();
			return;
		}
		Q_strncpy( m_chNotificationMode, szState, sizeof( m_chNotificationMode ) );
	}
	else if ( !Q_stricmp( "OnProfileDataLoaded", szEvent ) )
	{
		if ( !Q_stricmp( m_chNotificationMode, "splitscreen" ) )
		{
			m_chNotificationMode[0] = 0;
			InitiateSplitscreenPlay();
		}
	}
	else if ( !Q_stricmp( "OnHostChangeLevel", szEvent ) )
	{
		if ( IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession() )
		{
			const char* szGameMode = pIMatchSession->GetSessionSettings()->GetString( "game/mode" );
			if ( !V_stricmp( "coop", szGameMode ) || !V_stricmp( "coop_challenge", szGameMode ) || !V_stricmp( "coop_community", szGameMode ) )
				return; // don't submit changelevel stats in coop, coop submits "session" stats
		}

		GameStats_ReportAction( engine->IsInCommentaryMode() ? "changelevelcomm" : "changelevel",
			pEvent->GetString( "map" ), pEvent->GetUint64( "elapsed" ) );
	}
	else if ( !Q_stricmp( "OnRequestMapRating", szEvent ) )
	{
#if defined( PORTAL2_PUZZLEMAKER )
		// If we have a valid active map, then pop the UI up immediately
		if ( CBaseModPanel::GetSingleton().GetCurrentCommunityMapID() != 0 &&
			 ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_USER_QUEUE || BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_QUICK_PLAY ) )
		{
			// Activate game ui to see the dialog
			if ( !CBaseModPanel::GetSingleton().IsVisible() )
			{
				engine->ExecuteClientCmd( "gameui_activate" );
			}

			// Show our rating box
			m_bPendingMapVoteRequest = true;
		}
#endif // PORTAL2_PUZZLEMAKER
	}
	else if ( !Q_stricmp( "OnCloseMapRating", szEvent ) )
	{
		// HACK: This will force down the in-game menu if called via script
		GameUI().HideGameUI();
	}
}

void CUIGameData::GetDownloadableContent( char const *szContent )
{
	// Try to figure out if this DLC is paid/free/unknown
	KeyValues *kvDlcDetails = new KeyValues( "" );
	KeyValues::AutoDelete autodelete_kvDlcDetails( kvDlcDetails );
	if ( !kvDlcDetails->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/dlcdetailsinfo.res", "MOD" ) )
		kvDlcDetails = NULL;

	// Determine the DLC offer ID
	uint64 uiDlcOfferID = kvDlcDetails->FindKey( szContent )->GetUint64( "offerid" );

	if ( uiDlcOfferID )
	{
		g_MarketplaceEntryPoint.uiOfferID = uiDlcOfferID;
		g_MarketplaceEntryPoint.dwEntryPoint = kvDlcDetails->FindKey( szContent )->GetInt( "type" );
		GoToMarketplaceForOffer();
	}
	else
	{
		Warning( "CUIGameData::GetDownloadableContent failed to locate content description for `%s`\n", szContent );
	}
}


//=============================================================================
void CUIGameData::RunFrame()
{
	RunFrame_Storage();

	RunFrame_Invite();

#if !defined( NO_STEAM ) && defined( _PS3 )
	if ( g_nConnectionToSteamReasonResetMsgId )
	{
		// Check that the confirmation wasn't dismissed without notifying the invite system
		GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
		if ( !confirmation ||
			confirmation->GetUsageId() != g_nConnectionToSteamReasonResetMsgId )
		{
			// Need to reset steam connection reason
			ResetConnectToSteamReason();
		}
	}
#endif

	if ( m_bPendingMapVoteRequest )
	{
		// CBaseModFrame *pFrame = CBaseModPanel::GetSingleton().OpenWindow( WT_RATEMAP, NULL );
		CBaseModFrame *pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU );
		if ( pFrame )
		{
			// HACK: Hide the game console
			if ( IsPC() )
			{
				GameConsole().Hide();	
			}

			KeyValues *pKVRatingRequest = new KeyValues( "MsgRequestMapRating" );
			pFrame->PostMessage( pFrame, pKVRatingRequest );
			m_bPendingMapVoteRequest = false;
		}
	}

	if ( m_bNeedUpdateMatchMutelist )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSysMuteListChanged" ) );
		m_bNeedUpdateMatchMutelist = false;
	}

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
			if ( !m_bShowConnectionProblemActive &&
				!CBaseModPanel::GetSingleton().IsVisible() )
			{
				GameUI().ActivateGameUI();
				OpenWaitScreen( "#GameUI_RetryingConnectionToServer", 0.0f );
				m_bShowConnectionProblemActive = true;
			}
		}
		else
		{
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
		}
	}
}




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

	IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
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


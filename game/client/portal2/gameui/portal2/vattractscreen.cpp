//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VAttractScreen.h"
#include "VSignInDialog.h"
#include "EngineInterface.h"
#include "inputsystem/iinputsystem.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/imagepanel.h"
#include "VGenericConfirmation.h"
#include "VFooterPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "gameui_util.h"
#include "vmainmenu.h"
#include "tier0/icommandline.h"
#include "filesystem.h"
#include "bitmap/tgaloader.h"
#include "filesystem/IXboxInstaller.h"
#ifdef _X360
#include "xbox/xbox_launch.h"
#elif defined(_PS3)
#include "ps3/saverestore_ps3_api_ui.h"
#include "sysutil/sysutil_savedata.h"
#include "sysutil/sysutil_gamecontent.h"
#include "cell/sysmodule.h"
static int s_nPs3SaveStorageSizeKB = 21*1024;
static int s_nPs3TrophyStorageSizeKB = 0;
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "steamoverlay/isteamoverlaymgr.h"
#include "steamcloudsync.h"
#include "transitionpanel.h"
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern IVEngineClient *engine;

#if defined( _GAMECONSOLE ) && !defined( _CERT )
#define IMAGEVIEWER_ENABLED
#endif

#ifdef IMAGEVIEWER_ENABLED
ConVar ui_imageviewer_dir( "ui_imageviewer_dir", "", FCVAR_DEVELOPMENTONLY );
CUtlVector< char * > g_arrImageViewerFiles;
int g_ImageViewerFileIndex = 0, g_ImageViewerReloadImage = 0, g_ImageViewerReloadImageDir = 1;
int g_ImageViewerTextureId = -1, g_ImageViewerTextureSize[2];
void ImageViewerAdvanceIndex()
{
	if ( !g_arrImageViewerFiles.Count() ) return;
	g_ImageViewerFileIndex += g_ImageViewerReloadImageDir;
	g_ImageViewerFileIndex %= g_arrImageViewerFiles.Count();
	g_ImageViewerFileIndex += g_arrImageViewerFiles.Count();
	g_ImageViewerFileIndex %= g_arrImageViewerFiles.Count();
	g_ImageViewerReloadImage = 1;
}
int ImageViewerSortList( char * const *a, char * const *b )
{
	return Q_stricmp( *a, *b );
}
#endif

ConVar ui_sp_map_default( "ui_sp_map_default", "", FCVAR_DEVELOPMENTONLY );
ConVar ui_coop_map_default( "ui_coop_map_default", "", FCVAR_DEVELOPMENTONLY );
ConVar ui_coop_ss_fadeindelay( "ui_coop_ss_fadeindelay", IsPS3() ? "1.5" : "1", FCVAR_DEVELOPMENTONLY );

//
//	Primary user id is the one who attract screen uses for
//	signing in. All other systems will see no primary user
//	until attract screen transitions into the main menu.
//
static int s_idPrimaryUser = -1;
static int s_idSecondaryUser = -1;
static int s_bSecondaryUserIsGuest = 0;
static int s_eStorageUI = 0;

static int s_iAttractModeRequestCtrlr = -1;
static int s_iAttractModeRequestPriCtrlr = -1;
static CAttractScreen::AttractMode_t s_eAttractMode = CAttractScreen::ATTRACT_GAMESTART;

#ifdef _PS3
static CPS3SaveRestoreAsyncStatus s_PS3SaveAsyncStatus;
enum SaveInitializeState_t
{
	SIS_DEFAULT,
	SIS_INIT_REQUESTED,
	SIS_FINISHED
};
static SaveInitializeState_t s_ePS3SaveInitState;
#endif

void CAttractScreen::SetAttractMode( AttractMode_t eMode, int iCtrlr )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::SetAttractMode(%d)\n", eMode );
	}

	s_eAttractMode = eMode;
	
#ifdef _GAMECONSOLE
	if ( !XBX_GetNumGameUsers() )
		return;

	if ( iCtrlr >= 0 )
	{
		s_iAttractModeRequestCtrlr = iCtrlr;
		s_iAttractModeRequestPriCtrlr = XBX_GetPrimaryUserId();
		Msg( "[GAMEUI] CAttractScreen::SetAttractMode(%d) bookmarked ctrlr%d (primary %d)\n", eMode, s_iAttractModeRequestCtrlr, s_iAttractModeRequestPriCtrlr );
	}
#endif
}

bool IsUserSignedInProperly( int iCtrlr )
{
#ifdef _X360
	XUSER_SIGNIN_INFO xsi;
	if ( iCtrlr >= 0 && iCtrlr < XUSER_MAX_COUNT &&
		XUserGetSigninState( iCtrlr ) != eXUserSigninState_NotSignedIn &&
		ERROR_SUCCESS == XUserGetSigninInfo( iCtrlr, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
		!(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) )
		return true;
	else
		return false;
#else
	return true;
#endif
}

bool IsUserSignedInToLiveWithMultiplayer( int iCtrlr )
{
#ifdef _X360
	BOOL bPriv = FALSE;
	return IsUserSignedInProperly( iCtrlr ) &&
		( eXUserSigninState_SignedInToLive == XUserGetSigninState( iCtrlr ) ) &&
		( ERROR_SUCCESS == XUserCheckPrivilege( iCtrlr, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPriv ) ) &&
		bPriv;
#else
	return true;
#endif
}

static bool IsPrimaryUserSignedInProperly()
{
	return IsUserSignedInProperly( s_idPrimaryUser );
}

static void ChangeGamers();

bool CAttractScreen::IsUserIdleForAttractMode()
{
	BladeStatus_t bladeStatus = GetBladeStatus();
	if ( bladeStatus != BLADE_NOTWAITING )
	{
		// there is blade activity
		return false;
	}		

	if ( m_bHidePressStart )
	{
		// attract screen is in ghost mode
		return false;
	}

	if ( m_pPressStartlbl && !m_pPressStartlbl->IsVisible() )
	{
		// something is going on and "Press START" is not flashing
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// Device selector implementation
//
//////////////////////////////////////////////////////////////////////////

class CAttractScreenDeviceSelector : public ISelectStorageDeviceClient
{
public:
	CAttractScreenDeviceSelector( int iCtrlr, bool bForce, bool bAllowDeclined ) :
	  m_iCtrlr( iCtrlr ), m_bForce( bForce ), m_bAllowDeclined( bAllowDeclined ) {}
public:
	virtual int  GetCtrlrIndex() { return m_iCtrlr; }
	virtual bool ForceSelector() { return m_bForce; }
	virtual bool AllowDeclined() { return m_bAllowDeclined; }
	virtual bool AllowAnyController() { return false; }

	virtual void OnDeviceFail( FailReason_t eReason );

	virtual void OnDeviceSelected();		// After device has been picked in XUI blade, but before mounting symbolic roots and opening containers
	virtual void AfterDeviceMounted();		// After device has been successfully mounted, configs processed, etc.

public:
	int m_iCtrlr;
	bool m_bForce;
	bool m_bAllowDeclined;
};

void CAttractScreenDeviceSelector::OnDeviceFail( FailReason_t eReason )
{
	XBX_SetStorageDeviceId( GetCtrlrIndex(), XBX_STORAGE_DECLINED );

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );
	
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->ReportDeviceFail( eReason );
	}
	else
	{
		ChangeGamers();
	}
	delete this;
}

void CAttractScreenDeviceSelector::OnDeviceSelected()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StartGame_Storage_Selected - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	CUIGameData::Get()->UpdateWaitPanel( "#L4D360UI_WaitScreen_SignOnSucceded" );
}

void CAttractScreenDeviceSelector::AfterDeviceMounted()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StartGame_Storage_Loaded - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );

	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		switch ( s_eStorageUI )
		{
		case 1:
			attractScreen->StartGame_Stage2_Storage2();
			return;
		case 2:
			attractScreen->StartGame_Stage3_Ready();
			return;
		default:
			ChangeGamers();
			return;
		}
	}
	else
	{
		ChangeGamers();
	}
	delete this;
}

//////////////////////////////////////////////////////////////////////////
//
// Attract screen implementation
//
//////////////////////////////////////////////////////////////////////////

CAttractScreen::CAttractScreen( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true ),
	m_bGameBootReady( false )
{
	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	SetDeleteSelfOnClose( true );

	SetFooterEnabled( true );

	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );

	m_pPressStartlbl = NULL;
	m_bHidePressStart = false;
	m_msgData = 0;
	m_pfnMsgChanged = NULL;
	m_bladeStatus = BLADE_NOTWAITING;
	m_flFadeStart = 0;

	m_eSignInUI = SIGNIN_NONE;
	m_eStorageUI = STORAGE_NONE;

	// Subscribe for the events
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnAttractModeWaitNotification", "state", "" ) );
}

CAttractScreen::~CAttractScreen()
{
	// Unsubscribe for the events
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	RemoveFrameListener( this );
}

void CAttractScreen::RunFrame()
{
	if ( !IsVisible() )
	{
		// This is to handle the case when a generic waitscreen
		// is visible and hides attract screen,
		// attract screen needs to have logic running every frame
		// to take down the generic wait screen.
		// Don't run OnThink twice when we are visible.
		OnThink();
	}
}

void CAttractScreen::OnThink()
{
	// If the required message changed, call the requested callback
	// this is required because generic confirmations drive the signon
	// state machine and if another confirmation interferes the signon
	// machine can be aborted and leave UI in undetermined state
	if ( m_pfnMsgChanged )
	{
		GenericConfirmation* pMsg = ( GenericConfirmation* ) CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION );
		if ( !pMsg || pMsg->GetUsageId() != m_msgData )
		{
			void (*pfn)() = m_pfnMsgChanged;
			m_pfnMsgChanged = NULL;

			(*pfn)();
		}
	}

	// image and labels fade in 
	// press start fade in
	float flFade = 0;
	if ( !m_flFadeStart )
	{
		// start fade in as soon as overlay has revealed menu
		if ( !CBaseModPanel::GetSingleton().IsOpaqueOverlayActive() )
		{
			m_flFadeStart = Plat_FloatTime() + TRANSITION_FROM_OVERLAY_DELAY_TIME;
		}
	}
	if ( m_flFadeStart )
	{
		flFade = RemapValClamped( Plat_FloatTime(), m_flFadeStart, m_flFadeStart + TRANSITION_OVERLAY_FADE_TIME, 0.0f, 1.0f );
	}

	if ( m_pPressStartlbl )
	{
		int alpha = flFade * ( 80.0f + 50.0f * sin( Plat_FloatTime() * 4.0f ) );
		m_pPressStartlbl->SetAlpha( alpha );
	}

#ifdef _PS3
	if ( ( s_ePS3SaveInitState == SIS_INIT_REQUESTED ) && s_PS3SaveAsyncStatus.JobDone() )
	{
		s_PS3SaveAsyncStatus.m_bDone = false;
		OnGameBootSaveContainerReady();
	}
#endif

	BladeStatus_t bladeStatus = GetBladeStatus();
	switch ( bladeStatus )
	{
	case BLADE_WAITINGFOROPEN:
		{
			HidePressStart();
			if( CUIGameData::Get()->IsXUIOpen() )
			{
				SetBladeStatus( BLADE_WAITINGFORCLOSE );				
			}
		}
		break;

	case BLADE_WAITINGFORCLOSE:
		{
			HidePressStart();
			if( !CUIGameData::Get()->IsXUIOpen() )
			{
				m_flWaitStart = Plat_FloatTime();
				SetBladeStatus( BLADE_WAITINGFORSIGNIN );
			}
		}
		break;

	case BLADE_WAITINGFORSIGNIN:
		{
			if ( Plat_FloatTime() - m_flWaitStart > 0.1f )
			{
				SetBladeStatus( BLADE_NOTWAITING );

				if ( m_eSignInUI == SIGNIN_PROMOTETOGUEST )
				{
					// The user took down the blade without promoting an account to guest
					// start the game with the previously selected gamers if both of
					// them are properly signed in
					if ( IsUserSignedInProperly( s_idPrimaryUser ) && IsUserSignedInProperly( s_idSecondaryUser ) )
						StartGame( s_idPrimaryUser, s_idSecondaryUser );
					else
						SetBladeStatus( BLADE_WAITINGTOSHOWSIGNIN2 );
				}
				else
				{
					ShowPressStart();
				}

				m_eSignInUI = SIGNIN_NONE;
			}
		}
		break;

	case BLADE_NOTWAITING: //not waiting
		if ( CUIGameData::Get()->IsXUIOpen() )
		{
			// Check if our confirmation is up
			if ( GenericConfirmation* pMsg = ( GenericConfirmation* ) CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) )
			{
				if ( pMsg->GetUsageId() == m_msgData )
				{
					m_flWaitStart = Plat_FloatTime();
					SetBladeStatus( BLADE_TAKEDOWNCONFIRMATION );
				}
			}
		}
		else if ( s_eAttractMode == ATTRACT_GAMESTART )
		{
		}

		if ( s_eAttractMode == ATTRACT_GOSPLITSCREEN )
		{
			s_idPrimaryUser = s_iAttractModeRequestPriCtrlr;
			s_idSecondaryUser = s_iAttractModeRequestCtrlr;
			s_iAttractModeRequestCtrlr = -1;
			SetBladeStatus( BLADE_WAITINGTOSHOWSIGNIN2 );
		}
		if ( s_eAttractMode == ATTRACT_GOSINGLESCREEN )
		{
			SetAttractMode( ATTRACT_GAMESTART );
			ShowSignInDialog( s_iAttractModeRequestCtrlr, -1 );
			s_iAttractModeRequestCtrlr = -1;
		}
		if ( s_eAttractMode == ATTRACT_GUESTSIGNIN )
		{
			SetAttractMode( ATTRACT_GAMESTART );
			ShowSignInDialog( s_iAttractModeRequestCtrlr, -1, SIGNIN_SINGLE );
			s_iAttractModeRequestCtrlr = -1;
		}
		if ( s_eAttractMode == ATTRACT_ACCEPTINVITE )
		{
			SetAttractMode( ATTRACT_GAMESTART );
		}
		break;

	case BLADE_TAKEDOWNCONFIRMATION:
		if ( CUIGameData::Get()->IsXUIOpen() )
		{
			// Check if our confirmation is up
			bool bShouldWaitLonger = false;
			if ( GenericConfirmation* pMsg = ( GenericConfirmation* ) CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) )
			{
				if ( pMsg->GetUsageId() == m_msgData )
				{
					if ( Plat_FloatTime() - m_flWaitStart > 1.0f )
					{
						if ( UI_IsDebug() )
						{
							Msg( "[GAMEUI] TakeDownConfirmation( %s )\n", pMsg->GetName() );
						}
						
						ShowPressStart();
						pMsg->Close();
					}
					else
					{
						bShouldWaitLonger = true;
					}
				}
			}

			if ( !bShouldWaitLonger )
			{
				SetBladeStatus( BLADE_NOTWAITING );
			}
		}
		else
		{
			SetBladeStatus( BLADE_NOTWAITING );
		}
		break;

	case BLADE_WAITINGTOSHOWPROMOTEUI:
#ifdef _X360
		if ( !CUIGameData::Get()->IsXUIOpen() )
		{
			SetBladeStatus( BLADE_WAITINGFOROPEN );
			m_eSignInUI = SIGNIN_PROMOTETOGUEST;
			XShowSigninUI( 1, XSSUI_FLAGS_CONVERTOFFLINETOGUEST );
		}
#else
		Assert( 0 );
#endif
		break;

	case BLADE_WAITINGTOSHOWSTORAGESELECTUI:
#ifdef _GAMECONSOLE
		if ( !CUIGameData::Get()->IsXUIOpen() )
		{
			SetBladeStatus( BLADE_NOTWAITING );
			ShowStorageDeviceSelectUI();
		}
#endif
		break;

	case BLADE_WAITINGTOSHOWSIGNIN2:
		if ( IsPS3() || !CUIGameData::Get()->IsXUIOpen() )
		{
			SetAttractMode( ATTRACT_GAMESTART );
			SetBladeStatus( BLADE_NOTWAITING );
			ShowSignInDialog( s_idPrimaryUser, s_idSecondaryUser, SIGNIN_DOUBLE );
		}
		break;

	default:
		break;
	}

	BaseClass::OnThink();
}

void CAttractScreen::OnKeyCodePressed( KeyCode code )
{
	BaseModUI::CBaseModPanel::GetSingleton().ResetAttractDemoTimeout();

	if ( m_bHidePressStart || !m_bGameBootReady )
	{
		// Cannot process button presses if PRESS START is not available
		return;
	}

	int userId = GetJoystickForCode( code );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_XBUTTON_START:
		if ( s_eAttractMode == ATTRACT_GAMESTART )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
			ShowSignInDialog( userId, -1 );
		}	
		break;

#ifdef IMAGEVIEWER_ENABLED
	case KEY_XBUTTON_BACK:
		if ( ui_imageviewer_dir.GetString()[0] )
		{
			// Enter image viewer mode
			CFmtStr keySearch( "%s/*.tga", ui_imageviewer_dir.GetString() );
			V_FixSlashes( keySearch.Access() );

			// iterate the images
			g_arrImageViewerFiles.PurgeAndDeleteElements();
			FileFindHandle_t handle;
			for ( const char* pfileName = g_pFullFileSystem->FindFirst( keySearch.Access(), &handle );
				pfileName; pfileName = g_pFullFileSystem->FindNext( handle ) )
			{
				if ( Q_strlen( pfileName ) <= 4 ) continue;
				CFmtStr keyFile( "%s/%s", ui_imageviewer_dir.GetString(), pfileName );
				V_FixSlashes( keyFile.Access() );

				int nLen = Q_strlen( keyFile.Access() );
				char *szImgName = new char[ 1 + nLen ];
				Q_memcpy( szImgName, keyFile.Access(), 1 + nLen );
				g_arrImageViewerFiles.AddToTail( szImgName );
			}
			g_pFullFileSystem->FindClose( handle );
			g_ImageViewerFileIndex = 0;
			g_ImageViewerReloadImage = 1;
			g_ImageViewerReloadImageDir = 1;
			if ( g_arrImageViewerFiles.Count() )
			{
				HidePressStart();
				g_arrImageViewerFiles.Sort( ImageViewerSortList );
			}
		}
		break;

	case KEY_XBUTTON_LEFT_SHOULDER:
	case KEY_XBUTTON_RIGHT_SHOULDER:
		if ( g_arrImageViewerFiles.Count() )
		{
			g_ImageViewerReloadImageDir = ((GetBaseButtonCode( code ) == KEY_XBUTTON_RIGHT_SHOULDER)?1:-1);
			ImageViewerAdvanceIndex();
		}
		break;
#endif

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

bool CAttractScreen::BypassAttractScreen()
{
	if ( IsPS3() )
		return false; // cannot bypass on PS3 due to trophy install etc.

	// Attract screen can only be bypassed once
	static bool s_bBypassOnce = false;
	if ( s_bBypassOnce )
		return false;
	s_bBypassOnce = true;

	// Check if command line allows to bypass
	if ( !CommandLine()->FindParm( "-noattractscreen" ) )
		return false;

#ifdef _GAMECONSOLE
	// Check if there's a user signed in properly
	for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
	{
		if ( !IsUserSignedInProperly( k ) )
			continue;

		m_bHidePressStart = true;
		HidePressStart();

		XBX_SetStorageDeviceId( k, XBX_STORAGE_DECLINED );

		StartGame( k );
		return true;
	}
#endif

	// Couldn't find a user to be signed in
	return false;
}

void CAttractScreen::AcceptInvite()
{
#ifdef _X360
	static bool s_bAcceptedOnce = false;
	if ( s_bAcceptedOnce )
		return;
	s_bAcceptedOnce = true;

	int iInvitedUser = XBX_GetInvitedUserId();
	XUID iInvitedXuid = XBX_GetInvitedUserXuid();
	XNKID nSessionID = XBX_GetInviteSessionId();

	Msg( "[GAMEUI] Invite for user %d (%llx) session id: %llx\n", iInvitedUser, iInvitedXuid, ( uint64 const & ) nSessionID );

	if ( !( ( const uint64 & )( nSessionID ) ) ||
		( iInvitedUser < 0 || iInvitedUser >= XUSER_MAX_COUNT ) ||
		!iInvitedXuid )
	{
		return;
	}

	if ( !IsUserSignedInProperly( iInvitedUser ) )
	{
		Warning( "[GAMEUI] User no longer signed in\n" );
		return;
	}

	XUSER_SIGNIN_INFO xsi;
	if ( ERROR_SUCCESS != XUserGetSigninInfo( iInvitedUser, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) ||
		xsi.xuid != iInvitedXuid )
	{
		Warning( "[GAMEUI] Failed to match user xuid with invite\n" );
		return;
	}

	//
	// Proceed accepting the invite
	//
	m_bHidePressStart = true;

	// Need to fire off a game state change event to
	// all listeners
	XBX_SetInvitedUserId( iInvitedUser );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", (int) XBX_GetNumGameUsers() ) );

	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		" system { "
			" network LIVE "
		" } "
		" options { "
			" action joininvitesession "
		" } "
		);
	pSettings->SetUint64( "options/sessionid", * ( uint64 * ) &nSessionID );
	KeyValues::AutoDelete autodelete( pSettings );

	Msg( "[GAMEUI] CAttractScreen - invite restart, invite accepted!\n" );
	g_pMatchFramework->MatchSession( pSettings );
#endif
}

void CAttractScreen::PerformGameBootWork()
{
#ifdef _PS3
	static bool s_bBootOnce = false;
	if ( s_bBootOnce )
	{
		m_bGameBootReady = true;
		return;
	}
	s_bBootOnce = true;
	m_bHidePressStart = true; // don't show Press START

	// Start prefetching sounds early:
	bool bSucceeded;
	bSucceeded = g_pFullFileSystem->PrefetchFile( "/portal2/zip2.ps3.zip", 0, true );	// Average priority, but persistent.
	Assert( bSucceeded );		// Is the path incorrect?

	// Install PS3 trophies
	this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "#L4D360UI_Boot_Trophies" ) );
	m_CallbackOnPS3TrophiesInstalled.Register( this, &CAttractScreen::Steam_OnPS3TrophiesInstalled );
	steamapicontext->SteamUserStats()->InstallPS3Trophies();	
#else
	m_bGameBootReady = true;
#endif
}

#ifdef _PS3
void CAttractScreen::ShowFatalError( uint32 unSize )
{
	if ( unSize > 0 )
	{
		int nKbRequired = int( (unSize + 1023) / 1024 );
		int nMbRequired = AlignValue( nKbRequired, 1024 )/1024;
		wchar_t const *szNoSpacePart1 = g_pVGuiLocalize->Find( "#L4D360UI_Boot_Error_NOSPACE1" );
		wchar_t const *szNoSpacePart2 = g_pVGuiLocalize->Find( "#L4D360UI_Boot_Error_NOSPACE2" );
		if ( szNoSpacePart1 && szNoSpacePart2 )
		{
			int nLen1 = Q_wcslen( szNoSpacePart1 );
			int nLen2 = Q_wcslen( szNoSpacePart2 );
			wchar_t *wszBuffer = new wchar_t[ nLen1 + nLen2 + 100 ];
			Q_wcsncpy( wszBuffer, szNoSpacePart1, 2 * ( nLen1 + nLen2 + 100 ) );
			Q_snwprintf( wszBuffer + Q_wcslen( wszBuffer ), 2*100, L"%u", nMbRequired );
			Q_wcsncpy( wszBuffer + Q_wcslen( wszBuffer ), szNoSpacePart2, 2*( nLen2 + 1 ) );
			char *pchErrorBuffer = new char[16];
			Q_snprintf( pchErrorBuffer, 16, "$ptr%u", wszBuffer );
			this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", pchErrorBuffer ) );
			return;
		}
	}

	this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "#L4D360UI_Boot_ErrorFatal" ) );
}

void CAttractScreen::OnGameBootSaveContainerReady()
{
	s_ePS3SaveInitState = SIS_FINISHED;
	if ( s_PS3SaveAsyncStatus.GetSonyReturnValue() < 0 )
	{
		// We've got an error!
		Warning( "OnGameBootSaveContainerReady error: 0x%X\n", s_PS3SaveAsyncStatus.GetSonyReturnValue() );
		char const *szFmt = "#L4D360UI_Boot_Error_SAVE_GENERAL";
		switch ( s_PS3SaveAsyncStatus.GetSonyReturnValue() )
		{
		case CELL_SAVEDATA_ERROR_NOSPACE:
		case CELL_SAVEDATA_CBRESULT_ERR_NOSPACE:
		case CELL_SAVEDATA_ERROR_SIZEOVER:
			ShowFatalError( (s_PS3SaveAsyncStatus.m_uiAdditionalDetails ? s_PS3SaveAsyncStatus.m_uiAdditionalDetails : s_nPs3SaveStorageSizeKB ) * 1024 );
			return;
		case CELL_SAVEDATA_ERROR_BROKEN:
		case CELL_SAVEDATA_CBRESULT_ERR_BROKEN:
			szFmt = "#L4D360UI_Boot_Error_BROKEN";
			break;
		case CPS3SaveRestoreAsyncStatus::CELL_SAVEDATA_ERROR_WRONG_USER:
			szFmt = "#L4D360UI_Boot_Error_WRONG_USER";
			break;
		}
		this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", szFmt ) );
		return;
	}
	if ( s_PS3SaveAsyncStatus.m_nCurrentOperationTag != kSAVE_TAG_INITIALIZE )
	{
		ShowFatalError( s_nPs3TrophyStorageSizeKB );
		return;
	}

	// Get the user's Steam stats
	this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "#L4D360UI_Boot_Trophies" ) );

	CUtlBuffer *pInitialDataBuffer = GetPs3SaveSteamInfoProvider()->GetInitialLoadBuffer();
#ifndef NO_STEAM
	m_CallbackOnUserStatsReceived.Register( this, &CAttractScreen::Steam_OnUserStatsReceived );
	steamapicontext->SteamUserStats()->SetUserStatsData( pInitialDataBuffer->Base(), pInitialDataBuffer->TellPut() );
	steamapicontext->SteamUserStats()->RequestCurrentStats();
#endif
	pInitialDataBuffer->Purge();
}
#endif

#if !defined(NO_STEAM) && defined(_PS3)
void CAttractScreen::Steam_OnPS3TrophiesInstalled( PS3TrophiesInstalled_t *pParam )
{
	m_CallbackOnPS3TrophiesInstalled.Unregister();

	s_PS3SaveAsyncStatus.m_nCurrentOperationTag = kSAVE_TAG_INITIALIZE;
	EResult eResult = pParam->m_eResult;
	if ( eResult == k_EResultDiskFull )
	{
		s_PS3SaveAsyncStatus.m_nCurrentOperationTag = kSAVE_TAG_UNKNOWN;
		s_nPs3TrophyStorageSizeKB += ( pParam->m_ulRequiredDiskSpace + 1023 )/ 1024;
		s_nPs3SaveStorageSizeKB += s_nPs3TrophyStorageSizeKB;
		eResult = k_EResultOK; // report cumulative space required after save container gets created
	}

	if ( eResult == k_EResultOK )
	{
		// Prepare the save container
		this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "#L4D360UI_Boot_SaveContainer" ) );
		ps3saveuiapi->Initialize( &s_PS3SaveAsyncStatus, GetPs3SaveSteamInfoProvider(), true, s_nPs3SaveStorageSizeKB );
		s_ePS3SaveInitState = SIS_INIT_REQUESTED;
	}
	else
	{
		ShowFatalError( 0 );
	}

}
#endif

#ifndef NO_STEAM
void CAttractScreen::Steam_OnUserStatsReceived( UserStatsReceived_t *pParam )
{
	m_CallbackOnUserStatsReceived.Unregister();

#ifdef _PS3
	if ( pParam->m_eResult != k_EResultOK )
	{
		this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "#L4D360UI_Boot_ErrorFatal" ) );
		return;
	}

	// Otherwise we have successfully received user stats and installed the trophies
	this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "#L4D360UI_Boot_InviteCheck" ) );
	m_bGameBootReady = true;
	// Check Sony game boot message
	m_CallbackOnPSNGameBootInviteResult.Register( this, &CAttractScreen::Steam_OnPSNGameBootInviteResult );
	Msg( "CheckForPSNGameBootInvite( 0x%08X )\n", g_pPS3PathInfo->GetGameBootAttributes() );
	steamapicontext->SteamMatchmaking()->CheckForPSNGameBootInvite( g_pPS3PathInfo->GetGameBootAttributes() );
#else
	this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "" ) );
#endif
}

#ifdef _PS3
extern uint64 g_uiConnectionToSteamToJoinLobbyId;
void CAttractScreen::Steam_OnPSNGameBootInviteResult( PSNGameBootInviteResult_t *pParam )
{
	m_CallbackOnPSNGameBootInviteResult.Unregister();

	Msg( "Steam_OnPSNGameBootInviteResult( %s: 0x%X )\n", pParam->m_bGameBootInviteExists?"detected":"none", pParam->m_steamIDLobby.ConvertToUint64() );

	uint64 uiLobbyIdToConnect = g_uiConnectionToSteamToJoinLobbyId;
	if ( !uiLobbyIdToConnect && pParam->m_bGameBootInviteExists )
		uiLobbyIdToConnect = pParam->m_steamIDLobby.ConvertToUint64();
	
	if ( uiLobbyIdToConnect )
	{
		CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->SuspendTransitions( false );

		Msg( "Steam_OnPSNGameBootInviteResult( connecting to lobby: 0x%X )\n", uiLobbyIdToConnect );
		KeyValues *kvEvent = new KeyValues( "OnSteamOverlayCall::LobbyJoin" );
		kvEvent->SetUint64( "sessionid", uiLobbyIdToConnect );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
	}
	else
	{
		Msg( "Steam_OnPSNGameBootInviteResult( proceeding to Press START screen )\n" );
		this->PostMessage( this, new KeyValues( "DisplayGameBootProgress", "msg", "" ) );
	}

	// Let the overlay finally activate
	if ( g_pISteamOverlayMgr )
		g_pISteamOverlayMgr->GameBootReady();

	// Cloud sync kick off
	if ( g_pGameSteamCloudSync )
		g_pGameSteamCloudSync->Sync( IGameSteamCloudSync::SYNC_GAMEBOOTREADY );
}
#endif
#endif

void CAttractScreen::OnOpen()
{
	BaseClass::OnOpen();

	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_NONE );
	}

#ifdef _GAMECONSOLE
	if ( BypassAttractScreen() )
		return;

	AcceptInvite();
#endif

	PerformGameBootWork();

	if ( s_eAttractMode == ATTRACT_GAMESTART &&	!m_bHidePressStart )
	{
		ShowPressStart();
	}
	else if ( s_eAttractMode == ATTRACT_GOSINGLESCREEN ||
		s_eAttractMode == ATTRACT_GOSPLITSCREEN ||
		s_eAttractMode == ATTRACT_GUESTSIGNIN )
	{
		ShowPressStart();
		m_bHidePressStart = true;
		HidePressStart();
	}
	else
	{
		m_bHidePressStart = true;
		HidePressStart();
	}

	if ( s_eAttractMode == ATTRACT_ACCEPTINVITE )
	{
#if defined( _PS3 ) && !defined( NO_STEAM )
		if ( CUIGameData::Get()->CanInitiateConnectionToSteam() )
		{
			// "join" notification is required to proceed with the invite
			CUIGameData::Get()->SetConnectionToSteamReason( "invitejoin", NULL );
			CUIGameData::Get()->InitiateConnectionToSteam();
		}
#endif
		CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_WaitScreen_JoiningParty", 0.0f );
	}
}

void CAttractScreen::OnClose()
{
	// SetAttractMode( ATTRACT_GAMESTART );	// reset attract mode, not relying on OnThink
	// cannot reset attract mode due to some crazy flow when attract screen is created
	// and closed 4 times while joining an invite from PRESS START screen!
	// now when attract screen needs to pop up calling code will set ATTRACT_GAMESTART mode.
	BaseClass::OnClose();
}

void CAttractScreen::OnEvent( KeyValues *pEvent )
{
#ifdef _GAMECONSOLE
	if ( m_eSignInUI == SIGNIN_NONE &&
		!Q_stricmp( "OnSysSigninChange", pEvent->GetName() ) &&
		!Q_stricmp( "signout", pEvent->GetString( "action", "" ) ) )
	{
		// Make sure that one of the committed controllers is not signing out
		int nMask = pEvent->GetInt( "mask", 0 );
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			int iController = XBX_GetUserId( k );
			bool bSignedOut = !!( nMask & ( 1 << iController ) );
			
			if ( bSignedOut ||
#ifdef _X360
				XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn
#else
				IsX360()	// false
#endif
				)
			{
				if ( UI_IsDebug() )
				{
					Msg( "[GAMEUI] CAttractScreen::OnEvent(OnSysSigninChange) - ctrlr %d signed out!\n", iController );
				}

				// Ooops, a committed user signed out while we were processing user settings and stuff...
				ShowPressStart();
				HideProgress();
				HideMsgs();
				return;
			}
		}
	}

	// We aren't expecting any more notifications if we aren't waiting for the blade
	if ( BLADE_NOTWAITING == GetBladeStatus() )
		return;

	if ( !Q_stricmp( "OnSysSigninChange", pEvent->GetName() ) &&
		 !Q_stricmp( "signin", pEvent->GetString( "action", "" ) ) )
	{
		int numUsers = pEvent->GetInt( "numUsers", 0 );
		int nMask = pEvent->GetInt( "mask", 0 );

		if ( UI_IsDebug() )
		{
			Msg( "[GAMEUI] AttractScreen::signin %d ctrlrs (0x%x), state=%d\n", numUsers, nMask, m_eSignInUI );
		}

		if ( numUsers > 0 )
		{
			for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
			{
				if ( ( nMask & ( 1 << k ) ) &&
#ifdef _X360
					 XUserGetSigninState( k ) == eXUserSigninState_NotSignedIn
#else
					IsX360()	// false
#endif
					 )
				{
					Msg( "[GAMEUI] AttractScreen::SYSTEMNOTIFY_USER_SIGNEDIN is actually a sign out, discarded!\n" );
					return;
				}
			}

			switch ( m_eSignInUI )
			{
			case SIGNIN_SINGLE:
				{
					// We expected a profile to sign in
					m_eSignInUI = SIGNIN_NONE;
					SetBladeStatus( BLADE_NOTWAITING );
					StartGame( pEvent->GetInt( "user0", -1 ) );
				}
				break;

			case SIGNIN_DOUBLE:
				{
					m_eSignInUI = SIGNIN_NONE;
					SetBladeStatus( BLADE_NOTWAITING );

					// We expected two profiles to sign in
					if ( numUsers == 2 &&
						 ( IsUserSignedInProperly( pEvent->GetInt( "user0", -1 ) ) ||
						   IsUserSignedInProperly( pEvent->GetInt( "user1", -1 ) ) ) )
					{
						XBX_SetUserIsGuest( 0, 0 );
						XBX_SetUserIsGuest( 1, 0 );

						s_idPrimaryUser = pEvent->GetInt( "user0", -1 );
						s_idSecondaryUser = pEvent->GetInt( "user1", -1 );

						if ( !OfferPromoteToLiveGuest() )
						{
							StartGame( s_idPrimaryUser, s_idSecondaryUser );
						}
					}
					else
					{
						// We need to reset all controller state to be
						// able to dismiss the message box
						ShowPressStart();

						GenericConfirmation* confirmation = 
							static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

						GenericConfirmation::Data_t data;

						data.pWindowTitle = "#L4D360UI_MsgBx_NeedTwoProfilesC";
						data.pMessageText = "#L4D360UI_MsgBx_NeedTwoProfilesTxt";
						data.bOkButtonEnabled = true;

						m_msgData = confirmation->SetUsageData(data);
					}
				}
				break;

			case SIGNIN_PROMOTETOGUEST:
				{
					m_eSignInUI = SIGNIN_NONE;
					SetBladeStatus( BLADE_NOTWAITING );

					if ( UI_IsDebug() )
					{
						Msg( "[GAMEUI] Promoted to guest (%d:0x%x) (expected secondary %d), primary=%d\n",
							numUsers, nMask,
							s_idSecondaryUser, s_idPrimaryUser );
					}

					// Find the primary or secondary controller that was picked previously
					// and mark that guy as promoted to guest

					if ( !!( nMask & ( 1 << s_idSecondaryUser ) ) )
					{
						// Figure out which slot this controller will occupy
						// int nSlotPrimary = (s_idPrimaryUser < s_idSecondaryUser) ? 0 : 1;
						int nSlotSecondary;
						nSlotSecondary = (s_idPrimaryUser < s_idSecondaryUser) ? 1 : 0;

						if ( UI_IsDebug() )
						{
							Msg( "[GAMEUI] Marking slot%d ctrlrs%d as guest\n",
								nSlotSecondary, s_idSecondaryUser );
						}

						// Mark the secondary slot as guest
						s_bSecondaryUserIsGuest = 1;
					}

					// Start the game
					StartGame( s_idPrimaryUser, s_idSecondaryUser );
				}
				break;
			}
			return;
		}
	}
#endif
}

void CAttractScreen::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pPressStartlbl = dynamic_cast< vgui::Label* >( FindChildByName( "LblPressStart" ) );

	if ( m_pPressStartlbl && m_bHidePressStart )
	{
		HidePressStart();
	}

	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetAlpha( 0 );
	}
}

void CAttractScreen::PaintBackground()
{
#ifdef IMAGEVIEWER_ENABLED
	if ( !g_arrImageViewerFiles.Count() )
		return;

	if ( g_ImageViewerReloadImage )
	{
		int idxStart = g_ImageViewerFileIndex;
		
		CUtlMemory< unsigned char > mem;
		int w,h;
		do {
			if ( TGALoader::LoadRGBA8888( g_arrImageViewerFiles[g_ImageViewerFileIndex], mem, w, h ) )
			{
				g_ImageViewerReloadImage = 0;
				break;
			}
			ImageViewerAdvanceIndex();
		} while( g_ImageViewerFileIndex != idxStart );

		if ( g_ImageViewerReloadImage )
		{
			return;
		}

		if ( -1 == g_ImageViewerTextureId )
		{
			g_ImageViewerTextureId = vgui::surface()->CreateNewTextureID( true );
		}
		vgui::surface()->DrawSetTextureRGBA( g_ImageViewerTextureId, mem.Base(), w, h );
		
		int screenWide, screenTall;
		vgui::surface()->GetScreenSize( screenWide, screenTall );
		g_ImageViewerTextureSize[0] = w; // w * ( ( (float) screenWide ) / 640.0f );
		g_ImageViewerTextureSize[1] = h; // h * ( ( (float) screenTall ) / 480.0f );
	}

	// The texture is loaded, paint it
	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTexture( g_ImageViewerTextureId );
	vgui::surface()->DrawTexturedRect( 0, 0, g_ImageViewerTextureSize[0], g_ImageViewerTextureSize[1] );
#endif
}

void CAttractScreen::OpenMainMenu()
{
	m_bHidePressStart = false;
	CBaseModPanel::GetSingleton().CloseAllWindows();
#ifdef _GAMECONSOLE
	if ( XBX_GetNumGameUsers() > 1 )
	{
		CUIGameData::Get()->OpenWaitScreen( "#Portal2UI_Matchmaking_JoiningGame", 0.0f );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnAttractModeWaitNotification", "state", "splitscreen" ) );
		return;
	}
#endif
	MainMenu::m_szPreferredControlName = "BtnPlaySolo";
	CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL, true );
}

void CAttractScreen::OpenMainMenuJoinFailed( const char *msg )
{
	m_bHidePressStart = false;
	CBaseModPanel::GetSingleton().CloseAllWindows();
	
	CBaseModFrame *pMainMenu = CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL, true );
	if ( pMainMenu )
		pMainMenu->PostMessage( pMainMenu, new KeyValues( "OpenMainMenuJoinFailed", "msg", msg ) );
}

#ifndef _CERT
class CGameBootProgressOperation : public IMatchAsyncOperation
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
	CGameBootProgressOperation() { m_eState = AOS_FAILED; }
	IMatchAsyncOperation * Prepare();

public:
	AsyncOperationState_t m_eState;
}
g_GameBootProgressOperation;

IMatchAsyncOperation * CGameBootProgressOperation::Prepare()
{
	m_eState = AOS_RUNNING;

	return this;
}

void CGameBootProgressOperation::Abort()
{
	m_eState = AOS_FAILED;

	CAttractScreen *pScreen = ( CAttractScreen * ) CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN );
	if ( !pScreen )
		return;
	pScreen->PostMessage( pScreen, new KeyValues( "DisplayGameBootProgress", "msg", "" ) );
}
#endif

void CAttractScreen::DisplayGameBootProgress( const char *msg )
{
	if ( msg && *msg )
	{
		CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->SuspendTransitions( true );

		HidePressStart();
		KeyValues *pSettings = NULL;
#ifndef _CERT
		if ( CommandLine()->FindParm( "-cancelgameboot" ) )
		{
			pSettings = new KeyValues( "WaitScreen" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetPtr( "options/asyncoperation", g_GameBootProgressOperation.Prepare() );
		}
#endif
		CUIGameData::Get()->OpenWaitScreen( msg, 0.0f, pSettings );
	}
	else
	{
		CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->SuspendTransitions( false );

		CUIGameData::Get()->CloseWaitScreen( 0, 0 );
		ShowPressStart();
		m_bGameBootReady = true;
	}
}

void CAttractScreen::OnChangeGamersFromMainMenu()
{
	CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
	CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
	ShowPressStart();
}

void CAttractScreen::StartWaitingForBlade1()
{
#ifdef _GAMECONSOLE
	if ( IsPrimaryUserSignedInProperly() )
	{
		StartGame( s_idPrimaryUser );
		return;
	}

	m_eSignInUI = SIGNIN_SINGLE;

#ifdef _X360
	XEnableGuestSignin( FALSE );
	XShowSigninUI( 1, XSSUI_FLAGS_LOCALSIGNINONLY );
#endif

	SetBladeStatus( BLADE_WAITINGFOROPEN );
#endif
}

void CAttractScreen::StartWaitingForBlade2()
{
#ifdef _GAMECONSOLE
	m_eSignInUI = SIGNIN_DOUBLE;

	// Determine if the primary user is properly signed in to LIVE
	bool bShowLiveProfiles = IsUserSignedInToLiveWithMultiplayer( s_idPrimaryUser ) &&
		!IsUserSignedInProperly( s_idSecondaryUser );

#ifdef _X360
	XEnableGuestSignin( TRUE );
	XShowSigninUI( 2, bShowLiveProfiles ? XSSUI_FLAGS_SHOWONLYONLINEENABLED : XSSUI_FLAGS_LOCALSIGNINONLY );
	CBaseModPanel::GetSingleton().AddFadeinDelayAfterOverlay( ui_coop_ss_fadeindelay.GetFloat() );
#endif

	if ( IsPS3() )
	{
		// Double-signin only driven from main menu, controllers already configured
		Assert( s_idPrimaryUser >= 0 );
		Assert( s_idSecondaryUser >= 0 );
		s_bSecondaryUserIsGuest = 1;
		StartGame( s_idPrimaryUser, s_idSecondaryUser );
		return;
	}

	SetBladeStatus( BLADE_WAITINGFOROPEN );
#endif
}

CAttractScreen::BladeStatus_t CAttractScreen::GetBladeStatus()
{
	return m_bladeStatus;
}

void CAttractScreen::SetBladeStatus( BladeStatus_t bladeStatus )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::SetBladeStatus( %d )\n", bladeStatus );
	}

	if ( IsPS3() && ( bladeStatus == BLADE_WAITINGTOSHOWSTORAGESELECTUI ) )
	{
		// Since PS3 doesn't really have storage selection, let's fake it all here
		ShowStorageDeviceSelectUI();
		return;
	}

	m_bladeStatus = bladeStatus;
}

void CAttractScreen::ShowStorageDeviceSelectUI()
{
	if ( IsX360() && !( CUIGameData::Get()->SelectStorageDevicePolicy() & STORAGE_DEVICE_NEED_ATTRACT ) ) // User doesn't care for storage device in attract screen?
	{
		// On X360 we don't store anything on storage device, defer
		// selection until we need to load or save games
		if ( m_eStorageUI == STORAGE_0 )
			StartGame_Stage2_Storage2();
		else
			StartGame_Stage3_Ready();
		return;
	}

	int iSlot = m_eStorageUI - 1;
	int iContorller = XBX_GetUserId( iSlot );
	DWORD dwDevice = XBX_GetStorageDeviceId( iContorller );

	CUIGameData::Get()->SelectStorageDevice( new CAttractScreenDeviceSelector(
		iContorller,
		( dwDevice == XBX_INVALID_STORAGE_ID ) ? true : false, // force the XUI to display
		true ) );
}

void CAttractScreen::HidePressStart()
{
	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetVisible( false );
	}
}

void CAttractScreen::HideProgress()
{
	CBaseModFrame *pMsg = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN );
	if ( pMsg )
	{
		pMsg->Close();
	}
}

void CAttractScreen::HideMsgs()
{
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
	if ( confirmation && confirmation->GetUsageId() == m_msgData )
	{
		confirmation->Close();
	}
}

void CAttractScreen::ShowPressStart()
{
#ifdef _GAMECONSOLE
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::ShowPressStart\n" );
	}

	m_pfnMsgChanged = NULL;

	XBX_ResetUserIdSlots();
	XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
	XBX_SetPrimaryUserIsGuest( 0 );	
	XBX_SetNumGameUsers( 0 ); // users not selected yet
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", int(0) ) );

	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetVisible( true );
	}

	m_bHidePressStart = false;
#endif
}

void CAttractScreen::ShowSignInDialog( int iPrimaryUser, int iSecondaryUser, BladeSignInUI_t eForceSignin )
{
	s_idPrimaryUser = iPrimaryUser;
	s_idSecondaryUser = -1;
	s_bSecondaryUserIsGuest = 0;

	// Whoever presses start becomes the primary user
	// and determines who's config we load, etc.
	XBX_SetPrimaryUserId( iPrimaryUser );

	// Lock the UI convar options to a particular splitscreen player slot
	SetGameUIActiveSplitScreenPlayerSlot( 0 );

	HidePressStart();
	m_bHidePressStart = true;

	if ( iSecondaryUser >= 0 )
	{
		s_idSecondaryUser = iSecondaryUser;
		StartWaitingForBlade2();
	}
	else if ( eForceSignin == SIGNIN_DOUBLE )
	{
		StartWaitingForBlade2();
	}
	else if ( IsPrimaryUserSignedInProperly() )
	{
		StartGame( s_idPrimaryUser );
	}
	else if ( eForceSignin == SIGNIN_SINGLE )
	{
		StartWaitingForBlade1();
	}
	else
	{
		CBaseModPanel::GetSingleton().SetLastActiveUserId( iPrimaryUser );
		CBaseModPanel::GetSingleton().OpenWindow( WT_SIGNINDIALOG, this, false );
	}
}

static void PlayGameWithTemporaryProfile();
void CAttractScreen::StartGameWithTemporaryProfile_Stage1()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGameWithTemporaryProfile_Stage1 - pri=%d\n", s_idPrimaryUser );
	}

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#L4D360UI_MsgBx_ConfirmGuestC";
	data.pMessageText = "#L4D360UI_MsgBx_ConfirmGuestTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	data.pfnOkCallback = PlayGameWithTemporaryProfile;
	data.pfnCancelCallback = ChangeGamers;

	m_msgData = confirmation->SetUsageData(data);
	m_pfnMsgChanged = ChangeGamers;
}

void CAttractScreen::StartGameWithTemporaryProfile_Real()
{
#if defined( _GAMECONSOLE )
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGameWithTemporaryProfile_Real - pri=%d\n", s_idPrimaryUser );
	}

	// Turn off all other controllers
	XBX_ClearUserIdSlots();

	XBX_SetPrimaryUserId( s_idPrimaryUser );
	XBX_SetPrimaryUserIsGuest( 1 );		// primary user id is retained

	XBX_SetUserId( 0, s_idPrimaryUser );
	XBX_SetUserIsGuest( 0, 1 );

	XBX_SetNumGameUsers( 1 );			// playing single-screen
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", int(1) ) );

	OpenMainMenu();
#endif //defined( _GAMECONSOLE )
}

static void ChangeGamers()
{
	CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >(
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true ) ) )
	{
		attractScreen->ShowPressStart();
	}
}

static void PlayGameWithTemporaryProfile()
{
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();
		attractScreen->StartGameWithTemporaryProfile_Real();
	}
	else
	{
		ChangeGamers();
	}
}

static void PlayGameWithSelectedProfiles()
{
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();
		attractScreen->StartGame( s_idPrimaryUser, s_idSecondaryUser );
	}
	else
	{
		ChangeGamers();
	}
}

bool CAttractScreen::OfferPromoteToLiveGuest()
{
#ifdef _X360
	int state1 = XUserGetSigninState( s_idPrimaryUser );
	int state2 = XUserGetSigninState( s_idSecondaryUser );

	BOOL bPriv1, bPriv2;

	if ( ERROR_SUCCESS != XUserCheckPrivilege( s_idPrimaryUser, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPriv1 ) )
		bPriv1 = FALSE;
	if ( ERROR_SUCCESS != XUserCheckPrivilege( s_idSecondaryUser, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPriv2 ) )
		bPriv2 = FALSE;

	BOOL bProperSignin1 = IsUserSignedInProperly( s_idPrimaryUser ) ? 1 : 0;
	BOOL bProperSignin2 = IsUserSignedInProperly( s_idSecondaryUser ) ? 1 : 0;

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] OfferPromoteToLiveGuest ( 0: %d %d %d %d ; 1: %d %d %d %d )\n",
			s_idPrimaryUser, state1, bPriv1, bProperSignin1, s_idSecondaryUser, state2, bPriv2, bProperSignin2 );
	}

	if ( ( bProperSignin2 > bProperSignin1 ) ||
		 ( bProperSignin2 && ( bPriv2 > bPriv1 ) ) )
	{
		V_swap( s_idPrimaryUser, s_idSecondaryUser );
		V_swap( state1, state2 );
		V_swap( bPriv1, bPriv2 );
		V_swap( bProperSignin1, bProperSignin2 );
	}

	if ( !bProperSignin2 )
	{
		// Secondary guy has all access to LIVE, but is
		// not properly signed-in
		s_bSecondaryUserIsGuest = 1;
	}

	// Portal 2 doesn't care upgrading profiles, just play splitscreen
	return false;

	// Now if somebody is Live, then it's the first ctrlr
	if ( state1 == eXUserSigninState_SignedInToLive && bPriv1 )
	{
		if ( !bPriv2 )
		{
			// We should offer the secondary user to upgrade to guest
			SetBladeStatus( BLADE_WAITINGTOSHOWPROMOTEUI );

			if ( UI_IsDebug() )
			{
				Msg( "[GAMEUI] OfferPromoteToLiveGuest - offering to ctrlr %d\n",
					s_idSecondaryUser );
			}

			return true;
		}
	}
	// Nobody is signed in to Live
	else
	{
		// We can't show this dialog, because it doesn't take into account that both players me be using LIVE accounts
		// with multiplayer disabled (or one Silver account). We don't have time to add localization for these other 
		// cases. So no instead of popping this choice dialog we're going to go ahead with it and let them get warnings
		// later when they try to access mutiplayer items from the menu with the correct detection and text.
		PlayGameWithSelectedProfiles();

		/*GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MsgBx_NeedLiveSplitscreenC";
		data.pMessageText = "#L4D360UI_MsgBx_NeedLiveSplitscreenTxt";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		data.pfnOkCallback = PlayGameWithSelectedProfiles;
		data.pfnCancelCallback = ChangeGamers;

		m_msgData = confirmation->SetUsageData(data);
		m_pfnMsgChanged = ChangeGamers;

		*/

		return true;
	}

#endif

	return false;
}

void CAttractScreen::StartGame( int idxUser1 /* = -1 */, int idxUser2 /* = -1 */ )
{
#if defined( _GAMECONSOLE )
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGame - starting for %d and %d.\n", idxUser1, idxUser2 );
	}

	// Keep information about our users
	s_idPrimaryUser = idxUser1;
	s_idSecondaryUser = idxUser2;

	// Turn off all controllers
	XBX_ClearUserIdSlots();

	// Configure the game type and controller assignments
	XBX_SetPrimaryUserId( idxUser1 );
	XBX_SetPrimaryUserIsGuest( 0 );

	if ( idxUser2 < 0 )
	{
		XBX_SetUserId( 0, idxUser1 );
		XBX_SetUserIsGuest( 0, 0 );

		XBX_SetNumGameUsers( 1 );
	}
	else
	{
		// Due to the nature of splitscreen activation on PS3
		// we don't want to reorder controller indices
		bool bReorderPlayerIndicesMinMax = !IsPS3();

		// Second user's guest status should be remembered earlier if this is the case
		XBX_SetUserId( 0, bReorderPlayerIndicesMinMax ? MIN( idxUser1, idxUser2 ) : idxUser1 );
		XBX_SetUserId( 1, bReorderPlayerIndicesMinMax ? MAX( idxUser1, idxUser2 ) : idxUser2 );

		int iSecondaryUserSlot = (bReorderPlayerIndicesMinMax && (idxUser2 < idxUser1)) ? 0 : 1;
		XBX_SetUserIsGuest( iSecondaryUserSlot, s_bSecondaryUserIsGuest );

		XBX_SetNumGameUsers( 2 );	// Splitscreen
	}

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", (int) XBX_GetNumGameUsers() ) );

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] Starting game...\n" );
		Msg( "[GAMEUI]   num game users = %d\n", XBX_GetNumGameUsers() );
		Msg( "[GAMEUI]   pri ctrlr%d guest%d\n", XBX_GetPrimaryUserId(), XBX_GetPrimaryUserIsGuest() );
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			Msg( "[GAMEUI]   slot%d ctrlr%d guest%d\n", k, XBX_GetUserId( k ), XBX_GetUserIsGuest( k ) );
		}
	}

	// Select storage device for user1
	StartGame_Stage1_Storage1();
#endif //defined( _GAMECONSOLE )

}

static void StorageContinue()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StorageContinue - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();

		switch ( s_eStorageUI )
		{
		case 1:
			attractScreen->StartGame_Stage2_Storage2();
			return;
		case 2:
			attractScreen->StartGame_Stage3_Ready();
			return;
		default:
			ChangeGamers();
			return;
		}
	}
	else
	{
		ChangeGamers();
	}
}

static void StorageSelectAgain()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StorageSelectAgain - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();

		int iSlot = s_eStorageUI - 1;
		int iContorller = XBX_GetUserId( iSlot );
		XBX_SetStorageDeviceId( iContorller, XBX_INVALID_STORAGE_ID );

		switch ( s_eStorageUI )
		{
		case 1:
			attractScreen->StartGame_Stage1_Storage1();
			return;
		case 2:
			attractScreen->StartGame_Stage2_Storage2();
			return;
		default:
			ChangeGamers();
			return;
		}
	}
	else
	{
		ChangeGamers();
	}
}

void CAttractScreen::ReportDeviceFail( ISelectStorageDeviceClient::FailReason_t eReason )
{
	if ( eReason == ISelectStorageDeviceClient::FAIL_NOT_SELECTED &&
		 // Check if command line allows to bypass warning in this case
		 CommandLine()->FindParm( "-nostorage" ) )
	{
		StorageContinue();
		return;
	}

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;

	switch ( eReason )
	{
	case ISelectStorageDeviceClient::FAIL_ERROR:
	case ISelectStorageDeviceClient::FAIL_NOT_SELECTED:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceNoneC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceNoneTxt";
		break;
	case ISelectStorageDeviceClient::FAIL_FULL:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceFullC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceFullTxt";
		break;
	case ISelectStorageDeviceClient::FAIL_CORRUPT:
	default:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceCorruptC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceCorruptTxt";
		break;
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	
	data.pfnOkCallback = StorageContinue;
	data.pfnCancelCallback = StorageSelectAgain;

	m_msgData = confirmation->SetUsageData(data);
	m_pfnMsgChanged = ChangeGamers;
}

void CAttractScreen::StartGame_Stage1_Storage1()
{
#ifdef _GAMECONSOLE
	HideProgress();

	if ( XBX_GetUserIsGuest( 0 ) )
	{
		StartGame_Stage2_Storage2();
		return;
	}

	m_eStorageUI = STORAGE_0;
	s_eStorageUI = STORAGE_0;
	SetBladeStatus( BLADE_WAITINGTOSHOWSTORAGESELECTUI );

	// Now we should expect one of the following:
	//		msg = ReportNoDeviceSelected
	//		msg = ReportDeviceFull
	//		selected callback followed by loaded callback, then XBX_GetStorageDeviceId will be set for our controller
#endif
}

void CAttractScreen::StartGame_Stage2_Storage2()
{
#ifdef _GAMECONSOLE
	HideProgress();

	if ( XBX_GetNumGameUsers() < 2 ||
		 XBX_GetUserIsGuest( 1 ) )
	{
		StartGame_Stage3_Ready();
		return;
	}

	m_eStorageUI = STORAGE_1;
	s_eStorageUI = STORAGE_1;
	SetBladeStatus( BLADE_WAITINGTOSHOWSTORAGESELECTUI );

	// Now we should expect one of the following:
	//		msg = ReportNoDeviceSelected
	//		msg = ReportDeviceFull
	//		selected callback followed by loaded callback, then XBX_GetStorageDeviceId will be set for our controller
#endif
}

void CAttractScreen::StartGame_Stage3_Ready()
{
	HideProgress();
	StartGame_Real( s_idPrimaryUser, s_idSecondaryUser );
}

void CAttractScreen::StartGame_Real( int idxUser1 /* = -1 */, int idxUser2 /* = -1 */ )
{
#if defined( _GAMECONSOLE )
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGame_Real - starting for %d and %d.\n", idxUser1, idxUser2 );
	}

	NavigateFrom();

	OpenMainMenu();
#endif //defined( _GAMECONSOLE )
}

void CAttractScreen::OnMsgResetCancelFn()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::OnMsgResetCancelFn\n" );
	}

	m_pfnMsgChanged = NULL;
}

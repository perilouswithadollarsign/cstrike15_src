//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "uigamedata.h"
#include "engineinterface.h"
#include "vgui/ILocalize.h"
#include "matchmaking/imatchframework.h"
#include "filesystem.h"
#include "fmtstr.h"
#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace BaseModUI;
using namespace vgui;

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996L
#endif

#if 0

//
// Storage device selection
//

//-----------------------------------------------------------------------------
// Purpose: A storage device has been connected, update our settings and anything else
//-----------------------------------------------------------------------------
namespace BaseModUI {

class CAsyncCtxUIOnDeviceAttached : public CUIGameData::CAsyncJobContext
{
public:
	explicit CAsyncCtxUIOnDeviceAttached( int iController );
	~CAsyncCtxUIOnDeviceAttached();
	virtual void ExecuteAsync();
	virtual void Completed();
	uint GetContainerOpenResult( void ) { return m_ContainerOpenResult; }

	int GetController() const { return m_iController; }

private:
	uint m_ContainerOpenResult;
	int m_iController;
};

CAsyncCtxUIOnDeviceAttached::CAsyncCtxUIOnDeviceAttached( int iController ) :
	CUIGameData::CAsyncJobContext( 3.0f ),	// Storage device info for at least 3 seconds
	m_ContainerOpenResult( ERROR_SUCCESS ),
	m_iController( iController )
{
	//CUIGameData::Get()->ShowMessageDialog( MD_CHECKING_STORAGE_DEVICE );
}

CAsyncCtxUIOnDeviceAttached::~CAsyncCtxUIOnDeviceAttached()
{
	//CUIGameData::Get()->CloseMessageDialog( 0 );
}

void CAsyncCtxUIOnDeviceAttached::ExecuteAsync()
{
	// Asynchronously do the tasks that don't interact with the command buffer
	// g_pFullFileSystem->DiscoverDLC( GetController() ); - don't call DiscoverDLC here

	// Open user settings and save game container here
	m_ContainerOpenResult = engine->OnStorageDeviceAttached( GetController() );
	if ( m_ContainerOpenResult != ERROR_SUCCESS )
		return;
}

ConVar ui_start_dlc_time_pump( "ui_start_dlc_time_pump", "30" );
ConVar ui_start_dlc_time_loaded( "ui_start_dlc_time_loaded", "150" );
ConVar ui_start_dlc_time_corrupt( "ui_start_dlc_time_corrupt", "300" );

CON_COMMAND_F( ui_pump_dlc_mount_corrupt, "", FCVAR_DEVELOPMENTONLY )
{
	int nStage = -1;
	if ( args.ArgC() > 1 )
	{
		nStage = Q_atoi( args.Arg( 1 ) );
	}
	DevMsg( 2, "ui_pump_dlc_mount_corrupt %d\n", nStage );

	int nCorruptDLCs = g_pFullFileSystem->IsAnyCorruptDLC();
	while ( nStage >= 0 && nStage < nCorruptDLCs )
	{
		static wchar_t wszDlcInfo[ 3 * MAX_PATH ] = {0};
		if ( !g_pFullFileSystem->GetAnyCorruptDLCInfo( nStage, wszDlcInfo, sizeof( wszDlcInfo ) ) )
		{
			++ nStage;
			continue;
		}

		// information text
		if ( wchar_t *wszExplanation = g_pVGuiLocalize->Find( "#SFUI_MsgBx_DlcCorruptTxt" ) )
		{
			int wlen = Q_wcslen( wszDlcInfo );
			Q_wcsncpy( wszDlcInfo + wlen, wszExplanation, sizeof( wszDlcInfo ) - 2 * wlen );
		}

		// We've got a corrupt DLC, put it up on the spinner
		CUIGameData::Get()->UpdateWaitPanel( wszDlcInfo, 0.0f );
		engine->ClientCmd( CFmtStr( "echo corruptdlc%d; wait %d; ui_pump_dlc_mount_corrupt %d;",
			nStage + 1, ui_start_dlc_time_corrupt.GetInt(), nStage + 1 ) );
		return;
	}

	// end of dlc mounting phases
	CUIGameData::Get()->OnCompletedAsyncDeviceAttached( NULL );
}

CON_COMMAND_F( ui_pump_dlc_mount_content, "", FCVAR_DEVELOPMENTONLY )
{
	int nStage = -1;
	if ( args.ArgC() > 1 )
	{
		nStage = Q_atoi( args.Arg( 1 ) );
	}
	DevMsg( 2, "ui_pump_dlc_mount_content %d\n", nStage );

	bool bSearchPathMounted = false;
	int numDlcsContent = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPathMounted );

	while ( nStage >= 0 && nStage < numDlcsContent )
	{
		static wchar_t wszDlcInfo[ 3 * MAX_PATH ] = {0};
		unsigned int ulMask;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( nStage, &ulMask, wszDlcInfo, sizeof( wszDlcInfo ) ) )
		{
			++ nStage;
			continue;
		}

		// information text
		if ( wchar_t *wszExplanation = g_pVGuiLocalize->Find( "#SFUI_MsgBx_DlcMountedTxt" ) )
		{
			int wlen = Q_wcslen( wszDlcInfo );
			Q_wcsncpy( wszDlcInfo + wlen, wszExplanation, sizeof( wszDlcInfo ) - 2 * wlen );
		}

		// We've got a corrupt DLC, put it up on the spinner
		CUIGameData::Get()->UpdateWaitPanel( wszDlcInfo, 0.0f );
		engine->ClientCmd( CFmtStr( "echo mounteddlc%d (0x%08X); wait %d; ui_pump_dlc_mount_content %d;",
			nStage + 1, ulMask, ui_start_dlc_time_loaded.GetInt(), nStage + 1 ) );
		return;
	}

	// Done displaying found content, show corrupt
	engine->ClientCmd( "ui_pump_dlc_mount_corrupt 0" );
}

CON_COMMAND_F( ui_pump_dlc_mount_stage, "", FCVAR_DEVELOPMENTONLY )
{
	// execute in order
	int nStage = -1;
	if ( args.ArgC() > 1 )
	{
		nStage = Q_atoi( args.Arg( 1 ) );
	}
	DevMsg( 2, "ui_pump_dlc_mount_stage %d\n", nStage );

	static char const *s_arrClientCmdsDlcMount[] = 
	{
		"net_reloadgameevents",
		"hud_reloadscheme",
		"gameinstructor_reload_lessons",
		"scenefilecache_reload",
		"cc_reload",
		"rr_reloadresponsesystems",
		"cl_soundemitter_reload",
		"sv_soundemitter_reload",
	};

	if ( nStage >= 0 && nStage < ARRAYSIZE( s_arrClientCmdsDlcMount ) )
	{
		// execute in phases, each command deferred occurs on main thread as required
		// adding a wait <frames> to let spinner clock a little
		// no way to solve any one phase that blocks for too long...this is good enough
		engine->ClientCmd( CFmtStr( "wait %d; %s; ui_pump_dlc_mount_stage %d;",
			ui_start_dlc_time_pump.GetInt(),
			s_arrClientCmdsDlcMount[ nStage ],
			nStage + 1 ) );
		return;
	}
	
	// Done mounting
	engine->ClientCmd( "ui_pump_dlc_mount_content 0" );
}


void CAsyncCtxUIOnDeviceAttached::Completed()
{
	bool bDLCSearchPathMounted = false;
	if ( GetContainerOpenResult() == ERROR_SUCCESS &&
		 g_pFullFileSystem->IsAnyDLCPresent( &bDLCSearchPathMounted ) )
	{
		if ( !( CUIGameData::Get()->SelectStorageDevicePolicy() & STORAGE_DEVICE_ASYNC ) )
		{
			Warning( "<vitaliy> DLC discovered during sync storage mount (not mounted)!\n" );
			goto completed_done;
		}
		if ( !bDLCSearchPathMounted )
		{
			// add the DLC search paths if they exist
			// this must be done on the main thread
			// the DLC search path mount will incur a quick synchronous hit due to zip mounting
			g_pFullFileSystem->AddDLCSearchPaths();

			// new DLC data may trump prior data, so need to signal isolated system reloads
			engine->ClientCmd( "ui_pump_dlc_mount_stage 0" );
			return;
		}
	}

	// No valid DLC was discovered, check if we discovered some corrupt DLC
	if ( g_pFullFileSystem->IsAnyCorruptDLC() )
	{
		if ( !( CUIGameData::Get()->SelectStorageDevicePolicy() & STORAGE_DEVICE_ASYNC ) )
		{
			Warning( "<vitaliy> DLC discovered during sync storage mount (corrupt)!\n" );
			goto completed_done;
		}
		// need to show just corrupt DLC information
		engine->ClientCmd( CFmtStr( "ui_pump_dlc_mount_corrupt %d", 0 ) );
		return;
	}

	// Otherwise we are done attaching storage right now
completed_done:
	CUIGameData::Get()->OnCompletedAsyncDeviceAttached( this );
}


}


#endif




void CUIGameData::RunFrame_Storage()
{

#ifdef _PS3
	GetPs3SaveSteamInfoProvider()->RunFrame();
#endif

#if 0
	// Check to see if a pending async task has already finished
	if ( m_pAsyncJob && !m_pAsyncJob->m_hThreadHandle )
	{
		m_pAsyncJob->Completed();
		delete m_pAsyncJob;
		m_pAsyncJob = NULL;
	}

	if( m_bWaitingForStorageDeviceHandle )
	{
		//the select device blade just closed, get the selected device
		DWORD ret = xboxsystem->GetOverlappedResult( m_hStorageDeviceChangeHandle, NULL, true );
		if ( ret != ERROR_IO_INCOMPLETE )
		{
			// Done waiting
			xboxsystem->ReleaseAsyncHandle( m_hStorageDeviceChangeHandle );

			m_bWaitingForStorageDeviceHandle = false;

			// If we selected something, validate it
			if ( m_iStorageID != XBX_INVALID_STORAGE_ID )
			{
				OnSetStorageDeviceId( m_iStorageController, m_iStorageID );
			}
			else
			{
				CloseWaitScreen( NULL, "ReportNoDeviceSelected" );
				if ( m_pSelectStorageClient )
				{
					m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_NOT_SELECTED );
					m_pSelectStorageClient = NULL;
				}
			}
		}
	}

#ifdef _PS3
	GetPs3SaveSteamInfoProvider()->RunFrame();
#endif

	if ( g_pGameSteamCloudSync )
		g_pGameSteamCloudSync->RunFrame();

#endif

}


#if 0
//
// CChangeStorageDevice
//
//		Should be used when user wants to change storage device
//

static CChangeStorageDevice *s_pChangeStorageDeviceCallback = NULL;

static void CChangeStorageDevice_Continue()
{
	s_pChangeStorageDeviceCallback->DeviceChangeCompleted( true );
	delete s_pChangeStorageDeviceCallback;
	s_pChangeStorageDeviceCallback = NULL;
}

static void CChangeStorageDevice_SelectAgain()
{
	CUIGameData::Get()->SelectStorageDevice( s_pChangeStorageDeviceCallback );
	s_pChangeStorageDeviceCallback = NULL;
}

CChangeStorageDevice::CChangeStorageDevice( int iCtrlr ) :
	m_iCtrlr( iCtrlr ),
	m_bAllowDeclined( true ),
	m_bForce( true ),
	m_nConfirmationData( 0 )
{
	// Just in case clean up (if dialogs were cancelled due to user sign out or such)
	delete s_pChangeStorageDeviceCallback;
	s_pChangeStorageDeviceCallback = NULL;
}

void CChangeStorageDevice::OnDeviceFail( FailReason_t eReason )
{
	// Depending if the user had storage device by this moment
	// or not we will take different actions:
	DWORD dwDevice = XBX_GetStorageDeviceId( GetCtrlrIndex() );

	switch ( eReason )
	{
	case FAIL_ERROR:
	case FAIL_NOT_SELECTED:
		if ( XBX_DescribeStorageDevice( dwDevice ) )
		{
			// That's fine user has a valid storage device, didn't want to change
			DeviceChangeCompleted( false );
			delete this;
			return;
		}
		// otherwise, proceed with the ui msg
	}

	XBX_SetStorageDeviceId( GetCtrlrIndex(), XBX_STORAGE_DECLINED );

	// We don't want to fire notification because there might be unsaved
	// preferences changes that were done without a storage device
	// no: g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );

	m_bAllowDeclined = false;

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().
		OpenWindow( WT_GENERICCONFIRMATION, CUIGameData::Get()->GetParentWindowForSystemMessageBox(), false ) );

	GenericConfirmation::Data_t data;

	switch ( eReason )
	{
	case FAIL_ERROR:
	case FAIL_NOT_SELECTED:
		data.pWindowTitle = "#SFUI_MsgBx_AttractDeviceNoneC";
		data.pMessageText = "#SFUI_MsgBx_AttractDeviceNoneTxt";
		break;
	case FAIL_FULL:
		data.pWindowTitle = "#SFUI_MsgBx_AttractDeviceFullC";
		data.pMessageText = "#SFUI_MsgBx_AttractDeviceFullTxt";
		break;
	case FAIL_CORRUPT:
	default:
		data.pWindowTitle = "#SFUI_MsgBx_AttractDeviceCorruptC";
		data.pMessageText = "#SFUI_MsgBx_AttractDeviceCorruptTxt";
		break;
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	s_pChangeStorageDeviceCallback = this;

	data.pfnOkCallback = CChangeStorageDevice_Continue;
	data.pfnCancelCallback = CChangeStorageDevice_SelectAgain;

	// WARNING! WARNING! WARNING!
	// The nature of Generic Confirmation is that it will be silently replaced
	// with another Generic Confirmation if a system event occurs
	// e.g. user unplugs controller, user changes storage device, etc.
	// If that happens neither OK nor CANCEL callbacks WILL NOT BE CALLED
	// The state machine cannot depend on either callback advancing the
	// state because in some situations neither callback can fire and the
	// confirmation dismissed/closed/replaced.
	// State machine must implement OnThink and check if the required
	// confirmation box is still present!
	// This code implements some sort of fallback - it deletes the static
	// confirmation data when a new storage device change is requested.
	// Vitaliy -- 9/26/2009
	//
	m_nConfirmationData = confirmation->SetUsageData(data);
}

void CChangeStorageDevice::OnDeviceSelected()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CChangeStorageDevice::OnDeviceSelected( 0x%08X )\n",
			XBX_GetStorageDeviceId( GetCtrlrIndex() ) );
	}

	CUIGameData::Get()->UpdateWaitPanel( "#SFUI_WaitScreen_SignOnSucceded" );
}

void CChangeStorageDevice::AfterDeviceMounted()
{
	DeviceChangeCompleted( true );
	
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );

	delete this;
}

void CChangeStorageDevice::DeviceChangeCompleted( bool bChanged )
{
	if ( bChanged )
	{
		Msg( "CChangeStorageDevice::DeviceChangeCompleted for ctrlr%d device 0x%08X\n",
			GetCtrlrIndex(), XBX_GetStorageDeviceId( GetCtrlrIndex() ) );
	}
	else
	{
		Msg( "CChangeStorageDevice::DeviceChangeCompleted - ctrlr%d is keeping device 0x%08X\n",
			GetCtrlrIndex(), XBX_GetStorageDeviceId( GetCtrlrIndex() ) );
	}
}

//=============================================================================
//
//=============================================================================

class CChangeStorageDeviceChained : public CChangeStorageDevice
{
public:
	typedef CChangeStorageDevice BaseClass;

	explicit CChangeStorageDeviceChained( int iCtrlrs[2] ) :
	BaseClass( iCtrlrs[0] ), m_nChainCtrlr( iCtrlrs[1] ) {}

	virtual void DeviceChangeCompleted( bool bChanged )
	{
		// Defer to the base class
		BaseClass::DeviceChangeCompleted( bChanged );

		// If we have a chain target, then call this off again
		if ( m_nChainCtrlr >= 0 )
		{
			CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( m_nChainCtrlr ) );
		}
	}

private:
	int	m_nChainCtrlr;
};


void OnStorageDevicesChangedSelectNewDevice()
{
#ifdef _GAMECONSOLE
	int numChangedCtrlrs = 0;
	int nChangedCtrlrs[2] = { -1, -1 };	// We can only have two users (split-screen)
	for ( DWORD i = 0; i < XBX_GetNumGameUsers(); ++ i )
	{
		int iController = XBX_GetUserId( i );

		// Guests can't choose a storage device!
		if ( XBX_GetUserIsGuest( i ) )
			continue;

		int nStorageID = XBX_GetStorageDeviceId( iController );
		if ( nStorageID == XBX_INVALID_STORAGE_ID )
		{
			// A controller's device has changed, and we'll need to prompt them to replace it
			nChangedCtrlrs[numChangedCtrlrs] = iController;
			numChangedCtrlrs++;
		}
	}

	// If a controller changed, then start off our device change dialogs
	if ( numChangedCtrlrs )
	{
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDeviceChained( nChangedCtrlrs ) );
	}
#endif // _GAMECONSOLE
}




void StorageDevice_SelectAllNow()
{
#ifdef _GAMECONSOLE
	int numChangedCtrlrs = 0;
	int nChangedCtrlrs[2] = { -1, -1 };	// We can only have two users (split-screen)
	for ( DWORD i = 0; i < XBX_GetNumGameUsers(); ++ i )
	{
		int iController = XBX_GetUserId( i );

		// Guests can't choose a storage device!
		if ( XBX_GetUserIsGuest( i ) )
			continue;

		int nStorageID = XBX_GetStorageDeviceId( iController );
		if ( nStorageID == XBX_INVALID_STORAGE_ID )
		{
			// A controller's device has changed, and we'll need to prompt them to replace it
			nChangedCtrlrs[numChangedCtrlrs] = iController;
			numChangedCtrlrs++;
		}
	}

	// If a controller changed, then start off our device change dialogs
	if ( numChangedCtrlrs )
	{
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDeviceChained( nChangedCtrlrs ) );
	}
#endif // _GAMECONSOLE
}

//=============================================================================
//This is where we open the XUI pannel to let the user select the current storage device.
bool CUIGameData::SelectStorageDevice( ISelectStorageDeviceClient *pSelectClient )
{
#ifdef _GAMECONSOLE

	if ( !pSelectClient )
		return false;

	int iController = pSelectClient->GetCtrlrIndex();
	bool bAllowDeclined = pSelectClient->AllowDeclined();
	bool bForceDisplay = pSelectClient->ForceSelector();
	bool bCheckCtrlr = !pSelectClient->AllowAnyController();

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] SelectStorageDevice( ctrlr=%d; %d, %d ), waiting=%d\n",
			iController, bAllowDeclined, bForceDisplay, m_bWaitingForStorageDeviceHandle );
	}

	if ( bCheckCtrlr )
	{
		// Check if the game is in guest mode
		if ( XBX_GetPrimaryUserIsGuest() )
		{
			Warning( "[GAMEUI] SelectStorageDevice for guest!\n" );
			pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
			return false;	// go away, no storage for guests
		}

		int nSlot = -1;
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			int iCtrlr = XBX_GetUserId( k );
			if ( iCtrlr != iController )
				continue;
			else if ( XBX_GetUserIsGuest( k ) )
			{
				Warning( "[GAMEUI] SelectStorageDevice for guest!\n" );
				pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
				return false;	// go away, game thinks you are a guest
			}
			else
				nSlot = k;
		}
		if ( nSlot < 0 )
		{
			Warning( "[GAMEUI] SelectStorageDevice for not active ctrlr!\n" );
			pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
			return false;	// this controller is not involved in the game, go away
		}
	}

#ifdef _X360
	// Is the controller signed in?
	if( XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
	{
		Warning( "[GAMEUI] SelectStorageDevice for not signed in user!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false; // not signed in, no device selector
	}

	// Maybe a guest buddy?
	XUSER_SIGNIN_INFO xsi;
	if ( ERROR_SUCCESS == XUserGetSigninInfo( iController, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
		(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) != 0 )
	{
		Warning( "[GAMEUI] SelectStorageDevice for LIVE-guest!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false; // guests don't have device selectors, go away
	}

	if ( ERROR_SUCCESS != XUserGetSigninInfo( iController, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &xsi ) )
	{
		Warning( "[GAMEUI] SelectStorageDevice failed to obtain XUID!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false; // failed to obtain XUID?
	}
#endif

	//
	// Prevent reentry
	//
	if( m_bWaitingForStorageDeviceHandle )
	{
		Warning( "[GAMEUI] SelectStorageDevice is already busy selecting storage device! Cannot re-enter!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false;	// Somebody already selecting a device
	}

#if defined( _DEMO ) && defined( _GAMECONSOLE )
	// Demo mode cannot have access to storage devices anyway
	if ( IsGameConsole() )
	{
		m_iStorageID = XBX_STORAGE_DECLINED;
		m_iStorageController = iController;
		m_pSelectStorageClient = pSelectClient;
		m_pSelectStorageClient->OnDeviceSelected();
		OnCompletedAsyncDeviceAttached( NULL );
		return true;
	}
#endif

	if ( IsPS3() )
	{
		// PS3 will have only two storage partitions: primary and secondary
		if ( iController == (int) XBX_GetPrimaryUserId() )
			XBX_SetStorageDeviceId( iController, 1 );
		else
			XBX_SetStorageDeviceId( iController, 2 );
		
		bForceDisplay = false; // PS3 doesn't display anything, so don't force it
	}

	// Check if we already have a valid storage device
	if ( XBX_GetStorageDeviceId( iController ) != XBX_INVALID_STORAGE_ID &&
		( bAllowDeclined || XBX_GetStorageDeviceId( iController ) != XBX_STORAGE_DECLINED ) &&
		! bForceDisplay )
	{
		if ( UI_IsDebug() )
		{
			Msg( "[GAMEUI] SelectStorageDevice - storage id = 0x%08X\n",
				XBX_GetStorageDeviceId( iController ) );
		}

		// Put up a progress that we are loading profile...
		if ( SelectStorageDevicePolicy() & STORAGE_DEVICE_ASYNC )
			OpenWaitScreen( "#SFUI_WaitScreen_SigningOn" );

		m_iStorageID = XBX_GetStorageDeviceId( iController );
		m_iStorageController = iController;
		m_pSelectStorageClient = pSelectClient;

		OnSetStorageDeviceId( iController, XBX_GetStorageDeviceId( iController ) );

		// Already have a storage device
		return true;
	}

	// Put up a progress that we are loading profile...
	OpenWaitScreen( "#SFUI_WaitScreen_SigningOn", 0.0f );
	// NOTE: this shouldn't have a 3 sec time-out as a new wait message is taking over
	// the progress when container starts mounting

	//open the dialog
	m_bWaitingForStorageDeviceHandle = true;
	m_hStorageDeviceChangeHandle = xboxsystem->CreateAsyncHandle();
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_iStorageController = iController;
	m_pSelectStorageClient = pSelectClient;
	xboxsystem->ShowDeviceSelector( iController, bForceDisplay, &m_iStorageID, &m_hStorageDeviceChangeHandle );
#endif
	return false;
}

uint32 CUIGameData::SelectStorageDevicePolicy()
{
	return
		( IsX360() ? STORAGE_DEVICE_ASYNC : 0 )
		;
}

//=============================================================================
void CUIGameData::OnDeviceAttached()
{
	//This is straight from CBasePanel
	ExecuteAsync( new CAsyncCtxUIOnDeviceAttached( m_iStorageController ) );
}

//=============================================================================
void CUIGameData::OnCompletedAsyncDeviceAttached( CAsyncCtxUIOnDeviceAttached * job )
{
	ISelectStorageDeviceClient *pStorageDeviceClient = m_pSelectStorageClient;
	m_pSelectStorageClient = NULL;

	uint nRet = job ? job->GetContainerOpenResult() : ERROR_SUCCESS;
	if ( nRet != ERROR_SUCCESS )
	{
		if ( SelectStorageDevicePolicy() & STORAGE_DEVICE_ASYNC )
			CloseWaitScreen( NULL, "ReportDeviceCorrupt" );
		pStorageDeviceClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_CORRUPT );
	}
	else
	{
		// Notify that data has loaded
		pStorageDeviceClient->AfterDeviceMounted();

		// Check for opening a new storage device immediately
		if ( m_pSelectStorageClient == NULL )
		{
			// Close down the waiting screen
			if ( SelectStorageDevicePolicy() & STORAGE_DEVICE_ASYNC )
				CloseWaitScreen( NULL, "OnCompletedAsyncDeviceAttached" );
		}
	}
}



#ifdef _WIN32
//-------------------------
// Purpose: Job wrapper
//-------------------------
static unsigned UIGameDataJobWrapperFn( void *pvContext )
{
	CUIGameData::CAsyncJobContext *pAsync = reinterpret_cast< CUIGameData::CAsyncJobContext * >( pvContext );

	float const flTimeStart = Plat_FloatTime();

	pAsync->ExecuteAsync();

	float const flElapsedTime = Plat_FloatTime() - flTimeStart;

	if ( flElapsedTime < pAsync->m_flLeastExecuteTime )
	{
		ThreadSleep( ( pAsync->m_flLeastExecuteTime - flElapsedTime ) * 1000 );
	}

	ReleaseThreadHandle( ( ThreadHandle_t ) pAsync->m_hThreadHandle );
	pAsync->m_hThreadHandle = NULL;

	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Enqueues a job function to be called on a separate thread
//-----------------------------------------------------------------------------
void CUIGameData::ExecuteAsync( CAsyncJobContext *pAsync )
{
	Assert( !m_pAsyncJob );
	Assert( pAsync && !pAsync->m_hThreadHandle );
	m_pAsyncJob = pAsync;

#ifdef _WIN32
	ThreadHandle_t hHandle = CreateSimpleThread( UIGameDataJobWrapperFn, reinterpret_cast< void * >( pAsync ) );
	pAsync->m_hThreadHandle = hHandle;

#ifdef _X360
	ThreadSetAffinity( hHandle, XBOX_PROCESSOR_3 );
#endif

#else
	// Since we are not running on a separate thread, just fire it all here
	m_pAsyncJob = NULL;
	pAsync->ExecuteAsync();
	pAsync->Completed();
	delete pAsync;
#endif
}

//////////////////////////////////////////////////////////////////////////
//
// Gamestats reporting
//

static int GameStats_GetActionIndex( char const *szReportAction )
{
	char const * arrActions[] =
	{
		"", "newgame", "commentary",
		"load", "loadlast",
		"continuenew", "continueload",
		"saveover", "savenew", "savedel",
		"changelevel", "changelevelcomm"
	};
	for ( int k = 0; k < ARRAYSIZE( arrActions ); ++ k )
	{
		if ( !V_stricmp( arrActions[k], szReportAction ) )
			return k + 1;
	}
	return 0;
}

static int GameStats_GetReportMapNameIndex( char const *szMapName )
{
	int nCounter = 1000; // resolve it as single player map
#define CFG( spmap, ... ) ++ nCounter; if ( !V_stricmp( szMapName, #spmap ) ) return nCounter;
#include "xlast_portal2/inc_sp_maps.inc"
#undef CFG
	nCounter = 0; // resolve it as coop map
#define CFG( mpcoopmap, ... ) ++ nCounter; if ( !V_stricmp( szMapName, #mpcoopmap ) ) return nCounter;
#include "xlast_portal2/inc_coop_maps.inc"
#undef CFG
	return 0;
}

void CUIGameData::GameStats_ReportAction( char const *szReportAction, char const *szMapName, uint64 uiFlags )
{
	KeyValues *kv = new KeyValues( "gamestat_action" );
	KeyValues::AutoDelete autodelete_kv( kv );
	kv->SetInt( "version", 1 );
	kv->SetUint64( "xuid", g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID() );
	kv->SetUint64( "*mac", 0ull ); // this will be filled out with console MAC-address
	kv->SetInt( "game_action", GameStats_GetActionIndex( szReportAction ) );
	kv->SetInt( "game_mapid", GameStats_GetReportMapNameIndex( szMapName ) );
	kv->SetUint64( "game_flags", uiFlags );

	IDatacenterCmdBatch *pBatch = g_pMatchFramework->GetMatchSystem()->GetDatacenter()->CreateCmdBatch();
	pBatch->SetDestroyWhenFinished( true );
	pBatch->SetRetryCmdTimeout( 30.0f );
	pBatch->AddCommand( kv );
}







#endif









#ifdef _PS3

//////////////////////////////////////////////////////////////////////////
//
// Steam info provider implementation
//
//////////////////////////////////////////////////////////////////////////

class CPS3SaveSteamInfoProvider : public IPS3SaveSteamInfoProviderUiGameData
{
public:
	virtual CUtlBuffer * GetInitialLoadBuffer()
	{
		GetBufferForSaveUtil().EnsureCapacity( 512*1024 - 128 );
		return &GetBufferForSaveUtil();
	}
	virtual CUtlBuffer * GetSaveBufferForCommit()
	{
		// called inside the saveutil callback (cannot cause allocations)
		return &GetBufferForSaveUtil();
	}
	virtual CUtlBuffer * PrepareSaveBufferForCommit()
	{
		// called on the main thread
		if ( !FillSteamBuffer() )
		{
			m_uiState &=~ ( STATE_DIRTY | STATE_INITIATED | STATE_COMMITTING );
			return NULL;
		}
		m_uiState |= STATE_COMMITTING; // flipping the committing flag early
		++ m_idxSaveUtilOwned;	// now saveutil owns the buffer we were writing into
		m_uiState &=~ STATE_DIRTY; // mark the steam data buffer as not yet dirty
		return &GetBufferForSaveUtil();
	}

	virtual void RunFrame();
	virtual void WriteSteamStats();

protected:
	CUtlBuffer m_arrBuffers[2];
	int m_idxSaveUtilOwned;
	inline CUtlBuffer& GetBufferForSaveUtil() { return m_arrBuffers[ m_idxSaveUtilOwned%2 ]; }
	inline CUtlBuffer& GetBufferForSteamData() { return m_arrBuffers[ !(m_idxSaveUtilOwned%2) ]; }
	bool FillSteamBuffer();

	CPS3SaveRestoreAsyncStatus m_ps3AsyncSaveStatus;
	enum AsyncSaveState_t
	{
		STATE_DEFAULT		= 0x00,		// default state, no save data ready, no save pending
		STATE_DIRTY			= 0x01,		// save data ready, will trigger saveutil when possible
		STATE_INITIATED		= 0x10,		// saveutil triggered, can still slipstream updated data
		STATE_COMMITTING	= 0x20,		// saveutil committing data, new updates must wait
	};
	uint32 m_uiState;
	uint32 m_numDirtyFrames;
}
g_ps3saveSteamInfoProvider;

IPS3SaveSteamInfoProviderUiGameData * GetPs3SaveSteamInfoProvider()
{
	return &g_ps3saveSteamInfoProvider;
}

bool CPS3SaveSteamInfoProvider::FillSteamBuffer()
{
	// Write the data into save buffer
	CUtlBuffer &buf = GetBufferForSteamData();
#ifndef NO_STEAM
	uint32 uiSizeRequired = buf.Size();
	bool bResult = steamapicontext->SteamUserStats()->GetUserStatsData( buf.Base(), buf.Size(), &uiSizeRequired );
	if ( !bResult && uiSizeRequired > buf.Size() )
	{
		buf.EnsureCapacity( uiSizeRequired );
		bResult = steamapicontext->SteamUserStats()->GetUserStatsData( buf.Base(), buf.Size(), &uiSizeRequired );
	}
	if ( !bResult )
	{
		buf.Purge();
		return false;
	}
	else
	{
		buf.SeekPut( CUtlBuffer::SEEK_HEAD, uiSizeRequired );
		return true;
	}
#else
	buf.Purge();
	return true;
#endif
}

void CPS3SaveSteamInfoProvider::RunFrame()
{
	// if save has been initiated, see if it has completed already
	if ( m_uiState & STATE_INITIATED )
	{
		if ( m_ps3AsyncSaveStatus.JobDone() )
		{
			m_uiState &=~( STATE_INITIATED | STATE_COMMITTING );
			Msg( "%.3f  CPS3SaveSteamInfoProvider::WriteSteamStats completed!\n", Plat_FloatTime() );
			
			GetBufferForSaveUtil().Purge();
		}
	}

	// if we have some new dirty data, then see if we can kick off a save
	if ( ( m_uiState & STATE_DIRTY ) && !( m_uiState & STATE_INITIATED ) )
	{
		if ( m_numDirtyFrames )
			-- m_numDirtyFrames;
		if ( ps3saveuiapi && !ps3saveuiapi->IsSaveUtilBusy() &&
			!m_numDirtyFrames

			/*
			// Not needed for CSGO
			&&
			( !CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ||
			( ( CAttractScreen * ) CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) )->IsGameBootReady() ) 
			*/

			)
		{
			m_ps3AsyncSaveStatus.m_nCurrentOperationTag = kSAVE_TAG_WRITE_STEAMINFO;
			ps3saveuiapi->WriteSteamInfo( &m_ps3AsyncSaveStatus );

			Msg( "%.3f  CPS3SaveSteamInfoProvider::WriteSteamStats kicked off saveutil work\n", Plat_FloatTime() );
			m_uiState |= STATE_INITIATED; // we kicked off a save operation successfully
		}
	}
}

void CPS3SaveSteamInfoProvider::WriteSteamStats()
{
	Msg( "%.3f  CPS3SaveSteamInfoProvider::WriteSteamStats prepared data\n", Plat_FloatTime() );

	m_uiState |= STATE_DIRTY;
	m_numDirtyFrames = 3;
}

#endif




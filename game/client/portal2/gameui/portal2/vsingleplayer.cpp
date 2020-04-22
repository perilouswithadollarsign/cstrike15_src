//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vsingleplayer.h"
#include "vsaveloadgamedialog.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "vgui_controls/Button.h"
#include "KeyValues.h"
#include "vgenericconfirmation.h"
#include "filesystem.h"
#include "vportalleaderboard.h"

#ifdef _PS3
#include "sysutil/sysutil_savedata.h"
#endif

#include "cegclientwrapper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#ifdef _PS3
CPS3SaveRestoreAsyncStatus CSinglePlayer::m_PS3SaveRestoreAsyncStatus;
#endif

CEG_NOINLINE CSinglePlayer::CSinglePlayer( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetDialogTitle( "#PORTAL2_PlaySinglePlayer_Header" );
	SetFooterEnabled( true );

	m_bFullySetup = false;

#if defined( _X360 )
	m_bHasStorageDevice = false;
#else
	m_bHasStorageDevice = true;
#endif
	m_bHasAnySaveGame = false;

	CEG_PROTECT_MEMBER_FUNCTION( CSinglePlayer_CSinglePlayer );

	m_bWaitingToLoadFromContainer = false;
	m_bLoadingFromContainer = false;

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );
}

CSinglePlayer::~CSinglePlayer()
{
	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	RemoveFrameListener( this );
}

void CSinglePlayer::SetDataSettings( KeyValues *pSettings )
{
	// getting this hint from the main menu, avoids us having to do a costly rescan
	m_bHasAnySaveGame = pSettings->GetBool( "foundsavegame", false );
}

CEG_NOINLINE void CSinglePlayer::OnCommand( char const *pCommand )
{
	CEG_PROTECT_MEMBER_FUNCTION( CSinglePlayer_OnCommand );

	if ( !V_stricmp( pCommand, "BtnContinueGame" ) )
	{
		CUtlVector< SaveGameInfo_t > saveGameInfos;
		if ( !CBaseModPanel::GetSingleton().GetSaveGameInfos( saveGameInfos ) )
		{
			// no prior save games, start new game at chapter 1
			KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetString( "map", CBaseModPanel::GetSingleton().ChapterToMapName( 1 ) );
			pSettings->SetString( "reason", "continuenew" );
			CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
		}
		else
		{
			int iMostRecent = CBaseModPanel::GetSingleton().GetMostRecentSaveGame( saveGameInfos );
			LoadSaveGameFromContainer( saveGameInfos[iMostRecent].m_MapName.Get(), saveGameInfos[iMostRecent].m_Filename.Get() );
		}
		return;
	}
	else if ( !V_stricmp( pCommand, "OpenNewGameDialog" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_NEWGAME, this, true );
		return;
	}
	else if ( !V_stricmp( pCommand, "OpenLoadGameDialog" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_LOADGAME, this, true );
		return;
	}
	else if ( !V_stricmp( pCommand, "OpenChallengeMode" ) )
	{
		KeyValues *pSettings = new KeyValues( "Setting" );
		pSettings->SetInt( "state", STATE_MAIN_MENU );
		CBaseModPanel::GetSingleton().OpenWindow( WT_PORTALLEADERBOARD, this, true, KeyValues::AutoDeleteInline( pSettings ) );
		return;
	}
	else if ( !V_stricmp( pCommand, "OpenCommentaryDialog" ) )
	{
		GenericConfirmation	*pConfirmation = static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;
		data.pWindowTitle = "#L4D360UI_Extras_Commentary";
		data.pMessageText = "#L4D360UI_Commentary_Explanation";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;
		data.pfnOkCallback = &ConfirmCommentary_Callback;
		pConfirmation->SetUsageData( data );
		return;
	}
	else if ( !V_stricmp( pCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( pCommand );
}

CEG_NOINLINE void CSinglePlayer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// only allowed to continue if there was a save game
	SetControlEnabled( "BtnContinueGame", m_bHasAnySaveGame );

	SetControlEnabled( "BtnDeveloperCommentary", true );

	bool bPrimaryUserIsGuest = false;
#if defined( _GAMECONSOLE )
	bPrimaryUserIsGuest = ( XBX_GetPrimaryUserIsGuest() != 0 );
#endif
	SetControlEnabled( "BtnLoadGame", !bPrimaryUserIsGuest );

	vgui::Panel *pPanel = FindChildByName( "BtnContinueGame" );
	if ( pPanel )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		pPanel->NavigateTo();
	}

	UpdateFooter();

	CEG_PROTECT_VIRTUAL_FUNCTION( CSinglePlayer_ApplySchemeSettings );

	m_bFullySetup = true;
}

CEG_NOINLINE void CSinglePlayer::Activate()
{
	BaseClass::Activate();

	CEG_PROTECT_VIRTUAL_FUNCTION( CSinglePlayer_Activate );

	// Only want to check when we had save games and we just got re-activated
	// from a child menu closing that has deleted all saves. Saves cannot be
	// created from any child menu here, only deleted. A storage change operation
	// is trapped elsewhere.
	if ( m_bFullySetup && m_bHasAnySaveGame )
	{
		CheckForAnySaves();
	}

	UpdateFooter();
}

void CSinglePlayer::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( IsGameConsole() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CSinglePlayer::RunFrame()
{
	BaseClass::RunFrame();

	// check for storage device
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );
	DWORD nStorageDevice = XBX_GetStorageDeviceId( iController );
	bool bHasStorageDevice = ( XBX_DescribeStorageDevice( nStorageDevice ) != 0 );
	if ( m_bHasStorageDevice != bHasStorageDevice )
	{
		if ( !bHasStorageDevice )
		{
			m_bHasStorageDevice = false;
			m_bHasAnySaveGame = false;
			SetControlEnabled( "BtnContinueGame", false );
		}
	}

#if defined( _PS3 )
	if ( m_bWaitingToLoadFromContainer )
	{
		// must wait until api idle
		if ( !ps3saveuiapi->IsSaveUtilBusy() )
		{
			// start single-shot async load operation
			m_bWaitingToLoadFromContainer = false;
			m_bLoadingFromContainer = true;
			m_PS3SaveRestoreAsyncStatus.m_nCurrentOperationTag = kSAVE_TAG_READ_SAVE;
			ps3saveuiapi->Load( &m_PS3SaveRestoreAsyncStatus, V_GetFileName( m_LoadFilename.Get() ), CFmtStr( "%s%s", engine->GetSaveDirName(), V_GetFileName( m_LoadFilename.Get() ) ) );
		}
	}
	else if ( m_bLoadingFromContainer )
	{
		if ( !ps3saveuiapi->IsSaveUtilBusy() )
		{
			// decode success or error
			m_bLoadingFromContainer = false;
			PostMessage( this, new KeyValues( "MsgPS3AsyncOperationComplete" ) );
		}
	}
#endif
}

void CSinglePlayer::OnEvent( KeyValues *pEvent )
{
	char const *pEventName = pEvent->GetName();

	if ( !V_stricmp( "OnProfileStorageAvailable", pEventName ) )
	{
		CheckForAnySaves();
	}
}

void CSinglePlayer::CheckForAnySaves()
{
	CUtlVector< SaveGameInfo_t > saveGameInfos;
	m_bHasAnySaveGame = CBaseModPanel::GetSingleton().GetSaveGameInfos( saveGameInfos, false );
	SetControlEnabled( "BtnContinueGame", m_bHasAnySaveGame );
}

void CSinglePlayer::ConfirmCommentary_Callback()
{
	CSinglePlayer *pSelf = static_cast< CSinglePlayer * >( CBaseModPanel::GetSingleton().GetWindow( WT_SINGLEPLAYER ) );
	if ( !pSelf )
	{
		return;
	}

	pSelf->OpenCommentaryDialog();
}


void CSinglePlayer::OpenCommentaryDialog()
{
	CBaseModPanel::GetSingleton().OpenWindow( WT_COMMENTARY, this, true );
}


void CSinglePlayer::LoadSaveGameFromContainerSuccess()
{
	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() != GetWindowType() )
	{
		return;
	}

	const char *pFilenameToLoad = m_LoadFilename.Get();

#ifdef _PS3
	char ps3Filename[MAX_PATH];
	pFilenameToLoad = RenamePS3SaveGameFile( pFilenameToLoad, ps3Filename, sizeof( ps3Filename ) );
#endif

	KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetString( "map", m_MapName.Get() );
	pSettings->SetString( "loadfilename", pFilenameToLoad );
	pSettings->SetString( "reason", "continueload" );
	CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
}

void CSinglePlayer::LoadSaveGameFromContainer( const char *pMapName, const char *pFilename )
{
	m_MapName = pMapName;
	m_LoadFilename = pFilename;

#ifdef _PS3
	// establish the full filename at the expected target
	char fullFilename[MAX_PATH];
	V_ComposeFileName( engine->GetSaveDirName(), (char *)V_GetFileName( pFilename ), fullFilename, sizeof( fullFilename ) );
	m_LoadFilename = fullFilename;

	if ( !g_pFullFileSystem->FileExists( m_LoadFilename.Get(), "MOD" ) )
	{
		if ( CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_LoadingGame", 0.0f, NULL ) )
		{
			m_bWaitingToLoadFromContainer = true;
		}
		return;
	}
#endif

	LoadSaveGameFromContainerSuccess();
}

void CSinglePlayer::MsgPS3AsyncOperationFailure()
{
#ifdef _PS3
	int nError = m_PS3SaveRestoreAsyncStatus.GetSonyReturnValue();

	DevMsg( "MsgPS3AsyncOperationFailure(): SonyRetVal:%d\n", nError );

	// failure, open a confirmation
	GenericConfirmation::Data_t data;
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = false;
	data.pfnOkCallback = NULL;

	data.pWindowTitle = "#PORTAL2_MsgBx_LoadFailure";
	data.pMessageText = "#PORTAL2_MsgBx_LoadFailureTxt";
	
	GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );
	pConfirmation->SetUsageData( data );
#endif
}

void CSinglePlayer::MsgPS3AsyncOperationComplete()
{
#ifdef _PS3
	int nError = m_PS3SaveRestoreAsyncStatus.GetSonyReturnValue();

	DevMsg( "MsgPS3AsyncOperationComplete(): SonyRetVal:%d\n", nError );

	if ( nError != CELL_SAVEDATA_RET_OK )
	{
		// failure, transition to confirmation
		CUIGameData::Get()->CloseWaitScreen( this, "MsgPS3AsyncOperationFailure" );
		return;
	}

	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
	LoadSaveGameFromContainerSuccess();
#endif
}

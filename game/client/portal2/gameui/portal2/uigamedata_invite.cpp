//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "basemodpanel.h"
#include "basemodframe.h"
#include "UIGameData.h"


#include "VGenericConfirmation.h"
#include "VAttractScreen.h"

// vgui controls
#include "vgui/ILocalize.h"

// matchsystem
#include "matchmaking/imatchframework.h"

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

#if defined (PORTAL2_PUZZLEMAKER)
#include "puzzlemaker/puzzlemaker.h"
#include "vpuzzlemakerexitconfirmation.h"
#include "vpuzzlemakersavedialog.h"

extern void ExitPuzzleMaker();
#endif

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;


//
// Invite approval
//

static int s_nInviteApprovalConf = 0;
static float s_flAbandonedTimeout = 0.f;
static ISelectStorageDeviceClient *s_pPendingInviteStorageSelector = NULL;

enum InviteUserMapping_t
{
	INVITE_USER_BLOCK_INPUT,
	INVITE_USER_ALLOW_INPUT
};
static void Invite_MapUserForUiInput( InviteUserMapping_t eUi )
{
	// Check invited user if it was the active user
#ifdef _GAMECONSOLE
	if ( XBX_GetInvitedUserId() == XBX_INVALID_USER_ID )
		return;

	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetInvitedUserId() == (DWORD) XBX_GetUserId( k ) )
			return;
	}

	// Invited user artificial mapping
	switch ( eUi )
	{
	case INVITE_USER_BLOCK_INPUT:
		XBX_ClearUserId( XBX_GetInvitedUserId() );
		break;
	case INVITE_USER_ALLOW_INPUT:
		XBX_SetUserId( XBX_GetNumGameUsers(), XBX_GetInvitedUserId() );
		break;
	}
#endif
}

static void Invite_NotifyAction( char const *szNotifyAction )
{
	s_nInviteApprovalConf = 0;

	Invite_MapUserForUiInput( INVITE_USER_BLOCK_INPUT );

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"OnInvite", "action", szNotifyAction ) );
}

void CUIGameData::Invite_Approved()
{
	if ( Invite_Connecting() )
	{
		Invite_NotifyAction( "join" );
	}
	else
	{
		s_nInviteApprovalConf = 0;
	}
}
void CUIGameData::Invite_Declined()
{
	Invite_NotifyAction( "deny" );
}

void CUIGameData::RunFrame_Invite()
{
	if ( s_nInviteApprovalConf )
	{
		bool bInviteAbandoned = true;
		// Check that the confirmation wasn't dismissed without notifying the invite system
		GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
		bInviteAbandoned &= !confirmation || confirmation->GetUsageId() != s_nInviteApprovalConf;

#if defined (PORTAL2_PUZZLEMAKER)
		CPuzzleMakerExitConfirmation* pSaveConfirmation = 
			static_cast<CPuzzleMakerExitConfirmation*>( CBaseModPanel::GetSingleton().GetWindow( WT_PUZZLEMAKEREXITCONRFIRMATION ) );
		bInviteAbandoned &= !pSaveConfirmation;


		CPuzzleMakerSaveDialog* pSaveDialog = 
			static_cast<CPuzzleMakerSaveDialog*>( CBaseModPanel::GetSingleton().GetWindow( WT_PUZZLEMAKERSAVEDIALOG ) );
		bInviteAbandoned &= !pSaveDialog;
#endif

		// If none of the right dialogs are up and we still have a conf number then the user must have dismissed these dialogs.
		// Now we need to start timing out.  We dont want to immediately decline the invite because there's a tiny gap between
		// when the approval confirmation gets approved and the save confirmation pops up.
		if ( bInviteAbandoned )
		{
			if( s_flAbandonedTimeout == 0.f )
			{
				s_flAbandonedTimeout = gpGlobals->realtime;
			}
			// Longer than 1 second, we'll assume they dismissed the prompts
			else if( (gpGlobals->realtime - s_flAbandonedTimeout) > 5.f )
			{
				// Well, pretend like user declined the prompt
				Invite_Declined();
				s_flAbandonedTimeout = 0.f;
			}
		}
	}
	else
	{
		s_flAbandonedTimeout = 0.f;
	}

	if ( s_pPendingInviteStorageSelector && !IsXUIOpen() )
	{
		SelectStorageDevice( s_pPendingInviteStorageSelector );
		s_pPendingInviteStorageSelector = NULL;
	}
}


//=============================================================================
#ifdef _GAMECONSOLE
class CInviteSelectStorageDevice : public CChangeStorageDevice
{
public:
	explicit CInviteSelectStorageDevice();

public:
	virtual void DeviceChangeCompleted( bool bChanged );
};

CInviteSelectStorageDevice::CInviteSelectStorageDevice() :
	CChangeStorageDevice( XBX_GetInvitedUserId() )
{
	// Get UI panel
	CBaseModPanel &ui = CBaseModPanel::GetSingleton();
	ui.OnGameUIActivated();

	// Allow non-involved controller
	m_bAnyController = true;

	// Don't force to re-select, just reload configs
	m_bForce = false;

	Invite_MapUserForUiInput( INVITE_USER_ALLOW_INPUT );
}

void CInviteSelectStorageDevice::DeviceChangeCompleted( bool bChanged )
{
	CChangeStorageDevice::DeviceChangeCompleted( bChanged );

	// Proceed with joining the invite session
	Invite_NotifyAction( "join" );
}
#endif

//=============================================================================
bool CUIGameData::Invite_IsStorageDeviceValid()
{
#ifdef _GAMECONSOLE
	//
	// Note: the only code path that should lead to this routine is
	// from invite accepting code.
	// XBX_GetInvitedUserId() is set to the user id of who accepted the invite
	// For that user the storage device has to be validated.
	//
	// If this function returns "true" it means that the user has a valid device
	// selected and it is safe to proceed with the invite.
	//
	// If this function returns "false" it will send the "join" action OnInvite event
	// after storage device selection process is over.
	//

	int iCtrlr = XBX_GetInvitedUserId();
	if ( iCtrlr < 0 || iCtrlr >= XUSER_MAX_COUNT )
		return true;

	// Check what device the guy currently has mapped
	DWORD dwDevice = XBX_GetStorageDeviceId( iCtrlr );

	if ( XBX_DescribeStorageDevice( dwDevice ) ||
		 XBX_STORAGE_DECLINED == dwDevice )
		// The guy has a valid device selected
		// or allow the guy to play because we already
		// told him earlier that his settings will not
		// be saved
		return true;

	//
	// Need to show device selector
	//

	s_pPendingInviteStorageSelector = new CInviteSelectStorageDevice;

	return false;

#endif
	return true;
}

static void Invite_Approved()
{
	CUIGameData::Get()->Invite_Approved();
}

static void Invite_Approved_Do_Save_Check()
{
#if defined ( PORTAL2_PUZZLEMAKER )
	if( g_pPuzzleMaker->GetActive() )
	{
		// Check if we're clear to exit.  If not, this function will open up the prompt for
		// the user to save their puzzle before leaving the puzzle maker
		if( !g_pPuzzleMaker->RequestQuitGame( PUZZLEMAKER_QUIT_TO_ACCEPT_COOP_INVITE ) )
		{
			return;
		}

		// Close out the puzzle maker
		ExitPuzzleMaker();
	}

#endif
	CUIGameData::Get()->Invite_Approved();
}

static void Invite_Declined()
{
	CUIGameData::Get()->Invite_Declined();
}

void CUIGameData::Invite_Confirm()
{
	// Activate game ui
	CBaseModPanel &ui = CBaseModPanel::GetSingleton();
	if ( !ui.IsVisible() )
	{
		// Activate game ui to see the dialog
		engine->ExecuteClientCmd( "gameui_activate" );
	}

	// Get current window
	CBaseModFrame *pFrame = NULL;
	WINDOW_TYPE wt = ui.GetActiveWindowType();
	if ( wt != WT_NONE &&
		wt != WT_GENERICCONFIRMATION )
		pFrame = ui.GetWindow( wt );

	// Show a prompt
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( ui.
		OpenWindow( WT_GENERICCONFIRMATION, pFrame, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#L4D360UI_LeaveInviteConf";
	data.pMessageText = 
#if defined (PORTAL2_PUZZLEMAKER)
	g_pPuzzleMaker->GetActive() ? "#PORTAL2_LeavePuzzleInviteConfTxt" : 
#endif
	"#L4D360UI_LeaveInviteConfTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	data.pfnOkCallback = ::Invite_Approved_Do_Save_Check;
	data.pfnCancelCallback = ::Invite_Declined;

	s_flAbandonedTimeout = 0.f;
	s_nInviteApprovalConf = confirmation->SetUsageData(data);

	Invite_MapUserForUiInput( INVITE_USER_ALLOW_INPUT );
}

bool CUIGameData::Invite_Connecting()
{
	// Close any session that we might have outstanding
	g_pMatchFramework->CloseSession();

#ifdef _PS3
	if ( CAttractScreen *pAttract = ( CAttractScreen * ) CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) )
	{
		if ( !pAttract->IsGameBootReady() )
			return false; // PS3: eating the invite until game boot sequence completed, will check the lobby ID after SteamGameBootMsg check
	}
#endif

	// Navigate to attract screen which might take a frame
	CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );
	CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_ACCEPTINVITE );
	CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );

#if defined( _PS3 ) && !defined( NO_STEAM )
	return steamapicontext->SteamUser()->BLoggedOn();
#else
	return true;
#endif
}



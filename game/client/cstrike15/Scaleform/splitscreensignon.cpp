//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jpaquin] The "Player Two press start" widget
//
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "basepanel.h"
#include "splitscreensignon.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "../engine/filesystem_engine.h"

#if defined( _X360 )
#include "xbox/xbox_launch.h"
#else
#include "xbox/xboxstubs.h"
#endif

#if defined	( _PS3 )
#include <sysutil/sysutil_userinfo.h>
#endif

#include "engineinterface.h"
#include "modinfo.h"
#include "gameui_interface.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include <vgui/ILocalize.h>
#include "inputsystem/iinputsystem.h"


using namespace vgui;

// for SRC
#include <vstdlib/random.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SplitScreenSignonWidget, SplitScreenSignon );

#if defined ( _PS3 )
static CellUserInfoTypeSet s_CellTypeSet;
static bool s_bUserSelectFinished = true;
static CellUserInfoUserStat s_CellUserSelected;
static int s_iUserSelectResult;
static void UserSelectFinishCallback(int result, CellUserInfoUserStat* pSelectUser, void* userdata)
{
	s_iUserSelectResult = result;
	if(result == CELL_USERINFO_RET_OK)
	{
		memcpy(&s_CellUserSelected, pSelectUser, sizeof(s_CellUserSelected));
	}
	s_bUserSelectFinished = true;
}

#endif

SplitScreenSignonWidget::SplitScreenSignonWidget() : 
	m_bVisible( false ),
	m_bConditionsAreValid( false ),
	m_bLoading( false ),
	m_bWantShown( false ),
	m_pPlayer2Name( NULL ),
	m_bWaitingForSignon( false ),
	m_iSecondPlayerId( -1 ),
	m_iControllerThatPressedStart( -1 ),
	m_bCurrentlyProcessingSignin( false ),
	m_bDropSecondPlayer( false )
{
	ListenForGameEvent( "sfuievent" );
#ifdef _PS3	
	s_CellTypeSet.title = "Select Player 2 user";
	s_CellTypeSet.focus = CELL_USERINFO_FOCUS_LISTHEAD;
	s_CellTypeSet.type = CELL_USERINFO_LISTTYPE_NOCURRENT;
#endif
}

void SplitScreenSignonWidget::FlashReady( void )
{
	m_bLoading = false;
	// Setup subscription so we are notified when the user signs in
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	( m_bVisible ) ? OnShow() : OnHide();
}

bool SplitScreenSignonWidget::PreUnloadFlash( void )
{
	// Remember to unsubscribe so we don't crash later!
	StopListeningForAllEvents();
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	return true;
}
	

void SplitScreenSignonWidget::OnShow( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );
		}
	}
	else if ( !m_bLoading )
	{
		m_bLoading = true;
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, SplitScreenSignonWidget, this, SplitScreenSignon );
	}

	m_bVisible = true;
}

void SplitScreenSignonWidget::OnHide( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
		}
	}

	m_bVisible = false;

}

void SplitScreenSignonWidget::UpdateState( void )
{
	bool showit = ( m_bConditionsAreValid && m_bWantShown );

	if ( showit != m_bVisible )
		showit ? OnShow() : OnHide();
}

void SplitScreenSignonWidget::Show( bool showit )
{
	if ( showit != m_bWantShown )
	{
		m_bWantShown = showit;
		UpdateState();
	}
}

void SplitScreenSignonWidget::SplitScreenConditionsAreValid( bool value )
{
	if ( value != m_bConditionsAreValid )
	{
		m_bConditionsAreValid = value;
		UpdateState();
	}
}

void SplitScreenSignonWidget::Update( void )
{
#if defined( _GAMECONSOLE )
	SplitScreenConditionsAreValid( g_pInputSystem->GetJoystickCount() > 1 );

	if ( m_bVisible )
	{

#ifdef _PS3
		g_pInputSystem->SetPS3StartButtonIdentificationMode();
#endif

		int iUserPressingStart = -1;
		int iPrimary = XBX_GetUserId( 0 );

		if ( XBX_GetNumGameUsers() == 1 )
		{
			if ( m_iControllerThatPressedStart == -1 )
			{
				for ( int i = 0; i < XUSER_MAX_COUNT && ( iUserPressingStart == -1 ) ; i++ )
				{
					if ( i != iPrimary )
					{
						if ( g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_INACTIVE_START, i ) ) )
						{
							iUserPressingStart = i;
						}
					}
				}

				if ( iUserPressingStart != -1 )
				{
					m_iControllerThatPressedStart = iUserPressingStart;
					m_bWaitingForSignon = true;
#if defined( _X360 )
					xboxsystem->ShowSigninUI( 2, XSSUI_FLAGS_LOCALSIGNINONLY ); // Two user, no special flags
#elif defined ( _PS3 )
					if(s_bUserSelectFinished) // Prevent cellUserInfoSelectUser_ListType being called more than once
					{
						s_bUserSelectFinished = false;
						cellUserInfoEnableOverlay(1); // Dim background while showing user selection dialog
						cellUserInfoSelectUser_ListType(&s_CellTypeSet, UserSelectFinishCallback, SYS_MEMORY_CONTAINER_ID_INVALID, NULL);
					}
#endif
				}
			}
#ifdef _PS3
			if(m_bWaitingForSignon && s_bUserSelectFinished)
			{
				m_bWaitingForSignon = false;
				if(s_iUserSelectResult == CELL_USERINFO_RET_OK)
				{

					int userID = s_CellUserSelected.id;

					if ( userID != -1 )
					{
						SetPlayerSignedIn();
						SetPlayer2Name( s_CellUserSelected.name );
					}
					else
					{
						m_iControllerThatPressedStart = -1;
					}
				}
				else 
				{
					m_iControllerThatPressedStart = -1;
				}
			}
#endif
		}
		else if ( ( XBX_GetNumGameUsers() == 2 ) )
		{
			if(g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_START, 1 ) ))
			{
				m_bDropSecondPlayer = true;
			}
			else if(m_bDropSecondPlayer)
			{
				// Wait for start button to be released before dropping
				// otherwise release event gets converted to KEY_XBUTTON_INACTIVE_START on PS3
				m_bDropSecondPlayer = false;
				DropSecondPlayer();
			}
		}
	}

#endif

}

void SplitScreenSignonWidget::SetPlayerSignedIn( void )
{
	if ( m_iControllerThatPressedStart == -1 )
	{
		return;
	}

#if defined( _GAMECONSOLE )

	XBX_SetUserId( 1, m_iControllerThatPressedStart );
	XBX_SetUserIsGuest ( 1, 0 );
	XBX_SetNumGameUsers ( 2 );
#endif
	m_iSecondPlayerId = m_iControllerThatPressedStart;
	m_iControllerThatPressedStart = -1;

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", ( int ) XBX_GetNumGameUsers() ) );

	BasePanel()->UpdateRichPresenceInfo();

	ConVarRef ss_enable( "ss_enable" );
	ss_enable.SetValue( 1 );
	ConVarRef ss_pipsplit( "ss_pipsplit" );
	ss_pipsplit.SetValue( 0 );

}

void SplitScreenSignonWidget::SetPlayer2Name( const char* name )
{
#if defined( _GAMECONSOLE )

	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			SafeReleaseSFVALUE( m_pPlayer2Name );

			if ( name && *name )
			{
				m_pPlayer2Name = CreateFlashString( name );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setPlayer2Name", m_pPlayer2Name, 1 );
			}
			else
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "clearPlayer2Name", NULL, 0 );
			}
		}

	}
#endif
}

void SplitScreenSignonWidget::DropSecondPlayer( void )
{
#if defined( _GAMECONSOLE )
	XBX_ClearSlot( 1 );
	XBX_SetNumGameUsers ( 1 );

	RevertUIToOnePlayerMode();

	if ( !m_bCurrentlyProcessingSignin )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", ( int ) XBX_GetNumGameUsers() ) );
		BasePanel()->UpdateRichPresenceInfo();
	}
#endif
}


void SplitScreenSignonWidget::RevertUIToOnePlayerMode( void )
{
	if ( FlashAPIIsValid() )
		SetPlayer2Name( NULL );
	m_iSecondPlayerId = -1;
	m_bWaitingForSignon = false;
	ConVarRef ss_enable( "ss_enable" );
	ss_enable.SetValue( 0 );
	ConVarRef ss_pipsplit( "ss_pipsplit" );
	ss_pipsplit.SetValue( 0 );

}



void SplitScreenSignonWidget::FireGameEvent( IGameEvent* pEvent )
{
	char const *szName = pEvent->GetName();

	// Notify that sign-in has completed
	if ( !V_stricmp( szName, "sfuievent" ) )
	{
		const char* action = pEvent->GetString( "action" );
		const char* data = pEvent->GetString( "data" );

		if ( action && *action )
		{
			if ( data && *data )
			{
				if ( !V_stricmp( data, "mainmenu" ) || !V_stricmp( data, "creategamedialog" ) )
				{
					if ( !V_stricmp( action, "show" ) )
					{
						Show( true );
					}
					else 
					{
						Show( false );
					}
				}
			}
		}

	}

}

void SplitScreenSignonWidget::OnEvent( KeyValues *pEvent )
{
#if defined( _X360 )

	if ( !m_bCurrentlyProcessingSignin )
	{
		m_bCurrentlyProcessingSignin = true;

		char const *szName = pEvent->GetName();

		// Notify that sign-in has completed
	
		if ( m_bWaitingForSignon )
		{
			if ( !V_stricmp( szName, "OnSysSigninChange" ) 	&&
				 !V_stricmp( "signin", pEvent->GetString( "action", "" ) ) )
			{
				m_bWaitingForSignon = false;

				int userID = pEvent->GetInt( "user1", -1 );

				if ( userID != -1 )
				{
					SetPlayerSignedIn();
				}
				else
				{
					m_iControllerThatPressedStart = -1;
				}
			}
			else 
			if ( !V_stricmp( szName, "OnSysXUIEvent" ) 	&&
				 !V_stricmp( "closed", pEvent->GetString( "action", "" ) ) )
			{
				m_bWaitingForSignon = false;
				m_iControllerThatPressedStart = -1;
			}
			else if ( !V_stricmp( szName, "OnProfilesChanged" ) )
			{
				m_bWaitingForSignon = false;
				m_iControllerThatPressedStart = -1;
			}

		}
		else if ( !V_stricmp( szName, "OnProfilesChanged" ) )
		{
			if ( m_iSecondPlayerId != -1)
			{
				IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( m_iSecondPlayerId );

				uint state = XUserGetSigninState( m_iSecondPlayerId );
				if ( state != eXUserSigninState_NotSignedIn )
				{
					if ( pProfile )
					{
						SetPlayer2Name( pProfile->GetName() );
					}
					else
					{
						SetPlayer2Name( "Player2" );
					}
				}
				else
				{
					DropSecondPlayer();
				}
			}

		}

		m_bCurrentlyProcessingSignin = false;
	}

#endif

}

#endif

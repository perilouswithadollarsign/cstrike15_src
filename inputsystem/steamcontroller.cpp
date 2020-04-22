//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Native Steam Controller Interface
//=============================================================================//
#include "inputsystem.h"
#include "key_translation.h"
#include "filesystem.h"
#include "steam/isteamcontroller.h"
#include "math.h"

#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CSteamAPIContext s_SteamAPIContext;	
CSteamAPIContext *steamapicontext = &s_SteamAPIContext;

ConVar sc_joystick_map( "sc_joystick_map", "1", FCVAR_ARCHIVE, "How to map the analog joystick deadzone and extents 0 = Scaled Cross, 1 = Concentric Mapping to Square." );

bool CInputSystem::InitializeSteamControllers()
{
	static bool s_bSteamControllerInitAttempted = false;	// we only initialize SteamAPI once, prevent multiple calls
	static bool s_bSteamControllerInitSucceeded = false;	// cached result from first SteamController()->Init() call
	if ( !s_bSteamControllerInitAttempted )
	{
		s_bSteamControllerInitAttempted = true;

		SteamAPI_InitSafe();
		s_SteamAPIContext.Init();

		if( s_SteamAPIContext.SteamController() )
		{
			m_flLastSteamControllerInput = -FLT_MAX;

			s_bSteamControllerInitSucceeded = steamapicontext->SteamController()->Init();
			if ( s_bSteamControllerInitSucceeded )
			{
				DevMsg( "Successfully Initialized Steam Controller Configuration." );
			}
			else
			{
				DevMsg( "Failed to Initialize Steam Controller Configuration." );			
			}

			m_nJoystickBaseline = m_nJoystickCount;
			s_SteamAPIContext.SteamController()->ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, s_SteamAPIContext.SteamController()->GetActionSetHandle( "MenuControls" ) );
		}
	}

	return s_bSteamControllerInitSucceeded;
}

struct SDigitalMenuAction
{
	const char *strName;
	ButtonCode_t buttonCode;
	ControllerDigitalActionHandle_t handle;
	bool bState;
	bool bActive;
};

static SDigitalMenuAction g_DigitalMenuActions[] = {
	{ "menu_left", KEY_XSTICK1_LEFT, 0, false, false },
	{ "menu_right", KEY_XSTICK1_RIGHT, 0, false, false },
	{ "menu_up", KEY_XSTICK1_UP, 0, false, false },
	{ "menu_down", KEY_XSTICK1_DOWN, 0, false, false },
	{ "menu_cancel", KEY_XBUTTON_B, 0, false, false },
	{ "menu_select", KEY_XBUTTON_A, 0, false, false },
	{ "menu_x", KEY_XBUTTON_X, 0, false, false },
	{ "menu_y", KEY_XBUTTON_Y, 0, false, false },
	{ "pause_menu", KEY_ESCAPE, 0, false, false },		// Command is actually in the FPS Controls game action set.
	{ "vote_yes", KEY_F1, 0, false, false },		// Command is actually in the FPS Controls game action set.
	{ "vote_no", KEY_F2, 0, false, false },		// Command is actually in the FPS Controls game action set.
};

static void InitDigitalMenuActions()
{
	for ( int i = 0; i != ARRAYSIZE( g_DigitalMenuActions ); ++i )
	{
		g_DigitalMenuActions[i].handle = steamapicontext->SteamController()->GetDigitalActionHandle( g_DigitalMenuActions[i].strName );
	}
}

static const char *g_pSteamControllerMode = "MenuControls";

bool CInputSystem::PollSteamControllers( void )
{
	unsigned int unNumConnected = 0;
	if ( InitializeSteamControllers() )
	{
		ISteamController& ctrl = *s_SteamAPIContext.SteamController();
		ctrl.RunFrame();

		ControllerHandle_t handles[MAX_STEAM_CONTROLLERS];
		int nControllers = ctrl.GetConnectedControllers( handles );

		SetInputDeviceConnected( INPUT_DEVICE_STEAM_CONTROLLER, nControllers > 0 );

		if ( nControllers > 0 )
		{
			static bool s_bInitialized = false;
			if ( s_bInitialized == false )
			{

				ctrl.ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, ctrl.GetActionSetHandle( g_pSteamControllerMode ) );
				InitDigitalMenuActions();
				s_bInitialized = true;
			}

			for ( int i = 0; i != ARRAYSIZE( g_DigitalMenuActions ); ++i )
			{
				SDigitalMenuAction& action = g_DigitalMenuActions[ i ];

				bool bNewState = false;
				bool bNewActive = false;
				for ( uint64 j = 0; j < nControllers; ++j )
				{
					ControllerDigitalActionData_t data = steamapicontext->SteamController()->GetDigitalActionData( handles[ j ], action.handle );

					if ( data.bActive )
					{
						bNewActive = true;
					}

					if ( data.bState && data.bActive )
					{
						bNewState = true;
					}
				}

				bNewActive = bNewActive && ( action.bActive || !bNewState );

				if ( action.bActive && bNewState != action.bState )
				{
					if ( bNewState )
					{
						SetCurrentInputDevice( INPUT_DEVICE_STEAM_CONTROLLER );
						PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, action.buttonCode, action.buttonCode );
					}
					else
					{
						PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, action.buttonCode, action.buttonCode );
					}
				}

				action.bState = bNewState;
				action.bActive = bNewActive;
			}
		}
		
	}

	m_unNumSteamControllerConnected = unNumConnected;
	return unNumConnected > 0;
}

bool CInputSystem::IsSteamControllerActive() const
{
	if ( steamapicontext && steamapicontext->SteamController() )
	{
		uint64 nControllerHandles[STEAM_CONTROLLER_MAX_COUNT]; 
		return steamapicontext->SteamController()->GetConnectedControllers( nControllerHandles ) > 0; 
	}

	return false;
}

static const int g_nControllerModeMaxStackSize = 16;
static const void *g_pControllerModeObjectStack[g_nControllerModeMaxStackSize];
static const char *g_pControllerModeStack[g_nControllerModeMaxStackSize];
static int g_nControllerModeStackSize = 0;

static const char *g_pBaseControllerMode = "MenuControls";

static const char *GetCurrentSteamControllerMode()
{
	if ( g_nControllerModeStackSize == 0 )
	{
		return g_pBaseControllerMode;
	}

	return g_pControllerModeStack[ g_nControllerModeStackSize-1 ];
}

void CInputSystem::SetSteamControllerMode( const char *pSteamControllerMode, const void *obj )
{
	if ( !s_SteamAPIContext.SteamController() )
	{
		return;
	}

	if ( obj == NULL )
	{
		g_pBaseControllerMode = pSteamControllerMode;
	}
	else
	{
		int nIndex = 0;
		while ( nIndex < g_nControllerModeStackSize && g_pControllerModeObjectStack[ nIndex ] != obj )
		{
			++nIndex;
		}

		Assert( nIndex < g_nControllerModeMaxStackSize );
		if ( nIndex == g_nControllerModeMaxStackSize )
		{
			return;
		}

		if ( pSteamControllerMode != NULL )
		{
			g_pControllerModeObjectStack[ nIndex ] = obj;
			g_pControllerModeStack[ nIndex ] = pSteamControllerMode;

			if ( nIndex == g_nControllerModeStackSize )
			{
				++g_nControllerModeStackSize;
			}
		}
		else if ( nIndex < g_nControllerModeStackSize )
		{
			for ( int i = nIndex+1; i < g_nControllerModeStackSize; ++i )
			{
				g_pControllerModeObjectStack[ i - 1 ] = g_pControllerModeObjectStack[ i ];
				g_pControllerModeStack[ i - 1 ] = g_pControllerModeStack[ i ];
			}

			--g_nControllerModeStackSize;
		}
	}

	const char *pNewMode = GetCurrentSteamControllerMode();

	if ( V_strcmp( pNewMode, g_pSteamControllerMode ) == 0 )
	{
		return;
	}

	g_pSteamControllerMode = pNewMode;

	s_SteamAPIContext.SteamController()->ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, s_SteamAPIContext.SteamController()->GetActionSetHandle( pNewMode ) );

}

CON_COMMAND( steam_controller_status, "Spew report of steam controller status" )
{
	if ( !s_SteamAPIContext.SteamController() )
	{
		Msg( "Steam controller API is unavailable" );
		return;
	}

	ControllerDigitalActionHandle_t handleAction = s_SteamAPIContext.SteamController()->GetActionSetHandle( GetCurrentSteamControllerMode() );

	Msg( "Steam controller mode: %s (%d)\n", GetCurrentSteamControllerMode(), (int)handleAction );

	ControllerHandle_t handles[MAX_STEAM_CONTROLLERS];
	int nControllers = s_SteamAPIContext.SteamController()->GetConnectedControllers( handles );

	Msg( "Controllers connected: %d\n", nControllers );
	for ( int i = 0; i < nControllers; ++i )
	{
		Msg( "  Controller %d, action set = %d\n", i, (int)s_SteamAPIContext.SteamController()->GetCurrentActionSet( handles[ i ] ) );
		for ( int j = 0; j < STEAM_CONTROLLER_MAX_DIGITAL_ACTIONS; ++j )
		{
			ControllerDigitalActionData_t data = s_SteamAPIContext.SteamController()->GetDigitalActionData( handles[ i ], (int)j );
			if ( data.bState && data.bActive )
			{
				Msg( "    active action: %d\n", j );
			}
		}
	}

}

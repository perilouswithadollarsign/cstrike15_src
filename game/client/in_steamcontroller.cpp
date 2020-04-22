//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Mouse input routines
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#if defined( WIN32 ) && !defined( _GAMECONSOLE )
#define _WIN32_WINNT 0x0502
#include <windows.h>
#endif
#ifdef OSX
#include <Carbon/Carbon.h>
#endif
#include "hud.h"
#include "cdll_int.h"
#include "kbutton.h"
#include "basehandle.h"
#include "usercmd.h"
#include "input.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "tier0/icommandline.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "vgui/Cursor.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "tier1/convar_serverbounded.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/iinputstacksystem.h"
#include "ienginevgui.h"

//Debugging for SteamController
#include "engine/ivdebugoverlay.h"
#include "clientsteamcontext.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cam_idealyaw;
extern ConVar cam_idealpitch;
extern ConVar thirdperson_platformer;

extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

static ConVar sc_enable( "sc_enable","1.0", FCVAR_ARCHIVE | FCVAR_SS , "Enable SteamController" );

static ConVar sc_yaw_sensitivity( "sc_yaw_sensitivity","1.0", FCVAR_ARCHIVE | FCVAR_SS , "SteamController yaw factor." );
static ConVar sc_yaw_sensitivity_default( "sc_yaw_sensitivity_default","1.0", FCVAR_NONE );

static ConVar sc_pitch_sensitivity( "sc_pitch_sensitivity","1.0", FCVAR_ARCHIVE | FCVAR_SS , "SteamController pitch factor." );
static ConVar sc_pitch_sensitivity_default( "sc_pitch_sensitivity_default","1.0", FCVAR_NONE );

void CInput::ApplySteamControllerCameraMove( QAngle& viewangles, CUserCmd *cmd, Vector2D vecPosition )
{
	PerUserInput_t &user = GetPerUser();

	//roll the view angles so roll is 0 (the HL2 assumed state) and mouse adjustments are relative to the screen.
	//Assuming roll is unchanging, we want mouse left to translate to screen left at all times (same for right, up, and down)

	ConVarRef cl_pitchdown ( "cl_pitchdown" );
	ConVarRef cl_pitchup ( "cl_pitchup" );
	ConVarRef cl_mouselook_roll_compensation ( "cl_mouselook_roll_compensation" );
	

	if ( CAM_IsThirdPerson() /*&& thirdperson_platformer.GetInt()*/ )
	{
		if ( vecPosition.x )
		{
			// use the mouse to orbit the camera around the player, and update the idealAngle
			user.m_vecCameraOffset[ YAW ] -= vecPosition.x;
			cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );
		}
	}
	else
	{
		// Otherwise, use mouse to spin around vertical axis
		viewangles[YAW] -= sc_yaw_sensitivity.GetFloat() * vecPosition.x;
	}

	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		if ( vecPosition.y )
		{
			// use the mouse to orbit the camera around the player, and update the idealAngle
			user.m_vecCameraOffset[ PITCH ] += vecPosition.y;
			cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );

			// why doesn't this work??? CInput::AdjustYaw is why
			//cam_idealpitch.SetValue( cam_idealpitch.GetFloat() + m_pitch->GetFloat() * mouse_y );
		}
	}
	else
	{
		viewangles[PITCH] -= vecPosition.y;

		// Check pitch bounds

		viewangles[PITCH] = clamp ( viewangles[PITCH], -cl_pitchdown.GetFloat(), cl_pitchup.GetFloat() );
	}		

	// Finally, add mouse state to usercmd.
	// NOTE:  Does rounding to int cause any issues?  ywb 1/17/04
	cmd->mousedx = (int)vecPosition.x;
	cmd->mousedy = (int)vecPosition.y;
}

static const char *g_ControllerDigitalGameActions[] = {
	"+attack", "+attack2", "+reload", "+jump", "+duck", "toggle_duck", "+use", "invnext", "invprev", "lastinv", "buymenu", "+showscores", "drop", "+speed", "slot1", "slot2", "slot3", "slot4", "slot5", "invnextgrenade", "invnextitem", "invnextnongrenade", "+voicerecord", "autobuy", "rebuy", "+lookatweapon",
};

struct ControllerDigitalActionState {
	const char* cmd;
	ControllerDigitalActionHandle_t handle;
	bool bState;
	bool bActive;
};

static ControllerDigitalActionState g_ControllerDigitalActionState[ARRAYSIZE(g_ControllerDigitalGameActions)];

static ControllerAnalogActionHandle_t g_ControllerMoveHandle;
static ControllerAnalogActionHandle_t g_ControllerCameraHandle;

static bool InitControllerTables()
{
	for ( int i = 0; i < ARRAYSIZE( g_ControllerDigitalGameActions ); ++i )
	{
		const char *action = g_ControllerDigitalGameActions[ i ];

		if ( *action == '+' )
		{
			++action;
		}

		ControllerDigitalActionState& state = g_ControllerDigitalActionState[ i ];
		state.handle = steamapicontext->SteamController()->GetDigitalActionHandle( action );
		if ( i == 0 && state.handle == 0 )
		{
			return false;
		}

		state.cmd = g_ControllerDigitalGameActions[ i ];
		state.bState = false;
	}

	g_ControllerMoveHandle = steamapicontext->SteamController()->GetAnalogActionHandle( "Move" );
	g_ControllerCameraHandle = steamapicontext->SteamController()->GetAnalogActionHandle( "Camera" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: SteamControllerMove -- main entry point for applying Steam Controller Movements
// Input  : *cmd - 
//-----------------------------------------------------------------------------
void CInput::SteamControllerMove( float flFrametime, CUserCmd *cmd )
{
	g_pInputSystem->SetSteamControllerMode( enginevgui->IsGameUIVisible() ? "MenuControls" : "GameControls" );
	
	if ( !steamapicontext || !steamapicontext->SteamController() )
	{
		return;
	}

	uint64 nControllerHandles[STEAM_CONTROLLER_MAX_COUNT];
	int nControllerCount = steamapicontext->SteamController()->GetConnectedControllers(nControllerHandles);
	if ( nControllerCount <= 0 )
	{
		return;
	}

	static bool bControllerTablesInitialized = false;
	if ( !bControllerTablesInitialized )
	{
		bControllerTablesInitialized = InitControllerTables();
	}

	if ( !bControllerTablesInitialized )
	{
		return;
	}

	// Look at all our digital actions
	for ( int i = 0; i < ARRAYSIZE( g_ControllerDigitalActionState ); ++i )
	{
		ControllerDigitalActionState& state = g_ControllerDigitalActionState[ i ];

		// Since we don't support split screen we just take the union of inputs of all
		// connected controllers if there are multiple connected.
		bool bState = false, bActive = false;
		for ( int nSlot = 0; nSlot < nControllerCount; ++nSlot )
		{
			ControllerDigitalActionData_t data = steamapicontext->SteamController()->GetDigitalActionData( nControllerHandles[ nSlot ], state.handle );
			bState = bState || data.bState;
			bActive = bActive || data.bActive;
		}

		if ( state.bActive && bState != state.bState )
		{

			if ( bState || state.cmd[0] == '+' )
			{
				char cmdbuf[128];
				Q_snprintf( cmdbuf, sizeof( cmdbuf ), "%s", state.cmd );
				if ( !bState )
				{
					cmdbuf[0] = '-';
				}

				engine->ClientCmd_Unrestricted( cmdbuf, true );

				IClientMode *clientMode = GetClientMode();
				if ( clientMode != NULL )
				{
					clientMode->KeyInput( bState ? true : false, STEAMCONTROLLER_SELECT, cmdbuf );
				}

			}
		}

		state.bState = bState;
		state.bActive = bActive && ( state.bActive || !state.bState );
	}

	if ( enginevgui->IsGameUIVisible() )
		return;

	//Handle movement based on controller data.
	for ( int nSlot = 0; nSlot < nControllerCount; ++nSlot )
	{
		ControllerAnalogActionData_t moveData = steamapicontext->SteamController()->GetAnalogActionData( nControllerHandles[ nSlot ], g_ControllerMoveHandle );

		if ( moveData.y > 0.0 )
		{
			cmd->forwardmove += cl_forwardspeed.GetFloat() * moveData.y;
		}
		else
		{
			cmd->forwardmove += cl_backspeed.GetFloat() * moveData.y;
		}

		cmd->sidemove += cl_sidespeed.GetFloat() * moveData.x;
	}

	// Now work out if we should change camera direction
	QAngle	viewangles;
	engine->GetViewAngles( viewangles );

	view->StopPitchDrift();

	for ( int nSlot = 0; nSlot < nControllerCount; ++nSlot )
	{
		ControllerAnalogActionData_t action = steamapicontext->SteamController()->GetAnalogActionData( nControllerHandles[ nSlot ], g_ControllerCameraHandle );

		float mouse_x = action.x;
		float mouse_y = action.y;
		ScaleMouse(0, &mouse_x, &mouse_y);

		Vector2D vecMouseDelta = Vector2D( mouse_x, -mouse_y )*0.015;

		::g_pInputSystem->SetCurrentInputDevice( INPUT_DEVICE_STEAM_CONTROLLER );
		if ( vecMouseDelta.Length() > 0 )
		{
			m_bControllerMode = true;
			if ( !GetPerUser().m_fCameraInterceptingMouse && g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
			{
				ApplySteamControllerCameraMove( viewangles, cmd, vecMouseDelta );
			}
		} 
	}

	engine->SetViewAngles( viewangles );
}

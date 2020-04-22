//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Joystick handling function
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "basehandle.h"
#include "utlvector.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "kbutton.h"
#include "usercmd.h"
#include "iclientvehicle.h"
#include "input.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "convar.h"
#include "hud.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "vgui/Cursor.h"
#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "math.h"
#include "tier1/convar_serverbounded.h"
#include "c_baseplayer.h"
#include "ienginevgui.h"
#include "inputsystem/iinputstacksystem.h"


#if defined (CSTRIKE_DLL)
#include "c_cs_player.h"
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#elif defined( _PS3 )
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#else
#include "../common/xbox/xboxstubs.h"
#endif

#ifdef PORTAL2
#include "radialmenu.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Control like a joystick
#define JOY_ABSOLUTE_AXIS	0x00000000		
// Control like a mouse, spinner, trackball
#define JOY_RELATIVE_AXIS	0x00000010		

// Set the joystick being force disabled just as we write the config
// This allows us to chose this option in the menu with a controller without accidentally disabling our only mode of input
void JoystickForceDisabled_ChangeCallback( IConVar *pConVar, char const *pOldString, float flOldValue );
static ConVar joystick_force_disabled_set_from_options( "joystick_force_disabled_set_from_options", "1", FCVAR_ARCHIVE, "Sets controllers enabled/disabled just before the config is written.", JoystickForceDisabled_ChangeCallback );
static ConVar joystick_force_disabled( "joystick_force_disabled", "1", FCVAR_ARCHIVE, "Prevents any and all joystick input for cases where a piece of hardware is incorrectly identified as a joystick an sends bad signals." );
void JoystickForceDisabled_ChangeCallback( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && !var.GetBool() )
	{
		// Enabling joystick happens immediately, rather than being delayed
		if ( joystick_force_disabled.GetBool() )
		{
			joystick_force_disabled.SetValue( false );
		}
	}
}

static ConVar joy_variable_frametime( "joy_variable_frametime", IsGameConsole() ? "0" : "1", 0 );

// Axis mapping
static ConVar joy_name( "joy_name", "joystick", FCVAR_ARCHIVE );
static ConVar joy_advanced( "joy_advanced", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisx( "joy_advaxisx", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisy( "joy_advaxisy", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisz( "joy_advaxisz", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisr( "joy_advaxisr", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisu( "joy_advaxisu", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisv( "joy_advaxisv", "0", FCVAR_ARCHIVE );

// Basic "dead zone" and sensitivity
static ConVar joy_forwardthreshold( "joy_forwardthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_sidethreshold( "joy_sidethreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_pitchthreshold( "joy_pitchthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_yawthreshold( "joy_yawthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_forwardsensitivity( "joy_forwardsensitivity", "-1", FCVAR_ARCHIVE );
static ConVar joy_sidesensitivity( "joy_sidesensitivity", "1", FCVAR_ARCHIVE );
static ConVar joy_pitchsensitivity( "joy_pitchsensitivity", "-1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "joystick pitch sensitivity", true, -5.0f, true, -0.1f );
static ConVar joy_yawsensitivity( "joy_yawsensitivity", "-1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "joystick yaw sensitivity", true, -5.0f, true, -0.1f );

// Advanced sensitivity and response
#ifdef _X360 //tmuaer
static ConVar joy_response_move( "joy_response_move", "9", FCVAR_ARCHIVE, "'Movement' stick response mode: 0=Linear, 1=quadratic, 2=cubic, 3=quadratic extreme, 4=power function(i.e., pow(x,1/sensitivity)), 5=two-stage" );
#else
static ConVar joy_response_move( "joy_response_move", "1", FCVAR_ARCHIVE, "'Movement' stick response mode: 0=Linear, 1=quadratic, 2=cubic, 3=quadratic extreme, 4=power function(i.e., pow(x,1/sensitivity)), 5=two-stage" );
#endif

ConVar joy_response_move_vehicle("joy_response_move_vehicle", "6");
static ConVar joy_response_look( "joy_response_look", "0", FCVAR_ARCHIVE, "'Look' stick response mode: 0=Default, 1=Acceleration Promotion" );
static ConVar joy_response_look_pitch( "joy_response_look_pitch", "1", FCVAR_ARCHIVE, "'Look' stick response mode for pitch: 0=Default, 1=Acceleration Promotion" );

static ConVar joy_lowend( "joy_lowend", "1", FCVAR_ARCHIVE );
static ConVar joy_lowend_linear( "joy_lowend_linear", "0.55", FCVAR_ARCHIVE );
static ConVar joy_lowmap( "joy_lowmap", "1", FCVAR_ARCHIVE );
static ConVar joy_gamma( "joy_gamma", "0.2", FCVAR_ARCHIVE );
static ConVar joy_accelscale( "joy_accelscale", "3.5", FCVAR_ARCHIVE);
static ConVar joy_accelscalepoly( "joy_accelscalepoly", "0.4", FCVAR_ARCHIVE);
static ConVar joy_accelmax( "joy_accelmax", "1.0", FCVAR_ARCHIVE);
static ConVar joy_autoAimDampenMethod( "joy_autoAimDampenMethod", "0", FCVAR_ARCHIVE );
static ConVar joy_autoaimdampenrange( "joy_autoaimdampenrange", "0", FCVAR_ARCHIVE, "The stick range where autoaim dampening is applied. 0 = off" );
static ConVar joy_autoaimdampen( "joy_autoaimdampen", "0", FCVAR_ARCHIVE, "How much to scale user stick input when the gun is pointing at a valid target." );
// smooth out of the auto-aim at this amount per second
static ConVar joy_autoaim_dampen_smoothout_speed( "joy_autoaim_dampen_smoothout_speed", "0.25" ); // percentage per second.  0.5 == 50 percentage points per second

static ConVar joy_curvepoint_1( "joy_curvepoint_1", "0.001", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_2( "joy_curvepoint_2", "0.4", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_3( "joy_curvepoint_3", "0.75", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_4( "joy_curvepoint_4", "1", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_end( "joy_curvepoint_end", "2", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );

static ConVar joy_vehicle_turn_lowend("joy_vehicle_turn_lowend", "0.7");
static ConVar joy_vehicle_turn_lowmap("joy_vehicle_turn_lowmap", "0.4");

static ConVar joy_sensitive_step0( "joy_sensitive_step0", "0.1", FCVAR_ARCHIVE);
static ConVar joy_sensitive_step1( "joy_sensitive_step1", "0.4", FCVAR_ARCHIVE);
static ConVar joy_sensitive_step2( "joy_sensitive_step2", "0.90", FCVAR_ARCHIVE);
static ConVar joy_circle_correct( "joy_circle_correct", "1", FCVAR_ARCHIVE);

// Misc
static ConVar joy_diagonalpov( "joy_diagonalpov", "0", FCVAR_ARCHIVE, "POV manipulator operates on diagonal axes, too." );
static ConVar joy_display_input("joy_display_input", "0", FCVAR_ARCHIVE);
static ConVar joy_wwhack2( "joy_wingmanwarrior_turnhack", "0", FCVAR_ARCHIVE, "Wingman warrior hack related to turn axes." );
ConVar joy_autosprint("joy_autosprint", "0", 0, "Automatically sprint when moving with an analog joystick" );

static ConVar joy_inverty("joy_inverty", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Whether to invert the Y axis of the joystick for looking." );

#if !defined ( CSTRIKE15 )
static ConVar joy_inverty_default( "joy_inverty_default", "0", FCVAR_ARCHIVE_GAMECONSOLE );				// Extracted & saved from profile
static ConVar joy_movement_stick_default( "joy_movement_stick_default", "0", FCVAR_ARCHIVE_GAMECONSOLE );	// Extracted & saved from profile
#endif

// XBox Defaults
static ConVar joy_yawsensitivity_default( "joy_yawsensitivity_default", "-1.0", FCVAR_NONE );
static ConVar joy_pitchsensitivity_default( "joy_pitchsensitivity_default", "-1.0", FCVAR_NONE );
static ConVar sv_stickysprint_default( "sv_stickysprint_default", "0", FCVAR_NONE );
static ConVar joy_lookspin_default( "joy_lookspin_default", "0.35", FCVAR_NONE );

static ConVar joy_cfg_preset( "joy_cfg_preset", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );

void joy_movement_stick_Callback( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( engine )
	{
		engine->ClientCmd_Unrestricted( "joyadvancedupdate silent\n" );
	}
}

static ConVar joy_movement_stick("joy_movement_stick", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Which stick controls movement : 0 = left stick, 1 = right stick, 2 = legacy controls", joy_movement_stick_Callback );

static ConVar joy_xcontroller_cfg_loaded( "joy_xcontroller_cfg_loaded", "0", 0, "If 0, the 360controller.cfg file will be executed on startup & option changes." );

ConVar joy_no_accel_jump( "joy_no_accel_jump", "0", FCVAR_ARCHIVE );

// Motion controller
static ConVar mc_dead_zone_radius( "mc_dead_zone_radius", "0.06", FCVAR_ARCHIVE, "0 to 0.9. 0 being just around the center of the screen and 1 being the edges of the screen.", true, 0.0f, true, 0.9f );
static ConVar mc_accel_band_size( "mc_accel_band_size", "0.5", FCVAR_ARCHIVE, "Percentage of half the screen width or height.", true, 0.01f, true, 2.0f );
static ConVar mc_max_yawrate( "mc_max_yawrate", "230.0", FCVAR_ARCHIVE, "(degrees/sec)", true, 10.0,true, 720.0 );
static ConVar mc_max_pitchrate( "mc_max_pitchrate", "100.0", FCVAR_ARCHIVE, "(degrees/sec)", true, 10.0,true, 720.0 );
static ConVar mc_turnPctPegged("mc_turnPctPegged", "1.0", FCVAR_DEVELOPMENTONLY, "pegged at above this amount" );
static ConVar mc_turnPctPeggedMultiplier("mc_turnPctPeggedMultiplier", "1.0", FCVAR_DEVELOPMENTONLY, "speed multiplier when pegged" );
static ConVar mc_turn_curve("mc_turn_curve", "0", FCVAR_DEVELOPMENTONLY, "What type of acceleration curve to use for turning.");
static ConVar mc_screen_clamp( "mc_screen_clamp", "0.8f", FCVAR_DEVELOPMENTONLY, "Clamps the cursor to this much of the screen.");
static ConVar mc_force_aim_x("mc_force_aim_x", "0", FCVAR_DEVELOPMENTONLY, "debug for testing player's aim");
static ConVar mc_force_aim_y("mc_force_aim_y", "0", FCVAR_DEVELOPMENTONLY, "debug for testing player's aim");
static ConVar mc_always_lock_ret_on_zoom( "mc_always_lock_ret_on_zoom", "1", FCVAR_DEVELOPMENTONLY, "Always lock the reticle when zoomed (even for partial zoom weapons)");
static ConVar mc_max_dampening("mc_max_dampening", "0.9", FCVAR_DEVELOPMENTONLY, "dampening player's aim");
static ConVar mc_dampening_blend_amount("mc_dampening_blend_amount", "0.0", FCVAR_DEVELOPMENTONLY, "dampening player's aim");

static ConVar mc_max_turn_dampening("mc_max_turn_dampening", "0.8", FCVAR_DEVELOPMENTONLY, "dampening player's aim while scoped");
static ConVar mc_turn_dampening_blend_amount("mc_turn_dampening_blend_amount", "0.02", FCVAR_DEVELOPMENTONLY, "dampening player's aim while scoped");

static ConVar mc_zoom_out_cursor_offset_blend("mc_zoom_out_cursor_offset_blend", "0.05", FCVAR_DEVELOPMENTONLY, "0.0 means snap to the new amount.");

static ConVar mc_zoomed_out_dead_zone_radius( "mc_zoomed_out_dead_zone_radius", "0.1", FCVAR_DEVELOPMENTONLY, "0 to 0.9. 0 being just around the center of the screen and 1 being the edges of the screen.", true, 0.0f, true, 0.9f );
static ConVar mc_zoomed_aim_style("mc_zoomed_aim_style", "1", FCVAR_DEVELOPMENTONLY, "0-analog stick style. 1-pointer style.");

extern ConVar lookspring;
extern ConVar cl_forwardspeed;
extern ConVar lookstrafe;
extern ConVar in_joystick;
extern ConVar_ServerBounded *m_pitch;
extern ConVar l_pitchspeed;
extern ConVar cl_sidespeed;
extern ConVar cl_yawspeed;
extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;
extern ConVar cl_pitchspeed;
#ifdef INFESTED_DLL
extern ConVar asw_cam_marine_yaw;
#endif
extern ConVar cam_idealpitch;
extern ConVar cam_idealyaw;
extern ConVar thirdperson_platformer;
extern ConVar thirdperson_screenspace;

//-----------------------------------------------
// Response curve function for the move axes
//-----------------------------------------------
static float ResponseCurve( int curve, float x, int axis, float sensitivity )
{
	switch ( curve )
	{
	case 1:
		// quadratic
		if ( x < 0 )
			return -(x*x) * sensitivity;
		return x*x * sensitivity;

	case 2:
		// cubic
		return x*x*x*sensitivity;

	case 3:
		{
		// quadratic extreme
		float extreme = 1.0f;
		if ( fabs( x ) >= 0.95f )
		{
			extreme = 1.5f;
		}
		if ( x < 0 )
			return -extreme * x*x*sensitivity;
		return extreme * x*x*sensitivity;
		}
	case 4:
		{
			float flScale = sensitivity < 0.0f ? -1.0f : 1.0f;

			sensitivity = clamp( fabs( sensitivity ), 1.0e-8f, 1000.0f );

			float oneOverSens = 1.0f / sensitivity;
		
			if ( x < 0.0f )
			{
				flScale = -flScale;
			}

			float retval = clamp( powf( fabs( x ), oneOverSens ), 0.0f, 1.0f );
			return retval * flScale;
		}
		break;
	case 5:
		{
			float out = x;

			if( fabs(out) <= 0.6f )
			{
				out *= 0.5f;
			}

			out = out * sensitivity;
			return out;
		}
		break;
	case 6: // Custom for driving a vehicle!
		{
			if( axis == YAW )
			{
				// This code only wants to affect YAW axis (the left and right axis), which 
				// is used for turning in the car. We fall-through and use a linear curve on 
				// the PITCH axis, which is the vehicle's throttle. REALLY, these are the 'forward'
				// and 'side' axes, but we don't have constants for those, so we re-use the same
				// axis convention as the look stick. (sjb)
				float sign = 1;

				if( x  < 0.0 )
					sign = -1;

				x = fabs(x);

				if( x <= joy_vehicle_turn_lowend.GetFloat() )
					x = RemapVal( x, 0.0f, joy_vehicle_turn_lowend.GetFloat(), 0.0f, joy_vehicle_turn_lowmap.GetFloat() );
				else
					x = RemapVal( x, joy_vehicle_turn_lowend.GetFloat(), 1.0f, joy_vehicle_turn_lowmap.GetFloat(), 1.0f );

				return x * sensitivity * sign;
			}
			//else
			//	fall through and just return x*sensitivity below (as if using default curve)
		}
		//The idea is to create a max large walk zone surrounded by a max run zone.
	case 7:
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				return ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	case 8: //same concept as above but with smooth speeds
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step0.GetFloat())
					/ (joy_sensitive_step2.GetFloat()-joy_sensitive_step0.GetFloat());
				float speed = t*maxSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				float maxSpeed = 1.0f;
				float minSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step2.GetFloat())
					/ (1.0f-joy_sensitive_step2.GetFloat());
				float speed = t*(maxSpeed-minSpeed) + minSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	case 9: //same concept as above but with smooth speeds for walking and a hard speed for running
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step1.GetFloat())
			{
				float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step0.GetFloat())
					/ (joy_sensitive_step1.GetFloat()-joy_sensitive_step0.GetFloat());
				float speed = t*maxSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				return ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	}

	// linear
	return x*sensitivity;
}

//-----------------------------------------------
// If we have a valid autoaim target, dampen the 
// player's stick input if it is moving away from
// the target.
//
// This assists the player staying on target.
//-----------------------------------------------
float CInput::AutoAimDampening( float x, int axis, float dist )
{
	if ( joy_autoAimDampenMethod.GetInt() == 1 )	
	{
// disabled 6/29/15 -mtw
/*
		// $FIXME(hpe) Split screen

		// Help the user stay on target if the feature is enabled and the user
		// is not making a gross stick movement.
		if ( joy_autoaimdampen.GetFloat() > 0.0f && fabs(x) < joy_autoaimdampenrange.GetFloat() )
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

			if ( pLocalPlayer && pLocalPlayer->IsCursorOnAutoAimTarget() )
			{
				m_lastAutoAimValue = joy_autoaimdampen.GetFloat();
				return m_lastAutoAimValue;
			}
		}

		if ( m_lastAutoAimValue < 1.0f )
		{
			m_lastAutoAimValue += joy_autoaim_dampen_smoothout_speed.GetFloat() * gpGlobals->frametime;
			if ( m_lastAutoAimValue >= 1.0f )
			{
				m_lastAutoAimValue = 1.0f;
			}
		}
*/
		return m_lastAutoAimValue;// No dampening.
	}
	else
	{
		// Help the user stay on target if the feature is enabled and the user
		// is not making a gross stick movement.
		if ( joy_autoaimdampen.GetFloat() > 0.0f && fabs(x) < joy_autoaimdampenrange.GetFloat() )
		{
			// Get the player
			C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
			if ( pLocalPlayer )
			{
				// Get the autoaim target
				if ( pLocalPlayer->m_Local.m_bAutoAimTarget )
				{
					return joy_autoaimdampen.GetFloat();
				}
			}
		}
	}

	return 1.0f;// No dampening.
}


//-----------------------------------------------
// This structure holds persistent information used
// to make decisions about how to modulate analog
// stick input.
//-----------------------------------------------
typedef struct 
{
	float	envelopeScale[2];
	bool	peggedAxis[2];
	bool	axisPeggedDir[2];
} envelope_t;

envelope_t	controlEnvelope[ MAX_SPLITSCREEN_PLAYERS ];

static bool IsJoystickPegged( float input, float otherAxis )
{

#if defined( _X360 )
	static float fPower = 1.25f;
#elif defined( _PS3 )
	static float fPower = 0.9f;
#else
	static float fPower = 0.9f; // pc
#endif


	float fMinimumVal = 0.01f; // accomodate dead zone
	float algorythmX = abs(input); 
	float algorythmY = MAX( abs(otherAxis),fMinimumVal ); 

#if defined( _PS3 )
	float fltempAlgorythmSample = MAX( algorythmX, algorythmY );
#else
	float fltempAlgorythmSample = pow(  pow(algorythmX,fPower)+pow(algorythmY,fPower),fPower); 
#endif

#if defined( _X360 )
	float flJoyAddititiveDistComparison = 0.98f;
#elif defined( _PS3 )
	float flJoyAddititiveDistComparison = 0.91f;
#else
	float flJoyAddititiveDistComparison = 0.94f;
#endif
	bool result = fltempAlgorythmSample >= flJoyAddititiveDistComparison;

	return result;
}

//-----------------------------------------------
// Response curve function specifically for the 
// 'look' analog stick.
//
// when AXIS == YAW, otherAxisValue contains the 
// value for the pitch of the control stick, and
// vice-versa.
//-----------------------------------------------
ConVar joy_pegged("joy_pegged", "0.75");// Once the stick is pushed this far, it's assumed pegged.
ConVar joy_virtual_peg("joy_virtual_peg", "0");
float CInput::ResponseCurveLookDefault( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	envelope_t &envelope = controlEnvelope[ MAX( nSlot, 0 ) ];
	float input = x;
	// float maxX = 1.0; used in prev algorithm 
	float flJoyDist = dist;//MAX( dist, MIN( joy_pegged.GetFloat(), sqrt(x*x + otherAxis*otherAxis) ) );

	bool bPegged = ( flJoyDist >= joy_pegged.GetFloat() ) || IsJoystickPegged( x, otherAxis );

	// Make X positive to make things easier, just remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	if ( otherAxis < 0.0f )
	{
		otherAxis *= -1;
	}

	if( axis == YAW && joy_virtual_peg.GetBool() )
	{
		if( x >= 0.95f )
		{
			// User has pegged the stick
			envelope.peggedAxis[axis] = true;
			envelope.axisPeggedDir[axis] = negative;
		}
		
		if( envelope.peggedAxis[axis] == true )
		{
			// User doesn't have the stick pegged on this axis, but they used to. 
			// If the stick is physically pegged, pretend this axis is still pegged.
			if( bPegged && negative == envelope.axisPeggedDir[axis] )
			{
				// If the user still has the stick physically pegged and hasn't changed direction on
				// this axis, keep pretending they have the stick pegged on this axis.
				x = 1.0f;
			}
			else
			{
				envelope.peggedAxis[axis] = false;
			}
		}
	}

	// Perform the two-stage mapping.
	float tmpDist = dist;
	if( tmpDist > joy_lowend.GetFloat() || x > joy_lowend_linear.GetFloat() )
	{
		tmpDist = RemapValClamped( tmpDist, joy_lowend.GetFloat(), 1.0f, joy_lowmap.GetFloat(), 1.0f );

		// Accelerate.
		if( envelope.envelopeScale[axis] < 1.0f )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > 1.0f )
			{
				envelope.envelopeScale[axis] = 1.0f;
			}
		}

		float delta = tmpDist - joy_lowmap.GetFloat();
		tmpDist = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);
	}
	else
	{
		// Shut off acceleration
		envelope.envelopeScale[axis] = 0.0f;

		tmpDist = RemapValClamped( tmpDist, 0.0f, joy_lowend.GetFloat(), 0.0f, joy_lowmap.GetFloat() ); 

		if( tmpDist > 0.0f && joy_display_input.GetBool() )
		{
			Msg("AXIS == %d : 2:  x = :%f, otherAxis = %f\n", axis, x, otherAxis );
		}
	}

	if ( dist > 0.01f )
	{
		float newX = x;
		if ( axis == YAW )
		{
			float input = x / dist;
			input = clamp( input, -1.0f, 1.0f );
			float theta = acos( input );
			newX = cos( theta ) * tmpDist;
			//Msg( "input: %f, theta:%f, x: %f\n", input, theta, newX );
		}
		else
		{
			float input = x / dist;
			input = clamp( input, -1.0f, 1.0f );
			float theta = asin( input );
			newX = sin( theta ) * tmpDist;
			//Msg( "input: %f, theta:%f, x: %f\n", input, theta, newX );
		}
		x = newX;
	}
	/*(
	// Perform the two-stage mapping.
	if( x > joy_lowend.GetFloat() )
	{
		float highmap = 1.0f - joy_lowmap.GetFloat();
		float xNormal = x - joy_lowend.GetFloat();
		float xNormalMax = maxX - joy_lowend.GetFloat();

		float factor = xNormal / ( 1.0f - joy_lowend.GetFloat() );
		float factorMax = xNormalMax / ( 1.0f - joy_lowend.GetFloat() );
		x = joy_lowmap.GetFloat() + (highmap * factor);
		maxX = joy_lowmap.GetFloat() + (highmap * factorMax);

		//if( x > 0.0f && joy_display_input.GetBool() )
		//{
		//	Msg("AXIS == %d : 1a:  x = :%f\n", axis, x );
		//}

		// Accelerate.
		if( envelope.envelopeScale[axis] < 1.0f )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > 1.0f )
			{
				envelope.envelopeScale[axis] = 1.0f;
			}
		}

		float delta = x - joy_lowmap.GetFloat();
		float deltaMax = maxX - joy_lowmap.GetFloat();
		x = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);
		maxX = joy_lowmap.GetFloat() + (deltaMax * envelope.envelopeScale[axis]);

		if( x > 0.0f && joy_display_input.GetBool() )
		{
			Msg("AXIS == %d : 1b:  x = :%f, otherAxis = %f\n", axis, x, otherAxis );
		}
	}
	else
	{
		// Shut off acceleration
		envelope.envelopeScale[axis] = 0.0f;
		float factor = x / joy_lowend.GetFloat();

		x = (joy_lowmap.GetFloat() * factor);

		if( x > 0.0f && joy_display_input.GetBool() )
		{
			Msg("AXIS == %d : 2:  x = :%f, otherAxis = %f\n", axis, x, otherAxis );
		}
	}
	*/
	x *= AutoAimDampening( input, axis, dist );

	//float flDiagDiff = abs(x - otherAxis);
	//x *= MIN( maxX, 1+(otherAxis) );

	if( x > 0.0f && joy_display_input.GetBool() )
	{
		Msg("AXIS == %d : In:%f Out:%f Frametime:%f\n", axis, input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

ConVar joy_accel_filter("joy_accel_filter", "0.2");// If the non-accelerated axis is pushed farther than this, then accelerate it, too.
ConVar joy_useNewAcecelMethod("joy_useNewAcecelMethod","1");
ConVar joy_useNewJoystickPegged( "joy_useNewJoystickPeggedTest", "0" );
float CInput::ResponseCurveLookAccelerated( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	envelope_t &envelope = controlEnvelope[ MAX( nSlot, 0 ) ];

	float input = x;

	float flJoyDist = ( sqrt(x*x + otherAxis * otherAxis) );
	bool bIsPegged = ( flJoyDist>= joy_pegged.GetFloat() );
	if ( joy_useNewAcecelMethod.GetBool() || joy_useNewJoystickPegged.GetBool() )
	{
		bIsPegged = IsJoystickPegged( input, otherAxis );
	}

	float curvParam = joy_gamma.GetFloat() * 2.0f - 1.0f;

	// Make X positive to make arithmetic easier for the rest of this function, and
	// remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	// Perform the two-stage mapping.
	bool bDoAcceleration = false;// Assume we won't accelerate the input

	if( bIsPegged && x > joy_accel_filter.GetFloat() )
	{
		// Accelerate this axis, since the stick is pegged and 
		// this axis is pressed farther than the acceleration filter
		// Take the lowmap value, or the input, whichever is higher, since 
		// we don't necesarily know whether this is the axis which is pegged
		if( !joy_no_accel_jump.GetBool() )
		{
			x = MAX( joy_lowmap.GetFloat(), x );
		}

		bDoAcceleration = true;
	}
	else
	{
		// Joystick is languishing in the low-end, turn off acceleration.
		envelope.envelopeScale[axis] = 0.0f;
		float factor = x / joy_lowend.GetFloat();
		if ( joy_useNewAcecelMethod.GetBool() )
		{
			float divisor = factor * curvParam + 1;
			//ReleaseAssert(divisor);
			if (divisor != 0.0f)
				x = joy_lowmap.GetFloat() * ( factor * ( curvParam + 1 ) / divisor );		
		}
		else
		{
			x = joy_lowmap.GetFloat() * factor;
		}
	}

	if( bDoAcceleration )
	{
		float flMax = joy_accelmax.GetFloat();
		if( envelope.envelopeScale[axis] < flMax && !joy_useNewAcecelMethod.GetBool() )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > flMax )
			{
				envelope.envelopeScale[axis] = flMax;
			}
		}
		float delta = x - joy_lowmap.GetFloat();
		x = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);

		if ( joy_useNewAcecelMethod.GetBool() )
		{
			float factor = x / joy_lowend.GetFloat();
			float divisor = factor * curvParam + 1;
			//ReleaseAssert(divisor);
			float minx = 0.0f;
			if (divisor != 0.0f)
				minx = joy_lowmap.GetFloat() * ( factor * ( curvParam + 1 ) / divisor );

			x = MAX( x, minx );		
		}
	}

	x *= AutoAimDampening( input, axis, dist );

	if( axis == YAW && input != 0.0f && joy_display_input.GetBool() )
	{
		Msg("In:%f Out:%f Frametime:%f\n", input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

//-----------------------------------------------
//-----------------------------------------------
float CInput::ResponseCurveLookPolynomial( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	// Make X positive to make things easier, just remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	if ( otherAxis < 0.0f )
	{
		otherAxis *= -1;
	}

	envelope_t &envelope = controlEnvelope[ MAX( nSlot, 0 ) ];
	float input = x;
	float scale = MIN( 1.0f, sqrt(x*x+otherAxis*otherAxis) );
	bool bPegged = ( scale >= joy_pegged.GetFloat() ) || IsJoystickPegged( x, otherAxis );

	if( axis == YAW && joy_virtual_peg.GetBool() )
	{
		if( x >= 0.95f )
		{
			// User has pegged the stick
			envelope.peggedAxis[axis] = true;
			envelope.axisPeggedDir[axis] = negative;
		}

		if( envelope.peggedAxis[axis] == true )
		{
			// User doesn't have the stick pegged on this axis, but they used to. 
			// If the stick is physically pegged, pretend this axis is still pegged.
			if( bPegged && negative == envelope.axisPeggedDir[axis] )
			{
				// If the user still has the stick physically pegged and hasn't changed direction on
				// this axis, keep pretending they have the stick pegged on this axis.
				x = 1.0f;
			}
			else
			{
				envelope.peggedAxis[axis] = false;
			}
		}
	}

	//if ( bPegged )
	//{
	//	x = 1.0f;
	//}

	float flMaxOutput = 1;
	//float flMaxAccel = joy_accelmax.GetFloat();
	if( envelope.envelopeScale[axis] < 1.0f )
	{
		envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
		if( envelope.envelopeScale[axis] > 1.0f )
		{
			envelope.envelopeScale[axis] = 1.0f;
		}
	}
	
	//x = joy_curvepoint_3.GetFloat() + (delta * envelope.envelopeScale[axis]);

	if ( x > 0 && scale > 0 )
	{
		x = (joy_curvepoint_end.GetFloat()*sqrt(x*x*x*x*x*x) + joy_curvepoint_4.GetFloat()*sqrt(x*x*x*x*x) + joy_curvepoint_3.GetFloat()*sqrt(x*x*x*x) + joy_curvepoint_2.GetFloat()*sqrt(x*x*x) + joy_curvepoint_1.GetFloat()*sqrt(x*x) + 0.00001f*sqrt(x)) * joy_accelscalepoly.GetFloat();
		flMaxOutput = (joy_curvepoint_end.GetFloat() + joy_curvepoint_4.GetFloat() + joy_curvepoint_3.GetFloat() + joy_curvepoint_2.GetFloat() + joy_curvepoint_1.GetFloat() + 0.00001f) * joy_accelscalepoly.GetFloat();

		if( x > 0.0f && joy_display_input.GetBool() )
		{
			//Msg("scale = %f........\n", scale );
			Msg("AXIS == %d : 1b:  x = :%f, scale = %f\n", axis, x, scale );
		}

		x *= 1+((otherAxis+scale)*0.75);
	}
	else
	{
		envelope.envelopeScale[axis] = 0.0f;
	}


	// account for pushing diagonally
	float flDiagonal = 0;
	/*
	if ( x > otherAxis )
		flDiagonal = (scale-otherAxis) * (otherAxis/x);
	else if ( otherAxis > x )
		flDiagonal = (scale-x) * (x/otherAxis);

	x = MIN( flMaxOutput, x + (flDiagonal*scale) );
	*/
	//if ( bPegged )
	//	x = flMaxOutput;

	x *= envelope.envelopeScale[axis];
	x = MIN( flMaxOutput, x );

	if( x > 0.0f && joy_display_input.GetBool() )
		Msg("flDiagonal == %f : otherAxis = :%f : x = :%f, flMaxOutput = %f\n", flDiagonal, otherAxis, x, flMaxOutput );

	x *= AutoAimDampening( input, axis, dist );

	if( x > 0.0f && joy_display_input.GetBool() )
	{
		Msg("AXIS == %d : flJoyDist:%f In:%f Out:%f Frametime:%f\n", axis, scale, input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

//-----------------------------------------------
//-----------------------------------------------
float CInput::ResponseCurveLook( int nSlot, int curve, float x, int axis, float otherAxis, float dist, float frametime )
{
	switch( curve )
	{
	case 1://Promotion of acceleration
		return ResponseCurveLookAccelerated( nSlot, x, axis, otherAxis, dist, frametime );
		break;

	case 2://Modern
		return ResponseCurveLookPolynomial( nSlot, x, axis, otherAxis, dist, frametime );
		break;

	default:
		return ResponseCurveLookDefault( nSlot, x, axis, otherAxis, dist, frametime );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Advanced joystick setup
//-----------------------------------------------------------------------------
void CInput::Joystick_Advanced( bool bSilent )
{
	m_fJoystickAdvancedInit = true;

	// called whenever an update is needed
	int	i;
	DWORD dwTemp;

	if ( IsGameConsole() )
	{
		// Xbox always uses a joystick
		in_joystick.SetValue( 1 );
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		PerUserInput_t &user = GetPerUser();

		// Initialize all the maps
		for ( i = 0; i < MAX_JOYSTICK_AXES; i++ )
		{
			user.m_rgAxes[i].AxisMap = GAME_AXIS_NONE;
			user.m_rgAxes[i].ControlMap = JOY_ABSOLUTE_AXIS;
		}

		if ( !joy_advanced.GetBool() )
		{
			// default joystick initialization
			// 2 axes only with joystick control
			user.m_rgAxes[JOY_AXIS_X].AxisMap = GAME_AXIS_YAW;
			user.m_rgAxes[JOY_AXIS_Y].AxisMap = GAME_AXIS_FORWARD;
		}
		else
		{
			if ( !bSilent && 
				hh == 0 && Q_stricmp( joy_name.GetString(), "joystick") )
			{
				// notify user of advanced controller
				Msg( "Using joystick '%s' configuration\n", joy_name.GetString() );
			}

			static SplitScreenConVarRef s_joy_movement_stick( "joy_movement_stick" );

			bool bJoyMovementStick = s_joy_movement_stick.GetBool( hh );

			// advanced initialization here
			// data supplied by user via joy_axisn cvars
			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisu.GetInt() : (DWORD)joy_advaxisx.GetInt();
			user.m_rgAxes[JOY_AXIS_X].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_X].ControlMap = dwTemp & JOY_RELATIVE_AXIS;
	
			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisr.GetInt() : (DWORD)joy_advaxisy.GetInt();
			user.m_rgAxes[JOY_AXIS_Y].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_Y].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = (DWORD)joy_advaxisz.GetInt();
			user.m_rgAxes[JOY_AXIS_Z].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_Z].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisy.GetInt() : (DWORD)joy_advaxisr.GetInt();
			user.m_rgAxes[JOY_AXIS_R].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_R].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisx.GetInt() : (DWORD)joy_advaxisu.GetInt();
			user.m_rgAxes[JOY_AXIS_U].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_U].ControlMap = dwTemp & JOY_RELATIVE_AXIS;
	
			dwTemp = (DWORD)joy_advaxisv.GetInt();
			user.m_rgAxes[JOY_AXIS_V].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_V].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			if ( !bSilent )
			{
				Msg( "Advanced joystick settings initialized for joystick %d\n------------\n", hh + 1 );
				DescribeJoystickAxis( hh, "x axis", &user.m_rgAxes[JOY_AXIS_X] );
				DescribeJoystickAxis( hh, "y axis", &user.m_rgAxes[JOY_AXIS_Y] );
				DescribeJoystickAxis( hh, "z axis", &user.m_rgAxes[JOY_AXIS_Z] );
				DescribeJoystickAxis( hh, "r axis", &user.m_rgAxes[JOY_AXIS_R] );
				DescribeJoystickAxis( hh, "u axis", &user.m_rgAxes[JOY_AXIS_U] );
				DescribeJoystickAxis( hh, "v axis", &user.m_rgAxes[JOY_AXIS_V] );
			}
		}
	}

#if defined( SWARM_DLL ) || defined( PORTAL )
	// Load the xbox controller cfg file if it hasn't been loaded.
	if ( in_joystick.GetBool() )
	{
		if ( joy_xcontroller_cfg_loaded.GetBool() == false )
		{
			engine->ClientCmd( "exec joy_configuration" PLATFORM_EXT ".cfg" );
			joy_xcontroller_cfg_loaded.SetValue( 1 );
		}
	}
	else if ( joy_xcontroller_cfg_loaded.GetBool() )
	{
		engine->ClientCmd( "exec undo360controller.cfg" );
		joy_xcontroller_cfg_loaded.SetValue( 0 );
	}
#else // !SWARM_DLL && !PORTAL

#if !defined( _PS3 )

	// [Forrest] For CStrike 1.5 we want to load 360controller.cfg on Xbox as well as PC.
	// If we have an xcontroller on the PC, load the cfg file if it hasn't been loaded.
	// [Forrest] engine->ClientCmd didn't go through (FCVAR_CLIENTCMD_CAN_EXECUTE prevented running command).
	// Changed it to engine->ClientCmd_Unrestricted.
	ConVarRef var( "joy_xcontroller_found" );
	if ( var.IsValid() && var.GetBool() && in_joystick.GetBool() )
	{
		if ( joy_xcontroller_cfg_loaded.GetBool() == false )
		{
			for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
			{
				engine->ClientCmd_Unrestricted( "exec controller" PLATFORM_EXT ".cfg", false, i, false );
			}
			joy_xcontroller_cfg_loaded.SetValue( 1 );
		}
	}
	else if ( joy_xcontroller_cfg_loaded.GetBool() )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			engine->ClientCmd_Unrestricted( "exec undo360controller.cfg", false, i, false );
		}
		joy_xcontroller_cfg_loaded.SetValue( 0 );
	}

#endif

#endif // SWARM_DLL
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CInput::DescribeAxis( int index )
{
// $FIXME(hpe) the string command syntax differs from xbla; need to verify 
	switch ( index )
	{
	case GAME_AXIS_FORWARD:
		return "forward";
	case GAME_AXIS_PITCH:
		return "pitch";
	case GAME_AXIS_SIDE:
		return "strafe";
	case GAME_AXIS_YAW:
		return "yaw";
	case GAME_AXIS_NONE:
	default:
		return "n/a";
	}

	return "n/a";
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *axis - 
//			*mapping - 
//-----------------------------------------------------------------------------
void CInput::DescribeJoystickAxis( int nJoystick, char const *axis, joy_axis_t *mapping )
{
	if ( !mapping->AxisMap )
	{
		Msg( "joy%d %s:  unmapped\n", nJoystick + 1, axis );
	}
	else
	{
		Msg( "joy%d %s:  %s (%s)\n",
			nJoystick + 1,
			axis, 
			DescribeAxis( mapping->AxisMap ),
			mapping->ControlMap != 0 ? "relative" : "absolute" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Allow joystick to issue key events
// Not currently used - controller button events are pumped through the windprocs. KWD
//-----------------------------------------------------------------------------
void CInput::ControllerCommands( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Scales the raw analog value to lie withing the axis range (full range - deadzone )
//-----------------------------------------------------------------------------
float CInput::ScaleAxisValue( const float axisValue, const float axisThreshold )
{
	// Xbox scales the range of all axes in the inputsystem. PC can't do that because each axis mapping
	// has a (potentially) unique threshold value.  If all axes were restricted to a single threshold
	// as they are on the Xbox, this function could move to inputsystem and be slightly more optimal.
	float result = 0.f;
	if ( IsPC() )
	{
		if ( axisValue < -axisThreshold )
		{
			result = ( axisValue + axisThreshold ) / ( MAX_BUTTONSAMPLE - axisThreshold );
		}
		else if ( axisValue > axisThreshold )
		{
			result = ( axisValue - axisThreshold ) / ( MAX_BUTTONSAMPLE - axisThreshold );
		}
	}
	else
	{
		result =  axisValue * ( 1.f / MAX_BUTTONSAMPLE );
	}

	return result;
}


void CInput::Joystick_SetSampleTime(float frametime)
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		m_PerUser[ i ].m_flRemainingJoystickSampleTime = frametime;
	}
}

float CInput::Joystick_GetPitch( void )
{
	if ( !ControllerModeActive() )
		return 0.0f;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	return user.m_flPreviousJoystickPitch;
}

float CInput::Joystick_GetYaw( void )
{
	if ( !ControllerModeActive() )
		return 0.0f;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	return user.m_flPreviousJoystickYaw;
}

void CInput::Joystick_Querry( float &forward, float &side, float &pitch, float &yaw )
{
	bool bAbsoluteYaw, bAbsolutePitch;
	JoyStickSampleAxes( forward, side, pitch, yaw, bAbsoluteYaw, bAbsolutePitch );
}

void CInput::Joystick_ForceRecentering( int nStick, bool bSet /*= true*/ )
{
	if ( nStick < 0 || nStick > 1 )
		return;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	user.m_bForceJoystickRecentering[ nStick ] = bSet;
}

extern void IN_ForceSpeedUp( );
extern void IN_ForceSpeedDown( );


bool CInput::ControllerModeActive( void )
{
	return ( in_joystick.GetInt() != 0 && m_bControllerMode );
}

//--------------------------------------------------------------------
// See if we want to use the joystick
//--------------------------------------------------------------------
bool CInput::JoyStickActive()
{
	// verify joystick is available and that the user wants to use it
	if ( !in_joystick.GetInt() || 0 == inputsystem->GetJoystickCount() )
		return false; 

	// Skip out if vgui or gameui is active
	if ( !g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
		return false;

	return true;
}

//--------------------------------------------------------------------
// Reads joystick values
//--------------------------------------------------------------------
void CInput::JoyStickSampleAxes( float &forward, float &side, float &pitch, float &yaw, bool &bAbsoluteYaw, bool &bAbsolutePitch )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	struct axis_t
	{
		float	value;
		int		controlType;
	};
	axis_t gameAxes[ MAX_GAME_AXES ];
	memset( &gameAxes, 0, sizeof(gameAxes) );

	// Get each joystick axis value, and normalize the range
	for ( int i = 0; i < MAX_JOYSTICK_AXES; ++i )
	{
		if ( GAME_AXIS_NONE == user.m_rgAxes[i].AxisMap )
			continue;

		float fAxisValue = inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), i ) );

		if ( joy_wwhack2.GetInt() != 0 )
		{
			// this is a special formula for the Logitech WingMan Warrior
			// y=ax^b; where a = 300 and b = 1.3
			// also x values are in increments of 800 (so this is factored out)
			// then bounds check result to level out excessively high spin rates
			float fTemp = 300.0 * pow(abs(fAxisValue) / 800.0, 1.3);
			if (fTemp > 14000.0)
				fTemp = 14000.0;
			// restore direction information
			fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
		}

		unsigned int idx = user.m_rgAxes[i].AxisMap;
		gameAxes[idx].value = fAxisValue;
		gameAxes[idx].controlType = user.m_rgAxes[i].ControlMap;
	}

	// Before these axes are allowed to return values must bring them back to mostly centered
	if ( user.m_bForceJoystickRecentering[ 0 ] )
	{
		if ( fabsf( gameAxes[GAME_AXIS_FORWARD].value ) < 0.1f && fabsf( gameAxes[GAME_AXIS_SIDE].value ) < 0.1f )
		{
			user.m_bForceJoystickRecentering[ 0 ] = false;
		}

		gameAxes[GAME_AXIS_FORWARD].value = 0.0f;
		gameAxes[GAME_AXIS_SIDE].value = 0.0f;
	}

	// Before these axes are allowed to return values must bring them back to mostly centered
	if ( user.m_bForceJoystickRecentering[ 1 ] )
	{
		if ( fabsf( gameAxes[GAME_AXIS_PITCH].value ) < 0.1f && fabsf( gameAxes[GAME_AXIS_YAW].value ) < 0.1f )
		{
			user.m_bForceJoystickRecentering[ 1 ] = false;
		}

		gameAxes[GAME_AXIS_PITCH].value = 0.0f;
		gameAxes[GAME_AXIS_YAW].value = 0.0f;
	}

	// Re-map the axis values if necessary, based on the joystick configuration
	if ( (joy_advanced.GetInt() == 0) && (in_jlook.GetPerUser( nSlot ).state & 1) )
	{
		// user wants forward control to become pitch control
		gameAxes[GAME_AXIS_PITCH] = gameAxes[GAME_AXIS_FORWARD];
		gameAxes[GAME_AXIS_FORWARD].value = 0;

		// if mouse invert is on, invert the joystick pitch value
		// Note: only absolute control support here - joy_advanced = 0
		if ( m_pitch->GetFloat() < 0.0 )
		{
			gameAxes[GAME_AXIS_PITCH].value *= -1;
		}
	}

	if ( (in_strafe.GetPerUser( nSlot ).state & 1) || lookstrafe.GetFloat() && (in_jlook.GetPerUser( nSlot ).state & 1) )
	{
		// user wants yaw control to become side control
		gameAxes[GAME_AXIS_SIDE] = gameAxes[GAME_AXIS_YAW];
		gameAxes[GAME_AXIS_YAW].value = 0;
	}

	static SplitScreenConVarRef joy_movement_stick("joy_movement_stick");
	if( joy_movement_stick.IsValid() && joy_movement_stick.GetInt( nSlot ) == 2 )
	{
		axis_t swap = gameAxes[GAME_AXIS_YAW];
		gameAxes[GAME_AXIS_YAW] = gameAxes[GAME_AXIS_SIDE];
		gameAxes[GAME_AXIS_SIDE] = swap;
	}

	forward	= ScaleAxisValue( gameAxes[GAME_AXIS_FORWARD].value, MAX_BUTTONSAMPLE * joy_forwardthreshold.GetFloat() );
	side	= ScaleAxisValue( gameAxes[GAME_AXIS_SIDE].value, MAX_BUTTONSAMPLE * joy_sidethreshold.GetFloat()  );
	pitch	= ScaleAxisValue( gameAxes[GAME_AXIS_PITCH].value, MAX_BUTTONSAMPLE * joy_pitchthreshold.GetFloat()  );
	yaw		= ScaleAxisValue( gameAxes[GAME_AXIS_YAW].value, MAX_BUTTONSAMPLE * joy_yawthreshold.GetFloat()  );

	bAbsoluteYaw = ( JOY_ABSOLUTE_AXIS == gameAxes[GAME_AXIS_YAW].controlType );
	bAbsolutePitch = ( JOY_ABSOLUTE_AXIS == gameAxes[GAME_AXIS_PITCH].controlType );

	// If we're inverting our joystick, do so
	static SplitScreenConVarRef s_joy_inverty( "joy_inverty" );
	bool isInverted = s_joy_inverty.IsValid() && s_joy_inverty.GetBool( nSlot );
	if ( !isInverted )
	{
		pitch *= -1.0f;
	}
}


//--------------------------------------------------------------------
// drive yaw, pitch and move like a screen relative platformer game
//--------------------------------------------------------------------
void CInput::JoyStickThirdPersonPlatformer( CUserCmd *cmd, float &forward, float &side, float &pitch, float &yaw )
{
	// Get starting angles
	QAngle viewangles;
	engine->GetViewAngles( viewangles );

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( forward || side )
	{
		// apply turn control [ YAW ]
		// factor in the camera offset, so that the move direction is relative to the thirdperson camera
		viewangles[ YAW ] = RAD2DEG(atan2(-side, -forward)) + user.m_vecCameraOffset[ YAW ];
		engine->SetViewAngles( viewangles );

		// apply movement
		Vector2D moveDir( forward, side );
		cmd->forwardmove += moveDir.Length() * cl_forwardspeed.GetFloat();
	}

	if ( pitch || yaw )
	{
		static SplitScreenConVarRef s_joy_yawsensitivity( "joy_yawsensitivity" );
		static SplitScreenConVarRef s_joy_pitchsensitivity( "joy_pitchsensitivity" );

		// look around with the camera
		user.m_vecCameraOffset[ PITCH ] += pitch * s_joy_pitchsensitivity.GetFloat( nSlot );
		user.m_vecCameraOffset[ YAW ]   += yaw * s_joy_yawsensitivity.GetFloat( nSlot );
	}

	if ( forward || side || pitch || yaw )
	{
		// update the ideal pitch and yaw
		cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );
		cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );
	}
}

//-----------------------------------------------
// Turns viewangles based on sampled joystick
//-----------------------------------------------
void CInput::JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch )
{
	// Get starting angles
	QAngle viewangles;
	engine->GetViewAngles( viewangles );

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	// PerUserInput_t &user = GetPerUser( nSlot );

	static SplitScreenConVarRef s_joy_yawsensitivity( "joy_yawsensitivity" );
	static SplitScreenConVarRef s_joy_pitchsensitivity( "joy_pitchsensitivity" );

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	bool bSpecThird = pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_CHASE;

	Vector2D move( yaw, pitch );
	float dist = move.Length();
	bool	bVariableFrametime = joy_variable_frametime.GetBool();
	float	lookFrametime = bVariableFrametime ? frametime : gpGlobals->frametime;
	float   aspeed = lookFrametime * GetHud().GetFOVSensitivityAdjust();
	if ( bSpecThird )
		aspeed *= 1.5;

	// No quick turn so just sample the joystick
	float angle = 0.0f;

	// Sample the joystick.
	if ( bVariableFrametime || frametime != gpGlobals->frametime )
	{
		if ( bAbsoluteYaw )
		{
			float fAxisValue = ResponseCurveLook( nSlot, joy_response_look.GetInt(), yaw, YAW, pitch, dist, lookFrametime );
			angle = fAxisValue * s_joy_yawsensitivity.GetFloat( nSlot ) * aspeed * cl_yawspeed.GetFloat();
		}
		else
		{
			angle = yaw * s_joy_yawsensitivity.GetFloat( nSlot ) * aspeed * 180.0;
		}
	}

	// Update and apply turn control.  This may produce a new angle if we're doing a quick turn.
	angle = UpdateAndGetQuickTurnYaw( nSlot, lookFrametime, angle );

	viewangles[YAW] += angle;
	cmd->mousedx = angle;

	// apply look control
	if ( in_jlook.GetPerUser( nSlot ).state & 1 )
	{
		float angle = 0;
		if ( bVariableFrametime || frametime != gpGlobals->frametime )
		{
			if ( bAbsolutePitch )
			{
				float fAxisValue = ResponseCurveLook( nSlot, joy_response_look_pitch.GetInt(), pitch, PITCH, yaw, dist, lookFrametime );
				angle = fAxisValue * s_joy_pitchsensitivity.GetFloat( nSlot ) * aspeed * cl_pitchspeed.GetFloat();
			}
			else
			{
				angle = pitch * s_joy_pitchsensitivity.GetFloat( nSlot ) * aspeed * 180.0;
			}
		}
		viewangles[PITCH] += angle;
		cmd->mousedy = angle;
		view->StopPitchDrift();
		if ( pitch == 0.f && lookspring.GetFloat() == 0.f )
		{
			// no pitch movement
			// disable pitch return-to-center unless requested by user
			// *** this code can be removed when the lookspring bug is fixed
			// *** the bug always has the lookspring feature on
			view->StopPitchDrift();
		}
	}

	viewangles[PITCH] = clamp( viewangles[ PITCH ], -cl_pitchup.GetFloat(), cl_pitchdown.GetFloat() );
	engine->SetViewAngles( viewangles );
}


//---------------------------------------------------------------------
// Calculates strafe and forward/back motion based on sampled joystick
//---------------------------------------------------------------------
void CInput::JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove )
{
	joyForwardMove = joySideMove = 0.0f;

	// apply forward and side control
	if ( joy_response_move.GetInt() > 6 && joy_circle_correct.GetBool() )
	{
		// ok the 360 controller is scaled to a circular area.  our movement is scaled to the square two axis, 
		// so diagonal needs to be scaled properly to full speed.

		bool bInWalk = true;
		float scale = MIN(1.0f,sqrt(forward*forward+side*side));
		if ( scale > 0.01f )
		{
			float val;
			if ( scale > joy_sensitive_step2.GetFloat() )
			{
				bInWalk = false;
			}
			float scaledVal = ResponseCurve( joy_response_move.GetInt(), scale, PITCH, fabsf( joy_forwardsensitivity.GetFloat() ) );
			val = scaledVal * ( ( forward * Sign( joy_forwardsensitivity.GetFloat() ) ) / scale );
			joyForwardMove += val * cl_forwardspeed.GetFloat();

			scaledVal = ResponseCurve( joy_response_move.GetInt(), scale, PITCH, fabsf( joy_sidesensitivity.GetFloat() ) );
			val = scaledVal  * ( ( side * Sign( joy_sidesensitivity.GetFloat() ) ) / scale );
			joySideMove += val * cl_sidespeed.GetFloat();

			// big hack here, if we are not moving past the joy_sensitive_step2 thresh hold then walk.
			if ( bInWalk )
			{
				IN_ForceSpeedDown();
			}
			else
			{
				IN_ForceSpeedUp();
			}
		}
		else
		{
			IN_ForceSpeedUp();
		}
	}
	else
	{
		// apply forward and side control
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		int iResponseCurve = 0;
		if ( pLocalPlayer && pLocalPlayer->IsInAVehicle() )
		{
			iResponseCurve = pLocalPlayer->GetVehicle() ? pLocalPlayer->GetVehicle()->GetJoystickResponseCurve() : joy_response_move_vehicle.GetInt();
		}
		else
		{
			iResponseCurve = joy_response_move.GetInt();
		}	

		float val = ResponseCurve( iResponseCurve, forward, PITCH, joy_forwardsensitivity.GetFloat() );
		joyForwardMove	+= val * cl_forwardspeed.GetFloat();
		val = ResponseCurve( iResponseCurve, side, YAW, joy_sidesensitivity.GetFloat() );
		joySideMove		+= val * cl_sidespeed.GetFloat();
	}
}

// expects a -1.0 - 1.0 value
// returns a 0.0 - 1.2 value, signed to match the input
float CInput::HandleMotionControllerInputSmoothing( float flDeadZonePct, float val )
{	
	bool isPositive = val > 0.0f;
	float absVal = abs(val);
	if ( absVal <= flDeadZonePct )
		return 0.0f;

	// Allow player to point off the screen if they've made the dead zone the size of the screen.
	float flBandSize = mc_accel_band_size.GetFloat();
	float normalizedAfterDeadzone = (absVal - flDeadZonePct) / flBandSize;
	
	float result = 0.0f;
	if ( normalizedAfterDeadzone > mc_turnPctPegged.GetFloat() )
	{
		// in our high acceleration zone, bump up the value
		result = mc_turnPctPeggedMultiplier.GetFloat();
	}
	else
	{
		// low acceleration
		// X*X method
		switch (mc_turn_curve.GetInt() )
		{
		case 0:
			result = normalizedAfterDeadzone;
			break;
		case 1:
			result = normalizedAfterDeadzone * normalizedAfterDeadzone;
			break;
		case 2:
			result = normalizedAfterDeadzone * normalizedAfterDeadzone * normalizedAfterDeadzone;
			break;

		}
	}	

	result = isPositive ? result : result * -1.0f;
	return result;
}

//-----------------------------------------------------------------------------
// Purpose: Apply motion controller to CUserCmd creation
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CInput::MotionControllerMove( float frametime, CUserCmd *cmd )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();
	Assert( pPlayer );

	// [dkorus] make sure our max turn rate is based on our zoom level sensitivity
	float flDeadZonePct = mc_dead_zone_radius.GetFloat();
	float flScreenClamp = mc_screen_clamp.GetFloat();

	int width=0, height=0;
	materials->GetBackBufferDimensions( width, height );

	float fX = inputsystem->GetMotionControllerPosX();
	float fY = inputsystem->GetMotionControllerPosY();

	static float s_lastCursorValueX = fX;
	static float s_lastCursorValueY = fY;

	static QAngle s_referenceDirection;
	static float s_FOVSensitivityAdjust = 1.0f;
	static float s_FOVOffsetX = 0.0f;
	static float s_FOVOffsetY = 0.0f;
	static float s_TargetFOVOffsetX = 0.0f;
	static float s_TargetFOVOffsetY = 0.0f;

	// Don't consider pointer input unless we're actively in the game.  Otherwise our view will change outside of when we want it to.
	int iObserverMode = pPlayer->GetObserverMode();
	bool ignorePointerInput = ( iObserverMode == OBS_MODE_DEATHCAM || iObserverMode == OBS_MODE_FREEZECAM );

#if defined( INCLUDE_SCALEFORM )

	// If we're in the pause menu, then lock the cursor to the screen.
	if ( g_pScaleformUI->SlotDeniesInputToGame( SF_SS_SLOT( nSlot ) )  )
	{
		ignorePointerInput = true;
	}

#endif
	
	// If we're in the pause menu, then lock the cursor to the screen.
	if ( ignorePointerInput  )
	{
		// Move this to center screen.
		fX = s_lastCursorValueX * 0.85f;
		fY = s_lastCursorValueY * 0.85f;
	}

	bool bLookingAtTarget = pPlayer->IsCursorOnAutoAimTarget();

	// Free moving cursor dampening code.
	static float s_fDampeningValue = 0.0f;  // s_fDampeningValue: 0.0 don't dampen, 1.0 fully locked in place.
	float fTargetDampening = 0.0f;
	if ( bLookingAtTarget )
	{
		fTargetDampening = mc_max_dampening.GetFloat();
	}
	// This little bit of code gives us a frame rate independent blend value.
	float blend_t = 1.0f - pow( mc_dampening_blend_amount.GetFloat(), frametime );
	s_fDampeningValue += ( fTargetDampening - s_fDampeningValue )*blend_t;


	// Turn Dampening code.
	static float s_fTurnDampeningValue = 0.0f;
	float fTargetTurnDampening = 0.0f;
	if ( bLookingAtTarget )
	{
		fTargetTurnDampening = mc_max_turn_dampening.GetFloat();
	}
	// This little bit of code gives us a frame rate independent blend value.
	blend_t = 1.0f - pow( mc_turn_dampening_blend_amount.GetFloat(), frametime );
	s_fTurnDampeningValue += ( fTargetTurnDampening - s_fTurnDampeningValue )*blend_t;

	// handle turning the view with the motion controller
	QAngle currentViewAngles;
	engine->GetViewAngles( currentViewAngles );

	if ( mc_zoomed_aim_style.GetInt() == 1 )
	{
		if ( !pPlayer->m_bIsScoped )
		{
			// Only set the reference direction to the current view when we are not zoomed in.
			s_referenceDirection = currentViewAngles;
		}
	}
	else
	{
		s_referenceDirection = currentViewAngles;
	}

	CWeaponCSBase *pWeapon = ( CWeaponCSBase* )pPlayer->GetActiveWeapon();

	float fTurnDampeningMultiplier = 1.0f;
	if ( pWeapon )
	{
		if ( mc_zoomed_aim_style.GetInt() == 1 )
		{
			// For non scoped weapons like the bomb and knife, only do clamping if we're not scoped.
			if ( !pWeapon->WantReticleShown() && !pPlayer->m_bIsScoped )
			{
				flScreenClamp = 0.0f;
				flDeadZonePct = mc_zoomed_out_dead_zone_radius.GetFloat();
			}
		}
		else
		{
			// Lock reticle if we're using a scoped weapon with our motion controller.
			// We do this by setting the screen clamp to zero so that we move like with an analog stick.
			// We also want to lock when we have a sniper rifle because the reticule will not show up.
			if ( !pWeapon->WantReticleShown() || 
				( mc_always_lock_ret_on_zoom.GetBool() && pPlayer->m_bIsScoped ) )
			{
				flScreenClamp = 0.0f;

				// If in addition to not showing the reticule we are also zoomed in, apply dampening to turning.
				if ( pPlayer->m_bIsScoped )
				{
					// If we're zoomed in, we want to apply the dampening to turning to help lock onto the enemies.
					fTurnDampeningMultiplier = (1.0f - s_fTurnDampeningValue);
					// Zoomed in weapons get a zero dead zone to make aiming smoother.
					flDeadZonePct = 0.0f;
				}
				else
				{
					// We add a bit of deadzone when zoomed out so that world navigation is easier.
					// It also helps to keep your aim on your target after firing if the weapon pops back to zoomed out.
					flDeadZonePct = mc_zoomed_out_dead_zone_radius.GetFloat();
				}
			}

		}

	}


	// We increase the deadzone size for pitch by 1.5
	const float flPitchDeadZoneScale = 1.5f;
	float pitchDeadZone = clamp( flDeadZonePct * flPitchDeadZoneScale, 0.0f, 0.9f );

	// Calculate the pitch and yaw deltas.
	const float flCurrFOVScale = GetHud().GetFOVSensitivityAdjust();

	if ( s_FOVSensitivityAdjust != flCurrFOVScale )
	{
		if ( flCurrFOVScale == 1.0f )
		{
			// Set the offset to put the cursor in the middle of the screen.
			s_FOVOffsetX = -s_lastCursorValueX * flCurrFOVScale;
			s_FOVOffsetY = -s_lastCursorValueY * flCurrFOVScale;

			// Now we want to zero the offset over time.
			s_TargetFOVOffsetX = 0.0f;
			s_TargetFOVOffsetY = 0.0f;
		}
		else
		{
			// We're zooming in on an offset position, so we need to an offset compensation here.
			// save off the offsets from the last cursor value.
			// The last cursor position on the screen was
			float lastAdjustedX = s_lastCursorValueX * s_FOVSensitivityAdjust + s_FOVOffsetX;
			float lastAdjustedY = s_lastCursorValueY * s_FOVSensitivityAdjust + s_FOVOffsetY;

			// With the new FOV scale, the offsets to give us the same position is:
			s_TargetFOVOffsetX = lastAdjustedX - s_lastCursorValueX * flCurrFOVScale;
			s_TargetFOVOffsetY = lastAdjustedY - s_lastCursorValueY * flCurrFOVScale;

			// Set the offset immediately since we don't need or want to blend to this.
			// The blending is for zooming out when the cursor will not be pointing where we were looking.
			s_FOVOffsetX = s_TargetFOVOffsetX;
			s_FOVOffsetY = s_TargetFOVOffsetY;
		}
		s_FOVSensitivityAdjust = flCurrFOVScale;
	}

	blend_t = 1.0f - pow( mc_zoom_out_cursor_offset_blend.GetFloat(), frametime );
	s_FOVOffsetX += ( s_TargetFOVOffsetX - s_FOVOffsetX ) * blend_t;
	s_FOVOffsetY += ( s_TargetFOVOffsetY - s_FOVOffsetY ) * blend_t;



	float flMaxTurnRate = mc_max_yawrate.GetFloat() * s_FOVSensitivityAdjust; 
	float flMaxPitchRate = mc_max_pitchrate.GetFloat() * s_FOVSensitivityAdjust;
	float smoothedX = HandleMotionControllerInputSmoothing( flDeadZonePct, fX );
	float smoothedY = HandleMotionControllerInputSmoothing( pitchDeadZone, fY );
	float deltaYaw = -smoothedX * flMaxTurnRate * frametime * fTurnDampeningMultiplier;
	float deltaPitch = -smoothedY * flMaxPitchRate * frametime * fTurnDampeningMultiplier;

	// Update and apply turn control.  This may produce a new angle if we're doing a quick turn.
	deltaYaw = UpdateAndGetQuickTurnYaw( nSlot, frametime, deltaYaw );

	s_referenceDirection[YAW] += deltaYaw;
	s_referenceDirection[PITCH] += deltaPitch;

	if ( joy_autoAimDampenMethod.GetInt() == 1 )
	{
		// If we are dampening, we reduce the amount we update towards our target vector.
		fX = s_lastCursorValueX + (fX - s_lastCursorValueX) * (1.0f - s_fDampeningValue);
		fY = s_lastCursorValueY + (fY - s_lastCursorValueY) * (1.0f - s_fDampeningValue);
	}

	fX = clamp( fX, -flScreenClamp, flScreenClamp );
	fY = clamp( fY, -flScreenClamp, flScreenClamp );

	float adjustedX = fX * s_FOVSensitivityAdjust + s_FOVOffsetX;
	float adjustedY = fY * s_FOVSensitivityAdjust + s_FOVOffsetY;

	Vector forward, right, up;
	// Get the orientation matrix for the camera.
	AngleVectors (s_referenceDirection, &forward, &right, &up );

	// Forward project the cursor position into the world.
	// dist is the distance from the camera to the screen.
	// NOTE!  GAME_FOV_YAW needs to be the same value returned from CCSGameRules::DefaultFOV().
	const float GAME_FOV_YAW = 90.0f;
	float dist = ( width * 0.5f ) / tanf( GAME_FOV_YAW*0.5f );
	Vector aimDirection = forward*dist + up*height*0.5f*adjustedY + right*width*0.5f*adjustedX;
	aimDirection.NormalizeInPlace();
	pPlayer->SetAimDirection( aimDirection );

	Vector PureForward( 1.0f, 0.0f, 0.0f );
	Vector PureUp( 0.0f, 0.0f, 1.0f);
	Vector PureRight( 0.0f, -1.0f, 0.0f );
	QAngle viewOffset;

	Vector pureOffset = PureForward*dist + PureUp*height*0.5f*adjustedY + PureRight*width*0.5f*adjustedX;
	VectorAngles( pureOffset, viewOffset );
	
	if ( mc_force_aim_x.GetFloat() != 0.0f )
	{
		Vector eyePos = pPlayer->EyePosition();
		Vector forcedAimPoint = forward*dist + up*height*0.5f*mc_force_aim_y.GetFloat() + right*width*0.5f*mc_force_aim_x.GetFloat();

		const float DURATION = 0.15f;
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint + up*height*0.05f, 0,255,0, true, DURATION );
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint + right*width*0.05f, 255, 0, 0, true, DURATION );
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint - up*height*0.05f, 255, 255, 0, true, DURATION );
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint - right*width*0.05f, 255,0,255, true, DURATION );

		forcedAimPoint.NormalizeInPlace();
		pPlayer->SetAimDirection( forcedAimPoint );
		
		Vector pureOffset2 = PureForward*dist + PureUp*height*0.5f*mc_force_aim_y.GetFloat() + PureRight*width*0.5f*mc_force_aim_x.GetFloat();
		VectorAngles( pureOffset2, viewOffset );
	}

	pPlayer->SetEyeAngleOffset( viewOffset );

	// Update the camera's angle.
	if ( mc_zoomed_aim_style.GetInt() == 1 && pPlayer->m_bIsScoped )
	{
		QAngle aimDirectionAngles;
		VectorAngles(aimDirection, aimDirectionAngles);
		engine->SetViewAngles( aimDirectionAngles );
	}
	else
	{
		engine->SetViewAngles( s_referenceDirection );
	}

	s_lastCursorValueX = fX;
	s_lastCursorValueY = fY;

	cmd->aimdirection = pPlayer->GetAimDirection();
	cmd->mousedx = deltaYaw;
	cmd->mousedy = deltaPitch;
}

//-----------------------------------------------------------------------------
// Purpose: Apply joystick to CUserCmd creation
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CInput::JoyStickMove( float frametime, CUserCmd *cmd )
{
	// complete initialization if first time in ( needed as cvars are not available at initialization time )
	if ( !m_fJoystickAdvancedInit )
	{
		Joystick_Advanced( false );
	}

	// verify joystick is available and that the user wants to use it
	if ( !in_joystick.GetInt() || 0 == inputsystem->GetJoystickCount() )
		return; 

	// Skip out if vgui is active
	if ( vgui::surface()->IsCursorVisible() )
		return;

	// Don't move if GameUI is visible
	if ( enginevgui->IsGameUIVisible() )
		return;

#ifdef PORTAL2
	if ( IsRadialMenuOpen() )
		return;
#endif

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

#if defined( INCLUDE_SCALEFORM )
	if ( g_pScaleformUI->SlotDeniesInputToGame( SF_SS_SLOT( nSlot ) ) )
		return;
#endif

	PerUserInput_t &user = GetPerUser( nSlot );

	// Sample the axes, apply the input, and consume sample time.
	if ( user.m_flRemainingJoystickSampleTime > 0 )
	{
		frametime = MIN(user.m_flRemainingJoystickSampleTime, frametime);
		user.m_flRemainingJoystickSampleTime -= frametime;

		float forward, side, pitch, yaw;
		bool bAbsoluteYaw, bAbsolutePitch;

		JoyStickSampleAxes( forward, side, pitch, yaw, bAbsoluteYaw, bAbsolutePitch );
		
		if ( !m_bControllerMode )
		{
			if ( fabsf(forward) > 0.1f || fabsf(side) > 0.1f || fabsf(pitch) > 0.1f || fabsf(yaw) > 0.1f )
			{
				m_bControllerMode = true;
			}
		}
			
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			JoyStickThirdPersonPlatformer( cmd, forward, side, pitch, yaw );
			return;
		}

		float	joyForwardMove, joySideMove;
		JoyStickForwardSideControl( forward, side, joyForwardMove, joySideMove );

		// Cache off the input sample values in case we run out of sample time.
		user.m_flPreviousJoystickForwardMove = joyForwardMove;
		user.m_flPreviousJoystickSideMove = joySideMove;
		user.m_flPreviousJoystickYaw = yaw;
		user.m_flPreviousJoystickPitch = pitch;
		user.m_bPreviousJoystickUseAbsoluteYaw = bAbsoluteYaw;
		user.m_bPreviousJoystickUseAbsolutePitch = bAbsolutePitch;
	}

	if ( JoyStickActive() )
	{
		// If we are using a motion controller, then we use the pointing device for updating the look direction.
		if( inputsystem->MotionControllerActive())
		{
			MotionControllerMove( frametime, cmd );
		}
		else
		{
			JoyStickTurn( cmd,
				user.m_flPreviousJoystickYaw,
				user.m_flPreviousJoystickPitch,
				frametime,
				user.m_bPreviousJoystickUseAbsoluteYaw,
				user.m_bPreviousJoystickUseAbsolutePitch );
		}

		JoyStickApplyMovement( cmd,
			user.m_flPreviousJoystickForwardMove,
			user.m_flPreviousJoystickSideMove );
	}
}

//--------------------------------------------------------------
// Applies the calculated forward/side movement to the UserCmd
//--------------------------------------------------------------
void CInput::JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove )
{
	// apply player motion relative to screen space
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
#ifdef INFESTED_DLL
		float ideal_yaw = asw_cam_marine_yaw.GetFloat();
#else
		float ideal_yaw = cam_idealyaw.GetFloat();
#endif
		float ideal_sin = sin(DEG2RAD(ideal_yaw));
		float ideal_cos = cos(DEG2RAD(ideal_yaw));
		float side_movement = ideal_cos*joySideMove - ideal_sin*joyForwardMove;
		float forward_movement = ideal_cos*joyForwardMove + ideal_sin*joySideMove;
		cmd->forwardmove += forward_movement;
		cmd->sidemove += side_movement;
	}
	else
	{
		cmd->forwardmove += joyForwardMove;
		cmd->sidemove += joySideMove;
	}

	if ( IsPC() )
	{
		CCommand tmp;
		if ( FloatMakePositive(joyForwardMove) >= joy_autosprint.GetFloat() || FloatMakePositive(joySideMove) >= joy_autosprint.GetFloat() )
		{
			KeyDown( &in_joyspeed, NULL );
		}
		else
		{
			KeyUp( &in_joyspeed, NULL );
		}
	}
}


float CInput::UpdateAndGetQuickTurnYaw( int nSlot, float frametime, float angle )
{
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( user.m_flSpinFrameTime )
	{
		// apply specified yaw velocity until duration expires
		float delta = frametime;
		if ( user.m_flSpinFrameTime - delta <= 0 )
		{
			// effect expired, avoid floating point creep
			delta = user.m_flSpinFrameTime;
			user.m_flSpinFrameTime = 0;
		}
		else
		{
			user.m_flSpinFrameTime -= delta;
		}

		// Modify the angle if we're doing a quick turn.
		angle = user.m_flSpinRate * delta;
	}

	// Update the spin rate
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( nSlot );
	if ( ( in_lookspin.GetPerUser( nSlot ).state & 2 ) && !user.m_flSpinFrameTime && pLocalPlayer && !pLocalPlayer->IsObserver() )
	{
		// user has actuated a new spin boost
		float spinFrameTime = joy_lookspin_default.GetFloat();
		user.m_flSpinFrameTime = spinFrameTime;
		// yaw velocity is in last known direction
		if ( user.m_flLastYawAngle >= 0 )
		{ 
			user.m_flSpinRate = 180.0f/spinFrameTime;
		}
		else
		{
			user.m_flSpinRate = -180.0f/spinFrameTime;
		}
	}

	// Save off the last angle if non zero.
	if ( angle != 0.0f )
	{
		// track angular direction
		user.m_flLastYawAngle = angle;
	}

	return angle;
}

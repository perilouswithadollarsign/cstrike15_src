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

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#if defined( _PS3 )
#include "ps3/ps3_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// up / down
#define	PITCH	0
// left / right
#define	YAW		1

extern ConVar lookstrafe;
extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;
extern const ConVar *sv_cheats;

#if defined PORTAL
ConVar cl_mouselook_roll_compensation( "cl_mouselook_roll_compensation", "1", 0, "In Portal and Paint, if your view is being rolled, compensate for that. So mouse movements are always relative to the screen." );
#endif


class ConVar_m_pitch : public ConVar_ServerBounded
{
public:
	ConVar_m_pitch() : 
		ConVar_ServerBounded( "m_pitch","0.022", FCVAR_ARCHIVE|FCVAR_SS, "Mouse pitch factor." )
	{
	}
	
	virtual float GetFloat() const
	{
		if ( !sv_cheats )
			sv_cheats = cvar->FindVar( "sv_cheats" );

		float flBaseValue;
		// If sv_cheats is on then it can be anything.
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		if ( nSlot != 0 )
		{
			static SplitScreenConVarRef s_m_pitch( "m_pitch" );
			flBaseValue = s_m_pitch.GetFloat( nSlot );
		}
		else
		{
			flBaseValue = GetBaseFloatValue();
		}
		if ( !sv_cheats || sv_cheats->GetBool() )
			return flBaseValue;

		// If sv_cheats is off than it can only be 0.022 or -0.022 (if they've reversed the mouse in the options).		
		if ( flBaseValue > 0 )
			return 0.022f;
		else
			return -0.022f;
	}
} cvar_m_pitch;
ConVar_ServerBounded *m_pitch = &cvar_m_pitch;

extern ConVar cam_idealyaw;
extern ConVar cam_idealpitch;
extern ConVar thirdperson_platformer;

ConVar sensitivity( "sensitivity","2.5", FCVAR_ARCHIVE, "Mouse sensitivity.", true, 0.0001f, true, 1000 );

static ConVar m_side( "m_side","0.8", FCVAR_ARCHIVE, "Mouse side factor.", true, 0.0001f, true, 1000 );
static ConVar m_yaw( "m_yaw","0.022", FCVAR_ARCHIVE, "Mouse yaw factor.", true, 0.0001f, true, 1000 );
static ConVar m_forward( "m_forward","1", FCVAR_ARCHIVE, "Mouse forward factor.", true, 0.0001f, true, 1000 );

static ConVar m_customaccel( "m_customaccel", "0", FCVAR_ARCHIVE, "Custom mouse acceleration:"
	"\n0: custom accelaration disabled"
	"\n1: mouse_acceleration = min(m_customaccel_max, pow(raw_mouse_delta, m_customaccel_exponent) * m_customaccel_scale + sensitivity)"
	"\n2: Same as 1, with but x and y sensitivity are scaled by m_pitch and m_yaw respectively."
	"\n3: mouse_acceleration = pow(raw_mouse_delta, m_customaccel_exponent - 1) * sensitivity"
	);
static ConVar m_customaccel_scale( "m_customaccel_scale", "0.04", FCVAR_ARCHIVE, "Custom mouse acceleration value.", true, 0, true, 10 );
static ConVar m_customaccel_max( "m_customaccel_max", "0", FCVAR_ARCHIVE, "Max mouse move scale factor, 0 for no limit" );
static ConVar m_customaccel_exponent( "m_customaccel_exponent", "1.05", FCVAR_ARCHIVE, "Mouse move is raised to this power before being scaled by scale factor.", true, 0.0001f, true, 10 );

static ConVar m_mousespeed( "m_mousespeed", "1", FCVAR_ARCHIVE, "Windows mouse acceleration (0 to disable, 1 to enable [Windows 2000: enable initial threshold], 2 to enable secondary threshold [Windows 2000 only]).", true, 0, true, 2 );
static ConVar m_mouseaccel1( "m_mouseaccel1", "0", FCVAR_ARCHIVE, "Windows mouse acceleration initial threshold (2x movement).", true, 0, false, 0.0f );
static ConVar m_mouseaccel2( "m_mouseaccel2", "0", FCVAR_ARCHIVE, "Windows mouse acceleration secondary threshold (4x movement).", true, 0, false, 0.0f );

static ConVar m_rawinput( "m_rawinput", "1", FCVAR_ARCHIVE, "Use Raw Input for mouse input.");

ConVar cl_mouselook( "cl_mouselook", "1", FCVAR_ARCHIVE | FCVAR_NOT_CONNECTED | FCVAR_SS, "Set to 1 to use mouse for look, 0 for keyboard look. Cannot be set while connected to a server." );


ConVar cl_mouseenable( "cl_mouseenable", "1", FCVAR_RELEASE );

void cl_mouseenable_changed_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	if( cl_mouseenable.GetBool() == true )
		input->ClearStates();
}

void CInput::InitMouse( void )
{
		cl_mouseenable.InstallChangeCallback( cl_mouseenable_changed_callback, false );
}

//-----------------------------------------------------------------------------
// Purpose: Hides cursor and starts accumulation/re-centering
//-----------------------------------------------------------------------------
void CInput::ActivateMouse (void)
{
	if ( m_fMouseActive )
		return;

	if ( m_fMouseInitialized )
	{
		if ( m_fMouseParmsValid )
		{
#if defined( WIN32 ) && !defined( USE_SDL )
			m_fRestoreSPI = SystemParametersInfo (SPI_SETMOUSE, 0, m_rgNewMouseParms, 0) ? true : false;
#endif
		}
		m_fMouseActive = true;

		// re-center the mouse if we're controlling the camera with it, otherwise just reset the cursor
		if ( GetPerUser().m_fCameraInterceptingMouse || cl_mouseenable.GetBool() )
			ResetMouse();
		else
			g_pInputSystem->ResetCursorIcon();

		g_pInputStackSystem->SetCursorIcon( m_hInputContext, INPUT_CURSOR_HANDLE_INVALID );
#if defined( USE_SDL ) || defined( OSX )
        int dx, dy;
		engine->GetMouseDelta( dx, dy, true );
#endif
		ClearStates();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gives back the cursor and stops centering of mouse
//-----------------------------------------------------------------------------
void CInput::DeactivateMouse (void)
{
	// This gets called whenever the mouse should be inactive. We only respond to it if we had 
	// previously activated the mouse. We'll show the cursor in here.
	if ( !m_fMouseActive )
		return;

	if ( m_fMouseInitialized )
	{
		if ( m_fRestoreSPI )
		{
#if defined( WIN32 ) && !defined( USE_SDL )
			SystemParametersInfo( SPI_SETMOUSE, 0, m_rgOrigMouseParms, 0 );
#endif
		}
		m_fMouseActive = false;
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW ) );
#if defined( USE_SDL ) || defined( OSX )
        // now put the mouse back in the middle of the screen
		ResetMouse();
#endif

		// Clear accumulated error, too
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetPerUser().m_flAccumulatedMouseXMovement = 0;
			GetPerUser().m_flAccumulatedMouseYMovement = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInput::CheckMouseAcclerationVars()
{
	// Don't change them if the mouse is inactive, invalid, or not using parameters for restore
	if ( !m_fMouseActive ||
		 !m_fMouseInitialized || 
		 !m_fMouseParmsValid || 
		 !m_fRestoreSPI )
	{
		return;
	}

	int values[ NUM_MOUSE_PARAMS ];

	values[ MOUSE_SPEED_FACTOR ]		= m_mousespeed.GetInt();
	values[ MOUSE_ACCEL_THRESHHOLD1 ]	= m_mouseaccel1.GetInt();
	values[ MOUSE_ACCEL_THRESHHOLD2 ]	= m_mouseaccel2.GetInt();

	bool dirty = false;

	int i;
	for ( i = 0; i < NUM_MOUSE_PARAMS; i++ )
	{
		if ( !m_rgCheckMouseParam[ i ] )
			continue;

		if ( values[ i ] != m_rgNewMouseParms[ i ] )
		{
			dirty = true;
			m_rgNewMouseParms[ i ] = values[ i ];

			char const *name = "";
			switch ( i )
			{
			default:
			case MOUSE_SPEED_FACTOR:
				name = "m_mousespeed";
				break;
			case MOUSE_ACCEL_THRESHHOLD1:
				name = "m_mouseaccel1";
				break;
			case MOUSE_ACCEL_THRESHHOLD2:
				name = "m_mouseaccel2";
				break;
			}

			char sz[ 256 ];
			Q_snprintf( sz, sizeof( sz ), "Mouse parameter '%s' set to %i\n", name, values[ i ] );
			DevMsg( "%s", sz );
		}
	}

	if ( dirty )
	{
		// Update them
#ifdef WIN32
		m_fRestoreSPI = SystemParametersInfo( SPI_SETMOUSE, 0, m_rgNewMouseParms, 0 ) ? true : false;
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: One-time initialization
//-----------------------------------------------------------------------------
void CInput::Init_Mouse (void)
{
	if ( CommandLine()->FindParm("-nomouse" ) ) 
		return; 

	m_fMouseInitialized = true;

	m_fMouseParmsValid = false;

	if ( CommandLine()->FindParm ("-useforcedmparms" ) ) 
	{
#ifdef WIN32
		m_fMouseParmsValid = SystemParametersInfo( SPI_GETMOUSE, 0, m_rgOrigMouseParms, 0 ) ? true : false;
#else
		m_fMouseParmsValid = false;
#endif
		if ( m_fMouseParmsValid )
		{
			if ( CommandLine()->FindParm ("-noforcemspd" ) ) 
			{
				m_rgNewMouseParms[ MOUSE_SPEED_FACTOR ] = m_rgOrigMouseParms[ MOUSE_SPEED_FACTOR ];

/*
				int mouseAccel[3];
				SystemParametersInfo(SPI_GETMOUSE, 0, &mouseAccel, 0); mouseAccel[2] = 0; 
				bool ok = SystemParametersInfo(SPI_SETMOUSE, 0, &mouseAccel, SPIF_UPDATEINIFILE); 
				
				// Now check registry and close/re-open Control Panel > Mouse and see 'Enhance pointer precision' is OFF 
				mouseAccel[2] = 1; 
				ok = SystemParametersInfo(SPI_SETMOUSE, 0, &mouseAccel, SPIF_UPDATEINIFILE); 
				
				// Now check registry and close/re-open Control Panel > Mouse and see 'Enhance pointer precision' is ON
*/
			}
			else
			{
				m_rgCheckMouseParam[ MOUSE_SPEED_FACTOR ] = 1;
			}

			if ( CommandLine()->FindParm ("-noforcemaccel" ) ) 
			{
				m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ] = m_rgOrigMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ];
				m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ] = m_rgOrigMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ];
			}
			else
			{
				m_rgCheckMouseParam[ MOUSE_ACCEL_THRESHHOLD1 ] = true;
				m_rgCheckMouseParam[ MOUSE_ACCEL_THRESHHOLD2 ] = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the center point of the engine window
// Input  : int&x - 
//			y - 
//-----------------------------------------------------------------------------
void CInput::GetWindowCenter( int&x, int& y )
{
	int w, h;
	engine->GetScreenSize( w, h );

	x = w >> 1;
	y = h >> 1;
}

//-----------------------------------------------------------------------------
// Purpose: Recenter the mouse
//-----------------------------------------------------------------------------
void CInput::ResetMouse( void )
{
	int x, y;
	GetWindowCenter( x,  y );
	SetMousePos( x, y );	

	// There are cases when coming back from an alt+tab, the mouse is outside of the client rect.  Because of this, our earlier call
	// to SetCursorIcon is overridden, and we're stuck with a cursor on the screen in front of the crosshairs.  We should actually be calling
	// ShowCursor to enable/disable the cursor, but that's another story.
	//
	// On WINDOWS_PC this should be ok to call every frame without a perf hit.  Inside ResetCursorIcon, we call ::SetCursor.  
	// According to the msdn docs for SetCursor:
	// "The cursor is set only if the new cursor is different from the previous cursor; otherwise, the function returns immediately."
	g_pInputSystem->ResetCursorIcon();
}


//-----------------------------------------------------------------------------
// Purpose: GetAccumulatedMouse -- the mouse can be sampled multiple times per frame and
//  these results are accumulated each time. This function gets the accumulated mouse changes and resets the accumulators
// Input  : *mx - 
//			*my - 
//-----------------------------------------------------------------------------
void CInput::GetAccumulatedMouseDeltasAndResetAccumulators( int nSlot, float *mx, float *my )
{
	Assert( mx );
	Assert( my );

	PerUserInput_t &user = GetPerUser( nSlot );

	if ( m_rawinput.GetBool() )
	{
		int rawMouseX, rawMouseY;
		inputsystem->GetRawMouseAccumulators(rawMouseX, rawMouseY);
		*mx = (float)rawMouseX;
		*my = (float)rawMouseY;
    }
	else
	{
		*mx = user.m_flAccumulatedMouseXMovement;
		*my = user.m_flAccumulatedMouseYMovement;
    }

	user.m_flAccumulatedMouseXMovement = 0;
	user.m_flAccumulatedMouseYMovement = 0;
}

//-----------------------------------------------------------------------------
// Purpose: GetMouseDelta -- Filters the mouse and stores results in old position
// Input  : mx - 
//			my - 
//			*oldx - 
//			*oldy - 
//			*x - 
//			*y - 
//-----------------------------------------------------------------------------
void CInput::GetMouseDelta( int nSlot, float inmousex, float inmousey, float *pOutMouseX, float *pOutMouseY )
{
	// Apply filtering?
	*pOutMouseX = inmousex;
	*pOutMouseY = inmousey;
}

//-----------------------------------------------------------------------------
// Purpose: Multiplies mouse values by sensitivity.  Note that for windows mouse settings
//  the input x,y offsets are already scaled based on that.  The custom acceleration, therefore,
//  is totally engine-specific and applies as a post-process to allow more user tuning.
// Input  : *x - 
//			*y - 
//-----------------------------------------------------------------------------
void CInput::ScaleMouse( int nSlot, float *x, float *y )
{
    float mx = *x;
	float my = *y;

	float flHudSensitivity = GetHud( nSlot ).GetSensitivity();

	float mouse_sensitivity = ( flHudSensitivity != 0 ) ? flHudSensitivity : sensitivity.GetFloat();

	if ( m_customaccel.GetInt() == 1 ||  m_customaccel.GetInt() == 2 ) 
	{ 
		float raw_mouse_movement_distance = sqrt( mx * mx + my * my );
		float acceleration_scale = m_customaccel_scale.GetFloat();
		float accelerated_sensitivity_max = m_customaccel_max.GetFloat();
		float accelerated_sensitivity_exponent = m_customaccel_exponent.GetFloat();
		float accelerated_sensitivity = ( (float)pow( raw_mouse_movement_distance, accelerated_sensitivity_exponent ) * acceleration_scale + mouse_sensitivity );

		if ( accelerated_sensitivity_max > 0.0001f && 
			accelerated_sensitivity > accelerated_sensitivity_max )
		{
			accelerated_sensitivity = accelerated_sensitivity_max;
		}

		*x *= accelerated_sensitivity; 
		*y *= accelerated_sensitivity; 

		// Further re-scale by yaw and pitch magnitude if user requests alternate mode 2/4
		// This means that they will need to up their value for m_customaccel_scale greatly (>40x) since m_pitch/yaw default
		//  to 0.022
		if ( m_customaccel.GetInt() == 2 || m_customaccel.GetInt() == 4 )
		{ 
			*x *= m_yaw.GetFloat(); 
			*y *= m_pitch->GetFloat(); 
		} 
	}
	else if ( m_customaccel.GetInt() == 3 )
	{
		float raw_mouse_movement_distance_squared = mx * mx + my * my;
		float fExp = MAX( 0.0f, ((m_customaccel_exponent.GetFloat() - 1.0f) / 2.0f) );
		float accelerated_sensitivity = powf( raw_mouse_movement_distance_squared, fExp ) * mouse_sensitivity;

		*x *= accelerated_sensitivity; 
		*y *= accelerated_sensitivity; 
	}
	else
	{ 
		*x *= mouse_sensitivity;
		*y *= mouse_sensitivity;
	}
}

//-----------------------------------------------------------------------------
// Purpose: ApplyMouse -- applies mouse deltas to CUserCmd
// Input  : viewangles - 
//			*cmd - 
//			mouse_x - 
//			mouse_y - 
//-----------------------------------------------------------------------------
void CInput::ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y )
{
	PerUserInput_t &user = GetPerUser( nSlot );

	//roll the view angles so roll is 0 (the HL2 assumed state) and mouse adjustments are relative to the screen.
	//Assuming roll is unchanging, we want mouse left to translate to screen left at all times (same for right, up, and down)
	
#if defined PORTAL //Portal sometimes rolls the player and we want a understandable way of aiming. 
	//we want mouse left to translate to screen left at all times (same for right, up, and down)
	//So we'll be transforming mouse inputs by the roll value
	Quaternion quatRoll;
	Quaternion quatInverseRoll;
	QAngle roll( 0.0f, 0.0f, viewangles[ ROLL ] );
	QAngle invroll( 0.0f, 0.0f, -viewangles[ ROLL ] );
	AngleQuaternion( roll, quatRoll );
	AngleQuaternion( invroll, quatInverseRoll );
#endif	

	if ( !((in_strafe.GetPerUser( nSlot ).state & 1) || lookstrafe.GetInt()) )
	{
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			if ( mouse_x )
			{
				// use the mouse to orbit the camera around the player, and update the idealAngle
				user.m_vecCameraOffset[ YAW ] -= m_yaw.GetFloat() * mouse_x;
				cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );

				// why doesn't this work??? CInput::AdjustYaw is why
				//cam_idealyaw.SetValue( cam_idealyaw.GetFloat() - m_yaw.GetFloat() * mouse_x );
			}
		}
		else
		{
			// Otherwize, use mouse to spin around vertical axis

#if defined PORTAL
			if( cl_mouselook_roll_compensation.GetBool() ) //for portal, remap yaw/pitch adjustments to be relative to your current view roll so left/right on the mouse is left/right on the screen
			{
				QAngle qAngleTemp( 0.0f, -(m_yaw.GetFloat() * mouse_x), 0.0f );			

				Quaternion quatTemp;
				AngleQuaternion( qAngleTemp, quatTemp );

				Quaternion qRollUndone[2];
				QuaternionMult( quatTemp, quatInverseRoll, qRollUndone[0] );
				QuaternionMult( quatRoll, qRollUndone[0], qRollUndone[1] );
				QuaternionAngles( qRollUndone[1], qAngleTemp );

				viewangles[0] += qAngleTemp[0];
				viewangles[1] += qAngleTemp[1];
				viewangles[2] += qAngleTemp[2];
			}
			else
#endif
			{
				viewangles[YAW] -= m_yaw.GetFloat() * mouse_x;
			}
		}
	}
	else
	{
		// If holding strafe key or mlooking and have lookstrafe set to true, then apply
		//  horizontal mouse movement to sidemove.
		cmd->sidemove += m_side.GetFloat() * mouse_x;
	}

	// If mouselooking and not holding strafe key, then use vertical mouse
	//  to adjust view pitch.
	if (!(in_strafe.GetPerUser( nSlot ).state & 1))
	{
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			if ( mouse_y )
			{
				// use the mouse to orbit the camera around the player, and update the idealAngle
				user.m_vecCameraOffset[ PITCH ] += m_pitch->GetFloat() * mouse_y;
				cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );

				// why doesn't this work??? CInput::AdjustYaw is why
				//cam_idealpitch.SetValue( cam_idealpitch.GetFloat() + m_pitch->GetFloat() * mouse_y );
			}
		}
		else
		{
#if defined PORTAL
			if( cl_mouselook_roll_compensation.GetBool() ) //for portal, remap yaw/pitch adjustments to be relative to your current view roll so left/right on the mouse is left/right on the screen
			{
				QAngle qAngleTemp( m_pitch->GetFloat() * mouse_y, 0.0f, 0.0f );

				Quaternion quatTemp;
				AngleQuaternion( qAngleTemp, quatTemp );

				Quaternion qRollUndone[2];
				QuaternionMult( quatTemp, quatInverseRoll, qRollUndone[0] );
				QuaternionMult( quatRoll, qRollUndone[0], qRollUndone[1] );
				QuaternionAngles( qRollUndone[1], qAngleTemp );

				viewangles[0] += qAngleTemp[0];
				viewangles[1] += qAngleTemp[1];
				viewangles[2] += qAngleTemp[2];
			}
			else
#endif
			{
				viewangles[PITCH] += m_pitch->GetFloat() * mouse_y;
			}

			// Check pitch bounds
			if (viewangles[PITCH] > cl_pitchdown.GetFloat())
			{
				viewangles[PITCH] = cl_pitchdown.GetFloat();
			}
			if (viewangles[PITCH] < -cl_pitchup.GetFloat())
			{
				viewangles[PITCH] = -cl_pitchup.GetFloat();
			}
		}		
	}
	else
	{
		// Otherwise if holding strafe key and noclipping, then move upward
/*		if ((in_strafe.state & 1) && IsNoClipping() )
		{
			cmd->upmove -= m_forward.GetFloat() * mouse_y;
		} 
		else */
		{
			// Default is to apply vertical mouse movement as a forward key press.
			cmd->forwardmove -= m_forward.GetFloat() * mouse_y;
		}
	}

	// Finally, add mouse state to usercmd.
	// NOTE:  Does rounding to int cause any issues?  ywb 1/17/04
    cmd->mousedx = (int)mouse_x;
	cmd->mousedy = (int)mouse_y;
}

extern bool UsingMouselook( int nSlot );

//-----------------------------------------------------------------------------
// Purpose: AccumulateMouse
//-----------------------------------------------------------------------------
void CInput::AccumulateMouse( int nSlot )
{
	if( !cl_mouseenable.GetBool() )
	{
        return;
	}

	if( !UsingMouselook( nSlot ) )
	{
        return;
	}

	if ( m_rawinput.GetBool() ) 
	{
        return;
	}

	int w, h;
	engine->GetScreenSize( w, h );

	// x,y = screen center
	int x = w >> 1;
	int y = h >> 1;

	//only accumulate mouse if we are not moving the camera with the mouse
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( !user.m_fCameraInterceptingMouse && vgui::surface()->IsCursorLocked() )
	{
		//Assert( !vgui::surface()->IsCursorVisible() );
#if defined( USE_SDL ) || defined( OSX )
		int dx, dy;
		engine->GetMouseDelta( dx, dy );
		user.m_flAccumulatedMouseXMovement += dx;
		user.m_flAccumulatedMouseYMovement += dy;
#elif defined( WIN32 ) 
		int current_posx, current_posy;

		GetMousePos(current_posx, current_posy);

		user.m_flAccumulatedMouseXMovement += current_posx - x;
		user.m_flAccumulatedMouseYMovement += current_posy - y;
#elif defined( _PS3 )
#else
#error
#endif
		// force the mouse to the center, so there's room to move
		ResetMouse();
	}
	else if ( m_fMouseActive )
	{
		// Clamp
		int ox, oy;
		GetMousePos( ox, oy );
        ox = clamp( ox, 0, w - 1 );
		oy = clamp( oy, 0, h - 1 );
        SetMousePos( ox, oy );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get raw mouse position
// Input  : &ox - 
//			&oy - 
//-----------------------------------------------------------------------------
void CInput::GetMousePos(int &ox, int &oy)
{
	g_pInputSystem->GetCursorPosition( &ox, &oy );
}

//-----------------------------------------------------------------------------
// Purpose: Force raw mouse position
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CInput::SetMousePos( int x, int y )
{
    g_pInputStackSystem->SetCursorPosition( m_hInputContext, x, y );
}

//-----------------------------------------------------------------------------
// Purpose: MouseMove -- main entry point for applying mouse
// Input  : *cmd - 
//-----------------------------------------------------------------------------
void CInput::MouseMove( int nSlot, CUserCmd *cmd )
{
	float	mouse_x, mouse_y;
	float	mx, my;
	QAngle	viewangles;

	// Get view angles from engine
	engine->GetViewAngles( viewangles );

	// Validate mouse speed/acceleration settings
	CheckMouseAcclerationVars();

	// Don't drift pitch at all while mouselooking.
	view->StopPitchDrift ();

	//jjb - this disables normal mouse control if the user is trying to 
	//      move the camera, or if the mouse cursor is visible 
	if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) && cl_mouseenable.GetBool() )
	{
		// Sample mouse one more time
		AccumulateMouse( nSlot );

		// Latch accumulated mouse movements and reset accumulators
		GetAccumulatedMouseDeltasAndResetAccumulators( nSlot, &mx, &my );

		// Filter, etc. the delta values and place into mouse_x and mouse_y
		GetMouseDelta( nSlot, mx, my, &mouse_x, &mouse_y );

		if ( IsPC() )
		{
			if ( ControllerModeActive() )
			{
				// accumulate mouse movements and if we go over a certain threshold, switch out of controller mode
				m_fAccumulatedMouseMove += fabsf( mouse_x ) + fabsf( mouse_y );

				//Msg( "total_mouse_move = %f\n", m_fAccumulatedMouseMove );
				if ( m_fAccumulatedMouseMove > 30.0f )
				{
					m_bControllerMode = false;
					m_fAccumulatedMouseMove = 0.0f;
				}
			}
		}

		// Apply scaling factor
		ScaleMouse( nSlot, &mouse_x, &mouse_y );

		// Let the client mode at the mouse input before it's used
		GetClientMode()->OverrideMouseInput( &mouse_x, &mouse_y );

		// Add mouse X/Y movement to cmd
		ApplyMouse( nSlot, viewangles, cmd, mouse_x, mouse_y );

		// Re-center the mouse.
		ResetMouse();
	}

	// Store out the new viewangles.
	engine->SetViewAngles( viewangles );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mx - 
//			*my - 
//			*unclampedx - 
//			*unclampedy - 
//-----------------------------------------------------------------------------
void CInput::GetFullscreenMousePos( int *mx, int *my, int *unclampedx /*=NULL*/, int *unclampedy /*=NULL*/ )
{
	Assert( mx );
	Assert( my );

#if !(INFESTED_DLL) && !(DOTA_CLIENT_DLL)
	if ( g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
		return;
#endif

	int x, y;
	GetWindowCenter( x,  y );

	int		current_posx, current_posy;

	GetMousePos(current_posx, current_posy);

	current_posx -= x;
	current_posy -= y;

	// Now need to add back in mid point of viewport
	//

	int w, h;
	vgui::surface()->GetScreenSize( w, h );
	current_posx += w  / 2;
	current_posy += h / 2;

	if ( unclampedx )
	{
		*unclampedx = current_posx;
	}

	if ( unclampedy )
	{
		*unclampedy = current_posy;
	}

	// Clamp
	current_posx = MAX( 0, current_posx );
	current_posx = MIN( ScreenWidth(), current_posx );

	current_posy = MAX( 0, current_posy );
	current_posy = MIN( ScreenHeight(), current_posy );

	*mx = current_posx;
	*my = current_posy;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CInput::SetFullscreenMousePos( int mx, int my )
{
	SetMousePos( mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: ClearStates -- Resets mouse accumulators so you don't get a pop when returning to trapped mouse
//-----------------------------------------------------------------------------
void CInput::ClearStates (void)
{
	if ( !m_fMouseActive )
		return;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetPerUser().m_flAccumulatedMouseXMovement = 0;
		GetPerUser().m_flAccumulatedMouseYMovement = 0;
	}

	// clear raw mouse accumulated data
	int rawX, rawY;
	inputsystem->GetRawMouseAccumulators(rawX, rawY);
}

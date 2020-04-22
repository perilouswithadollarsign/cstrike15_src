//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef INPUTENUMS_H
#define INPUTENUMS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"

// Standard maximum +/- value of a joystick axis
#define MAX_BUTTONSAMPLE			32768

#if !defined( _X360 )
#define INVALID_USER_ID		-1
#else
#define INVALID_USER_ID		XBX_INVALID_USER_ID
#endif

//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------

enum
{
#ifdef _PS3
	MAX_JOYSTICKS = 7,
#else
	MAX_JOYSTICKS = 4,
#endif
	MOUSE_BUTTON_COUNT = 5,
	MAX_NOVINT_DEVICES = 2,
};

enum JoystickAxis_t
{
	JOY_AXIS_X = 0,
 	JOY_AXIS_Y,
	JOY_AXIS_Z,
	JOY_AXIS_R,
	JOY_AXIS_U,
	JOY_AXIS_V,
	MAX_JOYSTICK_AXES,
};

enum JoystickDeadzoneMode_t
{
	JOYSTICK_DEADZONE_CROSS = 0,
	JOYSTICK_DEADZONE_SQUARE = 1,
};


//-----------------------------------------------------------------------------
// Extra mouse codes
//-----------------------------------------------------------------------------
enum
{
	MS_WM_XBUTTONDOWN	= 0x020B,
	MS_WM_XBUTTONUP		= 0x020C,
	MS_WM_XBUTTONDBLCLK	= 0x020D,
	MS_MK_BUTTON4		= 0x0020,
	MS_MK_BUTTON5		= 0x0040,
};

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------
enum InputEventType_t
{
	IE_ButtonPressed = 0,	// m_nData contains a ButtonCode_t
	IE_ButtonReleased,		// m_nData contains a ButtonCode_t
	IE_ButtonDoubleClicked,	// m_nData contains a ButtonCode_t
	IE_AnalogValueChanged,	// m_nData contains an AnalogCode_t, m_nData2 contains the value

	IE_FirstSystemEvent = 100,
	IE_Quit = IE_FirstSystemEvent,
	IE_ControllerInserted,	// m_nData contains the controller ID
	IE_ControllerUnplugged,	// m_nData contains the controller ID
	IE_Close,
	IE_WindowSizeChanged,	// m_nData contains width, m_nData2 contains height, m_nData3 = 0 if not minimized, 1 if minimized
	IE_PS_CameraUnplugged,  // m_nData contains code for type of disconnect.  
	IE_PS_Move_OutOfView,   // m_nData contains bool (0, 1) for whether the move is now out of view (1) or in view (0)

	IE_FirstUIEvent = 200,
	IE_LocateMouseClick = IE_FirstUIEvent,
	IE_SetCursor,
	IE_KeyTyped,
	IE_KeyCodeTyped,
	IE_InputLanguageChanged,
	IE_IMESetWindow,
	IE_IMEStartComposition,
	IE_IMEComposition,
	IE_IMEEndComposition,
	IE_IMEShowCandidates,
	IE_IMEChangeCandidates,
	IE_IMECloseCandidates,
	IE_IMERecomputeModes,
	IE_OverlayEvent,

	IE_FirstVguiEvent = 1000,	// Assign ranges for other systems that post user events here
	IE_FirstAppEvent = 2000,
};

struct InputEvent_t
{
	int m_nType;				// Type of the event (see InputEventType_t)
	int m_nTick;				// Tick on which the event occurred
	int m_nData;				// Generic 32-bit data, what it contains depends on the event
	int m_nData2;				// Generic 32-bit data, what it contains depends on the event
	int m_nData3;				// Generic 32-bit data, what it contains depends on the event
};

//-----------------------------------------------------------------------------
// Steam Controller Enums
//-----------------------------------------------------------------------------

#ifndef MAX_STEAM_CONTROLLERS
#define MAX_STEAM_CONTROLLERS 16
#endif

typedef enum
{
	SK_NULL,
	SK_BUTTON_A,
	SK_BUTTON_B,
	SK_BUTTON_X,
	SK_BUTTON_Y,
	SK_BUTTON_UP,
	SK_BUTTON_RIGHT,
	SK_BUTTON_DOWN,
	SK_BUTTON_LEFT,
	SK_BUTTON_LEFT_BUMPER,
	SK_BUTTON_RIGHT_BUMPER,
	SK_BUTTON_LEFT_TRIGGER,
	SK_BUTTON_RIGHT_TRIGGER,
	SK_BUTTON_LEFT_GRIP,
	SK_BUTTON_RIGHT_GRIP,
	SK_BUTTON_LPAD_TOUCH,
	SK_BUTTON_RPAD_TOUCH,
	SK_BUTTON_LPAD_CLICK,
	SK_BUTTON_RPAD_CLICK,
	SK_BUTTON_LPAD_UP,
	SK_BUTTON_LPAD_RIGHT,
	SK_BUTTON_LPAD_DOWN,
	SK_BUTTON_LPAD_LEFT,
	SK_BUTTON_RPAD_UP, 
	SK_BUTTON_RPAD_RIGHT, 
	SK_BUTTON_RPAD_DOWN, 
	SK_BUTTON_RPAD_LEFT, 
	SK_BUTTON_SELECT, 
	SK_BUTTON_START, 
	SK_BUTTON_STEAM, 
	SK_BUTTON_INACTIVE_START, 
	SK_MAX_KEYS
} sKey_t;

enum ESteamPadAxis
{
	LEFTPAD_AXIS_X,
	LEFTPAD_AXIS_Y,
	RIGHTPAD_AXIS_X,
	RIGHTPAD_AXIS_Y,
	LEFT_TRIGGER_AXIS,
	RIGHT_TRIGGER_AXIS,
	GYRO_AXIS_PITCH,
	GYRO_AXIS_ROLL,
	GYRO_AXIS_YAW,
	MAX_STEAMPADAXIS = GYRO_AXIS_YAW
};

enum
{
	LASTINPUT_KBMOUSE = 0,
	LASTINPUT_CONTROLLER = 1,
	LASTINPUT_STEAMCONTROLLER = 2
};

#endif // INPUTENUMS_H

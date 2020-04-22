//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "platform.h"

#if !defined( _GAMECONSOLE ) && !defined( PLATFORM_POSIX )
#include <wtypes.h>
#include <winuser.h>
#include "xbox/xboxstubs.h"
#elif defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#undef unlink
#elif defined( _PS3 )
#include "ps3/ps3_core.h"
#include <cell/keyboard.h>
#endif // WIN32

#if defined( _OSX )
#include "posix_stubs.h"
#endif

#include "key_translation.h"
#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ButtonCode_t s_pVirtualKeyToButtonCode[256];

static ButtonCode_t s_pSKeytoButtonCode[SK_MAX_KEYS];

#if defined( PLATFORM_WINDOWS ) || defined( _GAMECONSOLE ) || defined( _OSX )
static ButtonCode_t s_pXKeyTrans[XK_MAX_KEYS];
#endif

static int s_pButtonCodeToVirtual[BUTTON_CODE_LAST];
#if defined ( _GAMECONSOLE )
#define JOYSTICK_NAMES_BUTTONS( x ) \
	"A_BUTTON",\
	"B_BUTTON",\
	"X_BUTTON",\
	"Y_BUTTON",\
	"L_SHOULDER",\
	"R_SHOULDER",\
	"BACK",\
	"START",\
	"STICK1",\
	"STICK2",\
	"JOY11",\
	"FIREMODE_1",\
	"FIREMODE_2",\
	"FIREMODE_3",\
	"RELOAD",\
	"TRIGGER",\
	"PUMP_ACTION",\
	"ROLL_RIGHT",\
	"ROLL_LEFT",\
	"JOY20",\
	"JOY21",\
	"JOY22",\
	"JOY23",\
	"JOY24",\
	"JOY25",\
	"JOY26",\
	"JOY27",\
	"JOY28",\
	"JOY29",\
	"JOY30",\
	"JOY31",\
	"JOY32"

#define JOYSTICK_NAMES_POV( x ) \
	"UP",\
	"RIGHT",\
	"DOWN",\
	"LEFT"

#define JOYSTICK_NAMES_AXIS( x ) \
	"S1_RIGHT",\
	"S1_LEFT",\
	"S1_DOWN",\
	"S1_UP",\
	"L_TRIGGER",\
	"R_TRIGGER",\
	"S2_RIGHT",\
	"S2_LEFT",\
	"S2_DOWN",\
	"S2_UP",\
	"V AXIS POS",\
	"V AXIS NEG"

#else

#define JOYSTICK_NAMES_BUTTONS( x )	\
	"JOY1",		\
	"JOY2",		\
	"JOY3",		\
	"JOY4",		\
	"JOY5",		\
	"JOY6",		\
	"JOY7",		\
	"JOY8",		\
	"JOY9",		\
	"JOY10",	\
	"JOY11",	\
	"JOY12",	\
	"JOY13",	\
	"JOY14",	\
	"JOY15",	\
	"JOY16",	\
	"JOY17",	\
	"JOY18",	\
	"JOY19",	\
	"JOY20",	\
	"JOY21",	\
	"JOY22",	\
	"JOY23",	\
	"JOY24",	\
	"JOY25",	\
	"JOY26",	\
	"JOY27",	\
	"JOY28",	\
	"JOY29",	\
	"JOY30",	\
	"JOY31",	\
	"JOY32"

#define JOYSTICK_NAMES_POV( x ) \
	"POV_UP",\
	"POV_RIGHT",\
	"POV_DOWN",\
	"POV_LEFT"

#define JOYSTICK_NAMES_AXIS( x ) \
	"X AXIS POS",\
	"X AXIS NEG",\
	"Y AXIS POS",\
	"Y AXIS NEG",\
	"Z AXIS POS",\
	"Z AXIS NEG",\
	"R AXIS POS",\
	"R AXIS NEG",\
	"U AXIS POS",\
	"U AXIS NEG",\
	"V AXIS POS",\
	"V AXIS NEG"

#endif

#define SCONTROLLERBUTTONS_BUTTONS( x ) \
	"SC_A",\
	"SC_B",\
	"SC_X",\
	"SC_Y",\
	"SC_DPAD_UP",\
	"SC_DPAD_RIGHT",\
	"SC_DPAD_DOWN",\
	"SC_DPAD_LEFT",\
	"SC_LEFT_BUMPER",\
	"SC_RIGHT_BUMPER",\
	"SC_LEFT_TRIGGER",\
	"SC_RIGHT_TRIGGER",\
	"SC_LEFT_GRIP",\
	"SC_RIGHT_GRIP",\
	"SC_LEFT_PAD_TOUCH",\
	"SC_RIGHT_PAD_TOUCH",\
	"SC_LEFT_PAD_CLICK",\
	"SC_RIGHT_PAD_CLICK",\
	"SC_LPAD_UP",\
	"SC_LPAD_RIGHT",\
	"SC_LPAD_DOWN",\
	"SC_LPAD_LEFT",\
	"SC_RPAD_UP",\
	"SC_RPAD_RIGHT",\
	"SC_RPAD_DOWN",\
	"SC_RPAD_LEFT",\
	"SC_SELECT",\
	"SC_START",\
	"SC_STEAM",\
	"SC_NULL"

#define SCONTROLLERBUTTONS_AXIS( x ) \
	"SC_LPAD_AXIS_RIGHT",\
	"SC_LPAD_AXIS_LEFT",\
	"SC_LPAD_AXIS_DOWN",\
	"SC_LPAD_AXIS_UP",\
	"SC_AXIS_L_TRIGGER",\
	"SC_AXIS_R_TRIGGER",\
	"SC_RPAD_AXIS_RIGHT",\
	"SC_RPAD_AXIS_LEFT",\
	"SC_RPAD_AXIS_DOWN",\
	"SC_RPAD_AXIS_UP",\
	"SC_GYRO_AXIS_PITCH_POSITIVE",\
	"SC_GYRO_AXIS_PITCH_NEGATIVE",\
	"SC_GYRO_AXIS_ROLL_POSITIVE",\
	"SC_GYRO_AXIS_ROLL_NEGATIVE",\
	"SC_GYRO_AXIS_YAW_POSITIVE",\
	"SC_GYRO_AXIS_YAW_NEGATIVE"

#if defined( _PS3 )

static const char *s_pPS3ButtonCodeName[ ] =
{
	"",				// KEY_NONE
	"KEY_0",			// KEY_0,
	"KEY_1",			// KEY_1,
	"KEY_2",			// KEY_2,
	"KEY_3",			// KEY_3,
	"KEY_4",			// KEY_4,
	"KEY_5",			// KEY_5,
	"KEY_6",			// KEY_6,
	"KEY_7",			// KEY_7,
	"KEY_8",			// KEY_8,
	"KEY_9",			// KEY_9,
	"KEY_A",			// KEY_A,
	"KEY_B",			// KEY_B,
	"KEY_C",			// KEY_C,
	"KEY_D",			// KEY_D,
	"KEY_E",			// KEY_E,
	"KEY_F",			// KEY_F,
	"KEY_G",			// KEY_G,
	"KEY_H",			// KEY_H,
	"KEY_I",			// KEY_I,
	"KEY_J",			// KEY_J,
	"KEY_K",			// KEY_K,
	"KEY_L",			// KEY_L,
	"KEY_M",			// KEY_M,
	"KEY_N",			// KEY_N,
	"KEY_O",			// KEY_O,
	"KEY_P",			// KEY_P,
	"KEY_Q",			// KEY_Q,
	"KEY_R",			// KEY_R,
	"KEY_S",			// KEY_S,
	"KEY_T",			// KEY_T,
	"KEY_U",			// KEY_U,
	"KEY_V",			// KEY_V,
	"KEY_W",			// KEY_W,
	"KEY_X",			// KEY_X,
	"KEY_Y",			// KEY_Y,
	"KEY_Z",			// KEY_Z,
	"KP_INS",		// KEY_PAD_0,
	"KP_END",		// KEY_PAD_1,
	"KP_DOWNARROW",	// KEY_PAD_2,
	"KP_PGDN",		// KEY_PAD_3,
	"KP_LEFTARROW",	// KEY_PAD_4,
	"KP_5",			// KEY_PAD_5,
	"KP_RIGHTARROW",// KEY_PAD_6,
	"KP_HOME",		// KEY_PAD_7,
	"KP_UPARROW",	// KEY_PAD_8,
	"KP_PGUP",		// KEY_PAD_9,
	"KP_SLASH",		// KEY_PAD_DIVIDE,
	"KP_MULTIPLY",	// KEY_PAD_MULTIPLY,
	"KP_MINUS",		// KEY_PAD_MINUS,
	"KP_PLUS",		// KEY_PAD_PLUS,
	"KP_ENTER",		// KEY_PAD_ENTER,
	"KP_DEL",		// KEY_PAD_DECIMAL,
	"LBRACKET",			// KEY_LBRACKET,
	"RBRACKET",			// KEY_RBRACKET,
	"SEMICOLON",	// KEY_SEMICOLON,
	"APOSTROPHE",			// KEY_APOSTROPHE,
	"BACKQUOTE",			// KEY_BACKQUOTE,
	"COMMA",			// KEY_COMMA,
	"PERIOD",			// KEY_PERIOD,
	"SLASH",			// KEY_SLASH,
	"BACKSLASH",			// KEY_BACKSLASH,
	"MINUS",			// KEY_MINUS,
	"EQUAL",			// KEY_EQUAL,
	"ENTER",		// KEY_ENTER,
	"SPACE",		// KEY_SPACE,
	"BACKSPACE",	// KEY_BACKSPACE,
	"TAB",			// KEY_TAB,
	"CAPSLOCK",		// KEY_CAPSLOCK,
	"NUMLOCK",		// KEY_NUMLOCK,
	"ESCAPE",		// KEY_ESCAPE,
	"SCROLLLOCK",	// KEY_SCROLLLOCK,
	"INS",			// KEY_INSERT,
	"DEL",			// KEY_DELETE,
	"HOME",			// KEY_HOME,
	"END",			// KEY_END,
	"PGUP",			// KEY_PAGEUP,
	"PGDN",			// KEY_PAGEDOWN,
	"PAUSE",		// KEY_BREAK,
	"SHIFT",		// KEY_LSHIFT,
	"RSHIFT",		// KEY_RSHIFT,
	"ALT",			// KEY_LALT,
	"RALT",			// KEY_RALT,
	"CTRL",			// KEY_LCONTROL,
	"RCTRL",		// KEY_RCONTROL,
	"LWIN",			// KEY_LWIN,
	"RWIN",			// KEY_RWIN,
	"APP",			// KEY_APP,
	"UPARROW",		// KEY_UP,
	"LEFTARROW",	// KEY_LEFT,
	"DOWNARROW",	// KEY_DOWN,
	"RIGHTARROW",	// KEY_RIGHT,
	"F1",			// KEY_F1,
	"F2",			// KEY_F2,
	"F3",			// KEY_F3,
	"F4",			// KEY_F4,
	"F5",			// KEY_F5,
	"F6",			// KEY_F6,
	"F7",			// KEY_F7,
	"F8",			// KEY_F8,
	"F9",			// KEY_F9,
	"F10",			// KEY_F10,
	"F11",			// KEY_F11,
	"F12",			// KEY_F12,

	// FIXME: CAPSLOCK/NUMLOCK/SCROLLLOCK all appear above. What are these for?!
	// They only appear in CInputWin32::UpdateToggleButtonState in vgui2
	"CAPSLOCKTOGGLE",	// KEY_CAPSLOCKTOGGLE,
	"NUMLOCKTOGGLE",	// KEY_NUMLOCKTOGGLE,
	"SCROLLLOCKTOGGLE", // KEY_SCROLLLOCKTOGGLE,

	// Mouse
	"MOUSE1",		// MOUSE_LEFT,
	"MOUSE2",		// MOUSE_RIGHT,
	"MOUSE3",		// MOUSE_MIDDLE,
	"MOUSE4",		// MOUSE_4,
	"MOUSE5",		// MOUSE_5,

	"MWHEELUP",		// MOUSE_WHEEL_UP
	"MWHEELDOWN",	// MOUSE_WHEEL_DOWN

	JOYSTICK_NAMES_BUTTONS( 0 ),
	JOYSTICK_NAMES_BUTTONS( 1 ),
	JOYSTICK_NAMES_BUTTONS( 2 ),
	JOYSTICK_NAMES_BUTTONS( 3 ),
	JOYSTICK_NAMES_BUTTONS( 4 ),
	JOYSTICK_NAMES_BUTTONS( 5 ),
	JOYSTICK_NAMES_BUTTONS( 6 ),

	JOYSTICK_NAMES_POV( 0 ),
	JOYSTICK_NAMES_POV( 1 ),
	JOYSTICK_NAMES_POV( 2 ),
	JOYSTICK_NAMES_POV( 3 ),
	JOYSTICK_NAMES_POV( 4 ),
	JOYSTICK_NAMES_POV( 5 ),
	JOYSTICK_NAMES_POV( 6 ),

	JOYSTICK_NAMES_AXIS( 0 ),
	JOYSTICK_NAMES_AXIS( 1 ),
	JOYSTICK_NAMES_AXIS( 2 ),
	JOYSTICK_NAMES_AXIS( 3 ),
	JOYSTICK_NAMES_AXIS( 4 ),
	JOYSTICK_NAMES_AXIS( 5 ),
	JOYSTICK_NAMES_AXIS( 6 ),
};

#endif

static const char *s_pButtonCodeName[ ] =
{
	"",				// KEY_NONE
	"0",			// KEY_0,
	"1",			// KEY_1,
	"2",			// KEY_2,
	"3",			// KEY_3,
	"4",			// KEY_4,
	"5",			// KEY_5,
	"6",			// KEY_6,
	"7",			// KEY_7,
	"8",			// KEY_8,
	"9",			// KEY_9,
	"a",			// KEY_A,
	"b",			// KEY_B,
	"c",			// KEY_C,
	"d",			// KEY_D,
	"e",			// KEY_E,
	"f",			// KEY_F,
	"g",			// KEY_G,
	"h",			// KEY_H,
	"i",			// KEY_I,
	"j",			// KEY_J,
	"k",			// KEY_K,
	"l",			// KEY_L,
	"m",			// KEY_M,
	"n",			// KEY_N,
	"o",			// KEY_O,
	"p",			// KEY_P,
	"q",			// KEY_Q,
	"r",			// KEY_R,
	"s",			// KEY_S,
	"t",			// KEY_T,
	"u",			// KEY_U,
	"v",			// KEY_V,
	"w",			// KEY_W,
	"x",			// KEY_X,
	"y",			// KEY_Y,
	"z",			// KEY_Z,
	"KP_INS",		// KEY_PAD_0,
	"KP_END",		// KEY_PAD_1,
	"KP_DOWNARROW",	// KEY_PAD_2,
	"KP_PGDN",		// KEY_PAD_3,
	"KP_LEFTARROW",	// KEY_PAD_4,
	"KP_5",			// KEY_PAD_5,
	"KP_RIGHTARROW",// KEY_PAD_6,
	"KP_HOME",		// KEY_PAD_7,
	"KP_UPARROW",	// KEY_PAD_8,
	"KP_PGUP",		// KEY_PAD_9,
	"KP_SLASH",		// KEY_PAD_DIVIDE,
	"KP_MULTIPLY",	// KEY_PAD_MULTIPLY,
	"KP_MINUS",		// KEY_PAD_MINUS,
	"KP_PLUS",		// KEY_PAD_PLUS,
	"KP_ENTER",		// KEY_PAD_ENTER,
	"KP_DEL",		// KEY_PAD_DECIMAL,
	"[",			// KEY_LBRACKET,
	"]",			// KEY_RBRACKET,
	"SEMICOLON",	// KEY_SEMICOLON,
	"'",			// KEY_APOSTROPHE,
	"`",			// KEY_BACKQUOTE,
	",",			// KEY_COMMA,
	".",			// KEY_PERIOD,
	"/",			// KEY_SLASH,
	"\\",			// KEY_BACKSLASH,
	"-",			// KEY_MINUS,
	"=",			// KEY_EQUAL,
	"ENTER",		// KEY_ENTER,
	"SPACE",		// KEY_SPACE,
	"BACKSPACE",	// KEY_BACKSPACE,
	"TAB",			// KEY_TAB,
	"CAPSLOCK",		// KEY_CAPSLOCK,
	"NUMLOCK",		// KEY_NUMLOCK,
	"ESCAPE",		// KEY_ESCAPE,
	"SCROLLLOCK",	// KEY_SCROLLLOCK,
	"INS",			// KEY_INSERT,
	"DEL",			// KEY_DELETE,
	"HOME",			// KEY_HOME,
	"END",			// KEY_END,
	"PGUP",			// KEY_PAGEUP,
	"PGDN",			// KEY_PAGEDOWN,
	"PAUSE",		// KEY_BREAK,
	"SHIFT",		// KEY_LSHIFT,
	"RSHIFT",		// KEY_RSHIFT,
	"ALT",			// KEY_LALT,
	"RALT",			// KEY_RALT,
	"CTRL",			// KEY_LCONTROL,
	"RCTRL",		// KEY_RCONTROL,
#if defined(OSX)
    "COMMAND",      // KEY_LWIN
    "COMMAND",      // KEY_RWIN
#else
	"LWIN",			// KEY_LWIN,
	"RWIN",			// KEY_RWIN,
#endif
	"APP",			// KEY_APP,
	"UPARROW",		// KEY_UP,
	"LEFTARROW",	// KEY_LEFT,
	"DOWNARROW",	// KEY_DOWN,
	"RIGHTARROW",	// KEY_RIGHT,
	"F1",			// KEY_F1,
	"F2",			// KEY_F2,
	"F3",			// KEY_F3,
	"F4",			// KEY_F4,
	"F5",			// KEY_F5,
	"F6",			// KEY_F6,
	"F7",			// KEY_F7,
	"F8",			// KEY_F8,
	"F9",			// KEY_F9,
	"F10",			// KEY_F10,
	"F11",			// KEY_F11,
	"F12",			// KEY_F12,

	// FIXME: CAPSLOCK/NUMLOCK/SCROLLLOCK all appear above. What are these for?!
	// They only appear in CInputWin32::UpdateToggleButtonState in vgui2
	"CAPSLOCKTOGGLE",	// KEY_CAPSLOCKTOGGLE,
	"NUMLOCKTOGGLE",	// KEY_NUMLOCKTOGGLE,
	"SCROLLLOCKTOGGLE", // KEY_SCROLLLOCKTOGGLE,

	// Mouse
	"MOUSE1",		// MOUSE_LEFT,
	"MOUSE2",		// MOUSE_RIGHT,
	"MOUSE3",		// MOUSE_MIDDLE,
	"MOUSE4",		// MOUSE_4,
	"MOUSE5",		// MOUSE_5,

	"MWHEELUP",		// MOUSE_WHEEL_UP
	"MWHEELDOWN",	// MOUSE_WHEEL_DOWN

	JOYSTICK_NAMES_BUTTONS( 0 ),
	JOYSTICK_NAMES_BUTTONS( 1 ),
	JOYSTICK_NAMES_BUTTONS( 2 ),
	JOYSTICK_NAMES_BUTTONS( 3 ),
#ifdef _PS3
	JOYSTICK_NAMES_BUTTONS( 4 ),
	JOYSTICK_NAMES_BUTTONS( 5 ),
	JOYSTICK_NAMES_BUTTONS( 6 ),
#endif

	JOYSTICK_NAMES_POV( 0 ),
	JOYSTICK_NAMES_POV( 1 ),
	JOYSTICK_NAMES_POV( 2 ),
	JOYSTICK_NAMES_POV( 3 ),
#ifdef _PS3
	JOYSTICK_NAMES_POV( 4 ),
	JOYSTICK_NAMES_POV( 5 ),
	JOYSTICK_NAMES_POV( 6 ),
#endif

	JOYSTICK_NAMES_AXIS( 0 ),
	JOYSTICK_NAMES_AXIS( 1 ),
	JOYSTICK_NAMES_AXIS( 2 ),
	JOYSTICK_NAMES_AXIS( 3 ),
#ifdef _PS3
	JOYSTICK_NAMES_AXIS( 4 ),
	JOYSTICK_NAMES_AXIS( 5 ),
	JOYSTICK_NAMES_AXIS( 6 ),
#endif

	SCONTROLLERBUTTONS_BUTTONS( 0 ),	
	SCONTROLLERBUTTONS_BUTTONS( 1 ),	
	SCONTROLLERBUTTONS_BUTTONS( 2 ),	
	SCONTROLLERBUTTONS_BUTTONS( 3 ),
	SCONTROLLERBUTTONS_BUTTONS( 4 ),
	SCONTROLLERBUTTONS_BUTTONS( 5 ),
	SCONTROLLERBUTTONS_BUTTONS( 6 ),
	SCONTROLLERBUTTONS_BUTTONS( 7 ),

	SCONTROLLERBUTTONS_BUTTONS( 8 ),	
	SCONTROLLERBUTTONS_BUTTONS( 9 ),	
	SCONTROLLERBUTTONS_BUTTONS( 10 ),	
	SCONTROLLERBUTTONS_BUTTONS( 11 ),
	SCONTROLLERBUTTONS_BUTTONS( 12 ),
	SCONTROLLERBUTTONS_BUTTONS( 13 ),
	SCONTROLLERBUTTONS_BUTTONS( 14 ),
	SCONTROLLERBUTTONS_BUTTONS( 15 ),

	SCONTROLLERBUTTONS_AXIS( 0 ),	
	SCONTROLLERBUTTONS_AXIS( 1 ),	
	SCONTROLLERBUTTONS_AXIS( 2 ),	
	SCONTROLLERBUTTONS_AXIS( 3 ),	
	SCONTROLLERBUTTONS_AXIS( 4 ),
	SCONTROLLERBUTTONS_AXIS( 5 ),
	SCONTROLLERBUTTONS_AXIS( 6 ),
	SCONTROLLERBUTTONS_AXIS( 7 ),

	SCONTROLLERBUTTONS_AXIS( 8 ),	
	SCONTROLLERBUTTONS_AXIS( 9 ),	
	SCONTROLLERBUTTONS_AXIS( 10 ),	
	SCONTROLLERBUTTONS_AXIS( 11 ),	
	SCONTROLLERBUTTONS_AXIS( 12 ),
	SCONTROLLERBUTTONS_AXIS( 13 ),
	SCONTROLLERBUTTONS_AXIS( 14 ),
	SCONTROLLERBUTTONS_AXIS( 15 ),

};

#define JOYSTICK_ANALOG( x )	\
	"X AXIS",\
	"Y AXIS",\
	"Z AXIS",\
	"R AXIS",\
	"U AXIS",\
	"V AXIS"

static const char *s_pAnalogCodeName[ ] =
{
	"MOUSE_X",		// MOUSE_X = 0,
	"MOUSE_Y",		// MOUSE_Y,
	"MOUSE_XY",		// MOUSE_XY,		// Invoked when either x or y changes
	"MOUSE_WHEEL",	// MOUSE_WHEEL,

	JOYSTICK_ANALOG( 0 ),
	JOYSTICK_ANALOG( 1 ),
	JOYSTICK_ANALOG( 2 ),
	JOYSTICK_ANALOG( 3 ),
#ifdef _PS3
	JOYSTICK_ANALOG( 4 ),
	JOYSTICK_ANALOG( 5 ),
	JOYSTICK_ANALOG( 6 ),
#endif
};

#if !defined ( _GAMECONSOLE )

#define XCONTROLLERBUTTONS_BUTTONS( x ) \
	"A_BUTTON",\
	"B_BUTTON",\
	"X_BUTTON",\
	"Y_BUTTON",\
	"L_SHOULDER",\
	"R_SHOULDER",\
	"BACK",\
	"START",\
	"STICK1",\
	"STICK2",\
	"JOY11",\
	"JOY12",\
	"JOY13",\
	"JOY14",\
	"JOY15",\
	"JOY16",\
	"JOY17",\
	"JOY18",\
	"JOY19",\
	"JOY20",\
	"JOY21",\
	"JOY22",\
	"JOY23",\
	"JOY24",\
	"JOY25",\
	"JOY26",\
	"JOY27",\
	"JOY28",\
	"JOY29",\
	"JOY30",\
	"JOY31",\
	"JOY32"

#define XCONTROLLERBUTTONS_POV( x ) \
	"UP",\
	"RIGHT",\
	"DOWN",\
	"LEFT"

#define XCONTROLLERBUTTONS_AXIS( x ) \
	"S1_RIGHT",\
	"S1_LEFT",\
	"S1_DOWN",\
	"S1_UP",\
	"L_TRIGGER",\
	"R_TRIGGER",\
	"S2_RIGHT",\
	"S2_LEFT",\
	"S2_DOWN",\
	"S2_UP",\
	"V AXIS POS",\
	"V AXIS NEG"

static const char *s_pXControllerButtonCodeNames[ ] =
{
	XCONTROLLERBUTTONS_BUTTONS( 0 ),	
	XCONTROLLERBUTTONS_BUTTONS( 1 ),	
	XCONTROLLERBUTTONS_BUTTONS( 2 ),	
	XCONTROLLERBUTTONS_BUTTONS( 3 ),
#ifdef _PS3
	XCONTROLLERBUTTONS_BUTTONS( 4 ),
	XCONTROLLERBUTTONS_BUTTONS( 5 ),
	XCONTROLLERBUTTONS_BUTTONS( 6 ),
#endif

	XCONTROLLERBUTTONS_POV( 0 ),	
	XCONTROLLERBUTTONS_POV( 1 ),	
	XCONTROLLERBUTTONS_POV( 2 ),	
	XCONTROLLERBUTTONS_POV( 3 ),	
#ifdef _PS3
	XCONTROLLERBUTTONS_POV( 4 ),
	XCONTROLLERBUTTONS_POV( 5 ),
	XCONTROLLERBUTTONS_POV( 6 ),
#endif

	XCONTROLLERBUTTONS_AXIS( 0 ),	
	XCONTROLLERBUTTONS_AXIS( 1 ),	
	XCONTROLLERBUTTONS_AXIS( 2 ),	
	XCONTROLLERBUTTONS_AXIS( 3 ),	
#ifdef _PS3
	XCONTROLLERBUTTONS_AXIS( 4 ),
	XCONTROLLERBUTTONS_AXIS( 5 ),
	XCONTROLLERBUTTONS_AXIS( 6 ),
#endif

};

static const char *s_pSControllerButtonCodeNames[ ] =
{
	SCONTROLLERBUTTONS_BUTTONS( 0 ),	
	SCONTROLLERBUTTONS_BUTTONS( 1 ),	
	SCONTROLLERBUTTONS_BUTTONS( 2 ),	
	SCONTROLLERBUTTONS_BUTTONS( 3 ),
	SCONTROLLERBUTTONS_BUTTONS( 4 ),
	SCONTROLLERBUTTONS_BUTTONS( 5 ),
	SCONTROLLERBUTTONS_BUTTONS( 6 ),
	SCONTROLLERBUTTONS_BUTTONS( 7 ),

	SCONTROLLERBUTTONS_AXIS( 0 ),	
	SCONTROLLERBUTTONS_AXIS( 1 ),	
	SCONTROLLERBUTTONS_AXIS( 2 ),	
	SCONTROLLERBUTTONS_AXIS( 3 ),	
	SCONTROLLERBUTTONS_AXIS( 4 ),
	SCONTROLLERBUTTONS_AXIS( 5 ),
	SCONTROLLERBUTTONS_AXIS( 6 ),
	SCONTROLLERBUTTONS_AXIS( 7 ),

};
#endif

// this maps non-translated keyboard scan codes to engine key codes
// Google for 'Keyboard Scan Code Specification'
static ButtonCode_t s_pScanToButtonCode_QWERTY[128] = 
{ 
	//	0				1				2				3				4				5				6				7 
	//	8				9				A				B				C				D				E				F 
	KEY_NONE,		KEY_ESCAPE,		KEY_1,			KEY_2,			KEY_3,			KEY_4,			KEY_5,			KEY_6,			// 0
	KEY_7,			KEY_8,			KEY_9,			KEY_0,			KEY_MINUS,		KEY_EQUAL,		KEY_BACKSPACE,	KEY_TAB,		// 0 

	KEY_Q,			KEY_W,			KEY_E,			KEY_R,			KEY_T,			KEY_Y,			KEY_U,			KEY_I,			// 1
	KEY_O,			KEY_P,			KEY_LBRACKET,	KEY_RBRACKET,	KEY_ENTER,		KEY_LCONTROL,	KEY_A,			KEY_S,			// 1 

	KEY_D,			KEY_F,			KEY_G,			KEY_H,			KEY_J,			KEY_K,			KEY_L,			KEY_SEMICOLON,	// 2 
	KEY_APOSTROPHE,	KEY_BACKQUOTE,	KEY_LSHIFT,		KEY_BACKSLASH,	KEY_Z,			KEY_X,			KEY_C,			KEY_V,			// 2 

	KEY_B,			KEY_N,			KEY_M,			KEY_COMMA,		KEY_PERIOD,		KEY_SLASH,		KEY_RSHIFT,		KEY_PAD_MULTIPLY,// 3
	KEY_LALT,		KEY_SPACE,		KEY_CAPSLOCK,	KEY_F1,			KEY_F2,			KEY_F3,			KEY_F4,			KEY_F5,			// 3 

	KEY_F6,			KEY_F7,			KEY_F8,			KEY_F9,			KEY_F10,		KEY_NUMLOCK,	KEY_SCROLLLOCK,	KEY_HOME,		// 4
	KEY_UP,			KEY_PAGEUP,		KEY_PAD_MINUS,	KEY_LEFT,		KEY_PAD_5,		KEY_RIGHT,		KEY_PAD_PLUS,	KEY_END,		// 4 

	KEY_DOWN,		KEY_PAGEDOWN,	KEY_INSERT,		KEY_DELETE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_F11,		// 5
	KEY_F12,		KEY_BREAK,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 5

	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 6
	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 6 

	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 7
	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE		// 7 
};

static ButtonCode_t s_pScanToButtonCode[128];


void ButtonCode_InitKeyTranslationTable()
{

#if defined( _PS3 )

	COMPILE_TIME_ASSERT( sizeof(s_pPS3ButtonCodeName) / sizeof( const char * ) == BUTTON_CODE_LAST );

#endif

	COMPILE_TIME_ASSERT( sizeof(s_pButtonCodeName) / sizeof( const char * ) == BUTTON_CODE_LAST );
	COMPILE_TIME_ASSERT( sizeof(s_pAnalogCodeName) / sizeof( const char * ) == ANALOG_CODE_LAST );

// For debugging, spews entire mapping
#if 0
	for ( int i = 0; i < BUTTON_CODE_LAST; ++i )
	{
		Msg( "code %d == %s\n", i, s_pButtonCodeName[ i ] );
	}
#endif

	// set virtual key translation table
	memset( s_pVirtualKeyToButtonCode, KEY_NONE, sizeof(s_pVirtualKeyToButtonCode) );

#if defined ( _PS3 )
	s_pVirtualKeyToButtonCode[CELL_KEYC_0]			=KEY_0;
	s_pVirtualKeyToButtonCode[CELL_KEYC_1]			=KEY_1;
	s_pVirtualKeyToButtonCode[CELL_KEYC_2]			=KEY_2;
	s_pVirtualKeyToButtonCode[CELL_KEYC_3]			=KEY_3;
	s_pVirtualKeyToButtonCode[CELL_KEYC_4]			=KEY_4;
	s_pVirtualKeyToButtonCode[CELL_KEYC_5]			=KEY_5;
	s_pVirtualKeyToButtonCode[CELL_KEYC_6]			=KEY_6;
	s_pVirtualKeyToButtonCode[CELL_KEYC_7]			=KEY_7;
	s_pVirtualKeyToButtonCode[CELL_KEYC_8]			=KEY_8;
	s_pVirtualKeyToButtonCode[CELL_KEYC_9]			=KEY_9;
	s_pVirtualKeyToButtonCode[CELL_KEYC_A]			=KEY_A;
	s_pVirtualKeyToButtonCode[CELL_KEYC_B] 			=KEY_B;
	s_pVirtualKeyToButtonCode[CELL_KEYC_C] 			=KEY_C;
	s_pVirtualKeyToButtonCode[CELL_KEYC_D] 			=KEY_D;
	s_pVirtualKeyToButtonCode[CELL_KEYC_E]			=KEY_E;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F]			=KEY_F;
	s_pVirtualKeyToButtonCode[CELL_KEYC_G]			=KEY_G;
	s_pVirtualKeyToButtonCode[CELL_KEYC_H] 			=KEY_H;
	s_pVirtualKeyToButtonCode[CELL_KEYC_I]			=KEY_I;
	s_pVirtualKeyToButtonCode[CELL_KEYC_J]			=KEY_J;
	s_pVirtualKeyToButtonCode[CELL_KEYC_K]			=KEY_K;
	s_pVirtualKeyToButtonCode[CELL_KEYC_L]			=KEY_L;
	s_pVirtualKeyToButtonCode[CELL_KEYC_M]			=KEY_M;
	s_pVirtualKeyToButtonCode[CELL_KEYC_N]			=KEY_N;
	s_pVirtualKeyToButtonCode[CELL_KEYC_O]			=KEY_O;
	s_pVirtualKeyToButtonCode[CELL_KEYC_P]			=KEY_P;
	s_pVirtualKeyToButtonCode[CELL_KEYC_Q]			=KEY_Q;
	s_pVirtualKeyToButtonCode[CELL_KEYC_R]			=KEY_R;
	s_pVirtualKeyToButtonCode[CELL_KEYC_S]			=KEY_S;
	s_pVirtualKeyToButtonCode[CELL_KEYC_T]			=KEY_T;
	s_pVirtualKeyToButtonCode[CELL_KEYC_U]			=KEY_U;
	s_pVirtualKeyToButtonCode[CELL_KEYC_V]			=KEY_V;
	s_pVirtualKeyToButtonCode[CELL_KEYC_W]			=KEY_W;
	s_pVirtualKeyToButtonCode[CELL_KEYC_X]			=KEY_X;
	s_pVirtualKeyToButtonCode[CELL_KEYC_Y]			=KEY_Y;
	s_pVirtualKeyToButtonCode[CELL_KEYC_Z]			=KEY_Z;
#else
	s_pVirtualKeyToButtonCode['0']			=KEY_0;
	s_pVirtualKeyToButtonCode['1']			=KEY_1;
	s_pVirtualKeyToButtonCode['2']			=KEY_2;
	s_pVirtualKeyToButtonCode['3']			=KEY_3;
	s_pVirtualKeyToButtonCode['4']			=KEY_4;
	s_pVirtualKeyToButtonCode['5']			=KEY_5;
	s_pVirtualKeyToButtonCode['6']			=KEY_6;
	s_pVirtualKeyToButtonCode['7']			=KEY_7;
	s_pVirtualKeyToButtonCode['8']			=KEY_8;
	s_pVirtualKeyToButtonCode['9']			=KEY_9;
	s_pVirtualKeyToButtonCode['A']			=KEY_A;
	s_pVirtualKeyToButtonCode['B'] 			=KEY_B;
	s_pVirtualKeyToButtonCode['C'] 			=KEY_C;
	s_pVirtualKeyToButtonCode['D'] 			=KEY_D;
	s_pVirtualKeyToButtonCode['E']			=KEY_E;
	s_pVirtualKeyToButtonCode['F']			=KEY_F;
	s_pVirtualKeyToButtonCode['G']			=KEY_G;
	s_pVirtualKeyToButtonCode['H'] 			=KEY_H;
	s_pVirtualKeyToButtonCode['I']			=KEY_I;
	s_pVirtualKeyToButtonCode['J']			=KEY_J;
	s_pVirtualKeyToButtonCode['K']			=KEY_K;
	s_pVirtualKeyToButtonCode['L']			=KEY_L;
	s_pVirtualKeyToButtonCode['M']			=KEY_M;
	s_pVirtualKeyToButtonCode['N']			=KEY_N;
	s_pVirtualKeyToButtonCode['O']			=KEY_O;
	s_pVirtualKeyToButtonCode['P']			=KEY_P;
	s_pVirtualKeyToButtonCode['Q']			=KEY_Q;
	s_pVirtualKeyToButtonCode['R']			=KEY_R;
	s_pVirtualKeyToButtonCode['S']			=KEY_S;
	s_pVirtualKeyToButtonCode['T']			=KEY_T;
	s_pVirtualKeyToButtonCode['U']			=KEY_U;
	s_pVirtualKeyToButtonCode['V']			=KEY_V;
	s_pVirtualKeyToButtonCode['W']			=KEY_W;
	s_pVirtualKeyToButtonCode['X']			=KEY_X;
	s_pVirtualKeyToButtonCode['Y']			=KEY_Y;
	s_pVirtualKeyToButtonCode['Z']			=KEY_Z;
#endif
#if defined ( _PS3 )
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_0]	=KEY_PAD_0;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_1]	=KEY_PAD_1;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_2]	=KEY_PAD_2;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_3]	=KEY_PAD_3;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_4]	=KEY_PAD_4;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_5]	=KEY_PAD_5;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_6]	=KEY_PAD_6;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_7]	=KEY_PAD_7;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_8]	=KEY_PAD_8;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_9]	=KEY_PAD_9;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_SLASH]		=KEY_PAD_DIVIDE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_ASTERISK]	=KEY_PAD_MULTIPLY;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_MINUS]		=KEY_PAD_MINUS;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_PLUS]		=KEY_PAD_PLUS;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_ENTER]		=KEY_PAD_ENTER;
	s_pVirtualKeyToButtonCode[CELL_KEYC_KPAD_PERIOD]	=KEY_PAD_DECIMAL;
#elif !defined( PLATFORM_POSIX )
	s_pVirtualKeyToButtonCode[VK_NUMPAD0]	=KEY_PAD_0;
	s_pVirtualKeyToButtonCode[VK_NUMPAD1]	=KEY_PAD_1;
	s_pVirtualKeyToButtonCode[VK_NUMPAD2]	=KEY_PAD_2;
	s_pVirtualKeyToButtonCode[VK_NUMPAD3]	=KEY_PAD_3;
	s_pVirtualKeyToButtonCode[VK_NUMPAD4]	=KEY_PAD_4;
	s_pVirtualKeyToButtonCode[VK_NUMPAD5]	=KEY_PAD_5;
	s_pVirtualKeyToButtonCode[VK_NUMPAD6]	=KEY_PAD_6;
	s_pVirtualKeyToButtonCode[VK_NUMPAD7]	=KEY_PAD_7;
	s_pVirtualKeyToButtonCode[VK_NUMPAD8]	=KEY_PAD_8;
	s_pVirtualKeyToButtonCode[VK_NUMPAD9]	=KEY_PAD_9;
	s_pVirtualKeyToButtonCode[VK_DIVIDE]	=KEY_PAD_DIVIDE;
	s_pVirtualKeyToButtonCode[VK_MULTIPLY]	=KEY_PAD_MULTIPLY;
	s_pVirtualKeyToButtonCode[VK_SUBTRACT]	=KEY_PAD_MINUS;
	s_pVirtualKeyToButtonCode[VK_ADD]		=KEY_PAD_PLUS;
	s_pVirtualKeyToButtonCode[VK_RETURN]	=KEY_PAD_ENTER;
	s_pVirtualKeyToButtonCode[VK_DECIMAL]	=KEY_PAD_DECIMAL;
#endif
#if defined ( _PS3 )
	s_pVirtualKeyToButtonCode[CELL_KEYC_LEFT_BRACKET_101]		=KEY_LBRACKET;
	s_pVirtualKeyToButtonCode[CELL_KEYC_RIGHT_BRACKET_101]		=KEY_RBRACKET;
	s_pVirtualKeyToButtonCode[CELL_KEYC_SEMICOLON]				=KEY_SEMICOLON;
	s_pVirtualKeyToButtonCode[CELL_KEYC_QUOTATION_101]			=KEY_APOSTROPHE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_106_KANJI]				=KEY_BACKQUOTE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_COMMA]					=KEY_COMMA;
	s_pVirtualKeyToButtonCode[CELL_KEYC_PERIOD]					=KEY_PERIOD;
	s_pVirtualKeyToButtonCode[CELL_KEYC_SLASH]					=KEY_SLASH;
	s_pVirtualKeyToButtonCode[CELL_KEYC_BACKSLASH_101]			=KEY_BACKSLASH;
	s_pVirtualKeyToButtonCode[CELL_KEYC_MINUS]					=KEY_MINUS;
	s_pVirtualKeyToButtonCode[CELL_KEYC_EQUAL_101]				=KEY_EQUAL;
#else
	s_pVirtualKeyToButtonCode[0xdb]			=KEY_LBRACKET;
	s_pVirtualKeyToButtonCode[0xdd]			=KEY_RBRACKET;
	s_pVirtualKeyToButtonCode[0xba]			=KEY_SEMICOLON;
	s_pVirtualKeyToButtonCode[0xde]			=KEY_APOSTROPHE;
	s_pVirtualKeyToButtonCode[0xc0]			=KEY_BACKQUOTE;
	s_pVirtualKeyToButtonCode[0xbc]			=KEY_COMMA;
	s_pVirtualKeyToButtonCode[0xbe]			=KEY_PERIOD;
	s_pVirtualKeyToButtonCode[0xbf]			=KEY_SLASH;
	s_pVirtualKeyToButtonCode[0xdc]			=KEY_BACKSLASH;
	s_pVirtualKeyToButtonCode[0xbd]			=KEY_MINUS;
	s_pVirtualKeyToButtonCode[0xbb]			=KEY_EQUAL;
#endif
#if defined ( _PS3 )
	s_pVirtualKeyToButtonCode[CELL_KEYC_ENTER]	=KEY_ENTER;
	s_pVirtualKeyToButtonCode[CELL_KEYC_SPACE]	=KEY_SPACE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_BS]		=KEY_BACKSPACE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_TAB]	=KEY_TAB;
	s_pVirtualKeyToButtonCode[CELL_KEYC_CAPS_LOCK]	=KEY_CAPSLOCK;
	s_pVirtualKeyToButtonCode[CELL_KEYC_NUM_LOCK]	=KEY_NUMLOCK;
	s_pVirtualKeyToButtonCode[CELL_KEYC_ESC]		=KEY_ESCAPE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_SCROLL_LOCK]	=KEY_SCROLLLOCK;
	s_pVirtualKeyToButtonCode[CELL_KEYC_INSERT]			=KEY_INSERT;
	s_pVirtualKeyToButtonCode[CELL_KEYC_DELETE]			=KEY_DELETE;
	s_pVirtualKeyToButtonCode[CELL_KEYC_HOME]			=KEY_HOME;
	s_pVirtualKeyToButtonCode[CELL_KEYC_END]			=KEY_END;
	s_pVirtualKeyToButtonCode[CELL_KEYC_PAGE_UP]		=KEY_PAGEUP;
	s_pVirtualKeyToButtonCode[CELL_KEYC_PAGE_DOWN]		=KEY_PAGEDOWN;
	s_pVirtualKeyToButtonCode[CELL_KEYC_PAUSE]			=KEY_BREAK;
	s_pVirtualKeyToButtonCode[CELL_KEYC_APPLICATION]	=KEY_APP;
	s_pVirtualKeyToButtonCode[CELL_KEYC_UP_ARROW]		=KEY_UP;
	s_pVirtualKeyToButtonCode[CELL_KEYC_LEFT_ARROW]		=KEY_LEFT;
	s_pVirtualKeyToButtonCode[CELL_KEYC_DOWN_ARROW]		=KEY_DOWN;
	s_pVirtualKeyToButtonCode[CELL_KEYC_RIGHT_ARROW]	=KEY_RIGHT;	
	s_pVirtualKeyToButtonCode[CELL_KEYC_F1]		=KEY_F1;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F2]		=KEY_F2;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F3]		=KEY_F3;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F4]		=KEY_F4;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F5]		=KEY_F5;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F6]		=KEY_F6;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F7]		=KEY_F7;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F8]		=KEY_F8;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F9]		=KEY_F9;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F10]	=KEY_F10;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F11]	=KEY_F11;
	s_pVirtualKeyToButtonCode[CELL_KEYC_F12]	=KEY_F12;
#elif !defined( PLATFORM_POSIX )
	s_pVirtualKeyToButtonCode[VK_RETURN]	=KEY_ENTER;
	s_pVirtualKeyToButtonCode[VK_SPACE]		=KEY_SPACE;
	s_pVirtualKeyToButtonCode[VK_BACK]		=KEY_BACKSPACE;
	s_pVirtualKeyToButtonCode[VK_TAB]		=KEY_TAB;
	s_pVirtualKeyToButtonCode[VK_CAPITAL]	=KEY_CAPSLOCK;
	s_pVirtualKeyToButtonCode[VK_NUMLOCK]	=KEY_NUMLOCK;
	s_pVirtualKeyToButtonCode[VK_ESCAPE]	=KEY_ESCAPE;
	s_pVirtualKeyToButtonCode[VK_SCROLL]	=KEY_SCROLLLOCK;
	s_pVirtualKeyToButtonCode[VK_INSERT]	=KEY_INSERT;
	s_pVirtualKeyToButtonCode[VK_DELETE]	=KEY_DELETE;
	s_pVirtualKeyToButtonCode[VK_HOME]		=KEY_HOME;
	s_pVirtualKeyToButtonCode[VK_END]		=KEY_END;
	s_pVirtualKeyToButtonCode[VK_PRIOR]		=KEY_PAGEUP;
	s_pVirtualKeyToButtonCode[VK_NEXT]		=KEY_PAGEDOWN;
	s_pVirtualKeyToButtonCode[VK_PAUSE]		=KEY_BREAK;
	s_pVirtualKeyToButtonCode[VK_SHIFT]		=KEY_RSHIFT;
	s_pVirtualKeyToButtonCode[VK_SHIFT]		=KEY_LSHIFT;	// SHIFT -> left SHIFT
	s_pVirtualKeyToButtonCode[VK_MENU]		=KEY_RALT;
	s_pVirtualKeyToButtonCode[VK_MENU]		=KEY_LALT;		// ALT -> left ALT
	s_pVirtualKeyToButtonCode[VK_CONTROL]	=KEY_RCONTROL;
	s_pVirtualKeyToButtonCode[VK_CONTROL]	=KEY_LCONTROL;	// CTRL -> left CTRL
	s_pVirtualKeyToButtonCode[VK_LWIN]		=KEY_LWIN;
	s_pVirtualKeyToButtonCode[VK_RWIN]		=KEY_RWIN;
	s_pVirtualKeyToButtonCode[VK_APPS]		=KEY_APP;
	s_pVirtualKeyToButtonCode[VK_UP]		=KEY_UP;
	s_pVirtualKeyToButtonCode[VK_LEFT]		=KEY_LEFT;
	s_pVirtualKeyToButtonCode[VK_DOWN]		=KEY_DOWN;
	s_pVirtualKeyToButtonCode[VK_RIGHT]		=KEY_RIGHT;	
	s_pVirtualKeyToButtonCode[VK_F1]		=KEY_F1;
	s_pVirtualKeyToButtonCode[VK_F2]		=KEY_F2;
	s_pVirtualKeyToButtonCode[VK_F3]		=KEY_F3;
	s_pVirtualKeyToButtonCode[VK_F4]		=KEY_F4;
	s_pVirtualKeyToButtonCode[VK_F5]		=KEY_F5;
	s_pVirtualKeyToButtonCode[VK_F6]		=KEY_F6;
	s_pVirtualKeyToButtonCode[VK_F7]		=KEY_F7;
	s_pVirtualKeyToButtonCode[VK_F8]		=KEY_F8;
	s_pVirtualKeyToButtonCode[VK_F9]		=KEY_F9;
	s_pVirtualKeyToButtonCode[VK_F10]		=KEY_F10;
	s_pVirtualKeyToButtonCode[VK_F11]		=KEY_F11;
	s_pVirtualKeyToButtonCode[VK_F12]		=KEY_F12;
#endif

	// init the xkey translation table
#if !defined( PLATFORM_POSIX ) || defined( _GAMECONSOLE ) || defined( _OSX )
	s_pXKeyTrans[XK_NULL]					= KEY_NONE;
	s_pXKeyTrans[XK_BUTTON_UP]				= KEY_XBUTTON_UP;
	s_pXKeyTrans[XK_BUTTON_DOWN]			= KEY_XBUTTON_DOWN;
	s_pXKeyTrans[XK_BUTTON_LEFT]			= KEY_XBUTTON_LEFT;
	s_pXKeyTrans[XK_BUTTON_RIGHT]			= KEY_XBUTTON_RIGHT;
	s_pXKeyTrans[XK_BUTTON_START]			= KEY_XBUTTON_START;
	s_pXKeyTrans[XK_BUTTON_BACK]			= KEY_XBUTTON_BACK;
	s_pXKeyTrans[XK_BUTTON_STICK1]			= KEY_XBUTTON_STICK1;
	s_pXKeyTrans[XK_BUTTON_STICK2]			= KEY_XBUTTON_STICK2;
	s_pXKeyTrans[XK_BUTTON_A]				= KEY_XBUTTON_A;
	s_pXKeyTrans[XK_BUTTON_B]				= KEY_XBUTTON_B;
	s_pXKeyTrans[XK_BUTTON_X]				= KEY_XBUTTON_X;
	s_pXKeyTrans[XK_BUTTON_Y]				= KEY_XBUTTON_Y;
	s_pXKeyTrans[XK_BUTTON_LEFT_SHOULDER]   = KEY_XBUTTON_LEFT_SHOULDER;
	s_pXKeyTrans[XK_BUTTON_RIGHT_SHOULDER]	= KEY_XBUTTON_RIGHT_SHOULDER;
	s_pXKeyTrans[XK_BUTTON_LTRIGGER]		= KEY_XBUTTON_LTRIGGER;
	s_pXKeyTrans[XK_BUTTON_RTRIGGER]		= KEY_XBUTTON_RTRIGGER;
	s_pXKeyTrans[XK_STICK1_UP]				= KEY_XSTICK1_UP;
	s_pXKeyTrans[XK_STICK1_DOWN]			= KEY_XSTICK1_DOWN;
	s_pXKeyTrans[XK_STICK1_LEFT]			= KEY_XSTICK1_LEFT;
	s_pXKeyTrans[XK_STICK1_RIGHT]			= KEY_XSTICK1_RIGHT;
	s_pXKeyTrans[XK_STICK2_UP]				= KEY_XSTICK2_UP;
	s_pXKeyTrans[XK_STICK2_DOWN]			= KEY_XSTICK2_DOWN;
	s_pXKeyTrans[XK_STICK2_LEFT]			= KEY_XSTICK2_LEFT;
	s_pXKeyTrans[XK_STICK2_RIGHT]			= KEY_XSTICK2_RIGHT;
	s_pXKeyTrans[XK_BUTTON_INACTIVE_START]	= KEY_XBUTTON_INACTIVE_START;

	s_pXKeyTrans[XK_BUTTON_FIREMODE_SELECTOR_1]	= KEY_XBUTTON_FIREMODE_SELECTOR_1;
	s_pXKeyTrans[XK_BUTTON_FIREMODE_SELECTOR_2]	= KEY_XBUTTON_FIREMODE_SELECTOR_2;
	s_pXKeyTrans[XK_BUTTON_FIREMODE_SELECTOR_3]	= KEY_XBUTTON_FIREMODE_SELECTOR_3;
	s_pXKeyTrans[XK_BUTTON_RELOAD]				= KEY_XBUTTON_RELOAD;
	s_pXKeyTrans[XK_BUTTON_TRIGGER]				= KEY_XBUTTON_TRIGGER;
	s_pXKeyTrans[XK_BUTTON_PUMP_ACTION]			= KEY_XBUTTON_PUMP_ACTION;
	s_pXKeyTrans[XK_XBUTTON_ROLL_RIGHT]			= KEY_XBUTTON_ROLL_RIGHT;
	s_pXKeyTrans[XK_XBUTTON_ROLL_LEFT]			= KEY_XBUTTON_ROLL_LEFT;
#endif // PLATFORM_POSIX

	// create reverse table engine to virtual
	for ( int i = 0; i < ARRAYSIZE( s_pVirtualKeyToButtonCode ); i++ )
	{
		s_pButtonCodeToVirtual[ s_pVirtualKeyToButtonCode[i] ] = i;
	}

	s_pButtonCodeToVirtual[0] = 0;

	s_pSKeytoButtonCode[SK_NULL]					= KEY_NONE;
	s_pSKeytoButtonCode[SK_BUTTON_A]				= STEAMCONTROLLER_A;
	s_pSKeytoButtonCode[SK_BUTTON_B]				= STEAMCONTROLLER_B;
	s_pSKeytoButtonCode[SK_BUTTON_X]				= STEAMCONTROLLER_X;
	s_pSKeytoButtonCode[SK_BUTTON_Y]				= STEAMCONTROLLER_Y;
	s_pSKeytoButtonCode[SK_BUTTON_UP]				= STEAMCONTROLLER_DPAD_UP;
	s_pSKeytoButtonCode[SK_BUTTON_RIGHT]			= STEAMCONTROLLER_DPAD_RIGHT;
	s_pSKeytoButtonCode[SK_BUTTON_DOWN]				= STEAMCONTROLLER_DPAD_DOWN;
	s_pSKeytoButtonCode[SK_BUTTON_LEFT]				= STEAMCONTROLLER_DPAD_LEFT;
	s_pSKeytoButtonCode[SK_BUTTON_LEFT_BUMPER]		= STEAMCONTROLLER_LEFT_BUMPER;
	s_pSKeytoButtonCode[SK_BUTTON_RIGHT_BUMPER]		= STEAMCONTROLLER_RIGHT_BUMPER;
	s_pSKeytoButtonCode[SK_BUTTON_LEFT_TRIGGER]		= STEAMCONTROLLER_LEFT_TRIGGER;
	s_pSKeytoButtonCode[SK_BUTTON_RIGHT_TRIGGER]	= STEAMCONTROLLER_RIGHT_TRIGGER;
	s_pSKeytoButtonCode[SK_BUTTON_LEFT_GRIP]		= STEAMCONTROLLER_LEFT_GRIP;
	s_pSKeytoButtonCode[SK_BUTTON_RIGHT_GRIP]		= STEAMCONTROLLER_RIGHT_GRIP;
	s_pSKeytoButtonCode[SK_BUTTON_LPAD_TOUCH]		= STEAMCONTROLLER_LEFT_PAD_FINGERDOWN;
	s_pSKeytoButtonCode[SK_BUTTON_RPAD_TOUCH]		= STEAMCONTROLLER_RIGHT_PAD_FINGERDOWN;
	s_pSKeytoButtonCode[SK_BUTTON_LPAD_CLICK]		= STEAMCONTROLLER_LEFT_PAD_CLICK;
	s_pSKeytoButtonCode[SK_BUTTON_RPAD_CLICK]		= STEAMCONTROLLER_RIGHT_PAD_CLICK;
	s_pSKeytoButtonCode[SK_BUTTON_LPAD_UP]			= STEAMCONTROLLER_LEFT_PAD_UP;
	s_pSKeytoButtonCode[SK_BUTTON_LPAD_RIGHT]		= STEAMCONTROLLER_LEFT_PAD_RIGHT;
	s_pSKeytoButtonCode[SK_BUTTON_LPAD_DOWN]		= STEAMCONTROLLER_LEFT_PAD_DOWN;
	s_pSKeytoButtonCode[SK_BUTTON_LPAD_LEFT]		= STEAMCONTROLLER_LEFT_PAD_LEFT;
	s_pSKeytoButtonCode[SK_BUTTON_RPAD_UP]			= STEAMCONTROLLER_RIGHT_PAD_UP;
	s_pSKeytoButtonCode[SK_BUTTON_RPAD_RIGHT]		= STEAMCONTROLLER_RIGHT_PAD_RIGHT;
	s_pSKeytoButtonCode[SK_BUTTON_RPAD_DOWN]		= STEAMCONTROLLER_RIGHT_PAD_DOWN;
	s_pSKeytoButtonCode[SK_BUTTON_RPAD_LEFT]		= STEAMCONTROLLER_RIGHT_PAD_LEFT;
	s_pSKeytoButtonCode[SK_BUTTON_SELECT]			= STEAMCONTROLLER_SELECT;
	s_pSKeytoButtonCode[SK_BUTTON_START]			= STEAMCONTROLLER_START;
	s_pSKeytoButtonCode[SK_BUTTON_STEAM]			= STEAMCONTROLLER_STEAM;
	s_pSKeytoButtonCode[SK_BUTTON_INACTIVE_START]	= STEAMCONTROLLER_INACTIVE_START;
}

ButtonCode_t ButtonCode_VirtualKeyToButtonCode( int keyCode )
{
	if ( keyCode < 0 || keyCode >= sizeof( s_pVirtualKeyToButtonCode ) / sizeof( s_pVirtualKeyToButtonCode[0] ) )
	{
		Assert( false );
		return KEY_NONE;
	}
	return s_pVirtualKeyToButtonCode[keyCode];
}

int ButtonCode_ButtonCodeToVirtualKey( ButtonCode_t code )
{
	return s_pButtonCodeToVirtual[code];
}

ButtonCode_t ButtonCode_XKeyToButtonCode( int nPort, int keyCode )
{
#if !defined( PLATFORM_POSIX ) || defined( _GAMECONSOLE ) || defined( _OSX )
	if ( keyCode < 0 || keyCode >= sizeof( s_pXKeyTrans ) / sizeof( s_pXKeyTrans[0] ) )
	{
		Assert( false );
		return KEY_NONE;
	}

	ButtonCode_t code = s_pXKeyTrans[keyCode];
	if ( IsJoystickButtonCode( code ) )
	{
		int nOffset = code - JOYSTICK_FIRST_BUTTON;
		return JOYSTICK_BUTTON( nPort, nOffset );
	}
	
	if ( IsJoystickPOVCode( code ) )
	{
		int nOffset = code - JOYSTICK_FIRST_POV_BUTTON;
		return JOYSTICK_POV_BUTTON( nPort, nOffset );
	}
	
	if ( IsJoystickAxisCode( code ) )
	{
		int nOffset = code - JOYSTICK_FIRST_AXIS_BUTTON;
		return JOYSTICK_AXIS_BUTTON( nPort, nOffset );
	}

	return code;
#else // PLATFORM_POSIX
	return KEY_NONE;
#endif // PLATFORM_POSIX
}

ButtonCode_t ButtonCode_SKeyToButtonCode( int nPort, int keyCode )
{
#if !defined( _GAMECONSOLE )
	if ( keyCode < 0 || keyCode >= sizeof( s_pSKeytoButtonCode ) / sizeof( s_pSKeytoButtonCode[0] ) )
	{
		Assert( false );
		return KEY_NONE;
	}

	ButtonCode_t code = s_pSKeytoButtonCode[keyCode];
// 	if ( IsSteamControllerCode( code ) )
// 	{
// 		// Need Per Controller Offset here.
// 		return code;
// 	}

	if ( IsSteamControllerButtonCode( code ) )
	{
		int nOffset = code - STEAMCONTROLLER_FIRST_BUTTON;
		return STEAMCONTROLLER_BUTTON( nPort, nOffset );
	}

	if ( IsSteamControllerAxisCode( code ) )
	{
		int nOffset = code - STEAMCONTROLLER_FIRST_AXIS_BUTTON;
		return STEAMCONTROLLER_AXIS_BUTTON( nPort, nOffset );
	}

	return code;
#else // _GAMECONSOLE
	return KEY_NONE;
#endif // _GAMECONSOLE
}

// Convert back + forth between ButtonCode/AnalogCode + strings
const char *ButtonCode_ButtonCodeToString( ButtonCode_t code, bool bXController )
{
#if !defined ( _GAMECONSOLE )
	if ( bXController )
	{
		if ( IsJoystickButtonCode( code ) )
		{
			int offset = ( code - JOYSTICK_FIRST_BUTTON ) % JOYSTICK_MAX_BUTTON_COUNT;

			return s_pXControllerButtonCodeNames[ offset ];
		}

		if ( IsJoystickPOVCode( code ) )
		{
			int offset = ( code - JOYSTICK_FIRST_POV_BUTTON ) % JOYSTICK_POV_BUTTON_COUNT;

			return s_pXControllerButtonCodeNames[ MAX_JOYSTICKS * JOYSTICK_MAX_BUTTON_COUNT + offset ];
		}

		if ( IsJoystickAxisCode( code ) )
		{
			int offset = ( code - JOYSTICK_FIRST_AXIS_BUTTON ) % JOYSTICK_AXIS_BUTTON_COUNT;

			return s_pXControllerButtonCodeNames[ MAX_JOYSTICKS * ( JOYSTICK_POV_BUTTON_COUNT + JOYSTICK_MAX_BUTTON_COUNT ) + offset ];
		}
	}
#endif

	return s_pButtonCodeName[ code ];
}

const char *AnalogCode_AnalogCodeToString( AnalogCode_t code )
{
	return s_pAnalogCodeName[ code ];
}

ButtonCode_t ButtonCode_StringToButtonCode( const char *pString, bool bXController )
{
	if ( !pString || !pString[0] )
		return BUTTON_CODE_INVALID;

	// Backward compat for screwed up previous joystick button names
	if ( !Q_strnicmp( pString, "aux", 3 ) )
	{
		int nIndex = atoi( &pString[3] );
		if ( nIndex < 29 )
			return JOYSTICK_BUTTON( 0, nIndex );
		if ( ( nIndex >= 29 ) && ( nIndex <= 32 ) )
			return JOYSTICK_POV_BUTTON( 0, nIndex - 29 );
		return BUTTON_CODE_INVALID;
	}

#if defined(OSX)
  // map "l_win" to the LWIN key on OSX (it appears in the table as "command" )
  if ( !Q_stricmp( pString, "lwin" ) )
#else
  // map "COMMAND" to the LWIN key on non-OSX
  if ( !Q_stricmp( pString, "command" ) )
#endif
  {
    return KEY_LWIN;
  }
  
#if defined( _PS3 )

	// For PS3, we want to check against specific PS3 button code names.
	for ( int i = 0; i < BUTTON_CODE_LAST; ++i )
	{
		if ( !Q_stricmp( s_pPS3ButtonCodeName[i], pString ) )
			return (ButtonCode_t)i;
	}

#endif

	for ( int i = 0; i < BUTTON_CODE_LAST; ++i )
	{
		if ( !Q_stricmp( s_pButtonCodeName[i], pString ) )
			return (ButtonCode_t)i;
	}

#if !defined ( _GAMECONSOLE )
	if ( bXController )
	{
		for ( int i = 0; i < ARRAYSIZE(s_pXControllerButtonCodeNames); ++i )
		{
			if ( !Q_stricmp( s_pXControllerButtonCodeNames[i], pString ) )
				return (ButtonCode_t)(JOYSTICK_FIRST_BUTTON + i);
		}
	}
#endif

	return BUTTON_CODE_INVALID;
}

AnalogCode_t AnalogCode_StringToAnalogCode( const char *pString )
{
	if ( !pString || !pString[0] )
		return ANALOG_CODE_INVALID;

	for ( int i = 0; i < ANALOG_CODE_LAST; ++i )
	{
		if ( !Q_stricmp( s_pAnalogCodeName[i], pString ) )
			return (AnalogCode_t)i;
	}

	return ANALOG_CODE_INVALID;
}

ButtonCode_t ButtonCode_ScanCodeToButtonCode( int lParam )
{
	int nScanCode = ( lParam >> 16 ) & 0xFF;
	if ( nScanCode > 127 )
		return KEY_NONE;

	ButtonCode_t result = s_pScanToButtonCode[nScanCode];

	bool bIsExtended = ( lParam & ( 1 << 24 ) ) != 0;
	if ( !bIsExtended )
	{
		switch ( result )
		{
		case KEY_HOME:
			return KEY_PAD_7;
		case KEY_UP:
			return KEY_PAD_8;
		case KEY_PAGEUP:
			return KEY_PAD_9;
		case KEY_LEFT:
			return KEY_PAD_4;
		case KEY_RIGHT:
			return KEY_PAD_6;
		case KEY_END:
			return KEY_PAD_1;
		case KEY_DOWN:
			return KEY_PAD_2;
		case KEY_PAGEDOWN:
			return KEY_PAD_3;
		case KEY_INSERT:
			return KEY_PAD_0;
		case KEY_DELETE:
			return KEY_PAD_DECIMAL;
		default:
			break;
		}
	}
	else
	{
		switch ( result )
		{
		case KEY_ENTER:
			return KEY_PAD_ENTER;
		case KEY_LALT:
			return KEY_RALT;
		case KEY_LCONTROL:
			return KEY_RCONTROL;
		case KEY_SLASH:
			return KEY_PAD_DIVIDE;
		case KEY_CAPSLOCK:
			return KEY_PAD_PLUS;
		}
	}

	return result;
}


//-----------------------------------------------------------------------------
// Update scan codes for foreign keyboards
//-----------------------------------------------------------------------------
void ButtonCode_UpdateScanCodeLayout( )
{
	// reset the keyboard
	memcpy( s_pScanToButtonCode, s_pScanToButtonCode_QWERTY, sizeof(s_pScanToButtonCode) );

#if !defined( _GAMECONSOLE ) && !defined( PLATFORM_POSIX )
	// fix up keyboard layout for other languages
	HKL currentKb = ::GetKeyboardLayout( 0 );
	HKL englishKb = ::LoadKeyboardLayout("00000409", 0);

	if (englishKb && englishKb != currentKb)
	{
		for ( int i = 0; i < ARRAYSIZE(s_pScanToButtonCode); i++ )
		{
			// take the english/QWERTY
			ButtonCode_t code = s_pScanToButtonCode_QWERTY[ i ];

			// only remap printable keys
			if ( code != KEY_NONE && code != KEY_BACKQUOTE && ( IsAlphaNumeric( code ) || IsPunctuation( code ) ) )
			{
				// get it's virtual key based on the old layout
				int vk = ::MapVirtualKeyEx( i, 1, englishKb );

				// turn in into a scancode on the new layout
				int newScanCode = ::MapVirtualKeyEx( vk, 0, currentKb );

				// strip off any high bits
				newScanCode &= 0x0000007F;

				// set in the new layout
				s_pScanToButtonCode[newScanCode] = code;
			}
		}
	}

	s_pScanToButtonCode[0] = KEY_NONE;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Updates the current keyboard layout
//-----------------------------------------------------------------------------
CON_COMMAND( key_updatelayout, "Updates game keyboard layout to current windows keyboard setting." )
{
	ButtonCode_UpdateScanCodeLayout();
}

//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: Win32 replacements for Xbox code
//
//=============================================================================
#ifndef PS3STUBS_H
#define PS3STUBS_H

#include "ps3_platform.h"
#include "tier0/platform.h"

//  Content creation/open flags
#define XCONTENTFLAG_NONE                           0x00
#define XCONTENTFLAG_CREATENEW                      0x00
#define XCONTENTFLAG_CREATEALWAYS                   0x00
#define XCONTENTFLAG_OPENEXISTING                   0x00
#define XCONTENTFLAG_OPENALWAYS                     0x00
#define XCONTENTFLAG_TRUNCATEEXISTING               0x00

//  Content attributes
#define XCONTENTFLAG_NOPROFILE_TRANSFER             0x00
#define XCONTENTFLAG_NODEVICE_TRANSFER              0x00
#define XCONTENTFLAG_STRONG_SIGNED                  0x00
#define XCONTENTFLAG_ALLOWPROFILE_TRANSFER          0x00
#define XCONTENTFLAG_MOVEONLY_TRANSFER              0x00

#define XDEVICE_TYPE_GAMEPAD					0
#define XDEVICE_TYPE_MEMORY_UNIT				1
#define XDEVICE_TYPE_VOICE_MICROPHONE			2
#define XDEVICE_TYPE_VOICE_HEADPHONE			3
#define XDEVICE_TYPE_HIGHFIDELITY_MICROPHONE	4

// Constants for gamepad buttons
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100 // AG -  Shoudler buttons should correspond to BLACK and WHITE in xboxStubs.h, but they don't seem to have proper bitfield values
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_LEFT_TRIGGER     0x0400
#define XINPUT_GAMEPAD_RIGHT_TRIGGER    0x0800
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

#define XINPUT_LIGHTGUN_ONSCREEN         0x2000
#define XINPUT_LIGHTGUN_FRAME_DOUBLER    0x4000
#define XINPUT_LIGHTGUN_LINE_DOUBLER     0x8000

//#define XINPUT_GAMEPAD_LEFT_TRIGGER     6
//#define XINPUT_GAMEPAD_RIGHT_TRIGGER    7

#define XDEVICE_PORT0               0
#define XDEVICE_PORT1               1
#define XDEVICE_PORT2               2
#define XDEVICE_PORT3               3
#ifdef CONTROLLER2_CHEATS_ENABLED
#define XBX_MAX_DPORTS				2
#else
#define XBX_MAX_DPORTS				1
#endif // CONTROLLER2_CHEATS_ENABLED

#define XDEVICE_NO_SLOT             0
#define XDEVICE_TOP_SLOT            0
#define XDEVICE_BOTTOM_SLOT         1

#define CLR_DEFAULT					0xFF000000
#define CLR_WARNING					0x0000FFFF
#define CLR_ERROR					0x000000FF

// Device types available in XINPUT_CAPABILITIES
#define XINPUT_DEVTYPE_GAMEPAD          0x01
#define XINPUT_DEVTYPE_USB_KEYBOARD     0x02

// Device subtypes available in XINPUT_CAPABILITIES
#define XINPUT_DEVSUBTYPE_UNKNOWN       0x00
#define XINPUT_DEVSUBTYPE_GAMEPAD       0x01
#define XINPUT_DEVSUBTYPE_WHEEL         0x02
#define XINPUT_DEVSUBTYPE_ARCADE_STICK  0x03
#define XINPUT_DEVSUBTYPE_FLIGHT_STICK  0x04
#define XINPUT_DEVSUBTYPE_DANCEPAD      0x05

// Flags for XINPUT_CAPABILITIES
#define XINPUT_CAPS_FFB_SUPPORTED       0x0001
#define XINPUT_CAPS_WIRELESS            0x0002
#define XINPUT_CAPS_VOICE_SUPPORTED     0x0004
#define XINPUT_CAPS_PMD_SUPPORTED       0x0008

// Flags for XInputGetCapabilities
#define XINPUT_FLAG_GAMEPAD             0x00000001
#define XINPUT_FLAG_KEYBOARD            0x00000002
#define XINPUT_FLAG_REMOTE              0x00000004
#define XINPUT_FLAG_ANYDEVICE           0x000000FF

// Gamepad thresholds
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30

// Wind


// Structures used by XInput APIs
typedef struct _XINPUT_GAMEPAD
{
	WORD                                wButtons;
	BYTE                                bLeftTrigger;
	BYTE                                bRightTrigger;
	short                               sThumbLX;
	short                               sThumbLY;
	short                               sThumbRX;
	short                               sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
	DWORD                               dwPacketNumber;
	XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION
{
	WORD                                wLeftMotorSpeed;
	WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;

typedef struct _XINPUT_CAPABILITIES
{
	BYTE                                Type;
	BYTE                                SubType;
	WORD                                Flags;
	XINPUT_GAMEPAD                      Gamepad;
	XINPUT_VIBRATION                    Vibration;
} XINPUT_CAPABILITIES, *PXINPUT_CAPABILITIES;

//typedef enum
//{
//	XK_NULL,
//	XK_BUTTON_UP,
//	XK_BUTTON_DOWN,
//	XK_BUTTON_LEFT,
//	XK_BUTTON_RIGHT,
//	XK_BUTTON_START,
//	XK_BUTTON_BACK,
//	XK_BUTTON_STICK1,
//	XK_BUTTON_STICK2,
//	XK_BUTTON_A,
//	XK_BUTTON_B,
//	XK_BUTTON_X,
//	XK_BUTTON_Y,
//	XK_BUTTON_BLACK,
//	XK_BUTTON_WHITE,
//	XK_BUTTON_LTRIGGER,
//	XK_BUTTON_RTRIGGER,
//	XK_STICK1_UP,
//	XK_STICK1_DOWN,
//	XK_STICK1_LEFT,
//	XK_STICK1_RIGHT,
//	XK_STICK2_UP,
//	XK_STICK2_DOWN,
//	XK_STICK2_LEFT,
//	XK_STICK2_RIGHT,
//	XK_MAX_KEYS,
//} xKey_t;
//
//typedef enum
//{
//	XVRB_NONE,		// off
//	XVRB_ERROR,		// fatal error
//	XVRB_ALWAYS,	// no matter what
//	XVRB_WARNING,	// non-fatal warnings
//	XVRB_STATUS,	// status reports
//	XVRB_ALL,
//} xverbose_e;

typedef struct _XINPUT_RUMBLE
{
   WORD   wLeftMotorSpeed;
   WORD   wRightMotorSpeed;
} XINPUT_RUMBLE, *PXINPUT_RUMBLE;

#define XINPUT_FEEDBACK_HEADER_INTERNAL_SIZE 58
typedef struct _XINPUT_FEEDBACK_HEADER
{
    DWORD dwStatus;
    void* hEvent;
    BYTE  Reserved[XINPUT_FEEDBACK_HEADER_INTERNAL_SIZE];
} XINPUT_FEEDBACK_HEADER, *PXINPUT_FEEDBACK_HEADER;

typedef struct _XINPUT_FEEDBACK
{
    XINPUT_FEEDBACK_HEADER Header;
    union
    {
      XINPUT_RUMBLE Rumble;
    };
} XINPUT_FEEDBACK, *PXINPUT_FEEDBACK;

//typedef struct _XINPUT_GAMEPAD
//{
//    WORD    wButtons;
//    BYTE    bAnalogButtons[8];
//    short   sThumbLX;
//    short   sThumbLY;
//    short   sThumbRX;
//    short   sThumbRY;
//} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XPP_DEVICE_TYPE
{
	unsigned long Reserved[3];
} XPP_DEVICE_TYPE, *PXPP_DEVICE_TYPE;

typedef struct _XDEVICE_PREALLOC_TYPE
{
    PXPP_DEVICE_TYPE	DeviceType;
    DWORD				dwPreallocCount;
} XDEVICE_PREALLOC_TYPE, *PXDEVICE_PREALLOC_TYPE;

//typedef struct _XINPUT_STATE
//{
//    DWORD			dwPacketNumber;
//    XINPUT_GAMEPAD	Gamepad;
//} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_POLLING_PARAMETERS
{
    BYTE       fAutoPoll:1;
    BYTE       fInterruptOut:1;
    BYTE       ReservedMBZ1:6;
    BYTE       bInputInterval;
    BYTE       bOutputInterval;
    BYTE       ReservedMBZ2;
} XINPUT_POLLING_PARAMETERS, *PXINPUT_POLLING_PARAMETERS;

/*EAPS3
void			XBX_DebugString(xverbose_e verbose, COLORREF color, const char* format, ...);
void			XBX_ProcessEvents(void);
void			XInitDevices(DWORD dwPreallocTypeCount, PXDEVICE_PREALLOC_TYPE PreallocTypes);
DWORD			XGetDevices(PXPP_DEVICE_TYPE DeviceType);
bool			XGetDeviceChanges(PXPP_DEVICE_TYPE DeviceType, DWORD *pdwInsertions, DWORD *pdwRemovals);
HANDLE			XInputOpen(PXPP_DEVICE_TYPE DeviceType, DWORD dwPort, DWORD dwSlot, PXINPUT_POLLING_PARAMETERS pPollingParameters);
void			XInputClose(HANDLE hDevice);
DWORD			XInputSetState(HANDLE hDevice, PXINPUT_FEEDBACK pFeedback);
DWORD			XInputGetState(HANDLE hDevice, PXINPUT_STATE  pState);
DWORD			XInputPoll(HANDLE hDevice);
unsigned int	XBX_GetSystemTime(void);
*/

#endif // XBOXSTUBS_H

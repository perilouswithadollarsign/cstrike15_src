//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifndef POSIX_WIN32STUBS_H
#define POSIX_WIN32STUBS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"
#include "tier0/platform.h"

typedef int32 LRESULT;
typedef void* HWND;
typedef uint32 UINT;
typedef uintp WPARAM;
typedef uintp LPARAM;

typedef uint8 BYTE;
typedef int16 SHORT;

typedef void* WNDPROC;
typedef void* HANDLE;

//typedef char xKey_t;

#define XUSER_MAX_COUNT 2

#if defined( _OSX )
// [will] Added Xbox button constants for MacOSX
typedef enum
{
	XK_NULL,
	XK_BUTTON_UP,
	XK_BUTTON_DOWN,
	XK_BUTTON_LEFT,
	XK_BUTTON_RIGHT,
	XK_BUTTON_START,
	XK_BUTTON_BACK,
	XK_BUTTON_STICK1,
	XK_BUTTON_STICK2,
	XK_BUTTON_A,
	XK_BUTTON_B,
	XK_BUTTON_X,
	XK_BUTTON_Y,
	XK_BUTTON_LEFT_SHOULDER,
	XK_BUTTON_RIGHT_SHOULDER,
	XK_BUTTON_LTRIGGER,
	XK_BUTTON_RTRIGGER,
	XK_STICK1_UP,
	XK_STICK1_DOWN,
	XK_STICK1_LEFT,
	XK_STICK1_RIGHT,
	XK_STICK2_UP,
	XK_STICK2_DOWN,
	XK_STICK2_LEFT,
	XK_STICK2_RIGHT,
	XK_BUTTON_INACTIVE_START, // Special key that is passed through on disabled controllers
	XK_BUTTON_FIREMODE_SELECTOR_1,
	XK_BUTTON_FIREMODE_SELECTOR_2,
	XK_BUTTON_FIREMODE_SELECTOR_3,
	XK_BUTTON_RELOAD,
	XK_BUTTON_TRIGGER,
	XK_BUTTON_PUMP_ACTION,
	XK_XBUTTON_ROLL_RIGHT,
	XK_XBUTTON_ROLL_LEFT,
	XK_MAX_KEYS,
} xKey_t;
#else
#define XK_MAX_KEYS 20
#endif // _OSX

typedef struct joyinfoex_tag 
{ 
    DWORD dwSize; 
    DWORD dwFlags; 
    DWORD dwXpos; 
    DWORD dwYpos; 
    DWORD dwZpos; 
    DWORD dwRpos; 
    DWORD dwUpos; 
    DWORD dwVpos; 
    DWORD dwButtons; 
    DWORD dwButtonNumber; 
    DWORD dwPOV; 
    DWORD dwReserved1; 
    DWORD dwReserved2; 
} JOYINFOEX, *LPJOYINFOEX; 


typedef struct _XINPUT_GAMEPAD
{
    WORD                                wButtons;
    BYTE                                bLeftTrigger;
    BYTE                                bRightTrigger;
    SHORT                               sThumbLX;
    SHORT                               sThumbLY;
    SHORT                               sThumbRX;
    SHORT                               sThumbRY;
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


#endif // POSIX_WIN32STUBS_H

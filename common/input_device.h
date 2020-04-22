//========= Copyright 2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef INPUT_DEVICES_H
#define INPUT_DEVICES_H

#ifdef _WIN32
#pragma once
#endif
#if defined( _PS3 )
#include <limits.h>
#endif



// [dkorus]:  The values here are setup to be used as a bit-field.  
//			for example: A PS3 with a KEYBOARD_MOUSE attached as well as a PLAYSTATION_MOVE would have a connected 
//			device bitfield of 5 (4 for playstation move, 1 for the keyboard/mouse, for 4+1==5)
//			when adding new entries, make sure they are a power of 2.

enum InputDevice_t
{
	INPUT_DEVICE_NONE				= 0,			// (0) no specific device is selected
	INPUT_DEVICE_KEYBOARD_MOUSE		= 1 << 0,		// (1) considered connected if BOTH INPUT_DEVICE_KEYBOARD and INPUT_DEVICE_MOUSE are connected
	INPUT_DEVICE_GAMEPAD			= 1 << 1,		// (2) includes PS3, XBox360, or 360 gamepad connected to PC
	INPUT_DEVICE_PLAYSTATION_MOVE	= 1 << 2,		// (4) PS3 only
	INPUT_DEVICE_HYDRA				= 1 << 3,		// (8) support on PC
	INPUT_DEVICE_SHARPSHOOTER		= 1 << 4,		// (16) PS3 only
	INPUT_DEVICE_MOVE_NAV_CONTROLLER = 1 << 5,		// (32) PS3 only
	INPUT_DEVICE_STEAM_CONTROLLER 	= 1 << 6,		// (64) Steam controller

	INPUT_DEVICE_MAX,
	INPUT_DEVICE_INVALID = INPUT_DEVICE_MAX,
	INPUT_DEVICE_FORCE_INT32		= INT32_MAX
};

DEFINE_ENUM_BITWISE_OPERATORS( InputDevice_t )


// [mpritchard]:  When we are talking about input devices, we also have an implicit, or explicit, platform
//     that we are talking about because the list of possible input devices is specific to and varies by the
//     the platform of the player.   We can't just use the platform that the code is compiled for because
//     some code which may be running on a dedicated server which can be a platform than the clients:
//	   Specifically, that the dedicated server for Xbox360 and PS3 clients is compiled for and runs on
//      either a windows PC or LINUX.   For this reason, we need a server on one platform to be able ask
//      what are the valid input devices for this other platform...
//
enum InputDevicePlatform_t
{
	INPUT_DEVICE_PLATFORM_ANY       = -2,
	INPUT_DEVICE_PLATFORM_LOCAL     = -1,
	INPUT_DEVICE_PLATFORM_NONE		= 0,		// Allows for a undefined platform
	INPUT_DEVICE_PLATFORM_WINDOWS,
	INPUT_DEVICE_PLATFORM_OSX,
	INPUT_DEVICE_PLATFORM_XBOX360,
	INPUT_DEVICE_PLATFORM_PS3,
	INPUT_DEVICE_PLATFORM_LINUX,

	INPUT_DEVICE_PLATFORM_COUNT,		// list auto counter
	INPUT_DEVICE_PLATFORM_FORCE_INT32	= INT32_MAX
};

// motion controller state
enum InputDeviceMCState
{
	INPUT_DEVICE_MC_STATE_CAMERA_NOT_CONNECTED = -1,
	INPUT_DEVICE_MC_STATE_CONTROLLER_NOT_CONNECTED,
	INPUT_DEVICE_MC_STATE_CONTROLLER_NOT_CALIBRATED,
	INPUT_DEVICE_MC_STATE_CONTROLLER_CALIBRATING,
	INPUT_DEVICE_MC_STATE_CONTROLLER_ERROR,
	INPUT_DEVICE_MC_STATE_OK,
};


#endif //INPUT_DEVICES_H

// ===== Copyright 1996-2012, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//===========================================================================//
#ifndef PLATFORMINPUTDEVICE_H
#define PLATFORMINPUTDEVICE_H

#ifdef _WIN32
#pragma once
#endif

#include "basetypes.h"
#include "platform.h"
#include "input_device.h"

class PlatformInputDevice
{
public:
	static	bool			s_Initialized;

	static	InputDevice_t	s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_COUNT];
	static	int				s_numberPlatformInputDevices[INPUT_DEVICE_PLATFORM_COUNT];
	static	InputDevice_t	s_AllInputDevices;

	static	const InputDevicePlatform_t	s_LocalInputPlatform;

	static	void			InitPlatfromInputDeviceInfo( void );

	// Generic Information functions that can handle other platforms

	// Input Device information functions
	static	int				GetInputDeviceCountforPlatform( InputDevicePlatform_t platform = INPUT_DEVICE_PLATFORM_LOCAL );
	static	InputDevice_t	GetValidInputDevicesForPlatform( InputDevicePlatform_t platform = INPUT_DEVICE_PLATFORM_LOCAL );

	static	bool			IsInputDeviceValid( InputDevice_t device, InputDevicePlatform_t platform = INPUT_DEVICE_PLATFORM_ANY );
	static	const char	   *GetInputDeviceNameUI( InputDevice_t device );				// Use me to get translated, specific name
	static	const char	   *GetInputDeviceNameInternal( InputDevice_t device );			// Use me for internal usage w/ consistency

	// Input platform information functions
	static	InputDevicePlatform_t	GetLocalInputDevicePlatform( void );
	static	bool					IsInputDevicePlatformValid( InputDevicePlatform_t platform );
	static	const char			   *GetInputDevicePlatformName( InputDevicePlatform_t platform );
	static	InputDevice_t			GetDefaultInputDeviceForPlatform( InputDevicePlatform_t platform = INPUT_DEVICE_PLATFORM_LOCAL );

	// methods to convert to/from an InputDevice_t and a 1-based ordinal 
	static	int				GetInputDeviceOrdinalForPlatform( InputDevice_t device, InputDevicePlatform_t platform = INPUT_DEVICE_PLATFORM_LOCAL );
	static	InputDevice_t	GetInputDeviceTypefromPlatformOrdinal( int deviceNo, InputDevicePlatform_t platform = INPUT_DEVICE_PLATFORM_LOCAL );

	// input device properties
	static	bool			IsInputDeviceAPointer( InputDevice_t device );				// Returns true if the device is treated as a pointer for input.
};

#endif		// PLATFORMINPUTDEVICE_H
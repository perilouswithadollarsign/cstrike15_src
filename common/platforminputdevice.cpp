// ===== Copyright 1996-2012, Valve Corporation, All rights reserved. =======
//
// Purpose: Provides Cross-Platform Input Device information
//
// ==========================================================================



#include "platforminputdevice.h"
#include "dbg.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class CAutoInit
{
public:
	CAutoInit( void )
	{
		PlatformInputDevice::InitPlatfromInputDeviceInfo();
	}
};

CAutoInit  forceInputInit;



int countBits( uint32 iValue )
{
	int count = 0;
	while ( iValue != 0 )
	{
		if ( iValue & 0x01 )
		{
			count++;
		}
		iValue >>= 1;
	}
	return count;
}


uint32 getValueofNthSetBit( uint32 iValue, int bitNo )
{
	if ( bitNo < 1 || bitNo > 32 )
		return 0;

	uint32 curValue = 1;
	int bitCount = 0;
	while ( curValue > 0 )
	{
		if ( ( iValue & curValue ) != 0 )
		{
			bitCount++;
			if ( bitCount == bitNo)
			{	
				return curValue;
			}
		}
		curValue <<= 1;
	}

	return 0;
}


int getOrdinalOfSetBit( uint32 iValue, uint32 theBit )
{
	if ( ( iValue & theBit ) == 0 )
		return 0;

	Assert( countBits( theBit ) == 1 );

	uint32 curValue = 1;
	int bitCount = 0;
	while ( curValue > 0 )
	{
		if ( ( iValue & curValue ) != 0 )
		{
			bitCount++;
			if ( curValue == theBit )
			{	
				return bitCount;
			}
		}
		curValue <<= 1;
	}

	return 0;
}



bool PlatformInputDevice::s_Initialized = false;

InputDevice_t	PlatformInputDevice::s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_COUNT];
int				PlatformInputDevice::s_numberPlatformInputDevices[INPUT_DEVICE_PLATFORM_COUNT];

InputDevice_t	PlatformInputDevice::s_AllInputDevices = INPUT_DEVICE_NONE;

const InputDevicePlatform_t PlatformInputDevice::s_LocalInputPlatform = 
	#if defined( PLATFORM_WINDOWS_PC )
		INPUT_DEVICE_PLATFORM_WINDOWS;
	#elif defined( PLATFORM_OSX )
		INPUT_DEVICE_PLATFORM_OSX;
	#elif defined( PLATFORM_X360 )
		INPUT_DEVICE_PLATFORM_XBOX360;
	#elif defined( PLATFORM_PS3 )
		INPUT_DEVICE_PLATFORM_PS3;
	#elif defined( PLATFORM_LINUX )
		INPUT_DEVICE_PLATFORM_LINUX;
	#else
		INPUT_DEVICE_PLATFORM_NONE;
	#endif


void PlatformInputDevice::InitPlatfromInputDeviceInfo( void )
{
	// clear all
	s_AllInputDevices = INPUT_DEVICE_NONE;

	for ( int n = 0; n < INPUT_DEVICE_PLATFORM_COUNT; n++ )
	{
		s_validPlatformInputDevices[n] = INPUT_DEVICE_NONE;
	}

	// Windows PC
	s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_WINDOWS] =
		INPUT_DEVICE_KEYBOARD_MOUSE | 
		INPUT_DEVICE_GAMEPAD | 
		INPUT_DEVICE_HYDRA |
		INPUT_DEVICE_STEAM_CONTROLLER;

	// Mac OSX
	s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_OSX] =
		INPUT_DEVICE_KEYBOARD_MOUSE |
		INPUT_DEVICE_STEAM_CONTROLLER;

	// Xbox 360
	s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_XBOX360] =
		INPUT_DEVICE_GAMEPAD;

	// Playstation 3
	s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_PS3] =
		INPUT_DEVICE_KEYBOARD_MOUSE | 
		INPUT_DEVICE_GAMEPAD | 
		INPUT_DEVICE_PLAYSTATION_MOVE |
		INPUT_DEVICE_SHARPSHOOTER;

	// Linux PC
	s_validPlatformInputDevices[INPUT_DEVICE_PLATFORM_LINUX] =
		INPUT_DEVICE_KEYBOARD_MOUSE |
		INPUT_DEVICE_STEAM_CONTROLLER;

	for (int n = 0; n < INPUT_DEVICE_PLATFORM_COUNT; n++ )
	{
		s_numberPlatformInputDevices[n] = countBits( s_validPlatformInputDevices[n] );
		s_AllInputDevices = s_AllInputDevices | s_validPlatformInputDevices[n];
	}

#if defined ( _PS3 )

	// On PS3 we need to make sure steamworks gets updated with all the required ELO data slots.
	// If we change how many devices we have, we need to update steamworks with the proper number
	// of ELO variables.
	AssertMsg( s_numberPlatformInputDevices[INPUT_DEVICE_PLATFORM_PS3] == 4, "You must update Steamworks with the correct number of input devices for the ELO values." );

#endif

	s_Initialized = true;
}


int	PlatformInputDevice::GetInputDeviceCountforPlatform( InputDevicePlatform_t platform )
{
	Assert( s_Initialized );
	if ( platform < INPUT_DEVICE_PLATFORM_LOCAL || platform >= INPUT_DEVICE_PLATFORM_COUNT )
	{
		AssertMsg( false, "invalid platform" );
		return 0;
	}

	return s_numberPlatformInputDevices[ ( ( platform == INPUT_DEVICE_PLATFORM_LOCAL ) ? s_LocalInputPlatform : platform ) ];
}


InputDevice_t PlatformInputDevice::GetValidInputDevicesForPlatform( InputDevicePlatform_t platform )
{
	Assert( s_Initialized );
	if ( platform < INPUT_DEVICE_PLATFORM_LOCAL || platform >= INPUT_DEVICE_PLATFORM_COUNT )
	{
		AssertMsg( false, "invalid platform" );
		return INPUT_DEVICE_NONE;
	}

	return  s_validPlatformInputDevices[ ( ( platform == INPUT_DEVICE_PLATFORM_LOCAL ) ? s_LocalInputPlatform : platform ) ];
}


bool PlatformInputDevice::IsInputDeviceValid( InputDevice_t device, InputDevicePlatform_t platform )
{
	Assert( s_Initialized );
	// make sure inputs make sense
	if ( platform < INPUT_DEVICE_PLATFORM_ANY || platform >= INPUT_DEVICE_PLATFORM_COUNT )
	{
		AssertMsg( false, "invalid platform" );
		return false;
	}

	if ( countBits( device ) != 1 || ( device & s_AllInputDevices ) != device )
	{
		return false;
	}

	// any platform?  already checked it above
	if ( platform == INPUT_DEVICE_PLATFORM_ANY )
	{
		return true;
	}

	// translate to current hardware platform if necessary
	if ( platform == INPUT_DEVICE_PLATFORM_LOCAL )
	{
		platform = s_LocalInputPlatform;
	}

	return ( ( s_validPlatformInputDevices[platform] & device ) == device );
}


const char *PlatformInputDevice::GetInputDeviceNameUI( InputDevice_t device )
{
	switch ( device )
	{
		case INPUT_DEVICE_KEYBOARD_MOUSE:			return "#INPUT_DEVICE_KBMOUSE";
#if defined( _PS3 )
		case INPUT_DEVICE_GAMEPAD:					return "#INPUT_DEVICE_GAMEPAD_PS3";
#else
		case INPUT_DEVICE_GAMEPAD:					return "#INPUT_DEVICE_GAMEPAD_XBOX";
#endif
		case INPUT_DEVICE_PLAYSTATION_MOVE:			return "#INPUT_DEVICE_PSMOVE";
		case INPUT_DEVICE_HYDRA:					return "#INPUT_DEVICE_HYDRA";
		case INPUT_DEVICE_SHARPSHOOTER:				return "#INPUT_DEVICE_SHARPSHOOTER";
		case INPUT_DEVICE_MOVE_NAV_CONTROLLER:		return "#INPUT_DEVICE_MOVE_NAV_CONTROLLER";
		default:
		{
			AssertMsg( false, "Invalid Input Device" );
			return "<invalid>";
		}
	}
}


const char *PlatformInputDevice::GetInputDeviceNameInternal( InputDevice_t device )
{
	switch ( device )
	{
		case INPUT_DEVICE_KEYBOARD_MOUSE:			return "KBMOUSE";
		case INPUT_DEVICE_GAMEPAD:					return "GAMEPAD";
		case INPUT_DEVICE_PLAYSTATION_MOVE:			return "PSMOVE";
		case INPUT_DEVICE_HYDRA:					return "HYDRA";
		case INPUT_DEVICE_SHARPSHOOTER:				return "SHARPSHOOTER";
		case INPUT_DEVICE_MOVE_NAV_CONTROLLER:		return "NAV_CONTROLLER";
		default:
		{
			AssertMsg( false, "Invalid Input Device" );
			return "<INVALID>";
		}
	}
}


InputDevicePlatform_t PlatformInputDevice::GetLocalInputDevicePlatform( void )
{
	return s_LocalInputPlatform;
}


bool PlatformInputDevice::IsInputDevicePlatformValid( InputDevicePlatform_t platform )
{
	return ( platform > INPUT_DEVICE_PLATFORM_NONE && platform < INPUT_DEVICE_PLATFORM_COUNT );
}


const char *PlatformInputDevice::GetInputDevicePlatformName( InputDevicePlatform_t platform )
{
	switch ( platform )
	{
		case INPUT_DEVICE_PLATFORM_NONE:		return "NONE (Not Set)";
		case INPUT_DEVICE_PLATFORM_WINDOWS:		return "Windows PC";
		case INPUT_DEVICE_PLATFORM_OSX:			return "Mac OS X";
		case INPUT_DEVICE_PLATFORM_XBOX360:		return "Xbox 360";
		case INPUT_DEVICE_PLATFORM_PS3:			return "Playstation 3";
		case INPUT_DEVICE_PLATFORM_LINUX:		return "Linux PC";
		default:
		{
			AssertMsg( false, "Invalid Input Platform" );
			return "<invalid>";
		}
	}
}

InputDevice_t PlatformInputDevice::GetDefaultInputDeviceForPlatform( InputDevicePlatform_t platform )
{
	// make sure inputs make sense
	if ( platform < INPUT_DEVICE_PLATFORM_LOCAL || platform >= INPUT_DEVICE_PLATFORM_COUNT )
	{
		AssertMsg( false, "invalid platform" );
		return INPUT_DEVICE_NONE;
	}

	// translate to current hardware platform if necessary
	if ( platform == INPUT_DEVICE_PLATFORM_LOCAL )
	{
		platform = s_LocalInputPlatform;
	}

	switch ( platform )
	{
		case INPUT_DEVICE_PLATFORM_NONE:		return INPUT_DEVICE_NONE;
		case INPUT_DEVICE_PLATFORM_WINDOWS:		return INPUT_DEVICE_KEYBOARD_MOUSE;
		case INPUT_DEVICE_PLATFORM_OSX:			return INPUT_DEVICE_KEYBOARD_MOUSE;
		case INPUT_DEVICE_PLATFORM_XBOX360:		return INPUT_DEVICE_GAMEPAD;
		case INPUT_DEVICE_PLATFORM_PS3:			return INPUT_DEVICE_GAMEPAD;
		case INPUT_DEVICE_PLATFORM_LINUX:		return INPUT_DEVICE_KEYBOARD_MOUSE;
		default:
		{
			AssertMsg( false, "Default device missing" );
			return INPUT_DEVICE_NONE;
		}
	}
}


int	PlatformInputDevice::GetInputDeviceOrdinalForPlatform( InputDevice_t device, InputDevicePlatform_t platform )
{
	Assert( s_Initialized );
	// make sure inputs make sense
	if ( platform < INPUT_DEVICE_PLATFORM_LOCAL || platform >= INPUT_DEVICE_PLATFORM_COUNT )
	{
		AssertMsg( false, "invalid platform" );
		return 0;
	}
	if ( countBits( device ) != 1 || ( device & s_AllInputDevices ) != device )
	{
		AssertMsg( false, "invalid device" );
		return 0;
	}

	// translate to current hardware platform if necessary
	if ( platform == INPUT_DEVICE_PLATFORM_LOCAL )
	{
		platform = s_LocalInputPlatform;
	}

	if ( ( s_validPlatformInputDevices[platform] & device ) == 0 )
	{
		return 0;
	}

	return getOrdinalOfSetBit( (uint32) s_validPlatformInputDevices[platform], (uint32) device );
}


InputDevice_t PlatformInputDevice::GetInputDeviceTypefromPlatformOrdinal( int deviceNo, InputDevicePlatform_t platform )
{
	Assert( s_Initialized );
	// make sure inputs make sense
	if ( platform < INPUT_DEVICE_PLATFORM_LOCAL || platform >= INPUT_DEVICE_PLATFORM_COUNT )
	{
		AssertMsg( false, "invalid platform" );
		return INPUT_DEVICE_NONE;
	}

	// translate to current hardware platform if necessary
	if ( platform == INPUT_DEVICE_PLATFORM_LOCAL )
	{
		platform = s_LocalInputPlatform;
	}

	if ( deviceNo < 1 || deviceNo > s_numberPlatformInputDevices[platform] )
	{
		AssertMsg( false, "bad platform device ordnial" );
		return INPUT_DEVICE_NONE;
	}

	return (InputDevice_t) getValueofNthSetBit( (uint32) s_validPlatformInputDevices[platform], deviceNo );
}

bool PlatformInputDevice::IsInputDeviceAPointer( InputDevice_t device )
{
	switch ( device )
	{
	case INPUT_DEVICE_NONE:
	case INPUT_DEVICE_KEYBOARD_MOUSE:
	case INPUT_DEVICE_GAMEPAD:
	case INPUT_DEVICE_MOVE_NAV_CONTROLLER:
		return false;
	case INPUT_DEVICE_PLAYSTATION_MOVE:
	case INPUT_DEVICE_HYDRA:
	case INPUT_DEVICE_SHARPSHOOTER:
		return true;
	default:
		AssertMsg(false, "Device not handled.");
		break;
	}
	return false;
}

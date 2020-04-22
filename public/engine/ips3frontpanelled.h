//===== Copyright   1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Access to the cuter features of the PS3
//          devkit, like the front LEDs.
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef PS3_FRONTPANELLED_H
#define PS3_FRONTPANELLED_H

#ifdef _WIN32
#pragma once
#endif


/// encapsulates the DECR-1000's front panel LEDs, dip switches, and foot pedal
/// the system only lets you set a subset of the LEDs; there are FOUR LIGHTS.
/// (really. you can set LEDs 0..3)
namespace CPS3FrontPanelLED
{
	// sets the on/off state of the LEDs on the front panel. 
	// you simply specify them as a bitmask -- ie, LED0 | LED 2 would light the zeroth and twoth LED and darken the other two.
	inline bool SetLEDs( uint64 lights );

	// you can use this to eg set LED1 while leaving the rest undisturbed. a 1 bit in the mask means the corresponding
	// bit in value will be written to the hardware.
	inline bool SetLEDsMasked( uint64 mask, uint64 lights );

	// gets the on/off state of the LEDs on the front panel
	inline uint64 GetLEDs();

	// gets the on/off state of the switches on the front panel
	inline uint64 GetSwitches();

	// you will notice there is no function to set the state of the DIP switches from software.

	// you don't actually need to use these, but just to be obvious:
	enum eLEDIndex_t
	{
		kPS3LED0 = 1,
		kPS3LED1 = 2,
		kPS3LED2 = 4,
		kPS3LED3 = 8
	};

	enum eSwitchIndex_t
	{
		kPS3SWITCH0 = 1,
		kPS3SWITCH1 = 2,
		kPS3SWITCH2 = 4,
		kPS3SWITCH3 = 8
	};
};

#if !defined(_PS3)
inline bool CPS3FrontPanelLED::SetLEDs( uint64 lights ) {return false;}
inline bool CPS3FrontPanelLED::SetLEDsMasked( uint64 mask, uint64 lights ) {return false;}
inline uint64 CPS3FrontPanelLED::GetLEDs() {return 0;}
inline uint64 CPS3FrontPanelLED::GetSwitches() {return 0;}
#else

#include <sys/gpio.h>

inline bool CPS3FrontPanelLED::SetLEDs( uint64 lights )
{
	return sys_gpio_set( SYS_GPIO_LED_DEVICE_ID, SYS_GPIO_LED_USER_AVAILABLE_BITS, lights ) == CELL_OK;
}

inline bool CPS3FrontPanelLED::SetLEDsMasked( uint64 mask, uint64 lights ) 
{
	return sys_gpio_set( SYS_GPIO_LED_DEVICE_ID, SYS_GPIO_LED_USER_AVAILABLE_BITS & mask, SYS_GPIO_LED_USER_AVAILABLE_BITS & lights ) == CELL_OK;
}

inline uint64 CPS3FrontPanelLED::GetLEDs() 
{
	uint64 val;
	if ( sys_gpio_get( SYS_GPIO_LED_DEVICE_ID, &val ) == CELL_OK )
	{
		return val & SYS_GPIO_LED_USER_AVAILABLE_BITS;
	}
	else
	{
		return 0;
	}
}

inline uint64 CPS3FrontPanelLED::GetSwitches() 
{
	uint64 val;
	if ( sys_gpio_get( SYS_GPIO_DIP_SWITCH_DEVICE_ID, &val ) == CELL_OK )
	{
		return val & SYS_GPIO_DIP_SWITCH_USER_AVAILABLE_BITS;
	}
	else
	{
		return 0;
	}
}


#endif

#endif // PS3_FRONTPANELLED_H
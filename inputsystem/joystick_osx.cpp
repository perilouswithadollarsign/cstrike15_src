//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: OSX Joystick implementation for inputsystem.dll
//
//===========================================================================//

#include <mach/mach.h>
#include <mach/mach_error.h>

/* For force feedback testing. */
#include "inputsystem.h"
#include "tier1/convar.h"


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar joy_axisbutton_threshold( "joy_axisbutton_threshold", "0.3", FCVAR_ARCHIVE, "Analog axis range before a button press is registered." );
ConVar joy_axis_deadzone( "joy_axis_deadzone", "0.2", FCVAR_ARCHIVE, "Dead zone near the zero point to not report movement." );

//-----------------------------------------------------------------------------
// Initialize all joysticks 
//-----------------------------------------------------------------------------
void CInputSystem::InitializeJoysticks( void ) 
{  
 	// assume no joystick
	m_nJoystickCount = 0; 

	// abort startup if user requests no joystick
	if ( CommandLine()->FindParm("-nojoy" ) ) 
		return; 
 
	IOReturn result = kIOReturnSuccess;
    mach_port_t masterPort = 0;
    io_iterator_t hidObjectIterator = 0;
    CFMutableDictionaryRef hidMatchDictionary = NULL;
		
    result = IOMasterPort( MACH_PORT_NULL, &masterPort);
    if ( kIOReturnSuccess != result ) 
	{
		DevMsg( 1, "joystick not found -- IOMasterPort error with bootstrap_port.\n");
        return;
    }
	
    hidMatchDictionary = IOServiceMatching(kIOHIDDeviceKey);
    if ( !hidMatchDictionary ) 
	{
        DevMsg( 1, "joystick not found -- Failed to get HID CFMutableDictionaryRef via IOServiceMatching.");
        return;
    }
	
    result = IOServiceGetMatchingServices( masterPort, hidMatchDictionary, &hidObjectIterator );
    if ( kIOReturnSuccess != result ) 
	{
        DevMsg( 1, "joystick not found -- Couldn't create a HID object iterator.");
        return ;
    }
	
    if ( !hidObjectIterator ) 	
	{  
		m_nJoystickCount = 0; 
        return;
    }
	
    io_object_t ioHIDDeviceObject = 0;	
    while ( ( ioHIDDeviceObject = IOIteratorNext(hidObjectIterator ) ) ) 
	{
		JoystickInfo_t &info = m_pJoystickInfo[m_nJoystickCount];
		info.m_pParent = this;
		info.m_bRemoved = false;
		info.m_Interface = NULL;
		info.m_FFInterface = 0;
		info.m_nButtonCount = 0;
		info.m_nFlags = 0;
		info.m_nAxisFlags = 0;
		info.m_bXBoxRumbleEnabled = false;

        // grab the device record
        if ( !HIDBuildDevice( ioHIDDeviceObject, info ) )
            continue;
		
        if ( FFIsForceFeedback(ioHIDDeviceObject) == FF_OK )
		{
            FFCreateDevice(ioHIDDeviceObject,&info.m_FFInterface);;
        } 
		else 
		{
            info.m_FFInterface = 0;
        }
		
		info.m_nDeviceId = m_nJoystickCount;
		info.m_nLastPolledButtons = 0;
		info.m_nLastPolledAxisButtons = 0;
		info.m_nLastPolledPOVState = 0;
		memset( info.m_pLastPolledAxes, 0, sizeof(info.m_pLastPolledAxes) );

		EnableJoystickInput( m_nJoystickCount, true );
		++m_nJoystickCount;		
    }

    result = IOObjectRelease(hidObjectIterator);
}

void HIDReportErrorNum(char *strError, long numError)
{
    DevMsg( 1, "%s", strError );
}

int ButtonSort(const void *a, const void *b)
{
	CInputSystem::OSXInputValue_t *left = (CInputSystem::OSXInputValue_t *)a;
	CInputSystem::OSXInputValue_t *right = (CInputSystem::OSXInputValue_t *)b;
	return left->m_Usage - right->m_Usage;
}


void CInputSystem::HIDSortJoystickButtons( JoystickInfo_t &info )
{
	qsort( info.m_Buttons, info.m_nButtonCount, sizeof(info.m_Buttons[0]), ButtonSort );
}


bool CInputSystem::HIDBuildDevice( io_object_t ioHIDDeviceObject, JoystickInfo_t &info )
{
	CFMutableDictionaryRef hidProperties = 0;
	kern_return_t result = IORegistryEntryCreateCFProperties( ioHIDDeviceObject, &hidProperties, kCFAllocatorDefault, kNilOptions);
	if ((result == KERN_SUCCESS) && hidProperties) 
	{
		if ( HIDCreateOpenDeviceInterface( ioHIDDeviceObject, info ) ) 
		{
			HIDGetDeviceInfo( ioHIDDeviceObject, hidProperties, info ); 
			HIDGetCollectionElements( hidProperties, info );
			HIDSortJoystickButtons( info );
		} 
		else 
			return false;

		CFRelease(hidProperties);
	} 
	else
		return false;
	
	// Filter device list to non-keyboard/mouse stuff 
	if ( info.usagePage != kHIDPage_GenericDesktop ||
		( ( info.usage != kHIDUsage_GD_Joystick &&
		  info.usage != kHIDUsage_GD_GamePad &&
		  info.usage != kHIDUsage_GD_MultiAxisController ) ) ) 
	{
		HIDDisposeDevice( info );
		return false;
	}
	return true;
}

static void HIDRemovalCallback(void *target, IOReturn result, void *refcon, void *sender)
{
    CInputSystem::JoystickInfo_t *joy = (CInputSystem::JoystickInfo_t *) refcon;
    joy->m_bRemoved = true;
}



/* Create and open an interface to device, required prior to extracting values or building queues.
 * Note: appliction now owns the device and must close and release it prior to exiting
 */

bool CInputSystem::HIDCreateOpenDeviceInterface( io_object_t hidDevice, JoystickInfo_t &info )
{
    IOReturn result = kIOReturnSuccess;
    HRESULT plugInResult = S_OK;
    SInt32 score = 0;
    IOCFPlugInInterface **ppPlugInInterface = NULL;
    if ( NULL == info.m_Interface ) 
	{
        result = IOCreatePlugInInterfaceForService(hidDevice, kIOHIDDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &ppPlugInInterface, &score);
        if (kIOReturnSuccess == result) 
		{
            plugInResult = (*ppPlugInInterface)->QueryInterface(ppPlugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),(void **)&(info.m_Interface) );
            if ( plugInResult != S_OK )
                HIDReportErrorNum("Couldn’t query HID class device interface from plugInInterface", plugInResult);
			
			(*ppPlugInInterface)->Release(ppPlugInInterface);
        } 
		else
            HIDReportErrorNum("Failed to create **plugInInterface via IOCreatePlugInInterfaceForService.", result);
    }
	
    if ( info.m_Interface != NULL ) 
	{
        result = (*info.m_Interface)->open( info.m_Interface, 0);
        if ( result != kIOReturnSuccess )
		{
            HIDReportErrorNum("Failed to open pDevice->interface via open.", result);
		}
        else
		{
            (*info.m_Interface)->setRemovalCallback( info.m_Interface, HIDRemovalCallback, &info, this );
		}
    }
    return result == kIOReturnSuccess;
}


static void HIDTopLevelElementHandler(const void *value, void *parameter)
{
    CFTypeRef refCF = 0;
    if (CFGetTypeID(value) != CFDictionaryGetTypeID())
        return;
	
    refCF = CFDictionaryGetValue((CFDictionaryRef)value, CFSTR(kIOHIDElementUsagePageKey));
    if ( !CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &((CInputSystem::JoystickInfo_t *) parameter)->usagePage) )
        DevMsg( 1, "HIDTopLevelElementHandler CFNumberGetValue error retrieving pDevice->usagePage." );
	
    refCF = CFDictionaryGetValue((CFDictionaryRef)value, CFSTR(kIOHIDElementUsageKey));
    if ( !CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &((CInputSystem::JoystickInfo_t *) parameter)->usage) )
        DevMsg( 1, "HIDTopLevelElementHandler CFNumberGetValue error retrieving pDevice->usage." );
}


void CInputSystem::HIDGetDeviceInfo( io_object_t hidDevice, CFMutableDictionaryRef hidProperties, JoystickInfo_t &info )
{
    CFMutableDictionaryRef usbProperties = 0;
    io_registry_entry_t parent1, parent2;
	
    if ((KERN_SUCCESS == IORegistryEntryGetParentEntry(hidDevice, kIOServicePlane, &parent1))
        && (KERN_SUCCESS == IORegistryEntryGetParentEntry(parent1, kIOServicePlane, &parent2))
        && (KERN_SUCCESS == IORegistryEntryCreateCFProperties(parent2, &usbProperties, kCFAllocatorDefault, kNilOptions))) 
	{
		if ( usbProperties ) 
		{
			CFTypeRef refCF = 0;

			long vendorid = 0;
			long productid = 0;
			refCF = CFDictionaryGetValue((CFDictionaryRef)hidProperties, CFSTR(kIOHIDVendorIDKey) );
			if (refCF)
				CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &vendorid );
			refCF = CFDictionaryGetValue((CFDictionaryRef)hidProperties, CFSTR(kIOHIDProductIDKey) );
			if (refCF)
				CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &productid );
				
			if ( vendorid == 0x045e )
			{
				m_bXController = true; // microsoft gamepad, lets call it a 360 controller
			}
			
			/* get usage page and usage */
			refCF = CFDictionaryGetValue((CFDictionaryRef)hidProperties, CFSTR(kIOHIDPrimaryUsagePageKey) );
			if (refCF) 
			{
				if ( !CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &info.usagePage ) )
					DevMsg( 1, "CInputSystem::HIDGetDeviceInfo error retrieving pDevice->usagePage." );
				
				refCF =	CFDictionaryGetValue((CFDictionaryRef)hidProperties, CFSTR(kIOHIDPrimaryUsageKey));
				if (refCF)
					if (!CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &info.usage))
						DevMsg( 1, "CInputSystem::HIDGetDeviceInfo error retrieving pDevice->usage." );
			}
			
			if (NULL == refCF) 					
			{        
				CFTypeRef refCFTopElement = 0;
				refCFTopElement = CFDictionaryGetValue( (CFDictionaryRef)hidProperties, CFSTR(kIOHIDElementKey) );
				{
					CFRange range = { 0, CFArrayGetCount((CFArrayRef)refCFTopElement) };
					CFArrayApplyFunction( (CFArrayRef)refCFTopElement, range, HIDTopLevelElementHandler, &info );
				}
			}
		
	
			CFRelease( usbProperties );
		} 
		else
			DevMsg( 1, "CInputSystem::HIDGetDeviceInfo IORegistryEntryCreateCFProperties failed to create usbProperties." );
		
		if (kIOReturnSuccess != IOObjectRelease(parent2) )
			DevMsg( 1, "CInputSystem::HIDGetDeviceInfo IOObjectRelease error with parent2." );
		if (kIOReturnSuccess != IOObjectRelease(parent1))
			DevMsg( 1, "CInputSystem::HIDGetDeviceInfo IOObjectRelease error with parent1." );
	}
}


void CInputSystem::HIDGetElementInfo( CFTypeRef refElement, OSXInputValue_t &input )
{
    long number;
    CFTypeRef refType;
	
    refType = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementCookieKey) );
    if ( refType && CFNumberGetValue( (CFNumberRef)refType, kCFNumberLongType, &number ) )
        input.m_Cookie = (int) number;
	
    refType = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementMinKey) );
    if ( refType && CFNumberGetValue( (CFNumberRef)refType, kCFNumberLongType, &number ) )
	{
	        input.m_MinVal = input.m_MinReport = number;
		input.m_MaxVal = input.m_MaxReport = input.m_MinVal;
	}
	
    refType = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementMaxKey) );
    if ( refType && CFNumberGetValue( (CFNumberRef)refType, kCFNumberLongType, &number ) )
	{
	        input.m_MaxVal = input.m_MaxReport = number;
	}
	input.m_bSet = true;
	input.m_RefElement = refElement;

	refType = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementUsageKey) );
    if ( refType && CFNumberGetValue( (CFNumberRef)refType, kCFNumberLongType, &number ) )
	{
		input.m_Usage = number;
	}	
}


void CInputSystem::HIDAddElement(CFTypeRef refElement, JoystickInfo_t &info )
{
    long elementType, usagePage, usage;
    CFTypeRef refElementType = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementTypeKey) );
    CFTypeRef refUsagePage = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementUsagePageKey) );
    CFTypeRef refUsage = CFDictionaryGetValue( (CFDictionaryRef)refElement, CFSTR(kIOHIDElementUsageKey) );
	
    if ( refElementType && CFNumberGetValue( (CFNumberRef)refElementType, kCFNumberLongType, &elementType) ) 
	{
        if ( (elementType == kIOHIDElementTypeInput_Misc) || (elementType == kIOHIDElementTypeInput_Button) || (elementType == kIOHIDElementTypeInput_Axis) )
		{
            if ( refUsagePage && CFNumberGetValue( (CFNumberRef)refUsagePage, kCFNumberLongType, &usagePage )
				&& refUsage && CFNumberGetValue( (CFNumberRef)refUsage, kCFNumberLongType, &usage ) ) 
			{
                switch (usagePage) 
				{    
					case kHIDPage_GenericDesktop:
                    {
                        switch ( usage )
						{
							case kHIDUsage_GD_X:
								HIDGetElementInfo( refElement, info.m_xaxis );
								info.m_nAxisFlags |= 0x3;
								break;
							case kHIDUsage_GD_Y:
								HIDGetElementInfo( refElement, info.m_yaxis );
								info.m_nAxisFlags |= 0x3;
								break;
							case kHIDUsage_GD_Z:
								HIDGetElementInfo( refElement, info.m_zaxis );
								info.m_nAxisFlags |= 0x4;
								break;
							case kHIDUsage_GD_Rx:
								HIDGetElementInfo( refElement, info.m_uaxis );
								info.m_nAxisFlags |= 0x8;
								break;
							case kHIDUsage_GD_Ry:
								HIDGetElementInfo( refElement, info.m_raxis );
								info.m_nAxisFlags |= 0x10;
								break;
							case kHIDUsage_GD_Rz:
								HIDGetElementInfo( refElement, info.m_vaxis );
								info.m_nAxisFlags |= 0x20;
								break;
							case kHIDUsage_GD_Slider:
							case kHIDUsage_GD_Dial:
							case kHIDUsage_GD_Wheel:
								// unused
								break;
							case kHIDUsage_GD_Hatswitch:
								HIDGetElementInfo( refElement, info.m_POV );
								info.m_bHasPOVControl = true;
								break;
							default:
								break;
                        }
                    }
					break;
						
					case kHIDPage_Button:
						if ( info.m_nButtonCount < MAX_JOYSTICK_BUTTONS )
						{
							HIDGetElementInfo( refElement, info.m_Buttons[ info.m_nButtonCount ] );
							info.m_nButtonCount++;
						}
						break;
					default:
						break;
                }
            }
        } 
		else if (kIOHIDElementTypeCollection == elementType)
		{
            HIDGetCollectionElements((CFMutableDictionaryRef) refElement, info );
		}
    }
	
}



static void HIDGetElementsCFArrayHandler( const void *value, void *parameter )
{
    if ( CFGetTypeID(value) == CFDictionaryGetTypeID() )
	{
		CInputSystem::JoystickInfo_t *info = (CInputSystem::JoystickInfo_t *)parameter;
		if ( info )
			info->m_pParent->HIDAddElement( (CFTypeRef)value, *info );
	}
}


void CInputSystem::HIDGetElements( CFTypeRef refElementCurrent, JoystickInfo_t &info )
{
    CFTypeID type = CFGetTypeID(refElementCurrent);
    if (type == CFArrayGetTypeID()) 
	{
        CFRange range = { 0, CFArrayGetCount((CFArrayRef)refElementCurrent) };
        CFArrayApplyFunction((CFArrayRef)refElementCurrent, range, HIDGetElementsCFArrayHandler, &info );
    }
}


void CInputSystem::HIDGetCollectionElements( CFMutableDictionaryRef deviceProperties, JoystickInfo_t &info )
{
    CFTypeRef refElementTop = CFDictionaryGetValue((CFDictionaryRef)deviceProperties, CFSTR(kIOHIDElementKey));
    if (refElementTop)
        HIDGetElements( refElementTop, info );
}




void CInputSystem::HIDDisposeDevice( JoystickInfo_t &info )
{
    kern_return_t result = KERN_SUCCESS;
	if ( info.m_FFInterface )
	{
		if ( m_bXController && info.m_bXBoxRumbleEnabled )
		{
			info.m_bXBoxRumbleEnabled = false;
			FFEFFESCAPE escape;
			char c;
			c=0x00;
			escape.dwSize=sizeof(escape);
			escape.dwCommand=0x00;
			escape.cbInBuffer=sizeof(c);
			escape.lpvInBuffer=&c;
			escape.cbOutBuffer=0;
			escape.lpvOutBuffer=NULL;
			FFDeviceEscape( info.m_FFInterface ,&escape );
		}
		FFReleaseDevice( info.m_FFInterface );
	}
	
	result = (*info.m_Interface)->close( info.m_Interface );
	if ( result == kIOReturnNotOpen ) 
	{
		// wasn't open, what?
	} 
	else if ( result != kIOReturnSuccess)
	{
		HIDReportErrorNum( "Failed to close IOHIDDeviceInterface.", result );
	}
	
	result = (*info.m_Interface)->Release( info.m_Interface );
	if ( kIOReturnSuccess != result )
	{
		HIDReportErrorNum( "Failed to release IOHIDDeviceInterface.", result );
	}
}



	
	
	

//-----------------------------------------------------------------------------
//	Process the event
//-----------------------------------------------------------------------------
void CInputSystem::JoystickButtonEvent( ButtonCode_t button, int sample )
{
	// package the key
	if ( sample )
	{
		PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, button, button );
	}
	else
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, button, button );
	}
}


//-----------------------------------------------------------------------------
// Update the joystick button state
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickButtonState( int nJoystick )
{
	JoystickInfo_t &info = m_pJoystickInfo[nJoystick];

	int nButtons = 0;
	for ( int i = 0; i < info.m_nButtonCount; i++ )
	{
		int value = HIDGetElementValue( info, info.m_Buttons[i] ) - info.m_Buttons[i].m_MinVal;
		int mask = info.m_nLastPolledButtons & ( 1 << i );
		if ( !mask && !value ) // not pressed last time or this
			continue;
		
		ButtonCode_t code = (ButtonCode_t)JOYSTICK_BUTTON( nJoystick, i );
		if ( m_bXController )
		{
			// translate the xcontroller xpad buttons into the POV button presses we expect
			if ( i == 11 )
				code = KEY_XBUTTON_UP;
			if ( i == 12 )
				code = KEY_XBUTTON_DOWN;
			if ( i == 13 )
				code = KEY_XBUTTON_LEFT;
			if ( i == 14 )
				code = KEY_XBUTTON_RIGHT;
		}
				
		if ( value )
		{
			// down event
			JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
		}
		else
		{
			// up event
			JoystickButtonEvent( code, 0 );
		}
	
		info.m_nLastPolledButtons &= ~( 1 << i );
		if ( value )
			info.m_nLastPolledButtons |= ( 1 << i );
	}
	
	// Analog axis buttons
	for ( int j = 0 ; j < MAX_JOYSTICK_AXES; ++j )
	{
		if ( ( info.m_nAxisFlags & (1 << j) ) == 0 )
			continue;
		
		// Positive side of the axis
		int mask = ( 1 << (j << 1) );
		ButtonCode_t code = JOYSTICK_AXIS_BUTTON( nJoystick, (j << 1) );
		float value = GetAnalogValue( JOYSTICK_AXIS( nJoystick, j ) );
		float minValue = joy_axisbutton_threshold.GetFloat();
		switch (j)
		{
			default:
			case 0:
				minValue *= info.m_xaxis.m_MaxVal;
				break;
			case 1:
				minValue *= info.m_yaxis.m_MaxVal;
				break;
			case 2:
				minValue *= info.m_zaxis.m_MaxVal;
				break;
			case 3:
				minValue *= info.m_raxis.m_MaxVal;
				break;
			case 4:
				minValue *= info.m_uaxis.m_MaxVal;
				break;				
			case 5:
				minValue *= info.m_vaxis.m_MaxVal;
				break;				
		}
		if ( j == 5 )
			code = KEY_XBUTTON_RTRIGGER; // left and right triggers go 0 to 255 under osx, so drag R axis over to z negative
		
		if ( value > minValue && !(info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons |= mask;
			JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
		}
		if ( value <= minValue && (info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons &= ~mask;
			JoystickButtonEvent( code, 0 );
		}
		
		// Negative side of the axis
		mask <<= 1;
		code = (ButtonCode_t)( code + 1 );
		if ( value < -minValue && !(info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons |= mask;
			JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
		}
		if ( value >= -minValue && (info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons &= ~mask;
			JoystickButtonEvent( code, 0 );
		}
	}	
}


int	CInputSystem::HIDGetElementValue( JoystickInfo_t &info, OSXInputValue_t &value )
{
	IOReturn result = kIOReturnSuccess;
	IOHIDEventStruct hidEvent;
	hidEvent.value = 0;
	
	if ( info.m_Interface != NULL )
	{
		result = (*info.m_Interface)->getElementValue( info.m_Interface, ( IOHIDElementCookie )value.m_Cookie, &hidEvent);
		if (kIOReturnSuccess == result) 
		{
			if ( hidEvent.value < value.m_MinReport )
				value.m_MinReport = hidEvent.value;
			if ( hidEvent.value > value.m_MaxReport )
				value.m_MaxReport = hidEvent.value;
		}
	}

	return hidEvent.value;
}
	
	
int CInputSystem::HIDScaledCalibratedValue( JoystickInfo_t &info, OSXInputValue_t &value )
{
	float deviceScale = value.m_MaxVal - value.m_MinVal;
	float readScale = value.m_MaxReport - value.m_MinReport;
	int joyvalue = HIDGetElementValue( info, value );
	if (readScale == 0)
		return joyvalue;
	else
		return ( (joyvalue - value.m_MinReport) * deviceScale / readScale) + value.m_MinVal;
}
	

//-----------------------------------------------------------------------------
// Purpose: Get raw joystick sample along axis
//-----------------------------------------------------------------------------
unsigned int CInputSystem::AxisValue( JoystickAxis_t axis, JoystickInfo_t &info )
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return (unsigned int)HIDScaledCalibratedValue( info, info.m_xaxis );
	case JOY_AXIS_Y:
		return (unsigned int)HIDScaledCalibratedValue( info, info.m_yaxis );
	case JOY_AXIS_Z:
		return (unsigned int)HIDScaledCalibratedValue( info, info.m_zaxis );
	case JOY_AXIS_R:
		return (unsigned int)HIDScaledCalibratedValue( info, info.m_raxis );
	case JOY_AXIS_U:
		return (unsigned int)HIDScaledCalibratedValue( info, info.m_uaxis );
	case JOY_AXIS_V:
		return (unsigned int)HIDScaledCalibratedValue( info, info.m_vaxis );
	}
	// FIX: need to do some kind of error
	return (unsigned int)HIDScaledCalibratedValue( info, info.m_xaxis );
}


//-----------------------------------------------------------------------------
// Update the joystick POV control
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickPOVControl( int nJoystick )
{
	JoystickInfo_t &info = m_pJoystickInfo[nJoystick];

	if ( !info.m_bHasPOVControl )
		return;

	// convert POV information into 4 bits of state information
	// this avoids any potential problems related to moving from one
	// direction to another without going through the center position
	unsigned int povstate = 0;
		
	int range = ( info.m_POV.m_MaxVal - info.m_POV.m_MinVal + 1 );
	int value = HIDGetElementValue( info, info.m_POV ) - info.m_POV.m_MinVal;
	if (range == 4)         /* 4 position hatswitch - scale up value */
		value *= 2;
	else if (range != 8)    /* Neither a 4 nor 8 positions - fall back to default position (centered) */
		value = -1;
	switch (value) 
	{
		case 0:
			povstate |= 0x01;
			break;
		case 1:
			povstate |= 0x01;
			if ( info.m_bDiagonalPOVControlEnabled )
				povstate |= 0x02;
			break;
		case 2:
			povstate |= 0x02;
			break;
		case 3:
			povstate |= 0x02;
			if ( info.m_bDiagonalPOVControlEnabled )
				povstate |= 0x04;
			break;
		case 4:
			povstate |= 0x04;
			break;
		case 5:
			povstate |= 0x04;
			if ( info.m_bDiagonalPOVControlEnabled )
				povstate |= 0x08;
			break;
		case 6:
			povstate |= 0x08;
			break;
		case 7:
			povstate |= 0x08;
			if ( info.m_bDiagonalPOVControlEnabled )
				povstate |= 0x01;
			break;
		default:
			povstate = 0;
			break;
	}

	// determine which bits have changed and key an auxillary event for each change
	unsigned int buttons = povstate ^ info.m_nLastPolledPOVState;
	if ( buttons )
	{
		for ( int i = 0; i < JOYSTICK_POV_BUTTON_COUNT; ++i )
		{
			unsigned int mask = buttons & ( 1 << i );
			if ( !mask )
				continue;

			ButtonCode_t code = (ButtonCode_t)JOYSTICK_POV_BUTTON( nJoystick, i );

			if ( mask & povstate )
			{
				// Keydown on POV buttons
				JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
			}
			else
			{
				// KeyUp on POV buttons
				JoystickButtonEvent( code, 0 );
			}
		}

		// Latch old values
		info.m_nLastPolledPOVState = povstate;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sample the joystick
//-----------------------------------------------------------------------------
void CInputSystem::PollJoystick( void )
{
	if ( !m_JoysticksEnabled.IsAnyFlagSet() )
		return;

	InputState_t &state = m_InputState[ m_bIsPolling ];
	for ( int i = 0; i < m_nJoystickCount; ++i )
	{
		if ( !m_JoysticksEnabled.IsFlagSet( 1 << i ) )
			continue;

		JoystickInfo_t &info = m_pJoystickInfo[i];
		if ( info.m_bRemoved ) 
			continue;

		// Poll joystick axes
		for ( int j = 0; j < MAX_JOYSTICK_AXES; ++j )
		{
			if ( ( info.m_nAxisFlags & ( 1 << j ) ) == 0 )
				continue;

			AnalogCode_t code = JOYSTICK_AXIS( i, j );
			int nValue = AxisValue( (JoystickAxis_t)j, info );
			float minValue = joy_axis_deadzone.GetFloat();
			switch (j)
			{
				default:
				case 0:
					minValue *= info.m_xaxis.m_MaxVal;
					break;
				case 1:
					minValue *= info.m_yaxis.m_MaxVal;
					break;
				case 2:
					minValue *= info.m_zaxis.m_MaxVal;
					break;
				case 3:
					minValue *= info.m_raxis.m_MaxVal;
					break;
				case 4:
					minValue *= info.m_uaxis.m_MaxVal;
					break;				
				case 5:
					minValue *= info.m_vaxis.m_MaxVal;
					break;				
			}
			
			if ( fabs(nValue) < minValue )
				nValue = 0;
			
			state.m_pAnalogDelta[ code ] = nValue - state.m_pAnalogValue[ code ];
			state.m_pAnalogValue[ code ] = nValue;
			
			if ( state.m_pAnalogDelta[ code ] != 0 )
			{
				//printf( "Joystick %i %d %d\n", code, state.m_pAnalogValue[ code ], state.m_pAnalogDelta[ code ] );
				PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, code, state.m_pAnalogValue[ code ], state.m_pAnalogDelta[ code ] );
			}
		}

		UpdateJoystickButtonState( i );
		UpdateJoystickPOVControl( i );
	}
}


void CInputSystem::SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId )
{
	for ( int i = 0; i < m_nJoystickCount; ++i )
	{
		JoystickInfo_t &info = m_pJoystickInfo[i];
		if ( info.m_bRemoved || !info.m_FFInterface ) 
			continue;

		if ( m_bXController && !info.m_bXBoxRumbleEnabled )
		{
			info.m_bXBoxRumbleEnabled = true;
			FFEFFESCAPE escape;
			char c;
			c=0x01;
			escape.dwSize=sizeof(escape);
			escape.dwCommand=0x00;
			escape.cbInBuffer=sizeof(c);
			escape.lpvInBuffer=&c;
			escape.cbOutBuffer=0;
			escape.lpvOutBuffer=NULL;
			FFDeviceEscape( info.m_FFInterface ,&escape );
		}
		FFEFFESCAPE escape;
		char c[2];
			
		c[0]=(unsigned char) (fLeftMotor*255.0);
		c[1]=(unsigned char) (fRightMotor*255.0);
		escape.dwSize=sizeof(escape);
		escape.dwCommand=0x01;
		escape.cbInBuffer=sizeof(c);
		escape.lpvInBuffer=c;
		escape.cbOutBuffer=0;
		escape.lpvOutBuffer=NULL;
		FFDeviceEscape( info.m_FFInterface ,&escape );		
	}
}

// [will] BEGIN - These were copied from xcontroller.cpp for X360 button press emulation in OSX.
// We don't want to fully support XController on OSX, just enough to emulate Xbox Controller button presses.
// So instead of #defining out half of xcontroller.cpp for OSX, just copied it here.
#if !defined( _CERT )

#define XBX_MAX_BUTTONSAMPLE		32768
#define XBX_MAX_ANALOGSAMPLE		255
#define XBX_MAX_STICKSAMPLE_LEFT	32768
#define XBX_MAX_STICKSAMPLE_RIGHT	32767
#define XBX_MAX_STICKSAMPLE_DOWN	32768
#define XBX_MAX_STICKSAMPLE_UP		32767

#define XBX_STICK_SCALE_LEFT(x) 	( ( float )XBX_MAX_STICKSAMPLE_LEFT/( float )( XBX_MAX_STICKSAMPLE_LEFT-(x) ) )
#define XBX_STICK_SCALE_RIGHT(x) 	( ( float )XBX_MAX_STICKSAMPLE_RIGHT/( float )( XBX_MAX_STICKSAMPLE_RIGHT-(x) ) )
#define XBX_STICK_SCALE_DOWN(x) 	( ( float )XBX_MAX_STICKSAMPLE_DOWN/( float )( XBX_MAX_STICKSAMPLE_DOWN-(x) ) )
#define XBX_STICK_SCALE_UP(x)	 	( ( float )XBX_MAX_STICKSAMPLE_UP/( float )( XBX_MAX_STICKSAMPLE_UP-(x) ) )

#define XBX_STICK_SMALL_THRESHOLD	((int)( 0.20f * XBX_MAX_STICKSAMPLE_LEFT ))

// Threshold for counting analog movement as a button press
#define JOYSTICK_ANALOG_BUTTON_THRESHOLD	XBX_MAX_STICKSAMPLE_LEFT * 0.4f
//-----------------------------------------------------------------------------
//	Purpose: Post Xbox events, ignoring key repeats
//-----------------------------------------------------------------------------
void CInputSystem::PostXKeyEvent( int userId, xKey_t xKey, int nSample )
{
	AnalogCode_t	code	= ANALOG_CODE_LAST;
	float			value	= 0.f;

	// Map the physical controller slot to the split screen slot
#if defined( _GAMECONSOLE )
	int nMsgSlot = XBX_GetSlotByUserId( userId );
	#ifdef _PS3
	if ( ( XBX_GetNumGameUsers() <= 1 ) && !ps3_joy_ss.GetBool() )
	{
		// In PS3 START button identification mode START key notification
		// is replaced with INACTIVE_START notification that can identify
		// controller that pressed the button
		if ( ( xKey == XK_BUTTON_START ) && ( nMsgSlot < 0 )
			&& ( ( Plat_FloatTime() - g_ps3_flTimeStartButtonIdentificationMode ) < 0.5f ) )
		{
			xKey = XK_BUTTON_INACTIVE_START;
			nMsgSlot = userId;
		}
		else
		{
			// When we don't have splitscreen then any controller can
			// play and will be visible as controller #0
			nMsgSlot = 0;
		}
	}
	#endif
	if ( nMsgSlot < 0 )
	{
		// special case, that if you press start on a controller we've marked inactive, switch it to an
		// XK_BUTTON_INACTIVE_START which you can handle joins from inactive controllers
		if ( xKey == XK_BUTTON_START )
		{
			xKey = XK_BUTTON_INACTIVE_START;
			nMsgSlot = userId;
		}
		else
		{
			return; // We are not listening to this controller (not signed in and assigned)
		}
	}
#else //defined( _GAMECONSOLE )
	int nMsgSlot = userId;
#endif //defined( _GAMECONSOLE )

	int nSampleThreshold = 0;

	// Look for changes on the analog axes
	switch( xKey )
	{
	case XK_STICK1_LEFT:
	case XK_STICK1_RIGHT:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_X );
			value = ( xKey == XK_STICK1_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK1_UP:
	case XK_STICK1_DOWN:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_Y );
			value = ( xKey == XK_STICK1_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK2_LEFT:
	case XK_STICK2_RIGHT:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_U );
			value = ( xKey == XK_STICK2_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK2_UP:
	case XK_STICK2_DOWN:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_R );
			value = ( xKey == XK_STICK2_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;
	}

	// Store the analog event
	if ( ANALOG_CODE_LAST != code )
	{
		InputState_t &state = m_InputState[ m_bIsPolling ];
		state.m_pAnalogDelta[ code ] = ( int )( value - state.m_pAnalogValue[ code ] );
		state.m_pAnalogValue[ code ] = ( int )value;
		if ( state.m_pAnalogDelta[ code ] != 0 )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, code, ( int )value, state.m_pAnalogDelta[ code ] );
		}
	}

	// store the key
	m_appXKeys[userId][xKey].sample = nSample;
	if ( nSample > nSampleThreshold )
	{
		m_appXKeys[userId][xKey].repeats++;
	}
	else
	{
		m_appXKeys[userId][xKey].repeats = 0;
		nSample = 0;
	}

	if ( m_appXKeys[userId][xKey].repeats > 1 )
	{
		// application cannot handle streaming keys
		// first keypress is the only edge trigger
		return;
	}

	// package the key
	ButtonCode_t buttonCode = XKeyToButtonCode( nMsgSlot, xKey );
	if ( nSample )
	{
		PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode );

		// [dkorus] check whether we're trying to set the current controller
		if( ( buttonCode == KEY_XBUTTON_A || buttonCode == XK_BUTTON_START )
			&& m_setCurrentInputDeviceOnNextButtonPress )
		{
			if( IsInputDeviceConnected( INPUT_DEVICE_GAMEPAD ) )
			{
				SetCurrentInputDevice( INPUT_DEVICE_GAMEPAD );
				ConVarRef var( "joystick" );
				if( var.IsValid( ) )
					var.SetValue( 1 );
				m_setCurrentInputDeviceOnNextButtonPress = false;
			}
		}

	}
	else
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode );
	}
}
#endif // _CERT
// [will] END - These were copied from xcontroller.cpp for X360 button press emulation in OSX.

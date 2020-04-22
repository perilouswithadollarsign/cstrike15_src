//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Platform level security functions.
//
// $NoKeywords: $
//=============================================================================//

// Uncomment the following line to require the prescence of a hardware key.
//#define REQUIRE_HARDWARE_KEY
// NOTE - this DOESN'T work under linux!!!!


#include "tier0/platform.h"
#include "tier0/memalloc.h"

#ifdef REQUIRE_HARDWARE_KEY

	#define VALVE_DESKEY_ID "uW"	// Uniquely identifies HL2 keys

	// Include the key's API:
	#include "deskey/algo.h"
	#include "deskey/dk2win32.h"
	
//	#pragma comment(lib, "DESKey/algo32.lib" )
//	#pragma comment(lib, "DESKey/dk2win32.lib" )

#endif


// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"


bool Plat_VerifyHardwareKey()
{
	#ifdef REQUIRE_HARDWARE_KEY
		
		// Ensure that a key with our ID exists:
		if( FindDK2( VALVE_DESKEY_ID, NULL ) )
			return true;

		return false;

	#else

		return true;

	#endif
}

bool Plat_VerifyHardwareKeyDriver()
{
	#ifdef REQUIRE_HARDWARE_KEY

		// Ensure that the driver is at least installed:
		return DK2DriverInstalled() != 0; 

	#else

		return true;

	#endif
}

bool Plat_VerifyHardwareKeyPrompt()
{
	#ifdef REQUIRE_HARDWARE_KEY

		if( !DK2DriverInstalled() )
		{
			if( IDCANCEL == MessageBox( NULL, "No drivers detected for the hardware key, please install them and re-run the application.\n", "No Driver Detected", MB_OKCANCEL ) )
			{
				return false;
			}

		}

		while( !Plat_VerifyHardwareKey() )
		{
			if( IDCANCEL == MessageBox( NULL, "Please insert the hardware key and hit 'ok'.\n", "Insert Hardware Key", MB_OKCANCEL ) )
			{
				return false;
			}

			for( int i=0; i < 2; i++ )
			{
				// Is the key in now?
				if( Plat_VerifyHardwareKey() )
				{
					return true;
				}

				// Sleep 2 / 3 of a second before trying again, in case the os recognizes the key slightly after it's being inserted:
				Sleep(666);
			}
		}

		return true;
	
	#else

		return true;

	#endif
}

bool Plat_FastVerifyHardwareKey()
{
	#ifdef REQUIRE_HARDWARE_KEY

		static int nIterations = 0;
		
		nIterations++;
		if( nIterations > 100 )
		{
			nIterations = 0;
			return Plat_VerifyHardwareKey();
		}

		return true;

	#else

		return true;

	#endif
}


// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"


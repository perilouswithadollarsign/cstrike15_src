//========= Copyright © 1996-2005, Valve LLC, All rights reserved. ============
//
// Purpose: Common XBox Launch data passed between apps
//
//=============================================================================
#include "ps3_platform.h"

#define RELAUNCH_MAGIC_NUMBER 0xbd122969

// used to hold persistent states across restart
struct RelaunchHeader_t
{
	unsigned int	magicNumber;
	unsigned int	contextCode;		// the context code that was used
	unsigned int	nBytesRelaunchData;
	unsigned int	activeDevice;		// which controller was active
	__int64			startTime;			// used to track duration of relaunch
	bool			bRetail;			// running as retail mode
	bool			bInDebugger;		// in debug session
};

#pragma pack()

#define GetRelaunchHeader( x ) (((RelaunchHeader_t *)(((unsigned int)(x)) + MAX_LAUNCH_DATA_SIZE / 2)) - 1)

// a context code is passed to installer or dashboard
// the dashboard passes the context code to the installer
// installer exits and launches HL2 with RelaunchHeader 
#define CONTEXTCODE_HL2MAGIC	0x9E000000
#define CONTEXTCODE_MAGICMASK	0xFF000000
// xbe image type
#define CONTEXTCODE_DEBUG_XBE	0x00000001	// running the debug xbe
#define CONTEXTCODE_RELEASE_XBE	0x00000002	// running the release xbe
#define CONTEXTCODE_RETAIL_XBE	0x00000004	// running the retail xbe
// mode options
#define CONTEXTCODE_RETAIL_MODE	0x00000010	// running the desired xbe in retail mode
#define CONTEXTCODE_INDEBUGGER	0x00000020	// running during a debugger session
#define CONTEXTCODE_NO_XBDM		0x00000040	// No XBDM calls
// operation commands
#define CONTEXTCODE_DASHBOARD	0x00010000	// pass through immediately to hl2
#define CONTEXTCODE_ATTRACT		0x00020000	// run the attract mode
#define CONTEXTCODE_LOADMAP		0x00040000	// restart directly to load a map
#define CONTEXTCODE_QUIT		0x00080000	// quit game, go directly to main menu

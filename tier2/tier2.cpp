//===== Copyright (c) 2005-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//=============================================================================//

#include <tier2/tier2.h>
#include "tier0/dbg.h"
#include "tier2/resourceprecacher.h"
#include "resourcesystem/iresourcesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// These tier2 libraries must be set by any users of this library.
// They can be set by calling ConnectTier2Libraries or InitDefaultFileSystem.
// It is hoped that setting this, and using this library will be the common mechanism for
// allowing link libraries to access tier2 library interfaces
//-----------------------------------------------------------------------------

// Fade data.
FadeData_t g_aFadeData[FADE_MODE_COUNT] = 
{
	//	PixelMin	PixelMax	Width		DistScale		FadeMode_t
#ifdef CSTRIKE15
	// Ensure fade settings are consistent across CPU levels in CS:GO.
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_NONE 
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_LOW
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_MED
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_HIGH
#else
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_NONE 
	{	 10.0f,		 15.0f,		 800.0f,	1.0f	}, //	FADE_MODE_LOW
	{	  5.0f,		 10.0f,		1024.0f,	1.0f	}, //	FADE_MODE_MED
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_HIGH
#endif

	{	 0.0f,		0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_360
	{	 0.0f,		0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_PS3
	{	  0.0f,		  0.0f,		1280.0f,	1.0f	}, //	FADE_MODE_LEVEL
};


//-----------------------------------------------------------------------------
// Used by the resource system for fast resource frame counter
//-----------------------------------------------------------------------------
uint32 g_nResourceFrameCount;
static bool s_bResourceFCRegistered;
static bool s_bPrecachesRegistered;


//-----------------------------------------------------------------------------
// Call this to connect to all tier 2 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier2Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	if ( g_pPrecacheSystem && !s_bPrecachesRegistered )
	{
		// Make all the PRECACHE_ macros register w/precache system now that it's connected
		CBaseResourcePrecacher::RegisterAll(); 
		s_bPrecachesRegistered = true;
	}

	if ( g_pResourceSystem && !s_bResourceFCRegistered )
	{
		g_pResourceSystem->RegisterFrameCounter( &g_nResourceFrameCount );
		s_bResourceFCRegistered = true;

		CSchemaClassBindingBase::Install();
	}
}

void DisconnectTier2Libraries()
{
	if ( g_pResourceSystem && s_bResourceFCRegistered )
	{
		g_pResourceSystem->UnregisterFrameCounter( &g_nResourceFrameCount );
		s_bResourceFCRegistered = false;
	}
}



//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Steam API context hooks
//

#include "mm_framework.h"

#include "memdbgon.h"

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )

// Context for the Game Coordinator
#ifndef NO_STEAM_GAMECOORDINATOR
static GCSDK::CGCClient g_GCClient;
GCSDK::CGCClient *GGCClient()
{
	return &g_GCClient;
}
#endif

static CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;

// Init the steam APIs
void SteamApiContext_Init()
{
#ifndef _PS3
	if ( !SteamAPI_InitSafe() )
		return;
#endif

	if ( !steamapicontext->Init() )
		return;

#if !defined( _DEMO ) && !defined( NO_STEAM_GAMECOORDINATOR )
	if ( SteamClient() )
	{
		ISteamGameCoordinator *pGCInterface = 
			(ISteamGameCoordinator*)SteamClient()->GetISteamGenericInterface( SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMGAMECOORDINATOR_INTERFACE_VERSION );
		GGCClient()->BInit( pGCInterface );
	}
#endif
}

// Shut down the steam APIs
void SteamApiContext_Shutdown()
{
	steamapicontext->Clear();
	// SteamAPI_Shutdown(); << Steam shutdown is controlled by engine
#ifndef NO_STEAM_GAMECOORDINATOR
	GGCClient()->Uninit();
	GCSDK::UninitTempTextBuffers();
#endif
}

#else

class CSteamAPIContext *steamapicontext = NULL;
void SteamApiContext_Init()
{
}

void SteamApiContext_Shutdown()
{
}

#endif

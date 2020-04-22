//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Steam API context exposure
//

#ifndef MATCHMAKING_STEAM_API_HOOK_H
#define MATCHMAKING_STEAM_API_HOOK_H

#ifdef _WIN32
#pragma once
#endif

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
// Steam uses C-runtime calls in headers, need to remap
	#ifdef strncpy
		#undef strncpy
		#define strncpy Q_strncpy
	#endif
	#ifdef _snprintf
		#undef _snprintf
		#define _snprintf Q_snprintf
	#endif
#include "steam/steam_api.h"

#ifndef NO_STEAM_GAMECOORDINATOR
#include "gcsdk/gcclientsdk.h"
GCSDK::CGCClient *GGCClient();
#endif

#endif

extern class CSteamAPIContext *steamapicontext;

void SteamApiContext_Init();
void SteamApiContext_Shutdown();

#include "steam_lobbyapi.h"


#endif

//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef STEAM_LOBBYAPI_H
#define STEAM_LOBBYAPI_H

#ifdef _WIN32
#pragma once
#endif

#if !defined( _X360 ) && !defined( NO_STEAM )

void Steam_WriteLeaderboardData( KeyValues *pViewDescription, KeyValues *pViewData );

#endif

#endif


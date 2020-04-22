//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.
//
// $NoKeywords: $
//===========================================================================//


#ifndef LOAD_SCREEN_UPDATE_H
#define LOAD_SCREEN_UPDATE_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"


//-----------------------------------------------------------------------------
// Activate, deactivate loader updates
//-----------------------------------------------------------------------------
void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode );
void RefreshScreenIfNecessary();
void EndLoadingUpdates();
void PauseLoadingUpdates( bool bPause );

#endif /* LOAD_SCREEN_UPDATE_H */

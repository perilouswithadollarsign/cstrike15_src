//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Methods associated with the cursor
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef MATSURFACE_CURSOR_H
#define MATSURFACE_CURSOR_H

#ifdef _WIN32
#pragma once
#endif

#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui/Cursor.h>

FORWARD_DECLARE_HANDLE( InputContextHandle_t );


//-----------------------------------------------------------------------------
// Initializes cursors
//-----------------------------------------------------------------------------
void InitCursors();


//-----------------------------------------------------------------------------
// Selects a cursor
//-----------------------------------------------------------------------------
void CursorSelect( InputContextHandle_t hContext, vgui::HCursor hCursor );


//-----------------------------------------------------------------------------
// Activates the current cursor
//-----------------------------------------------------------------------------
void ActivateCurrentCursor( InputContextHandle_t hContext );


//-----------------------------------------------------------------------------
// handles mouse movement
//-----------------------------------------------------------------------------
void CursorSetPos( InputContextHandle_t hContext, int x, int y );
void CursorGetPos( InputContextHandle_t hContext, int &x, int &y );


//-----------------------------------------------------------------------------
// Purpose: prevents vgui from changing the cursor
//-----------------------------------------------------------------------------
void LockCursor( InputContextHandle_t hContext, bool bEnable );


//-----------------------------------------------------------------------------
// Purpose: unlocks the cursor state
//-----------------------------------------------------------------------------
bool IsCursorLocked();

//-----------------------------------------------------------------------------
// Purpose: loads a custom cursor file from the file system
//-----------------------------------------------------------------------------
vgui::HCursor Cursor_CreateCursorFromFile( char const *curOrAniFile, char const *pPathID );

// Helper for shutting down cursors
void Cursor_ClearUserCursors();

#endif // MATSURFACE_CURSOR_H






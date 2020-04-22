//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef PLATWINDOW_H
#define PLATWINDOW_H

#ifdef COMPILER_MSVC32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/basetypes.h"

//-----------------------------------------------------------------------------
// Window handle
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( PlatWindow_t );
#define PLAT_WINDOW_INVALID ( (PlatWindow_t)0 )


//-----------------------------------------------------------------------------
// Window creation
//-----------------------------------------------------------------------------
enum WindowCreateFlags_t
{
	WINDOW_CREATE_FULLSCREEN	= 0x1,
	WINDOW_CREATE_RESIZING		= 0x2,

};

PLATFORM_INTERFACE PlatWindow_t Plat_CreateWindow( void *hInstance, const char *pTitle, int nWidth, int nHeight, int nFlags );


//-----------------------------------------------------------------------------
// Window title
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void Plat_SetWindowTitle( PlatWindow_t hWindow, const char *pTitle );


//-----------------------------------------------------------------------------
// Window movement
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void Plat_SetWindowPos( PlatWindow_t hWindow, int x, int y );


//-----------------------------------------------------------------------------
// Gets the desktop resolution
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void Plat_GetDesktopResolution( int *pWidth, int *pHeight );


//-----------------------------------------------------------------------------
// Gets a window size
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void Plat_GetWindowClientSize( PlatWindow_t hWindow, int *pWidth, int *pHeight );


//-----------------------------------------------------------------------------
// Is the window minimized?
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE bool Plat_IsWindowMinimized( PlatWindow_t hWindow );


//-----------------------------------------------------------------------------
// Gets the shell window in a console app
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE PlatWindow_t Plat_GetShellWindow( );


//-----------------------------------------------------------------------------
// Convert window -> Screen coordinates and back
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y );
PLATFORM_INTERFACE void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y );


#endif // PLATWINDOW_H

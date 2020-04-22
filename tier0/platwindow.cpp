//========== Copyright © 2007, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_tier0.h"
#include "tier0/platwindow.h"

#if defined( PLATFORM_WINDOWS )
#if !defined( PLATFORM_X360 )
#include <windows.h>
#else
#include "xbox/xbox_win32stubs.h"
#endif
#endif


#ifdef PLATFORM_WINDOWS

//-----------------------------------------------------------------------------
// Window creation
//-----------------------------------------------------------------------------
PlatWindow_t Plat_CreateWindow( void *hInstance, const char *pTitle, int nWidth, int nHeight, int nFlags )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
	wc.style         = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc   = DefWindowProc;
	wc.hInstance     = (HINSTANCE)hInstance;
	wc.lpszClassName = "Valve001";
	wc.hIcon		 = NULL; //LoadIcon( s_HInstance, MAKEINTRESOURCE( IDI_LAUNCHER ) );
	wc.hIconSm		 = wc.hIcon;

	RegisterClassEx( &wc );

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	style &= ~WS_MAXIMIZEBOX;

	if ( ( nFlags & WINDOW_CREATE_FULLSCREEN ) == 0 )
	{
		// Give it a frame
		style |= WS_OVERLAPPEDWINDOW;
		if ( nFlags & WINDOW_CREATE_RESIZING )
		{
			style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
		}
		else
		{
			style &= ~WS_THICKFRAME;
		}
	}

	RECT windowRect;
	windowRect.top		= 0;
	windowRect.left		= 0;
	windowRect.right	= nWidth;
	windowRect.bottom	= nHeight;

	// Compute rect needed for that size client area based on window style
	AdjustWindowRectEx( &windowRect, style, FALSE, 0 );

	// Create the window
	void *hWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, (HINSTANCE)hInstance, NULL );

	return (PlatWindow_t)hWnd;
}


//-----------------------------------------------------------------------------
// Window title
//-----------------------------------------------------------------------------
void Plat_SetWindowTitle( PlatWindow_t hWindow, const char *pTitle )
{
#ifdef PLATFORM_WINDOWS_PC
	SetWindowText( (HWND)hWindow, pTitle );
#endif
}


//-----------------------------------------------------------------------------
// Window movement
//-----------------------------------------------------------------------------
void Plat_SetWindowPos( PlatWindow_t hWindow, int x, int y )
{
	SetWindowPos( (HWND)hWindow, NULL, x, y, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME );
}


//-----------------------------------------------------------------------------
// Gets the desktop resolution
//-----------------------------------------------------------------------------
void Plat_GetDesktopResolution( int *pWidth, int *pHeight )
{
	*pWidth = GetSystemMetrics( SM_CXSCREEN );
	*pHeight = GetSystemMetrics( SM_CYSCREEN );
}

//-----------------------------------------------------------------------------
// Gets a window size
//-----------------------------------------------------------------------------
void Plat_GetWindowClientSize( PlatWindow_t hWindow, int *pWidth, int *pHeight )
{
	RECT rect;
	GetClientRect( (HWND)hWindow, &rect );
	*pWidth = rect.right - rect.left;
	*pHeight = rect.bottom - rect.top;
}

//-----------------------------------------------------------------------------
// Is the window minimized?
//-----------------------------------------------------------------------------
bool Plat_IsWindowMinimized( PlatWindow_t hWindow )
{
	return IsIconic( (HWND)hWindow ) != 0;
}

//-----------------------------------------------------------------------------
// Gets the shell window in a console app
//-----------------------------------------------------------------------------
PlatWindow_t Plat_GetShellWindow( )
{
#ifdef PLATFORM_WINDOWS_PC
	return (PlatWindow_t)GetShellWindow();
#else
	return PLAT_WINDOW_INVALID;
#endif
}


//-----------------------------------------------------------------------------
// Convert window -> Screen coordinates
//-----------------------------------------------------------------------------
void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y )
{
	POINT pt;
	pt.x = x; pt.y = y;
	ClientToScreen( (HWND)hWnd, &pt );
	x = pt.x; y = pt.y;
}

void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y )
{
	POINT pt;
	pt.x = x; pt.y = y;
	ScreenToClient( (HWND)hWnd, &pt );
	x = pt.x; y = pt.y;
}


#else

//-----------------------------------------------------------------------------
// Window creation
//-----------------------------------------------------------------------------
PlatWindow_t Plat_CreateWindow( void *hInstance, const char *pTitle, int nWidth, int nHeight, int nFlags )
{
	return PLAT_WINDOW_INVALID;
}


//-----------------------------------------------------------------------------
// Window title
//-----------------------------------------------------------------------------
void Plat_SetWindowTitle( PlatWindow_t hWindow, const char *pTitle )
{
}


//-----------------------------------------------------------------------------
// Window movement
//-----------------------------------------------------------------------------
void Plat_SetWindowPos( PlatWindow_t hWindow, int x, int y )
{
}


//-----------------------------------------------------------------------------
// Gets the desktop resolution
//-----------------------------------------------------------------------------
void Plat_GetDesktopResolution( int *pWidth, int *pHeight )
{
	*pWidth = 0;
	*pHeight = 0;
}

//-----------------------------------------------------------------------------
// Gets a window size
//-----------------------------------------------------------------------------
void Plat_GetWindowClientSize( PlatWindow_t hWindow, int *pWidth, int *pHeight )
{
	*pWidth = 0;
	*pHeight = 0;
}

//-----------------------------------------------------------------------------
// Is the window minimized?
//-----------------------------------------------------------------------------
bool Plat_IsWindowMinimized( PlatWindow_t hWindow )
{
	return false;
}

//-----------------------------------------------------------------------------
// Gets the shell window in a console app
//-----------------------------------------------------------------------------
PlatWindow_t Plat_GetShellWindow( )
{
	return PLAT_WINDOW_INVALID;
}


//-----------------------------------------------------------------------------
// Convert window -> Screen coordinates
//-----------------------------------------------------------------------------
void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y )
{
}

void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y )
{
}


#endif 

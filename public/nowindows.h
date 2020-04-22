//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef NOWINDOWS_H
#define NOWINDOWS_H
#pragma once

// put defs in here to avoid including windows.h.  This one allows you to declare WinMain without windows.h

#ifndef DECLARE_HANDLE
typedef void *HANDLE;
#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
DECLARE_HANDLE(HINSTANCE);
typedef char *LPSTR;
#endif

// int __stdcall WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow );

#endif // NOWINDOWS_H

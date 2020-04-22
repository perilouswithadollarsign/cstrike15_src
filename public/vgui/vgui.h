//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Basic header for using vgui
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUI_H
#define VGUI_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"


#pragma warning( disable: 4786 )	// disables 'identifier truncated in browser information' warning
#pragma warning( disable: 4355 )	// disables 'this' : used in base member initializer list
#pragma warning( disable: 4097 )	// warning C4097: typedef-name 'BaseClass' used as synonym for class-name
#pragma warning( disable: 4514 )	// warning C4514: 'Color::Color' : unreferenced inline function has been removed
#pragma warning( disable: 4100 )	// warning C4100: 'code' : unreferenced formal parameter
#pragma warning( disable: 4127 )	// warning C4127: conditional expression is constant

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

#ifndef _WCHAR_T_DEFINED
// DAL - wchar_t is a built in define in gcc 3.2 with a size of 4 bytes
#if !defined( __x86_64__ ) && !defined( __WCHAR_TYPE__  )
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
#endif

// do this in GOLDSRC only!!!
//#define Assert assert

namespace vgui
{
// handle to an internal vgui panel
// this is the only handle to a panel that is valid across dll boundaries
typedef uintp VPANEL;

// handles to vgui objects
// NULL values signify an invalid value
typedef unsigned long HScheme;
typedef unsigned long HTexture;
typedef unsigned long HCursor;
typedef unsigned long HPanel;
const HPanel INVALID_PANEL = 0xffffffff;
typedef unsigned long HFont;
const HFont INVALID_FONT = 0; // the value of an invalid font handle

const float STEREO_NOOP = 1.0f;
const float STEREO_INVALID = 0.0f;
}

#include "tier1/strtools.h"


#endif // VGUI_H

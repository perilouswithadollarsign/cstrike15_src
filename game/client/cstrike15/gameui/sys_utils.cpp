//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "winlite.h"
#include "sys_utils.h"
#include "engineinterface.h"

// dgoodenough - select the correct stubs header based on current console
// PS3_BUILDFIX#if defined( _PS3 )
#if defined( _PS3 )
#include "ps3/ps3_win32stubs.h"
#endif
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// dgoodenough - All changes in this file duplicate the changes made for
// portal2, i.e. they stub them out on PS3
// PS3_BUILDFIX
// FIXME - I've added a FIXME tag here because despite the changes made for portal2,
// I'm not at all happy about nuking some of these.  In particular removing mutices
// and the "WaitForSingleObject" functionality seems like a recipe for disaster in
// a multithreaded environment.  Bottom line, I wouldn't mind having a second set of
// eyes give this a once over.

// @wge Doing the same for OSX. Same concerns apply.

#if defined( _OSX ) || defined (LINUX)
// @wge - adapted from portal2/sys_utils.cpp
const unsigned int SYS_NO_ERROR = 0;
const unsigned int SYS_ERROR_INVALID_HANDLE = -1;
#elif !defined( _PS3 )
const unsigned int SYS_NO_ERROR = NO_ERROR;
const unsigned int SYS_ERROR_INVALID_HANDLE = ERROR_INVALID_HANDLE;
#endif

void Sys_SetLastError(unsigned long error)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	::SetLastError(error);
#endif
}

unsigned long Sys_GetLastError()
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	return ::GetLastError();
#else
	return 0;
#endif
}


WHANDLE Sys_CreateMutex(const char *mutexName)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	return (WHANDLE)::CreateMutex(NULL, FALSE, TEXT(mutexName));
#else
	return 0;
#endif
}

void Sys_ReleaseMutex(WHANDLE mutexHandle)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	::ReleaseMutex((HANDLE)mutexHandle);
#endif
}


#if defined( _OSX ) || defined (LINUX)
// @wge - adapted from portal2/sys_utils.cpp
const unsigned int SYS_WAIT_OBJECT_0 = WAIT_OBJECT_0;
const unsigned int SYS_WAIT_ABANDONED = -2;
#elif !defined( _PS3 )
const unsigned int SYS_WAIT_OBJECT_0 = WAIT_OBJECT_0;
const unsigned int SYS_WAIT_ABANDONED = WAIT_ABANDONED;
#endif

unsigned int Sys_WaitForSingleObject(WHANDLE mutexHandle, int milliseconds)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	return WaitForSingleObject((HANDLE)mutexHandle, milliseconds);
#else
    return -1;
#endif
}

unsigned int Sys_RegisterWindowMessage(const char *msgName)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	return ::RegisterWindowMessage(msgName);
#else
	return 0;
#endif
}

WHANDLE Sys_FindWindow(const char *className, const char *windowName)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	return (WHANDLE)::FindWindow(className, windowName);
#else
	return 0;
#endif
}

void Sys_EnumWindows(void *callbackFunction, int lparam)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	::EnumWindows((WNDENUMPROC)callbackFunction, lparam);
#endif
}

void Sys_GetWindowText(WHANDLE wnd, char *buffer, int bufferSize)
{
// dgoodenough - duplicate changes in portal2, i.e. stub these out on PS3
// PS3_BUILDFIX
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	::GetWindowText((HWND)wnd, buffer, bufferSize - 1);
#else
	buffer[0] = 0;
#endif
}

void Sys_PostMessage(WHANDLE wnd, unsigned int msg, unsigned int wParam, unsigned int lParam)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	::PostMessageA((HWND)wnd, msg, wParam, lParam);
#endif
}

void Sys_SetCursorPos(int x, int y)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	::SetCursorPos(x, y);
//	engine->SetCursorPos(x,y); // SRC version
#endif
}

#if !defined( _OSX ) && !defined (LINUX)
static ATOM staticWndclassAtom = 0;
static WNDCLASS staticWndclass = { NULL };

static LRESULT CALLBACK staticProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
	return DefWindowProc(hwnd,msg,wparam,lparam);
#else
	return 0;
#endif
}
#endif

WHANDLE Sys_CreateWindowEx(const char *windowName)
{
	/*
	if (!staticWndclassAtom)
	{
		memset( &staticWndclass,0,sizeof(staticWndclass) );
		staticWndclass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		staticWndclass.lpfnWndProc = staticProc;
		staticWndclass.hInstance = GetModuleHandle(NULL);
		staticWndclass.hIcon = 0;
		staticWndclass.lpszClassName = windowName;
		staticWndclassAtom = ::RegisterClass( &staticWndclass );

		DWORD error = ::GetLastError();
	}

	return (WHANDLE)::CreateWindow(windowName, windowName, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(NULL), 0);
	*/
	return (WHANDLE)1;
}

void Sys_DestroyWindow(WHANDLE wnd)
{
	//::DestroyWindow((HWND)wnd);
}


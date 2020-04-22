//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "winlite.h"
#include "Sys_Utils.h"
#include "EngineInterface.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#if defined( _PS3 )
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


const unsigned int SYS_NO_ERROR = 0;

const unsigned int SYS_ERROR_INVALID_HANDLE =
					#ifdef ERROR_INVALID_HANDLE
					ERROR_INVALID_HANDLE
					#else
					-1
					#endif
;

void Sys_SetLastError(unsigned long error)
{
#ifdef _WIN32
	::SetLastError(error);
#endif
}

unsigned long Sys_GetLastError()
{
#ifdef _WIN32
	return ::GetLastError();
#else
	return 0;
#endif
}


WHANDLE Sys_CreateMutex(const char *mutexName)
{
#ifdef _WIN32
	return (WHANDLE)::CreateMutex(NULL, FALSE, TEXT(mutexName));
#else
	return (WHANDLE) NULL;
#endif
}

void Sys_ReleaseMutex(WHANDLE mutexHandle)
{
#ifdef _WIN32
	::ReleaseMutex((HANDLE)mutexHandle);
#endif
}


const unsigned int SYS_WAIT_OBJECT_0 = WAIT_OBJECT_0;
const unsigned int SYS_WAIT_ABANDONED =
				#ifdef WAIT_ABANDONED
				WAIT_ABANDONED
				#else
				-2
				#endif
;

unsigned int Sys_WaitForSingleObject(WHANDLE mutexHandle, int milliseconds)
{
#ifdef _WIN32
	return WaitForSingleObject((HANDLE)mutexHandle, milliseconds);
#else
	return -1;
#endif
}

unsigned int Sys_RegisterWindowMessage(const char *msgName)
{
#ifdef _WIN32
	return ::RegisterWindowMessage(msgName);
#else
	return 0;
#endif
}

WHANDLE Sys_FindWindow(const char *className, const char *windowName)
{
#ifdef _WIN32
	return (WHANDLE)::FindWindow(className, windowName);
#else
	return 0;
#endif
}

void Sys_EnumWindows(void *callbackFunction, int lparam)
{
#ifdef _WIN32
	::EnumWindows((WNDENUMPROC)callbackFunction, lparam);
#endif
}

#if defined( _WIN32)
void Sys_GetWindowText(WHANDLE wnd, char *buffer, int bufferSize)
{
	::GetWindowText((HWND)wnd, buffer, bufferSize - 1);
}
#endif

void Sys_PostMessage(WHANDLE wnd, unsigned int msg, unsigned int wParam, unsigned int lParam)
{
#if defined( _WIN32)
	::PostMessageA((HWND)wnd, msg, wParam, lParam);
#endif
}

#if defined( _WIN32)
void Sys_SetCursorPos(int x, int y)
{
	::SetCursorPos(x, y);
//	engine->SetCursorPos(x,y); // SRC version
}
#endif

#if defined( _WIN32)
static ATOM staticWndclassAtom = 0;
static WNDCLASS staticWndclass = { NULL };

static LRESULT CALLBACK staticProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
	return DefWindowProc(hwnd,msg,wparam,lparam);
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

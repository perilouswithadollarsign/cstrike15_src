//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: PS/3 Platform include
//
// $NoKeywords: $
//=============================================================================

#ifndef _PS3_PLATFORM_H
#define _PS3_PLATFORM_H

//EAPS3 #undef Verify // can't use verify, because there's a Verify member of struct D3DPushBuffer

#include <stdio.h>
#include <math.h>
#include <wchar.h>
#include <string.h>  // needed for memset

//EAPS3: Defines of win32 types used in Visual Studio.
typedef void                VOID;
typedef const void *		LPCVOID;
typedef unsigned long       DWORD;
typedef DWORD *				LPDWORD;
typedef int                 BOOL;
typedef int *               LPBOOL;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef short               SHORT;
typedef long                LONG;
typedef char                CHAR;
typedef unsigned int	    UINT;
typedef unsigned long       ULONG;
typedef unsigned char       BYTE;
typedef BYTE*               LPBYTE;
typedef long long			DWORDLONG;

typedef void*               PVOID;
typedef int				    DWORD_PTR;
typedef int			        ULONG_PTR;
typedef long*               LONG_PTR;
typedef unsigned int*       UINT_PTR;
typedef WORD*               LPWORD;
typedef ULONG_PTR *			PDWORD_PTR;
typedef ULONG_PTR			SIZE_T;
typedef ULONG_PTR *			PSIZE_T;

typedef wchar_t             WCHAR;
typedef CHAR                TCHAR;

typedef const char*         LPCSTR;
typedef char*               LPSTR;
typedef LPSTR               LPTSTR;
typedef const char*		    LPCTSTR;
typedef const wchar_t*      LPCWSTR;
typedef WCHAR *				LPWSTR;
typedef WCHAR *				PWSTR;

typedef void*		        HICON;
typedef void*               HCURSOR;
typedef void*               HBRUSH;
typedef void*               HMENU;
typedef void*		        HFONT;
typedef void*		        HBITMAP;

typedef DWORD               COLORREF;
typedef void*               HINSTANCE;
typedef UINT                MMRESULT;
typedef long                HRESULT;
typedef long                LRESULT;
typedef void*               HANDLE;
//typedef void*		        HWND;
typedef void*               LPVOID;
typedef unsigned int        WPARAM;
typedef int			        LPARAM;
typedef void*		        HDC;
typedef void*		        HHOOK;
typedef void*		        HMODULE;
typedef void*				HKL;
typedef void*				HKEY;
typedef HKEY*				PHKEY;
typedef void*				HGDIOBJ;
typedef WORD                ATOM;
typedef HANDLE              HGLOBAL;
//typedef WORD                WAVEFORMATEX;

typedef long long           __int64;

typedef float				vec3_t[3];
typedef signed long long    s64_t;
typedef unsigned long long	u64_t;
typedef signed int			s32_t;
typedef unsigned int		u32_t;
typedef signed short		s16_t;
typedef unsigned short		u16_t;
typedef signed char			s8_t;
typedef unsigned char		u8_t;
typedef unsigned char		byte_t;
typedef unsigned int		rgba_t;

typedef long long           ULARGE_INTEGER;

typedef u64_t				ULONGLONG;

typedef struct _POINTL      /* ptl  */
{
	LONG  x;
	LONG  y;
} POINT, POINTL, *PPOINTL, *LPPOINT;

typedef struct _GUID
{
	unsigned long Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
} GUID;

typedef struct tagRECT
{
	LONG    left;
	LONG    top;
	LONG    right;
	LONG    bottom;
} RECT, *PRECT, *NPRECT, *LPRECT;

typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SECURITY_ATTRIBUTES
{
	DWORD nLength;
	/* [size_is] */ LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} 	SECURITY_ATTRIBUTES;

typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;

// Function call convention
#define _cdecl
#define __cdecl
#define __declspec(x)

#define WINAPI
#define FAR
#define NEAR
#define CONST       const
#define CALLBACK
#define IN
#define OPTIONAL

struct PDM_CMDCONT
{

};

#ifndef __stdcall
#define __stdcall
#endif
//typedef HRESULT (__stdcall *PDM_CMDPROC)(LPCSTR szCommand, LPSTR szResponse, DWORD cchResponse, PDM_CMDCONT pdmcc);
#undef __stdcall

typedef int (*PROC)();
typedef int (*FARPROC)();
typedef int (*NEARPROC)();

#define FAILED(x)   (x < 0)

//EAPS3: Copied from malloc.h in Visual Studio.
#define _HEAPEMPTY      (-1)
#define _HEAPOK         (-2)
#define _HEAPBADBEGIN   (-3)
#define _HEAPBADNODE    (-4)
#define _HEAPEND        (-5)
#define _HEAPBADPTR     (-6)
#define _FREEENTRY      0
#define _USEDENTRY      1

// use for api's 'ignored' params for clarity
#define	XBOX_DONTCARE		0

// trap debugging output
#define OutputDebugString(v)   printf(v);

// not defined in ps3_system.cpp.
// DWORD   GetTickCount();

// this comment means nothing

#define LOWORD(l)           ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l)           ((WORD)((DWORD_PTR)(l) >> 16))
#define S_OK				((HRESULT)0x00000000L)

#ifndef _strnicmp
#define _strnicmp Q_strncasecmp
#endif

#ifndef wcsnicmp
// ???
#endif

#endif

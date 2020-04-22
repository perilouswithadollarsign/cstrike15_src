// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

// temporarily make unicode go away as we deal with Valve types
#ifdef _UNICODE
#define PUT_UNICODE_BACK 
#undef _UNICODE
#endif

#if _MANAGED
#pragma unmanaged
#undef FASTCALL
#define FASTCALL 
#endif
#include "platform.h"
#include "wchartypes.h"
#include <ctype.h>
struct datamap_t;
template <typename T> datamap_t *DataMapInit(T *);

#include "responserules/response_types.h"
#include "../../responserules/runtime/response_types_internal.h"
#include "response_system.h"


#ifdef PUT_UNICODE_BACK
#define _UNICODE 
#undef PUT_UNICODE_BACK
#endif

#if _MANAGED
/// implicitly converts a unicode CLR String^
/// to a C string. The pointer returned should
/// not be stored; it is valid only so long as
/// this class exists.
using namespace System;
class StrToAnsi
{

public:
	StrToAnsi( String ^unicodestr );
	~StrToAnsi( );
	operator TCHAR *() const;

private:
	TCHAR *m_pStr;
};
#pragma managed
#undef FASTCALL
#define FASTCALL __fastcall 
#include "cli_appsystem_thunk.h"
#include "responserules_cli.h"
#endif

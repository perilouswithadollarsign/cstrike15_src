//=========== (C) Copyright 2015 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Shares dynamic loading of COM/ole32/oleaut32 out of tier0.
//=============================================================================

#include "tier0/platform_com.h"
#include "tier0/threadtools.h"

#ifdef PLATFORM_WINDOWS

typedef int32 ThreadInitOnce_t;
typedef class CSysModule* PlatModule_t;
#define PLAT_MODULE_INVALID ( (PlatModule_t)0 )

static PlatCOMFunctions_t g_COMFunctions;
static PlatModule_t g_hCOM;
static ThreadInitOnce_t g_COMFunctionsInit;

static const char *g_COMFunctionNames[] =
{
    "CoInitializeEx",
    "CoUninitialize",
    "CoCreateInstance",
    "CoTaskMemAlloc",
    "CoTaskMemRealloc",
    "CoTaskMemFree",
};

enum PlatGetProcAddressesFlags_t
{
	k_nPlatGetProcAddresses_Default = 0x00000000,
	k_nPlatGetProcAddresses_Optional = 0x00000001,
};

// Plat_GetProcAddresses copied from Source2 platform.cpp
// Added here to avoid changing platform.h...
void *Plat_GetProcAddresses( const char *pModuleName,
	int nNames,
	const char **pNames,
	size_t nTotalPointerBytes,
	void *pPointers,
	size_t nTotalFlagsBytes,
	uint32 *pFlags,
	bool bFailFatal )
{
	if ( (nTotalPointerBytes & (sizeof( void* ) - 1)) != 0 )
	{
		Plat_FatalError( "%s: invalid pointer byte count\n", __FUNCTION__ );
	}
	if ( (size_t)nNames != nTotalPointerBytes / sizeof( void* ) )
	{
		Plat_FatalError( "%s: different number of names and pointers\n", __FUNCTION__ );
	}
	if ( (nTotalFlagsBytes & (sizeof( *pFlags ) - 1)) != 0 )
	{
		Plat_FatalError( "%s: invalid flag byte count\n", __FUNCTION__ );
	}
	if ( pFlags )
	{
		if ( (size_t)nNames != nTotalFlagsBytes / sizeof( *pFlags ) )
		{
			Plat_FatalError( "%s: different number of names and flags\n", __FUNCTION__ );
		}
	}

	HMODULE hModule = ::LoadLibrary( pModuleName );
	if ( hModule == 0 )
	{
		if ( bFailFatal )
		{
			int nErr = GetLastError();
			Plat_FatalError( "%s: unable to load '%s', error %d\n", __FUNCTION__, pModuleName, nErr );
		}

		return NULL;
	}

	void** pFunc = (void**)pPointers;
	for ( int i = 0; i < nNames; i++ )
	{
		uint32 nFlags = pFlags ? pFlags[i] : 0;
		*pFunc = ::GetProcAddress( hModule, pNames[i] );
		if ( !*pFunc &&
			(nFlags & k_nPlatGetProcAddresses_Optional) == 0 )
		{
			if ( bFailFatal )
			{
				Plat_FatalError( "%s: unable to find %s in '%s'\n", __FUNCTION__, pNames[i], pModuleName );
			}

			::FreeLibrary( hModule );
			memset( pPointers, 0, nTotalPointerBytes );
			return NULL;
		}

		pFunc++;
	}

	return hModule;
}

static bool Plat_LoadCOM_Once( ThreadInitOnce_t *pMarker )
{
    const char *pCOMName = "ole32.dll";
    g_hCOM = (PlatModule_t)Plat_GetProcAddresses( pCOMName,
                                    ARRAYSIZE( g_COMFunctionNames ),
                                    g_COMFunctionNames,
                                    sizeof( g_COMFunctions ),
                                    &g_COMFunctions,
                                    0,
                                    NULL,
                                    false );
    // We always succeed in this routine as we
    // handle failure at a higher level.
    return true;
}

PlatCOMFunctions_t *Plat_LoadCOM()
{
    ThreadInitOnceCall( &g_COMFunctionsInit, Plat_LoadCOM_Once );
    return &g_COMFunctions;
}

PlatCOMFunctions_t *Plat_CheckCOM()
{
    if ( g_hCOM == PLAT_MODULE_INVALID )
    {
        return NULL;
    }

    return &g_COMFunctions;
}

PlatCOMFunctions_t *Plat_RequireCOM()
{
    if ( g_hCOM == PLAT_MODULE_INVALID )
    {
        Plat_FatalError( "Unable to load COM\n" );
    }

    return &g_COMFunctions;
}

void Plat_UnloadCOM()
{
    // We do not unload COM currently but we have a stub
    // that callers should use so that we can drop in unloading
    // if we need it in the future.
}

static PlatOleAutFunctions_t g_OleAutFunctions;
static PlatModule_t g_hOleAut;
static ThreadInitOnce_t g_OleAutFunctionsInit;

static const char *g_OleAutFunctionNames[] =
{
    "VariantInit",
    "VariantClear",
    "VariantCopy",
    "SysAllocString",
    "SysAllocStringByteLen",
    "SysAllocStringLen",
    "SysFreeString",
    "SysReAllocString",
    "SysReAllocStringLen",
    "SysStringByteLen",
    "SysStringLen",
};

static bool Plat_LoadOleAut_Once( ThreadInitOnce_t *pMarker )
{
    const char *pOleAutName = "oleaut32.dll";
    g_hOleAut = (PlatModule_t)Plat_GetProcAddresses( pOleAutName,
                                       ARRAYSIZE( g_OleAutFunctionNames ),
                                       g_OleAutFunctionNames,
                                       sizeof( g_OleAutFunctions ),
                                       &g_OleAutFunctions,
                                       0,
                                       NULL,
                                       false );
    // We always succeed in this routine as we
    // handle failure at a higher level.
    return true;
}

PlatOleAutFunctions_t *Plat_LoadOleAut()
{
    ThreadInitOnceCall( &g_OleAutFunctionsInit, Plat_LoadOleAut_Once );
    return &g_OleAutFunctions;
}

PlatOleAutFunctions_t *Plat_CheckOleAut()
{
    if ( g_hOleAut == PLAT_MODULE_INVALID )
    {
        return NULL;
    }

    return &g_OleAutFunctions;
}

PlatOleAutFunctions_t *Plat_RequireOleAut()
{
    if ( g_hOleAut == PLAT_MODULE_INVALID )
    {
        Plat_FatalError( "Unable to load oleaut32\n" );
    }

    return &g_OleAutFunctions;
}

void Plat_UnloadOleAut()
{
    // We do not unload oleaut32 currently but we have a stub
    // that callers should use so that we can drop in unloading
    // if we need it in the future.
}

#endif // #ifdef PLATFORM_WINDOWS

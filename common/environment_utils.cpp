//========= (C) Copyright 2009-2016 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================

#include "tier0/dbg.h"
#include "environment_utils.h"

#ifdef PLATFORM_WINDOWS_PC
#include <windows.h> // for RegGetValue
#endif
#undef GetCurrentDirectory
#undef OUT
#undef ERR

#if defined( PLATFORM_WINDOWS_PC )
bool VGetRegistryKeyValue( HKEY baseKey, const char *pSubKey, const char *pValue, char *pOutBuf, int nMaxBuf )
{
	DWORD nBufSize = nMaxBuf;
	HKEY hKey = NULL;
	LONG nResult;

	nResult = RegOpenKeyEx( baseKey, pSubKey, NULL, KEY_READ, &hKey );
	if ( nResult == ERROR_SUCCESS )
	{
		nResult = RegQueryValueExA( hKey, pValue, NULL, NULL, (LPBYTE)pOutBuf, &nBufSize);
		RegCloseKey( hKey );
		return nResult == ERROR_SUCCESS;
	}

	return false;
}
#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool GetWindowsSDKDir( char *pOutBuf, int nMaxBuf )
{
#ifdef PLATFORM_WINDOWS_PC
	const char *pWindowsSDKDirRegKey = "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows";

	if( !VGetRegistryKeyValue( HKEY_LOCAL_MACHINE, pWindowsSDKDirRegKey, "CurrentInstallFolder", pOutBuf, nMaxBuf ) &&
		!VGetRegistryKeyValue( HKEY_CURRENT_USER, pWindowsSDKDirRegKey, "CurrentInstallFolder", pOutBuf, nMaxBuf ) )
	{
		Warning( "ERROR: Failed to read VS Windows SDK from registry key '%s'\n", pWindowsSDKDirRegKey );
		return false;
	}

	return true;
#else
	return false;
#endif
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool GetMSVCIncludePaths( CUtlVector<CUtlString> &paths, const char *pVSToolsEnv )
{
	const char *pVSToolsPath = Plat_GetEnv( pVSToolsEnv );
	if ( !pVSToolsPath )
	{
		Warning( "ERROR: GetMSVCIncludePaths failed to read VS location from environment variable '%s'\n", pVSToolsEnv );
		return false;
	}

	char pWindowsSDKDir[MAX_PATH];
	if ( !GetWindowsSDKDir( pWindowsSDKDir, sizeof(pWindowsSDKDir) ) )
	{
		return false;
	}

	char pVSRootPath[MAX_PATH];
	V_MakeAbsolutePath( pVSRootPath, sizeof(pVSRootPath), "../../", pVSToolsPath );

	char pIncludePath[MAX_PATH];
	V_MakeAbsolutePath( pIncludePath, sizeof(pIncludePath), "VC/INCLUDE", pVSRootPath );
	paths.AddToTail( pIncludePath );
	V_MakeAbsolutePath( pIncludePath, sizeof(pIncludePath), "VC/ATLMFC/INCLUDE", pVSRootPath );
	paths.AddToTail( pIncludePath );
	V_MakeAbsolutePath( pIncludePath, sizeof(pIncludePath), "INCLUDE", pWindowsSDKDir );
	paths.AddToTail( pIncludePath );

	return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool GetX360IncludePaths( CUtlVector<CUtlString> &paths )
{
	const char *pXdkPathEnv = "XEDK";
	const char *pXdkPath = Plat_GetEnv( pXdkPathEnv );
	if ( !pXdkPath )
	{
		Warning( "ERROR: GetX360IncludePaths failed to read XDK location from environment variable '%s'\n", pXdkPath );
		return false;
	}

	char pIncludePath[MAX_PATH];
	V_MakeAbsolutePath( pIncludePath, sizeof(pIncludePath), "include/xbox", pXdkPath );
	paths.AddToTail( pIncludePath );
	V_MakeAbsolutePath( pIncludePath, sizeof(pIncludePath), "include/win32", pXdkPath );
	paths.AddToTail( pIncludePath );

	return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool GetOSXIncludePaths( CUtlVector<CUtlString> &paths )
{
	const char *pPath = Plat_GetEnv( "SDKROOT" );
	if ( !pPath )
	{
		pPath = Plat_GetEnv( "OSX_SDK_PATH" );
		if ( !pPath )
		{
			Warning( "ERROR: %s failed to read SDK location from environment variable 'SDKROOT' or 'OSX_SDK_PATH'\n",
					__FUNCTION__ );
			return false;
		}
	}

	char pIncludePath[MAX_PATH];
	V_MakeAbsolutePath( pIncludePath, sizeof(pIncludePath), "usr/include", pPath, false );
	paths.AddToTail( pIncludePath );

	return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool GetSystemIncludePaths( CUtlVector<CUtlString> &paths, const char *pPlatform, const char *pCompiler )
{
#if !defined( PLATFORM_WINDOWS ) && !defined( PLATFORM_POSIX ) && !defined( PLATFORM_OSX )
	Warning( "ERROR: GetSystemIncludePaths not implemented for this platform!\n" );
	return false;
#endif

	if ( !V_stricmp_fast( pPlatform, "WIN32" ) || !V_stricmp_fast( pPlatform, "WIN64" ) )
	{
		if ( !V_stricmp_fast( pCompiler, "VS2005" ) )
		{
			return GetMSVCIncludePaths( paths, "VS80COMNTOOLS" );
		}
		else if ( !V_stricmp_fast( pCompiler, "VS2010" ) )
		{
			return GetMSVCIncludePaths( paths, "VS100COMNTOOLS" );
		}
		else if ( !V_stricmp_fast( pCompiler, "VS2012" ) )
		{
			return GetMSVCIncludePaths( paths, "VS110COMNTOOLS" );
		}
		else if ( !V_stricmp_fast( pCompiler, "VS2013" ) )
		{
			return GetMSVCIncludePaths( paths, "VS120COMNTOOLS" );
		}
		else if ( !V_stricmp_fast( pCompiler, "VS2015" ) )
		{
			return GetMSVCIncludePaths( paths, "VS140COMNTOOLS" );
		}
		AssertMsg1( false, "ERROR: GetSystemIncludePaths not implemented for this compiler yet! (%s)\n", pCompiler );
		return false;
	}
	else if ( !V_stricmp_fast( pPlatform, "X360" ) )
	{
		return GetX360IncludePaths( paths );
	}
    else if ( !V_stricmp_fast( pPlatform, "LINUXSTEAMRT64" ) ||
              !V_stricmp_fast( pPlatform, "LINUXSERVER64" ) )
    {
        // The Steam runtime tool.sh script will rewrite /usr/include
        // to whatever is appropriate for runtime compiles, so
        // we can just blindly use it here and it will work both
        // for runtime and non-runtime compiles.
        paths.AddToTail( "/usr/include" );
        return true;
    }
	else if ( !V_stricmp_fast( pPlatform, "OSX32" ) ||
              !V_stricmp_fast( pPlatform, "OSX64" ) )
	{
		return GetOSXIncludePaths( paths );
	}
    
	AssertMsg1( false, "ERROR: GetSystemIncludePaths not implemented for this platform yet! (%s)\n", pPlatform );
	return false;
}

//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// Purpose: Video configuration management (split out from tier2)
//
//===============================================================================

#include "tier2/fileutils.h"
#include "tier2/tier2.h"
#include "videocfg.h"
#include "tier1/tier1.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "tier1/keyvalues.h"
#include "utlbuffer.h"
#include "mathlib/IceKey.H"
#include "tier0/icommandline.h"


// Video Config Filenames
#ifdef POSIX
#define VIDEOCONFIG_DEFAULT_FILENAME	"cfg/videodefaults.txt"
#define VIDEOCONFIG_FILENAME			"cfg/video.txt"
#define VIDEOCONFIG_FILENAME_BACKUP		"cfg/video.bak"
#else
#define VIDEOCONFIG_DEFAULT_FILENAME	"cfg\\videodefaults.txt"
#define VIDEOCONFIG_FILENAME			"cfg\\video.txt"
#define VIDEOCONFIG_FILENAME_BACKUP		"cfg\\video.bak"
#endif

#define VIDEOCONFIG_PATHID				"USRLOCAL"

static void WriteVideoCfgDataToFile( char const *pszFile, CUtlBuffer &buf )
{
	// Video defaults and video settings detection runs very early, so we need to create cfg path in USRLOCAL storage
	g_pFullFileSystem->CreateDirHierarchy( "cfg", "USRLOCAL" );
	g_pFullFileSystem->WriteFile( pszFile, VIDEOCONFIG_PATHID, buf );
}

enum AspectRatioMode_t
{
	ASPECT_RATIO_OTHER = -1,
	ASPECT_RATIO_4x3 = 0,
	ASPECT_RATIO_16x9,
	ASPECT_RATIO_16x10,
};

struct VideoConfigSetting_t
{
	const char *m_pSettingVar;
	bool		m_bChooseLower;
	bool		m_bSaved;
	bool		m_bConVar;
	bool		m_bUseAutoOption;
};

struct RatioToAspectMode_t
{
	AspectRatioMode_t m_Mode;
	int m_nWidth;
	int m_nHeight;
};

static VideoConfigSetting_t s_pVideoConfigSettingsWhitelist[] =
{
	// ConVars.
	{ "setting.cpu_level",										true,		true,		true,	true },				
	{ "setting.gpu_level",										true,		true,		true,	true },					
	{ "setting.mat_antialias",									true,		true,		true,	true },				
	{ "setting.mat_aaquality",									true,		true,		true,	true },				
	{ "setting.mat_forceaniso",									true,		true,		true,	true },				
	{ "setting.mat_vsync",										true,		true,		true },
	{ "setting.mat_triplebuffered",								true,		true,		true },
	{ "setting.mat_grain_scale_override",						true,		true,		true },
//	{ "setting.mat_monitorgamma",								true,		true,		true },
	{ "setting.gpu_mem_level",									true,		true,		true,	true },
	{ "setting.mem_level",										true,		true,		true },
	{ "setting.videoconfig_version",							true,		true,		true },
	{ "setting.mat_queue_mode",									true,		true,		true },
	{ "setting.mat_tonemapping_occlusion_use_stencil",			false,		false,		true },
	{ "setting.csm_quality_level",								true,		true,		true,	true },
	{ "setting.mat_software_aa_strength",						true,		true,		true },
	{ "setting.mat_motion_blur_enabled",						true,		true,		true },

	// Settings.
	{ "setting.fullscreen",										true,		true,		false },
	{ "setting.nowindowborder",									true,		true,		false },
	{ "setting.aspectratiomode",								true,		true,		false },
	{ "setting.defaultres",										true,		true,		false },							
	{ "setting.defaultresheight",								true,		true,		false },							
	{ "setting.dxlevel",										true,		false,		false },							
	{ "setting.mindxlevel",										true,		false,		false },							
	{ "setting.maxdxlevel",										true,		false,		false },							
	{ "setting.preferhardwaresync",								true,		false,		false },
	{ "setting.centroidhack",									true,		false,		false },							
	{ "setting.preferzprepass",									true,		false,		false },							
	{ "setting.prefertexturesinhwmemory",						true,		false,		false },							
	{ "setting.laptop",											true,		false,		false },							
	{ "setting.suppresspixelshadercentroidhackfixup",			true,		false,		false },							
	{ "setting.nouserclipplanes",								true,		false,		false },							
	{ "setting.unsupported",									true,		false,		false },							
};
static VideoConfigSetting_t const * VideoConfigSettingFindWhitelistEntryByName( char const *szSettingName )
{
	for ( int k = 0; k < Q_ARRAYSIZE( s_pVideoConfigSettingsWhitelist ); ++ k )
	{
		if ( !V_stricmp( szSettingName, s_pVideoConfigSettingsWhitelist[k].m_pSettingVar ) )
			return &s_pVideoConfigSettingsWhitelist[k];
	}
	return NULL;
}

static RatioToAspectMode_t g_pRatioToAspectModes[] =
{
	{ ASPECT_RATIO_4x3,		4,  3 },
	{ ASPECT_RATIO_16x9,	16, 9 },
	{ ASPECT_RATIO_16x10,	16, 10 },
};


//--------------------------------------------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
static AspectRatioMode_t GetScreenAspectMode( int width, int height )
{
	for (int i = 0; i < ARRAYSIZE(g_pRatioToAspectModes); i++)
	{
		int nFactor = width / g_pRatioToAspectModes[i].m_nWidth;
		if ( nFactor * g_pRatioToAspectModes[i].m_nHeight == height )
			return g_pRatioToAspectModes[i].m_Mode;
	}

	return ASPECT_RATIO_OTHER;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
static inline int ReadHexValue( KeyValues *pVal, const char *pName )
{
	const char *pString = pVal->GetString( pName, NULL );
	if (!pString)
	{
		return -1;
	}

	char *pTemp;
	int nVal = strtol( pString, &pTemp, 16 );
	return (pTemp != pString) ? nVal : -1;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
void AddNewKeysUsingVideoWhitelist( KeyValues *pModKeys, KeyValues *pConfigKeys )
{
	for( KeyValues *pSubKey = pModKeys->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
	{
		const char *pSubKeyName = pSubKey->GetName();

		// Find all the "setting" keys that are white-listed and add them to the 
		// Video Config keys.

		int iVar;
		bool bWhiteListedVar = false;
		int nVideoConfigCount = ARRAYSIZE( s_pVideoConfigSettingsWhitelist );
		for ( iVar = 0; iVar < nVideoConfigCount; ++iVar )
		{
			if ( !V_stricmp( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar, pSubKeyName ) )
			{
				bWhiteListedVar = true;
				break;
			}
		}

		// This is not a valid key.
		if ( !bWhiteListedVar )
			continue;

		// See if the key already exists - if it doesn't add it.
		if ( pConfigKeys->FindKey( pSubKeyName ) )
			continue;

		const char *pValue = pSubKey->GetString();
		pConfigKeys->SetString( pSubKeyName, pValue );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
void CopySettingKeysUsingVideoWhitelist( KeyValues *pModKeys, KeyValues *pConfigKeys )
{
	for( KeyValues *pSubKey = pModKeys->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
	{
		const char *pSubKeyName = pSubKey->GetName();

		// Find all the "setting" keys that are white-listed and add them to the 
		// Video Config keys.

		int iVar;
		bool bWhiteListedVar = false;
		int nVideoConfigCount = ARRAYSIZE( s_pVideoConfigSettingsWhitelist );
		for ( iVar = 0; iVar < nVideoConfigCount; ++iVar )
		{
			if ( !V_stricmp( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar, pSubKeyName ) )
			{
				bWhiteListedVar = true;
				break;
			}
		}

		// This is not a valid key.
		if ( !bWhiteListedVar )
			continue;

		if ( pConfigKeys->FindKey( pSubKeyName ) )
		{
			float flOldValue = pConfigKeys->GetFloat( pSubKeyName );
			float flNewValue = pSubKey->GetFloat();
			if ( s_pVideoConfigSettingsWhitelist[iVar].m_bChooseLower )
			{
				if ( flNewValue >= flOldValue )
					continue;
			}
			else
			{
				if ( flNewValue <= flOldValue )
					continue;
			}
		}

		const char *pValue = pSubKey->GetString();
		pConfigKeys->SetString( pSubKeyName, pValue );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Match the CPU data and add all the "setting" data to the Video Config Keys.
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
void AddCPULevelKeys( KeyValues *pModKeys, KeyValues *pVideoConfigKeys )
{
	// Get the number of physical processors in the machine.
	const CPUInformation &cpuInfo = GetCPUInformation();
	int nProcessorCount = cpuInfo.m_nPhysicalProcessors;

	// Test all "cpu_level_*" blocks to determine the correct block to copy data from.
	for( KeyValues *pModKey = pModKeys->GetFirstSubKey(); pModKey; pModKey = pModKey->GetNextKey() )
	{
		KeyValues *pMinKey = pModKey->FindKey( "min_processor_count" );
		KeyValues *pMaxKey = pModKey->FindKey( "max_processor_count" );
		if ( pMinKey && pMaxKey )
		{
			int nMin = pMinKey->GetInt();
			int nMax = pMaxKey->GetInt();

			// Is this the correct cpu_level setting.
			if ( nMin <= nProcessorCount && nProcessorCount <= nMax )
			{
				CopySettingKeysUsingVideoWhitelist( pModKey, pVideoConfigKeys );
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
void AddMemoryKeys( KeyValues *pModKeys, int nMemory, KeyValues *pVideoConfigKeys )
{
	for( KeyValues *pModKey = pModKeys->GetFirstSubKey(); pModKey; pModKey = pModKey->GetNextKey() )
	{
		KeyValues *pMinMegabytes = pModKey->FindKey( "min megabytes" );
		KeyValues *pMaxMegabytes = pModKey->FindKey( "max megabytes" );
		if ( pMinMegabytes && pMaxMegabytes )
		{
			int nMin = pMinMegabytes->GetInt();
			int nMax = pMaxMegabytes->GetInt();

			// Is this the correct cpu_level setting.
			if ( nMin <= nMemory && nMemory <= nMax )
			{
				CopySettingKeysUsingVideoWhitelist( pModKey, pVideoConfigKeys );
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
void AddVideoMemoryKeys( KeyValues *pModKeys, int nVidMemory, KeyValues *pVideoConfigKeys )
{
	for( KeyValues *pModKey = pModKeys->GetFirstSubKey(); pModKey; pModKey = pModKey->GetNextKey() )
	{
		KeyValues *pMinMegaTexels = pModKey->FindKey( "min megatexels" );
		KeyValues *pMaxMegaTexels = pModKey->FindKey( "max megatexels" );
		if ( pMinMegaTexels && pMaxMegaTexels )
		{
			int nMin = pMinMegaTexels->GetInt();
			int nMax = pMaxMegaTexels->GetInt();

			// Is this the correct cpu_level setting.
			if ( nMin <= nVidMemory && nVidMemory <= nMax )
			{
				CopySettingKeysUsingVideoWhitelist( pModKey, pVideoConfigKeys );
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Match the device and vendor id and add all the "setting" data to
//	the Video Config Keys.
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool AddVideoCardKeys( KeyValues *pModKeys, int nVendorID, int nDeviceID, KeyValues *pVideoConfigKeys )
{
	bool bFoundDevice = false;

	// Test all video card blocks to determine the correct blocks to copy data from.
	for( KeyValues *pModKey = pModKeys->GetFirstSubKey(); pModKey; pModKey = pModKey->GetNextKey() )
	{
		// Get and match the vendor and device id.
		int iVender = ReadHexValue( pModKey, "vendorid" );
		if ( iVender == -1 )
			continue;

		int iDeviceMin = ReadHexValue( pModKey, "mindeviceid" );
		int iDeviceMax = ReadHexValue( pModKey, "maxdeviceid" );
		if ( iDeviceMin == -1 || iDeviceMax == -1 )
			continue;

		// Only initialize with unknown data if we didn't find the actual card.
		bool bUnknownDevice = ( pModKey->FindKey( "makemelast" ) != NULL );

		// Fixed for CS:GO - Don't apply this node's keys at all unless the node's vendor ID matches (for example, some NVidia device ID's, such as the 6800's, alias a few Intel ID's).
		if ( ( iDeviceMin <= nDeviceID ) && ( nDeviceID <= iDeviceMax ) && ( nVendorID == iVender ) )
		{
			if ( !bUnknownDevice )
			{
				CopySettingKeysUsingVideoWhitelist( pModKey, pVideoConfigKeys );
				bFoundDevice = true;
			}
			else
			{
				AddNewKeysUsingVideoWhitelist( pModKey, pVideoConfigKeys );
			}
		}			
	}

	return bFoundDevice;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Match the device and vendor id and add all the "setting" data to
//	the Video Config Keys.
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
void AddDXLevelKeys( KeyValues *pModKeys, int nDXLevel, KeyValues *pVideoConfigKeys )
{
	// Test all video card blocks to determine the correct blocks to copy data from.
	for( KeyValues *pModKey = pModKeys->GetFirstSubKey(); pModKey; pModKey = pModKey->GetNextKey() )
	{
		KeyValues *pDXLevelKey = pModKey->FindKey( "setting.maxdxlevel" );
		if ( !pDXLevelKey )
			continue;

		if ( pDXLevelKey->GetInt() == nDXLevel )
		{
			AddNewKeysUsingVideoWhitelist( pModKey, pVideoConfigKeys );
		}			
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool ParseConfigKeys( VidMatConfigData_t &configData )
{
	// Does the file exist?
	bool bFileExists = g_pFullFileSystem->FileExists( configData.szFileName, configData.szPathID );
	if ( !bFileExists )
		return false;

	// Get the key values in the file.
	KeyValues *pFileKeys = new KeyValues( "FileKeys" );
	if ( !pFileKeys )
		return false;

	if ( !pFileKeys->LoadFromFile( g_pFullFileSystem, configData.szFileName, configData.szPathID ) )
	{
		pFileKeys->deleteThis();
		return false;
	}

#ifdef _DEBUG
	Msg( "VIDEOCFG.ParseConfigKeys: (start)\n" );
	KeyValuesDumpAsDevMsg( configData.pConfigKeys, 0, 0 );
#endif

	// Add Config Keys based on cpu.
	AddCPULevelKeys( pFileKeys, configData.pConfigKeys );
#ifdef _DEBUG
	Msg( "VIDEOCFG.ParseConfigKeys: (+cpu_level)\n" );
	KeyValuesDumpAsDevMsg( configData.pConfigKeys, 0, 0 );
#endif

	// Add Config Keys based on memory.
	AddMemoryKeys( pFileKeys, configData.nSystemMemory, configData.pConfigKeys );
#ifdef _DEBUG
	Msg( "VIDEOCFG.ParseConfigKeys: (+memory)\n" );
	KeyValuesDumpAsDevMsg( configData.pConfigKeys, 0, 0 );
#endif

	// Add Config Keys based on video memory.
	AddVideoMemoryKeys( pFileKeys, configData.nVideoMemory, configData.pConfigKeys );
#ifdef _DEBUG
	Msg( "VIDEOCFG.ParseConfigKeys: (+video memory)\n" );
	KeyValuesDumpAsDevMsg( configData.pConfigKeys, 0, 0 );
#endif

	// Add Config Keys based on video card.
	bool bFoundDevice = AddVideoCardKeys( pFileKeys, configData.nVendorID, configData.nDeviceID, configData.pConfigKeys );
#ifdef _DEBUG
	Msg( "VIDEOCFG.ParseConfigKeys: (+video card)\n" );
	KeyValuesDumpAsDevMsg( configData.pConfigKeys, 0, 0 );
#endif

	// Add Config Keys based on DXLevel.
	if ( !bFoundDevice )
	{
		//
		// This actually doesn't work for CS:GO the way moddefaults.txt is setup
		//
		AddDXLevelKeys( pFileKeys, configData.nDXLevel, configData.pConfigKeys );
	}

	// Destroy the file keys.
	pFileKeys->deleteThis();

	return true;
}
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
static bool GetNearestFullscreenResolution( int& nWidth, int& nHeight, VidMatConfigData_t &configData )
{
	float flDesiredArea = nWidth * nHeight;
	int nBestWidth = 0;
	int nBestHeight = 0;
	float flBestDelta = FLT_MAX;

	AspectRatioMode_t nRequiredAspectMode = GetScreenAspectMode( configData.nPhysicalScreenWidth, configData.nPhysicalScreenHeight );

	// Iterate modes, looking for one that is just smaller than currentWidth and currentHeight while retaining the aspect ratio
	while ( true )
	{
		for ( int i = 0; i < configData.displayModes.Count(); i++ )
		{
			const ShaderDisplayMode_t &mode = configData.displayModes[i];

			if ( nRequiredAspectMode != ASPECT_RATIO_OTHER )
			{
				AspectRatioMode_t nAspectMode = GetScreenAspectMode( mode.m_nWidth, mode.m_nHeight );
				if ( nRequiredAspectMode != nAspectMode )
					continue;
			}

			float flArea = mode.m_nWidth * mode.m_nHeight;
			if ( flArea > flDesiredArea * 1.35f )
				continue;

			float flDelta = fabs( flDesiredArea - flArea );
			if ( flDelta >= flBestDelta )
				continue;

			flBestDelta = flDelta;
			nBestWidth = mode.m_nWidth;
			nBestHeight = mode.m_nHeight;
		}

		if ( nBestWidth != 0 )
			break;

		if ( nRequiredAspectMode == ASPECT_RATIO_OTHER )
			return false;

		nRequiredAspectMode = ASPECT_RATIO_OTHER;
	}

	nWidth = nBestWidth;
	nHeight = nBestHeight;
	return true;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Create the video config file and fill it in with default data.
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool CreateDefaultVideoKeyValues( VidMatConfigData_t &configData )
{
	// Sets the highest CSM quality level and enabled FXAA, then let the csm_quality_level settings in moddefaults override these settings with lower values.
	configData.pConfigKeys->SetInt( "setting.csm_quality_level", CSMQUALITY_HIGH ); //g_pHardwareConfig->GetCSMQuality() );
	configData.pConfigKeys->SetInt( "setting.mat_software_aa_strength", 1 );

	// Set the vendor and device id key values.
	configData.pConfigKeys->SetInt( "VendorID", configData.nVendorID );
	configData.pConfigKeys->SetInt( "DeviceID", configData.nDeviceID );

	// Initial config always assume v-sync, normal aspect ratio, and fullscreen.
	configData.pConfigKeys->SetInt( "setting.fullscreen", 1 );
	configData.pConfigKeys->SetInt( "setting.nowindowborder", 0 );
	configData.pConfigKeys->SetInt( "setting.aspectratiomode", GetScreenAspectMode( configData.nPhysicalScreenWidth, configData.nPhysicalScreenHeight ) );
	configData.pConfigKeys->SetInt( "setting.mat_vsync", IsGameConsole() ? 1 : 0 );
	configData.pConfigKeys->SetInt( "setting.mat_triplebuffered", 0 );
	configData.pConfigKeys->SetFloat( "setting.mat_monitorgamma", 2.2f );
	configData.pConfigKeys->SetInt( "setting.mat_queue_mode", -1 );
	// Before, mat_motion_blur_enabled came from gpu_level.csv (it was set to 0 at GPU levels 0 and 1, and 1 at higher levels).
	// These GPU levels are not that useful in CS:GO any longer (because we've dumped a bunch of old cards not capable of at least shader model 3), and
	// it's now a video option, so we need a better way of defaulting mat_motion_blur_enabled in fresh installs. For now, the safest choice on all cards is 
	// disabled.
	configData.pConfigKeys->SetInt( "setting.mat_motion_blur_enabled", 0 );
	// Not all video card DXSupport configs define GPU mem level, default it to High
	#if defined( DX_TO_GL_ABSTRACTION )
	configData.pConfigKeys->SetInt( "setting.gpu_mem_level", 3 );
	#else
	configData.pConfigKeys->SetInt( "setting.gpu_mem_level", 2 );
	#endif
	
	// Assume if we don't find a GPU level then it's a high level recent GPU
	configData.pConfigKeys->SetInt( "setting.gpu_level", 3 );

	// Assume if we don't find a lower mat_antialias value then we will run at the highest support MSAA setting
	
#if defined( DX_TO_GL_ABSTRACTION )

	configData.pConfigKeys->SetInt( "setting.mat_antialias", 0 );

#else
	if ( materials->SupportsMSAAMode( 8 ) )
		configData.pConfigKeys->SetInt( "setting.mat_antialias", 8 );
	else if ( materials->SupportsMSAAMode( 4 ) )
		configData.pConfigKeys->SetInt( "setting.mat_antialias", 4 );
	else if ( materials->SupportsMSAAMode( 2 ) )
		configData.pConfigKeys->SetInt( "setting.mat_antialias", 2 );
	else
		configData.pConfigKeys->SetInt( "setting.mat_antialias", 0 );
#endif

	// None of our moddefaults.txt specify CSAA settings, so default quality to zero
	configData.pConfigKeys->SetInt( "setting.mat_aaquality", 0 );

	// Get the key value you data from the defined file.
	if ( !ParseConfigKeys( configData ) )
		return false;

	// LEGACY: mat_antialias = 1 - if such configuration is found just set it to zero (CS:GO video options interpret NONE that way)
	if ( configData.pConfigKeys->GetInt( "setting.mat_antialias", 0 ) == 1 )
		configData.pConfigKeys->SetInt( "setting.mat_antialias", 0 );

	// Set the default resolution based on the aspect ratio mode 
// 	int nWidth = configData.pConfigKeys->GetInt( "setting.defaultres" );
// 	int nHeight = configData.pConfigKeys->GetInt( "setting.defaultresheight" );
	// Ignore moddefaults configuration that might be outdated, just default to user's desktop resolution
	int nWidth = configData.nPhysicalScreenWidth;
	int nHeight = configData.nPhysicalScreenHeight;
	if ( GetNearestFullscreenResolution( nWidth, nHeight, configData ) )
	{
		configData.pConfigKeys->SetInt( "setting.defaultres", nWidth );
		configData.pConfigKeys->SetInt( "setting.defaultresheight", nHeight );
	}
	
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
static bool VerifyVideoConfigSettingRequired( char const *szSetting )
{
	static char const * arrIgnoredSettings[] = { "setting.mem_level" };
	for ( int j = 0; j < Q_ARRAYSIZE( arrIgnoredSettings ); ++ j )
	{
		if ( !V_stricmp( arrIgnoredSettings[j], szSetting ) )
			return false;
	}
	return true;
}
bool VerifyDefaultVideoConfig( VidMatConfigData_t &configData )
{
	// Make sure the file exists to verify.
	bool bFileExists = g_pFullFileSystem->FileExists( VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_PATHID );
	if ( !bFileExists )
		return false;

	// Open the moddefaults.cfg file and load it into key values.
	KeyValues *pDefaultKeys = new KeyValues( "DefaultKeys" );
	if ( !pDefaultKeys )
		return false;

	if ( !pDefaultKeys->LoadFromFile( g_pFullFileSystem, VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_PATHID ) )
	{
		pDefaultKeys->deleteThis();
		return false;
	}

	// Derive the default state from dxsupport.cfg and moddefaults.txt.
	if ( !CreateDefaultVideoKeyValues( configData ) )
	{
		pDefaultKeys->deleteThis();
		return false;
	}

	// Diagnostic buffer
	CUtlBuffer bufDiagnostic( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// Start with the assumption they are the same.
	bool bEqual = true;
	for( KeyValues *pTestKey = configData.pConfigKeys->GetFirstSubKey(); pTestKey; pTestKey = pTestKey->GetNextKey() )
	{
		const char *pszTestName = pTestKey->GetName();
		KeyValues *pFindKey = pDefaultKeys->FindKey( pszTestName );
		if ( !pFindKey )
		{
			Warning( "The default video config has changed, config key '%s=%s' is no longer default.\n", pszTestName, pTestKey->GetString() );
			bufDiagnostic.Printf( "config key '%s=%s' is no longer default.\n", pszTestName, pTestKey->GetString() );
			if ( VerifyVideoConfigSettingRequired( pszTestName ) )
				bEqual = false;
		}
		else if ( V_stricmp( pFindKey->GetString(), pTestKey->GetString() ) )
		{
			Warning( "The default video config has changed, config key '%s=%s' is no longer default '%s'.\n", pszTestName, pTestKey->GetString(), pFindKey->GetString() );
			bufDiagnostic.Printf( "config key '%s=%s' is no longer default '%s'.\n", pszTestName, pTestKey->GetString(), pFindKey->GetString() );
			if ( VerifyVideoConfigSettingRequired( pszTestName ) )
				bEqual = false;
		}
	}

	// If we are still equal - test to see if the default file has any keys that have been removed by the default config.
	if ( bEqual )
	{
		for( KeyValues *pTestKey = pDefaultKeys->GetFirstSubKey(); pTestKey; pTestKey = pTestKey->GetNextKey() )
		{
			const char *pszTestName = pTestKey->GetName();
			KeyValues *pFindKey = configData.pConfigKeys->FindKey( pszTestName );
			if ( !pFindKey )
			{
				Warning( "The default video config has changed, config key '%s=%s' has been added.\n", pszTestName, pTestKey->GetString() );
				bufDiagnostic.Printf( "config key '%s=%s' has been added.\n", pszTestName, pTestKey->GetString() );
				if ( VerifyVideoConfigSettingRequired( pszTestName ) )
					bEqual = false;
			}
		}
	}

	// If we are not equal, put up some warning and reset the current video config file.
	if ( !bEqual )
	{
		Warning( "VerifyDefaultVideoConfig: The default video config for the machine has changed, updating the current config to match.\n" );

		bufDiagnostic.Printf( "--ConfigData--\n" );
		configData.pConfigKeys->RecursiveSaveToFile( bufDiagnostic, 0 );
		bufDiagnostic.Printf( "--Defaults--\n" );
		pDefaultKeys->RecursiveSaveToFile( bufDiagnostic, 0 );
		bufDiagnostic.Printf( "----\n" );
	
		char chBuffer[64] = {};
		struct tm tmNow;
		Plat_GetLocalTime( &tmNow );
		V_sprintf_safe( chBuffer, "cfg\\video.change%u.txt", Plat_timegm( &tmNow ) );
		WriteVideoCfgDataToFile( chBuffer, bufDiagnostic );

		// Create the file.
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		configData.pConfigKeys->RecursiveSaveToFile( buf, 0 );
		WriteVideoCfgDataToFile( VIDEOCONFIG_DEFAULT_FILENAME, buf );

		//
		// Upgrade user's video preferences with new default autoconfig
		//
		KeyValues *kvPreviousVideoCfg = new KeyValues( "videocfg" );
		if ( kvPreviousVideoCfg->LoadFromFile( g_pFullFileSystem, VIDEOCONFIG_FILENAME, VIDEOCONFIG_PATHID ) )
		{
			// Preserve all settings that support auto, but user had on a custom setting
			for ( int k = 0; k < Q_ARRAYSIZE( s_pVideoConfigSettingsWhitelist ); ++ k )
			{
				if ( !s_pVideoConfigSettingsWhitelist[k].m_bUseAutoOption ) continue;
				int nUserPreferenceValue = kvPreviousVideoCfg->GetInt( s_pVideoConfigSettingsWhitelist[k].m_pSettingVar, 9999999 );
				if ( nUserPreferenceValue != 9999999 )
				{
					// User had explicit value, so store same explicit value in new video config
					configData.pConfigKeys->SetInt( s_pVideoConfigSettingsWhitelist[ k ].m_pSettingVar, nUserPreferenceValue );
				}
				else
				{
					// User had auto configured value or no value at all, if the new default config has a value then
					// make sure it is set on auto here
					if ( KeyValues *kvSubKey = configData.pConfigKeys->FindKey( s_pVideoConfigSettingsWhitelist[ k ].m_pSettingVar ) )
					{
						char chNewAutoName[ 128 ];
						// e.g.: "setting.cpu_level" -> "setauto.cpu_level"
						V_sprintf_safe( chNewAutoName, "setauto.%s", s_pVideoConfigSettingsWhitelist[ k ].m_pSettingVar + 8 );
						kvSubKey->SetName( chNewAutoName );
					}
				}
			}
		}
		else
		{
			FOR_EACH_SUBKEY( configData.pConfigKeys, kvSubKey )
			{
				if ( VideoConfigSetting_t const *pSetting = VideoConfigSettingFindWhitelistEntryByName( kvSubKey->GetName() ) )
				{
					if ( pSetting->m_bUseAutoOption )
					{
						char chNewAutoName[ 128 ];
						// e.g.: "setting.cpu_level" -> "setauto.cpu_level"
						V_sprintf_safe( chNewAutoName, "setauto.%s", pSetting->m_pSettingVar + 8 );
						kvSubKey->SetName( chNewAutoName );
					}
				}
			}
		}
		kvPreviousVideoCfg->deleteThis();
		buf.Purge();
		configData.pConfigKeys->RecursiveSaveToFile( buf, 0 );
		WriteVideoCfgDataToFile( VIDEOCONFIG_FILENAME, buf );
	}

	// Destroy keys.
	configData.pConfigKeys->Clear();
	pDefaultKeys->deleteThis();

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool CopyDefaultVideoToCurrentVideoConfig( const char *pszDefaultFileName, const char *pszCurrentFileName )
{
	bool bFileExists = g_pFullFileSystem->FileExists( pszDefaultFileName, VIDEOCONFIG_PATHID );
	if ( !bFileExists )
		return false;

	// Open the moddefaults.cfg file and load it into key values.
	KeyValues *pDefaultKeys = new KeyValues( "DefaultKeys" );
	if ( !pDefaultKeys )
		return false;

	if ( !pDefaultKeys->LoadFromFile( g_pFullFileSystem, pszDefaultFileName, VIDEOCONFIG_PATHID ) )
	{
		pDefaultKeys->deleteThis();
		return false;
	}

	// Create the file.
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	pDefaultKeys->RecursiveSaveToFile( buf, 0 );
	WriteVideoCfgDataToFile( pszCurrentFileName, buf );

	// Destroy the keys.
	pDefaultKeys->deleteThis();

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool CreateDefaultVideoConfig( VidMatConfigData_t &configData )
{
	// Create the default video keys.
	if ( !CreateDefaultVideoKeyValues( configData ) )
		return false;

	// Create the file with default settings
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	configData.pConfigKeys->RecursiveSaveToFile( buf, 0 );
	WriteVideoCfgDataToFile( VIDEOCONFIG_DEFAULT_FILENAME, buf );

	//
	// Now write the file with user settings skipping the settings that support "AUTO" in options
	//
	FOR_EACH_SUBKEY( configData.pConfigKeys, kvSubKey )
	{
		if ( VideoConfigSetting_t const *pSetting = VideoConfigSettingFindWhitelistEntryByName( kvSubKey->GetName() ) )
		{
			if ( pSetting->m_bUseAutoOption )
			{
				char chNewAutoName[128];
				// e.g.: "setting.cpu_level" -> "setauto.cpu_level"
				V_sprintf_safe( chNewAutoName, "setauto.%s", pSetting->m_pSettingVar + 8 );
				kvSubKey->SetName( chNewAutoName );
			}
		}
	}
	buf.Purge();
	configData.pConfigKeys->RecursiveSaveToFile( buf, 0 );
	WriteVideoCfgDataToFile( VIDEOCONFIG_FILENAME, buf );

	// Clear out the data after writing the file.
	configData.pConfigKeys->Clear();

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Look for a video config file to setup the system defaults.  Create the
//   file if one doesn't already exist.
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool BLoadUserVideoConfigFileFromDisk( KeyValues *pConfigKeys )
{
	// Parse current video file, note that some settings will be on AUTO with setauto
	// their values are ensured by code above to match the default autoconfig values
	if ( !pConfigKeys->LoadFromFile( g_pFullFileSystem, VIDEOCONFIG_FILENAME, VIDEOCONFIG_PATHID ) )
		return false;

	// load the default config as well to expand setauto. fields
	KeyValues *kvDefaultSettings = new KeyValues( "default" );
	if ( !kvDefaultSettings->LoadFromFile( g_pFullFileSystem, VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_PATHID ) )
	{
		kvDefaultSettings->deleteThis();
		kvDefaultSettings = NULL;
	}

	// bloat the AUTO detected settings for compatibility as proper 'setting.' fields
	FOR_EACH_SUBKEY( pConfigKeys, kvSubKey )
	{
		if ( char const *szSetting = StringAfterPrefix( kvSubKey->GetName(), "setauto." ) )
		{
			char chNewAutoName[ 128 ];
			// e.g.: "setauto.cpu_level" -> "setting.cpu_level"
			V_sprintf_safe( chNewAutoName, "setting.%s", szSetting );
			if ( KeyValues *kvValueDefault = kvDefaultSettings->FindKey( chNewAutoName ) )
			{
				pConfigKeys->AddSubKey( kvValueDefault->MakeCopy() );
			}
		}
	}

	if ( kvDefaultSettings )
	{
		kvDefaultSettings->deleteThis();
		kvDefaultSettings = NULL;
	}

	return true;
}
bool RecommendedVideoConfig( VidMatConfigData_t &configData )
{
	// Get the default video config file if it exists, create it otherwise.
	bool bFileExists = g_pFullFileSystem->FileExists( VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_PATHID );
	if ( !bFileExists )
	{
		if ( !CreateDefaultVideoConfig( configData ) )
			return false;
	}
	else
	{
		// Verify the default data is up to date.
		VerifyDefaultVideoConfig( configData );
	}

	return BLoadUserVideoConfigFileFromDisk( configData.pConfigKeys );
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool RecommendedConfig( VidMatConfigData_t &configData )
{
	// Verify that the file system has been created.
	Assert( g_pFullFileSystem != NULL );

	// If we are a video - this is a special case.
	if ( configData.bIsVideo )
	{
		return RecommendedVideoConfig( configData );
	}

	// Parse the configuration keys.
	return ParseConfigKeys( configData );
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool ResetVideoConfigToDefaults( KeyValues *pConfigKeys )
{
	// Copy the defaults settings into the current = Reset.
	if ( !CopyDefaultVideoToCurrentVideoConfig( VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_FILENAME ) )
		return false;

	// Copy the new key values if there the config keys exist.
	if ( pConfigKeys )
	{
		return pConfigKeys->LoadFromFile( g_pFullFileSystem, VIDEOCONFIG_FILENAME, VIDEOCONFIG_PATHID );
	}
	
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool UpdateCurrentVideoConfig( int nWidth, int nHeight, int nAspectRatioMode, bool bFullscreen, bool bNoWindow, bool bUseRestartConvars )
{
	// Create and Init the video config block.
	KeyValues *pVideoConfigKeys = new KeyValues( "VideoConfig" );
	if ( !pVideoConfigKeys )
		return false;

	// Go through each of the video settings and save off all necessary data.
	int nVideoConfigCount = ARRAYSIZE( s_pVideoConfigSettingsWhitelist );
	for ( int iVar = 0; iVar < nVideoConfigCount; ++iVar )
	{
		// Do we need to save this setting?
		if ( !s_pVideoConfigSettingsWhitelist[iVar].m_bSaved )
			continue;

		// Strip off the "setting." prefix and check for a ConVar
		// Strip off the "setting." prefix and check for a ConVar
		char szConVarName[256];
		int nStringLength = V_strlen( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar );
		nStringLength -= 8;
		if ( nStringLength <= 0 )
			continue;

		V_StrRight( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar, nStringLength, szConVarName, sizeof( szConVarName ) );

		bool bAutodetectedSetting = false;
		{
			char szConVarRestart[ 256 ];
			if ( s_pVideoConfigSettingsWhitelist[ iVar ].m_bUseAutoOption )
			{
				V_snprintf( szConVarRestart, sizeof( szConVarRestart ), "%s_optionsui", szConVarName );
				ConVarRef cvOptionsUi( szConVarRestart );
				if ( cvOptionsUi.IsValid() )
				{
					bAutodetectedSetting = ( cvOptionsUi.GetInt() == 9999999 );
				}
			}
			if ( bUseRestartConvars )
			{
				V_snprintf( szConVarRestart, sizeof( szConVarRestart ), "%s_restart", szConVarName );

				if ( g_pCVar->FindVar( szConVarRestart ) )
				{
					V_strncpy( szConVarName, szConVarRestart, sizeof( szConVarName ) );
				}
			}
		}

		// Is it a CVar?  If so, get the value.
		const ConVar *pVar = g_pCVar->FindVar( szConVarName );
		if ( pVar )
		{
			if ( bAutodetectedSetting )
			{
				char chSettingAutoName[ 128 ];
				V_sprintf_safe( chSettingAutoName, "setauto.%s", s_pVideoConfigSettingsWhitelist[ iVar ].m_pSettingVar + 8 );
				pVideoConfigKeys->SetString( chSettingAutoName, pVar->GetString() );
			}
			else
			{
				pVideoConfigKeys->SetString( s_pVideoConfigSettingsWhitelist[ iVar ].m_pSettingVar, pVar->GetString() );
			}
		}
	}
	
	// Set these window settings.
	pVideoConfigKeys->SetInt( "setting.defaultres", nWidth );
	pVideoConfigKeys->SetInt( "setting.defaultresheight", nHeight );
	pVideoConfigKeys->SetInt( "setting.aspectratiomode", nAspectRatioMode );
	pVideoConfigKeys->SetInt( "setting.fullscreen", static_cast<int>( bFullscreen ) );
	pVideoConfigKeys->SetInt( "setting.nowindowborder", static_cast<int>( bNoWindow ) );

	// Write out the file.
	// Create the file.
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	pVideoConfigKeys->RecursiveSaveToFile( buf, 0 );
	WriteVideoCfgDataToFile( VIDEOCONFIG_FILENAME, buf );

	// Destroy keys.
	pVideoConfigKeys->deleteThis();

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool UpdateVideoConfigConVars( KeyValues *pConfigKeys )
{
	bool bAllocConfig = false;
	if ( !pConfigKeys )
	{
		// Create and Init the video config block.
		pConfigKeys = new KeyValues( "VideoConfig" );
		if ( !pConfigKeys )
			return false;

		if ( !BLoadUserVideoConfigFileFromDisk( pConfigKeys ) )
		{
			pConfigKeys->deleteThis();
			return false;
		}

		bAllocConfig = true;
	}

#ifdef _DEBUG
	Msg( "UpdateVideoConfigConVars:\n" );
	KeyValuesDumpAsDevMsg( pConfigKeys, 0, 0 );
#endif

	ConVar *pVarOptionsUiCallback = g_pCVar->FindVar( "videooptions_optionsui_callback_disabled" );

	int nVideoConfigCount = ARRAYSIZE( s_pVideoConfigSettingsWhitelist );
	for ( int iVar = 0; iVar < nVideoConfigCount; ++iVar )
	{
		// Do we need to save this setting?
		if ( !s_pVideoConfigSettingsWhitelist[iVar].m_bConVar )
			continue;

		// Strip off the "setting." prefix and check for a ConVar
		char szConVarName[256];
		int nStringLength = V_strlen( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar );
		nStringLength -= 8;
		if ( nStringLength <= 0 )
			continue;

		V_StrRight( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar, nStringLength, szConVarName, sizeof( szConVarName ) );

		ConVar *pVar = g_pCVar->FindVar( szConVarName );
		if ( !pVar )
			continue;

		KeyValues *pFindKey = pConfigKeys->FindKey( s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar );
		if ( !pFindKey )
			continue;

		// Always allow the command line to totally override whatever convars come from the videocfg system.
		char szOption[256];
		V_snprintf( szOption, sizeof( szOption ), "+%s", szConVarName );
		if ( CommandLine()->CheckParm( szOption ) )
		{
			const char *pOverrideValue = CommandLine()->ParmValue( szOption, pFindKey->GetString() );
			pVar->SetValue( pOverrideValue );

			Warning( "UpdateVideoConfigConVars: Value of convar \"%s\" is being set from the cmd line, so this value will not be set by the video config system\n", szConVarName );
			continue;
		}

		pVar->SetValue( pFindKey->GetString() );

		{
			char szConVarRestart[ 256 ];
			V_sprintf_safe( szConVarRestart, "%s_restart", szConVarName );
			if ( ConVar *pVarRestart = g_pCVar->FindVar( szConVarRestart ) )
			{
				pVarRestart->SetValue( pFindKey->GetString() );
			}
		}

		if ( s_pVideoConfigSettingsWhitelist[iVar].m_bUseAutoOption )
		{
			char szConVarOptionsUi[ 256 ];
			V_sprintf_safe( szConVarOptionsUi, "%s_optionsui", szConVarName );
			if ( ConVar *pVarOptionsUi = g_pCVar->FindVar( szConVarOptionsUi ) )
			{
				if ( pVarOptionsUiCallback )
					pVarOptionsUiCallback->SetValue( 1 );

				// Check if the config instructs the convar to list as "AUTO"?
				char chSettingAutoName[128];
				V_sprintf_safe( chSettingAutoName, "setauto.%s", s_pVideoConfigSettingsWhitelist[iVar].m_pSettingVar + 8 );
				if ( pConfigKeys->FindKey( chSettingAutoName ) )
					pVarOptionsUi->SetValue( 9999999 );
				else
					pVarOptionsUi->SetValue( pFindKey->GetString() );

				if ( pVarOptionsUiCallback )
					pVarOptionsUiCallback->SetValue( 0 );
			}
		}
	}

	// If we created it - destroy it!
	if ( bAllocConfig )
	{
		pConfigKeys->deleteThis();
	}

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
bool ReadCurrentVideoConfig( KeyValues *pConfigKeys, bool bDefault )
{
	if ( !pConfigKeys )
		return false;

	if ( !bDefault )
	{
		// Do we have a current video config file?  If not, copy the defaults file.
		if ( !g_pFullFileSystem->FileExists( VIDEOCONFIG_FILENAME, VIDEOCONFIG_PATHID ) )
			return false;

		// Parse current video file.
		return BLoadUserVideoConfigFileFromDisk( pConfigKeys );
	}
	else
	{
		// Do we have a current video config file?  If not, copy the defaults file.
		if ( !g_pFullFileSystem->FileExists( VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_PATHID ) )
			return false;

		// Parse current video file.
		return pConfigKeys->LoadFromFile( g_pFullFileSystem, VIDEOCONFIG_DEFAULT_FILENAME, VIDEOCONFIG_PATHID );
	}
}
#endif

const unsigned char *GetModEncryptionKey( const char *pModName )
{
	if ( !V_strnicmp( "left4dead", pModName, V_strlen("left4dead") ) )
		return (unsigned char*)"zp14Hi(]";
	else if ( !V_strnicmp( "portal2", pModName, V_strlen("portal2") ) )
		return (unsigned char *)"UrE66!Ap"; 
	else if ( !V_strnicmp( "csgo", pModName, V_strlen("csgo") ) )
		return (unsigned char *)"aY7!rn[z";
	else if ( !V_strnicmp( "infested", pModName, V_strlen("infested") ) )
		return (unsigned char *)"sW9.JupP"; 
	else if ( !V_strnicmp( "ep2", pModName, V_strlen("ep2") ) )
		return (unsigned char *)"Xx81uBl)"; 
	else if ( !V_strnicmp( "tf", pModName, V_strlen("tf") ) )
		return (unsigned char *)"E2NcUkG2";
	else if ( !V_strnicmp( "nimbus", pModName, V_strlen("nimbus") ) )
		return (unsigned char *)"E2NcUkG2"; 
	else if ( !V_strnicmp( "dota", pModName, V_strlen( "dota" ) ) )
		return (unsigned char *)"dAIt1IL!"; 
	else
		return (unsigned char*)"X8bU2qll";
}


KeyValues* ReadEncryptedKVFile( const char *pRelativePath, const char *pPathID, const char *pModName )
{
	// Open the keyvalues, and abort if we can't
	FileHandle_t f = g_pFullFileSystem->Open( pRelativePath, "rb", pPathID );
	if ( !f )
	{
		return NULL;
	}

	// load file into a null-terminated buffer
	int fileSize = g_pFullFileSystem->Size( f );
	char *buffer = (char*)stackalloc( fileSize + 1 );
	g_pFullFileSystem->Read( buffer, fileSize, f );
	buffer[fileSize] = 0;
	g_pFullFileSystem->Close( f );

	DecodeICE( (unsigned char*)buffer, fileSize, GetModEncryptionKey( pModName ) );

	KeyValues *pKV = new KeyValues( "kv" );
	bool retOK = pKV->LoadFromBuffer( pRelativePath, buffer, g_pFullFileSystem );
	if ( !retOK )
	{
		pKV->deleteThis();
		return NULL;
	}

	return pKV;
}

struct SystemLevelConvar_t
{
	const char *m_pConVar;
	bool m_bChooseLower;
	bool m_bAllowed;
};

SystemLevelConvar_t s_pConVarsAllowedInSystemLevel[] = 
{
	{ "lower_body",						true, true },
	{ "r_shadow_half_update_rate",		false, true },
	{ "r_rainparticledensity",			true, true },
	{ "cl_particle_fallback_base",		false, true },
	{ "cl_particle_fallback_multiplier",false, true },
	{ "r_flashlightdepthtexture",		true, true },
	{ "r_shadowrendertotexture",		true, true },
	{ "r_shadowfromworldlights",		true, true },
	{ "cl_detaildist",					true, true },
	{ "cl_detailfade",					true, true },
	{ "r_drawmodeldecals",				true, false },		// force consistency across all levels
#ifndef _PS3
	{ "r_decalstaticprops",				true, false },		// force consistency across all levels
#endif
	{ "ragdoll_sleepaftertime",			true, true },
	{ "cl_phys_maxticks",				true, true },
	{ "r_worldlightmin",				true, true },
	{ "props_break_max_pieces",			true, true },
	{ "r_worldlights",					true, true },
	{ "r_decals",						true, false },		// force consistency across all levels
	{ "r_decal_overlap_count",			true, false },		// force consistency across all levels
	{ "mat_bumpmap",					true, true },
	{ "mat_detail_tex",					true, true },
	{ "mat_specular",					true, true },
	{ "mat_phong",						true, true },
	{ "mat_grain_enable",				true, true },
	{ "mat_local_contrast_enable",		true, true },
	{ "mat_motion_blur_enabled",		true, true },
	{ "mat_disablehwmorph",				false, true },
	{ "r_overlayfademin",				true, true },
	{ "r_overlayfademax",				true, true },
	{ "z_mob_simple_shadows",			false, true },
	{ "cl_ragdoll_maxcount",			true, true },
	{ "cl_ragdoll_maxcount_gib",		true, true },
	{ "cl_ragdoll_maxcount_generic",	true, true },
	{ "cl_ragdoll_maxcount_special",	true, true },
	{ "sv_ragdoll_maxcount",			true, true },
	{ "sv_ragdoll_maxcount_gib",		true, true },
	{ "sv_ragdoll_maxcount_generic",	true, true },
	{ "sv_ragdoll_maxcount_special",	true, true },
	{ "z_infected_decals",				true, true },
	{ "cl_impacteffects_limit_general",	true, true },
	{ "cl_impacteffects_limit_exit",	true, true },
	{ "cl_impacteffects_limit_water",	true, true },
	{ "cl_ragdoll_self_collision",		true, true },
	{ "cl_player_max_decal_count",		true, true },
	{ "cl_footstep_fx",					true, true },
	{ "mp_usehwmvcds",					true, true },
	{ "mp_usehwmmodels",				true, true },
	{ "mat_depthfeather_enable",		true, true },
	{ "mat_dxlevel",					true, true },
	{ "r_flashlightinfectedfov",		true, true },
	{ "r_flashlightinfectedfar",		true, true },
	{ "r_flashlightinfectedlinear",		true, true },
	{ "r_rootlod",						false, true },
	{ "mat_picmip",						false, true },
	{ "mat_force_vertexfog",			false, true },
	{ "r_simpleworldmodel_waterreflections_fullscreen",		 false, true },
	{ "r_simpleworldmodel_drawforrecursionlevel_fullscreen", true, true },
	{ "r_simpleworldmodel_drawbeyonddistance_fullscreen",	 true, true },
	{ "r_simpleworldmodel_waterreflections_splitscreen",	 true, true },
	{ "r_simpleworldmodel_drawforrecursionlevel_splitscreen",true, true },
	{ "r_simpleworldmodel_drawbeyonddistance_splitscreen",	 true, true },
	{ "r_simpleworldmodel_waterreflections_pip",			 true, true },
	{ "r_simpleworldmodel_drawforrecursionlevel_pip",		 true, true },
	{ "r_simpleworldmodel_drawbeyonddistance_pip",			 true, true },
	{ "r_lod_switch_scale",									 true, true },
	{ "r_lod",												 false, true },
	{ "r_paintblob_highres_cube",							 false, true },
	{ "r_paintblob_force_single_pass",						 true, true },
	{ "r_paintblob_max_number_of_threads",					 true, true },
	{ "cl_csm_enabled",										 true, true },
};

void PerformSystemConfiguration( KeyValues *pResult, int nSystemLevel, const char *pConfigFile, const char *pModName, bool bUseSplitScreenCfg, bool bVGUIIsSplitscreen )
{
	char pCfgFile[MAX_PATH];
	if ( nSystemLevel == CONSOLE_SYSTEM_LEVEL_PS3 )
	{
		// PS3
		Q_snprintf( pCfgFile, sizeof(pCfgFile), "cfg\\%s_ps3", pConfigFile );
	}
	else
	{
		// everything else
		Q_snprintf( pCfgFile, sizeof(pCfgFile), "cfg\\%s_%d", pConfigFile, nSystemLevel );
	}
	if ( !IsGameConsole() )
	{
		Q_strncat( pCfgFile, "_pc", sizeof(pCfgFile) );
	}
	if ( bUseSplitScreenCfg && bVGUIIsSplitscreen )
	{
		Q_strncat( pCfgFile, "_ss", sizeof(pCfgFile) );
	}
	Q_strncat( pCfgFile, ".ekv", sizeof(pCfgFile) );

	KeyValues *pKeyValues = ReadEncryptedKVFile( pCfgFile, "GAME", pModName );
	if ( !pKeyValues )
	{
		DevWarning( "PerformSystemConfiguration: Missing %s\n", pCfgFile );
		return;
	}

	for( KeyValues *pKey = pKeyValues->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		const char *pCVarName = pKey->GetName();

		// Check if legal
		int i;
		bool bLegalVar = false;
		for( i=0; i < ARRAYSIZE( s_pConVarsAllowedInSystemLevel ); i++ )
		{
			if ( !Q_stricmp( s_pConVarsAllowedInSystemLevel[i].m_pConVar, pCVarName ) )
			{
				bLegalVar = true;
				break;
			}
		}
		if ( !bLegalVar )
		{
			DevWarning("PerformSystemConfiguration: Bad convar found in %s - %s\n", pConfigFile, pCVarName );
			continue;
		}
		if ( !s_pConVarsAllowedInSystemLevel[i].m_bAllowed )
		{
#ifdef _DEBUG
			DevWarning("PerformSystemConfiguration: Skipping convar found in %s - %s\n", pConfigFile, pCVarName );
#endif
			continue;
		}

		if ( pResult->FindKey( pCVarName ) )
		{
			float flOldValue = pResult->GetFloat( pCVarName );
			float flNewValue = pKey->GetFloat();
			if ( s_pConVarsAllowedInSystemLevel[i].m_bChooseLower )
			{
				if ( flNewValue >= flOldValue )
					continue;
			}
			else
			{
				if ( flNewValue <= flOldValue )
					continue;
			}
		}

		const char *pValue = pKey->GetString();
		pResult->SetString( pCVarName, pValue );
	}

	pKeyValues->deleteThis();
}

void UpdateSystemLevel( int nCPULevel, int nGPULevel, int nMemLevel, int nGPUMemLevel, bool bVGUIIsSplitscreen, const char *pModName )
{
	KeyValues *pKeyValues = new KeyValues( "kv" );
	PerformSystemConfiguration( pKeyValues, nCPULevel, "cpu_level", pModName, true, bVGUIIsSplitscreen );
	PerformSystemConfiguration( pKeyValues, nGPULevel, "gpu_level", pModName, false, bVGUIIsSplitscreen );
	PerformSystemConfiguration( pKeyValues, nMemLevel, "mem_level", pModName, false, bVGUIIsSplitscreen );
	PerformSystemConfiguration( pKeyValues, nGPUMemLevel, "gpu_mem_level", pModName, false, bVGUIIsSplitscreen );

	for( KeyValues *pKey = pKeyValues->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		const char *pCVarName = pKey->GetName();
		ConVar *pConVar = g_pCVar->FindVar( pCVarName );
		if ( !pConVar )
			continue;
				
		// We want this on all platforms now - having the config system always slam convars being defined on the cmd line is just too confusing.
		//if ( IsX360() )
		{
			bool bFound = false;
			for ( int i=1; !bFound && i < CommandLine()->ParmCount(); i++ )
			{
				const char *szParm = CommandLine()->GetParm(i);
				if ( szParm && szParm[0] == '+' )
				{
					bFound = ( V_stricmp( pCVarName, szParm + 1 ) == 0 );
				}
			}
			if ( bFound )
			{
				// found on command line, ignore any value the script would have set
				Warning( "UpdateSystemLevel: System configuration ignoring %s due to command line override\n", pCVarName );
				continue;
			}
		}

		if ( pConVar->GetFlags() & ( FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_CHEAT ) )
		{
			Warning( "UpdateSystemLevel: ConVar %s controlled by gpu_level/cpu_level must not be marked as FCVAR_ARCHIVE or FCVAR_CHEAT!\n", pCVarName );
			continue;
		}

		pConVar->SetValue( pKey->GetString() );
	}
	pKeyValues->deleteThis();
}

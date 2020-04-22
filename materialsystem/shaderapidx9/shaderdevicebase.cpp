//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "shaderdevicebase.h"
#include "tier1/keyvalues.h"
#include "tier1/convar.h"
#include "tier1/utlbuffer.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "datacache/idatacache.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapibase.h"
#include "shaderapi/ishadershadow.h"
#include "shaderapi_global.h"
#include "videocfg/videocfg.h"
#include "vjobs_interface.h"
#include "winutils.h"

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CShaderDeviceBase *g_pShaderDevice;
CShaderAPIBase *g_pShaderAPI;
CShaderDeviceMgrBase *g_pShaderDeviceMgr;
IShaderShadow *g_pShaderShadow;
#if !defined( _PS3 ) && !defined( _OSX )
IShaderUtil* g_pShaderUtil;		// The main shader utility interface
IVJobs * g_pVJobs;
#else
extern IVJobs * g_pVJobs;
#endif

bool g_bUseShaderMutex = false;	// Shader mutex globals
bool g_bShaderAccessDisallowed;
CShaderMutex g_ShaderMutex;

//-----------------------------------------------------------------------------
// FIXME: Hack related to setting command-line values for convars. Remove!!!
//-----------------------------------------------------------------------------
class CShaderAPIConVarAccessor : public IConCommandBaseAccessor
{
public:
	virtual bool RegisterConCommandBase( ConCommandBase *pCommand )
	{
		// Link to engine's list instead
		g_pCVar->RegisterConCommand( pCommand );

		char const *pValue = g_pCVar->GetCommandLineValue( pCommand->GetName() );
		if( pValue && !pCommand->IsCommand() )
		{
			( ( ConVar * )pCommand )->SetValue( pValue );
		}
		return true;
	}
};

static void InitShaderAPICVars( )
{
	static CShaderAPIConVarAccessor g_ConVarAccessor;
	if ( g_pCVar )
	{
		ConVar_Register( FCVAR_MATERIAL_SYSTEM_THREAD, &g_ConVarAccessor );
	}
}



//-----------------------------------------------------------------------------
// Read dx support levels
//-----------------------------------------------------------------------------
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
#define SUPPORT_CFG_FILE "dxsupport_mac.cfg"
// TODO: make this different for Mac?
#define SUPPORT_CFG_OVERRIDE_FILE "dxsupport_override.cfg"
#else
#define SUPPORT_CFG_FILE "dxsupport.cfg"
#define SUPPORT_CFG_OVERRIDE_FILE "dxsupport_override.cfg"
#endif


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceMgrBase::CShaderDeviceMgrBase()
{
	m_pDXSupport = NULL;
#if defined( _PS3 ) || defined( _OSX )
	g_pShaderDeviceMgr = this;
#endif
}

CShaderDeviceMgrBase::~CShaderDeviceMgrBase()
{
}


//-----------------------------------------------------------------------------
// Factory used to get at internal interfaces (used by shaderapi + shader dlls)
//-----------------------------------------------------------------------------
static CreateInterfaceFn s_TempFactory;
void *ShaderDeviceFactory( const char *pName, int *pReturnCode )
{
	if (pReturnCode)
	{
		*pReturnCode = IFACE_OK;
	}

	void *pInterface = s_TempFactory( pName, pReturnCode );
	if ( pInterface )
		return pInterface;

	pInterface = Sys_GetFactoryThis()( pName, pReturnCode );
	if ( pInterface )
		return pInterface;

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;	
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::Connect( CreateInterfaceFn factory )
{
	LOCK_SHADERAPI();

	Assert( IsPS3() || IsOSX() || !g_pShaderDeviceMgr );

	s_TempFactory = factory;

	// Connection/convar registration
	CreateInterfaceFn actualFactory = ShaderDeviceFactory;
	ConnectTier1Libraries( &actualFactory, 1 );
	InitShaderAPICVars();
	ConnectTier2Libraries( &actualFactory, 1 );
#if !defined( _PS3 ) && !defined( _OSX )
	if ( !g_pShaderUtil )
		g_pShaderUtil = (IShaderUtil*)ShaderDeviceFactory( SHADER_UTIL_INTERFACE_VERSION, NULL );
#endif
	if ( !g_pVJobs )
		g_pVJobs = (IVJobs *)ShaderDeviceFactory( VJOBS_INTERFACE_VERSION, NULL );
	
	g_pShaderDeviceMgr = this;

	s_TempFactory = NULL;

	if ( !g_pShaderUtil || !g_pFullFileSystem || !g_pShaderDeviceMgr )
	{
		Warning( "ShaderAPIDx10 was unable to access the required interfaces!\n" );
		return false;
	}

	// NOTE! : Overbright is 1.0 so that Hammer will work properly with the white bumped and unbumped lightmaps.
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	return true;
}

void CShaderDeviceMgrBase::Disconnect()
{
	LOCK_SHADERAPI();

#if !defined( _PS3 ) && !defined( _OSX )
	g_pShaderDeviceMgr = NULL;
	g_pShaderUtil = NULL;
#endif
	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();

	if ( m_pDXSupport )
	{
		m_pDXSupport->deleteThis();
		m_pDXSupport = NULL;
	}
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CShaderDeviceMgrBase::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION ) )
		return ( IShaderDeviceMgr* )this;
	if ( !Q_stricmp( pInterfaceName, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION ) )
		return ( IMaterialSystemHardwareConfig* )g_pHardwareConfig;
	return NULL;
}


//-----------------------------------------------------------------------------
// Returns the hardware caps for a particular adapter
//-----------------------------------------------------------------------------
const HardwareCaps_t& CShaderDeviceMgrBase::GetHardwareCaps( int nAdapter ) const
{
	Assert( ( nAdapter >= 0 ) && ( nAdapter < GetAdapterCount() ) );
	return m_Adapters[nAdapter].m_ActualCaps;
}


//-----------------------------------------------------------------------------
// Utility methods for reading config scripts
//-----------------------------------------------------------------------------
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

static bool ReadBool( KeyValues *pGroup, const char *pKeyName, bool bDefault )
{
	int nVal = pGroup->GetInt( pKeyName, -1 );
	if ( nVal != -1 )
	{
		//		Warning( "\t%s = %s\n", pKeyName, (nVal != false) ? "true" : "false" );
		return (nVal != false);
	}
	return bDefault;
}

static void ReadInt( KeyValues *pGroup, const char *pKeyName, int nInvalidValue, int *pResult )
{
	int nVal = pGroup->GetInt( pKeyName, nInvalidValue );
	if ( nVal != nInvalidValue )
	{
		*pResult = nVal;
		//		Warning( "\t%s = %d\n", pKeyName, *pResult );
	}
}


//-----------------------------------------------------------------------------
// Utility method to copy over a keyvalue
//-----------------------------------------------------------------------------
static void AddKey( KeyValues *pDest, KeyValues *pSrc )
{
	// Note this will replace already-existing values
	switch( pSrc->GetDataType() )
	{
	case KeyValues::TYPE_NONE:
		break;
	case KeyValues::TYPE_STRING:
		pDest->SetString( pSrc->GetName(), pSrc->GetString() );
		break;
	case KeyValues::TYPE_INT:
		pDest->SetInt( pSrc->GetName(), pSrc->GetInt() );
		break;
	case KeyValues::TYPE_FLOAT:
		pDest->SetFloat( pSrc->GetName(), pSrc->GetFloat() );
		break;
	case KeyValues::TYPE_PTR:
		pDest->SetPtr( pSrc->GetName(), pSrc->GetPtr() );
		break;
	case KeyValues::TYPE_WSTRING:
		pDest->SetWString( pSrc->GetName(), pSrc->GetWString() );
		break;
	case KeyValues::TYPE_COLOR:
		pDest->SetColor( pSrc->GetName(), pSrc->GetColor() );
		break;
	default:
		Assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Finds if we have a dxlevel-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindDXLevelSpecificConfig( KeyValues *pKeyValues, int nDxLevel )
{
	KeyValues *pGroup = pKeyValues->GetFirstSubKey();
	for( pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nFoundDxLevel = pGroup->GetInt( "name", 0 );
		if( nFoundDxLevel == nDxLevel )
			return pGroup;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Finds if we have a dxlevel and vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindDXLevelAndVendorSpecificConfig( KeyValues *pKeyValues, int nDxLevel, int nVendorID )
{
	if ( IsX360() )
	{
		// 360 unique dxlevel implies hw config, vendor variance not applicable
		return NULL;
	}

	KeyValues *pGroup = pKeyValues->GetFirstSubKey();
	for( pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nFoundDxLevel = pGroup->GetInt( "name", 0 );
		int nFoundVendorID = ReadHexValue( pGroup, "VendorID" );
		if( nFoundDxLevel == nDxLevel && nFoundVendorID == nVendorID )
			return pGroup;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Finds if we have a vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindCPUSpecificConfig( KeyValues *pKeyValues, int nCPUMhz, bool bAMD )
{
	if ( IsX360() )
	{
		// 360 unique dxlevel implies hw config, cpu variance not applicable
		return NULL;
	}

	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		const char *pName = pGroup->GetString( "name", NULL );
		if ( !pName )
			continue;

		if ( ( bAMD && Q_stristr( pName, "AMD" ) ) || 
			( !bAMD && Q_stristr( pName, "Intel" ) ) )
		{
			int nMinMegahertz = pGroup->GetInt( "min megahertz", -1 );
			int nMaxMegahertz = pGroup->GetInt( "max megahertz", -1 );
			if( nMinMegahertz == -1 || nMaxMegahertz == -1 )
				continue;

			if( nMinMegahertz <= nCPUMhz && nCPUMhz < nMaxMegahertz )
				return pGroup;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Finds if we have a vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindCardSpecificConfig( KeyValues *pKeyValues, int nVendorId, int nDeviceId )
{
	if ( IsX360() )
	{
		// 360 unique dxlevel implies hw config, vendor variance not applicable
		return NULL;
	}

	KeyValues *pGroup = pKeyValues->GetFirstSubKey();
	for( pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nFoundVendorId = ReadHexValue( pGroup, "VendorID" );
		int nFoundDeviceIdMin = ReadHexValue( pGroup, "MinDeviceID" );
		int nFoundDeviceIdMax = ReadHexValue( pGroup, "MaxDeviceID" );
		if ( nFoundVendorId == nVendorId && nDeviceId >= nFoundDeviceIdMin && nDeviceId <= nFoundDeviceIdMax )
			return pGroup;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Finds if we have a vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindMemorySpecificConfig( KeyValues *pKeyValues, int nSystemRamMB )
{
	if ( IsX360() )
	{
		// 360 unique dxlevel implies hw config, memory variance not applicable
		return NULL;
	}

	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		// Used to help us debug this code
//		const char *pDebugName = pGroup->GetString( "name", "blah" );

		int nMinMB = pGroup->GetInt( "min megabytes", -1 );
		int nMaxMB = pGroup->GetInt( "max megabytes", -1 );
		if ( nMinMB == -1 || nMaxMB == -1 )
			continue;

		if ( nMinMB <= nSystemRamMB && nSystemRamMB < nMaxMB )
			return pGroup;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Finds if we have a texture mem size specific config
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindVidMemSpecificConfig( KeyValues *pKeyValues, int nVideoRamMB )
{	
	if ( IsX360() )
	{
		// 360 unique dxlevel implies hw config, vidmem variance not applicable
		return NULL;
	}

	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nMinMB = pGroup->GetInt( "min megatexels", -1 );
		int nMaxMB = pGroup->GetInt( "max megatexels", -1 );
		if ( nMinMB == -1 || nMaxMB == -1 )
			continue;

		if ( nMinMB <= nVideoRamMB && nVideoRamMB < nMaxMB )
			return pGroup;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Methods related to reading DX support levels given particular devices
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::ReadDXSupportKeyValues()
{
	if ( CommandLine()->CheckParm( "-ignoredxsupportcfg" ) )
		return NULL;

	if ( m_pDXSupport )
		return m_pDXSupport;

	KeyValues *pCfg = new KeyValues( "dxsupport" );

	const char *pPathID = "EXECUTABLE_PATH";
	if ( IsX360() && g_pFullFileSystem->GetDVDMode() != DVDMODE_OFF )
	{
		// 360 dvd optimization, expect it inside the platform zip
		pPathID = "PLATFORM";
	}

	// First try to read a game-specific config, if it exists
	if ( !pCfg->LoadFromFile( g_pFullFileSystem, SUPPORT_CFG_FILE, pPathID ) )
	{
		pCfg->deleteThis();
		return NULL;
	}

	m_pDXSupport = pCfg;
	return pCfg;
}


//-----------------------------------------------------------------------------
// Returns the max dx support level achievable with this board
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::ReadDXSupportLevels( HardwareCaps_t &caps )
{
	// See if the file tells us otherwise
	KeyValues *pCfg = ReadDXSupportKeyValues();
	if ( !pCfg )
		return;

	KeyValues *pDeviceKeyValues = FindCardSpecificConfig( pCfg, caps.m_VendorID, caps.m_DeviceID );
	if ( pDeviceKeyValues )
	{
		// First, set the max dx level
		int nMaxDXSupportLevel = 0;
		ReadInt( pDeviceKeyValues, "setting.MaxDXLevel", 0, &nMaxDXSupportLevel );
		if ( nMaxDXSupportLevel != 0 )
		{
			caps.m_nMaxDXSupportLevel = nMaxDXSupportLevel;
		}

		int nMinDXSupportLevel = 0;
		ReadInt( pDeviceKeyValues, "setting.MinDXLevel", 0, &nMinDXSupportLevel );
		if ( nMinDXSupportLevel != 0 && nMinDXSupportLevel <= caps.m_nMaxDXSupportLevel )
		{
			caps.m_nMinDXSupportLevel = nMinDXSupportLevel;
		}

		// Next, set the preferred dx level
		int nDXSupportLevel = 0;
		ReadInt( pDeviceKeyValues, "setting.DXLevel", 0, &nDXSupportLevel );
		if ( nDXSupportLevel != 0 )
		{
			caps.m_nDXSupportLevel = nDXSupportLevel;
		}
		else
		{
			caps.m_nDXSupportLevel = caps.m_nMaxDXSupportLevel;
		}
	}
}


//-----------------------------------------------------------------------------
// Loads the hardware caps, for cases in which the D3D caps lie or where we need to augment the caps
//-----------------------------------------------------------------------------

#ifdef OSX
ConVar mat_osx_csm_enabled( "mat_osx_csm_enabled", "1", FCVAR_DEVELOPMENTONLY, "" );
#endif

void CShaderDeviceMgrBase::LoadHardwareCaps( KeyValues *pGroup, HardwareCaps_t &caps )
{
	if( !pGroup )
		return;

#ifndef _PS3
	// don't just blanket kill clip planes on POSIX, only shoot them down if we're running ARB, or asked for nouserclipplanes.
	//FIXME need to take into account the caps bit that GLM can now provide, so NV can use normal clipping and ATI can fall back to fastclip.
	if ( CommandLine()->FindParm("-arbmode") || CommandLine()->CheckParm( "-nouserclip" ) )
	{
		caps.m_UseFastClipping = true;
	}
	else
	{
		caps.m_UseFastClipping = ReadBool( pGroup, "setting.NoUserClipPlanes", caps.m_UseFastClipping );
	}
#endif

	caps.m_bNeedsATICentroidHack = ReadBool( pGroup, "setting.CentroidHack", caps.m_bNeedsATICentroidHack );
	caps.m_bDisableShaderOptimizations = ReadBool( pGroup, "setting.DisableShaderOptimizations", caps.m_bDisableShaderOptimizations );
	caps.m_bPreferZPrepass = ReadBool( pGroup, "setting.PreferZPrepass", caps.m_bPreferZPrepass );
	caps.m_bSuppressPixelShaderCentroidHackFixup = ReadBool( pGroup, "setting.SuppressPixelShaderCentroidHackFixup", caps.m_bSuppressPixelShaderCentroidHackFixup );
	caps.m_bPreferTexturesInHWMemory = ReadBool( pGroup, "setting.PreferTexturesInHWMemory", caps.m_bPreferTexturesInHWMemory );
	caps.m_bPreferHardwareSync = ReadBool( pGroup, "setting.PreferHardwareSync", caps.m_bPreferHardwareSync );
	caps.m_bUnsupported = ReadBool( pGroup, "setting.Unsupported", caps.m_bUnsupported );
	
	// dxsupport can only kill CSM support, not forcefully enable it.
	if ( !ReadBool( pGroup, "setting.SupportsCascadedShadowMapping", true ) )
	{
#ifdef OSX
		// Set convar mat_osx_csm_enabled to 0 and do not touch caps.m_bSupportsCascadedShadowMapping (as CS:GO always had
		// the caps set to true, therefore code path where the caps is false haven't been tested and might not be safe)
		mat_osx_csm_enabled.SetValue( 0 );
#else
		caps.m_bSupportsCascadedShadowMapping = false;
#endif
	}
	
	int nCSMQuality = CSMQUALITY_VERY_LOW;
	ReadInt( pGroup, "setting.CSMQuality", 0, &nCSMQuality );
	caps.m_nCSMQuality = static_cast<uint8>( clamp( nCSMQuality, 0, CSMQUALITY_TOTAL_MODES - 1 ) );
}


//-----------------------------------------------------------------------------
// Reads in the hardware caps from the dxsupport.cfg file
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::ReadHardwareCaps( HardwareCaps_t &caps, int nDxLevel )
{
	KeyValues *pCfg = ReadDXSupportKeyValues();
	if ( !pCfg )
		return;

	// Next, read the hardware caps for that dx support level.
	KeyValues *pDxLevelKeyValues = FindDXLevelSpecificConfig( pCfg, nDxLevel );
	// Look for a vendor specific line for a given dxlevel.
	KeyValues *pDXLevelAndVendorKeyValue = FindDXLevelAndVendorSpecificConfig( pCfg, nDxLevel, caps.m_VendorID );
	// Finally, override the hardware caps based on the specific card
	KeyValues *pCardKeyValues = FindCardSpecificConfig( pCfg, caps.m_VendorID, caps.m_DeviceID );

	// Apply 
	if( pCardKeyValues && ReadHexValue( pCardKeyValues, "MinDeviceID" ) == 0 && ReadHexValue( pCardKeyValues, "MaxDeviceID" ) == 0xffff )
	{
		// The card specific case is a catch all for device ids, so run it before running the dxlevel and card specific stuff.
		LoadHardwareCaps( pDxLevelKeyValues, caps );
		LoadHardwareCaps( pCardKeyValues, caps );
		LoadHardwareCaps( pDXLevelAndVendorKeyValue, caps );
	}
	else
	{
		// The card specific case is a small range of cards, so run it last to override all other configs.
		LoadHardwareCaps( pDxLevelKeyValues, caps );
		// don't run this one since we have a specific config for this card.
		//		LoadHardwareCaps( pDXLevelAndVendorKeyValue, caps );
		LoadHardwareCaps( pCardKeyValues, caps );
	}
}


//-----------------------------------------------------------------------------
// Reads in ConVars + config variables
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::LoadConfig( KeyValues *pKeyValues, KeyValues *pConfiguration )
{
	if( !pKeyValues )
		return;

	if( CommandLine()->FindParm( "-debugdxsupport" ) )
	{
		CUtlBuffer tmpBuf;
		pKeyValues->RecursiveSaveToFile( tmpBuf, 0 );
		Warning( "%s\n", ( const char * )tmpBuf.Base() );
	}
	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		AddKey( pConfiguration, pGroup );
	}
}


//-----------------------------------------------------------------------------
// Computes amount of ram
//-----------------------------------------------------------------------------
static unsigned long GetRam()
{
#if defined(PLATFORM_WINDOWS)
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof( MEMORYSTATUSEX );
	GlobalMemoryStatusEx( &statex );
	if ( statex.ullAvailPhys < statex.ullAvailVirtual )
	{
		return ( unsigned long )( statex.ullAvailPhys / ( 1024 * 1024 ) );
	}
	return ( unsigned long )( statex.ullAvailVirtual / ( 1024 * 1024 ) );
#else
	MEMORYSTATUS stat;
	stat.dwLength = sizeof( MEMORYSTATUS );
	GlobalMemoryStatus( &stat );
	return ( unsigned long )( stat.dwTotalPhys / ( 1024 * 1024 ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Gets the recommended video configuration.
// Called from: 
// 1. Called from gameui.dll->OptionsSubVideo.cpp->COptionsSubVideoAdvancedDlg to fill in the video options panel.
// 2. Called from gameui.dll->OptionsSubVideo.cpp->MarkDefaultSettingsAsRecommended when action is "Reset To Defaults"
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedVideoConfig( int nAdapter, int nVendorID, int nDeviceID, KeyValues *pConfiguration ) 
{
	if ( IsX360() )
	{
		// this is not compatible with xbox
		return false;
	}

	LOCK_SHADERAPI();

	VidMatConfigData_t configData;
#ifdef POSIX
	V_strcpy_safe( configData.szFileName, "cfg/moddefaults_mac.txt" );
#else
	V_strcpy_safe( configData.szFileName, "cfg\\moddefaults.txt" );
#endif
	V_strcpy_safe( configData.szPathID, "MOD" );
	configData.pConfigKeys = pConfiguration;
	configData.nVendorID = nVendorID;
	configData.nDeviceID = nDeviceID;
	configData.nSystemMemory = GetRam();
	int nTextureMemorySize = GetVidMemBytes( nAdapter );
	configData.nVideoMemory = nTextureMemorySize / ( 1024 * 1024 );
	configData.bIsVideo = true;
	configData.nDXLevel = g_pHardwareConfig->GetMaxDXSupportLevel();
	GetDesktopResolution( &configData.nPhysicalScreenWidth, &configData.nPhysicalScreenHeight, nAdapter );

	int nModeCount = GetModeCount( nAdapter );
	configData.displayModes.SetCount( nModeCount );
	for ( int i = 0; i < nModeCount; ++i )
	{
		GetModeInfo( &configData.displayModes[i], nAdapter, i );
	}
	
#if !defined( _X360 )
	// Call into the video.cfg file to setup the video config cvars.
	RecommendedConfig( configData );

	// Update the convars.
	UpdateVideoConfigConVars( configData.pConfigKeys );
#endif

	return true;
//#endif
}

//-----------------------------------------------------------------------------
// Purpose: Gets the recommended video configuration.
// Called from: 
// 3. Used by materialsystem.dll->cmaterialsystem.cpp->GenerateConfigFromConfigKeyValues to fill in material system data (MaterialSystem_Config_t)
// 4. Used by materialsystem.dll->cmaterialsystem.cpp->WriteConfigurationInfoToConVars to write the material system data into ConVars
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, int nVendorID, int nDeviceID, KeyValues *pConfiguration ) 
{
	if ( IsX360() )
	{
		// this is not compatible with xbox
		return false;
	}

	LOCK_SHADERAPI();

	VidMatConfigData_t configData;
#ifdef OSX
	V_strcpy_safe( configData.szFileName, "../bin/dxsupport_mac.cfg" );
#elif LINUX
	V_strcpy_safe( configData.szFileName, "../bin/dxsupport.cfg" );
#else
	V_strcpy_safe( configData.szFileName, "..\\bin\\dxsupport.cfg" );
#endif
	V_strcpy_safe( configData.szPathID, "GAME" );
	configData.pConfigKeys = pConfiguration;
	configData.nVendorID = nVendorID;
	configData.nDeviceID = nDeviceID;
	configData.nSystemMemory = GetRam();
	int nTextureMemorySize = GetVidMemBytes( nAdapter );
	configData.nVideoMemory = nTextureMemorySize / ( 1024 * 1024 );
	configData.bIsVideo = false;
	configData.nDXLevel = g_pHardwareConfig->GetMaxDXSupportLevel();
	GetDesktopResolution( &configData.nPhysicalScreenWidth, &configData.nPhysicalScreenHeight, nAdapter );

	int nModeCount = GetModeCount( nAdapter );
	configData.displayModes.SetCount( nModeCount );
	for ( int i = 0; i < nModeCount; ++i )
	{
		GetModeInfo( &configData.displayModes[i], nAdapter, i );
	}

#if !defined( _X360 ) && !defined( _PS3 )
	// Call into the video.cfg file to setup the video config cvars.
	RecommendedConfig( configData );
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Gets recommended congifuration for a particular adapter at a particular dx level
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedVideoConfig( int nAdapter, KeyValues *pCongifuration )
{
	Assert( nAdapter >= 0 && nAdapter <= GetAdapterCount() );
	MaterialAdapterInfo_t info;
	GetAdapterInfo( nAdapter, info );
	return GetRecommendedVideoConfig( nAdapter, info.m_VendorID, info.m_DeviceID, pCongifuration );
}

//-----------------------------------------------------------------------------
// Gets recommended congifuration for a particular adapter at a particular dx level
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pCongifuration )
{
	Assert( nAdapter >= 0 && nAdapter <= GetAdapterCount() );
	MaterialAdapterInfo_t info;
	GetAdapterInfo( nAdapter, info );
	return GetRecommendedConfigurationInfo( nAdapter, nDXLevel, info.m_VendorID, info.m_DeviceID, pCongifuration );
}


//-----------------------------------------------------------------------------
// Returns only valid dx levels
//-----------------------------------------------------------------------------
int CShaderDeviceMgrBase::GetClosestActualDXLevel( int nDxLevel ) const
{
	if ( IsX360() )
		return 98;
	
	if ( nDxLevel < 92 )
		return 90;
	if ( nDxLevel < 95 )
		return 92;
	if ( nDxLevel < 100 )
		return 95;
    
	return 100;
}

//-----------------------------------------------------------------------------
// Mode change callback
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func )
{
	LOCK_SHADERAPI();
	Assert( func && m_ModeChangeCallbacks.Find( func ) < 0 );
	m_ModeChangeCallbacks.AddToTail( func );
}

void CShaderDeviceMgrBase::RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func )
{
	LOCK_SHADERAPI();
	m_ModeChangeCallbacks.FindAndRemove( func );
}

void CShaderDeviceMgrBase::InvokeModeChangeCallbacks( int screenWidth, int screenHeight )
{
	int nCount = m_ModeChangeCallbacks.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_ModeChangeCallbacks[i]();
	}

	nCount = m_DeviceDependentObjects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_DeviceDependentObjects[i]->ScreenSizeChanged( screenWidth, screenHeight );
	}
}

void CShaderDeviceMgrBase::AddDeviceDependentObject( IShaderDeviceDependentObject *pObject )
{
	if ( pObject )
	{
		LOCK_SHADERAPI();
		Assert( pObject && m_DeviceDependentObjects.Find( pObject ) < 0 );
		m_DeviceDependentObjects.AddToTail( pObject );
	}
}

void CShaderDeviceMgrBase::RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject )
{
	if ( pObject )
	{
		LOCK_SHADERAPI();
		m_DeviceDependentObjects.FindAndRemove( pObject );
	}
}

void CShaderDeviceMgrBase::InvokeDeviceLostNotifications( void )
{
	int nCount = m_DeviceDependentObjects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_DeviceDependentObjects[i]->DeviceLost();
	}
}

void CShaderDeviceMgrBase::InvokeDeviceResetNotifications( IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentParameters, void *pHWnd )
{
	int nCount = m_DeviceDependentObjects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_DeviceDependentObjects[i]->DeviceReset( ( void * )pDevice, ( void * )pPresentParameters, pHWnd );
	}
}

//-----------------------------------------------------------------------------
// Factory to return from SetMode
//-----------------------------------------------------------------------------
void* CShaderDeviceMgrBase::ShaderInterfaceFactory( const char *pInterfaceName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}
	if ( !Q_stricmp( pInterfaceName, SHADER_DEVICE_INTERFACE_VERSION ) )
		return static_cast< IShaderDevice* >( g_pShaderDevice );
	if ( !Q_stricmp( pInterfaceName, SHADERAPI_INTERFACE_VERSION ) )
		return static_cast< IShaderAPI* >( g_pShaderAPI );
	if ( !Q_stricmp( pInterfaceName, SHADERSHADOW_INTERFACE_VERSION ) )
		return static_cast< IShaderShadow* >( g_pShaderShadow );

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
//
// The Base implementation of the shader device
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceBase::CShaderDeviceBase()
{
	m_bInitialized = false;
	m_nAdapter = -1;
	m_hWnd = NULL;
	m_hWndCookie = NULL;
	m_dwThreadId = ThreadGetCurrentId();
}

CShaderDeviceBase::~CShaderDeviceBase()
{
}

void CShaderDeviceBase::SetCurrentThreadAsOwner()
{
	m_dwThreadId = ThreadGetCurrentId();
}

void CShaderDeviceBase::RemoveThreadOwner()
{
	m_dwThreadId = 0xFFFFFFFF;
}

bool CShaderDeviceBase::ThreadOwnsDevice()
{
	if ( ThreadGetCurrentId() == m_dwThreadId )
		return true;
	return false;
}


// Methods of IShaderDevice
ImageFormat CShaderDeviceBase::GetBackBufferFormat() const
{
	return IMAGE_FORMAT_UNKNOWN;
}

int CShaderDeviceBase::StencilBufferBits() const
{
	return 0;
}

bool CShaderDeviceBase::IsAAEnabled() const
{
	return false;
}


//-----------------------------------------------------------------------------
// Methods for interprocess communication to release resources
//-----------------------------------------------------------------------------
#define MATERIAL_SYSTEM_WINDOW_ID		0xFEEDDEAD

#ifdef USE_ACTUAL_DX
static VD3DHWND GetTopmostParentWindow( VD3DHWND hWnd )
{
	// Find the parent window...
	VD3DHWND hParent = GetParent( hWnd );
	while ( hParent )
	{
		hWnd = hParent;
		hParent = GetParent( hWnd );
	}

	return hWnd;
}

static BOOL CALLBACK EnumChildWindowsProc( VD3DHWND hWnd, LPARAM lParam )
{
	int windowId = GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if (windowId == MATERIAL_SYSTEM_WINDOW_ID)
	{
		COPYDATASTRUCT copyData;
		copyData.dwData = lParam;
		copyData.cbData = 0;
		copyData.lpData = 0;

		SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&copyData);
	}
	return TRUE;
}

static BOOL CALLBACK EnumWindowsProc( VD3DHWND hWnd, LPARAM lParam )
{
	EnumChildWindows( hWnd, EnumChildWindowsProc, lParam );
	return TRUE;
}

static BOOL CALLBACK EnumWindowsProcNotThis( VD3DHWND hWnd, LPARAM lParam )
{
	if ( g_pShaderDevice && ( GetTopmostParentWindow( (VD3DHWND)g_pShaderDevice->GetIPCHWnd() ) == hWnd ) )
		return TRUE;

	EnumChildWindows( hWnd, EnumChildWindowsProc, lParam );
	return TRUE;
}
#endif

//-----------------------------------------------------------------------------
// Adds a hook to let us know when other instances are setting the mode
//-----------------------------------------------------------------------------

#ifdef STRICT
#define WINDOW_PROC WNDPROC
#else
#define WINDOW_PROC FARPROC
#endif

#ifdef USE_ACTUAL_DX
static LRESULT CALLBACK ShaderDX8WndProc(VD3DHWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
#if !defined( _X360 )
	// FIXME: Should these IPC messages tell when an app has focus or not?
	// If so, we'd want to totally disable the shader api layer when an app
	// doesn't have focus.

	// Look for the special IPC message that tells us we're trying to set
	// the mode....
	switch(msg)
	{
	case WM_COPYDATA:
		{
			if ( !g_pShaderDevice )
				break;

			COPYDATASTRUCT* pData = (COPYDATASTRUCT*)lParam;

			// that number is our magic cookie number
			if ( pData->dwData == CShaderDeviceBase::RELEASE_MESSAGE )
			{
				g_pShaderDevice->OtherAppInitializing(true);
			}
			else if ( pData->dwData == CShaderDeviceBase::REACQUIRE_MESSAGE )  
			{
				g_pShaderDevice->OtherAppInitializing(false);
			}
			else if ( pData->dwData == CShaderDeviceBase::EVICT_MESSAGE )  
			{
				g_pShaderDevice->EvictManagedResourcesInternal( );
			}
		}
		break;
	}

	return DefWindowProc( hWnd, msg, wParam, lParam );
#endif
}
#endif


//-----------------------------------------------------------------------------
// Install, remove ability to talk to other shaderapi apps
//-----------------------------------------------------------------------------
void CShaderDeviceBase::InstallWindowHook( void* hWnd )
{
	Assert( m_hWndCookie == NULL );
#ifdef USE_ACTUAL_DX
#if !defined( _X360 )
	VD3DHWND hParent = GetTopmostParentWindow( (VD3DHWND)hWnd );

	// Attach a child window to the parent; we're gonna store special info there
	// We can't use the USERDATA, cause other apps may want to use this.
	HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr( hParent, GWLP_HINSTANCE );
	WNDCLASS		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.style         = CS_NOCLOSE | CS_PARENTDC;
	wc.lpfnWndProc   = ShaderDX8WndProc;
	wc.hInstance     = hInst;
	wc.lpszClassName = "shaderdx8";

	// In case an old one is sitting around still...
	UnregisterClass( "shaderdx8", hInst );

	RegisterClass( &wc );

	// Create the window
	m_hWndCookie = CreateWindow( "shaderdx8", "shaderdx8", WS_CHILD, 
		0, 0, 0, 0, hParent, NULL, hInst, NULL );

	// Marks it as a material system window
	SetWindowLongPtr( (VD3DHWND)m_hWndCookie, GWLP_USERDATA, MATERIAL_SYSTEM_WINDOW_ID );
#endif
#endif
}

void CShaderDeviceBase::RemoveWindowHook( void* hWnd )
{
#ifdef USE_ACTUAL_DX
#if !defined( _X360 )
	if ( m_hWndCookie )
	{
		DestroyWindow( (VD3DHWND)m_hWndCookie ); 
		m_hWndCookie = 0;
	}

	VD3DHWND hParent = GetTopmostParentWindow( (VD3DHWND)hWnd );
	HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr( hParent, GWLP_HINSTANCE );
	UnregisterClass( "shaderdx8", hInst );
#endif
#endif
}


//-----------------------------------------------------------------------------
// Sends a message to other shaderapi applications
//-----------------------------------------------------------------------------
void CShaderDeviceBase::SendIPCMessage( IPCMessage_t msg )
{
#ifdef USE_ACTUAL_DX
#if !defined( _X360 )
	// Gotta send this to all windows, since we don't know which ones
	// are material system apps...
	if ( msg != EVICT_MESSAGE )
	{
		EnumWindows( EnumWindowsProc, (DWORD)msg );
	}
	else
	{
		EnumWindows( EnumWindowsProcNotThis, (DWORD)msg );
	}
#endif
#endif
}


//-----------------------------------------------------------------------------
// Find view
//-----------------------------------------------------------------------------
int CShaderDeviceBase::FindView( void* hWnd ) const
{
	/* FIXME: Is this necessary?
	// Look for the view in the list of views
	for (int i = m_Views.Count(); --i >= 0; )
	{
	if (m_Views[i].m_HWnd == (VD3DHWND)hwnd)
	return i;
	}
	*/
	return -1;
}

//-----------------------------------------------------------------------------
// Creates a child window
//-----------------------------------------------------------------------------
bool CShaderDeviceBase::AddView( void* hWnd )
{
	LOCK_SHADERAPI();
	/*
	// If we haven't created a device yet
	if (!Dx9Device())
		return false;

	// Make sure no duplicate hwnds...
	if (FindView(hwnd) >= 0)
		return false;

	// In this case, we need to create the device; this is our
	// default swap chain. This here says we're gonna use a part of the
	// existing buffer and just grab that.
	int view = m_Views.AddToTail();
	m_Views[view].m_HWnd = (VD3DHWND)hwnd;
	//	memcpy( &m_Views[view].m_PresentParamters, m_PresentParameters, sizeof(m_PresentParamters) );

	HRESULT hr;
	hr = Dx9Device()->CreateAdditionalSwapChain( &m_PresentParameters,
	&m_Views[view].m_pSwapChain );
	return !FAILED(hr);
	*/

	return true;
}

void CShaderDeviceBase::RemoveView( void* hWnd )
{
	LOCK_SHADERAPI();
	/*
	// Look for the view in the list of views
	int i = FindView(hwnd);
	if (i >= 0)
	{
	// FIXME		m_Views[i].m_pSwapChain->Release();
	m_Views.FastRemove(i);
	}
	*/
}

//-----------------------------------------------------------------------------
// Activates a child window
//-----------------------------------------------------------------------------
void CShaderDeviceBase::SetView( void* hWnd )
{
	LOCK_SHADERAPI();

	ShaderViewport_t viewport;
	g_pShaderAPI->GetViewports( &viewport, 1 );

	// Get the window (*not* client) rect of the view window
	m_ViewHWnd = (VD3DHWND)hWnd;
	GetWindowSize( m_nWindowWidth, m_nWindowHeight );

	// Reset the viewport (takes into account the view rect)
	// Don't need to set the viewport if it's not ready
	g_pShaderAPI->SetViewports( 1, &viewport );
}


//-----------------------------------------------------------------------------
// Gets the window size
//-----------------------------------------------------------------------------
void CShaderDeviceBase::GetWindowSize( int& nWidth, int& nHeight ) const
{
#if defined( USE_SDL )

	// this matches up to what the threaded material system does
	g_pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );

#else

	// If the window was minimized last time swap buffers happened, or if it's iconic now, 
	// return 0 size
#ifdef _WIN32
	if ( !m_bIsMinimized && !IsIconic( ( HWND )m_hWnd ) )
#else
	if ( !m_bIsMinimized && !IsIconic( (VD3DHWND)m_hWnd ) )
#endif
	{
		// NOTE: Use the 'current view' (which may be the same as the main window) 
		RECT rect;
#ifdef _WIN32
		GetClientRect( ( HWND )m_ViewHWnd, &rect );
#else
		toglGetClientRect( (VD3DHWND)m_ViewHWnd, &rect );
#endif
		nWidth = rect.right - rect.left;
		nHeight = rect.bottom - rect.top;
	}
	else
	{
		nWidth = nHeight = 0;
	}

#endif
}



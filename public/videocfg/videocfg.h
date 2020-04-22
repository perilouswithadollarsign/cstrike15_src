//===== Copyright © 2005-2008, Valve Corporation, All rights reserved. ======//
//
//
//===========================================================================//

#ifndef VIDEOCFG_H
#define VIDEOCFG_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier1/utlvector.h"
#include "shaderapi/IShaderDevice.h"

class KeyValues;

enum ConsoleSystemLevel_t
{
	CONSOLE_SYSTEM_LEVEL_360 = 360,
	CONSOLE_SYSTEM_LEVEL_PS3 = 361
};

struct VidMatConfigData_t
{
	char szFileName[MAX_PATH];
	char szPathID[64];
	KeyValues *pConfigKeys;
	int nVendorID;
	int nDeviceID;
	int nDXLevel;
	unsigned int nSystemMemory;
	unsigned int nVideoMemory;
	int nPhysicalScreenWidth;
	int nPhysicalScreenHeight;
	CUtlVector< ShaderDisplayMode_t > displayModes;
	bool bIsVideo;
};

enum CPULevel_t
{
	CPU_LEVEL_UNKNOWN = -1,

	CPU_LEVEL_LOW = 0,
	CPU_LEVEL_MEDIUM,
	CPU_LEVEL_HIGH,
	CPU_LEVEL_PC_COUNT,

	CPU_LEVEL_360 = CPU_LEVEL_PC_COUNT,
	CPU_LEVEL_PS3 = CPU_LEVEL_360 + 1,
	CPU_LEVEL_COUNT,

	CPU_LEVEL_BIT_COUNT = 3,
};

enum GPULevel_t
{
	GPU_LEVEL_UNKNOWN = -1,

	GPU_LEVEL_LOW = 0,
	GPU_LEVEL_MEDIUM,
	GPU_LEVEL_HIGH,
	GPU_LEVEL_VERYHIGH,
	GPU_LEVEL_PC_COUNT,

	GPU_LEVEL_360 = GPU_LEVEL_PC_COUNT,
	GPU_LEVEL_PS3 = GPU_LEVEL_360 + 1,
	GPU_LEVEL_COUNT,

	GPU_LEVEL_BIT_COUNT = 3,
};

enum MemLevel_t
{
	MEM_LEVEL_UNKNOWN = -1,

	MEM_LEVEL_LOW = 0,
	MEM_LEVEL_MEDIUM,
	MEM_LEVEL_HIGH,
	MEM_LEVEL_PC_COUNT,

	MEM_LEVEL_360 = MEM_LEVEL_PC_COUNT,
	MEM_LEVEL_PS3 = MEM_LEVEL_360 + 1,
	MEM_LEVEL_COUNT,

	MEM_LEVEL_BIT_COUNT = 3,
};

enum GPUMemLevel_t
{
	GPU_MEM_LEVEL_UNKNOWN = -1,

	GPU_MEM_LEVEL_LOW = 0,
	GPU_MEM_LEVEL_MEDIUM,
	GPU_MEM_LEVEL_HIGH,
	GPU_MEM_LEVEL_PC_COUNT,

	GPU_MEM_LEVEL_360 = GPU_MEM_LEVEL_PC_COUNT,
	GPU_MEM_LEVEL_PS3 = GPU_MEM_LEVEL_360 + 1,
	GPU_MEM_LEVEL_COUNT,

	GPU_MEM_LEVEL_BIT_COUNT = 3,
};

bool RecommendedConfig( VidMatConfigData_t &configData );
bool ResetVideoConfigToDefaults( KeyValues *pConfigKeys = NULL );
bool UpdateVideoConfigConVars( KeyValues *pConfigKeys = NULL );

bool ReadCurrentVideoConfig( KeyValues *pConfigKeys, bool bDefault = false );
bool UpdateCurrentVideoConfig( int nWidth, int nHeight, int nAspectRatioMode, bool bFullscreen, bool bNoWindowBorder, bool bUseRestartConvars = false );
void UpdateSystemLevel( int nCPULevel, int nGPULevel, int nMemLevel, int nGPUMemLevel, bool bVGUIIsSplitscreen, const char *pModName );

#endif // VIDEOCFG_H
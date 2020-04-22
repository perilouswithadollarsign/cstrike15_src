//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================//

#ifndef MATERIALSYSTEM_GLOBAL_H
#define MATERIALSYSTEM_GLOBAL_H

#ifdef _WIN32
#pragma once
#endif

#include "imaterialsysteminternal.h"
#include "tier0/dbg.h"
#include "tier2/tier2.h"

#if defined( _PS3 ) || defined( _OSX )
#include "shaderapidx9/shaderapidx8.h"
#include "shaderapidx9/shaderdevicedx8.h"
#include "shaderapidx9/hardwareconfig.h"
#include "shaderapidx9/shaderapidx8_global.h"
#include "shaderapidx9/shadershadowdx8.h"
#endif

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;
class IShaderAPI;
class IHardwareConfigInternal;
class IShaderUtil;
class IShaderShadow;
class IShaderDeviceMgr;
class IShaderDevice;
class IShaderSystemInternal;
class IMaterialInternal;
class IColorCorrectionSystem;
class IMaterialVar;
class IClientMaterialSystem;


//-----------------------------------------------------------------------------
// Constants used by the system
//-----------------------------------------------------------------------------

#define MATERIAL_MAX_PATH 256

// GR - limits for blured image (HDR stuff)
#define MAX_BLUR_IMAGE_WIDTH  256
#define MAX_BLUR_IMAGE_HEIGHT 192

#define CLAMP_BLUR_IMAGE_WIDTH( _w ) ( ( _w < MAX_BLUR_IMAGE_WIDTH ) ? _w : MAX_BLUR_IMAGE_WIDTH )
#define CLAMP_BLUR_IMAGE_HEIGHT( _h ) ( ( _h < MAX_BLUR_IMAGE_HEIGHT ) ? _h : MAX_BLUR_IMAGE_HEIGHT )

//-----------------------------------------------------------------------------
// Global structures
//-----------------------------------------------------------------------------
extern MaterialSystem_Config_t g_config;
extern uint32 g_nDebugVarsSignature;

//extern MaterialSystem_ErrorFunc_t	Error;
//extern MaterialSystem_WarningFunc_t Warning;

extern int				g_FrameNum;


#ifndef SHADERAPI_GLOBAL_H
extern IShaderAPI*	g_pShaderAPI;
extern IShaderDeviceMgr* g_pShaderDeviceMgr;
extern IShaderDevice*	g_pShaderDevice;
extern IShaderShadow* g_pShaderShadow;
#endif
extern IClientMaterialSystem *g_pClientMaterialSystem;

extern IMaterialInternal *g_pErrorMaterial;

IShaderSystemInternal* ShaderSystem();
inline IShaderSystemInternal* ShaderSystem()
{
	extern IShaderSystemInternal *g_pShaderSystem;
	return g_pShaderSystem;
}

#ifdef _PS3
#include "shaderapidx9/hardwareconfig_ps3nonvirt.h"
#elif !defined( _OSX )
inline IHardwareConfigInternal *HardwareConfig()
{
	extern IHardwareConfigInternal* g_pHWConfig;
	return g_pHWConfig;
}
#endif

#if defined( INCLUDE_SCALEFORM )
inline IScaleformUI* ScaleformUI()
{
	extern IScaleformUI* g_pScaleformUI;
	return g_pScaleformUI;
}
#endif

//-----------------------------------------------------------------------------
// Accessor to get at the material system
//-----------------------------------------------------------------------------
inline IMaterialSystemInternal* MaterialSystem()
{
	extern IMaterialSystemInternal *g_pInternalMaterialSystem;
	return g_pInternalMaterialSystem;
}

#ifndef SHADERAPI_GLOBAL_H
inline IShaderUtil* ShaderUtil()
{
	extern IShaderUtil *g_pShaderUtil;
	return g_pShaderUtil;
}
#endif

extern IColorCorrectionSystem *g_pColorCorrectionSystem;
inline IColorCorrectionSystem *ColorCorrectionSystem()
{
	return g_pColorCorrectionSystem;
}


//-----------------------------------------------------------------------------
// Global methods related to material vars
//-----------------------------------------------------------------------------
void EnableThreadedMaterialVarAccess( bool bEnable, IMaterialVar **ppParams, int nVarCount );


#endif // MATERIALSYSTEM_GLOBAL_H

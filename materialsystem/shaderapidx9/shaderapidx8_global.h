//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPIDX8_GLOBAL_H
#define SHADERAPIDX8_GLOBAL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "shaderapi_global.h"
#include "tier2/tier2.h"
#include "shaderdevicedx8.h"


//-----------------------------------------------------------------------------
// Use this to fill in structures with the current board state
//-----------------------------------------------------------------------------

#ifdef _DEBUG
#define DEBUG_BOARD_STATE 0
#endif

#if !defined( _GAMECONSOLE )
#include "d3d_async.h"
typedef D3DDeviceWrapper D3DDev_t;
D3DDev_t *Dx9Device();
IDirect3D9 *D3D();
#else
#define SHADERAPI_NO_D3DDeviceWrapper 1
typedef IDirect3DDevice D3DDeviceWrapper;
#endif


//-----------------------------------------------------------------------------
// Measures driver allocations
//-----------------------------------------------------------------------------
//#define MEASURE_DRIVER_ALLOCATIONS 1


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IShaderUtil;
class IVertexBufferDX8;
class IShaderShadowDX8;
class IMeshMgr;
class IShaderAPIDX8;
class IFileSystem;
class IShaderManager;


//-----------------------------------------------------------------------------
// A new shader draw flag we need to workaround dx8
//-----------------------------------------------------------------------------
enum
{
	SHADER_HAS_CONSTANT_COLOR = 0x80000000
};

//-----------------------------------------------------------------------------
// The main shader API
//-----------------------------------------------------------------------------
extern IShaderAPIDX8 *g_pShaderAPIDX8;
#ifdef _PS3
class CPs3NonVirt_IShaderAPIDX8;
inline CPs3NonVirt_IShaderAPIDX8* ShaderAPI()
{
	return ( CPs3NonVirt_IShaderAPIDX8 * ) 1;
}
#else
inline IShaderAPIDX8* ShaderAPI()
{
	return g_pShaderAPIDX8;
}
#endif

//-----------------------------------------------------------------------------
// The shader shadow
//-----------------------------------------------------------------------------
IShaderShadowDX8* ShaderShadow();

//-----------------------------------------------------------------------------
// Manager of all vertex + pixel shaders
//-----------------------------------------------------------------------------
inline IShaderManager *ShaderManager()
{
	extern IShaderManager *g_pShaderManager;
	return g_pShaderManager;
}

//-----------------------------------------------------------------------------
// The mesh manager
//-----------------------------------------------------------------------------
IMeshMgr* MeshMgr();

//-----------------------------------------------------------------------------
// The main hardware config interface
//-----------------------------------------------------------------------------
#ifdef _PS3
#include "shaderapidx9/hardwareconfig_ps3nonvirt.h"
#elif defined( _OSX )
// @wge: Moved from materialsystem_global.h, since we include shaderapidx8 headers in material system
inline IHardwareConfigInternal *HardwareConfig()
{
	extern IHardwareConfigInternal* g_pHWConfig;
	return g_pHWConfig;
}
#else
inline IMaterialSystemHardwareConfig* HardwareConfig()
{	
	return g_pMaterialSystemHardwareConfig;
}
#endif


#endif // SHADERAPIDX8_GLOBAL_H

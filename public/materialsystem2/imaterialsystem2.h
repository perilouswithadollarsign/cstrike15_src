//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef IMATERIALSYSTEM2
#define IMATERIALSYSTEM2

#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include "materialsystem2/imaterial2.h"

// TODO: Split out common enums and typedefs out of irenderdevice.h and include the lighter-weight .h file here
#include "rendersystem/irenderdevice.h"

//---------------------------------------------------------------------------------------------------------------------------------------------------
// Forward declarations
//---------------------------------------------------------------------------------------------------------------------------------------------------
class IRenderContext;
class IRenderHardwareConfig;

//---------------------------------------------------------------------------------------------------------------------------------------------------
// The new material system
//---------------------------------------------------------------------------------------------------------------------------------------------------
abstract_class IMaterialSystem2 : public IAppSystem
{
public:
	// This must be called before anything else in this interface! If you don't need modes, then call it with an array of { "", NULL } which is the
	// same as not using modes at all in your vfx shader files. The error shader is setup in this function, so it must be called! Last element must be NULL!
	// NOTE: Most callers will want the first mode string to be "" which is the default mode where the mode section in the .vfx is ignored
	virtual void SetModeStrings( const char **pModeStrings ) = 0;

	// If material cannot load, it will still return the error material. You can find out if you have that material by calling IMaterial2::IsErrorMaterial()
	virtual IMaterial2 *FindOrCreateMaterialFromVmt( const char *pVmtFileName, const char *pTextureGroupName, RenderSystemAssetFileLoadMode_t loadMode = LOADMODE_IMMEDIATE ) = 0;

	// renderablePass is created by IMaterial2->IMaterialMode->ComputeRenderablePassesForContext()
	virtual void SetRenderStateForRenderablePass( IRenderContext &renderContext, RenderInputLayout_t hInputLayout, const MaterialRenderablePass_t &renderablePass ) const = 0;

	// This will free all materials, shaders, constant buffers, textures, etc. that are ref counted to 0
	virtual void FreeAllUnreferencedData() = 0;

	// Must be called when no one else is calling into the material system or any IMaterial2
	virtual void FrameUpdate() = 0;

	// Temp interface to reload shaders (This will eventually live inside the material system but we don't have concommands right now)
	virtual void DynamicShaderCompile_ReloadAllShaders() = 0;
};

#endif // IMATERIALSYSTEM2

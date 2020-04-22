//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef IMATERIAL2_H
#define IMATERIAL2_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/utlsymbol.h"

// TODO: Split out common enums and typedefs out of irenderdevice.h and include the lighter-weight .h file here
#include "rendersystem/irenderdevice.h"

/* Example API usage
	IMaterialMode *pMode = pMaterial->GetMode( 0 );
	if ( pMode != NULL ) // NULL if unsupported mode
	{
		MaterialRenderablePass_t renderablePassArray[ MATERIAL_RENDERABLE_PASS_MAX ];
		int nNumPasses = pMode->ComputeRenderablePassesForContext( *pRenderContext, renderablePassArray );
		for ( int i = 0; i < nNumPasses; i++ )
		{
			g_pMaterialSystem2->SetRenderStateForRenderablePass( *pRenderContext, g_hLayout, renderablePassArray[i] );
			pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, 6 );
		}
	}
//*/

//---------------------------------------------------------------------------------------------------------------------------------------------------
// Forward declarations
//---------------------------------------------------------------------------------------------------------------------------------------------------
class IRenderContext;

//---------------------------------------------------------------------------------------------------------------------------------------------------
abstract_class IMaterialLayer
{
public:
	// Need calls to get shader handle and anything else that will help the renderer bucket passes

};

//---------------------------------------------------------------------------------------------------------------------------------------------------
#define MATERIAL_RENDERABLE_PASS_MAX 8 // For code that wants to hard-code the array size to pass into ComputeRenderablePassesForContext() below

enum MaterialShaderProgram_t
{
	MATERIAL_SHADER_PROGRAM_VS = 0,
	MATERIAL_SHADER_PROGRAM_GS,
	MATERIAL_SHADER_PROGRAM_PS,

	MATERIAL_SHADER_PROGRAM_MAX,
};

struct MaterialRenderablePass_t
{
	const IMaterialLayer *pLayer;
	//MaterialDataFromClient *data;

	// TODO: This data needs to be compacted
	int nLayerIndex;
	int nPassIndex;

	// TODO: Change this to shader handles
	uint64 staticComboIdArray[ MATERIAL_SHADER_PROGRAM_MAX ]; // Should these be a 16-bit index to a post-skipped combo or anything smaller than 64-bits?
	uint64 dynamicComboIdArray[ MATERIAL_SHADER_PROGRAM_MAX ];
};

//---------------------------------------------------------------------------------------------------------------------------------------------------
abstract_class IMaterialMode
{
public:
	// This gets the max number of passes this mode will ever render
	virtual int GetTotalNumPasses() const = 0;

	// This must be called to converge on a dynamic combo before calling g_pMaterialSystem2->SetRenderStateForRenderablePass().
	// The array size must be at least GetTotalNumPasses() large, but using MATERIAL_RENDERABLE_PASS_MAX will always work.
	// NOTE: If you pass in a NULL pRenderContext, it will return all possible passes
	virtual int ComputeRenderablePassesForContext( IRenderContext &renderContext, MaterialRenderablePass_t *pRenderablePassArray, int nRenderablePassArraySize = MATERIAL_RENDERABLE_PASS_MAX ) = 0;
};

//---------------------------------------------------------------------------------------------------------------------------------------------------
// Material interface
//---------------------------------------------------------------------------------------------------------------------------------------------------
abstract_class IMaterial2
{
public:
	// Get the name of the material. This is a full path to the vmt file starting from "mod/materials" without a file extension
	virtual const char *GetName() const = 0;
	virtual CUtlSymbol GetNameSymbol() const = 0;

	// When async loading, this will let the caller know everything is loaded
	virtual bool IsLoaded() const = 0;

	// If there was a problem loading the material, this will reference the error material internally
	virtual bool IsErrorMaterial() const = 0;

	// The number of modes is constant for all materials and is defined by the client and stored in the material system.
	// Returning NULL means this is an unsupported mode. A non-NULL mode can still have 0 passes depending on game state, so
	// NULL here means the mode isn't supported by this material and it is up to the caller to decide what to do.
	// For a simple renderer that doesn't create custom render modes in the material system, GetMode(0) is always valid.
	virtual IMaterialMode *GetMode( int nMode ) = 0;

	// Decrement the ref count on the material. The memory will eventually be freed if the ref count hits 0.
	virtual void Release() = 0;
};

#endif // IMATERIAL2_H

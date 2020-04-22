//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IMESHSYSTEM_H
#define IMESHSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/IAppSystem.h"
#include "rendersystem/schema/renderbuffer.g.h"
#include "rendersystem/schema/renderable.g.h"
#include "rendersystem/irendercontext.h"
#include "mathlib/camera.h"

//-----------------------------------------------------------------------------
// Methods related to rendering meshes
//-----------------------------------------------------------------------------
abstract_class IMeshSystem : public IAppSystem
{
public:
	virtual HRenderable FindOrCreateFileRenderable( const char *pFileName, RenderSystemAssetFileLoadMode_t nLoadMode = LOADMODE_ASYNCHRONOUS ) = 0;
	virtual void DrawRenderable( IRenderContext *pRenderContext, HRenderable hRenderable, RenderShaderHandle_t hVS, RenderShaderHandle_t hPS, int nExplicitInstanceCount = 0 ) = 0;
	virtual const Renderable_t *GetRenderableData( HRenderable hRenderable ) = 0;
	virtual const PermRenderableBounds_t *GetPermRenderableData( HRenderable hRenderable ) = 0;
	virtual bool RenderableIntersectsFrustum( HRenderable hRenderable, CFrustum *pFrustum, Vector &vOrigin ) = 0;
	virtual bool IsFullyCached( HRenderable hRenderable ) = 0;
	virtual void CacheRenderable( HRenderable hRenderable ) = 0;
};


#endif // IMESHSYSTEM_H

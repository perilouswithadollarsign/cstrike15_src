//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef SCENEVIEW_H
#define SCENEVIEW_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/camera.h"
#include "tier1/utlvector.h"
#include "tier1/utlintrusivelist.h"
#include "rendersystem/irendercontext.h"

// to use the scenesystem, you feed it a bunch of CSceneViews to render. More CSceneViews may be
// generated during rendering as a result of things such as lights, etc.


enum ELayerType 
{
	LAYERTYPE_DRAW_WORLDOBJECTS,							// this layer causes a world-graph traversal producing an object list
	LAYERTYPE_PROCEDURAL,
};



enum ELayerFlags
{
	LAYERFLAG_NEEDS_FULLSORT = 1,
	LAYERFLAGS_CLEAR_RENDERTARGET = 2,
};

class CSceneDrawList;

struct CStandardTextureBindingRecord_t
{
	StandardTextureID_t m_nID;
	HRenderTexture m_hTexture;
};


class CSceneLayer
{
public:
	RenderViewport_t m_viewport;

	ELayerType m_eLayerType;
	MaterialDrawMode_t m_eShaderMode;
	int m_nShadingMode;										// mode to use for binding materials when rendering into this layer
	uint m_nObjectFlagsRequiredMask;
	uint m_nObjectFlagsExcludedMask;
	uint m_nLayerFlags;										// LAYERFLAG_xxx
	Vector4D m_vecClearColor;
	int m_nClearFlags;
	int m_nLayerIndex;
	RenderTargetBinding_t m_hRenderTargetBinding;
	LAYERDRAWFN m_pRenderProcedure;
	CUtlIntrusiveList<CSceneDrawList> m_pendingDrawList;
	
	CUtlIntrusiveList<CDisplayList> m_renderLists;

	CUtlVectorFixed<CStandardTextureBindingRecord_t, STDTEXTURE_NUMBER_OF_IDS> m_standardBindings;

	FORCEINLINE void AddStandardTextureBinding( StandardTextureID_t nID, HRenderTexture hTexture );

};

FORCEINLINE void CSceneLayer::AddStandardTextureBinding( StandardTextureID_t nID, HRenderTexture hTexture )
{
	int nSlot = m_standardBindings.AddToTail();
	m_standardBindings[nSlot].m_nID = nID;
	m_standardBindings[nSlot].m_hTexture = hTexture;
}

// constant buffer layout for camera constants. needs to be moved somewhere where shaders can see it.
struct ViewRelatedConstants_t
{
	VMatrix m_mViewProjection;
	Vector4D m_vEyePt;
	Vector4D m_vEyeDir;
	Vector4D m_flFarPlane;
};

// reserved constant slots. needs to go somewhere in the new mat/shader system
#define VERTEX_CONSTANT_SLOT_VIEWRELATED_BASE 0



#endif //sceneview_h

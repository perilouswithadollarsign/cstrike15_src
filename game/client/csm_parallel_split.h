//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef CSM_PARALLEL_SPLIT_H
#define CSM_PARALLEL_SPLIT_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/volumeculler.h"
#include "mathlib/camera.h"

namespace CCSMFrustumDefinition
{
	enum { NUM_FRUSTUM_VERTS = 8 };
	enum { NUM_FRUSTUM_QUADS = 6 };
	enum { NUM_FRUSTUM_PLANES = NUM_FRUSTUM_QUADS };
	enum { NUM_FRUSTUM_EDGES = 12 };
	enum { NUM_FRUSTUM_LINES = 12 };

	extern Vector4D g_vProjFrustumVerts[NUM_FRUSTUM_VERTS];
	extern const uint8 g_nFrustumLineVertIndices[24];

	// The vertices of each frustum quad
	extern const uint8 g_nQuadVertIndices[NUM_FRUSTUM_QUADS][4];

	// The vertices of each frustum edge
	extern const uint8 g_nEdgeVertIndices[NUM_FRUSTUM_EDGES][2];

	// The quads sharing each frustum edge
	extern const uint8 g_nEdgeQuadIndices[NUM_FRUSTUM_EDGES][2];
} // namespace CCSMFrustumDefinition

enum 
{ 
	// Use caution changing this constant. There are a number of assumptions made about the max # of cascades being 4 (such as the manually laid out shader constants).
	MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE = 4
};

struct ShadowFrustaDebugInfo_t
{
	uint m_nNumWorldFocusVerts;

	enum { MAX_WORLD_FOCUS_POINTS = 9 };
	Vector m_WorldFocusVerts[MAX_WORLD_FOCUS_POINTS];

	float m_flSplitPlaneDistance;

	float m_flLeft, m_flRight;
	float m_flTop, m_flBottom;
	float m_flZNear, m_flZFar;
};

struct SunLightState_t
{
	uint m_nShadowCascadeSize;
       		
	CFrustum m_CascadeFrustums[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];	
	CVolumeCuller m_CascadeVolumeCullers[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	VMatrix m_CascadeProjToTexMatrices[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	Rect_t m_CascadeViewports[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	VPlane m_CascadeFrustumPlanes[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ][6];
	
	CascadedShadowMappingState_t m_SunLightShaderParams;

	ShadowFrustaDebugInfo_t m_DebugInfo[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];

	float m_flActualMaxShadowDist;
	float m_flZLerpStartDist;
	float m_flZLerpEndDist;
};

struct SunLightViewState_t
{
	Vector m_Direction;
	color32 m_LightColor;
	int m_LightColorScale;
	CFrustum m_frustum;
	int m_nMaxCascadeSize;
	int m_nAtlasFirstCascadeIndex;
	bool m_bSetAllCascadesToFirst;
	float m_flMaxVisibleDist;
	float m_flMaxShadowDist;
	int m_nCSMQualityLevel;
};

class CCSMParallelSplit : public CAlignedNewDelete<>
{
public:
	CCSMParallelSplit();
	~CCSMParallelSplit();
	
	CCSMParallelSplit( const CCSMParallelSplit &other );
	const CCSMParallelSplit &operator==( const CCSMParallelSplit &rhs );
	
	void Init( uint nMaxShadowBufferSize, uint nMaxCascadeSize );
	void Clear();
	
	bool Update( const SunLightViewState_t &viewState );

	bool IsValid() const { return m_bValid; }
	inline void Reset() { m_bValid = false; }
			
	const SunLightViewState_t& GetViewState() const { return m_viewState; }
	SunLightViewState_t& GetViewState() { return m_viewState; }

	const SunLightState_t &GetLightState() const { return m_lightState; }
	SunLightState_t &GetLightState() { return m_lightState; }
                    		
private:
	
	void CalculateShadowFrustaParallelSplits( const CFrustum &sceneFrustum, uint &nOutCascadeSize, CFrustum *pOutFrustums, CVolumeCuller *pOutVolumeCullers, float &flOutActualMaxShadowDist, int nMaxCascadeSize, Vector vLightDir, float flMaxShadowDistance, float flMaxVisibleDistance, int nCSMQualityLevel, ShadowFrustaDebugInfo_t* pDebugInfo );
	uint ComputeCullingVolumePlanes( VPlane *pOutPlanes, Vector vDirToLight, const CFrustum &sceneFrustum, float flMaxDistBetweenAnyCasterAndReceiver );

	void ComputeCascadeProjToTexMatrices( SunLightState_t &lightState );
		
	static float CalculateSplitPlaneDistance( int nSplit, int nShadowSplits, float flMaxShadowDistance, float flZNear, float flZFar );
	
	float ComputeFarPlaneCameraRelativePoints( Vector *pPoints, float flZNear, float flZFar, const CFrustum &frustum );

	bool m_bValid;		
	
	uint m_nShadowBufferSize;			// resolution of a single shadow map
	uint m_nShadowCascadeMaxSize;		// cascade size (number of shadow maps)

	uint m_nShadowAtlasWidth;			// actual shadow texture resolution
	uint m_nShadowAtlasHeight;
	
	SunLightViewState_t m_viewState;
	
	SunLightState_t m_lightState;
};

#endif //CSM_PARALLEL_SPLIT_H


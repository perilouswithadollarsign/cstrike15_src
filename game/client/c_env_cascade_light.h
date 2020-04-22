//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Directional lighting with cascaded shadow mapping entity.
//
// $NoKeywords: $
//=============================================================================//
#ifndef C_ENV_CASCADE_LIGHT_H
#define C_ENV_CASCADE_LIGHT_H

#include "c_baseplayer.h"
#include "csm_parallel_split.h"

//------------------------------------------------------------------------------
// Purpose: Directional lighting with cascaded shadow mapping entity.
//------------------------------------------------------------------------------
class C_CascadeLight : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_CascadeLight, C_BaseEntity );
		
	DECLARE_CLIENTCLASS();

	C_CascadeLight();

	virtual ~C_CascadeLight();

	static C_CascadeLight *Get() { return m_pCascadeLight; }

	virtual void Spawn();
	virtual void Release();
	virtual bool ShouldDraw();
	virtual void ClientThink();

	inline const Vector &GetShadowDirection() const { return m_shadowDirection; }
	inline const Vector &GetEnvLightShadowDirection() const { return m_envLightShadowDirection; }
	inline bool IsEnabled() const { return m_bEnabled; }
	inline color32 GetColor() const { return m_LightColor; }
	inline int GetColorScale() const { return m_LightColorScale; }
	inline bool UseLightEnvAngles() const { return m_bUseLightEnvAngles; }
	float GetMaxShadowDist() const { return m_flMaxShadowDist; }

private:
	static C_CascadeLight *m_pCascadeLight;

	Vector m_shadowDirection;
	Vector m_envLightShadowDirection;
	bool m_bEnabled;
	bool m_bUseLightEnvAngles;
	color32	m_LightColor;
	int	m_LightColorScale;
	float m_flMaxShadowDist;
};

class CDebugPrimRenderer2D
{
public:
	CDebugPrimRenderer2D();

	void Clear();

	// Normalized [0,1] coords, where (0,0) is upper left
	void AddNormalizedLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b );
	
	// Screenspace (pixel) coords
	void AddScreenspaceLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b );
	void AddScreenspaceRect2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b );
	void AddScreenspaceLineList2D( uint nCount, const Vector2D *pVerts, const VertexColor_t &color );
	void AddScreenspaceWireframeFrustum2D( const VMatrix &xform, const VertexColor_t &color, bool bShowAxes );
		
	void Render2D( );

	void RenderScreenspaceDepthTexture( float sx, float sy, float ex, float ey, float su, float sv, float eu, float ev, CTextureReference &depthTex, float zLo, float zHi );

private:
	class CDebugLine
	{
	public:
		inline CDebugLine() { }
		inline CDebugLine( const Vector2D &start, const Vector2D &end, uint r = 255, uint g = 255, uint b = 255, uint a = 255 ) { Init( start, end, r, g, b, a ); }

		inline void Init( const Vector2D &start, const Vector2D &end, uint r = 255, uint g = 255, uint b = 255, uint a = 255 ) { m_EndPoints[0] = start; m_EndPoints[1] = end; m_nColor[0] = r; m_nColor[1] = g; m_nColor[2] = b; m_nColor[3] = a; }

		Vector2D m_EndPoints[2];		// normalized [0,1] coordinates, (0,0) is top-left
		uint8 m_nColor[4];
	};
	CUtlVector< CDebugLine > m_debugLines;

	void RenderDebugLines2D( uint nNumLines, const CDebugLine *pLines );
};

class CCascadeLightManager : public CAutoGameSystemPerFrame
{
public:
	CCascadeLightManager();
	virtual ~CCascadeLightManager();

	bool InitRenderTargets();
	void ShutdownRenderTargets();

	void LevelInitPreEntity();
	void LevelInitPostEntity();
	void LevelShutdownPreEntity();
	void LevelShutdownPostEntity();

	void Shutdown();
	
	bool IsEnabled() const;
	bool IsEnabledAndActive() const;

	virtual void PreRender ();
	
	void ComputeShadowDepthTextures( const CViewSetup &viewSetup, bool bSetup = false );
	void UnlockAllShadowDepthTextures();

	void BeginViewModelRendering();
	void EndViewModelRendering();

	void BeginReflectionView();
	void EndReflectionView();

	void Draw3DDebugInfo();
	void Draw2DDebugInfo();

	void DumpStatus();

	CSMQualityMode_t GetCSMQualityMode();

public:
	static void RotXPlusDown( const CCommand &args );
	static void RotXPlusUp( const CCommand &args );
	static void RotXNegDown( const CCommand &args );
	static void RotXNegUp( const CCommand &args );
	static void RotYPlusDown( const CCommand &args );
	static void RotYPlusUp( const CCommand &args );
	static void RotYNegDown( const CCommand &args );
	static void RotYNegUp( const CCommand &args );

private:
	class CFullCSMState
	{
	public:
		CFullCSMState()
		{
			Clear();
		}

		inline void Clear()
		{
			V_memset( &m_sceneFrustum, 0, sizeof( m_sceneFrustum ) );
			m_flSceneAspectRatio = 0.0f;
			m_sceneWorldToView.Identity();
			m_sceneViewToProj.Identity();
			m_sceneWorldToProj.Identity();
			m_CSMParallelSplit.Clear();
			m_shadowDir.Init( 0, 0, -1.0f );
			m_flMaxShadowDist = 0.0f;
			m_flMaxVisibleDist = 0.0f;
			m_nMaxCascadeSize = 0;
			m_bValid = false;
		}

		void Update( const CViewSetup &viewSetup, const Vector &shadowDir, const color32 lightColor, const int lightColorScale, float flMaxShadowDist, float flMaxVisibleDist, uint nMaxCascadeSize, uint nAtlasFirstCascadeIndex, int nCSMQualityLevel, bool bSetAllCascadesToFirst );

		bool IsValid() const { return m_CSMParallelSplit.IsValid() && m_CSMParallelSplit.GetLightState().m_nShadowCascadeSize; }
		void Reset() { m_CSMParallelSplit.Reset(); }

		CFrustum m_sceneFrustum;
		float m_flSceneAspectRatio;
		VMatrix	m_sceneWorldToView, m_sceneViewToProj, m_sceneWorldToProj;
		Vector m_shadowDir;
		float m_flMaxShadowDist;
		float m_flMaxVisibleDist;
		uint m_nMaxCascadeSize;
		CCSMParallelSplit m_CSMParallelSplit;

		bool m_bValid;
	};

	void RenderViews( CFullCSMState &state,  bool bIncludeViewModels );

	int m_nDepthTextureResolution;
	CSMQualityMode_t m_nCurRenderTargetQualityMode;

	CFullCSMState m_curState;
	CFullCSMState m_curViewModelState;

	CFullCSMState m_capturedState;
	
	CDebugPrimRenderer2D m_debugPrimRenderer;

	bool m_bRenderTargetsAllocated;
	CTextureReference m_ShadowDepthTexture;
	CTextureReference m_DummyColorTexture;

	float m_flRotX[2], m_flRotY[2];

	bool m_bCSMIsActive;
	bool m_bStateIsValid;

	void DeinitRenderTargets();
	void DrawTextDebugInfo();
	Vector GetShadowDirection();
	inline CFullCSMState &GetActiveState() { return m_capturedState.m_CSMParallelSplit.IsValid() ? m_capturedState : m_curState; }
};

extern CCascadeLightManager g_CascadeLightManager;

#endif // C_ENV_CASCADE_LIGHT_H

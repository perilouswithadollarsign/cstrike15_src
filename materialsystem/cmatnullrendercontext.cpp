//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#include "cmatnullrendercontext.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif
#include "cmatrendercontext.h"
#include "itextureinternal.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CMatNullRenderContext : public CMatRenderContextBase
{
public:
	CMatNullRenderContext()
		:	m_WidthBackBuffer( 0 ), 
			m_HeightBackBuffer( 0 )
	{
	}

	virtual void InitializeFrom( CMatRenderContextBase *pInitialState )
	{
		CMatRenderContextBase::InitializeFrom( pInitialState );
		g_pShaderAPI->GetBackBufferDimensions( m_WidthBackBuffer, m_HeightBackBuffer );
	}

	void BeginRender()
	{
	}

	void EndRender()
	{
	}

	void Flush(bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetRenderTargetDimensions(int &,int &) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DepthRange(float,float)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearBuffers(bool,bool,bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixels(int,int,int,int,unsigned char *,ImageFormat, ITexture *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixelsAsync(int,int,int,int,unsigned char *,ImageFormat, ITexture *, CThreadEvent *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixelsAsyncGetResult(int,int,int,int,unsigned char *,ImageFormat,CThreadEvent *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetLightingState( const MaterialLightingState_t &state )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetLights( int nCount, const LightDesc_t *pDesc )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetLightingOrigin( Vector vLightingOrigin )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetAmbientLightCube(Vector4D [])
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CopyRenderTargetToTexture(ITexture *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFrameBufferCopyTexture(ITexture *,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	ITexture *GetFrameBufferCopyTexture(int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	void GetViewport( int& x, int& y, int& width, int& height ) const
	{
		// Verify valid top of RT stack
		Assert ( m_RenderTargetStack.Count() > 0 );

		// Grab the top of stack
		const RenderTargetStackElement_t& element = m_RenderTargetStack.Top();

		// If either dimension is negative, set to full bounds of current target
		if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
		{
			// Viewport origin at target origin
			x = y = 0;

			// If target is back buffer
			if ( element.m_pRenderTargets[0] == NULL )
			{
				width = m_WidthBackBuffer;
				height = m_HeightBackBuffer;
			}
			else // if target is texture
			{
				width = element.m_pRenderTargets[0]->GetActualWidth();
				height = element.m_pRenderTargets[0]->GetActualHeight();
			}
		}
		else // use the bounds from the stack directly
		{
			x = element.m_nViewX;
			y = element.m_nViewY;
			width = element.m_nViewW;
			height = element.m_nViewH;
		}
	}

	void BeginGeneratingCSMs()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndGeneratingCSMs()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CullMode(MaterialCullMode_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FlipCullMode( void )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogMode(MaterialFogMode_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogStart(float)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogEnd(float)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFogZ(float)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	MaterialFogMode_t GetFogMode()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return MATERIAL_FOG_NONE;
	}

	int	GetCurrentNumBones( ) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void FogColor3f(float,float,float)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogColor3fv(const float *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogColor3ub(unsigned char,unsigned char,unsigned char)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogColor3ubv(const unsigned char *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetFogColor(unsigned char *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetNumBoneWeights(int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	IMesh *CreateStaticMesh(VertexFormat_t,const char *,IMaterial *,VertexStreamSpec_t *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	void DestroyStaticMesh(IMesh *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	IMesh *GetDynamicMesh(bool,IMesh *,IMesh *,IMaterial *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	virtual IMesh* GetDynamicMeshEx( VertexFormat_t, bool, IMesh*, IMesh*, IMaterial * )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	int SelectionMode(bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void SelectionBuffer(unsigned int *,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearSelectionNames()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void LoadSelectionName(int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PushSelectionName(int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PopSelectionName()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearColor3ub(unsigned char,unsigned char,unsigned char)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearColor4ub(unsigned char,unsigned char,unsigned char,unsigned char)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void OverrideDepthEnable( bool, bool, bool )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void OverrideAlphaWriteEnable( bool, bool )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void OverrideColorWriteEnable( bool, bool )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DrawScreenSpaceQuad(IMaterial *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SyncToken(const char *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	OcclusionQueryObjectHandle_t CreateOcclusionQueryObject()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	void DestroyOcclusionQueryObject(OcclusionQueryObjectHandle_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginOcclusionQueryDrawing(OcclusionQueryObjectHandle_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndOcclusionQueryDrawing(OcclusionQueryObjectHandle_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	int OcclusionQuery_GetNumPixelsRendered(OcclusionQueryObjectHandle_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 1;
	}

	virtual void SetFlashlightMode(bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}
	virtual bool GetFlashlightMode( void ) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return false;
	}

	virtual bool IsCullingEnabledForSinglePassFlashlight( void ) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return false;
	}

	virtual void EnableCullingForSinglePassFlashlight( bool )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetRenderingPaint( bool bEnable )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFlashlightState(const FlashlightState_t &,const VMatrix &)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual bool IsCascadedShadowMapping() const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return false;
	}

	virtual void SetCascadedShadowMapping( bool bEnable )
	{
		bEnable;
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
	{
		state, pDepthTextureAtlas;
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PushScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom  )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PopScissorRect()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void PushDeformation( DeformationBase_t const *Deformation )
	{
	}

	virtual void PopDeformation( )
	{
	}

	virtual int GetNumActiveDeformations() const
	{
		return 0;
	}

	void EnableUserClipTransformOverride(bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void UserClipTransform(const VMatrix &)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	IMorph *CreateMorph(MorphFormat_t, const char *pDebugName)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	void DestroyMorph(IMorph *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BindMorph(IMorph *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetMorphTargetFactors(int,float *,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixelsAndStretch(Rect_t *,Rect_t *,unsigned char *,ImageFormat,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetWindowSize(int &,int &) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DrawScreenSpaceRectangle(IMaterial *,int,int,int,int,float,float,float,float,int,int,void*,int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void LoadBoneMatrix(int,const matrix3x4_t &)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BindLightmapTexture(ITexture *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CopyRenderTargetToTextureEx(ITexture *,int,Rect_t *,Rect_t *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CopyTextureToRenderTargetEx(int,ITexture *,Rect_t *,Rect_t *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFloatRenderingParameter(int,float)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetIntRenderingParameter(int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetTextureRenderingParameter( int, ITexture * )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetVectorRenderingParameter(int,const Vector &)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetStencilState( const ShaderStencilState_t & )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearStencilBufferRectangle(int,int,int,int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PushCustomClipPlane(const float *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PopCustomClipPlane()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetMaxToRender(IMesh *,bool,int *,int *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	int GetMaxVerticesToRender(IMaterial *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	int GetMaxIndicesToRender()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void DisableAllLocalLights()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	int CompareMaterialCombos(IMaterial *,IMaterial *,int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	IMesh *GetFlexMesh()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	void SetFlashlightStateEx(const FlashlightState_t &,const VMatrix &,ITexture *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	ITexture *GetLocalCubemap()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return NULL;
	}

	void ClearBuffersObeyStencil(bool,bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PerformFullScreenStencilOperation( void )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	bool GetUserClipTransform(VMatrix &)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	void GetFogDistances(float *,float *,float *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginPIXEvent(unsigned long,const char *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndPIXEvent()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetPIXMarker(unsigned long,const char *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginBatch(IMesh *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BindBatch(IMesh *,IMaterial *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DrawBatch(MaterialPrimitiveType_t, int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndBatch()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetToneMappingScaleLinear(const Vector &)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	float GetFloatRenderingParameter(int) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	int GetIntRenderingParameter(int) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	ITexture *GetTextureRenderingParameter(int) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	Vector GetVectorRenderingParameter(int) const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return Vector(0,0,0);
	}

	void SwapBuffers()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ForceDepthFuncEquals(bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	bool InFlashlightMode() const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool IsRenderingPaint() const
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	void GetLightmapDimensions(int *,int *)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	MorphFormat_t GetBoundMorphFormat()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void DrawClearBufferQuad(unsigned char,unsigned char,unsigned char,unsigned char,bool,bool,bool)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

#ifdef _PS3
	virtual void DrawReloadZcullQuad()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}
#endif // _PS3

	bool OnDrawMesh(IMesh *,CPrimList *,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnDrawMesh(IMesh *,int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnDrawMeshModulated(IMesh *, const Vector4D&, int,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnSetFlexMesh(IMesh *,IMesh *,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnSetColorMesh(IMesh *,IMesh *,int)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnSetPrimitiveType(IMesh *,MaterialPrimitiveType_t)
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	void ForceHardwareSync()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginFrame()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndFrame()
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetShadowDepthBiasFactors( float fSlopeScaleDepthBias, float fDepthBias )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}
	
	void BindStandardTexture( Sampler_t, TextureBindFlags_t nBindFlags, StandardTextureId_t )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EvictManagedResources()
	{
		AssertMsg( 0, "EvictManagedResources only provides base features, not a stub (right now)" );
	}

// ------------ New Vertex/Index Buffer interface ----------------------------
	// Do we need support for bForceTempMesh and bSoftwareVertexShader?
	// I don't think we use bSoftwareVertexShader anymore. .need to look into bForceTempMesh.
	IVertexBuffer *CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
	{
		Assert( 0 );
		return NULL;
	}
	IIndexBuffer *CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
	{
		Assert( 0 );
		return NULL;
	}
	void DestroyVertexBuffer( IVertexBuffer * )
	{
		Assert( 0 );
	}
	void DestroyIndexBuffer( IIndexBuffer * )
	{
		Assert( 0 );
	}
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBufferedtrue )
	{
		Assert( 0 );
		return NULL;
	}
	IIndexBuffer *GetDynamicIndexBuffer( )
	{
		Assert( 0 );
		return NULL;
	}
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions1 )
	{
		Assert( 0 );
	}
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
	{
		Assert( 0 );
	}
	void Draw( MaterialPrimitiveType_t primitiveType, int firstIndex, int numIndices )
	{
		Assert( 0 );
	}
	virtual void BeginMorphAccumulation()
	{
		Assert( 0 );
	}
	virtual void EndMorphAccumulation()
	{
		Assert( 0 );
	}
	virtual void AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
	{
		Assert( 0 );
	}
	virtual bool GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
	{
		Assert(0);
		pTexCoord->Init();
		return false;
	}

	virtual int GetSubDBufferWidth()
	{
		Assert(0);
		return 0;
	}
	virtual float* LockSubDBuffer( int nNumRows )
	{
		Assert(0);
		return NULL;
	}
	virtual void UnlockSubDBuffer()
	{
		Assert(0);
	}

	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights ) {}

	virtual void FogMaxDensity( float flMaxDensity )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void EnableColorCorrection( bool bEnable ) {}
	virtual ColorCorrectionHandle_t AddLookup( const char *pName ) { return 0; }
	virtual bool RemoveLookup( ColorCorrectionHandle_t handle ) { return true; }
	virtual void LockLookup( ColorCorrectionHandle_t handle ) {}
	virtual void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName ) {}
	virtual void UnlockLookup( ColorCorrectionHandle_t handle ) {}
	virtual void SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight ) {}
	virtual void ResetLookupWeights( ) {}
	virtual void SetResetable( ColorCorrectionHandle_t handle, bool bResetable ) {}
	virtual void SetFullScreenDepthTextureValidityFlag( bool bIsValid ) {}

	virtual void SetNonInteractiveLogoTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedW, float flNormalizedH ) {}
	virtual void SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize ) {}
	virtual void SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode ) {}
	virtual void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode ) {}
	virtual void RefreshFrontBufferNonInteractive() {}

	virtual void FlipCulling( bool bFlipCulling ) {}


#if defined( _X360 )
	virtual void PushVertexShaderGPRAllocation( int iVertexShaderCount = 64 )
	{
		Assert( 0 );
	}

	virtual void PopVertexShaderGPRAllocation( void )
	{
		Assert( 0 );
	}

	virtual void FlushHiStencil()
	{
		Assert( 0 );
	}
#endif

#if defined( _GAMECONSOLE )
	virtual void BeginConsoleZPass( const WorldListIndicesInfo_t &indicesInfo )
	{
		Assert( 0 );
	}

	virtual void BeginConsoleZPass2( int nSlack )
	{
		Assert( 0 );
	}
	
	virtual void EndConsoleZPass()
	{
		Assert( 0 );
	}
#endif

#if defined( _PS3 )
	virtual void FlushTextureCache()
	{
		Assert( 0 );
	}
#endif
	virtual void AntiAliasingHint(int )
	{
		Assert( 0 );
	}
	
	virtual void EnableSinglePassFlashlightMode( bool bEnable )
	{
		Assert( 0 );
	}

	virtual bool SinglePassFlashlightModeEnabled( void ) const
	{
		Assert( 0 );
		return false;
	}

	void UpdateGameTime( float )
	{
		AssertMsg( 0, "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

#if defined( INCLUDE_SCALEFORM )
	virtual void SetScaleformSlotViewport( int slot, int x, int y, int w, int h ) { Assert (0); }
	virtual void RenderScaleformSlot( int slot ) { Assert (0); }
	virtual void ForkRenderScaleformSlot( int slot ) { Assert (0); }
	virtual void JoinRenderScaleformSlot( int slot ) { Assert (0); }
	virtual void SetScaleformCursorViewport( int x, int y, int w, int h ) { Assert (0); }
	virtual void RenderScaleformCursor() { Assert (0); }
	virtual void AdvanceAndRenderScaleformSlot( int slot ) { Assert (0); }
	virtual void AdvanceAndRenderScaleformCursor() { Assert (0); }
#endif

	//--------------------------------------------------------
	// debug logging - no-op in queued context
	//--------------------------------------------------------
	virtual void							Printf( char *fmt, ... ) {};
	virtual void							PrintfVA( char *fmt, va_list vargs ){};
	virtual float							Knob( char *knobname, float *setvalue=NULL ) { return 0.0f; };	

#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _GAMECONSOLE )
	void									DoStartupShaderPreloading( void ) {};
#endif

	virtual ColorCorrectionHandle_t FindLookup( const char *pName ) { return 0; }

	int m_WidthBackBuffer, m_HeightBackBuffer;
};


CMatRenderContextBase *CreateNullRenderContext()
{
	return new CMatNullRenderContext;
}

//========== Copyright (c) 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//===============================================================================

#ifndef IMATRENDERCONTEXTINTERNAL_H
#define IMATRENDERCONTEXTINTERNAL_H

#if defined( _WIN32 )
#pragma once
#endif

// typedefs to allow use of delegation macros
typedef Vector4D LightCube_t[6];


abstract_class IMatRenderContextInternal : public IMatRenderContext
{
	// For now, stuck implementing these until IMaterialSystem is reworked
	bool Connect(CreateInterfaceFn) { return true; }
	void Disconnect(void) {}
	void *QueryInterface(const char *pszInterface) { return NULL;	}
	InitReturnVal_t Init(void) { return INIT_OK; }
	void Shutdown(void) {}

public:
	virtual float GetFloatRenderingParameter(int parm_number) const = 0;
	virtual int GetIntRenderingParameter(int parm_number) const = 0;
	virtual ITexture *GetTextureRenderingParameter(int parm_number) const = 0;
	virtual Vector GetVectorRenderingParameter(int parm_number) const = 0;

	virtual void SwapBuffers() = 0;

	virtual void SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial) = 0;
	virtual IMaterialInternal* GetCurrentMaterialInternal() const = 0;
	virtual int GetLightmapPage() = 0;
	virtual void ForceDepthFuncEquals( bool) = 0;

	virtual bool InFlashlightMode() const = 0;
	virtual bool IsCascadedShadowMapping() const = 0;
	virtual void BindStandardTexture( Sampler_t, TextureBindFlags_t nBindFlags, StandardTextureId_t ) = 0;
	virtual void GetLightmapDimensions( int *, int *) = 0;
	virtual MorphFormat_t GetBoundMorphFormat() = 0;
	virtual ITexture *GetRenderTargetEx( int ) = 0;
	virtual void DrawClearBufferQuad( unsigned char, unsigned char, unsigned char, unsigned char, bool, bool, bool ) = 0;
#ifdef _PS3
	virtual void DrawReloadZcullQuad() = 0;
#endif // _PS3

	virtual bool OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices ) = 0;
	virtual bool OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists ) = 0;
	virtual bool OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices ) = 0;
	virtual bool OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) = 0;
	virtual bool OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) = 0;
	virtual bool OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type ) = 0;

	virtual void SyncMatrices() = 0;
	virtual void SyncMatrix( MaterialMatrixMode_t ) = 0;

	virtual void ForceHardwareSync() = 0;
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual void SetFrameTime( float frameTime ) = 0;
	virtual void SetCurrentProxy( void *pProxy ) = 0;
	virtual void MarkRenderDataUnused( bool bBeginFrame ) = 0;
	virtual CMatCallQueue *GetCallQueueInternal() = 0;

	virtual void EvictManagedResources() = 0;

	virtual ShaderAPITextureHandle_t GetLightmapTexture( int nLightmapPage ) = 0;
	virtual bool IsRenderingPaint() const = 0;
	virtual ShaderAPITextureHandle_t GetPaintmapTexture( int nLightmapPage ) = 0;
	
#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _GAMECONSOLE )
	virtual void DoStartupShaderPreloading( void ) = 0;
#endif
};

#endif // IMATRENDERCONTEXTINTERNAL_H

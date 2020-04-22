//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
#ifndef SHADERUTIL_PS3NONVIRT_H
#define SHADERUTIL_PS3NONVIRT_H

#ifdef _PS3

#include "shaderapi/ishaderutil.h"

//////////////////////////////////////////////////////////////////////////
//
// PS3 non-virtual implementation proxy
//
// cat shaderutil_ps3nonvirt.h | nonvirtualscript.pl > shaderutil_ps3nonvirt.inl
struct CPs3NonVirt_IShaderUtil
{
//NONVIRTUALSCRIPTBEGIN
//NONVIRTUALSCRIPT/PROXY/CPs3NonVirt_IShaderUtil
//NONVIRTUALSCRIPT/DELEGATE/g_MaterialSystem.CMaterialSystem::

	//
	// IShaderUtil
	//
	static MaterialSystem_Config_t& GetConfig();
	static bool ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat, unsigned char *dst, enum ImageFormat dstImageFormat, int width, int height, int srcStride = 0, int dstStride = 0 );
	static int GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap );
	static const ImageFormatInfo_t& ImageFormatInfo( ImageFormat fmt );
	static void BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id );
	static void GetLightmapDimensions( int *w, int *h );
	static void ReleaseShaderObjects( int nChangeFlags = 0 );
	static void RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags = 0 );
	static bool IsInStubMode();
	static bool InFlashlightMode();
	static void NoteAnisotropicLevel( int currentLevel );
	static bool InEditorMode();
	static ITexture *GetRenderTargetEx( int nRenderTargetID );
	static void DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth );
	static void DrawReloadZcullQuad();
	static bool OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	static bool OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	static bool OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	static bool OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	static bool OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type );
	static void SyncMatrices();
	static void SyncMatrix( MaterialMatrixMode_t mmm );
	static void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	static ShaderAPITextureHandle_t GetStandardTexture( StandardTextureId_t id );
	static void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );
	static int MaxHWMorphBatchCount();
	static void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );
	static ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel );
	static float GetSubDHeight();
	static bool OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices );
	static void OnThreadEvent( uint32 threadEvent );
	static MaterialThreadMode_t GetThreadMode();
	static void UncacheUnusedMaterials( bool bRecomputeStateSnapshots = false );
	static bool IsInFrame( );
	static ShaderAPITextureHandle_t GetLightmapTexture( int nLightmapPage );
	static bool IsRenderingPaint();
	static ShaderAPITextureHandle_t	GetPaintmapTexture( int nLightmapPage );
	static bool IsCascadedShadowMapping();

//NONVIRTUALSCRIPTEND
};

#endif


#endif // SHADERUTIL_PS3NONVIRT_H

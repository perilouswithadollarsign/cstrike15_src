MaterialSystem_Config_t& CPs3NonVirt_IShaderUtil::GetConfig()
{
	return g_MaterialSystem.CMaterialSystem::GetConfig();
}

bool CPs3NonVirt_IShaderUtil::ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat, unsigned char *dst, enum ImageFormat dstImageFormat, int width, int height, int srcStride, int dstStride)
{
	return g_MaterialSystem.CMaterialSystem::ConvertImageFormat(src,srcImageFormat,dst,dstImageFormat,width,height,srcStride,dstStride);
}

int CPs3NonVirt_IShaderUtil::GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap )
{
	return g_MaterialSystem.CMaterialSystem::GetMemRequired(width,height,depth,format,mipmap);
}

const ImageFormatInfo_t& CPs3NonVirt_IShaderUtil::ImageFormatInfo( ImageFormat fmt )
{
	return g_MaterialSystem.CMaterialSystem::ImageFormatInfo(fmt);
}

void CPs3NonVirt_IShaderUtil::BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
{
	g_MaterialSystem.CMaterialSystem::BindStandardTexture(sampler, nBindFlags, id);
}

void CPs3NonVirt_IShaderUtil::GetLightmapDimensions( int *w, int *h )
{
	g_MaterialSystem.CMaterialSystem::GetLightmapDimensions(w,h);
}

void CPs3NonVirt_IShaderUtil::ReleaseShaderObjects( int nChangeFlags)
{
	g_MaterialSystem.CMaterialSystem::ReleaseShaderObjects(nChangeFlags);
}

void CPs3NonVirt_IShaderUtil::RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags)
{
	g_MaterialSystem.CMaterialSystem::RestoreShaderObjects(shaderFactory,nChangeFlags);
}

bool CPs3NonVirt_IShaderUtil::IsInStubMode()
{
	return g_MaterialSystem.CMaterialSystem::IsInStubMode();
}

bool CPs3NonVirt_IShaderUtil::InFlashlightMode()
{
	return g_MaterialSystem.CMaterialSystem::InFlashlightMode();
}

bool CPs3NonVirt_IShaderUtil::IsRenderingPaint()
{
	return g_MaterialSystem.CMaterialSystem::IsRenderingPaint();
}

void CPs3NonVirt_IShaderUtil::NoteAnisotropicLevel( int currentLevel )
{
	g_MaterialSystem.CMaterialSystem::NoteAnisotropicLevel(currentLevel);
}

bool CPs3NonVirt_IShaderUtil::InEditorMode()
{
	return g_MaterialSystem.CMaterialSystem::InEditorMode();
}

ITexture * CPs3NonVirt_IShaderUtil::GetRenderTargetEx( int nRenderTargetID )
{
	return g_MaterialSystem.CMaterialSystem::GetRenderTargetEx(nRenderTargetID);
}

void CPs3NonVirt_IShaderUtil::DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
	g_MaterialSystem.CMaterialSystem::DrawClearBufferQuad(r,g,b,a,bClearColor,bClearAlpha,bClearDepth);
}

void CPs3NonVirt_IShaderUtil::DrawReloadZcullQuad()
{
	g_MaterialSystem.CMaterialSystem::DrawReloadZcullQuad();
}

bool CPs3NonVirt_IShaderUtil::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	return g_MaterialSystem.CMaterialSystem::OnDrawMesh(pMesh,firstIndex,numIndices);
}

bool CPs3NonVirt_IShaderUtil::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	return g_MaterialSystem.CMaterialSystem::OnDrawMesh(pMesh,pLists,nLists);
}

bool CPs3NonVirt_IShaderUtil::OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	return g_MaterialSystem.CMaterialSystem::OnSetFlexMesh(pStaticMesh,pMesh,nVertexOffsetInBytes);
}

bool CPs3NonVirt_IShaderUtil::OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	return g_MaterialSystem.CMaterialSystem::OnSetColorMesh(pStaticMesh,pMesh,nVertexOffsetInBytes);
}

bool CPs3NonVirt_IShaderUtil::OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type )
{
	return g_MaterialSystem.CMaterialSystem::OnSetPrimitiveType(pMesh,type);
}

void CPs3NonVirt_IShaderUtil::SyncMatrices()
{
	g_MaterialSystem.CMaterialSystem::SyncMatrices();
}

void CPs3NonVirt_IShaderUtil::SyncMatrix( MaterialMatrixMode_t mmm )
{
	g_MaterialSystem.CMaterialSystem::SyncMatrix(mmm);
}

void CPs3NonVirt_IShaderUtil::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{
	g_MaterialSystem.CMaterialSystem::BindStandardVertexTexture(sampler,id);
}

ShaderAPITextureHandle_t CPs3NonVirt_IShaderUtil::GetStandardTexture( StandardTextureId_t id )
{
	return g_MaterialSystem.CMaterialSystem::GetStandardTexture(id);
}

void CPs3NonVirt_IShaderUtil::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{
	g_MaterialSystem.CMaterialSystem::GetStandardTextureDimensions(pWidth,pHeight,id);
}

int CPs3NonVirt_IShaderUtil::MaxHWMorphBatchCount()
{
	return g_MaterialSystem.CMaterialSystem::MaxHWMorphBatchCount();
}

void CPs3NonVirt_IShaderUtil::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{
	g_MaterialSystem.CMaterialSystem::GetCurrentColorCorrection(pInfo);
}

ShaderAPITextureHandle_t CPs3NonVirt_IShaderUtil::GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel )
{
	return g_MaterialSystem.CMaterialSystem::GetShaderAPITextureBindHandle(pTexture,nFrame,nTextureChannel);
}

float CPs3NonVirt_IShaderUtil::GetSubDHeight()
{
	return g_MaterialSystem.CMaterialSystem::GetSubDHeight();
}

bool CPs3NonVirt_IShaderUtil::OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices )
{
	return g_MaterialSystem.CMaterialSystem::OnDrawMeshModulated(pMesh,diffuseModulation,firstIndex,numIndices);
}

void CPs3NonVirt_IShaderUtil::OnThreadEvent( uint32 threadEvent )
{
	g_MaterialSystem.CMaterialSystem::OnThreadEvent(threadEvent);
}

MaterialThreadMode_t CPs3NonVirt_IShaderUtil::GetThreadMode()
{
	return g_MaterialSystem.CMaterialSystem::GetThreadMode();
}

void CPs3NonVirt_IShaderUtil::UncacheUnusedMaterials( bool bRecomputeStateSnapshots)
{
	g_MaterialSystem.CMaterialSystem::UncacheUnusedMaterials(bRecomputeStateSnapshots);
}

bool CPs3NonVirt_IShaderUtil::IsInFrame( )
{
	return g_MaterialSystem.CMaterialSystem::IsInFrame();
}

ShaderAPITextureHandle_t CPs3NonVirt_IShaderUtil::GetLightmapTexture( int nLightmapPage )
{
	return g_MaterialSystem.CMaterialSystem::GetLightmapTexture( nLightmapPage );
}

ShaderAPITextureHandle_t CPs3NonVirt_IShaderUtil::GetPaintmapTexture( int nLightmapPage )
{
	return g_MaterialSystem.CMaterialSystem::GetPaintmapTexture( nLightmapPage );
}

bool CPs3NonVirt_IShaderUtil::IsCascadedShadowMapping()
{
	return g_MaterialSystem.CMaterialSystem::IsCascadedShadowMapping(); 
}

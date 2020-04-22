ShaderAPITextureHandle_t CPs3NonVirt_IShaderAPIDX8::GetStandardTextureHandle(StandardTextureId_t id)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetStandardTextureHandle(id);
}

void CPs3NonVirt_IShaderAPIDX8::SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetViewports(nCount,pViewports, setImmediately );
}

int CPs3NonVirt_IShaderAPIDX8::GetViewports( ShaderViewport_t* pViewports, int nMax )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetViewports(pViewports,nMax);
}

void CPs3NonVirt_IShaderAPIDX8::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearBuffers(bClearColor,bClearDepth,bClearStencil,renderTargetWidth,renderTargetHeight);
}

void CPs3NonVirt_IShaderAPIDX8::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearColor3ub(r,g,b);
}

void CPs3NonVirt_IShaderAPIDX8::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearColor4ub(r,g,b,a);
}

void CPs3NonVirt_IShaderAPIDX8::BindVertexShader( VertexShaderHandle_t hVertexShader )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindVertexShader(hVertexShader);
}

void CPs3NonVirt_IShaderAPIDX8::BindGeometryShader( GeometryShaderHandle_t hGeometryShader )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindGeometryShader(hGeometryShader);
}

void CPs3NonVirt_IShaderAPIDX8::BindPixelShader( PixelShaderHandle_t hPixelShader )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindPixelShader(hPixelShader);
}

void CPs3NonVirt_IShaderAPIDX8::SetRasterState( const ShaderRasterState_t& state )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetRasterState(state);
}

bool CPs3NonVirt_IShaderAPIDX8::SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::SetMode(hwnd,nAdapter,info);
}

void CPs3NonVirt_IShaderAPIDX8::ChangeVideoMode( const ShaderDeviceInfo_t &info )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ChangeVideoMode(info);
}

StateSnapshot_t CPs3NonVirt_IShaderAPIDX8::TakeSnapshot( )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::TakeSnapshot();
}

void CPs3NonVirt_IShaderAPIDX8::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexMinFilter(texFilterMode);
}

void CPs3NonVirt_IShaderAPIDX8::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexMagFilter(texFilterMode);
}

void CPs3NonVirt_IShaderAPIDX8::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexWrap(coord,wrapMode);
}

void CPs3NonVirt_IShaderAPIDX8::CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle )
{
	g_ShaderAPIDX8.CShaderAPIDx8::CopyRenderTargetToTexture(textureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::Bind( IMaterial* pMaterial )
{
	g_ShaderAPIDX8.CShaderAPIDx8::Bind(pMaterial);
}

IMesh* CPs3NonVirt_IShaderAPIDX8::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetDynamicMesh(pMaterial,nHWSkinBoneCount,bBuffered,pVertexOverride,pIndexOverride);
}

IMesh* CPs3NonVirt_IShaderAPIDX8::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetDynamicMeshEx(pMaterial,vertexFormat,nHWSkinBoneCount,bBuffered,pVertexOverride,pIndexOverride);
}

bool CPs3NonVirt_IShaderAPIDX8::IsTranslucent( StateSnapshot_t id )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsTranslucent(id);
}

bool CPs3NonVirt_IShaderAPIDX8::IsAlphaTested( StateSnapshot_t id )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsAlphaTested(id);
}

bool CPs3NonVirt_IShaderAPIDX8::UsesVertexAndPixelShaders( StateSnapshot_t id )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::UsesVertexAndPixelShaders(id);
}

bool CPs3NonVirt_IShaderAPIDX8::IsDepthWriteEnabled( StateSnapshot_t id )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsDepthWriteEnabled(id);
}

VertexFormat_t CPs3NonVirt_IShaderAPIDX8::ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::ComputeVertexFormat(numSnapshots,pIds);
}

VertexFormat_t CPs3NonVirt_IShaderAPIDX8::ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::ComputeVertexUsage(numSnapshots,pIds);
}

void CPs3NonVirt_IShaderAPIDX8::BeginPass( StateSnapshot_t snapshot )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BeginPass(snapshot);
}

void CPs3NonVirt_IShaderAPIDX8::RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount )
{
	g_ShaderAPIDX8.CShaderAPIDx8::RenderPass(pInstanceCommandBuffer,nPass,nPassCount);
}

void CPs3NonVirt_IShaderAPIDX8::SetNumBoneWeights( int numBones )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetNumBoneWeights(numBones);
}

void CPs3NonVirt_IShaderAPIDX8::SetLights( int nCount, const LightDesc_t *pDesc )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetLights(nCount,pDesc);
}

void CPs3NonVirt_IShaderAPIDX8::SetLightingOrigin( Vector vLightingOrigin )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetLightingOrigin(vLightingOrigin);
}

void CPs3NonVirt_IShaderAPIDX8::SetLightingState( const MaterialLightingState_t& state )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetLightingState(state);
}

void CPs3NonVirt_IShaderAPIDX8::SetAmbientLightCube( Vector4D cube[6] )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetAmbientLightCube(cube);
}

void CPs3NonVirt_IShaderAPIDX8::ShadeMode( ShaderShadeMode_t mode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ShadeMode(mode);
}

void CPs3NonVirt_IShaderAPIDX8::CullMode( MaterialCullMode_t cullMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::CullMode(cullMode);
}

void CPs3NonVirt_IShaderAPIDX8::FlipCullMode()
{
	g_ShaderAPIDX8.CShaderAPIDx8::FlipCullMode();
}

void CPs3NonVirt_IShaderAPIDX8::ForceDepthFuncEquals( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ForceDepthFuncEquals(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::OverrideDepthEnable(bEnable,bDepthWriteEnable,bDepthTestEnable);
}

void CPs3NonVirt_IShaderAPIDX8::SetHeightClipZ( float z )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetHeightClipZ(z);
}

void CPs3NonVirt_IShaderAPIDX8::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetHeightClipMode(heightClipMode);
}

void CPs3NonVirt_IShaderAPIDX8::SetClipPlane( int index, const float *pPlane )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetClipPlane(index,pPlane);
}

void CPs3NonVirt_IShaderAPIDX8::EnableClipPlane( int index, bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableClipPlane(index,bEnable);
}

ImageFormat CPs3NonVirt_IShaderAPIDX8::GetNearestSupportedFormat( ImageFormat fmt )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetNearestSupportedFormat(fmt);
}

ImageFormat CPs3NonVirt_IShaderAPIDX8::GetNearestRenderTargetFormat( ImageFormat fmt )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetNearestRenderTargetFormat(fmt);
}

bool CPs3NonVirt_IShaderAPIDX8::DoRenderTargetsNeedSeparateDepthBuffer()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::DoRenderTargetsNeedSeparateDepthBuffer();
}

ShaderAPITextureHandle_t CPs3NonVirt_IShaderAPIDX8::CreateTexture( int width, int height, int depth, ImageFormat dstImageFormat, int numMipLevels, int numCopies, int flags, const char *pDebugName, const char *pTextureGroupName )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::CreateTexture(width,height,depth,dstImageFormat,numMipLevels,numCopies,flags,pDebugName,pTextureGroupName);
}

void CPs3NonVirt_IShaderAPIDX8::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
	g_ShaderAPIDX8.CShaderAPIDx8::DeleteTexture(textureHandle);
}

ShaderAPITextureHandle_t CPs3NonVirt_IShaderAPIDX8::CreateDepthTexture( ImageFormat renderTargetFormat, int width, int height, const char *pDebugName, bool bTexture, bool bAliasDepthSurfaceOverColorX360 )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::CreateDepthTexture(renderTargetFormat,width,height,pDebugName,bTexture,bAliasDepthSurfaceOverColorX360);
}

bool CPs3NonVirt_IShaderAPIDX8::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsTexture(textureHandle);
}

bool CPs3NonVirt_IShaderAPIDX8::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsTextureResident(textureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ModifyTexture(textureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::TexImage2D( int level, int cubeFaceID, ImageFormat dstFormat, int zOffset, int width, int height, ImageFormat srcFormat, bool bSrcIsTiled, void *imageData )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexImage2D(level,cubeFaceID,dstFormat,zOffset,width,height,srcFormat,bSrcIsTiled,imageData);
}

void CPs3NonVirt_IShaderAPIDX8::TexSubImage2D( int level, int cubeFaceID, int xOffset, int yOffset, int zOffset, int width, int height, ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexSubImage2D(level,cubeFaceID,xOffset,yOffset,zOffset,width,height,srcFormat,srcStride,bSrcIsTiled,imageData);
}

bool CPs3NonVirt_IShaderAPIDX8::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, int width, int height, CPixelWriter& writer )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::TexLock(level,cubeFaceID,xOffset,yOffset,width,height,writer);
}

void CPs3NonVirt_IShaderAPIDX8::TexUnlock( )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexUnlock();
}

void CPs3NonVirt_IShaderAPIDX8::UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture )
{
	g_ShaderAPIDX8.CShaderAPIDx8::UpdateTexture(xOffset,yOffset,w,h,hDstTexture,hSrcTexture);
}

void * CPs3NonVirt_IShaderAPIDX8::LockTex( ShaderAPITextureHandle_t hTexture )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::LockTex(hTexture);
}

void CPs3NonVirt_IShaderAPIDX8::UnlockTex( ShaderAPITextureHandle_t hTexture )
{
	g_ShaderAPIDX8.CShaderAPIDx8::UnlockTex(hTexture);
}

void CPs3NonVirt_IShaderAPIDX8::TexSetPriority( int priority )
{
	g_ShaderAPIDX8.CShaderAPIDx8::TexSetPriority(priority);
}

void CPs3NonVirt_IShaderAPIDX8::BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindTexture( sampler, nBindFlags, textureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetRenderTarget(colorTextureHandle,depthTextureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearBuffersObeyStencil(bClearColor,bClearDepth);
}

void CPs3NonVirt_IShaderAPIDX8::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ReadPixels(x,y,width,height,data,dstFormat);
}

void CPs3NonVirt_IShaderAPIDX8::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ReadPixels(pSrcRect,pDstRect,data,dstFormat,nDstStride);
}

void CPs3NonVirt_IShaderAPIDX8::FlushHardware()
{
	g_ShaderAPIDX8.CShaderAPIDx8::FlushHardware();
}

void CPs3NonVirt_IShaderAPIDX8::BeginFrame()
{
	g_ShaderAPIDX8.CShaderAPIDx8::BeginFrame();
}

void CPs3NonVirt_IShaderAPIDX8::EndFrame()
{
	g_ShaderAPIDX8.CShaderAPIDx8::EndFrame();
}

int CPs3NonVirt_IShaderAPIDX8::SelectionMode( bool selectionMode )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::SelectionMode(selectionMode);
}

void CPs3NonVirt_IShaderAPIDX8::SelectionBuffer( unsigned int* pBuffer, int size )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SelectionBuffer(pBuffer,size);
}

void CPs3NonVirt_IShaderAPIDX8::ClearSelectionNames( )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearSelectionNames();
}

void CPs3NonVirt_IShaderAPIDX8::LoadSelectionName( int name )
{
	g_ShaderAPIDX8.CShaderAPIDx8::LoadSelectionName(name);
}

void CPs3NonVirt_IShaderAPIDX8::PushSelectionName( int name )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PushSelectionName(name);
}

void CPs3NonVirt_IShaderAPIDX8::PopSelectionName()
{
	g_ShaderAPIDX8.CShaderAPIDx8::PopSelectionName();
}

void CPs3NonVirt_IShaderAPIDX8::ForceHardwareSync()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ForceHardwareSync();
}

void CPs3NonVirt_IShaderAPIDX8::ClearSnapshots()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearSnapshots();
}

void CPs3NonVirt_IShaderAPIDX8::FogStart( float fStart )
{
	g_ShaderAPIDX8.CShaderAPIDx8::FogStart(fStart);
}

void CPs3NonVirt_IShaderAPIDX8::FogEnd( float fEnd )
{
	g_ShaderAPIDX8.CShaderAPIDx8::FogEnd(fEnd);
}

void CPs3NonVirt_IShaderAPIDX8::SetFogZ( float fogZ )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFogZ(fogZ);
}

void CPs3NonVirt_IShaderAPIDX8::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SceneFogColor3ub(r,g,b);
}

void CPs3NonVirt_IShaderAPIDX8::GetSceneFogColor( unsigned char *rgb )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetSceneFogColor(rgb);
}

void CPs3NonVirt_IShaderAPIDX8::SceneFogMode( MaterialFogMode_t fogMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SceneFogMode(fogMode);
}

bool CPs3NonVirt_IShaderAPIDX8::CanDownloadTextures()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::CanDownloadTextures();
}

void CPs3NonVirt_IShaderAPIDX8::ResetRenderState( bool bFullReset)
{
	g_ShaderAPIDX8.CShaderAPIDx8::ResetRenderState(bFullReset);
}

int CPs3NonVirt_IShaderAPIDX8::GetCurrentDynamicVBSize()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentDynamicVBSize();
}

void CPs3NonVirt_IShaderAPIDX8::DestroyVertexBuffers( bool bExitingLevel)
{
	g_ShaderAPIDX8.CShaderAPIDx8::DestroyVertexBuffers(bExitingLevel);
}

void CPs3NonVirt_IShaderAPIDX8::EvictManagedResources()
{
	g_ShaderAPIDX8.CShaderAPIDx8::EvictManagedResources();
}

void CPs3NonVirt_IShaderAPIDX8::GetGPUMemoryStats( GPUMemoryStats &stats )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetGPUMemoryStats(stats);
}

void CPs3NonVirt_IShaderAPIDX8::SetAnisotropicLevel( int nAnisotropyLevel )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetAnisotropicLevel(nAnisotropyLevel);
}

void CPs3NonVirt_IShaderAPIDX8::SyncToken( const char *pToken )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SyncToken(pToken);
}

void CPs3NonVirt_IShaderAPIDX8::SetStandardVertexShaderConstants( float fOverbright )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetStandardVertexShaderConstants(fOverbright);
}

ShaderAPIOcclusionQuery_t CPs3NonVirt_IShaderAPIDX8::CreateOcclusionQueryObject()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::CreateOcclusionQueryObject();
}

void CPs3NonVirt_IShaderAPIDX8::DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t q )
{
	g_ShaderAPIDX8.CShaderAPIDx8::DestroyOcclusionQueryObject(q);
}

void CPs3NonVirt_IShaderAPIDX8::BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t q )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BeginOcclusionQueryDrawing(q);
}

void CPs3NonVirt_IShaderAPIDX8::EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t q )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EndOcclusionQueryDrawing(q);
}

int CPs3NonVirt_IShaderAPIDX8::OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t hQuery, bool bFlush)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::OcclusionQuery_GetNumPixelsRendered(hQuery,bFlush);
}

void CPs3NonVirt_IShaderAPIDX8::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFlashlightState(state,worldToTexture);
}

void CPs3NonVirt_IShaderAPIDX8::ClearVertexAndPixelShaderRefCounts()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearVertexAndPixelShaderRefCounts();
}

void CPs3NonVirt_IShaderAPIDX8::PurgeUnusedVertexAndPixelShaders()
{
	g_ShaderAPIDX8.CShaderAPIDx8::PurgeUnusedVertexAndPixelShaders();
}

void CPs3NonVirt_IShaderAPIDX8::DXSupportLevelChanged( int nDXLevel )
{
	g_ShaderAPIDX8.CShaderAPIDx8::DXSupportLevelChanged(nDXLevel);
}

void CPs3NonVirt_IShaderAPIDX8::EnableUserClipTransformOverride( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableUserClipTransformOverride(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::UserClipTransform( const VMatrix &worldToView )
{
	g_ShaderAPIDX8.CShaderAPIDx8::UserClipTransform(worldToView);
}

void CPs3NonVirt_IShaderAPIDX8::SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetRenderTargetEx(nRenderTargetID,colorTextureHandle,depthTextureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect)
{
	g_ShaderAPIDX8.CShaderAPIDx8::CopyRenderTargetToTextureEx(textureHandle,nRenderTargetID,pSrcRect,pDstRect);
}

void CPs3NonVirt_IShaderAPIDX8::HandleDeviceLost()
{
	g_ShaderAPIDX8.CShaderAPIDx8::HandleDeviceLost();
}

void CPs3NonVirt_IShaderAPIDX8::EnableLinearColorSpaceFrameBuffer( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableLinearColorSpaceFrameBuffer(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::SetFullScreenTextureHandle( ShaderAPITextureHandle_t h )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFullScreenTextureHandle(h);
}

void CPs3NonVirt_IShaderAPIDX8::SetFloatRenderingParameter(int parm_number, float value)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFloatRenderingParameter(parm_number,value);
}

void CPs3NonVirt_IShaderAPIDX8::SetIntRenderingParameter(int parm_number, int value)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetIntRenderingParameter(parm_number,value);
}

void CPs3NonVirt_IShaderAPIDX8::SetVectorRenderingParameter(int parm_number, Vector const &value)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetVectorRenderingParameter(parm_number,value);
}

float CPs3NonVirt_IShaderAPIDX8::GetFloatRenderingParameter(int parm_number)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetFloatRenderingParameter(parm_number);
}

int CPs3NonVirt_IShaderAPIDX8::GetIntRenderingParameter(int parm_number)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetIntRenderingParameter(parm_number);
}

Vector CPs3NonVirt_IShaderAPIDX8::GetVectorRenderingParameter(int parm_number)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetVectorRenderingParameter(parm_number);
}

void CPs3NonVirt_IShaderAPIDX8::SetFastClipPlane( const float *pPlane )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFastClipPlane(pPlane);
}

void CPs3NonVirt_IShaderAPIDX8::EnableFastClip( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableFastClip(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetMaxToRender(pMesh,bMaxUntilFlush,pMaxVerts,pMaxIndices);
}

int CPs3NonVirt_IShaderAPIDX8::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetMaxVerticesToRender(pMaterial);
}

int CPs3NonVirt_IShaderAPIDX8::GetMaxIndicesToRender( )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetMaxIndicesToRender();
}

void CPs3NonVirt_IShaderAPIDX8::SetStencilState( const ShaderStencilState_t& state )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetStencilState(state);
}

void CPs3NonVirt_IShaderAPIDX8::ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax, int value)
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearStencilBufferRectangle(xmin,ymin,xmax,ymax,value);
}

void CPs3NonVirt_IShaderAPIDX8::DisableAllLocalLights()
{
	g_ShaderAPIDX8.CShaderAPIDx8::DisableAllLocalLights();
}

int CPs3NonVirt_IShaderAPIDX8::CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::CompareSnapshots(snapshot0,snapshot1);
}

IMesh * CPs3NonVirt_IShaderAPIDX8::GetFlexMesh()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetFlexMesh();
}

void CPs3NonVirt_IShaderAPIDX8::SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFlashlightStateEx(state,worldToTexture,pFlashlightDepthTexture);
}

void CPs3NonVirt_IShaderAPIDX8::SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
{
	g_ShaderAPIDX8.CShaderAPIDx8::m_CascadedShadowMappingState = state;
	g_ShaderAPIDX8.CShaderAPIDx8::m_pCascadedShadowMappingDepthTexture = pDepthTextureAtlas;
}

const CascadedShadowMappingState_t &CPs3NonVirt_IShaderAPIDX8::GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas )
{
	if ( pDepthTextureAtlas )
		*pDepthTextureAtlas = g_ShaderAPIDX8.CShaderAPIDx8::m_pCascadedShadowMappingDepthTexture;

	return g_ShaderAPIDX8.CShaderAPIDx8::m_CascadedShadowMappingState;
}

bool CPs3NonVirt_IShaderAPIDX8::SupportsMSAAMode( int nMSAAMode )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::SupportsMSAAMode(nMSAAMode);
}

bool CPs3NonVirt_IShaderAPIDX8::PostQueuedTexture( const void *pData, int nSize, ShaderAPITextureHandle_t *pHandles, int nHandles, int nWidth, int nHeight, int nDepth, int nMips, int *pRefCount )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::PostQueuedTexture(pData,nSize,pHandles,nHandles,nWidth,nHeight,nDepth,nMips,pRefCount);
}

void CPs3NonVirt_IShaderAPIDX8::AntiAliasingHint( int nHint )
{
	g_ShaderAPIDX8.CShaderAPIDx8::AntiAliasingHint( nHint );
}

void CPs3NonVirt_IShaderAPIDX8::FlushTextureCache()
{
	g_ShaderAPIDX8.CShaderAPIDx8::FlushTextureCache();
}

bool CPs3NonVirt_IShaderAPIDX8::OwnGPUResources( bool bEnable )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::OwnGPUResources(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetFogDistances(fStart,fEnd,fFogZ);
}

void CPs3NonVirt_IShaderAPIDX8::BeginPIXEvent( unsigned long color, const char *szName )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BeginPIXEvent(color,szName);
}

void CPs3NonVirt_IShaderAPIDX8::EndPIXEvent()
{
	g_ShaderAPIDX8.CShaderAPIDx8::EndPIXEvent();
}

void CPs3NonVirt_IShaderAPIDX8::SetPIXMarker( unsigned long color, const char *szName )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetPIXMarker(color,szName);
}

void CPs3NonVirt_IShaderAPIDX8::EnableAlphaToCoverage()
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableAlphaToCoverage();
}

void CPs3NonVirt_IShaderAPIDX8::DisableAlphaToCoverage()
{
	g_ShaderAPIDX8.CShaderAPIDx8::DisableAlphaToCoverage();
}

void CPs3NonVirt_IShaderAPIDX8::ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ComputeVertexDescription(pBuffer,vertexFormat,desc);
}

int CPs3NonVirt_IShaderAPIDX8::VertexFormatSize( VertexFormat_t vertexFormat )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::VertexFormatSize(vertexFormat);
}

void CPs3NonVirt_IShaderAPIDX8::SetDisallowAccess( bool b )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetDisallowAccess(b);
}

void CPs3NonVirt_IShaderAPIDX8::EnableShaderShaderMutex( bool b )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableShaderShaderMutex(b);
}

void CPs3NonVirt_IShaderAPIDX8::ShaderLock()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ShaderLock();
}

void CPs3NonVirt_IShaderAPIDX8::ShaderUnlock()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ShaderUnlock();
}

void CPs3NonVirt_IShaderAPIDX8::SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetShadowDepthBiasFactors(fShadowSlopeScaleDepthBias,fShadowDepthBias);
}

void CPs3NonVirt_IShaderAPIDX8::BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions)
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindVertexBuffer(nStreamID,pVertexBuffer,nOffsetInBytes,nFirstVertex,nVertexCount,fmt,nRepetitions);
}

void CPs3NonVirt_IShaderAPIDX8::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindIndexBuffer(pIndexBuffer,nOffsetInBytes);
}

void CPs3NonVirt_IShaderAPIDX8::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{
	g_ShaderAPIDX8.CShaderAPIDx8::Draw(primitiveType,nFirstIndex,nIndexCount);
}

void CPs3NonVirt_IShaderAPIDX8::PerformFullScreenStencilOperation()
{
	g_ShaderAPIDX8.CShaderAPIDx8::PerformFullScreenStencilOperation();
}

void CPs3NonVirt_IShaderAPIDX8::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetScissorRect(nLeft,nTop,nRight,nBottom,bEnableScissor);
}

bool CPs3NonVirt_IShaderAPIDX8::SupportsCSAAMode( int nNumSamples, int nQualityLevel )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::SupportsCSAAMode(nNumSamples,nQualityLevel);
}

void CPs3NonVirt_IShaderAPIDX8::InvalidateDelayedShaderConstants()
{
	g_ShaderAPIDX8.CShaderAPIDx8::InvalidateDelayedShaderConstants();
}

float CPs3NonVirt_IShaderAPIDX8::GammaToLinear_HardwareSpecific( float fGamma )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GammaToLinear_HardwareSpecific(fGamma);
}

float CPs3NonVirt_IShaderAPIDX8::LinearToGamma_HardwareSpecific( float fLinear )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::LinearToGamma_HardwareSpecific(fLinear);
}

void CPs3NonVirt_IShaderAPIDX8::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetLinearToGammaConversionTextures(hSRGBWriteEnabledTexture,hIdentityTexture);
}

void CPs3NonVirt_IShaderAPIDX8::BindVertexTexture( VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t textureHandle )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindVertexTexture(nSampler,textureHandle);
}

void CPs3NonVirt_IShaderAPIDX8::EnableHWMorphing( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableHWMorphing(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetFlexWeights(nFirstWeight,nCount,pWeights);
}

void CPs3NonVirt_IShaderAPIDX8::FogMaxDensity( float flMaxDensity )
{
	g_ShaderAPIDX8.CShaderAPIDx8::FogMaxDensity(flMaxDensity);
}

void CPs3NonVirt_IShaderAPIDX8::CreateTextures( ShaderAPITextureHandle_t *pHandles, int count, int width, int height, int depth, ImageFormat dstImageFormat, int numMipLevels, int numCopies, int flags, const char *pDebugName, const char *pTextureGroupName )
{
	g_ShaderAPIDX8.CShaderAPIDx8::CreateTextures(pHandles,count,width,height,depth,dstImageFormat,numMipLevels,numCopies,flags,pDebugName,pTextureGroupName);
}

void CPs3NonVirt_IShaderAPIDX8::AcquireThreadOwnership()
{
	g_ShaderAPIDX8.CShaderAPIDx8::AcquireThreadOwnership();
}

void CPs3NonVirt_IShaderAPIDX8::ReleaseThreadOwnership()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ReleaseThreadOwnership();
}

void CPs3NonVirt_IShaderAPIDX8::EnableBuffer2FramesAhead( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableBuffer2FramesAhead(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::FlipCulling( bool bFlipCulling )
{
	g_ShaderAPIDX8.CShaderAPIDx8::FlipCulling(bFlipCulling);
}

void CPs3NonVirt_IShaderAPIDX8::SetTextureRenderingParameter(int parm_number, ITexture *pTexture)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetTextureRenderingParameter(parm_number,pTexture);
}

void CPs3NonVirt_IShaderAPIDX8::EnableSinglePassFlashlightMode( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableSinglePassFlashlightMode(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::MatrixMode( MaterialMatrixMode_t matrixMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::MatrixMode(matrixMode);
}

void CPs3NonVirt_IShaderAPIDX8::PushMatrix()
{
	g_ShaderAPIDX8.CShaderAPIDx8::PushMatrix();
}

void CPs3NonVirt_IShaderAPIDX8::PopMatrix()
{
	g_ShaderAPIDX8.CShaderAPIDx8::PopMatrix();
}

void CPs3NonVirt_IShaderAPIDX8::LoadMatrix( float *m )
{
	g_ShaderAPIDX8.CShaderAPIDx8::LoadMatrix(m);
}

void CPs3NonVirt_IShaderAPIDX8::MultMatrix( float *m )
{
	g_ShaderAPIDX8.CShaderAPIDx8::MultMatrix(m);
}

void CPs3NonVirt_IShaderAPIDX8::MultMatrixLocal( float *m )
{
	g_ShaderAPIDX8.CShaderAPIDx8::MultMatrixLocal(m);
}

void CPs3NonVirt_IShaderAPIDX8::LoadIdentity()
{
	g_ShaderAPIDX8.CShaderAPIDx8::LoadIdentity();
}

void CPs3NonVirt_IShaderAPIDX8::LoadCameraToWorld()
{
	g_ShaderAPIDX8.CShaderAPIDx8::LoadCameraToWorld();
}

void CPs3NonVirt_IShaderAPIDX8::Ortho( double left, double right, double bottom, double top, double zNear, double zFar )
{
	g_ShaderAPIDX8.CShaderAPIDx8::Ortho(left,right,bottom,top,zNear,zFar);
}

void CPs3NonVirt_IShaderAPIDX8::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PerspectiveX(fovx,aspect,zNear,zFar);
}

void CPs3NonVirt_IShaderAPIDX8::PickMatrix( int x, int y, int width, int height )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PickMatrix(x,y,width,height);
}

void CPs3NonVirt_IShaderAPIDX8::Rotate( float angle, float x, float y, float z )
{
	g_ShaderAPIDX8.CShaderAPIDx8::Rotate(angle,x,y,z);
}

void CPs3NonVirt_IShaderAPIDX8::Translate( float x, float y, float z )
{
	g_ShaderAPIDX8.CShaderAPIDx8::Translate(x,y,z);
}

void CPs3NonVirt_IShaderAPIDX8::Scale( float x, float y, float z )
{
	g_ShaderAPIDX8.CShaderAPIDx8::Scale(x,y,z);
}

void CPs3NonVirt_IShaderAPIDX8::ScaleXY( float x, float y )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ScaleXY(x,y);
}

void CPs3NonVirt_IShaderAPIDX8::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PerspectiveOffCenterX(fovx,aspect,zNear,zFar,bottom,top,left,right);
}

void CPs3NonVirt_IShaderAPIDX8::LoadBoneMatrix( int boneIndex, const float *m )
{
	g_ShaderAPIDX8.CShaderAPIDx8::LoadBoneMatrix(boneIndex,m);
}

void CPs3NonVirt_IShaderAPIDX8::SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetStandardTextureHandle(nId,nHandle);
}

void CPs3NonVirt_IShaderAPIDX8::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
{
	g_ShaderAPIDX8.CShaderAPIDx8::DrawInstances(nInstanceCount,pInstance);
}

void CPs3NonVirt_IShaderAPIDX8::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::OverrideAlphaWriteEnable(bOverrideEnable,bAlphaWriteEnable);
}

void CPs3NonVirt_IShaderAPIDX8::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::OverrideColorWriteEnable(bOverrideEnable,bColorWriteEnable);
}

void CPs3NonVirt_IShaderAPIDX8::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ClearBuffersObeyStencilEx(bClearColor,bClearAlpha,bClearDepth);
}

void CPs3NonVirt_IShaderAPIDX8::OnPresent()
{
	g_ShaderAPIDX8.CShaderAPIDx8::OnPresent();
}

void CPs3NonVirt_IShaderAPIDX8::UpdateGameTime( float flTime )
{
	g_ShaderAPIDX8.CShaderAPIDx8::UpdateGameTime(flTime);
}

double CPs3NonVirt_IShaderAPIDX8::CurrentTime()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::CurrentTime();
}

void CPs3NonVirt_IShaderAPIDX8::GetLightmapDimensions( int *w, int *h )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetLightmapDimensions(w,h);
}

MaterialFogMode_t CPs3NonVirt_IShaderAPIDX8::GetSceneFogMode( )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetSceneFogMode();
}

void CPs3NonVirt_IShaderAPIDX8::SetVertexShaderConstant( int var, float const* pVec, int numConst, bool bForce)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetVertexShaderConstant(var,pVec,numConst,bForce);
}

void CPs3NonVirt_IShaderAPIDX8::SetPixelShaderConstant( int var, float const* pVec, int numConst, bool bForce)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetPixelShaderConstant(var,pVec,numConst,bForce);
}

void CPs3NonVirt_IShaderAPIDX8::SetDefaultState()
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetDefaultState();
}

void CPs3NonVirt_IShaderAPIDX8::GetWorldSpaceCameraPosition( float* pPos )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetWorldSpaceCameraPosition(pPos);
}

void CPs3NonVirt_IShaderAPIDX8::GetWorldSpaceCameraDirection( float* pDir )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetWorldSpaceCameraDirection(pDir);
}

int CPs3NonVirt_IShaderAPIDX8::GetCurrentNumBones()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentNumBones();
}

MaterialFogMode_t CPs3NonVirt_IShaderAPIDX8::GetCurrentFogType()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentFogType();
}

void CPs3NonVirt_IShaderAPIDX8::SetVertexShaderIndex( int vshIndex)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetVertexShaderIndex(vshIndex);
}

void CPs3NonVirt_IShaderAPIDX8::SetPixelShaderIndex( int pshIndex)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetPixelShaderIndex(pshIndex);
}

void CPs3NonVirt_IShaderAPIDX8::GetBackBufferDimensions( int& width, int& height )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetBackBufferDimensions(width,height);
}

const AspectRatioInfo_t &CPs3NonVirt_IShaderAPIDX8::GetAspectRatioInfo( void )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetAspectRatioInfo();
}

void CPs3NonVirt_IShaderAPIDX8::GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentRenderTargetDimensions(nWidth,nHeight);
}

void CPs3NonVirt_IShaderAPIDX8::GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentViewport(nX,nY,nWidth,nHeight);
}

void CPs3NonVirt_IShaderAPIDX8::SetPixelShaderFogParams( int reg )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetPixelShaderFogParams(reg);
}

bool CPs3NonVirt_IShaderAPIDX8::InFlashlightMode()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::InFlashlightMode();
}

const FlashlightState_t & CPs3NonVirt_IShaderAPIDX8::GetFlashlightState( VMatrix &worldToTexture )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetFlashlightState(worldToTexture);
}

bool CPs3NonVirt_IShaderAPIDX8::InEditorMode()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::InEditorMode();
}

void CPs3NonVirt_IShaderAPIDX8::BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindStandardTexture( sampler, nBindFlags, id );
}

ITexture * CPs3NonVirt_IShaderAPIDX8::GetRenderTargetEx( int nRenderTargetID )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetRenderTargetEx(nRenderTargetID);
}

void CPs3NonVirt_IShaderAPIDX8::SetToneMappingScaleLinear( const Vector &scale )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetToneMappingScaleLinear(scale);
}

const Vector & CPs3NonVirt_IShaderAPIDX8::GetToneMappingScaleLinear()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetToneMappingScaleLinear();
}

const FlashlightState_t & CPs3NonVirt_IShaderAPIDX8::GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetFlashlightStateEx(worldToTexture,pFlashlightDepthTexture);
}

void CPs3NonVirt_IShaderAPIDX8::GetDX9LightState( LightState_t *state )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetDX9LightState(state);
}

int CPs3NonVirt_IShaderAPIDX8::GetPixelFogCombo( )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetPixelFogCombo();
}

void CPs3NonVirt_IShaderAPIDX8::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BindStandardVertexTexture(sampler,id);
}

bool CPs3NonVirt_IShaderAPIDX8::IsHWMorphingEnabled( )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsHWMorphingEnabled();
}

void CPs3NonVirt_IShaderAPIDX8::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetStandardTextureDimensions(pWidth,pHeight,id);
}

void CPs3NonVirt_IShaderAPIDX8::SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetBooleanVertexShaderConstant(var,pVec,numBools,bForce);
}

void CPs3NonVirt_IShaderAPIDX8::SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetIntegerVertexShaderConstant(var,pVec,numIntVecs,bForce);
}

void CPs3NonVirt_IShaderAPIDX8::SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetBooleanPixelShaderConstant(var,pVec,numBools,bForce);
}

void CPs3NonVirt_IShaderAPIDX8::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetIntegerPixelShaderConstant(var,pVec,numIntVecs,bForce);
}

bool CPs3NonVirt_IShaderAPIDX8::ShouldWriteDepthToDestAlpha()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::ShouldWriteDepthToDestAlpha();
}

void CPs3NonVirt_IShaderAPIDX8::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetMatrix(matrixMode,dst);
}

void CPs3NonVirt_IShaderAPIDX8::PushDeformation( DeformationBase_t const *Deformation )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PushDeformation(Deformation);
}

void CPs3NonVirt_IShaderAPIDX8::PopDeformation( )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PopDeformation();
}

int CPs3NonVirt_IShaderAPIDX8::GetNumActiveDeformations()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetNumActiveDeformations();
}

int CPs3NonVirt_IShaderAPIDX8::GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations, float *pConstantValuesOut, int nBufferSize, int nMaximumDeformations, int *pNumDefsOut )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetPackedDeformationInformation(nMaskOfUnderstoodDeformations,pConstantValuesOut,nBufferSize,nMaximumDeformations,pNumDefsOut);
}

void CPs3NonVirt_IShaderAPIDX8::MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords )
{
	g_ShaderAPIDX8.CShaderAPIDx8::MarkUnusedVertexFields(nFlags,nTexCoordCount,pUnusedTexCoords);
}

void CPs3NonVirt_IShaderAPIDX8::ExecuteCommandBuffer( uint8 *pCmdBuffer )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ExecuteCommandBuffer(pCmdBuffer);
}

void CPs3NonVirt_IShaderAPIDX8::ExecuteCommandBufferPPU( uint8 *pCmdBuffer )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ExecuteCommandBufferPPU(pCmdBuffer);
}


void CPs3NonVirt_IShaderAPIDX8::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentColorCorrection(pInfo);
}

ITexture * CPs3NonVirt_IShaderAPIDX8::GetTextureRenderingParameter(int parm_number)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetTextureRenderingParameter(parm_number);
}

void CPs3NonVirt_IShaderAPIDX8::SetScreenSizeForVPOS( int pshReg)
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetScreenSizeForVPOS(pshReg);
}

void CPs3NonVirt_IShaderAPIDX8::SetVSNearAndFarZ( int vshReg )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetVSNearAndFarZ(vshReg);
}

float CPs3NonVirt_IShaderAPIDX8::GetFarZ()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetFarZ();
}

bool CPs3NonVirt_IShaderAPIDX8::SinglePassFlashlightModeEnabled()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::SinglePassFlashlightModeEnabled();
}

void CPs3NonVirt_IShaderAPIDX8::GetActualProjectionMatrix( float *pMatrix )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetActualProjectionMatrix( pMatrix );
}

void CPs3NonVirt_IShaderAPIDX8::SetDepthFeatheringPixelShaderConstant( int iConstant, float fDepthBlendScale )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetDepthFeatheringPixelShaderConstant(iConstant,fDepthBlendScale);
}

void CPs3NonVirt_IShaderAPIDX8::GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetFlashlightShaderInfo(pShadowsEnabled,pUberLight);
}

float CPs3NonVirt_IShaderAPIDX8::GetFlashlightAmbientOcclusion( )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetFlashlightAmbientOcclusion();
}

void CPs3NonVirt_IShaderAPIDX8::SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetTextureFilterMode(sampler,nMode);
}

TessellationMode_t CPs3NonVirt_IShaderAPIDX8::GetTessellationMode()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetTessellationMode();
}

float CPs3NonVirt_IShaderAPIDX8::GetSubDHeight()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetSubDHeight();
}

bool CPs3NonVirt_IShaderAPIDX8::IsRenderingPaint()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsRenderingPaint();
}

bool CPs3NonVirt_IShaderAPIDX8::IsStereoActiveThisFrame()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsStereoActiveThisFrame();
}

bool CPs3NonVirt_IShaderAPIDX8::OnDeviceInit()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::OnDeviceInit();
}

void CPs3NonVirt_IShaderAPIDX8::OnDeviceShutdown()
{
	g_ShaderAPIDX8.CShaderAPIDx8::OnDeviceShutdown();
}

void CPs3NonVirt_IShaderAPIDX8::AdvancePIXFrame()
{
	g_ShaderAPIDX8.CShaderAPIDx8::AdvancePIXFrame();
}

void CPs3NonVirt_IShaderAPIDX8::ReleaseShaderObjects( bool bReleaseManagedResources)
{
	g_ShaderAPIDX8.CShaderAPIDx8::ReleaseShaderObjects(bReleaseManagedResources);
}

void CPs3NonVirt_IShaderAPIDX8::RestoreShaderObjects()
{
	g_ShaderAPIDX8.CShaderAPIDx8::RestoreShaderObjects();
}

IDirect3DBaseTexture* CPs3NonVirt_IShaderAPIDX8::GetD3DTexture( ShaderAPITextureHandle_t hTexture )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetD3DTexture(hTexture);
}

void CPs3NonVirt_IShaderAPIDX8::GetPs3Texture(void* pPs3tex, ShaderAPITextureHandle_t hTexture )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetPs3Texture(pPs3tex, hTexture);
}

void CPs3NonVirt_IShaderAPIDX8::GetPs3Texture(void* pPs3tex, StandardTextureId_t nTextureId  )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetPs3Texture(pPs3tex, nTextureId);
}


void CPs3NonVirt_IShaderAPIDX8::QueueResetRenderState()
{
	g_ShaderAPIDX8.CShaderAPIDx8::QueueResetRenderState();
}

void CPs3NonVirt_IShaderAPIDX8::DrawMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo )
{
	g_ShaderAPIDX8.CShaderAPIDx8::DrawMesh(pMesh,nCount,pInstances,nCompressionType,pCompiledState,pInfo);
}

void CPs3NonVirt_IShaderAPIDX8::DrawWithVertexAndIndexBuffers()
{
	g_ShaderAPIDX8.CShaderAPIDx8::DrawWithVertexAndIndexBuffers();
}

void CPs3NonVirt_IShaderAPIDX8::GetBufferedState( BufferedState_t &state )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GetBufferedState(state);
}

D3DCULL CPs3NonVirt_IShaderAPIDX8::GetCullMode()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetCullMode();
}

void CPs3NonVirt_IShaderAPIDX8::ComputeFillRate()
{
	g_ShaderAPIDX8.CShaderAPIDx8::ComputeFillRate();
}

bool CPs3NonVirt_IShaderAPIDX8::IsInSelectionMode()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsInSelectionMode();
}

void CPs3NonVirt_IShaderAPIDX8::RegisterSelectionHit( float minz, float maxz )
{
	g_ShaderAPIDX8.CShaderAPIDx8::RegisterSelectionHit(minz,maxz);
}

IMaterialInternal* CPs3NonVirt_IShaderAPIDX8::GetBoundMaterial()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetBoundMaterial();
}

void CPs3NonVirt_IShaderAPIDX8::ApplyZBias( const DepthTestState_t& shaderState )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ApplyZBias(shaderState);
}

void CPs3NonVirt_IShaderAPIDX8::ApplyCullEnable( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ApplyCullEnable(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableGammaCorrection )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ApplyFogMode(fogMode,bVertexFog,bSRGBWritesEnabled,bDisableGammaCorrection);
}

int CPs3NonVirt_IShaderAPIDX8::GetActualSamplerCount()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetActualSamplerCount();
}

bool CPs3NonVirt_IShaderAPIDX8::IsRenderingMesh()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::IsRenderingMesh();
}

void CPs3NonVirt_IShaderAPIDX8::EnableFixedFunctionFog( bool bFogEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnableFixedFunctionFog(bFogEnable);
}

int CPs3NonVirt_IShaderAPIDX8::GetCurrentFrameCounter()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetCurrentFrameCounter();
}

void CPs3NonVirt_IShaderAPIDX8::SetupSelectionModeVisualizationState()
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetupSelectionModeVisualizationState();
}

bool CPs3NonVirt_IShaderAPIDX8::UsingSoftwareVertexProcessing()
{
	return g_ShaderAPIDX8.CShaderAPIDx8::UsingSoftwareVertexProcessing();
}

void CPs3NonVirt_IShaderAPIDX8::EnabledSRGBWrite( bool bEnabled )
{
	g_ShaderAPIDX8.CShaderAPIDx8::EnabledSRGBWrite(bEnabled);
}

void CPs3NonVirt_IShaderAPIDX8::ApplyAlphaToCoverage( bool bEnable )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ApplyAlphaToCoverage(bEnable);
}

void CPs3NonVirt_IShaderAPIDX8::PrintfVA( char *fmt, va_list vargs )
{
	g_ShaderAPIDX8.CShaderAPIDx8::PrintfVA(fmt,vargs);
}

float CPs3NonVirt_IShaderAPIDX8::Knob( char *knobname, float *setvalue)
{
	return g_ShaderAPIDX8.CShaderAPIDx8::Knob(knobname,setvalue);
}

void CPs3NonVirt_IShaderAPIDX8::NotifyShaderConstantsChangedInRenderPass()
{
	g_ShaderAPIDX8.CShaderAPIDx8::NotifyShaderConstantsChangedInRenderPass();
}

void CPs3NonVirt_IShaderAPIDX8::GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** pCompiledState, InstanceInfo_t **pInfo )
{
	g_ShaderAPIDX8.CShaderAPIDx8::GenerateNonInstanceRenderState(pInstance,pCompiledState,pInfo);
}

void CPs3NonVirt_IShaderAPIDX8::ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet )
{
	g_ShaderAPIDX8.CShaderAPIDx8::ExecuteInstanceCommandBuffer(pCmdBuf,nInstanceIndex,bForceStateSet);
}

void CPs3NonVirt_IShaderAPIDX8::SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetVertexDecl(vertexFormat,bHasColorMesh,bUsingFlex,bUsingMorph,bUsingPreTessPatch,pStreamSpec);
}

void CPs3NonVirt_IShaderAPIDX8::SetTessellationMode( TessellationMode_t mode )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetTessellationMode(mode);
}

IMesh * CPs3NonVirt_IShaderAPIDX8::GetExternalMesh( const ExternalMeshInfo_t& info )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetExternalMesh(info);
}

void CPs3NonVirt_IShaderAPIDX8::SetExternalMeshData( IMesh *pMesh, const ExternalMeshData_t &data )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetExternalMeshData(pMesh,data);
}

IIndexBuffer * CPs3NonVirt_IShaderAPIDX8::GetExternalIndexBuffer( int nIndexCount, uint16 *pIndexData )
{
	return g_ShaderAPIDX8.CShaderAPIDx8::GetExternalIndexBuffer(nIndexCount,pIndexData);
}

void CPs3NonVirt_IShaderAPIDX8::FlushGPUCache( void *pBaseAddr, size_t nSizeInBytes )
{
	g_ShaderAPIDX8.CShaderAPIDx8::FlushGPUCache(pBaseAddr,nSizeInBytes);
}

void CPs3NonVirt_IShaderAPIDX8::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{
	g_ShaderAPIDX8.CShaderAPIDx8::AddShaderComboInformation(pSemantics);
}

void CPs3NonVirt_IShaderAPIDX8::BeginConsoleZPass2( int nNumDynamicIndicesNeeded )
{
	g_ShaderAPIDX8.CShaderAPIDx8::BeginConsoleZPass2( nNumDynamicIndicesNeeded );
}

void CPs3NonVirt_IShaderAPIDX8::EndConsoleZPass()
{
	g_ShaderAPIDX8.CShaderAPIDx8::EndConsoleZPass( );
}

void CPs3NonVirt_IShaderAPIDX8::SetSRGBWrite( bool bState )
{
	g_ShaderAPIDX8.CShaderAPIDx8::SetSRGBWrite( bState );
}


ShaderAPITextureHandle_t CPs3NonVirt_IShaderSystem::GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel)
{
	return s_ShaderSystem.CShaderSystem::GetShaderAPITextureBindHandle(pTexture,nFrameVar,nTextureChannel);
}

void CPs3NonVirt_IShaderSystem::BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar)
{
	s_ShaderSystem.CShaderSystem::BindTexture(sampler1, nBindFlags, pTexture,nFrameVar);
}

void CPs3NonVirt_IShaderSystem::BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar)
{
	s_ShaderSystem.CShaderSystem::BindTexture(sampler1, sampler2, nBindFlags, pTexture, nFrameVar );
}

void CPs3NonVirt_IShaderSystem::TakeSnapshot()
{
	s_ShaderSystem.CShaderSystem::TakeSnapshot();
}

void CPs3NonVirt_IShaderSystem::DrawSnapshot( const unsigned char *pInstanceCommandBuffer, bool bMakeActualDrawCall)
{
	s_ShaderSystem.CShaderSystem::DrawSnapshot(pInstanceCommandBuffer,bMakeActualDrawCall);
}

bool CPs3NonVirt_IShaderSystem::IsUsingGraphics()
{
	return s_ShaderSystem.CShaderSystem::IsUsingGraphics();
}

bool CPs3NonVirt_IShaderSystem::CanUseEditorMaterials()
{
	return s_ShaderSystem.CShaderSystem::CanUseEditorMaterials();
}

void CPs3NonVirt_IShaderSystem::BindVertexTexture( VertexTextureSampler_t vtSampler, ITexture *pTexture, int nFrameVar)
{
	s_ShaderSystem.CShaderSystem::BindVertexTexture(vtSampler,pTexture,nFrameVar);
}


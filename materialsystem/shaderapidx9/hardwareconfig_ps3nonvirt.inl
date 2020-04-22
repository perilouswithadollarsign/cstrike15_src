bool CPs3NonVirt_IHardwareConfigInternal::HasSetDeviceGammaRamp()
{
	return g_pHardwareConfig->CHardwareConfig::HasSetDeviceGammaRamp();
}

VertexCompressionType_t CPs3NonVirt_IHardwareConfigInternal::SupportsCompressedVertices()
{
	return g_pHardwareConfig->CHardwareConfig::SupportsCompressedVertices();
}

int CPs3NonVirt_IHardwareConfigInternal::MaximumAnisotropicLevel()
{
	return g_pHardwareConfig->CHardwareConfig::MaximumAnisotropicLevel();
}

int CPs3NonVirt_IHardwareConfigInternal::MaxTextureWidth()
{
	return g_pHardwareConfig->CHardwareConfig::MaxTextureWidth();
}

int CPs3NonVirt_IHardwareConfigInternal::MaxTextureHeight()
{
	return g_pHardwareConfig->CHardwareConfig::MaxTextureHeight();
}

int CPs3NonVirt_IHardwareConfigInternal::TextureMemorySize()
{
	return g_pHardwareConfig->CHardwareConfig::TextureMemorySize();
}

bool CPs3NonVirt_IHardwareConfigInternal::SupportsMipmappedCubemaps()
{
	return g_pHardwareConfig->CHardwareConfig::SupportsMipmappedCubemaps();
}

int CPs3NonVirt_IHardwareConfigInternal::MaxTextureAspectRatio()
{
	return g_pHardwareConfig->CHardwareConfig::MaxTextureAspectRatio();
}

int CPs3NonVirt_IHardwareConfigInternal::MaxVertexShaderBlendMatrices()
{
	return g_pHardwareConfig->CHardwareConfig::MaxVertexShaderBlendMatrices();
}

bool CPs3NonVirt_IHardwareConfigInternal::UseFastClipping()
{
	return g_pHardwareConfig->CHardwareConfig::UseFastClipping();
}

bool CPs3NonVirt_IHardwareConfigInternal::ReadPixelsFromFrontBuffer()
{
	return g_pHardwareConfig->CHardwareConfig::ReadPixelsFromFrontBuffer();
}

bool CPs3NonVirt_IHardwareConfigInternal::PreferDynamicTextures()
{
	return g_pHardwareConfig->CHardwareConfig::PreferDynamicTextures();
}

bool CPs3NonVirt_IHardwareConfigInternal::NeedsAAClamp()
{
	return g_pHardwareConfig->CHardwareConfig::NeedsAAClamp();
}

bool CPs3NonVirt_IHardwareConfigInternal::SpecifiesFogColorInLinearSpace()
{
	return g_pHardwareConfig->CHardwareConfig::SpecifiesFogColorInLinearSpace();
}

int CPs3NonVirt_IHardwareConfigInternal::GetVertexSamplerCount()
{
	return g_pHardwareConfig->CHardwareConfig::GetVertexSamplerCount();
}

int CPs3NonVirt_IHardwareConfigInternal::GetMaxVertexTextureDimension()
{
	return g_pHardwareConfig->CHardwareConfig::GetMaxVertexTextureDimension();
}

int CPs3NonVirt_IHardwareConfigInternal::MaxTextureDepth()
{
	return g_pHardwareConfig->CHardwareConfig::MaxTextureDepth();
}

bool CPs3NonVirt_IHardwareConfigInternal::SupportsStreamOffset()
{
	return g_pHardwareConfig->CHardwareConfig::SupportsStreamOffset();
}

int CPs3NonVirt_IHardwareConfigInternal::StencilBufferBits()
{
	return g_pHardwareConfig->CHardwareConfig::StencilBufferBits();
}

int CPs3NonVirt_IHardwareConfigInternal::MaxViewports()
{
	return g_pHardwareConfig->CHardwareConfig::MaxViewports();
}

void CPs3NonVirt_IHardwareConfigInternal::OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport )
{
	g_pHardwareConfig->CHardwareConfig::OverrideStreamOffsetSupport(bOverrideEnabled,bEnableSupport);
}

int CPs3NonVirt_IHardwareConfigInternal::MaxHWMorphBatchCount()
{
	return g_pHardwareConfig->CHardwareConfig::MaxHWMorphBatchCount();
}

float CPs3NonVirt_IHardwareConfigInternal::GetShadowDepthBias()
{
	return g_pHardwareConfig->CHardwareConfig::GetShadowDepthBias();
}

float CPs3NonVirt_IHardwareConfigInternal::GetShadowSlopeScaleDepthBias()
{
	return g_pHardwareConfig->CHardwareConfig::GetShadowSlopeScaleDepthBias();
}

bool CPs3NonVirt_IHardwareConfigInternal::PreferZPrepass()
{
	return g_pHardwareConfig->CHardwareConfig::PreferZPrepass();
}

bool CPs3NonVirt_IHardwareConfigInternal::SuppressPixelShaderCentroidHackFixup()
{
	return g_pHardwareConfig->CHardwareConfig::SuppressPixelShaderCentroidHackFixup();
}

bool CPs3NonVirt_IHardwareConfigInternal::PreferTexturesInHWMemory()
{
	return g_pHardwareConfig->CHardwareConfig::PreferTexturesInHWMemory();
}

bool CPs3NonVirt_IHardwareConfigInternal::PreferHardwareSync()
{
	return g_pHardwareConfig->CHardwareConfig::PreferHardwareSync();
}

bool CPs3NonVirt_IHardwareConfigInternal::SupportsShadowDepthTextures()
{
	return g_pHardwareConfig->CHardwareConfig::SupportsShadowDepthTextures();
}

ImageFormat CPs3NonVirt_IHardwareConfigInternal::GetShadowDepthTextureFormat()
{
	return g_pHardwareConfig->CHardwareConfig::GetShadowDepthTextureFormat();
}

ImageFormat CPs3NonVirt_IHardwareConfigInternal::GetHighPrecisionShadowDepthTextureFormat()
{
	return g_pHardwareConfig->CHardwareConfig::GetHighPrecisionShadowDepthTextureFormat();
}

ImageFormat CPs3NonVirt_IHardwareConfigInternal::GetNullTextureFormat()
{
	return g_pHardwareConfig->CHardwareConfig::GetNullTextureFormat();
}

float CPs3NonVirt_IHardwareConfigInternal::GetLightMapScaleFactor()
{
	return g_pHardwareConfig->CHardwareConfig::GetLightMapScaleFactor();
}

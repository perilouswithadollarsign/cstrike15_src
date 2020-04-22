//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
#ifndef HARDWARECONFIG_PS3NONVIRT_H
#define HARDWARECONFIG_PS3NONVIRT_H

#ifdef _PS3

#include "ihardwareconfiginternal.h"

//////////////////////////////////////////////////////////////////////////
//
// PS3 non-virtual implementation proxy
//
// cat hardwareconfig_ps3nonvirt.h | nonvirtualscript.pl > hardwareconfig_ps3nonvirt.inl
struct CPs3NonVirt_IHardwareConfigInternal
{
//NONVIRTUALSCRIPTBEGIN
//NONVIRTUALSCRIPT/PROXY/CPs3NonVirt_IHardwareConfigInternal
//NONVIRTUALSCRIPT/DELEGATE/g_pHardwareConfig->CHardwareConfig::

	//
	// IMaterialSystemHardwareConfig
	//
	static bool HasSetDeviceGammaRamp();
	static VertexCompressionType_t SupportsCompressedVertices();
	static int  MaximumAnisotropicLevel();
	static int  MaxTextureWidth();
	static int  MaxTextureHeight();
	static int	 TextureMemorySize();
	static bool SupportsMipmappedCubemaps();
	static int	 MaxTextureAspectRatio();
	static int	 MaxVertexShaderBlendMatrices();
	static bool UseFastClipping();
	static bool ReadPixelsFromFrontBuffer();
	static bool PreferDynamicTextures();
	static bool NeedsAAClamp();
	static bool SpecifiesFogColorInLinearSpace();
	static bool IsAAEnabled();	// Is antialiasing being used?
	static int GetVertexSamplerCount();
	static int GetMaxVertexTextureDimension();
	static int  MaxTextureDepth();
	static bool SupportsStreamOffset();
	static int StencilBufferBits();
	static int MaxViewports();
	static void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport );
	static int MaxHWMorphBatchCount();
	static float GetShadowDepthBias();
	static float GetShadowSlopeScaleDepthBias();
	static bool PreferZPrepass();
	static bool SuppressPixelShaderCentroidHackFixup();
	static bool PreferTexturesInHWMemory();
	static bool PreferHardwareSync();
	static bool SupportsShadowDepthTextures();
	static ImageFormat GetShadowDepthTextureFormat();
	static ImageFormat GetHighPrecisionShadowDepthTextureFormat();
	static ImageFormat GetNullTextureFormat();
	static float GetLightMapScaleFactor();

//NONVIRTUALSCRIPTEND

	//
	// Predefined implementation
	//
	static inline bool SupportsStaticControlFlow() { return true; }
	static inline bool FakeSRGBWrite() { return false; }
	static inline bool CanDoSRGBReadFromRTs() { return true; }
	static inline bool SupportsGLMixedSizeTargets() { return true; }
	static inline int MaxNumLights() { return MAX_NUM_LIGHTS; }
	static inline int MaxUserClipPlanes() { return 0; }
	static inline ShadowFilterMode_t GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) { return SHADOWFILTERMODE_DEFAULT; } // PCF filter
	static inline bool SupportsHDRMode( HDRType_t nHDRMode ) { return ( nHDRMode == HDR_TYPE_NONE ) || ( nHDRMode == HDR_TYPE_INTEGER ); }
	static inline HDRType_t GetHDRType() { return HDR_TYPE_INTEGER; }
	static inline HDRType_t GetHardwareHDRType() { return HDR_TYPE_INTEGER; }
	static inline bool HasFastVertexTextures() { return false; }
	static inline bool ActualHasFastVertexTextures() { return false; }
	static int NeedsShaderSRGBConversion() { return false; }
	static inline bool SupportsBorderColor() { return true; }
	static inline bool SupportsFetch4() { return false; }
	static inline bool NeedsATICentroidHack() { return false; }
	static inline int	 NumVertexShaderConstants() { return 256; }
	static inline int	 NumPixelShaderConstants() { return MAX_FRAGMENT_PROGRAM_CONSTS; }
	static inline bool GetHDREnabled() { return true; }
	static inline void SetHDREnabled( bool bEnable ) {}
	static inline bool IsUnsupported() { return false; }
	static inline int GetFrameBufferColorDepth() { return 4; }
	static inline int GetSamplerCount() { return 16; }
	static inline int GetDXSupportLevel() { return 98; }
	static inline int GetMaxDXSupportLevel() { return GetDXSupportLevel(); }
	static inline int GetMinDXSupportLevel() { return GetDXSupportLevel(); }
	static inline bool SupportsHDR() { return true; }
	static inline bool SupportsSRGB() { return true; }
	static inline bool UsesSRGBCorrectBlending() { return IsX360(); }
	static inline bool SupportsPixelShaders_2_b() { return true; }
	static inline const char *GetShaderDLLName() { return "shaderapidx9"; }
	static inline const char *GetHWSpecificShaderDLLName() { return "shaderapidx9"; }
	static inline bool SupportsCascadedShadowMapping() { return false; }
	static inline bool SupportsBilinearPCFSampling() { return true; }
};

inline CPs3NonVirt_IHardwareConfigInternal* HardwareConfig()
{	
	return ( CPs3NonVirt_IHardwareConfigInternal * ) 1;
}

#endif


#endif

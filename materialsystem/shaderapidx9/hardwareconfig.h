//===== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef HARDWARECONFIG_H
#define HARDWARECONFIG_H

#ifdef _WIN32
#pragma once
#endif


#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "IHardwareConfigInternal.h"
#include "bitmap/imageformat.h"
#include "materialsystem/imaterialsystem.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct ShaderDeviceInfo_t;


//-----------------------------------------------------------------------------
// Vendor IDs sometimes needed for vendor-specific code
//-----------------------------------------------------------------------------
#define VENDORID_NVIDIA	0x10DE
#define VENDORID_ATI	0x1002
#define VENDORID_INTEL  0x8086

//-----------------------------------------------------------------------------
// ShaderAPI constants
//-----------------------------------------------------------------------------

enum
{
#if defined( DX_TO_GL_ABSTRACTION )
	MAXUSERCLIPPLANES = 2,
    
#if defined( OSX )
    MAX_NUM_LIGHTS = 2,
#else
    MAX_NUM_LIGHTS = 4,
#endif

#else
	MAXUSERCLIPPLANES = 6,
	MAX_NUM_LIGHTS = 4,
#endif	

	MAX_OUTPUTS = 3,

#if defined( _PS3 )
	MAX_FRAGMENT_PROGRAM_CONSTS = 96,
#endif
};

//-----------------------------------------------------------------------------
// Hardware caps structures
//-----------------------------------------------------------------------------
enum CompressedTextureState_t
{
	COMPRESSED_TEXTURES_ON,
	COMPRESSED_TEXTURES_OFF,
	COMPRESSED_TEXTURES_NOT_INITIALIZED
};

struct HardwareCaps_t : public MaterialAdapterInfo_t
{
	// *****************************NOTE*********************************************
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// *****************************NOTE*********************************************
	// 
	// NOTE: Texture stages are an obsolete concept; used by fixed-function hardware
	// Samplers are dx9+, indicating how many textures we can simultaneously bind
	// In Dx8, samplers didn't exist and texture stages were used to indicate the
	// number of simultaneously bound textures; we'll emulate that by slamming
	// the number of samplers to == the number of texture stages.
	CompressedTextureState_t m_SupportsCompressedTextures;
	VertexCompressionType_t m_SupportsCompressedVertices;
	int  m_NumSamplers;
	int  m_NumVertexSamplers;
	int	 m_nMaxAnisotropy;
	int  m_MaxTextureWidth;
	int  m_MaxTextureHeight;
	int  m_MaxTextureDepth;
	int	 m_MaxTextureAspectRatio;
	int  m_MaxPrimitiveCount;
	int  m_NumPixelShaderConstants;
	int  m_NumBooleanPixelShaderConstants;
	int  m_NumIntegerPixelShaderConstants;
	int  m_NumVertexShaderConstants;
	int  m_NumBooleanVertexShaderConstants;
	int  m_NumIntegerVertexShaderConstants;
	int  m_TextureMemorySize;
	int  m_MaxNumLights;
	int	 m_MaxVertexShaderBlendMatrices;
	int  m_MaxUserClipPlanes;
	HDRType_t m_HDRType;
	char m_pShaderDLL[32];
	ImageFormat m_ShadowDepthTextureFormat;
	ImageFormat m_HighPrecisionShadowDepthTextureFormat;
	ImageFormat m_NullTextureFormat;
	int  m_nMaxVertexTextureDimension;
	unsigned long m_AlphaToCoverageState;					// State to ping to toggle Alpha To Coverage              (vendor-dependent)
	unsigned long m_AlphaToCoverageEnableValue;				// Value to set above state to turn on Alpha To Coverage  (vendor-dependent)
	unsigned long m_AlphaToCoverageDisableValue;			// Value to set above state to turn off Alpha To Coverage (vendor-dependent)
	int	 m_nMaxViewports;
	float m_flMinGammaControlPoint;
	float m_flMaxGammaControlPoint;
	int m_nGammaControlPointCount;
	int m_MaxVertexShader30InstructionSlots;
	int m_MaxPixelShader30InstructionSlots;
	int m_MaxSimultaneousRenderTargets;

	bool m_bDeviceOk : 1;
	bool m_HasSetDeviceGammaRamp : 1;
	bool m_SupportsPixelShaders_2_b : 1;
	bool m_SupportsShaderModel_3_0 : 1;
	bool m_bSupportsAnisotropicFiltering : 1;
	bool m_bSupportsMagAnisotropicFiltering : 1;
	bool m_bSupportsVertexTextures : 1;
	bool m_ZBiasAndSlopeScaledDepthBiasSupported : 1;
	bool m_SupportsMipmappedCubemaps : 1;
	bool m_PreferDynamicTextures : 1;
	bool m_SupportsSRGB : 1;
	bool m_UseFastClipping : 1;
	bool m_bNeedsATICentroidHack : 1;
	bool m_bDisableShaderOptimizations : 1;
	bool m_bSupportsStreamOffset : 1;
	bool m_bFogColorSpecifiedInLinearSpace : 1;
	bool m_bFogColorAlwaysLinearSpace : 1;
	bool m_bSupportsAlphaToCoverage : 1;
	bool m_bSupportsShadowDepthTextures : 1;
	bool m_bSupportsFetch4 : 1;
	bool m_bSoftwareVertexProcessing : 1;
	bool m_bScissorSupported : 1;
	bool m_bSupportsFloat32RenderTargets : 1;
	bool m_bSupportsBorderColor : 1;
	bool m_bDX10Card : 1;							// Indicates DX10 part with performant vertex textures running DX9 path
	bool m_bDX10Blending : 1;						// Indicates DX10 part that does DX10 blending (but may not have performant vertex textures, such as Intel parts)
	bool m_bSupportsTessellation : 1;
	bool m_bPreferZPrepass : 1;
	bool m_bSuppressPixelShaderCentroidHackFixup : 1;
	bool m_bPreferTexturesInHWMemory : 1;
	bool m_bPreferHardwareSync : 1;
	bool m_bUnsupported : 1;
	bool m_bSupportsStaticControlFlow : 1;			// Useful on OpenGL, where we have a mix of support...
	bool m_FakeSRGBWrite : 1;						// Gotta do this on OpenGL.  Mostly hidden, but some high level code needs to know
	bool m_CanDoSRGBReadFromRTs : 1;				// Gotta do this on OpenGL.  Mostly hidden, but some high level code needs to know
	bool m_bSupportsGLMixedSizeTargets : 1;			// on OpenGL, are mixed size depth buffers supported - aka ARB_framebuffer_object
	bool m_bSupportsSRGBDecode : 1;					// Supports GL_EXT_texture_sRGB_decode, which allows us to toggle sRGB read D3D-style
	bool m_bSupportsCascadedShadowMapping : 1;
	bool m_bSupportsRESZ : 1;
	bool m_bSupportsINTZ : 1;
	
	HDRType_t m_MaxHDRType;
	
	uint8 m_nCSMQuality;
};


//-----------------------------------------------------------------------------
// Contains the hardware configuration for the current device
//-----------------------------------------------------------------------------
class CHardwareConfig : public IHardwareConfigInternal
{
public:
	CHardwareConfig();

	// Sets up the hardware caps given the specified DX level
	void SetupHardwareCaps( const ShaderDeviceInfo_t& mode, const HardwareCaps_t &actualCaps );

	// FIXME: This is for backward compat only.. don't use these
	void SetupHardwareCaps( int nDXLevel, const HardwareCaps_t &actualCaps );
	HardwareCaps_t& ActualCapsForEdit() { return m_ActualCaps; }
	HardwareCaps_t& CapsForEdit() { return m_Caps; }

	// Members of IMaterialSystemHardwareConfig
	virtual bool HasStencilBuffer() const;
	virtual int	 GetFrameBufferColorDepth() const;
	virtual int  GetSamplerCount() const;
	virtual bool HasSetDeviceGammaRamp() const;
	virtual VertexCompressionType_t SupportsCompressedVertices() const;
	virtual bool SupportsBorderColor() const;
	virtual bool SupportsFetch4() const;
	virtual bool SupportsStaticControlFlow() const;
	virtual int  MaximumAnisotropicLevel() const;
	virtual int  MaxTextureWidth() const;
	virtual int  MaxTextureHeight() const;
	virtual int	 TextureMemorySize() const;
	virtual bool SupportsMipmappedCubemaps() const;
	virtual int	 NumVertexShaderConstants() const;
	virtual int	 NumBooleanVertexShaderConstants() const;
	virtual int	 NumIntegerVertexShaderConstants() const;
	virtual int	 NumPixelShaderConstants() const;
	virtual int	 NumBooleanPixelShaderConstants() const;
	virtual int	 NumIntegerPixelShaderConstants() const;
	virtual int	 MaxNumLights() const;
	virtual int  MaxTextureAspectRatio() const;
	virtual int	 MaxVertexShaderBlendMatrices() const;
	virtual int	 MaxUserClipPlanes() const;
	virtual bool UseFastClipping() const;
	virtual int  GetDXSupportLevel() const;
	virtual const char *GetShaderDLLName() const;
	virtual bool ReadPixelsFromFrontBuffer() const;
	virtual bool PreferDynamicTextures() const;
	virtual bool SupportsHDR() const;
	virtual bool NeedsAAClamp() const;
	virtual bool NeedsATICentroidHack() const;
	virtual int	 GetMaxDXSupportLevel() const;
	virtual bool SpecifiesFogColorInLinearSpace() const;
	virtual bool SupportsSRGB() const;
	virtual bool FakeSRGBWrite() const;
	virtual bool CanDoSRGBReadFromRTs() const;
	virtual bool SupportsGLMixedSizeTargets() const;
	virtual bool IsAAEnabled() const;
	virtual int  GetVertexSamplerCount() const;
	virtual int  GetMaxVertexTextureDimension() const;
	virtual int  MaxTextureDepth() const;
	virtual HDRType_t GetHDRType() const;
	virtual HDRType_t GetHardwareHDRType() const;
	virtual bool SupportsStreamOffset() const;
	virtual int  StencilBufferBits() const;
	virtual int  MaxViewports() const;
	virtual void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport );
	
	// PS30 only supports shadow filter modes 0-2 (there's no use supporting mode 3, which is the ATI_NOPCF pass only used on low end), while PS20B supports all modes
	virtual ShadowFilterMode_t GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const;

	virtual int NeedsShaderSRGBConversion() const;
	virtual bool UsesSRGBCorrectBlending() const;
	virtual bool HasFastVertexTextures() const;
	virtual int MaxHWMorphBatchCount() const;
	virtual float GetShadowDepthBias() const;
	virtual float GetShadowSlopeScaleDepthBias() const;
	virtual bool PreferZPrepass() const;
	virtual bool SuppressPixelShaderCentroidHackFixup() const;
	virtual bool PreferTexturesInHWMemory() const;
	virtual bool PreferHardwareSync() const;
	virtual bool ActualHasFastVertexTextures() const;
	virtual bool SupportsShadowDepthTextures( void ) const;
	virtual ImageFormat GetShadowDepthTextureFormat( void ) const;
	virtual ImageFormat GetHighPrecisionShadowDepthTextureFormat( void ) const;
	virtual ImageFormat GetNullTextureFormat( void ) const;
	virtual int	GetMinDXSupportLevel() const;
	virtual bool IsUnsupported() const;

	const char *GetHWSpecificShaderDLLName() const;
	int GetActualSamplerCount() const;
	int GetActualVertexSamplerCount() const;

	virtual bool SupportsHDRMode( HDRType_t nHDRMode ) const;

	const HardwareCaps_t& ActualCaps() const { return m_ActualCaps; }
	const HardwareCaps_t& Caps() const { return m_Caps; }
	virtual bool GetHDREnabled( void ) const;
	virtual void SetHDREnabled( bool bEnable );

	float GetLightMapScaleFactor( void ) const;
	
	bool SupportsCascadedShadowMapping( void ) const;
	CSMQualityMode_t GetCSMQuality() const;
	bool SupportsBilinearPCFSampling() const;
	CSMShaderMode_t GetCSMShaderMode( CSMQualityMode_t nQualityLevel ) const;
	bool GetCSMAccurateBlending() const;
	void SetCSMAccurateBlending( bool bEnable );

	bool SupportsResolveDepth() const;
	bool HasFullResolutionDepthTexture() const;

protected:
	// Gets the recommended configuration associated with a particular dx level
	void ForceCapsToDXLevel( HardwareCaps_t *pCaps, int nDxLevel, const HardwareCaps_t &actualCaps );

	// Members related to capabilities
	HardwareCaps_t m_ActualCaps;
	HardwareCaps_t m_Caps;
	HardwareCaps_t m_UnOverriddenCaps;
	bool m_bHDREnabled;
	bool m_bCSMAccurateBlending;
};


//-----------------------------------------------------------------------------
// Singleton hardware config
//-----------------------------------------------------------------------------
extern CHardwareConfig *g_pHardwareConfig;


#endif // HARDWARECONFIG_H

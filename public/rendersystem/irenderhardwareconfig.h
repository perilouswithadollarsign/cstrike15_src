//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#ifndef IRENDERHARDWARECONFIG_H
#define IRENDERHARDWARECONFIG_H

#ifdef _WIN32
#pragma once
#endif


#include "tier1/interface.h"
#include "bitmap/imageformat.h"


// use DEFCONFIGMETHOD to define time-critical methods that we want to make just return constants
// on the 360, so that the checks will happen at compile time. Not all methods are defined this way
// - just the ones that I perceive as being called often in the frame interval.
#ifdef _X360
#define DEFCONFIGMETHOD( ret_type, method, xbox_return_value )		\
	FORCEINLINE ret_type method const 									\
{																	\
	return xbox_return_value;										\
	}


#else
#define DEFCONFIGMETHOD( ret_type, method, xbox_return_value )	\
	virtual ret_type method const = 0;
#endif



//-----------------------------------------------------------------------------
// Render system configuration
//-----------------------------------------------------------------------------
class IRenderHardwareConfig
{
public:
	virtual int	 GetFrameBufferColorDepth() const = 0;
	virtual int  GetSamplerCount() const = 0;
	virtual bool HasSetDeviceGammaRamp() const = 0;
	DEFCONFIGMETHOD( bool, SupportsNormalMapCompression(), true );
	virtual int  MaximumAnisotropicLevel() const = 0;	// 0 means no anisotropic filtering
	virtual int  MaxTextureWidth() const = 0;
	virtual int  MaxTextureHeight() const = 0;
	virtual int	 TextureMemorySize() const = 0;
	virtual bool SupportsMipmappedCubemaps() const = 0;

	virtual int	 NumVertexShaderConstants() const = 0;
	virtual int	 NumPixelShaderConstants() const = 0;
	virtual int	 MaxNumLights() const = 0;
	virtual int	 MaxTextureAspectRatio() const = 0;
	virtual int	 MaxVertexShaderBlendMatrices() const = 0;
	virtual int	 MaxUserClipPlanes() const = 0;
	virtual bool UseFastClipping() const = 0;

	// This here should be the major item looked at when checking for compat
	// from anywhere other than the material system	shaders
	DEFCONFIGMETHOD( int, GetDXSupportLevel(), 98 );
	virtual const char *GetShaderDLLName() const = 0;

	virtual bool ReadPixelsFromFrontBuffer() const = 0;

	// Are dx dynamic textures preferred?
	virtual bool PreferDynamicTextures() const = 0;

	virtual bool NeedsAAClamp() const = 0;
	virtual bool NeedsATICentroidHack() const = 0;

	// This is the max dx support level supported by the card
	virtual int	 GetMaxDXSupportLevel() const = 0;

	// Does the card specify fog color in linear space when sRGBWrites are enabled?
	virtual bool SpecifiesFogColorInLinearSpace() const = 0;

	// Does the card support sRGB reads/writes?
	DEFCONFIGMETHOD( bool, SupportsSRGB(), true );

	virtual bool IsAAEnabled() const = 0;	// Is antialiasing being used?

	// NOTE: Anything after this was added after shipping HL2.
	virtual int GetVertexTextureCount() const = 0;
	virtual int GetMaxVertexTextureDimension() const = 0;

	virtual int  MaxTextureDepth() const = 0;

	virtual bool SupportsStreamOffset() const = 0;

	virtual int StencilBufferBits() const = 0;
	virtual int MaxViewports() const = 0;

	virtual void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport ) = 0;

	virtual ShadowFilterMode_t GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const = 0;

	virtual int NeedsShaderSRGBConversion() const = 0;

	DEFCONFIGMETHOD( bool, UsesSRGBCorrectBlending(), true );

	virtual bool HasFastVertexTextures() const = 0;
	virtual int MaxHWMorphBatchCount() const = 0;

	virtual bool SupportsBorderColor( void ) const = 0;
	virtual bool SupportsFetch4( void ) const = 0;

	virtual float GetShadowDepthBias() const = 0;
	virtual float GetShadowSlopeScaleDepthBias() const = 0;

	virtual bool PreferZPrepass() const = 0;

	virtual bool SuppressPixelShaderCentroidHackFixup() const = 0;
	virtual bool PreferTexturesInHWMemory() const = 0;
	virtual bool PreferHardwareSync() const = 0;
	virtual bool ActualHasFastVertexTextures() const = 0;

	virtual bool SupportsShadowDepthTextures( void ) const = 0;
	virtual ImageFormat GetShadowDepthTextureFormat( void ) const = 0;
	virtual ImageFormat GetNullTextureFormat( void ) const = 0;
	virtual int	GetMinDXSupportLevel() const = 0;
	virtual bool IsUnsupported() const = 0;

	// Necessary on the 360 to improve performance of hierarchical Z
	virtual bool ReverseDepth() const = 0;
};

#endif // IRENDERHARDWARECONFIG_H

//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef MATERIALSYSTEM_CONFIG_H
#define MATERIALSYSTEM_CONFIG_H
#ifdef _WIN32
#pragma once
#endif

#if (!defined(_CERT)) && defined (_X360)
#define X360_ALLOW_TIMESTAMPS 1				// Comment in to enable showfps 12 etc...
#endif

#include "materialsystem/imaterialsystem.h"

#define MATERIALSYSTEM_CONFIG_VERSION "VMaterialSystemConfig004"

enum MaterialSystem_Config_Flags_t
{
	MATSYS_VIDCFG_FLAGS_WINDOWED					= ( 1 << 0 ),
	MATSYS_VIDCFG_FLAGS_RESIZING					= ( 1 << 1 ),
	MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC			= ( 1 << 3 ),
	MATSYS_VIDCFG_FLAGS_STENCIL						= ( 1 << 4 ),
	MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR			= ( 1 << 7 ),
	MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP				= ( 1 << 8 ),
	MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING		= ( 1 << 9 ),
	MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL				= ( 1 << 10 ),
	MATSYS_VIDCFG_FLAGS_ENABLE_HDR					= ( 1 << 12 ),
	MATSYS_VIDCFG_FLAGS_LIMIT_WINDOWED_SIZE			= ( 1 << 13 ),
	MATSYS_VIDCFG_FLAGS_SCALE_TO_OUTPUT_RESOLUTION  = ( 1 << 14 ),
	MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS      = ( 1 << 15 ),
	MATSYS_VIDCFG_FLAGS_DISABLE_PHONG				= ( 1 << 16 ),
	MATSYS_VIDCFG_FLAGS_NO_WINDOW_BORDER			= ( 1 << 17 ),
	MATSYS_VIDCFG_FLAGS_DISABLE_DETAIL				= ( 1 << 18 ),
	MATSYS_VIDCFG_FLAGS_UNSUPPORTED					= ( 1 << 19 ),
};

struct MaterialSystemHardwareIdentifier_t
{
	char *m_pCardName;
	unsigned int m_nVendorID;
	unsigned int m_nDeviceID;
};

struct MaterialSystem_Config_t
{
	bool Windowed() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_WINDOWED ) != 0; }
	bool NoWindowBorder() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_NO_WINDOW_BORDER ) != 0; }
	bool Resizing() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_RESIZING ) != 0; }
	bool WaitForVSync() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC ) == 0; }
	bool Stencil() const { return (m_Flags & MATSYS_VIDCFG_FLAGS_STENCIL ) != 0; }
	bool UseSpecular() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR ) == 0; }
	bool UseBumpmapping() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP ) == 0; }
	bool UseDetailTexturing() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_DETAIL ) == 0; }
	bool UseParallaxMapping() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING ) != 0; }
	bool UseZPrefill() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL ) != 0; }
	bool HDREnabled() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_ENABLE_HDR ) != 0; }
	bool LimitWindowedSize() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_LIMIT_WINDOWED_SIZE ) != 0; }
	bool ScaleToOutputResolution() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_SCALE_TO_OUTPUT_RESOLUTION ) != 0; }
	bool UsingMultipleWindows() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS ) != 0; }
	bool ShadowDepthTexture() const { return m_bShadowDepthTexture; }
	bool MotionBlur() const { return m_bMotionBlur; }
	bool SupportFlashlight() const { return m_bSupportFlashlight; }
	bool UsePhong() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_PHONG ) == 0; }
	bool IsUnsupported() const { return ( m_Flags & MATSYS_VIDCFG_FLAGS_UNSUPPORTED ) != 0; }
	CSMQualityMode_t GetCSMQualityMode() const { return m_nCSMQuality; }

	void SetFlag( unsigned int flag, bool val )
	{
		if( val )
		{
			m_Flags |= flag;	
		}
		else
		{
			m_Flags &= ~flag;	
		}
	}
	
	// control panel stuff
	MaterialVideoMode_t m_VideoMode;
	float m_fMonitorGamma;
	float m_fGammaTVRangeMin;
	float m_fGammaTVRangeMax;
	float m_fGammaTVExponent;
	bool m_bGammaTVEnabled;

	bool m_bWantTripleBuffered; // We only get triple buffering if fullscreen and vsync'd
	int m_nAASamples;
	int m_nForceAnisotropicLevel;
	int skipMipLevels;
	int dxSupportLevel;
	unsigned int m_Flags;
	bool bEditMode;				// true if in Hammer.
	unsigned char proxiesTestMode;	// 0 = normal, 1 = no proxies, 2 = alpha test all, 3 = color mod all
	bool bCompressedTextures;
	bool bFilterLightmaps;
	bool bFilterTextures;
	bool bReverseDepth;
	bool bBufferPrimitives;
	bool bDrawFlat;
	bool bMeasureFillRate;
	bool bVisualizeFillRate;
	bool bNoTransparency;
	bool bSoftwareLighting;
	bool bAllowCheats;
	char nShowMipLevels;
	bool bShowLowResImage;
	bool bShowNormalMap; 
	bool bMipMapTextures;
	unsigned char nFullbright;
	bool m_bFastNoBump;
	bool m_bSuppressRendering;
	bool bDrawGray;

	// debug modes
	bool bShowSpecular; // This is the fast version that doesn't require reloading materials
	bool bShowDiffuse;  // This is the fast version that doesn't require reloading materials

	uint m_WindowedSizeLimitWidth;
	uint m_WindowedSizeLimitHeight;
	int m_nAAQuality;
	bool m_bShadowDepthTexture;
	bool m_bMotionBlur;
	bool m_bSupportFlashlight;

	// PAINT
	// True if the current mod supports paint at all (should be always true or always false in a game, and is practically only true in Portal2)
	bool m_bPaintInGame;
	// True if the current level supports paint (should be only true if m_bPaintInGame is true AND the currently loaded level supports paint)
	bool m_bPaintInMap;
	
	CSMQualityMode_t m_nCSMQuality;
	bool m_bCSMAccurateBlending;
				
	MaterialSystem_Config_t()
	{
		memset( this, 0, sizeof( *this ) );

		// video config defaults
		SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_NO_WINDOW_BORDER, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, true );
		SetFlag( MATSYS_VIDCFG_FLAGS_STENCIL, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_DETAIL, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING, true );
		SetFlag( MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_LIMIT_WINDOWED_SIZE, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_SCALE_TO_OUTPUT_RESOLUTION, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS, false );
		SetFlag( MATSYS_VIDCFG_FLAGS_UNSUPPORTED, false );

		m_VideoMode.m_Width = 640;
		m_VideoMode.m_Height = 480;
		m_VideoMode.m_RefreshRate = 60;
		dxSupportLevel = 0;
		bCompressedTextures = true;
		bFilterTextures = true;
		bFilterLightmaps = true;
		bMipMapTextures = true;
		bBufferPrimitives = true;

		m_fMonitorGamma = 2.2f;
		m_fGammaTVRangeMin = 16.0f;
		m_fGammaTVRangeMax = 255.0f;
		m_fGammaTVExponent = 2.5;
		m_bGammaTVEnabled = IsGameConsole();

		m_bWantTripleBuffered = false;
		m_nAASamples = 1;
		m_bShadowDepthTexture = false;
		m_bMotionBlur = false;
		m_bSupportFlashlight = true;

		// misc defaults
		bAllowCheats = false;
		bCompressedTextures = true;
		bEditMode = false;

		// debug modes
		bShowSpecular = true;
		bShowDiffuse = true;
		nFullbright = 0;
		bShowNormalMap = false;
		bFilterLightmaps = true;
		bFilterTextures = true;
		bMipMapTextures = true;
		nShowMipLevels = 0;
		bShowLowResImage = false;
		bReverseDepth = false;
		bBufferPrimitives = true;
		bDrawFlat = false;
		bMeasureFillRate = false;
		bVisualizeFillRate = false;
		bSoftwareLighting = false;
		bNoTransparency = false;
		proxiesTestMode = 0;
		m_bFastNoBump = false;
		m_bSuppressRendering = false;
		m_WindowedSizeLimitWidth = 1280;
		m_WindowedSizeLimitHeight = 1024;
		bDrawGray = false;

		// PAINT
		m_bPaintInGame = false;
		m_bPaintInMap = false;

		m_nCSMQuality = CSMQUALITY_VERY_LOW;
		m_bCSMAccurateBlending = true;
	}
};


#endif // MATERIALSYSTEM_CONFIG_H

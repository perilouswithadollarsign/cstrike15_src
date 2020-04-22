//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// A class representing a projected "light"
//
//=============================================================================

#ifndef DMEPROJECTEDLIGHT_H
#define DMEPROJECTEDLIGHT_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmelight.h"

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
struct LightDesc_t;
struct FlashlightState_t;
class CTextureReference;


//-----------------------------------------------------------------------------
// A spot light - includes shadow mapping parameters
//-----------------------------------------------------------------------------
class CDmeProjectedLight : public CDmePointLight
{
	DEFINE_ELEMENT( CDmeProjectedLight, CDmePointLight );

public:
	// Sets the spotlight direction
	void SetFOV( float flHorizontalFOV, float flVerticalFOV );

	float GetFOVx() const { return m_flHorizontalFOV; }
	float GetFOVy() const { return m_flVerticalFOV; }
	const char *GetFlashlightTexture() const { return m_Texture.Get(); }

	void GetFlashlightState( FlashlightState_t &flashlightState, CTextureReference &texture ) const;

private:

	CDmaVar< float >	m_flMinDistance;
	CDmaVar< float >	m_flHorizontalFOV;
	CDmaVar< float >	m_flVerticalFOV;
	CDmaVar< float >	m_flAmbientIntensity;
	CDmaString			m_Texture;
	CDmaVar< bool  >	m_bCastsShadows;
	CDmaVar< float >	m_flRadius;
	CDmaVar< float >	m_flShadowDepthBias;
	CDmaVar< float >	m_flShadowSlopeScaleDepthBias;
	CDmaVar< float >	m_flShadowFilterSize;
	CDmaVar< float >	m_flShadowJitterSeed;
	CDmaVar< Vector2D >	m_vPositionJitter;
	CDmaTime			m_AnimationTime;
	CDmaVar< float >	m_flFrameRate;

	CDmaVar< float >	m_flShadowAtten;
	CDmaVar< bool  >	m_bDrawShadowFrustum;
	CDmaVar< float >	m_flFarZAtten;
	CDmaVar< float >	m_flAmbientOcclusion;

	// Uberlight parameters
	CDmaVar< bool  >	m_bUberlight;
	CDmaVar< float >	m_flNearEdge;
	CDmaVar< float >	m_flFarEdge;
	CDmaVar< float >	m_flCutOn;
	CDmaVar< float >	m_flCutOff;
	CDmaVar< float >	m_flWidth;
	CDmaVar< float >	m_flWedge;
	CDmaVar< float >	m_flHeight;
	CDmaVar< float >	m_flHedge;
	CDmaVar< float >	m_flRoundness;

	CDmaVar< bool  >	m_bVolumetric;
	CDmaVar< float >	m_flNoiseStrength;
	CDmaVar< float >	m_flFlashlightTime;
	CDmaVar< int   >	m_nNumPlanes;
	CDmaVar< float >	m_flPlaneOffset;
	CDmaVar< float >	m_flVolumetricIntensity;
};


#endif // DMEPROJECTEDLIGHT_H

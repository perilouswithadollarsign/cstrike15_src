//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a light
//
//=============================================================================

#ifndef DMELIGHT_H
#define DMELIGHT_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmedag.h"


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
struct LightDesc_t;


//-----------------------------------------------------------------------------
// A base class for lights
//-----------------------------------------------------------------------------
class CDmeLight : public CDmeDag
{
	DEFINE_ELEMENT( CDmeLight, CDmeDag );

public:
	// Sets the color and intensity
	// NOTE: Color is specified 0-255 floating point.
	void SetColor( const Color &color );
	void SetIntensity( float flIntensity );

	// Sets up render state in the material system for rendering
	virtual bool GetLightDesc( LightDesc_t *pDesc ) { return false; }

protected:
	// Sets up render state in the material system for rendering
	void SetupRenderStateInternal( LightDesc_t &desc, float flAtten0, float flAtten1, float flAtten2 );

	CDmaVar< Color > m_Color;
	CDmaVar< float > m_flIntensity;
};


//-----------------------------------------------------------------------------
// A directional light
//-----------------------------------------------------------------------------
class CDmeDirectionalLight : public CDmeLight
{
	DEFINE_ELEMENT( CDmeDirectionalLight, CDmeLight );

public:
	// Sets up render state in the material system for rendering
	virtual bool GetLightDesc( LightDesc_t *pDesc );
};


//-----------------------------------------------------------------------------
// A point light
//-----------------------------------------------------------------------------
class CDmePointLight : public CDmeLight
{
	DEFINE_ELEMENT( CDmePointLight, CDmeLight );

public:
	// Sets the attenuation factors
	void SetAttenuation( float flConstant, float flLinear, float flQuadratic );

	// Sets the maximum range
	void SetMaxDistance( float flMaxDistance );

	// Sets up render state in the material system for rendering
	virtual bool GetLightDesc( LightDesc_t *pDesc );

protected:
	CDmaVar< float >	m_flAttenuation0;
	CDmaVar< float >	m_flAttenuation1;
	CDmaVar< float >	m_flAttenuation2;
	CDmaVar< float >	m_flMaxDistance;
};


//-----------------------------------------------------------------------------
// A spot light
//-----------------------------------------------------------------------------
class CDmeSpotLight : public CDmePointLight
{
	DEFINE_ELEMENT( CDmeSpotLight, CDmePointLight );

public:
	// Sets the spotlight angle factors
	// Angles are specified in degrees, as full angles (as opposed to half-angles)
	void SetAngles( float flInnerAngle, float flOuterAngle, float flAngularFalloff );

	// Sets up render state in the material system for rendering
	virtual bool GetLightDesc( LightDesc_t *pDesc );

private:
	CDmaVar<float>	m_flSpotInnerAngle;
	CDmaVar<float>	m_flSpotOuterAngle;
	CDmaVar<float>	m_flSpotAngularFalloff;
};


//-----------------------------------------------------------------------------
// An ambient light
//-----------------------------------------------------------------------------
class CDmeAmbientLight : public CDmeLight
{
	DEFINE_ELEMENT( CDmeAmbientLight, CDmeLight );
};


#endif // DMELIGHT_H

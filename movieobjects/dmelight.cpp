//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmelight.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "movieobjects/dmetransform.h"
#include "materialsystem/imaterialsystem.h"
#include "movieobjects_interfaces.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeLight, CDmeLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeLight::OnConstruction()
{
	m_Color.InitAndSet( this, "color", Color( 255, 255, 255, 255 ) );
	m_flIntensity.InitAndSet( this, "intensity", 1.0f );
}

void CDmeLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the color and intensity
// NOTE: Color is specified 0-255 floating point.
//-----------------------------------------------------------------------------
void CDmeLight::SetColor( const Color &color )
{
	m_Color.Set( color );
}

void CDmeLight::SetIntensity( float flIntensity )
{
	m_flIntensity = flIntensity;
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
void CDmeLight::SetupRenderStateInternal( LightDesc_t &desc, float flAtten0, float flAtten1, float flAtten2 )
{
	desc.m_Color[0] = m_Color.Get().r();
	desc.m_Color[1] = m_Color.Get().g();
	desc.m_Color[2] = m_Color.Get().b();
	desc.m_Color *= m_flIntensity / 255.0f;

	desc.m_Attenuation0 = flAtten0;
	desc.m_Attenuation1 = flAtten1;
	desc.m_Attenuation2 = flAtten2;

	desc.m_Flags = 0;
	if ( desc.m_Attenuation0 != 0.0f )
	{
		desc.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
	}
	if ( desc.m_Attenuation1 != 0.0f )
	{
		desc.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
	}
	if ( desc.m_Attenuation2 != 0.0f )
	{
		desc.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
	}
}


//-----------------------------------------------------------------------------
//
// A directional light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDirectionalLight, CDmeDirectionalLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeDirectionalLight::OnConstruction()
{
}

void CDmeDirectionalLight::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// Gets a light desc for the light
//-----------------------------------------------------------------------------
bool CDmeDirectionalLight::GetLightDesc( LightDesc_t *pDesc )
{
	memset( pDesc, 0, sizeof(LightDesc_t) );

	pDesc->m_Type = MATERIAL_LIGHT_DIRECTIONAL;
	SetupRenderStateInternal( *pDesc, 1.0f, 0.0f, 0.0f );

	matrix3x4_t m;
	GetAbsTransform( m );
	MatrixGetColumn( m, 0, pDesc->m_Direction ); // from mathlib_base.cpp: MatrixVectors(): Matrix is right-handed x=forward, y=left, z=up. We a left-handed convention for vectors in the game code (forward, right, up)

	pDesc->m_Theta = 0.0f;
	pDesc->m_Phi = 0.0f;
	pDesc->m_Falloff = 1.0f;
	pDesc->RecalculateDerivedValues();

	return true;
}


//-----------------------------------------------------------------------------
//
// A point light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePointLight, CDmePointLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmePointLight::OnConstruction()
{
	m_flAttenuation0.InitAndSet( this, "constantAttenuation", 1.0f );
	m_flAttenuation1.InitAndSet( this, "linearAttenuation", 0.0f );
	m_flAttenuation2.InitAndSet( this, "quadraticAttenuation", 0.0f );
	m_flMaxDistance.InitAndSet( this, "maxDistance", 600.0f ); // 50 feet
}

void CDmePointLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the attenuation factors
//-----------------------------------------------------------------------------
void CDmePointLight::SetAttenuation( float flConstant, float flLinear, float flQuadratic )
{
	m_flAttenuation0 = flConstant;
	m_flAttenuation1 = flLinear;
	m_flAttenuation2 = flQuadratic;
}

	
//-----------------------------------------------------------------------------
// Sets the maximum range
//-----------------------------------------------------------------------------
void CDmePointLight::SetMaxDistance( float flMaxDistance )
{
	m_flMaxDistance = flMaxDistance;
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
bool CDmePointLight::GetLightDesc( LightDesc_t *pDesc )
{
	memset( pDesc, 0, sizeof(LightDesc_t) );

	pDesc->m_Type = MATERIAL_LIGHT_POINT;
	SetupRenderStateInternal( *pDesc, m_flAttenuation0, m_flAttenuation1, m_flAttenuation2 );

	matrix3x4_t m;
	GetAbsTransform( m );
	MatrixPosition( m, pDesc->m_Position );
	pDesc->m_Direction.Init( 0, 0, 1 );
	pDesc->m_Range = m_flMaxDistance;

	pDesc->m_Theta = 0.0f;
	pDesc->m_Phi = 0.0f;
	pDesc->m_Falloff = 1.0f;
	pDesc->RecalculateDerivedValues();

	return true;
}


//-----------------------------------------------------------------------------
//
// A spot light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSpotLight, CDmeSpotLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSpotLight::OnConstruction()
{
	m_flSpotInnerAngle.InitAndSet( this, "spotInnerAngle", 60.0f );
	m_flSpotOuterAngle.InitAndSet( this, "spotOuterAngle", 90.0f );
	m_flSpotAngularFalloff.InitAndSet( this, "spotAngularFalloff", 1.0f );
}

void CDmeSpotLight::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// Sets the spotlight angle factors
// Angles are specified in degrees, as full angles (as opposed to half-angles)
//-----------------------------------------------------------------------------
void CDmeSpotLight::SetAngles( float flInnerAngle, float flOuterAngle, float flAngularFalloff )
{
	m_flSpotInnerAngle = flInnerAngle;
	m_flSpotOuterAngle = flOuterAngle;
	m_flSpotAngularFalloff = flAngularFalloff;
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
bool CDmeSpotLight::GetLightDesc( LightDesc_t *pDesc )
{
	memset( pDesc, 0, sizeof(LightDesc_t) );

	pDesc->m_Type = MATERIAL_LIGHT_SPOT;
	SetupRenderStateInternal( *pDesc, m_flAttenuation0, m_flAttenuation1, m_flAttenuation2 );

	matrix3x4_t m;
	GetAbsTransform( m );
	MatrixPosition( m, pDesc->m_Position );
	MatrixGetColumn( m, 0, pDesc->m_Direction ); // from mathlib_base.cpp: MatrixVectors(): Matrix is right-handed x=forward, y=left, z=up. We a left-handed convention for vectors in the game code (forward, right, up)
	pDesc->m_Range = m_flMaxDistance;

	// Convert to radians
	pDesc->m_Theta = 0.5f * m_flSpotInnerAngle * M_PI / 180.0f;
	pDesc->m_Phi = 0.5f * m_flSpotOuterAngle * M_PI / 180.0f;
	pDesc->m_Falloff = m_flSpotAngularFalloff;
	pDesc->RecalculateDerivedValues();

	return true;
}


//-----------------------------------------------------------------------------
//
// An ambient light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAmbientLight, CDmeAmbientLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeAmbientLight::OnConstruction()
{
}

void CDmeAmbientLight::OnDestruction()
{
}
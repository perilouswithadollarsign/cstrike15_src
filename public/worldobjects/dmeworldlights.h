//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a world node
//
//=============================================================================

#ifndef DMEWORLDLIGHTS_H
#define DMEWORLDLIGHTS_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "datamodel/dmelement.h"

//-----------------------------------------------------------------------------
// Light types
//-----------------------------------------------------------------------------
class CDmeWorldPointLight : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorldPointLight, CDmElement );

public:
	CDmaString m_lightName;
	CDmaVar< Vector > m_vOrigin;
	CDmaVar< Vector > m_vColor;
	CDmaVar< float > m_flRadius;
	CDmaVar< Vector > m_vAttenuation;
};

class CDmeWorldHemiLight : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorldHemiLight, CDmElement );

public:
	CDmaString m_lightName;
	CDmaVar< Vector4D > m_vTransform0;		// Direction is z column
	CDmaVar< Vector4D > m_vTransform1;		// Direction is z column
	CDmaVar< Vector4D > m_vTransform2;		// Direction is z column
	CDmaVar< Vector > m_vColor;
	CDmaVar< float > m_flRadius;
	CDmaVar< Vector > m_vAttenuation;
};

class CDmeWorldSpotLight : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorldSpotLight, CDmElement );

public:
	CDmaString m_lightName;
	CDmaVar< Vector4D > m_vTransform0;		// Direction is z column
	CDmaVar< Vector4D > m_vTransform1;		// Direction is z column
	CDmaVar< Vector4D > m_vTransform2;		// Direction is z column
	CDmaVar< Vector > m_vColor;
	CDmaVar< float > m_flRadius;
	CDmaVar< Vector > m_vAttenuation;
	CDmaVar< float > m_flCosSpot;
};

#endif // DMEWORLDLIGHTS_H

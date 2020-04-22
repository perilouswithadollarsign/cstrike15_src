//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a world node
//
//=============================================================================

#ifndef DMEWORLDNODE_H
#define DMEWORLDNODE_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "dmeworldlights.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// A scene object in a world node
//-----------------------------------------------------------------------------
class CDmeSceneObject : public CDmElement
{
	DEFINE_ELEMENT( CDmeSceneObject, CDmElement );

public:
	CDmaVar< Vector4D >						m_vTransform0;
	CDmaVar< Vector4D >						m_vTransform1;
	CDmaVar< Vector4D >						m_vTransform2;
	CDmaString								m_renderableFileName;
};

//-----------------------------------------------------------------------------
// A world node
//-----------------------------------------------------------------------------
class CDmeWorldNode : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorldNode, CDmElement );

public:
	CDmaVar< int32 >						m_nID;
	CDmaVar< int32 >						m_Flags;
	CDmaVar< int32 >						m_nParent;
	CDmaVar< Vector >						m_vOrigin;
	CDmaVar< Vector >						m_vMinBounds;
	CDmaVar< Vector >						m_vMaxBounds;
	CDmaVar< float >						m_flMinimumDistance;

	CDmaArray< int32 >						m_ChildNodeIndices;
	CDmaElementArray< CDmeSceneObject >		m_SceneObjects;
	CDmaElementArray< CDmeWorldPointLight >	m_PointLights;
	CDmaElementArray< CDmeWorldHemiLight >	m_HemiLights;
	CDmaElementArray< CDmeWorldSpotLight >	m_SpotLights;
};

#endif // DMEWORLDNODE_H

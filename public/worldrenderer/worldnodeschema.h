//============ Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===============================================================================//
#ifndef WORLDNODE_SCHEMA_H
#define WORLDNODE_SCHEMA_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include <tier0/platform.h>
#include <mathlib/vector.h>
#include <mathlib/vector4d.h>
#include <mathlib/mathlib.h>
#include <mathlib/vmatrix.h>
#include "rendersystem/schema/renderable.g.h"

//--------------------------------------------------------------------------------------
// Light data
//--------------------------------------------------------------------------------------
schema struct WorldPointLightData_t
{
	Vector m_vOrigin;
	Vector4D m_vColorNRadius;
	Vector m_vAttenuation;
};

schema struct WorldHemiLightData_t
{
	Vector4D m_vTransform0;		// Direction is z column
	Vector4D m_vTransform1;		// Direction is z column
	Vector4D m_vTransform2;		// Direction is z column
	Vector4D m_vColorNRadius;
	Vector m_vAttenuation;
};

schema struct WorldSpotLightData_t
{
	Vector4D m_vTransform0;		// Direction is z column
	Vector4D m_vTransform1;		// Direction is z column
	Vector4D m_vTransform2;		// Direction is z column
	Vector4D m_vColorNRadius;
	Vector4D m_vAttenuationNCosSpot;
};

schema struct SceneObject_t
{
	Vector4D								m_vTransform[3];
	CResourceReference< Renderable_t >		m_Renderable;
};

schema struct WorldNode_t
{
	CResourceArray< SceneObject_t >			m_SceneObjects;				// List of scene objects in this node
	CResourceArray< WorldPointLightData_t >	m_PointLights;				// TODO: These should actually become scene objects
	CResourceArray< WorldHemiLightData_t >	m_HemiLights;				// TODO: These should actually become scene objects
	CResourceArray< WorldSpotLightData_t >	m_SpotLights;				// TODO: These should actually become scene objects
};

class CWorldNode;	// Forward declaration of associated runtime class
DEFINE_RESOURCE_CLASS_TYPE( WorldNode_t, CWorldNode, RESOURCE_TYPE_WORLD_NODE );
typedef const ResourceBinding_t< CWorldNode > *HWorldNode;
typedef CStrongHandle< CWorldNode > HWorldNodeStrong;
#define WORLD_NODE_HANDLE_INVALID ( (HWorldNode)0 )

#endif // WORLDNODE_SCHEMA_H

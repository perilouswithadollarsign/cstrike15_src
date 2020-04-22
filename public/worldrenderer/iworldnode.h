//============ Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===============================================================================//
#ifndef IWORLDNODE_H
#define IWORLDNODE_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "tier0/platform.h"
#include "mathlib/vector.h"
#include "mathlib/vector4d.h"
#include "mathlib/mathlib.h"
#include "mathlib/vmatrix.h"
#include "mathlib/camera.h"
#include "worldrenderer/worldnodeschema.h"

class CSceneObject;

abstract_class IWorldNode
{
public:
	// helpers
	//virtual int32 GetID()	= 0;
	//virtual int32 GetFlags() = 0;
	//virtual int32 GetNumChildren() = 0;
	virtual int32 GetNumSceneObjects() = 0;
	virtual bool IsFullyResident() = 0;
	virtual void SetIsFullyResident( bool bRes ) = 0;
	virtual bool IsLoading() = 0;
	virtual void SetIsLoading( bool bLoading ) = 0;
	//virtual int32 GetChild( int32 c ) = 0;
	//virtual int32 GetParent() = 0;
	virtual CSceneObject *GetSceneObject( int32 s ) = 0;

	virtual int32 GetNumPointLights() = 0;
	virtual int32 GetNumHemiLights() = 0;
	virtual int32 GetNumSpotLights() = 0;
	virtual WorldPointLightData_t *GetPointLights() = 0;
	virtual WorldHemiLightData_t *GetHemiLights() = 0;
	virtual WorldSpotLightData_t *GetSpotLights() = 0;
};

#endif // IWORLDNODE_H

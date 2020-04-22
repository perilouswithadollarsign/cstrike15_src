//============ Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===============================================================================//
#ifndef IWORLD_H
#define IWORLD_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "worldrenderer/worldschema.h"
#include "worldrenderer/iworldnode.h"

struct WorldBuilderParams_t;
class KeyValues;

//-----------------------------------------------------------------------------
enum BuildListAction_t
{
	BUILD_ACTION_NONE						= 0,
	BUILD_ACTION_RENDER_PARENT			= 1
};

abstract_class IWorld
{
public:
	// Loading
	virtual void CreateAndDispatchLoadRequests( const Vector &vEye ) = 0;
	virtual void Destroy() = 0;

	// Reflection
	virtual int GetNumNodes() = 0;
	virtual const WorldBuilderParams_t *GetBuilderParams() = 0;
	virtual IWorldNode *GetNode( int n ) = 0;
	virtual bool IsAncestor( int nNodeInQuestion, int nPotentialAncestor ) = 0;
	virtual const NodeData_t *GetNodeData( int n ) = 0;
	virtual AABB_t GetNodeBounds( int n ) = 0;
	virtual Vector GetNodeOrigin( int n ) = 0;
	virtual float GetNodeMinDistance( int n ) = 0;

	// Resource updates
	virtual void ClearOutstandingLoadRequests() = 0;
	virtual void UpdateResources() = 0;

	// Visibility
	virtual int GetLeafNodeForPoint( Vector &vPosition ) = 0;
	virtual float GetMaxVisibleDistance( Vector &vPosition ) = 0;

	// Rendering
	virtual BuildListAction_t BuildRenderList( WorldNodeFlags_t nSkipFlags, 
												const Vector &vEyePoint,
												float flLODScale,
												float flFarPlane,
												float flElapsedTime ) = 0;

	// Entities
	virtual void GetEntities( char *pEntityName, CUtlVector<KeyValues*> &entityList ) = 0;
};

#endif // IWORLD_H

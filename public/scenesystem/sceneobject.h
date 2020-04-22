//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef SCENEOBJECT_H
#define SCENEOBJECT_H

#ifdef _WIN32
#pragma once
#endif

#include "scenesystem/iscenesystem.h"



class ISceneObjectDesc;

enum ESceneObjectFlags
{
	SCENEOBJECTFLAG_IS_LOADED = 1,							// this will go away when we have permanent data - we can't afford to poll all objects.
	SCENEOBJECTFLAG_DRAW_IN_PREPASS = 2,
	SCENEOBJECTFLAG_DRAW_IN_LIGHTPASS = 4,
	SCENEOBJECTFLAG_DRAW_IN_COMBINE_PASS = 8,
	SCENEOBJECTFLAG_DRAW_IN_TRANSLUCENT_PASS = 16,
	SCENEOBJECTFLAG_DRAW_IN_REFLECTION_PASS = 32,
	SCENEOBJECTFLAG_IS_DISABLED = 64,
};

enum ESceneObjectTypeFlags
{
	SCENEOBJECTTYPEFLAG_IS_PROCEDURAL = 1,					// objects with this set draw via calling the DrawArray entrypoint in the ISceneObjectDesc, not by bundling up meshes
	SCENEOBJECTTYPEFLAG_FROM_POOL = 2,						// was allocated from our pool
};


class CSceneObjectReference_t								// this is what we store in our spatial data structures. 32 bytes
{
public:
	VectorAligned m_vecAABBMins;
	uint m_nRenderableFlags;
	VectorAligned m_vecAABBMaxes;
	class CSceneObject *m_pObject;
	
};


class CSceneObject
{
public:
	CSceneObject *m_pNext;									// temp for linking objects in list
	CSceneObjectReference_t *m_pRefData;

	uint8 m_nID;											// not unique! - for hashing, etc
	uint8 m_nNumDrawPrimitives;
	uint8 m_nObjectTypeFlags;
	uint8 m_nPad;
	HRenderableStrong m_hRenderable;
	uint m_nRenderableFlags;								// this is a mirror of the flags for objects which are not in hierarchy yet

	// temporary so that alex can check in without breaking scenesystem
	IMat2 *m_pMaterialHack;


	union
	{
		MaterialDrawDescriptor_t const *m_pDrawPrimitives;	// fixed mesh objects have this
		void *m_pObjectData;								// procedurals and non-fixed meshes get this, and it is used as a sort key
	};

	template<class T> FORCEINLINE T * ObjectData( void )
	{
		Assert( m_nNumDrawPrimitives == 0 );
		return ( reinterpret_cast< T* >( m_pObjectData ) );
	}

	matrix3x4_t m_transform;

	ISceneObjectDesc *m_pDesc;

	FORCEINLINE uint32 GetID( void ) const
	{
		return m_nID;
	}

	FORCEINLINE void MirrorFlags( void ) const
	{
		if ( m_pRefData )
		{
			m_pRefData->m_nRenderableFlags = m_nRenderableFlags;
		}

	}

	FORCEINLINE bool IsLoaded( void ) const
	{
		return ( m_nRenderableFlags & SCENEOBJECTFLAG_IS_LOADED ) != 0;
	}

	FORCEINLINE void SetLoaded( void )
	{
		m_nRenderableFlags |= SCENEOBJECTFLAG_IS_LOADED;
		MirrorFlags();
	}

	FORCEINLINE void ClearLoaded( void )
	{
		m_nRenderableFlags &= ~SCENEOBJECTFLAG_IS_LOADED;
		MirrorFlags();
	}

	FORCEINLINE void SetBounds( Vector const &vecMins, Vector const &vecMaxes )
	{
		g_pSceneSystem->SetObjectBounds( this, vecMins, vecMaxes );
	}

};


// particle vertex layout defs - ! to be moved. eventaully all of this will be hidden inside the particles.lib render ops
struct VertexRepeatedStream_t
{
	Vector2D m_vecCoord;									// -1 1   1 1   1 -1  -1 -1
};

struct VertexUVPos_t
{
	Vector m_vecPos;
	Vector2D m_vecUV;
};


struct VertexPerParticleStream_t
{
	Vector m_vecPos;
	float m_flRadius;
	VertexColor_t m_color;
};


class CSceneParticleObject : public CSceneObject
{
public:
	IMat2 *m_pMaterial;										// will go away with particle/mat2 integration
	CParticleCollection *m_pParticles;
};


class CSceneMonitorObject : public CSceneObject
{
public:
	Vector m_MonitorVerts[4];
	HRenderTextureStrong m_hMonitorTexture;
	RenderTargetBinding_t m_hRenderTargetBinding;
	IMat2 *m_pMaterial;
	CFrustum m_frustum;

};

#define DRAWLIST_CHUNKSIZE 1024

class CSceneDrawList
{
public:
	CSceneDrawList *m_pNext;
	int m_nNumPrimitives;
	CMeshDrawPrimitive_t m_drawPrimitives[DRAWLIST_CHUNKSIZE];

};


#endif


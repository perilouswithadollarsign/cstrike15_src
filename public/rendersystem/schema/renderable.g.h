
#ifndef RENDERABLE_G_H
#define RENDERABLE_G_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "resourcefile/resourcefile.h"
#include "resourcefile/resourcetype.h"

#include "renderbuffer.g.h"

//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
enum RenderPrimitiveType_t
{
	RENDER_PRIM_POINTS = 0,
	RENDER_PRIM_LINES,
	RENDER_PRIM_LINES_WITH_ADJACENCY,
	RENDER_PRIM_LINE_STRIP,
	RENDER_PRIM_LINE_STRIP_WITH_ADJACENCY,
	RENDER_PRIM_TRIANGLES,
	RENDER_PRIM_TRIANGLES_WITH_ADJACENCY,
	RENDER_PRIM_TRIANGLE_STRIP,
	RENDER_PRIM_TRIANGLE_STRIP_WITH_ADJACENCY,
	RENDER_PRIM_INSTANCED_QUADS,
	RENDER_PRIM_HETEROGENOUS,
	RENDER_PRIM_TYPE_COUNT,
};


//-----------------------------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------------------------

schema struct RenderBufferBinding_t
{
	CResourceReference< RenderBufferBits_t > m_pRenderBuffer;
	uint32           m_nBindOffsetBytes;
	uint8            m_padding[4];
};

schema struct MaterialDrawDescriptor_t
{
	int32            m_nBaseVertex;
	int32            m_nVertexCount;
	int32            m_nStartIndex;
	int32            m_nIndexCount;
	int32            m_nStartInstance;
	int32            m_nInstanceCount;
	uint8            m_nPrimitiveType;	// See RenderPrimitiveType_t
	uint8            m_padding[3];
	CResourceArray< RenderBufferBinding_t > m_Buffers;
	CResourceString m_pMaterialName;

	//! opaquePointer
	void           *m_pMaterial;
};

schema struct PermRenderableBounds_t
{
	Vector           m_MinBounds;
	Vector           m_MaxBounds;
};

//! uncacheableStruct = PermRenderableBounds_t
schema struct Renderable_t
{
	CResourceArray< MaterialDrawDescriptor_t > m_DrawCalls;
};

class CRenderable;	// Forward declaration of associated runtime class
DEFINE_RESOURCE_CLASS_TYPE( Renderable_t, CRenderable, RESOURCE_TYPE_RENDERABLE );
typedef const ResourceBinding_t< CRenderable > *HRenderable;
typedef CStrongHandle< CRenderable > HRenderableStrong;
#define RENDERABLE_HANDLE_INVALID ( (HRenderable)0 )


#endif // RENDERABLE_G_H

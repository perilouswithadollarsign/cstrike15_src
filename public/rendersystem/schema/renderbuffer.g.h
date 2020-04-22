#ifndef RENDERBUFFER_G_H
#define RENDERBUFFER_G_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "resourcefile/resourcefile.h"
#include "resourcefile/resourcetype.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct RenderInputLayoutField2_t;
struct RenderBufferDesc_t;
struct RenderBufferBits_t;


//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
schema enum RenderBufferType_t
{
	RENDER_BUFFER_TYPE_STATIC = 0, // GPU can read from it only, CPU can only write once
	RENDER_BUFFER_TYPE_SEMISTATIC , // GPU can read, writes are infrequent from CPU
	RENDER_BUFFER_TYPE_STAGING, // GPU can write, CPU can read
	RENDER_BUFFER_TYPE_GPU_ONLY, // GPU can read/write, CPU cannot read/write (used for GPU-generated data)
	RENDER_BUFFER_TYPE_COUNT,
	RENDER_BUFFER_TYPE_COUNT_PLUS_1, // Ignore the man behind the curtain (used to silence a warning in rendersystem)
};

schema enum RenderBufferClass_t
{
	RENDER_BUFFER_CLASS_VERTEX_BUFFER = 0,// (explicit)
	RENDER_BUFFER_CLASS_INDEX_BUFFER,
	RENDER_BUFFER_CLASS_RESERVED_VALUE_1, // These are for internal use only
	RENDER_BUFFER_CLASS_RESERVED_VALUE_2,
};

schema enum RenderSlotType_t
{
	RENDER_SLOT_INVALID = -1,
	RENDER_SLOT_PER_VERTEX = 0,
	RENDER_SLOT_PER_INSTANCE = 1,
};

schema enum MaxInputLayoutSemanticNameSize_t
{
	RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE2 = 32,
};


//-----------------------------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------------------------
schema struct RenderInputLayoutField2_t
{
	uint8            m_SemanticName[32];
	int32            m_nSemanticIndex; // TODO: Change to ColorFormat_t and make bitmap/colorformat.h depend on ColorFormat.sch
	int32            m_nFormat;
	int32            m_nOffset;
	int32            m_nSlot;
	uint8            m_nSlotType;	// See RenderSlotType_t
	uint8            m_Padding[3];
	int32            m_nInstanceStepRate;
};

schema struct RenderBufferDesc_t
{
	int32            m_nElementCount;
	uint16           m_nElementSizeInBytes;	// Assume no single element is over 65k in size
	uint8            m_nBufferType;	// See RenderBufferType_t
	uint8            m_nBufferClass;	// See RenderBufferClass_t
	CResourceArray< RenderInputLayoutField2_t > m_InputLayoutFields; // Unused for index buffers
};

//! uncacheableStruct = RenderBufferDesc_t
schema struct RenderBufferBits_t
{
	// Empty like texture bits... Just a bag of bits with no reflection data...
};

class CRenderBufferBits;	// Forward declaration of associated runtime class
DEFINE_RESOURCE_CLASS_TYPE( RenderBufferBits_t, CRenderBufferBits, RESOURCE_TYPE_RENDER_BUFFER );
typedef const ResourceBinding_t< CRenderBufferBits > *HRenderBuffer;
typedef CStrongHandle< CRenderBufferBits > HRenderBufferStrong;
#define RENDER_BUFFER_HANDLE_INVALID ( (HRenderBuffer)0 )


#endif // RENDERBUFFER_G_H

#ifndef TEXTURE_G_H
#define TEXTURE_G_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "resourcefile/resourcefile.h"
#include "resourcefile/resourcetype.h"
#include "mathlib/vector4d.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct TextureDesc_t;
struct TextureHeader_t;
struct TextureBits_t;


//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
schema enum TextureSpecificationFlags_t
{
	TSPEC_FLAGS						= 0x0000,// (explicit)
	TSPEC_RENDER_TARGET				= 0x0001,// (explicit)
	TSPEC_VERTEX_TEXTURE			= 0x0002,// (explicit)
	TSPEC_UNFILTERABLE_OK			= 0x0004,// (explicit)
	TSPEC_RENDER_TARGET_SAMPLEABLE	= 0x0008,// (explicit)
	TSPEC_SUGGEST_CLAMPS			= 0x0010,
	TSPEC_SUGGEST_CLAMPT			= 0x0020,
	TSPEC_SUGGEST_CLAMPU			= 0x0040,
	TSPEC_NO_LOD					= 0x0080,	// Don't downsample on lower-level cards
};

schema enum RenderMultisampleType_t
{
	RENDER_MULTISAMPLE_INVALID = -1,// (explicit)
	RENDER_MULTISAMPLE_NONE = 0,// (explicit)
	RENDER_MULTISAMPLE_2X = 1,
	RENDER_MULTISAMPLE_4X = 2,
	RENDER_MULTISAMPLE_6X = 3,
	RENDER_MULTISAMPLE_8X = 4,
	RENDER_MULTISAMPLE_16X = 5,
};


//-----------------------------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------------------------
schema struct TextureDesc_t
{
	uint16           m_nWidth;
	uint16           m_nHeight;
	uint16           m_nDepth;
	int8             m_nImageFormat;
	uint8            m_nNumMipLevels;
};

schema struct TextureHeader_t : public TextureDesc_t
{
	uint16           m_nMultisampleType;	// See RenderMultisampleType_t
	uint16           m_nFlags;	// See TextureSpecificationFlags_t
	Vector4D         m_Reflectivity;
};

//! uncacheableStruct = TextureHeader_t
schema struct TextureBits_t
{
};

class CTextureBits;	// Forward declaration of associated runtime class
DEFINE_RESOURCE_CLASS_TYPE( TextureBits_t, CTextureBits, RESOURCE_TYPE_TEXTURE );
typedef const ResourceBinding_t< CTextureBits > *HRenderTexture;
typedef CStrongHandle< CTextureBits > HRenderTextureStrong;
#define RENDER_TEXTURE_HANDLE_INVALID ( (HRenderTexture)0 )


#endif // TEXTURE_G_H

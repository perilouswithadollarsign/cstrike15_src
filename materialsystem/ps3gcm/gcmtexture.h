//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Texture Layout, CPs3gcmTexture, and CPs3gcmTextureData_t 
//
//==================================================================================================

#ifndef INCLUDED_GCMTEXTURE_H
#define INCLUDED_GCMTEXTURE_H

#include "ps3/ps3_platform.h"

#include "ps3gcmmemory.h"
#include "gcmstate.h"

//--------------------------------------------------------------------------------------------------
// Literals
//--------------------------------------------------------------------------------------------------

#define PS3_TEX_MAX_FORMAT_COUNT 48
#define PS3_TEX_CANONICAL_FORMAT_COUNT 19

//--------------------------------------------------------------------------------------------------
// Texture layout, texture etc..
//--------------------------------------------------------------------------------------------------

struct ALIGN16 CPs3gcmTextureLayout
{
#ifndef _CERT
	char		*m_layoutSummary;	// for debug visibility
#endif

	// format mapping description
	struct ALIGN16 Format_t
	{
#ifndef _CERT
		char		*m_formatSummary;	// for debug visibility
#endif

		enum GcmCaps_t
		{
			kCapSRGB		=	(1<<0),		// GCM can sample it as SRGB
			kCap4xBlocks	=	(1<<1),		// Pitch is referring to 4 texel blocks and not single texel blocks (DXT)
		};

		D3DFORMAT	m_d3dFormat;		// what D3D knows it as; see public/bitmap/imageformat.h

		uint32		m_gcmRemap;			// GCM remap mask
		uint16		m_gcmPitchPer4X;	// GCM pitch multiplier per every 4 pixels of width
		uint8		m_gcmFormat;		// GCM format
		uint8		m_gcmCaps;			// GCM caps of this texture
	}
	ALIGN16_POST;

	// const inputs used for hashing
	struct Key_t
	{
		D3DFORMAT			m_texFormat;				// D3D texel format
		uint16				m_size[3];					// dimensions of the base mip
		uint8				m_texFlags;					// mipped, autogen mips, render target, ... ?
		uint8				m_nActualMipCount;			// Actual number of mips; on console builds, we typically drop the smallest (highest index) 
		// mips to save space (they waste a lot of space for page-alignment reasons)
		// high-bit 0x80 indicates cubemap
	};

	// layout flags
	enum Flags_t
	{
		kfDynamicNoSwizzle	=	(1<<0),		// Indicates whether this texture needs to keep a backing store for incremental updates.
		// (On PS3 this will prevent texture from being swizzled to allow CPU writes at subrect offsets)
		kfMip				=	(1<<1),
		kfMipAuto			=	(1<<2),
		kfTypeRenderable	=	(1<<3),
		kfTypeDepthStencil	=	(1<<4),
		kfTypeCubeMap		=	(1<<5),
		kfSrgbEnabled		=	(1<<6),
		kfNoD3DMemory		=	(1<<7),		// Allocation of storage for the bits has been deferred (call IDirect3DDevice9::AllocateTextureStorage to do the allocation)
		//   -!!--!!- DO NOT ADD MORE FLAGS -!!--!!-  (m_texFlags is only 8 bits)
	};

	// slice information
	struct Slice_t
	{
		uint32	m_storageOffset;	//where in the storage slab does this slice live
		uint32	m_storageSize;		//how much storage does this slice occupy
		uint16	m_size[3];			//texel dimensions of this slice
	};

	//
	// Structure definition
	//

	Key_t m_key;										// key of the layout
	int32 mutable			m_refCount;					// refcount
	uint32					m_storageTotalSize;			// size of storage slab required
	uint16                  m_nFormat;					// format specific info; index in g_ps3texFormats table
	uint8					m_mipCount;					// derived by starting at base size and working down towards 1x1
	CPs3gcmAllocationType_t mutable m_gcmAllocType;				// type of GCM allocation to determine pool/alignment/etc.
#ifndef SPU
	// slice array
	Slice_t					m_slices[0];				// dynamically allocated 2-d array [faces][mips]
public:
	inline int SlicePitch( int iSlice ) const;
	inline int DefaultPitch() const;
	inline const Format_t * GetFormatPtr()const;
#endif
public:

	inline bool IsSwizzled() const { return !( m_key.m_texFlags & ( kfDynamicNoSwizzle | kfTypeRenderable ) ) && IsPowerOfTwo( m_key.m_size[0] ) && IsPowerOfTwo( m_key.m_size[1] ) && IsPowerOfTwo( m_key.m_size[2] ); }
	inline bool IsCubeMap() const { return !!(m_key.m_texFlags & kfTypeCubeMap); }
	inline bool IsVolumeTex() const { return !!(m_key.m_size[2] > 1); }
	inline bool IsTiledMemory() const { return (m_key.m_texFlags & ( kfTypeRenderable | kfDynamicNoSwizzle )) == kfTypeRenderable; }

	inline int FaceCount() const { return ( !IsCubeMap() ) ? 1 : 6; }
	inline int MipCount() const { return ( m_key.m_texFlags & kfMip ) ? m_key.m_nActualMipCount : 1; }

	inline int SlicePitch2( int iSlice, const Slice_t* pSlices, const Format_t *pTexFormats ) const{ return !IsTiledMemory() ? ( ( IsSwizzled() ? pSlices[iSlice].m_size[0] : m_key.m_size[0] ) * pTexFormats[m_nFormat].m_gcmPitchPer4X / 4 ) : pTexFormats[m_nFormat].m_gcmPitchPer4X; }
	inline int DefaultPitch2( const Format_t *pTexFormats ) const { return !IsTiledMemory() ? m_key.m_size[0] * pTexFormats[m_nFormat].m_gcmPitchPer4X / 4 : pTexFormats[m_nFormat].m_gcmPitchPer4X; }

	inline int SliceIndex( int face, int mip ) const { return mip + ( face * MipCount() ); }

public:
#ifndef SPU
	static CPs3gcmTextureLayout const * New( Key_t const &k );
	void Release() const;
#endif
}
ALIGN16_POST;

extern CPs3gcmTextureLayout::Format_t g_ps3texFormats[PS3_TEX_MAX_FORMAT_COUNT];
extern uint g_nPs3texFormatCount;


#ifndef SPU
// convenience functions on PPU that use implicit tables always accessible on PPU
inline int CPs3gcmTextureLayout::SlicePitch( int iSlice ) const
{
	return SlicePitch2( iSlice, &m_slices[0], g_ps3texFormats );
}
inline int CPs3gcmTextureLayout::DefaultPitch() const
{
	return DefaultPitch2( g_ps3texFormats );
}
inline const CPs3gcmTextureLayout::Format_t * CPs3gcmTextureLayout::GetFormatPtr()const
{
	return &g_ps3texFormats[ m_nFormat ];
}
#endif



struct ALIGN16 CPs3gcmTexture
{
	CPs3gcmTextureLayout const *m_layout;  // this structure persists. see CPs3gcmTextureLayout::Release( it asserts if refcount goes down to zero )
	ALIGN16 CPs3gcmLocalMemoryBlock m_lmBlock ALIGN16_POST;     // this structure has the Offset, and the texture bits at that offset persist until all Draw calls are made that use it

	inline uint32 Offset()const { Assert( m_lmBlock.Size() ); return m_lmBlock.Offset(); }
#ifndef SPU
	inline char * Data() { Assert( m_lmBlock.Size() ); return m_lmBlock.DataInAnyMemory(); }
#endif
public:
#ifndef SPU
	static CPs3gcmTexture * New( CPs3gcmTextureLayout::Key_t const &key );
	void Release();
	bool Allocate();
#endif
}
ALIGN16_POST;

struct CPs3gcmTextureData_t
{
	// CPs3gcmTextureLayout const *m_eaLayout
	uint32 m_eaLayout;  // this structure persists. see CPs3gcmTextureLayout::Release( it asserts if refcount goes down to zero )
	uint32 m_nLocalOffset; // the offset of the texture bits

	void Assign( const CPs3gcmTexture * pThat )
	{
		if( pThat )
		{
			m_eaLayout = ( uint32 )pThat->m_layout;
			m_nLocalOffset = pThat->Offset();
			Assert( m_eaLayout ? !( 15 & ( uintp( m_eaLayout ) | m_nLocalOffset ) ) && m_nLocalOffset : !m_nLocalOffset );
		}
		else
		{
			Reset();
		}
	}

	inline uint32 Offset()const { return m_nLocalOffset; }

	void Reset()
	{
		m_eaLayout = 0;
		m_nLocalOffset = 0;
	}

	bool IsNull()const
	{
		return !NotNull();
	}
	bool NotNull()const
	{
		// either both are null, or none is null
		Assert( ( m_eaLayout == 0 ) == ( m_nLocalOffset == 0 ) );
		return m_eaLayout != 0;
	}

	operator bool() const { return NotNull(); }
};


//
// CPs3BindTexture_t : Everything we need to bind a texture
//

// This is what the SPU needs to bind the texture

struct CPs3BindTexture_t
{
	uint8					m_sampler;
	uint8					m_nBindFlags;
	uint8					m_UWrap;
	uint8					m_VWrap;
	uint8					m_WWrap;
	uint8					m_minFilter;
	uint8					m_magFilter;
	uint8					m_mipFilter;

	uint32					m_nLayout;
	CPs3gcmLocalMemoryBlock *m_pLmBlock;

	int						m_boundStd;
	int						m_hTexture;
};

// This is what we store when asked to bind a texture
// When the cmd buffer is executed, at this time we lookup 
// the remaining fields and pack some CPs3BindTexture_t to actually use on the SPU

struct CPs3BindParams_t
{
    uint16                  m_nBindTexIndex;
    uint8					m_sampler;
    uint8                   m_nBindFlags;
    int						m_boundStd;
    int						m_hTexture;
};


#endif // INCLUDED_GCMTEXTURE_H
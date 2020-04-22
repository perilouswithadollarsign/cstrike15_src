//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef TEXTUREHEAP_H
#define TEXTUREHEAP_H

// Portal2 Console is not using due to amount of memory free on Xbox and RSX memory on PS3.
// The desired console pattern is to have a similar footprint, because PS3 does not have a streaming solution and it has enough texture memory
// the texture content choices for the consoles will be made to adapt.

// Uncomment to allow system to operate
//#define SUPPORTS_TEXTURE_STREAMING

#if defined( _X360 )

#include "locald3dtypes.h"
#include "utllinkedlist.h"
#include "filesystem.h"
#include "materialsystem/itexture.h"

typedef int TextureCacheHandle_t;
#define INVALID_TEXTURECACHE_HANDLE 0

class CTextureHeap
{
public:
	CTextureHeap();

	IDirect3DTexture		*AllocTexture( int width, int height, int levels, DWORD usage, D3DFORMAT format, bool bNoD3DMemory, bool bCacheable );
	IDirect3DCubeTexture	*AllocCubeTexture( int width, int levels, DWORD usage, D3DFORMAT format, bool bNoD3DMemory );
	IDirect3DVolumeTexture	*AllocVolumeTexture( int width, int height, int depth, int levels, DWORD usage, D3DFORMAT format );
	IDirect3DSurface		*AllocRenderTargetSurface( int width, int height, D3DFORMAT format, RTMultiSampleCount360_t multiSampleCount = RT_MULTISAMPLE_NONE, int base = -1 );

	// Perform the real d3d allocation, returns true if succesful, false otherwise.
	// Only valid for a texture created with no d3d bits, otherwise no-op.
	bool					FixupAllocD3DMemory( IDirect3DBaseTexture *pTexture );

	// Release header and d3d bits.
	void					FreeTexture( IDirect3DBaseTexture *pTexture );

	// Returns the total amount of memory needed or allocated for the entire texture.
	int						GetSize( IDirect3DBaseTexture *pTexture );
	// Returns the amount of memory needed just for the cacheable component.
	int						GetCacheableSize( IDirect3DBaseTexture *pTexture );

	// Crunch the heap.
	void					Compact();

	// Get current backbuffer multisample type
	D3DMULTISAMPLE_TYPE		GetBackBufferMultiSampleType();

	// Query to determine if the texture is managed by cacheing.
	bool					IsTextureCacheManaged( IDirect3DBaseTexture *pD3DTexture );
	bool					IsBaseAllocated( IDirect3DBaseTexture *pD3DTexture );
	bool					IsTextureResident( IDirect3DBaseTexture *pD3DTexture );

	// update the lru for a texture. returns false if the high mipmpa is not valid
	bool                    TouchTexture( class CXboxTexture *pXboxTexture );
	void					SetCacheableTextureParams( IDirect3DBaseTexture *pD3DTexture, const char *pFilename, int mipSkipCount );
	void					FlushTextureCache();
	void					SpewTextureCache();
	int						GetCacheableHeapSize();

private:
	bool					RestoreCacheableTexture( IDirect3DBaseTexture *pD3DTexture );

	CUtlFixedLinkedList< IDirect3DBaseTexture* >	m_TextureCache;
};

#if defined( SUPPORTS_TEXTURE_STREAMING )
	#define BASEPOOL1024_SIZE	( 12*1024*1024 )	// 1024x1024 DXT5
	#define BASEPOOL512_SIZE	( 12*1024*1024 )	// 1024x1024 DXT1
	#define BASEPOOL256_SIZE	( 16*1024*1024 )	// 512x512 DXT5
	#define BASEPOOL128_SIZE	( 16*1024*1024 )	// 512x512 DXT1
	#define BASEPOOL64_SIZE		( 4*1024*1024 )		// 256x256 DXT5
	#define BASEPOOL32_SIZE		( 4*1024*1024 )		// 256x256 DXT1
#else
	#define BASEPOOL1024_SIZE	0
	#define BASEPOOL512_SIZE	0
	#define BASEPOOL256_SIZE	0
	#define BASEPOOL128_SIZE	0
	#define BASEPOOL64_SIZE		0
	#define BASEPOOL32_SIZE		0
#endif

enum TextureAllocator_t
{
	TA_BASEPOOL_1024,	// 1024K = 1024x1024 DXT5 Mip0
	TA_BASEPOOL_512,	// 512K = 1024x1024 DXT1 Mip0
	TA_BASEPOOL_256,	// 256K = 512x512 DXT5 Mip0
	TA_BASEPOOL_128,	// 128K = 512x512 DXT1 Mip0
	TA_BASEPOOL_64,		// 64K = 256x256 DXT5 Mip0
	TA_BASEPOOL_32,		// 32K = 256x256 DXT1 Mip0
	TA_MIXED,
	TA_STANDARD,
	TA_MAX
};

enum TextureLoadError_t
{
	TEXLOADERROR_NONE     = 0,
	TEXLOADERROR_FILEOPEN = -1,
	TEXLOADERROR_READING  = -2,
};

struct THBaseInfo_t
{
	THBaseInfo_t()
	{
		m_tcHandle = INVALID_TEXTURECACHE_HANDLE;
		m_hFilename = NULL;
		m_hAsyncControl = NULL;
		m_nFrameCount = 0;
		m_nBaseSize = 0;
		m_nMipSize = 0;
		m_nMipSkipCount = 0;
		m_bBaseAllocated = false;
		m_bMipAllocated = false;
		m_BaseValid = 0;
	}

	TextureAllocator_t		m_fAllocator;

	// for cacheable tetxures
	TextureCacheHandle_t	m_tcHandle;
	FileNameHandle_t		m_hFilename;
	FSAsyncControl_t		m_hAsyncControl;

	// for age, tracks which frame a texture was touched, fastest GPU non-blocking technique
	int			            m_nFrameCount;

	// base and mip are both non-zero for cacheable textures, which require mips
	// a non-cacheable texture only has a non-zero base, regardless of its mips
	unsigned int			m_nBaseSize;
	unsigned int			m_nMipSize;

	// used for texture restoration, the top mip
	int						m_nMipSkipCount;

	// tracks when valid for rendering, r/w across threads
	CInterlockedInt			m_BaseValid;			

	// tracks when valid for i/o loading
	bool					m_bBaseAllocated : 1;
	// tracks when mip is a true seperate allocation, used to resolve offset or pointer
	bool					m_bMipAllocated : 1;
};


//-----------------------------------------------------------------------------
// Get Texture HW bases
//-----------------------------------------------------------------------------
inline void *GetD3DTextureBasePtr( IDirect3DBaseTexture* pTex )
{
	return (void *)( (unsigned int)pTex->Format.BaseAddress << 12 );
}
inline void *GetD3DTextureMipPtr( IDirect3DBaseTexture* pTex )
{
	// could be an offset or a pointer, state dependant
	return (void *)( (unsigned int)pTex->Format.MipAddress << 12 );
}

extern CTextureHeap g_TextureHeap;

struct THInfo_t : public THBaseInfo_t
{
	// Mixed heap info
	int		nLogicalBytes;
	int		nBytes;
	bool	bFree:1;
	bool	bNonTexture:1;

	THInfo_t *pPrev, *pNext;
};

class CXboxTexture : public IDirect3DTexture, public THInfo_t
{
public:
	CXboxTexture() 
	  : bImmobile( false )
	{
	}

	bool CanRelocate()	{ return ( !bImmobile && !IsBusy() ); }

	bool bImmobile;
};

class CXboxCubeTexture : public IDirect3DCubeTexture, public THBaseInfo_t
{
};

class CXboxVolumeTexture : public IDirect3DVolumeTexture, public THBaseInfo_t
{
};

#endif
#endif // TEXTUREHEAP_H

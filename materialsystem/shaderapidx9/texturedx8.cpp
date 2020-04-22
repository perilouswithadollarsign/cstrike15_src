//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "locald3dtypes.h"
#include "texturedx8.h"
#include "shaderapidx8_global.h"
#include "colorformatdx8.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/imaterialsystem.h"
#include "utlvector.h"
#include "recording.h"
#include "shaderapi/ishaderapi.h"
#include "filesystem.h"
#include "locald3dtypes.h"
#include "textureheap.h"
#include "tier1/utlbuffer.h"
#include "tier1/callqueue.h"

#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

#ifdef _WIN32
#pragma warning (disable:4189 4701)
#endif

static int s_TextureCount = 0;

//-----------------------------------------------------------------------------
// Stats...
//-----------------------------------------------------------------------------

int TextureCount()
{
	return s_TextureCount;
}

static bool IsVolumeTexture( IDirect3DBaseTexture* pBaseTexture )
{
	if ( !pBaseTexture )
	{
		return false;
	}

	return ( pBaseTexture->GetType() == D3DRTYPE_VOLUMETEXTURE );
}

static HRESULT GetLevelDesc( IDirect3DBaseTexture* pBaseTexture, UINT level, D3DSURFACE_DESC* pDesc )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pBaseTexture )
	{
		return ( HRESULT )-1;
	}

	HRESULT hr;
	switch( pBaseTexture->GetType() )
	{
	case D3DRTYPE_TEXTURE:
		hr = ( ( IDirect3DTexture * )pBaseTexture )->GetLevelDesc( level, pDesc );
		break;
	case D3DRTYPE_CUBETEXTURE:
		hr = ( ( IDirect3DCubeTexture * )pBaseTexture )->GetLevelDesc( level, pDesc );
		break;
	default:
		return ( HRESULT )-1;
	}
	return hr;
}

static HRESULT GetSurfaceFromTexture( IDirect3DBaseTexture* pBaseTexture, UINT level, 
									  D3DCUBEMAP_FACES cubeFaceID, IDirect3DSurface** ppSurfLevel )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pBaseTexture )
	{
		return ( HRESULT )-1;
	}

	HRESULT hr;

	switch( pBaseTexture->GetType() )
	{
	case D3DRTYPE_TEXTURE:
		hr = ( ( IDirect3DTexture * )pBaseTexture )->GetSurfaceLevel( level, ppSurfLevel );
		break;
	case D3DRTYPE_CUBETEXTURE:
		if (cubeFaceID !=0)
		{
			//Debugger();
		}
		
		hr = ( ( IDirect3DCubeTexture * )pBaseTexture )->GetCubeMapSurface( cubeFaceID, level, ppSurfLevel );
		break;
	default:
		Assert(0);
		return ( HRESULT )-1;
	}
	return hr;
}

//-----------------------------------------------------------------------------
// Gets the image format of a texture
//-----------------------------------------------------------------------------
static ImageFormat GetImageFormat( IDirect3DBaseTexture* pTexture )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( pTexture )
	{
		HRESULT hr;
		if ( !IsVolumeTexture( pTexture ) )
		{
			D3DSURFACE_DESC desc;
			hr = GetLevelDesc( pTexture, 0, &desc );
			if ( !FAILED( hr ) )
				return ImageLoader::D3DFormatToImageFormat( desc.Format );
		}
		else
		{
			D3DVOLUME_DESC desc;
			IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( pTexture );
			hr = pVolumeTexture->GetLevelDesc( 0, &desc );
			if ( !FAILED( hr ) )
				return ImageLoader::D3DFormatToImageFormat( desc.Format );
		}
	}

	// Bogus baby!
	return (ImageFormat)-1;
}


//-----------------------------------------------------------------------------
// Allocates the D3DTexture
//-----------------------------------------------------------------------------
IDirect3DBaseTexture* CreateD3DTexture( int width, int height, int nDepth, 
		ImageFormat dstFormat, int numLevels, int nCreationFlags, char *debugLabel )		// OK to skip the last param
{
	if ( nDepth <= 0 )
	{
		nDepth = 1;
	}

	bool bIsCubeMap = ( nCreationFlags & TEXTURE_CREATE_CUBEMAP ) != 0;
	bool bIsRenderTarget = ( nCreationFlags & TEXTURE_CREATE_RENDERTARGET ) != 0;
	bool bManaged = ( nCreationFlags & TEXTURE_CREATE_MANAGED ) != 0;
	bool bIsDepthBuffer = ( nCreationFlags & TEXTURE_CREATE_DEPTHBUFFER ) != 0;
	bool bIsDynamic = ( nCreationFlags & TEXTURE_CREATE_DYNAMIC ) != 0;
	bool bAutoMipMap = ( nCreationFlags & TEXTURE_CREATE_AUTOMIPMAP ) != 0;
	bool bVertexTexture = ( nCreationFlags & TEXTURE_CREATE_VERTEXTEXTURE ) != 0;
	bool bAllowNonFilterable = ( nCreationFlags & TEXTURE_CREATE_UNFILTERABLE_OK ) != 0;
	bool bVolumeTexture = ( nDepth > 1 );
	bool bNoD3DBits = ( nCreationFlags & TEXTURE_CREATE_NOD3DMEMORY ) != 0;
	bool bSysMem = ( nCreationFlags & TEXTURE_CREATE_SYSMEM ) != 0;
	bool bDefaultPool = ( nCreationFlags & TEXTURE_CREATE_DEFAULT_POOL ) != 0;
	bool bCacheable = ( nCreationFlags & TEXTURE_CREATE_CACHEABLE ) != 0;
	bool bSRGB = (nCreationFlags & TEXTURE_CREATE_SRGB) != 0;				// for Posix/GL only
	bool bAnsiotropic = (nCreationFlags & TEXTURE_CREATE_ANISOTROPIC) != 0;	// for Posix/GL only

	// NOTE: This function shouldn't be used for creating depth buffers!
	Assert( !bIsDepthBuffer );

	D3DFORMAT d3dFormat = D3DFMT_UNKNOWN;

	if ( IsX360() )
	{
		// 360 does not support vertex textures
		// 360 render target creation path is for the target as a texture source (NOT the EDRAM version)
		// use normal texture format rules
		Assert( !bVertexTexture );
		if ( !bVertexTexture )
		{
			d3dFormat = FindNearestSupportedFormat( dstFormat, false, false, false );
		}
	}
	else
	{
		d3dFormat = FindNearestSupportedFormat( dstFormat, bVertexTexture, bIsRenderTarget, bAllowNonFilterable );
	}

	if ( d3dFormat == D3DFMT_UNKNOWN )
	{
		Warning( "ShaderAPIDX8::CreateD3DTexture: Invalid color format!\n" );
		Assert( 0 );
		return 0;
	}

	IDirect3DBaseTexture* pBaseTexture = NULL;
	IDirect3DTexture* pD3DTexture = NULL;
	IDirect3DCubeTexture* pD3DCubeTexture = NULL;
	IDirect3DVolumeTexture* pD3DVolumeTexture = NULL;
	HRESULT hr = S_OK;
	DWORD usage = 0;

	if ( bIsRenderTarget )
	{
		usage |= D3DUSAGE_RENDERTARGET;
	}
	if ( bIsDynamic )
	{
		usage |= D3DUSAGE_DYNAMIC;
	}
	if ( bAutoMipMap )
	{
		usage |= D3DUSAGE_AUTOGENMIPMAP;
	}

#ifdef DX_TO_GL_ABSTRACTION
	if ( bSRGB )
	{
		// This flag does not exist in real DX9... just for GL to know that this is an SRGB tex
		usage |= D3DUSAGE_TEXTURE_SRGB;
	}
#endif

#if defined( _PS3 )
	if ( bNoD3DBits && !bVolumeTexture )
	{
		// This flag does not exist in real DX9... tells GCM to defer allocating storage for the bits (until CShaderAPIDx8::PostQueuedTexture)
		usage |= D3DUSAGE_TEXTURE_NOD3DMEMORY;
	}
#endif

	if ( bIsCubeMap )
	{
#if !defined( _X360 )
		hr = Dx9Device()->CreateCubeTexture( 
				width,
				numLevels,
				usage,
				d3dFormat,
				bManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT, 
				&pD3DCubeTexture,
				NULL
	#if ( defined( DX_TO_GL_ABSTRACTION ) && !defined( _PS3 ) )
				, debugLabel					// tex create funcs take extra arg for debug name on GL
	#endif
				   );
#else
		pD3DCubeTexture = g_TextureHeap.AllocCubeTexture( width, numLevels, usage, d3dFormat, bNoD3DBits );
#endif
		pBaseTexture = pD3DCubeTexture;
	}
	else if ( bVolumeTexture )
	{
		Assert( !bNoD3DBits );
#if !defined( _X360 )
		hr = Dx9Device()->CreateVolumeTexture( 
				width, 
				height, 
				nDepth,
				numLevels, 
				usage, 
				d3dFormat, 
				bManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT, 
				&pD3DVolumeTexture,
				NULL
	#if ( defined( DX_TO_GL_ABSTRACTION ) && !defined( _PS3 ) )
				, debugLabel					// tex create funcs take extra arg for debug name on GL
	#endif
				  );
#else
		pD3DVolumeTexture = g_TextureHeap.AllocVolumeTexture( width, height, nDepth, numLevels, usage, d3dFormat );
#endif
		pBaseTexture = pD3DVolumeTexture;
	}
	else
	{
#if !defined( _X360 )
		// Override usage and managed params if using special hardware shadow depth map formats...
		if ( ( d3dFormat == NVFMT_RAWZ ) || ( d3dFormat == NVFMT_INTZ   ) || 
		     ( d3dFormat == D3DFMT_D16 ) || ( d3dFormat == D3DFMT_D24S8 ) || 
			 ( d3dFormat == ATIFMT_D16 ) || ( d3dFormat == ATIFMT_D24S8 ) )
		{
			// Not putting D3DUSAGE_RENDERTARGET here causes D3D debug spew later, but putting the flag causes this create to fail...
			usage = D3DUSAGE_DEPTHSTENCIL;
			bManaged = false;
		}

		// Override managed param if using special null texture format
		if ( d3dFormat == NVFMT_NULL )
		{
			bManaged = false;
		}

		D3DPOOL d3dPool = D3DPOOL_DEFAULT;
		if ( bSysMem )
			d3dPool = D3DPOOL_SYSTEMMEM;
		else if ( bManaged )
			d3dPool = D3DPOOL_MANAGED;

		hr = Dx9Device()->CreateTexture(
				width,
				height,
				numLevels, 
				usage,
				d3dFormat,
				d3dPool,
				&pD3DTexture,
				NULL
	#if ( defined( DX_TO_GL_ABSTRACTION ) && !defined( _PS3 ) )
				, debugLabel					// tex create funcs take extra arg for debug name on GL
	#endif
				 );

#else
		pD3DTexture = g_TextureHeap.AllocTexture( width, height, numLevels, usage, d3dFormat, bNoD3DBits, bCacheable );
		if ( pD3DTexture == NULL )
		{
			Warning( "ShaderAPIDX8::CreateD3DTexture: TexureHeap out of memory.\n" );
			g_pMemAlloc->DumpStats();
			return 0;
		}
#endif
		pBaseTexture = pD3DTexture;
	}

    if ( FAILED( hr ) )
	{
		switch ( hr )
		{
		case D3DERR_INVALIDCALL:
			Warning( "ShaderAPIDX8::CreateD3DTexture: D3DERR_INVALIDCALL\n" );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			// This conditional is here so that we don't complain when testing
			// how much video memory we have. . this is kinda gross.
			if ( bManaged )
			{
				Warning( "ShaderAPIDX8::CreateD3DTexture: D3DERR_OUTOFVIDEOMEMORY\n" );
			}
			break;
		case E_OUTOFMEMORY:
			Warning( "ShaderAPIDX8::CreateD3DTexture: E_OUTOFMEMORY\n" );
			break;
		default:
			break;
		}
		return 0;
	}

#ifdef MEASURE_DRIVER_ALLOCATIONS
	int nMipCount = numLevels;
	if ( !nMipCount )
	{
		while ( width > 1 || height > 1 )
		{
			width >>= 1;
			height >>= 1;
			++nMipCount;
		}
	}

	int nMemUsed = nMipCount * 1.1f * 1024;
	if ( bIsCubeMap )
	{
		nMemUsed *= 6;
	}

	VPROF_INCREMENT_GROUP_COUNTER( "texture count", COUNTER_GROUP_NO_RESET, 1 );
	VPROF_INCREMENT_GROUP_COUNTER( "texture driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
#endif

	++s_TextureCount;

	return pBaseTexture;
}


//-----------------------------------------------------------------------------
// Texture destruction
//-----------------------------------------------------------------------------
void ReleaseD3DTexture( IDirect3DBaseTexture* pD3DTex )
{
	int ref = pD3DTex->Release();
	Assert( ref == 0 );
}

void DestroyD3DTexture( IDirect3DBaseTexture* pD3DTex )
{
	if ( pD3DTex )
	{
#ifdef MEASURE_DRIVER_ALLOCATIONS
		D3DRESOURCETYPE type = pD3DTex->GetType();
		int nMipCount = pD3DTex->GetLevelCount();
		if ( type == D3DRTYPE_CUBETEXTURE )
		{
			nMipCount *= 6;
		}
		int nMemUsed = nMipCount * 1.1f * 1024;
		VPROF_INCREMENT_GROUP_COUNTER( "texture count", COUNTER_GROUP_NO_RESET, -1 );
		VPROF_INCREMENT_GROUP_COUNTER( "texture driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
#endif

#if !defined( _X360 )
		CMatRenderContextPtr pRenderContext( materials );
		ICallQueue *pCallQueue;
		if ( ( pCallQueue = pRenderContext->GetCallQueue() ) != NULL )
		{
			pCallQueue->QueueCall( ReleaseD3DTexture, pD3DTex );
		}
		else
		{
			ReleaseD3DTexture( pD3DTex );
		}
#else
		g_TextureHeap.FreeTexture( pD3DTex );
#endif
		--s_TextureCount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTex - 
// Output : int
//-----------------------------------------------------------------------------
int GetD3DTextureRefCount( IDirect3DBaseTexture *pTex )
{
	if ( !pTex )
		return 0;

	pTex->AddRef();
	int ref = pTex->Release();

	return ref;
}

//-----------------------------------------------------------------------------
// See version 13 for a function that converts a texture to a mipmap (ConvertToMipmap)
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Lock, unlock a texture...
//-----------------------------------------------------------------------------

static RECT s_LockedSrcRect;
static D3DLOCKED_RECT s_LockedRect;
#ifdef DBGFLAG_ASSERT
static bool s_bInLock = false;
#endif

bool LockTexture( ShaderAPITextureHandle_t bindId, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID, int xOffset, int yOffset, int width, int height, bool bDiscard,
	CPixelWriter& writer )
{
	Assert( !s_bInLock );
	
	IDirect3DSurface* pSurf;
	HRESULT hr = GetSurfaceFromTexture( pTexture, level, cubeFaceID, &pSurf );
	if ( FAILED( hr ) )
		return false;

	s_LockedSrcRect.left = xOffset;
	s_LockedSrcRect.right = xOffset + width;
	s_LockedSrcRect.top = yOffset;
	s_LockedSrcRect.bottom = yOffset + height;

	unsigned int flags = D3DLOCK_NOSYSLOCK;
	flags |= bDiscard ? D3DLOCK_DISCARD : 0;
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( bindId );
	RECORD_INT( copy );
	RECORD_INT( level );
	RECORD_INT( cubeFaceID );
	RECORD_STRUCT( &s_LockedSrcRect, sizeof(s_LockedSrcRect) );
	RECORD_INT( flags );

	hr = pSurf->LockRect( &s_LockedRect, &s_LockedSrcRect, flags );
	pSurf->Release();

	if ( FAILED( hr ) )
		return false;

	writer.SetPixelMemory( GetImageFormat(pTexture), s_LockedRect.pBits, s_LockedRect.Pitch );

#ifdef DBGFLAG_ASSERT
	s_bInLock = true;
#endif
	return true;
}

void UnlockTexture( ShaderAPITextureHandle_t bindId, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID )
{
	Assert( s_bInLock );

	IDirect3DSurface* pSurf;
	HRESULT hr = GetSurfaceFromTexture( pTexture, level, cubeFaceID, &pSurf );
	if (FAILED(hr))
		return;

#ifdef RECORD_TEXTURES 
	int width = s_LockedSrcRect.right - s_LockedSrcRect.left;
	int height = s_LockedSrcRect.bottom - s_LockedSrcRect.top;
	int imageFormatSize = ImageLoader::SizeInBytes( GetImageFormat( pTexture ) );
	Assert( imageFormatSize != 0 );
	int validDataBytesPerRow = imageFormatSize * width;
	int storeSize = validDataBytesPerRow * height;
	static CUtlVector< unsigned char > tmpMem;
	if( tmpMem.Size() < storeSize )
	{
		tmpMem.AddMultipleToTail( storeSize - tmpMem.Size() );
	}
	unsigned char *pDst = tmpMem.Base();
	unsigned char *pSrc = ( unsigned char * )s_LockedRect.pBits;
	RECORD_COMMAND( DX8_SET_TEXTURE_DATA, 3 );
	RECORD_INT( validDataBytesPerRow );
	RECORD_INT( height );
	int i;
	for( i = 0; i < height; i++ )
	{
		memcpy( pDst, pSrc, validDataBytesPerRow );
		pDst += validDataBytesPerRow;
		pSrc += s_LockedRect.Pitch;
	}
	RECORD_STRUCT( tmpMem.Base(), storeSize );
#endif // RECORD_TEXTURES 
	
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( bindId );
	RECORD_INT( copy );
	RECORD_INT( level );
	RECORD_INT( cubeFaceID );

	hr = pSurf->UnlockRect();
	pSurf->Release();
#ifdef DBGFLAG_ASSERT
	s_bInLock = false;
#endif
}

//-----------------------------------------------------------------------------
// Compute texture size based on compression
//-----------------------------------------------------------------------------

static inline int DetermineGreaterPowerOfTwo( int val )
{
	int num = 1;
	while (val > num)
	{
		num <<= 1;
	}

	return num;
}

inline int DeterminePowerOfTwo( int val )
{
	int pow = 0;
	while ((val & 0x1) == 0x0)
	{
		val >>= 1;
		++pow;
	}

	return pow;
}


//-----------------------------------------------------------------------------
// Blit in bits
//-----------------------------------------------------------------------------
// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
// OPTIMIZE??: could lock the texture directly instead of the surface in dx9.
#if !defined( _X360 )
static void BlitSurfaceBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	// Get the level of the texture we want to write into
	IDirect3DSurface* pTextureLevel;

	if (info.m_CubeFaceID !=0)
	{
		//Debugger();
	}


	HRESULT hr = GetSurfaceFromTexture( info.m_pTexture, info.m_nLevel, info.m_CubeFaceID, &pTextureLevel );
	if ( FAILED( hr ) )
		return;

	RECT			srcRect;
	D3DLOCKED_RECT	lockedRect;

	srcRect.left   = xOffset;
	srcRect.right  = xOffset + info.m_nWidth;
	srcRect.top    = yOffset;
	srcRect.bottom = yOffset + info.m_nHeight;

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	// lock the region (could be the full surface or less)
	if ( FAILED( pTextureLevel->LockRect( &lockedRect, &srcRect, D3DLOCK_NOSYSLOCK ) ) )
	{
		Warning( "CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect\n" );
		pTextureLevel->Release();
		return;
	}

	ImageFormat dstFormat = GetImageFormat( info.m_pTexture );
	unsigned char *pImage = (unsigned char *)lockedRect.pBits;
	// garymcthack : need to make a recording command for this.
	ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
						pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedRect.Pitch );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	if ( FAILED( pTextureLevel->UnlockRect() ) ) 
	{
		Warning( "CShaderAPIDX8::BlitTextureBits: couldn't unlock texture rect\n" );
		pTextureLevel->Release();
		return;
	}
	
	pTextureLevel->Release();
}
#endif

//-----------------------------------------------------------------------------
// Puts 2D texture data into 360 gpu memory.
//-----------------------------------------------------------------------------
#if defined( _X360 )
static void BlitSurfaceBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	// xbox textures are NOT backed in gpu memory contiguously
	// stride details are critical - see [Xbox 360 Texture Storage]
	// a d3dformat identifier on the xbox is tiled, the same d3dformat on the pc is expected linear to the app
	// we purposely hide the tiling here, otherwise much confusion for the pc
	// the *entire* target must be un-tiled *only* before any *subrect* blitting linear work
	// the *entire* target must then be re-tiled after the *subrect* blit
	// procedural textures require this to subrect blit their new portions correctly
	// the tiling dance can be avoided if the source and target match in tiled state during a full rect blit

	if ( info.m_nLevel == 0 && !g_TextureHeap.IsBaseAllocated( info.m_pTexture ) )
	{
		return;
	}

	if ( info.m_bSrcIsTiled )
	{
		// not supporting subrect blitting from a tiled source
		Assert( 0 );
		return;
	}

	CUtlBuffer formatConvertMemory;
	unsigned char *pSrcData = info.m_pSrcData;

	ImageFormat	dstFormat = GetImageFormat( info.m_pTexture );
	if ( dstFormat != info.m_SrcFormat )
	{
		if ( !info.m_bCanConvertFormat )
		{
			// texture is expected to be in target format
			// not supporting conversion of a tiled source
			Assert( 0 );
			return;
		}

		int srcSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, info.m_SrcFormat, false );
		int dstSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, dstFormat, false );
		formatConvertMemory.EnsureCapacity( dstSize );

		// due to format conversion, source is in non-native order
		ImageLoader::PreConvertSwapImageData( (unsigned char*)info.m_pSrcData, srcSize, info.m_SrcFormat, VTF_CONSOLE_360, info.m_nWidth, srcStride );

		// slow conversion operation
		if ( !ShaderUtil()->ConvertImageFormat( 
				info.m_pSrcData,
				info.m_SrcFormat,
				(unsigned char*)formatConvertMemory.Base(),
				dstFormat,
				info.m_nWidth,
				info.m_nHeight,
				srcStride,
				0 ) )
		{
			// conversion failed
			Assert( 0 );
			return;
		}

		// due to format conversion, source must have been in non-native order
		ImageLoader::PostConvertSwapImageData( (unsigned char*)formatConvertMemory.Base(), dstSize, dstFormat, VTF_CONSOLE_360 );

		pSrcData = (unsigned char*)formatConvertMemory.Base();
	}

	// get the top mip level info (needed for proper sub mip access)
	XGTEXTURE_DESC baseDesc;
	XGGetTextureDesc( info.m_pTexture, 0, &baseDesc );
	bool bDstIsTiled = XGIsTiledFormat( baseDesc.Format ) == TRUE;

	// get the target mip level info
	XGTEXTURE_DESC mipDesc;
	XGGetTextureDesc( info.m_pTexture, info.m_nLevel, &mipDesc );
	bool bFullSurfBlit = ( mipDesc.Width == (unsigned)info.m_nWidth && mipDesc.Height == (unsigned)info.m_nHeight );

	// get the mip level of the texture we want to write into
	IDirect3DSurface* pTextureLevel = NULL;
	unsigned char *pTargetImage;
	if ( info.m_nLevel == 0 )
	{
		// can bypass API to gain pointer, circumventing a D3DRIP when async texture streaming
		pTargetImage = (unsigned char *)GetD3DTextureBasePtr( info.m_pTexture );
		// textures can have a non-zero offset due to texture tiles
		pTargetImage += XGGetMipLevelOffset( info.m_pTexture, info.m_CubeFaceID, 0 );
	}
	else
	{
		// using API to avoid determining true mip bases
		// texture streaming does not blat in mips, so no D3DRIP
		HRESULT hr = GetSurfaceFromTexture( info.m_pTexture, info.m_nLevel, info.m_CubeFaceID, &pTextureLevel );
		if ( FAILED( hr ) )
		{
			Warning( "CShaderAPIDX8::BlitSurfaceBits: GetSurfaceFromTexture() failure\n" );
			return;
		}
		D3DLOCKED_RECT lockedRect;
		hr = pTextureLevel->LockRect( &lockedRect, NULL, D3DLOCK_NOSYSLOCK );
		if ( FAILED( hr ) )
		{
			Warning( "CShaderAPIDX8::BlitSurfaceBits: couldn't lock texture rect\n" );
			pTextureLevel->Release();
			return;
		}
		pTargetImage = (unsigned char *)lockedRect.pBits;
	}

	POINT p;
	p.x = xOffset;
	p.y = yOffset;

	RECT r;
	r.left = 0;
	r.top = 0;
	r.right = info.m_nWidth;
	r.bottom = info.m_nHeight;

	int blockSize = mipDesc.Width/mipDesc.WidthInBlocks;
	if ( !srcStride )
	{
		srcStride = (mipDesc.Width/blockSize)*mipDesc.BytesPerBlock;
	}

	// subrect blitting path
	if ( !bDstIsTiled )
	{
		// Copy the subrect without conversion
		HRESULT hr = XGCopySurface(
				pTargetImage,
				mipDesc.RowPitch,
				mipDesc.Width,
				mipDesc.Height,
				mipDesc.Format,
				&p,
				pSrcData,
				srcStride,
				mipDesc.Format,
				&r,
				0,
				0 );
		if ( FAILED( hr ) )
		{
			Warning( "CShaderAPIDX8::BlitSurfaceBits: failed subrect copy\n" );
		}
	}
	else
	{
		int tileFlags = 0;
		if ( !( mipDesc.Flags & XGTDESC_PACKED ) )
			tileFlags |= XGTILE_NONPACKED;
		if ( mipDesc.Flags & XGTDESC_BORDERED )
			tileFlags |= XGTILE_BORDER;

		// tile the temp store back into the target surface
		XGTileTextureLevel(
			baseDesc.Width,
			baseDesc.Height,
			info.m_nLevel,
			XGGetGpuFormat( baseDesc.Format ),
			tileFlags,
			pTargetImage,
			&p,
			pSrcData,
			srcStride,
			&r );
	}

	if ( pTextureLevel )
	{
		pTextureLevel->UnlockRect();
		pTextureLevel->Release();
	}
}
#endif

#if defined( _PS3 )
static void BlitVolumeBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	if ( xOffset || yOffset || /*info.m_nZOffset ||*/ srcStride )
	{
		// not supporting any subvolume blitting
		// the entire volume per mip must be blitted
		Assert( 0 );
		return;
	}

	ImageFormat	dstFormat = GetImageFormat( info.m_pTexture );
	if ( dstFormat != info.m_SrcFormat )
	{
		// texture is expected to be in target format
		// not supporting conversion
		Assert( 0 );
		return;
	}

	D3DBOX srcBox;
	D3DLOCKED_BOX lockedBox;
	srcBox.Left = xOffset;
	srcBox.Right = xOffset + info.m_nWidth;
	srcBox.Top = yOffset;
	srcBox.Bottom = yOffset + info.m_nHeight;
	srcBox.Front = 0;
	srcBox.Back = info.m_nZOffset;	// doesn't matter, locking entire texture effectively

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );
	if ( FAILED( pVolumeTexture->LockBox( info.m_nLevel, &lockedBox, &srcBox, D3DLOCK_NOSYSLOCK ) ) )
	{
		Warning( "BlitVolumeBits: couldn't lock volume texture rect\n" );
		return;
	}

	// <vitaliy> blast the bits straight into local memory
	unsigned char *pImage = (unsigned char *)lockedBox.pBits;
	size_t numBytes = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, info.m_nZOffset, dstFormat, false );
	memcpy( pImage, info.m_pSrcData, numBytes );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	if ( FAILED( pVolumeTexture->UnlockBox( info.m_nLevel ) ) ) 
	{
		Warning( "BlitVolumeBits: couldn't unlock volume texture rect\n" );
		return;
	}
}
#endif

//-----------------------------------------------------------------------------
// Blit in bits
//-----------------------------------------------------------------------------
#if !defined( _X360 ) && !defined( _PS3 )
static void BlitVolumeBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	D3DBOX srcBox;
	D3DLOCKED_BOX lockedBox;
	srcBox.Left = xOffset;
	srcBox.Right = xOffset + info.m_nWidth;
	srcBox.Top = yOffset;
	srcBox.Bottom = yOffset + info.m_nHeight;
	srcBox.Front = info.m_nZOffset;
	srcBox.Back = info.m_nZOffset + 1;

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );
	if ( FAILED( pVolumeTexture->LockBox( info.m_nLevel, &lockedBox, &srcBox, D3DLOCK_NOSYSLOCK ) ) )
	{
		Warning( "BlitVolumeBits: couldn't lock volume texture rect\n" );
		return;
	}

	// garymcthack : need to make a recording command for this.
	ImageFormat dstFormat = GetImageFormat( info.m_pTexture );
	unsigned char *pImage = (unsigned char *)lockedBox.pBits;
	ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
						pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedBox.RowPitch );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	if ( FAILED( pVolumeTexture->UnlockBox( info.m_nLevel ) ) ) 
	{
		Warning( "BlitVolumeBits: couldn't unlock volume texture rect\n" );
		return;
	}
}
#endif

//-----------------------------------------------------------------------------
// Puts 3D texture data into 360 gpu memory.
// Does not support any subvolume or slice blitting.
//-----------------------------------------------------------------------------
#if defined( _X360 )
static void BlitVolumeBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	if ( xOffset || yOffset || info.m_nZOffset || srcStride )
	{
		// not supporting any subvolume blitting
		// the entire volume per mip must be blitted
		Assert( 0 );
		return;
	}

	ImageFormat	dstFormat = GetImageFormat( info.m_pTexture );
	if ( dstFormat != info.m_SrcFormat )
	{
		// texture is expected to be in target format
		// not supporting conversion
		Assert( 0 );
		return;
	}

	// get the top mip level info (needed for proper sub mip access)
	XGTEXTURE_DESC baseDesc;
	XGGetTextureDesc( info.m_pTexture, 0, &baseDesc );
	bool bDstIsTiled = XGIsTiledFormat( baseDesc.Format ) == TRUE;
	if ( info.m_bSrcIsTiled && !bDstIsTiled )
	{
		// not supporting a tiled source into an untiled target
		Assert( 0 );
		return;
	}

	// get the mip level info
	XGTEXTURE_DESC mipDesc;
	XGGetTextureDesc( info.m_pTexture, info.m_nLevel, &mipDesc );
	bool bFullSurfBlit = ( mipDesc.Width == (unsigned int)info.m_nWidth && mipDesc.Height == (unsigned int)info.m_nHeight );

	if ( !bFullSurfBlit )
	{
		// not supporting subrect blitting
		Assert( 0 );
		return;
	}

	D3DLOCKED_BOX lockedBox;

	// get the mip level of the volume we want to write into
	IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );
	HRESULT hr = pVolumeTexture->LockBox( info.m_nLevel, &lockedBox, NULL, D3DLOCK_NOSYSLOCK );
	if ( FAILED( hr ) )
	{
		Warning( "CShaderAPIDX8::BlitVolumeBits: Couldn't lock volume box\n" );
		return;
	}

	unsigned char *pSrcData = info.m_pSrcData;
	unsigned char *pTargetImage = (unsigned char *)lockedBox.pBits;

	int tileFlags = 0;
	if ( !( mipDesc.Flags & XGTDESC_PACKED ) )
		tileFlags |= XGTILE_NONPACKED;
	if ( mipDesc.Flags & XGTDESC_BORDERED )
		tileFlags |= XGTILE_BORDER;

	if ( !info.m_bSrcIsTiled && bDstIsTiled )
	{
		// tile the source directly into the target surface
		XGTileVolumeTextureLevel(
			baseDesc.Width,
			baseDesc.Height,
			baseDesc.Depth,
			info.m_nLevel,
			XGGetGpuFormat( baseDesc.Format ),
			tileFlags,
			pTargetImage,
			NULL,
			pSrcData,
			mipDesc.RowPitch,
			mipDesc.SlicePitch,
			NULL );
	}
	else if ( !info.m_bSrcIsTiled && !bDstIsTiled )
	{
		// not implemented yet
		Assert( 0 );
	}
	else
	{
		// not implemented yet
		Assert( 0 );
	}

	hr = pVolumeTexture->UnlockBox( info.m_nLevel );
	if ( FAILED( hr ) )
	{
		Warning( "CShaderAPIDX8::BlitVolumeBits: couldn't unlock volume box\n" );
		return;
	}
}
#endif

// FIXME: How do I blit from D3DPOOL_SYSTEMMEM to D3DPOOL_MANAGED?  I used to use CopyRects for this.  UpdateSurface doesn't work because it can't blit to anything besides D3DPOOL_DEFAULT.
// We use this only in the case where we need to create a < 4x4 miplevel for a compressed texture.  We end up creating a 4x4 system memory texture, and blitting it into the proper miplevel.
// 6) LockRects should be used for copying between SYSTEMMEM and
// MANAGED.  For such a small copy, you'd avoid a significant 
// amount of overhead from the old CopyRects code.  Ideally, you 
// should just lock the bottom of MANAGED and generate your 
// sub-4x4 data there.
	
// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
static void BlitTextureBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
#ifdef RECORD_TEXTURES
	RECORD_COMMAND( DX8_BLIT_TEXTURE_BITS, 14 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_INT( xOffset );
	RECORD_INT( yOffset );
	RECORD_INT( info.m_nZOffset );
	RECORD_INT( info.m_nWidth );
	RECORD_INT( info.m_nHeight );
	RECORD_INT( info.m_SrcFormat );
	RECORD_INT( srcStride );
	RECORD_INT( GetImageFormat( info.m_pTexture ) );
	// strides are in bytes.
	int srcDataSize;
	if ( srcStride == 0 )
	{
		srcDataSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, info.m_SrcFormat, false );
	}
	else
	{
		srcDataSize = srcStride * info.m_nHeight;
	}
	RECORD_INT( srcDataSize );
	RECORD_STRUCT( info.m_pSrcData, srcDataSize );
#endif // RECORD_TEXTURES
	
	if ( !IsVolumeTexture( info.m_pTexture ) )
	{
		Assert( info.m_nZOffset == 0 );
		BlitSurfaceBits( info, xOffset, yOffset, srcStride );
	}
	else
	{
		BlitVolumeBits( info, xOffset, yOffset, srcStride );
	}
}

//-----------------------------------------------------------------------------
// Texture image upload
//-----------------------------------------------------------------------------
void LoadTexture( TextureLoadInfo_t &info )
{
	MEM_ALLOC_D3D_CREDIT();

	Assert( info.m_pSrcData );
	Assert( info.m_pTexture );

#ifdef _DEBUG
	ImageFormat format = GetImageFormat( info.m_pTexture );
	Assert( (format != -1) && (format == ImageLoader::D3DFormatToImageFormat( FindNearestSupportedFormat( format, false, false, false )) ) );
#endif

	// Copy in the bits...
	BlitTextureBits( info, 0, 0, 0 );
}

//-----------------------------------------------------------------------------
// Upload to a sub-piece of a texture
//-----------------------------------------------------------------------------
void LoadSubTexture( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	Assert( info.m_pSrcData );
	Assert( info.m_pTexture );

#if defined( _X360 )
	// xboxissue - not supporting subrect swizzling
	Assert( !info.m_bSrcIsTiled );
#endif

#ifdef _DEBUG
	ImageFormat format = GetImageFormat( info.m_pTexture );
	Assert( (format == ImageLoader::D3DFormatToImageFormat( FindNearestSupportedFormat(format, false, false, false )) ) && (format != -1) );
#endif

	// Copy in the bits...
	BlitTextureBits( info, xOffset, yOffset, srcStride );
}


//-----------------------------------------------------------------------------
// Returns the size of texture memory, in MB
//-----------------------------------------------------------------------------
// Helps with startup time.. we don't use the texture memory size for anything anyways
#define DONT_CHECK_MEM

int ComputeTextureMemorySize( const GUID &nDeviceGUID, D3DDEVTYPE deviceType )
{
#if !defined( _X360 )
	FileHandle_t file = g_pFullFileSystem->Open( "vidcfg.bin", "rb", "EXECUTABLE_PATH" );
	if ( file )
	{
		GUID deviceId;
		int texSize;
		g_pFullFileSystem->Read( &deviceId, sizeof(deviceId), file );
		g_pFullFileSystem->Read( &texSize, sizeof(texSize), file );
		g_pFullFileSystem->Close( file );
		if ( nDeviceGUID == deviceId )
		{
			return texSize;
		}
	}
	// How much texture memory?
	if (deviceType == D3DDEVTYPE_REF)
		return 64 * 1024 * 1024;

	// Sadly, the only way to compute texture memory size
	// is to allocate a crapload of textures until we can't any more
	ImageFormat fmt = ImageLoader::D3DFormatToImageFormat( FindNearestSupportedFormat( IMAGE_FORMAT_BGR565, false, false, false ) );
	int textureSize = ShaderUtil()->GetMemRequired( 256, 256, 1, fmt, false );

	int totalSize = 0;
	CUtlVector< IDirect3DBaseTexture* > textures;

#ifndef DONT_CHECK_MEM
	while (true)
	{
		RECORD_COMMAND( DX8_CREATE_TEXTURE, 7 );
		RECORD_INT( textures.Count() );
		RECORD_INT( 256 );
		RECORD_INT( 256 );
		RECORD_INT( ImageLoader::ImageFormatToD3DFormat(fmt) );
		RECORD_INT( 1 );
		RECORD_INT( false );
		RECORD_INT( 1 );

		IDirect3DBaseTexture* pTex = CreateD3DTexture( 256, 256, 1, fmt, 1, 0 );
		if (!pTex)
			break;
		totalSize += textureSize;

		textures.AddToTail( pTex );
	} 

	// Free all the temp textures
	for (int i = textures.Size(); --i >= 0; )
	{
		RECORD_COMMAND( DX8_DESTROY_TEXTURE, 1 );
		RECORD_INT( i );

		DestroyD3DTexture( textures[i] );
	}
#else
	totalSize = 102236160;
#endif
	file = g_pFullFileSystem->Open( "vidcfg.bin", "wb", "EXECUTABLE_PATH" );
	if ( file )
	{
		g_pFullFileSystem->Write( &nDeviceGUID, sizeof(GUID), file );
		g_pFullFileSystem->Write( &totalSize, sizeof(totalSize), file );
		g_pFullFileSystem->Close( file );
	}

	return totalSize;
#else
	return 0;
#endif
}

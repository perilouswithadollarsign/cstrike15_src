//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"

#include "utlmap.h"
#include "ps3gcmmemory.h"
#include "gcmstate.h"
#include "bitmap/imageformat_declarations.h"
#include "gcmtexture.h"

#include "memdbgon.h"

#ifdef _CERT
#define Debugger() ((void)0)
#else
#define Debugger() DebuggerBreak()
#endif

//--------------------------------------------------------------------------------------------------
// Texture Layouts
//--------------------------------------------------------------------------------------------------

#ifdef _CERT
#define GLMTEX_FMT_DESC( x )
#else
#define GLMTEX_FMT_DESC( x ) x ,
#endif

#define CELL_GCM_REMAP_MODE_OIO(order, inputARGB, outputARGB) \
	(((order)<<16)|((inputARGB))|((outputARGB)<<8))
#define REMAPO( x ) CELL_GCM_TEXTURE_REMAP_ORDER_X##x##XY
#define REMAP4(a,r,g,b) (((a)<<0)|((r)<<2)|((g)<<4)|((b)<<6))

#define REMAP_ARGB  REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G, CELL_GCM_TEXTURE_REMAP_FROM_B )
#define REMAP_4		REMAP4( CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP )
#define REMAP_13	REMAP4( CELL_GCM_TEXTURE_REMAP_ONE, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP )
#define REMAP_4X(x)	REMAP4( x, x, x, x )
#define REMAP_13X(y, x)	REMAP4( y, x, x, x )

#define REMAP_ALL_DEFAULT CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_ARGB, REMAP_4 )
#define REMAP_ALL_DEFAULT_X CELL_GCM_REMAP_MODE_OIO( REMAPO(X), REMAP_ARGB, REMAP_4 )

#define CAP( x ) CPs3gcmTextureLayout::Format_t::kCap##x

CPs3gcmTextureLayout::Format_t g_ps3texFormats[PS3_TEX_MAX_FORMAT_COUNT] = 
{
	// summ-name						d3d-format
	//			gcmRemap
	//									gcmFormat
	//			gcmPitchPer4X			gcmFlags

	{ GLMTEX_FMT_DESC("_D16")			D3DFMT_D16,
	REMAP_ALL_DEFAULT,
	8,
	CELL_GCM_TEXTURE_DEPTH16,
	0 },

	{ GLMTEX_FMT_DESC("_D24X8")			D3DFMT_D24X8,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_DEPTH24_D8,
	0 },

	{ GLMTEX_FMT_DESC("_D24S8")			D3DFMT_D24S8,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_DEPTH24_D8,
	0 },

	{ GLMTEX_FMT_DESC("_A8R8G8B8")		D3DFMT_A8R8G8B8,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_A8R8G8B8,
	CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_X8R8G8B8")		D3DFMT_X8R8G8B8,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_A8R8G8B8,
	CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_X1R5G5B5")		D3DFMT_X1R5G5B5,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(X), REMAP_ARGB, REMAP_13 ),
	8,
	CELL_GCM_TEXTURE_R5G6B5,
	0 },

	{ GLMTEX_FMT_DESC("_A1R5G5B5")		D3DFMT_A1R5G5B5,
	REMAP_ALL_DEFAULT_X,
	8,
	CELL_GCM_TEXTURE_A1R5G5B5,
	0 },

	{ GLMTEX_FMT_DESC("_L8")			D3DFMT_L8,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_4X(CELL_GCM_TEXTURE_REMAP_FROM_B), REMAP_13 ),
	4,
	CELL_GCM_TEXTURE_B8,
	0 },

	{ GLMTEX_FMT_DESC("_A8L8")			D3DFMT_A8L8,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_13X( CELL_GCM_TEXTURE_REMAP_FROM_G, CELL_GCM_TEXTURE_REMAP_FROM_B), REMAP_4 ),
	8,
	CELL_GCM_TEXTURE_G8B8,
	0 },

	{ GLMTEX_FMT_DESC("_DXT1")			D3DFMT_DXT1,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_ARGB, REMAP_13 ),
	8,
	CELL_GCM_TEXTURE_COMPRESSED_DXT1,
	CAP(SRGB) | CAP(4xBlocks) },

	{ GLMTEX_FMT_DESC("_DXT3")			D3DFMT_DXT3,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_COMPRESSED_DXT23,
	CAP(SRGB) | CAP(4xBlocks) },

	{ GLMTEX_FMT_DESC("_DXT5")			D3DFMT_DXT5,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_COMPRESSED_DXT45,
	CAP(SRGB) | CAP(4xBlocks) },

	{ GLMTEX_FMT_DESC("_A16B16G16R16F")	D3DFMT_A16B16G16R16F,
	REMAP_ALL_DEFAULT_X,
	32,
	CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT,
	0 },

	{ GLMTEX_FMT_DESC("_A16B16G16R16")	D3DFMT_A16B16G16R16,
	REMAP_ALL_DEFAULT_X,
	64,
	CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT,
	0 },

	{ GLMTEX_FMT_DESC("_A32B32G32R32F")	D3DFMT_A32B32G32R32F,
	REMAP_ALL_DEFAULT_X,
	64,
	CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT,
	0 },

	{ GLMTEX_FMT_DESC("_R8G8B8")		D3DFMT_R8G8B8,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(Y),
	REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G ),
	REMAP_13 ),
	16,
	CELL_GCM_TEXTURE_A8R8G8B8,
	CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_A8")			D3DFMT_A8,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(Y),
	REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_B ),
	REMAP_13X( CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_ZERO ) ),
	4,
	CELL_GCM_TEXTURE_B8,
	0 },

	{ GLMTEX_FMT_DESC("_R5G6B5")		D3DFMT_R5G6B5,
	CELL_GCM_REMAP_MODE_OIO( REMAPO(Y),
	REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G ),
	REMAP_13 ),
	16,
	CELL_GCM_TEXTURE_A8R8G8B8,
	CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_Q8W8V8U8")		D3DFMT_Q8W8V8U8,
	REMAP_ALL_DEFAULT,
	16,
	CELL_GCM_TEXTURE_A8R8G8B8,
	CAP(SRGB) },
};

uint g_nPs3texFormatCount = PS3_TEX_CANONICAL_FORMAT_COUNT;

#undef CAP
#undef GLMTEX_FMT_DESC

static bool Ps3texLayoutLessFunc( CPs3gcmTextureLayout::Key_t const &a, CPs3gcmTextureLayout::Key_t const &b )
{
	return ( memcmp( &a, &b, sizeof( CPs3gcmTextureLayout::Key_t ) ) < 0 );
}
static CUtlMap< CPs3gcmTextureLayout::Key_t, CPs3gcmTextureLayout const * > s_ps3texLayouts( Ps3texLayoutLessFunc );

CPs3gcmTextureLayout const * CPs3gcmTextureLayout::New( Key_t const &k )
{
	// look up 'key' in the map and see if it's a hit, if so, bump the refcount and return
	// if not, generate a completed layout based on the key, add to map, set refcount to 1, return that
	unsigned short index = s_ps3texLayouts.Find( k );
	if ( index != s_ps3texLayouts.InvalidIndex() )
	{
		CPs3gcmTextureLayout const *layout = s_ps3texLayouts[ index ];
		++ layout->m_refCount;
		return layout;
	}

	// Need to generate complete information about the texture layout
	uint8 nMips = ( k.m_texFlags & kfMip ) ? k.m_nActualMipCount : 1;
	uint8 nFaces = ( k.m_texFlags & kfTypeCubeMap ) ? 6 : 1;
	uint32 nSlices = nMips * nFaces;

	// Allocate layout memory
	size_t numLayoutBytes = sizeof( CPs3gcmTextureLayout ) + nSlices * sizeof( Slice_t );
	CPs3gcmTextureLayout *layout = ( CPs3gcmTextureLayout * ) MemAlloc_AllocAligned( numLayoutBytes, 16 );
	memset( layout, 0, numLayoutBytes );
	memcpy( &layout->m_key, &k, sizeof( Key_t ) );
	layout->m_refCount = 1;

	// Find the format descriptor
	for ( int j = 0; j < PS3_TEX_CANONICAL_FORMAT_COUNT; ++ j )
	{
		if ( g_ps3texFormats[j].m_d3dFormat == k.m_texFormat )
		{
			layout->m_nFormat = j;
			break;
		}
		Assert( j != PS3_TEX_CANONICAL_FORMAT_COUNT - 1 );
	}

	layout->m_mipCount = nMips;

	//
	// Slices
	//
	bool bSwizzled = layout->IsSwizzled();
	size_t fmtPitch = layout->GetFormatPtr()->m_gcmPitchPer4X;
	size_t fmtPitchBlock = ( layout->GetFormatPtr()->m_gcmCaps & CPs3gcmTextureLayout::Format_t::kCap4xBlocks ) ? 16 : 4;
	size_t numDataBytes = 0;
	Slice_t *pSlice = &layout->m_slices[0];
	for ( int face = 0; face < nFaces; ++ face )
	{
		// For cubemaps every next face in swizzled addressing
		// must be aligned on 128-byte boundary
		if ( bSwizzled )
		{
			numDataBytes = ( numDataBytes + 127 ) & ~127;
		}

		for ( int mip = 0; mip < nMips; ++ mip, ++ pSlice )
		{
			for ( int j = 0; j < ARRAYSIZE( k.m_size ); ++ j )
			{
				pSlice->m_size[j] = k.m_size[j] >> mip;
				pSlice->m_size[j] = MAX( pSlice->m_size[j], 1 );
			}
			pSlice->m_storageOffset = numDataBytes;

			size_t numTexels;
			// For linear layout textures every mip row must be padded to the
			// width of the original highest level mip so that the pitch was
			// the same for every mip
			if ( bSwizzled )
				numTexels = ( pSlice->m_size[0] * pSlice->m_size[1] * pSlice->m_size[2] );
			else
				numTexels = ( k.m_size[0] * pSlice->m_size[1] * pSlice->m_size[2] );

			size_t numBytes = ( numTexels * fmtPitch ) / fmtPitchBlock;

			if ( layout->GetFormatPtr()->m_gcmCaps & CPs3gcmTextureLayout::Format_t::kCap4xBlocks )
			{
				// Ensure the size of the smallest mipmap levels of DXT1/3/5 textures (the 1x1 and 2x2 mips) is accurately computed.
				numBytes = MAX( numBytes, fmtPitch );
			}

			pSlice->m_storageSize = MAX( numBytes, 1 );

			numDataBytes += pSlice->m_storageSize;
		}
	}
	// Make the total size 128-byte aligned
	// Realistically it is required only for depth textures
	numDataBytes = ( numDataBytes + 127 ) & ~127;

	//
	// Tiled and ZCull memory adjustments
	//
	layout->m_gcmAllocType = GCM_MAINPOOLSIZE ? kAllocPs3gcmTextureData0 : kAllocPs3gcmTextureData;


	if ( layout->IsTiledMemory() )
	{
		if( g_nPs3texFormatCount >= PS3_TEX_MAX_FORMAT_COUNT )
		{
			Error("Modified ps3 format array overflow. Increase PS3_TEX_MAX_FORMAT_COUNT appropriately and recompile\n");
		}
		Format_t *pModifiedFormat = &g_ps3texFormats[g_nPs3texFormatCount];
		V_memcpy( pModifiedFormat, layout->GetFormatPtr(), sizeof( Format_t ) );
		layout->m_nFormat = g_nPs3texFormatCount;
		g_nPs3texFormatCount ++;

		if ( k.m_texFlags & kfTypeDepthStencil )
		{
			//
			// Tiled Zcull Surface
			//
			uint32 zcullSize[2] = { AlignValue( k.m_size[0], 64 ), AlignValue( k.m_size[1], 64 ) };
			uint32 nDepthPitch;
							
			if ( k.m_texFormat == D3DFMT_D16 ) 
				nDepthPitch = cellGcmGetTiledPitchSize( zcullSize[0] * 2 );
			else
				nDepthPitch = cellGcmGetTiledPitchSize( zcullSize[0] * 4 );
			
			pModifiedFormat->m_gcmPitchPer4X = nDepthPitch;

			uint32 uDepthBufferSize32bpp = nDepthPitch * zcullSize[1];
			uDepthBufferSize32bpp = AlignValue( uDepthBufferSize32bpp, PS3GCMALLOCATIONALIGN( kAllocPs3gcmDepthBuffer ) );

			Assert( uDepthBufferSize32bpp >= numDataBytes );
			numDataBytes = uDepthBufferSize32bpp;

			layout->m_gcmAllocType = kAllocPs3gcmDepthBuffer;
		}
		else
		{
			//
			// Tiled Color Surface
			//
			uint32 nTiledPitch = cellGcmGetTiledPitchSize( k.m_size[0] * layout->GetFormatPtr()->m_gcmPitchPer4X / 4 );
			pModifiedFormat->m_gcmPitchPer4X = nTiledPitch;

            // We Don't allocate any 512x512 RTs (they are used only when in PAL576i which can use the FB mem pool)
			/*if ( k.m_size[0] == 512 && k.m_size[1] == 512 && k.m_size[2] == 1 )
				layout->m_gcmAllocType = kAllocPs3gcmColorBuffer512;
			else*/ 
            
            if ( k.m_size[0] == g_ps3gcmGlobalState.m_nRenderSize[0] && k.m_size[1] == g_ps3gcmGlobalState.m_nRenderSize[1] && k.m_size[2] == 1 )
				layout->m_gcmAllocType = kAllocPs3gcmColorBufferFB;
			else if ( k.m_size[0] == g_ps3gcmGlobalState.m_nRenderSize[0]/4 && k.m_size[1] == g_ps3gcmGlobalState.m_nRenderSize[1]/4 && k.m_size[2] == 1 )
				layout->m_gcmAllocType = kAllocPs3gcmColorBufferFBQ;
			else
				layout->m_gcmAllocType = kAllocPs3gcmColorBufferMisc;

			uint32 uRenderSize = nTiledPitch * AlignValue( k.m_size[1], 32 ); // 32-line vertical alignment required in local memory
			if ( layout->m_gcmAllocType == kAllocPs3gcmColorBufferMisc )
				uRenderSize = AlignValue( uRenderSize, PS3GCMALLOCATIONALIGN( kAllocPs3gcmColorBufferMisc ) );

			Assert( uRenderSize >= numDataBytes );
			numDataBytes = uRenderSize;
		}
	}

	layout->m_storageTotalSize = numDataBytes;

	//
	// Finished creating the layout information
	//

#ifndef _CERT
	// generate summary
	// "target, format, +/- mips, base size"
	char scratch[1024];

	char	*targetname = targetname = "2D  ";
	if ( layout->IsVolumeTex() )
		targetname = "3D  ";
	if ( layout->IsCubeMap() )
		targetname = "CUBE";

	sprintf( scratch, "[%s %s %dx%dx%d mips=%d slices=%d flags=%02X%s]",
		targetname,
		layout->GetFormatPtr()->m_formatSummary,
		layout->m_key.m_size[0], layout->m_key.m_size[1], layout->m_key.m_size[2],
		nMips,
		nSlices,
		layout->m_key.m_texFlags,
		(layout->m_key.m_texFlags & kfSrgbEnabled) ? " SRGB" : ""
		);
	layout->m_layoutSummary = strdup( scratch );
#endif

	// then insert into map. disregard returned index.
	s_ps3texLayouts.Insert( k, layout );

	return layout;
}

void CPs3gcmTextureLayout::Release() const
{
	-- m_refCount;
	// keep the layout in the map for easy access
	Assert( m_refCount >= 0 );
}

//////////////////////////////////////////////////////////////////////////
//
// Texture management
//

CPs3gcmTexture * CPs3gcmTexture::New( CPs3gcmTextureLayout::Key_t const &key )
{
	//
	// Allocate a new layout for the texture
	//
	CPs3gcmTextureLayout const *pLayout = CPs3gcmTextureLayout::New( key );
	if ( !pLayout )
	{
		Debugger();
		return NULL;
	}

	CPs3gcmTexture *tex = (CPs3gcmTexture *)MemAlloc_AllocAligned( sizeof( CPs3gcmTexture ), 16 );
	memset( tex, 0, sizeof( CPs3gcmTexture ) ); // NOTE: This clears the CPs3gcmLocalMemoryBlock

	tex->m_layout = pLayout;
	CPs3gcmAllocationType_t uAllocationType = pLayout->m_gcmAllocType;

	if ( key.m_texFlags & CPs3gcmTextureLayout::kfNoD3DMemory )
	{
		if ( ( uAllocationType == kAllocPs3gcmDepthBuffer ) || ( uAllocationType == kAllocPs3gcmColorBufferMisc ) )
		{
			Assert( 0 );
			Warning( "ERROR: (CPs3gcmTexture::New) depth/colour buffers should not be marked with kfNoD3DMemory!\n" );
		}
		else
		{
			// Early-out, storage will be allocated later (via IDirect3DDevice9::AllocateTextureStorage)
			return tex;
		}
	}

	tex->Allocate();

	return tex;
}

void CPs3gcmTexture::Release()
{
	// Wait for RSX to finish using the texture memory
	// and free it later
	if ( m_lmBlock.Size() )
	{
		m_lmBlock.Free();
	}
	m_layout->Release();
	MemAlloc_FreeAligned( this );
}

bool CPs3gcmTexture::Allocate()
{
	if ( m_lmBlock.Size() )
	{
		// Already allocated!
		Assert( 0 );
		Warning( "ERROR: CPs3gcmTexture::Allocate called twice!\n" );
		return true;
	}

	CPs3gcmAllocationType_t uAllocationType = m_layout->m_gcmAllocType;
	const CPs3gcmTextureLayout::Key_t & key = m_layout->m_key;

	// if kAllocPs3gcmTextureData0 (main memory) fails try kAllocPs3gcmTextureData

	if (!m_lmBlock.Alloc( uAllocationType, m_layout->m_storageTotalSize ) )
	{
		if (m_layout->m_gcmAllocType == kAllocPs3gcmTextureData0)
		{
			m_layout->m_gcmAllocType = kAllocPs3gcmTextureData;
			CPs3gcmAllocationType_t uAllocationType = m_layout->m_gcmAllocType;
			m_lmBlock.Alloc( uAllocationType, m_layout->m_storageTotalSize );
		}
	}

	if ( m_layout->IsTiledMemory() )
	{
		if ( uAllocationType == kAllocPs3gcmDepthBuffer )
		{
			bool bIs16BitDepth = ( m_layout->GetFormatPtr()->m_gcmFormat == CELL_GCM_TEXTURE_DEPTH16 ) || ( m_layout->m_nFormat == CELL_GCM_TEXTURE_DEPTH16_FLOAT );

			uint32 zcullSize[2] = { AlignValue( key.m_size[0], 64 ), AlignValue( key.m_size[1], 64 ) };

			uint32 uiZcullIndex = m_lmBlock.ZcullMemoryIndex();
			cellGcmBindZcull( uiZcullIndex,
				m_lmBlock.Offset(),
				zcullSize[0], zcullSize[1],
				m_lmBlock.ZcullMemoryStart(),
				bIs16BitDepth ? CELL_GCM_ZCULL_Z16 : CELL_GCM_ZCULL_Z24S8,
				CELL_GCM_SURFACE_CENTER_1,
				CELL_GCM_ZCULL_LESS,
				CELL_GCM_ZCULL_LONES,
				CELL_GCM_SCULL_SFUNC_ALWAYS,
				0, 0	// sRef, sMask
				);

			uint32 uiTileIndex = m_lmBlock.TiledMemoryIndex();
			cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL, m_lmBlock.Offset(),
				m_layout->m_storageTotalSize, m_layout->DefaultPitch(), bIs16BitDepth ? CELL_GCM_COMPMODE_DISABLED : CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR,
				m_lmBlock.TiledMemoryTagAreaBase(), // The area base + size/0x10000 will be allocated as the tag area.
				1 );	// Misc depth buffers on bank 1
			cellGcmBindTile( uiTileIndex );
		}
		else if ( uAllocationType == kAllocPs3gcmColorBufferMisc )
		{
			uint32 uiTileIndex = m_lmBlock.TiledMemoryIndex();
			cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL, m_lmBlock.Offset(),
				m_layout->m_storageTotalSize, m_layout->DefaultPitch(), CELL_GCM_COMPMODE_DISABLED,
				m_lmBlock.TiledMemoryTagAreaBase(), // The area base + size/0x10000 will be allocated as the tag area.
				1 );	// Tile misc color buffers on bank 1
			cellGcmBindTile( uiTileIndex );
		}
	}

#ifdef _DEBUG
	memset( Data(), 0, m_layout->m_storageTotalSize );	// initialize texture data to BLACK in DEBUG
#endif

	return true;
}


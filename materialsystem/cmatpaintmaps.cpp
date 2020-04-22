//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif

#include "cmaterialsystem.h"
#include "IHardwareConfigInternal.h"
#include "cmatpaintmaps.h"
#include "cmatlightmaps.h"
#include "materialsystem_global.h"
#include "materialsystem/materialsystem_config.h"
#include "itextureinternal.h"

// src/public/
#include "game/shared/portal2/paint_enum.h"

static Color g_PaintColors[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER];
static void RegisterPaintColors()
{

#if defined ( PORTAL2 )

	// These ConVar are defined in src/game/shared/portal/paint_color_manager.cpp
	static ConVarRef speed_paint_color( "speed_paint_color" );
	static ConVarRef bounce_paint_color( "bounce_paint_color" );
	static ConVarRef reflect_paint_color( "reflect_paint_color" );
	static ConVarRef portal_paint_color( "portal_paint_color" );
	static ConVarRef erase_color( "erase_color" );

	g_PaintColors[SPEED_POWER]		= speed_paint_color.GetColor();
	g_PaintColors[BOUNCE_POWER]		= bounce_paint_color.GetColor();
	g_PaintColors[PORTAL_POWER]		= portal_paint_color.GetColor();
	g_PaintColors[REFLECT_POWER]	= reflect_paint_color.GetColor();
	g_PaintColors[NO_POWER]			= erase_color.GetColor();

#endif

}

static const BYTE NUM_ALPHA_BITS = 5;
static const BYTE PAINT_COLOR_BITS = 7 << NUM_ALPHA_BITS; // 224
static const BYTE PAINT_ALPHA_BITS	= PAINT_COLOR_BITS ^ 0xFF; // 31

BYTE GetColorIndex( BYTE byte )
{
	return ( PAINT_COLOR_BITS & byte ) >> NUM_ALPHA_BITS;
}

inline const Color &GetColor( BYTE byte )
{
	BYTE colorIndex = GetColorIndex( byte );
	return g_PaintColors[ colorIndex ];
}

inline float GetAlpha( BYTE byte )
{
	double alpha = ( PAINT_ALPHA_BITS & byte ); // leave as double to postpone the frsp...
	alpha /= PAINT_ALPHA_BITS;
	alpha = clamp( alpha, 0.0, 1.0 ); 
	return alpha; // ...to here
}


CMatPaintmaps::CMatPaintmaps( void )
{
	m_pDataManager = NULL;
	m_nUpdatingPaintmapsStackDepth = 0;
	m_nLockedPaintmap = -1;
}

bool CMatPaintmaps::IsEnabled( void )
{
	return (m_pDataManager != NULL);
}


void CMatPaintmaps::RegisterPaintmapDataManager( IPaintmapDataManager *pDataManager )
{
	m_pDataManager = pDataManager;
	RegisterPaintColors();
}


void CMatPaintmaps::BeginPaintTextureAllocation( int iPaintmapCount )
{
	CleanupPaintmaps();

	m_pDataManager->BeginPaintmapsDataAllocation( iPaintmapCount );
}

void CMatPaintmaps::EndPaintTextureAllocation( void )
{
	//do we need to do anything?
}

void CMatPaintmaps::ReleasePaintmaps( void )
{
	// clean up paint textures, leave the paint data alone
	for( int i = 0; i < m_PaintmapTextureHandles.Count(); i++ )
	{
		g_pShaderAPI->DeleteTexture( m_PaintmapTextureHandles[i] );
	}
	m_PaintmapTextureHandles.RemoveAll();
}

void CMatPaintmaps::RestorePaintmaps( int nNumLightmaps )
{
	Assert( m_PaintmapTextureHandles.Count() == 0 );

	// reallocate + update paint textures
	for ( int i=0; i<nNumLightmaps; ++i )
	{
		int width, height;
		m_pDataManager->GetPaintmapSize( i, width, height );
		AllocatePaintmapTexture( i, width, height );
	}

	m_pDataManager->OnRestorePaintmaps();
}

void CMatPaintmaps::CleanupPaintmaps( void )
{
	for( int i = 0; i < m_PaintmapTextureHandles.Count(); i++ )
	{
		g_pShaderAPI->DeleteTexture( m_PaintmapTextureHandles[i] );
	}
	m_PaintmapTextureHandles.RemoveAll();

	if( m_pDataManager )
	{
		m_pDataManager->DestroyPaintmapsData();
	}
}

ShaderAPITextureHandle_t CMatPaintmaps::GetPaintmapPageTextureHandle( int paintmap )
{
	//test new texture
	if( paintmap >= 0 && paintmap < m_PaintmapTextureHandles.Count() )
	{
		return m_PaintmapTextureHandles[paintmap];
	}
	return INVALID_SHADERAPI_TEXTURE_HANDLE;
}


void CMatPaintmaps::BeginUpdatePaintmaps()
{
	CMatCallQueue *pCallQueue = GetMaterialSystem()->GetRenderContextInternal()->GetCallQueueInternal();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMatPaintmaps::BeginUpdatePaintmaps );
		return;
	}

	m_nUpdatingPaintmapsStackDepth++;
}


void CMatPaintmaps::FillRect( int paintmap, Rect_t* RESTRICT pRect, BYTE* RESTRICT pPaintData, Rect_t* RESTRICT pSubRect /*= NULL*/ ) RESTRICT 
{
	VPROF("CMatPaintmaps::FillRect");

	int width, height;
	m_pDataManager->GetPaintmapSize( paintmap, width, height );

	// trap corrupted paint rect
	// TODO: Server writing to and Client reading from rect list in paint.cpp in the engine is not threadsafe
	if ( pRect->x < 0 || pRect->y < 0 || pRect->x >= width || pRect->y >= height ||
		 pRect->width < 0 || pRect->height < 0 || pRect->width > width || pRect->height > height ||
		 pRect->x + pRect->width > width || pRect->y + pRect->height > height )
	{
		DevWarning( "Corrupted paint rect\n" );
		return;
	}

	int rectOffset[2] = { pRect->x, pRect->y };
	if ( pSubRect )
	{
		rectOffset[0] -= pSubRect->x;
		rectOffset[1] -= pSubRect->y;
	}


	int index;
	for( int t = 0; t < pRect->height; ++t )
	{
		m_PaintmapPixelWriter.Seek( rectOffset[0], rectOffset[1] + t );

		index = ( t + pRect->y ) * width;
		for( int s = 0; s < pRect->width; ++s )
		{
			BYTE paintData = pPaintData[index + s + pRect->x];
			const Color &color = GetColor( paintData );
			float alpha = GetAlpha( paintData );
#ifndef _PS3
			m_PaintmapPixelWriter.WritePixel( color.r(), color.g(), color.b(), alpha * 255 );
#else // is PS3
			Assert( m_PaintmapPixelWriter.IsUsing16BitFloatFormat() );
			m_PaintmapPixelWriter.WritePixelNoAdvance16F( 
				(float)color.r() / 255.0f, 
				(float)color.g() / 255.0f, 
				(float)color.b() / 255.0f,
				alpha );
			m_PaintmapPixelWriter.SkipPixels(1);
				
#endif
		}
	}
}


void CMatPaintmaps::UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects )
{
	VPROF("CMatPaintmaps::UpdatePaintmap");
	if ( paintmap >= GetMaterialSystem()->GetNumLightmapPages() || paintmap < 0 )
	{
		Error( "CMatPaintmaps::UpdatePaintmap paintmap=%d out of range\n", paintmap );
		return;
	}

	bool bLockSubRect = pRects != NULL;

	Rect_t rect;
	if ( bLockSubRect )
	{
		int minX = 512;
		int minY = 512;
		int maxX = 0;
		int maxY = 0;

		// find min/max rect
		for ( int i=0; i<numRects; ++i )
		{
			Rect_t* pCurrentRect = &pRects[i];
			minX = MIN( pCurrentRect->x, minX );
			minY = MIN( pCurrentRect->y, minY );
			maxX = MAX( pCurrentRect->x + pCurrentRect->width, maxX );
			maxY = MAX( pCurrentRect->y + pCurrentRect->height, maxY );
		}

		rect.x = minX;
		rect.y = minY;
		rect.width = maxX - minX;
		rect.height = maxY - minY;
	}
	else
	{
		rect.x = rect.y = 0;
		m_pDataManager->GetPaintmapSize( paintmap, rect.width, rect.height );
	}

	if ( bLockSubRect )
	{
		g_pShaderAPI->ModifyTexture( m_PaintmapTextureHandles[paintmap] );
		if ( !g_pShaderAPI->TexLock( 0, 0, rect.x, rect.y, rect.width, rect.height, m_PaintmapPixelWriter ) )
		{
			Assert("Failed to lock paint texture!");
			return;
		}
	}
	else
	{
		if ( !LockPaintmap( paintmap ) )
		{
			Assert("Failed to lock paint texture!");
			return;
		}
	}

	// modify texture!	
	if ( pRects )
	{
		for ( int i=0; i<numRects; ++i )
		{
			FillRect( paintmap, &pRects[i], pPaintData, &rect );
		}
	}
	else
	{
		FillRect( paintmap, &rect, pPaintData );
	}
	

	if ( bLockSubRect )
	{
		g_pShaderAPI->TexUnlock();
	}
}


void CMatPaintmaps::EndUpdatePaintmaps()
{
	CMatCallQueue *pCallQueue = GetMaterialSystem()->GetRenderContextInternal()->GetCallQueueInternal();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMatPaintmaps::EndUpdatePaintmaps );
		return;
	}

	m_nUpdatingPaintmapsStackDepth--;
	Assert( m_nUpdatingPaintmapsStackDepth >= 0 );
	if( m_nUpdatingPaintmapsStackDepth <= 0 && m_nLockedPaintmap != -1 )
	{
		g_pShaderAPI->TexUnlock();
		m_nLockedPaintmap = -1;
	}
}

ConVar mat_dynamicPaintmaps( "mat_dynamicPaintmaps", "0", FCVAR_CHEAT );
void CMatPaintmaps::AllocatePaintmapTexture( int paintmap, int iWidth, int iHeight )
{
	// allocate paint texture
	int flags = 0;
	bool bUseDynamicTextures = HardwareConfig()->PreferDynamicTextures() && mat_dynamicPaintmaps.GetBool();
	if ( bUseDynamicTextures || IsPS3() ) // On PS3, we need the dynamic flag as a hint that we're going to update this texture incrementally in the future
	{
		flags |= TEXTURE_CREATE_DYNAMIC;
	}
	else
	{
		flags |= TEXTURE_CREATE_MANAGED;
	}

	//flags |= TEXTUREFLAGS_PROCEDURAL;
	m_PaintmapTextureHandles.EnsureCount( paintmap + 1 );

	char debugName[256];
	Q_snprintf( debugName, sizeof( debugName ), "[paintmap %d]", paintmap );

	ImageFormat imageFormat;
#if !defined( _X360 )
	imageFormat = IMAGE_FORMAT_RGBA8888;
#else
	imageFormat = IMAGE_FORMAT_LINEAR_RGBA8888;
#endif

#ifdef _PS3
	// PS3 needs 16F textures...but the HDR_TYPE_FLOAT codepath has a lot of other baggage with it.  Just lie here.
	imageFormat = IMAGE_FORMAT_RGBA16161616F;
#endif // _PS3

	m_PaintmapTextureHandles[paintmap] = g_pShaderAPI->CreateTexture( 
		iWidth, iHeight, 1,
		imageFormat, 
		1, 1, flags, debugName, TEXTURE_GROUP_LIGHTMAP );	// don't mipmap Paintmaps

	Assert( m_PaintmapTextureHandles[paintmap] != INVALID_SHADERAPI_TEXTURE_HANDLE );

	// Load up the texture data
	g_pShaderAPI->ModifyTexture( m_PaintmapTextureHandles[paintmap] );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );

	// Blat out the paintmap bits
	InitPaintmapBits( paintmap );
}


void CMatPaintmaps::AllocatePaintmap( int paintmap, int iWidth, int iHeight )
{
	if( !IsEnabled() )
		return;

	// allocate paint data
	m_pDataManager->AllocatePaintmapData( paintmap, iWidth, iHeight );

	// allocate paint texture
	AllocatePaintmapTexture( paintmap, iWidth, iHeight );
}


void CMatPaintmaps::InitPaintmapBits( int paintmap )
{
	CPixelWriter writer;

	int width, height;
	m_pDataManager->GetPaintmapSize( paintmap, width, height );

	g_pShaderAPI->ModifyTexture( m_PaintmapTextureHandles[paintmap] );
	if ( !g_pShaderAPI->TexLock( 0, 0, 0, 0, width, height, writer ) )
		return;

	// This always needs to be initialized fully to black
	void *pBits = writer.GetPixelMemory();
	memset( pBits, 0, width * height * writer.GetPixelSize() );
	g_pShaderAPI->TexUnlock();
}


bool CMatPaintmaps::LockPaintmap( int paintmap )
{
	if( m_nLockedPaintmap != -1 )
	{
		g_pShaderAPI->TexUnlock();
	}

	g_pShaderAPI->ModifyTexture( m_PaintmapTextureHandles[paintmap] );
	int width, height;
	m_pDataManager->GetPaintmapSize( paintmap, width, height );

	if ( !g_pShaderAPI->TexLock( 0, 0, 0, 0, width, height, m_PaintmapPixelWriter ) )
	{
		Assert( 0 );
		return false;
	}

	m_nLockedPaintmap = paintmap;
	return true;
}

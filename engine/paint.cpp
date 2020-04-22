//========= Copyright  Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//
#include "paint.h"
#include "materialsystem/imaterial.h"
#include "pixelwriter.h"
#include "gl_model_private.h"
#include "gl_matsysiface.h"
#include "dt_common.h"
#include "keyvalues.h"
#include <vstdlib/random.h> // RandomFloat
#include "collisionutils.h" // ray triangle test
#include "cmodel_private.h"
// debug stuff
#include "debugoverlay.h"
#include "con_nprint.h"

// src/public/
#include "game/shared/portal2/paint_enum.h"

#include "tier1/callqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar r_redownloadallpaintmaps;

static const BYTE NUM_ALPHA_BITS	= 5;
static const BYTE PAINT_COLOR_BITS	= 7 << NUM_ALPHA_BITS; // 224
static const BYTE PAINT_ALPHA_BITS	= PAINT_COLOR_BITS ^ 0xFF; // 31

ConVar paint_max_surface_border_alpha("paint_max_surface_border_alpha", "0.7f", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );

ConVar paint_alpha_offset_enabled("paint_alpha_offset_enabled", "1", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );

ConVar paintsplat_bias("paintsplat_bias", "0.1f", FCVAR_REPLICATED | FCVAR_CHEAT, "Change bias value for computing circle buffer" );

ConVar paintsplat_noise_enabled("paintsplat_noise_enabled", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintsplat_max_alpha_noise("paintsplat_max_alpha_noise", "0.1f", FCVAR_REPLICATED | FCVAR_CHEAT, "Max noise value of circle alpha" );

ConVar paint_min_valid_alpha_value("paint_min_valid_alpha_value", "0.7f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

ConVar debug_paint_alpha("debug_paint_alpha", "0", FCVAR_DEVELOPMENTONLY );

int GetDiameter( int radius )
{
	return radius * 2 + 1;
}


BYTE GetColorIndex( BYTE byte )
{
	return ( PAINT_COLOR_BITS & byte ) >> NUM_ALPHA_BITS;
}

void SetColorBits( BYTE& byte, BYTE colorIndex, float alpha )
{
	BYTE nAlpha = static_cast< BYTE >( alpha * PAINT_ALPHA_BITS );
	BYTE nColor = colorIndex << NUM_ALPHA_BITS;

	byte = nColor | nAlpha;
}

float GetAlpha( BYTE byte )
{
	float alpha = ( PAINT_ALPHA_BITS & byte );
	alpha /= PAINT_ALPHA_BITS;
	alpha = clamp( alpha, 0.f, 1.f );
	return alpha;
}


#if !defined( LINUX )
// draw a surface in random color
void DebugDrawSurface( SurfaceHandle_t surfID )
{
	Color surfColor = Color( RandomInt(0, 255), RandomInt(0, 255), RandomInt(0, 255), 128 );

	// check if the sphere actually intersecting with the surface using barycentric test
	int nFirstVertex = MSurf_FirstVertIndex( surfID );
	int numVert = MSurf_VertCount( surfID );

	int vertIndex = host_state.worldbrush->vertindices[nFirstVertex];
	Vector vOrigin = host_state.worldbrush->vertexes[vertIndex].position;
	for (int v = 1; v < numVert - 1; ++v )
	{
		vertIndex = host_state.worldbrush->vertindices[nFirstVertex+v];
		Vector v1 = host_state.worldbrush->vertexes[vertIndex].position;
		vertIndex = host_state.worldbrush->vertindices[nFirstVertex+v+1];
		Vector v2 = host_state.worldbrush->vertexes[vertIndex].position;
	
		CDebugOverlay::AddTriangleOverlay( vOrigin, v1, v2, surfColor.r(), surfColor.g(), surfColor.b(), 128, false, 0.1f );
	}
}
#endif

extern MaterialSystem_SortInfo_t *materialSortInfoArray;

CPaintmapDataManager g_PaintManager;


CPaintmapDataManager::CPaintmapDataManager( void )
	: m_pPaintTextureDataArray( NULL ), m_iPaintmaps( 0 ), m_bShouldRegister( false )
{
}

CPaintmapDataManager::~CPaintmapDataManager( void )
{
}

void CPaintmapDataManager::DestroyPaintmapsData( void )
{
	if( m_pPaintTextureDataArray )
	{
		for( int i = 0; i != m_iPaintmaps; ++i )
		{
			m_pPaintTextureDataArray[i].Destroy();
		}
		delete []m_pPaintTextureDataArray;
		m_pPaintTextureDataArray = NULL;
		m_iPaintmaps = 0;
	}
}


void R_UpdatePaintmapRect( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects )
{
	materials->UpdatePaintmap( paintmap, pPaintData, numRects, pRects );
}


void R_UpdatePaintmap( ICallQueue *pCallQueue, int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects )
{
	if ( !pCallQueue )
	{
		R_UpdatePaintmapRect( paintmap, pPaintData, numRects, pRects );
	}
	else
	{
		pCallQueue->QueueCall( R_UpdatePaintmapRect, paintmap, pPaintData, numRects, CUtlEnvelope<Rect_t>( pRects, numRects ) );
	}
}


void MarkSurfaceBrushes( int nSurfIndex, worldbrushdata_t *pData = host_state.worldbrush )
{
	if ( !pData->m_pSurfaceBrushList )
		return;

	const dfacebrushlist_t &brushList = pData->m_pSurfaceBrushList[nSurfIndex];
	const uint16 *pBrushIndex = (brushList.m_nFaceBrushCount <= 1) ? &brushList.m_nFaceBrushStart : &pData->m_pSurfaceBrushes[brushList.m_nFaceBrushStart];
	CCollisionBSPData *pBSPData = GetCollisionBSPData();
	for ( int i = 0; i < brushList.m_nFaceBrushCount; i++ )
	{
		pBSPData->map_brushes[ pBrushIndex[i] ].contents |= CONTENTS_BRUSH_PAINT;
	}
}



void CPaintmapDataManager::UpdatePaintmapTextures()
{
	// Can't build lightmaps if the source data has been dumped
	CMatRenderContextPtr pRenderContext( materials );
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();

	if( pCallQueue )
		pCallQueue->QueueCall( materials, &IMaterialSystem::BeginUpdatePaintmaps );
	else
		materials->BeginUpdatePaintmaps();

	for( int paintmap=0; paintmap < m_iPaintmaps; ++paintmap )
	{
		PaintDirtyFlags_t nDirtyFlag = m_pPaintTextureDataArray[paintmap].GetDirtyFlag();
		if ( nDirtyFlag == PAINTMAP_CLEAN )
			continue;

		if ( nDirtyFlag == PAINTMAP_DIRTY_FULLRECT )
		{
			R_UpdatePaintmap( pCallQueue, paintmap, m_pPaintTextureDataArray[paintmap].GetPaintmapData(), 0, NULL );
		}
		else
		{
			CUtlVectorFixedGrowable<Rect_t, 1024> *pRects = m_pPaintTextureDataArray[paintmap].GetDirtyRectList();
			int count = pRects->Count();
			Assert( count > 0 );
			R_UpdatePaintmap( pCallQueue, paintmap, m_pPaintTextureDataArray[paintmap].GetPaintmapData(), count,  pRects->Base() );
		}

		m_pPaintTextureDataArray[paintmap].RemoveDirty();
	}

	if( pCallQueue )
		pCallQueue->QueueCall( materials, &IMaterialSystem::EndUpdatePaintmaps );
	else
		materials->EndUpdatePaintmaps();
}


BYTE* CPaintmapDataManager::GetPaintmapData( int paintmap )
{
	if ( paintmap >= 0 && paintmap < m_iPaintmaps )
	{
		return m_pPaintTextureDataArray[paintmap].GetPaintmapData();
	}

	return NULL;
}


void CPaintmapDataManager::GetPaintmapSize( int paintmap, int& width, int& height )
{
	if ( paintmap >= 0 && paintmap < m_iPaintmaps )
	{
		m_pPaintTextureDataArray[paintmap].GetPaintSize( &width, &height );
	}
}


void CPaintmapDataManager::OnRestorePaintmaps()
{
	for ( int i=0; i<m_iPaintmaps; ++i )
	{
		m_pPaintTextureDataArray[i].MarkAsDirty();
	}
}


void CPaintmapDataManager::RemoveAllPaint( void )
{
	if( m_pPaintTextureDataArray )
	{
		for( int i = 0; i != m_iPaintmaps; ++i )
		{
			m_pPaintTextureDataArray[i].ClearTexture();
		}
	}
	int nBrushCount = GetCollisionBSPData()->numbrushes;
	cbrush_t *pBrush = GetCollisionBSPData()->map_brushes.Base();
	for ( int i = 0; i < nBrushCount; i++ )
	{
		pBrush[i].contents &= ~CONTENTS_BRUSH_PAINT;
	}
}


void CPaintmapDataManager::RemovePaint( const model_t *pModel )
{
	Assert( pModel );
	if ( !pModel )
		return;

	Assert( m_pPaintTextureDataArray );
	if ( !m_pPaintTextureDataArray )
		return;

	BYTE noColor;
	SetColorBits( noColor, NO_POWER, 0.f );

	for ( int i=0; i<pModel->brush.nummodelsurfaces; ++i )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface + i );
		int lightmapID = materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID;
		
		CPaintTextureData& paintTexture = m_pPaintTextureDataArray[ lightmapID ];
		Rect_t rect;
		rect.x = MSurf_OffsetIntoLightmapPage( surfID )[0];
		rect.y = MSurf_OffsetIntoLightmapPage( surfID )[1];
		rect.width = MSurf_LightmapExtents( surfID )[0] + 1;
		rect.height= MSurf_LightmapExtents( surfID )[1] + 1;

		for ( int y=0; y<rect.height; ++y )
		{
			for ( int x=0; x<rect.width; ++x )
			{
				paintTexture.SetPixel( x + rect.x, y + rect.y, noColor );
			}
		}

		paintTexture.AddDirtyRect( rect );
	}
}



void CPaintmapDataManager::PaintAllSurfaces( BYTE color )
{
	AssertMsg( m_pPaintTextureDataArray, "Failed to paint all surfaces. Paint textures are not allocated." );

	if( m_pPaintTextureDataArray )
	{
		for( int i = 0; i != m_iPaintmaps; ++i )
		{
			m_pPaintTextureDataArray[i].PaintAllSurfaces( color );
		}
	}

	int numSurfs = host_state.worldbrush->numsurfaces;

	for ( int i=0; i<numSurfs; ++i )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( i );

		MSurf_Flags( surfID ) |= SURFDRAW_PAINTED;
	}

	int nBrushCount = GetCollisionBSPData()->numbrushes;
	cbrush_t *pBrush = GetCollisionBSPData()->map_brushes.Base();
	for ( int i = 0; i < nBrushCount; i++ )
	{
		pBrush[i].contents |= CONTENTS_BRUSH_PAINT;
	}
}


void CPaintmapDataManager::BeginPaintmapsDataAllocation( int iPaintmapCount )
{
	DestroyPaintmapsData();

	m_iPaintmaps = iPaintmapCount;
	m_pPaintTextureDataArray = new CPaintTextureData[iPaintmapCount];
}

void CPaintmapDataManager::AllocatePaintmapData( int iPaintmapID, int iCorrespondingLightMapWidth, int iCorrespondingLightMapHeight )
{
	m_pPaintTextureDataArray[iPaintmapID].Init( iCorrespondingLightMapWidth, iCorrespondingLightMapHeight, iPaintmapID );
}



CPaintTextureData::CPaintTextureData()
{
	m_nPaintWidth = m_nPaintHeight = 0;
	m_backbuffer = NULL;
	m_nDirtyFlag = PAINTMAP_CLEAN;
}


// Initializes, shuts down the material
bool CPaintTextureData::Init( int width, int height, int lightmapPageID )
{
	m_nPaintWidth =  width;
	m_nPaintHeight = height;
	m_lightmapPageID = lightmapPageID;

	m_backbuffer = new BYTE[ m_nPaintWidth * m_nPaintHeight ];
	AssertMsg( m_backbuffer, "Failed to allocate paint texture data array" );
	ClearBuffer();

	return true;
}


void CPaintTextureData::Destroy()
{
	if ( m_backbuffer )
	{
		delete[] m_backbuffer;
		m_backbuffer = NULL;
	}
}


BYTE CPaintTextureData::GetPixel( int x, int y ) const
{
	return m_backbuffer[ y * m_nPaintWidth + x ];
}


void CPaintTextureData::SetPixel( int x, int y, BYTE color )
{
	m_backbuffer[ y * m_nPaintWidth + x ] = color;
}


bool CPaintTextureData::Paint( const PaintRect_t& paintRect )
{
	uint32 nChangeFlags = DrawCircle( paintRect );
	if ( nChangeFlags & TEXEL_CHANGED )
	{
		// no need for extra alpha if we are erasing
		if ( paint_alpha_offset_enabled.GetBool() && paintRect.colorIndex != NO_POWER )
		{
			Rect_t rect = paintRect.rect;

			int sOffset = MSurf_OffsetIntoLightmapPage( paintRect.surfID )[0];
			int tOffset = MSurf_OffsetIntoLightmapPage( paintRect.surfID )[1];
			int sMax = ( MSurf_LightmapExtents( paintRect.surfID )[0] );
			int tMax = ( MSurf_LightmapExtents( paintRect.surfID )[1] );

			if ( paintRect.rect.x > sOffset )
			{
				--rect.x;
				++rect.width;
			}

			if ( paintRect.rect.y > tOffset )
			{
				--rect.y;
				++rect.height;
			}

			if ( paintRect.rect.x + paintRect.rect.width - 1 < sOffset + sMax )
			{
				++rect.width;
			}

			if ( paintRect.rect.y + paintRect.rect.height - 1 < tOffset + tMax )
			{
				++rect.height;
			}

			AddDirtyRect( rect );
		}
		else
		{
			AddDirtyRect( paintRect.rect );
		}
	}
	return ( nChangeFlags & PAINT_POWER_CHANGED ) != 0;
}


BYTE BlendColor( BYTE colorIndex, BYTE nPrePixel, float flAlpha, float flPaintCoatPercent, float flMaxAlpha )
{
	BYTE finalColor;

	// stomp erase color
	if ( colorIndex == NO_POWER )
	{
		// if it's erase, the alpha is always 0.f or 1.f (0.f for surrounding edges)
		SetColorBits( finalColor, paint_alpha_offset_enabled.GetBool() ? GetColorIndex( nPrePixel ) : colorIndex, 0.f );
	}
	else
	{
		float bAlpha = GetAlpha( nPrePixel );
		float flNewAlpha = clamp( bAlpha + ( flPaintCoatPercent * flAlpha ), 0.f, flMaxAlpha );

		SetColorBits( finalColor, colorIndex, flNewAlpha );
	}

	return finalColor;
}


float ComputeCircleAlpha( const PaintRect_t& paintRect, int x, int y )
{
	float flPixelDist = Vector2D( x - paintRect.uvCenter.x, y - paintRect.uvCenter.y ).Length();
	float flRadiusRatio = clamp( paintRect.flCenterAlpha + flPixelDist / paintRect.flCircleRadius, 0.f, 1.f );
	float flAlpha = 1.f - Bias(  flRadiusRatio, paintsplat_bias.GetFloat() );

	if ( paintsplat_noise_enabled.GetBool() )
	{
		float flNoise = paintsplat_max_alpha_noise.GetFloat();
		flAlpha += RandomFloat( -flNoise, flNoise );
		flAlpha = clamp( flAlpha, 0.f, 1.f );
	}

	return flAlpha;
}


uint32 CPaintTextureData::BlendLuxel( const PaintRect_t& paintRect, int x, int y, float flNewAlpha, float flMaxAlpha /*= 1.f*/ )
{
	uint32 nChangeFlags = 0;
	BYTE nPrePixel = GetPixel( x, y );
	BYTE nPostPixel = BlendColor( paintRect.colorIndex, nPrePixel, flNewAlpha, paintRect.flPaintCoatPercent, flMaxAlpha );

	if ( nPrePixel != nPostPixel )
	{
		SetPixel( x, y, nPostPixel );
		nChangeFlags |= TEXEL_CHANGED;
		if ( GetColorIndex( nPrePixel ) != GetColorIndex( nPostPixel ) )
		{
			nChangeFlags |= PAINT_POWER_CHANGED;
		}
	}

	return nChangeFlags;
}


uint32 CPaintTextureData::AddSurroundingAlpha( const PaintRect_t& paintRect, int x, int y )
{
	SurfaceHandle_t surfID = paintRect.surfID;
	int surroundCase = 0;
	int sOffset = MSurf_OffsetIntoLightmapPage( surfID )[0];
	int tOffset = MSurf_OffsetIntoLightmapPage( surfID )[1];
	int sMax = ( MSurf_LightmapExtents( surfID )[0] );
	int tMax = ( MSurf_LightmapExtents( surfID )[1] );
	surroundCase |= ( x > sOffset ) ? 1 : 0; // left
	surroundCase |= ( x < sOffset + sMax ) ? 2 : 0; // right
	surroundCase |= ( y > tOffset ) ? 4 : 0; // up
	surroundCase |= ( y < tOffset + tMax ) ? 8 : 0; // down

	uint32 nChangeFlags = 0;

	switch ( surroundCase )
	{
	case 1: // left only
		{
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			break;
		}
	case 2: // right only
		{
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			break;
		}
	case 3: // left right
		{
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			break;
		}
	case 4: // up only
		{
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			break;
		}
	case 5: // left up
		{
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			break;
		}
	case 6: // right up
		{
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			break;
		}
	case 7: // left up right
		{
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			break;
		}
	case 8: // down only
		{
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	case 9: // left down
		{
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	case 10: // right down
		{
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	case 11: // left down right
		{
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			break;
		}
	case 12: // up down
		{
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	case 13: // up left down
		{
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	case 14: // up right down
		{
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	case 15: // all 8 surrounding luxels
		{
			nChangeFlags |= BlendLuxel( paintRect, x, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x - 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y - 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x + 1, y + 1, 0.f );
			nChangeFlags |= BlendLuxel( paintRect, x, y + 1, 0.f );
			break;
		}
	default:
		break;
	}

	return nChangeFlags;
}


uint32 CPaintTextureData::DrawLine( const PaintRect_t& paintRect, int x1, int x2, int y )
{
	uint32 nChangeFlags = 0;

	int sOffset = MSurf_OffsetIntoLightmapPage( paintRect.surfID )[0];
	int tOffset = MSurf_OffsetIntoLightmapPage( paintRect.surfID )[1];
	int sMax = ( MSurf_LightmapExtents( paintRect.surfID )[0] );
	int tMax = ( MSurf_LightmapExtents( paintRect.surfID )[1] );

	int start = x1;
	int end = x2;

	// clamp border luxel alpha below paint_max_surface_border_alpha threshold so the shader won't look up color from other surface border
	float flMaxAlpha = 1.f;
	if ( y == tOffset || y == tOffset + tMax )
	{
		flMaxAlpha = paint_max_surface_border_alpha.GetFloat();
	}
	else
	{
		if ( x1 == sOffset )
		{
			float flAlpha = ( paintRect.colorIndex == NO_POWER ) ? 1.f : ComputeCircleAlpha( paintRect, x1, y );
			nChangeFlags |= BlendLuxel( paintRect, x1, y, flAlpha, paint_max_surface_border_alpha.GetFloat() );
			++start;
		}
		if ( x2 == sOffset + sMax )
		{
			float flAlpha = ( paintRect.colorIndex == NO_POWER ) ? 1.f : ComputeCircleAlpha( paintRect, x2, y );
			nChangeFlags |= BlendLuxel( paintRect, x2, y, flAlpha, paint_max_surface_border_alpha.GetFloat() );
			--end;
		}
	}

	for ( int x = start; x <= end; ++x )
	{
		float flAlpha = ( paintRect.colorIndex == NO_POWER ) ? 1.f : ComputeCircleAlpha( paintRect, x, y );

		nChangeFlags |= BlendLuxel( paintRect, x, y, flAlpha, flMaxAlpha );
	}

	// don't add surrounding alpha if we are erasing
	if ( paint_alpha_offset_enabled.GetBool() && paintRect.colorIndex != NO_POWER )
	{
		if ( x1 == x2 )
		{
			nChangeFlags |= AddSurroundingAlpha( paintRect, x1, y );
		}
		else
		{
			nChangeFlags |= AddSurroundingAlpha( paintRect, x1, y );
			nChangeFlags |= AddSurroundingAlpha( paintRect, x2, y );
		}
	}

	return nChangeFlags;
}


uint32 CPaintTextureData::Draw2Lines( const PaintRect_t& paintRect, float x, float y )
{
	const Rect_t& rect = paintRect.rect;
	int minX = rect.x;
	int minY = rect.y;
	int maxX = rect.x + rect.width - 1;
	int maxY = rect.y + rect.height - 1;

	const Vector2D& uvCenter = paintRect.uvCenter;

	int x1 = MAX( ( int )( uvCenter.x - x - 0.5f ), minX );
	int y1 = MAX( ( int )( uvCenter.y - y - 0.5f ), minY );
	int x2 = MIN( ( int )( uvCenter.x + x + 0.5f ), maxX );
	int y2 = MIN( ( int )( uvCenter.y + y + 0.5f ), maxY );

	// if line is outside the rect, don't do anything
	if ( x1 > maxX || x2 < minX )
		return 0;

	uint32 nChangeFlags = 0;

	// draw line1
	if ( minY <= y1 && y1 <= maxY )
	{
		nChangeFlags |= DrawLine( paintRect, x1, x2, y1 );
	}

	if ( y1 != y2 )
	{
		if ( minY <= y2 && y2 <= maxY )
		{
			nChangeFlags |= DrawLine( paintRect, x1, x2, y2 );
		}
	}

	return nChangeFlags;
}


uint32 CPaintTextureData::Draw4Lines( const PaintRect_t& paintRect, float x, float y )
{
	uint32 nChangeFlags = 0;

	nChangeFlags |= Draw2Lines( paintRect, x, y );
	if ( x != y )
	{
		nChangeFlags |= Draw2Lines( paintRect, y, x );
	}

	return nChangeFlags;
}


uint32 CPaintTextureData::DrawCircle( const PaintRect_t& paintRect )
{
	// assume uvExtents x and y are about the same
	float radius = paintRect.flCircleRadius;
	float error = -radius;
	float x = radius;
	float y = 0;

	uint32 nChangeFlags = 0;

	while (x >= y)
	{
		nChangeFlags |= Draw4Lines( paintRect, x, y );

		error += y;
		++y;
		error += y;

		if (error >= 0)
		{
			--x;
			error -= x;
			error -= x;
		}
	}

	return nChangeFlags;
}


// Returns the tecxoord range
void CPaintTextureData::GetTexCoordRange( float *pMaxU, float *pMaxV )
{
	*pMaxU = *pMaxV = 1.0f;
	return;
}


// Returns the size of the paint texture (stored in a subrect of the material itself)
void CPaintTextureData::GetPaintSize( int *pWidth, int *pHeight )
{
	*pWidth = m_nPaintWidth;
	*pHeight = m_nPaintHeight;
}


void CPaintTextureData::ClearTexture()
{
	ClearBuffer();

	MarkAsDirty();
}

void CPaintTextureData::GetPixels( const Rect_t& splatRect, CUtlVector<BYTE>& surfColors )
{
	//sample color inside rect
	for (int y = 0; y < splatRect.height; ++y )
	{
		for (int x = 0; x < splatRect.width; ++x )
		{
			BYTE packedColor = GetPixel( splatRect.x + x, splatRect.y + y );
			BYTE colorIndex = GetColorIndex( packedColor );
			float flAlpha = GetAlpha( packedColor );

			if( flAlpha > paint_min_valid_alpha_value.GetFloat() )
				surfColors.AddToTail( colorIndex );

			if( debug_paint_alpha.GetBool() )
			{
				Con_NPrintf( y * splatRect.width + x, "(%d, %d), Alpha: %f\n", x, y, flAlpha );
			}
		}
	}
}


void CPaintTextureData::ClearBuffer( BYTE *pByte )
{
	BYTE nPixel = ( pByte ) ? *pByte : NO_POWER << NUM_ALPHA_BITS;
	V_memset( m_backbuffer, nPixel, m_nPaintWidth * m_nPaintHeight );
}

// paint all surfaces
void CPaintTextureData::PaintAllSurfaces( BYTE colorIndex )
{
	BYTE nPixel;
	SetColorBits( nPixel, colorIndex, 1.f );
	ClearBuffer( &nPixel );

	MarkAsDirty();
}


void CPaintTextureData::GetSurfacePaintData( SurfaceHandle_t surfID, CUtlVector< BYTE > &data ) const
{
	if ( !m_backbuffer )
		return;

	short x = MSurf_OffsetIntoLightmapPage( surfID )[0];
	short y = MSurf_OffsetIntoLightmapPage( surfID )[1];
	short width		= MSurf_LightmapExtents( surfID )[0] + 1;
	short height	= MSurf_LightmapExtents( surfID )[1] + 1;

	int nDataCount = width * height;

	// make sure alloc number is divisible by 4
	uint nAllocSize = nDataCount + ( ( 4 - ( nDataCount % 4 ) ) % 4 );
	Assert( ( nAllocSize % 4 ) == 0 );

	data.EnsureCount( nAllocSize );

	BYTE* pOut = data.Base();
	const BYTE* pEnd = data.Base() + nDataCount;
	const BYTE* pRead = &m_backbuffer[ y * m_nPaintWidth + x ];
	while( pOut < pEnd )
	{
		V_memcpy( pOut, pRead, width );
		pOut += width;
		pRead += m_nPaintWidth;
	}
	Assert( pOut == pEnd );
}


void CPaintTextureData::SetSurfacePaintData( SurfaceHandle_t surfID, const CUtlVector< BYTE > &data )
{
	if ( !m_backbuffer )
		return;

	Rect_t rect;
	rect.x = MSurf_OffsetIntoLightmapPage( surfID )[0];
	rect.y = MSurf_OffsetIntoLightmapPage( surfID )[1];
	rect.width	= MSurf_LightmapExtents( surfID )[0] + 1;
	rect.height	= MSurf_LightmapExtents( surfID )[1] + 1;

	uint nDataCount = rect.width * rect.height;

	const BYTE* pRead = data.Base();
	const BYTE* pEnd = data.Base() + nDataCount;

	BYTE *pSet = &m_backbuffer[ rect.y * m_nPaintWidth + rect.x ];
	while ( pRead < pEnd )
	{
		V_memcpy( pSet, pRead, rect.width );
		pRead += rect.width;
		pSet += m_nPaintWidth;
	}

	AddDirtyRect( rect );

	// mark surf as painted
	MSurf_Flags( surfID ) |= SURFDRAW_PAINTED;
	MarkSurfaceBrushes( MSurf_Index(surfID) );
}


void EncodeDataRLE( const uint32* pBuffer, uint32 nDwordCount, CUtlVector< uint32 > &data )
{
	if ( nDwordCount == 0 )
		return;

	// first build a list of runs into a local utlvector
	CUtlVectorFixedGrowable<intp, 1024> outList;

	// this is the start of the run of unique data to copy
	const uint32 *pCopyStart = pBuffer;

	// we need to find at least a run of 3 in order for encoding a run to pay for itself, keep last two
	uint32 nSymbol0 = *pCopyStart;
	uint32 nSymbol1 = pCopyStart[1];
	const uint32 *pEndOfData = pCopyStart + nDwordCount;
	// start here with the first two loaded up and ready to copy
	const uint32 *pCurrent = pCopyStart + 2;

	// this is the size in dwords of the rle output buffer
	uint32 nRLESize = 0;

	// anything that defines _GAMECONSOLE should support prefetches
#if defined(_GAMECONSOLE)
	// prefetch the first 7 cache lines
	for ( int i = 0; i < 7; i++ )
	{
		PREFETCH_128( pCurrent + (i * 32), 0 );
	}
#endif
	// this is the next read that will trigger a prefetch (not every read should prefetch)
	const uint32 *pNextPrefetch = pCurrent + 32;
	// this is the end of the space we'd want to prefetch ahead of
	const uint32 *pLastPrefetch = pEndOfData - (8 * 32);
	// this is how far ahead of the current read to prefetch
#if defined(_GAMECONSOLE)
	const uint32 nPrefetchOffsetDwords = 7 * 32;		// will cause an error if your platform has prefetch but not _GAMECONSOLE
#endif

	while ( pCurrent < pEndOfData )
	{
		uint32 nSymbol2 = *pCurrent;
		if ( nSymbol0 == nSymbol1 && nSymbol1 == nSymbol2 )
		{
			// found a run
			const uint32 *pRunStart = pCurrent - 2;
			int nCopyOut = pRunStart - pCopyStart;
			// copy any previous run of non-similar values
			if ( nCopyOut )
			{
				// note, we're building a list of copies, so just add a record
				outList.AddToTail( nCopyOut );
				outList.AddToTail( intp(pCopyStart) );
				nRLESize += nCopyOut + 1;
				pCopyStart = pRunStart;
			}
			// follow current run until its end
			pCurrent++;
			while ( pCurrent < pEndOfData && *pCurrent == nSymbol0 )
			{
				pCurrent++;
				if ( pCurrent >= pNextPrefetch && pNextPrefetch < pLastPrefetch )
				{
					// schedule prefetches 128 bytes (32 dwords) apart
					PREFETCH_128( pNextPrefetch + nPrefetchOffsetDwords, 0 );
					pNextPrefetch += 32;
				}
			}
			// write out the run
			int nRunCount = pCurrent - pRunStart;
			outList.AddToTail( -nRunCount );
			outList.AddToTail( nSymbol0 );
			nRLESize += 2;
			if ( pCurrent < pEndOfData )
			{
				// restart run detection
				nSymbol2 = *pCurrent;
				nSymbol1 = nSymbol2+1;  // make these not equal so a run will not be detected
			}

			pCopyStart = pCurrent;
		}
		// advance read cursor and shift over previous run-detect state
		pCurrent++;
		nSymbol0 = nSymbol1;
		nSymbol1 = nSymbol2;
		if ( pCurrent >= pNextPrefetch && pNextPrefetch < pLastPrefetch )
		{
			PREFETCH_128( pNextPrefetch + nPrefetchOffsetDwords, 0 );
			pNextPrefetch += 32;
		}
	}
	int nCopyOut = pEndOfData - pCopyStart;
	if ( nCopyOut )
	{
		// add another record if the map ends in a copy
		outList.AddToTail( nCopyOut );
		outList.AddToTail( (intp)pCopyStart );
		nRLESize += nCopyOut + 1;
		pCopyStart = pEndOfData;
	}

	// now build the full RLE output from the records we just made
	// we can allocate once because we know the output size
	data.SetCount( nRLESize );
	uint32 *pOut = data.Base();
	const intp *pRLE = outList.Base();
	const intp *pEndRLE = pRLE + outList.Count();
	while ( pRLE < pEndRLE )
	{
		int nCount = *pRLE++;
		if ( nCount < 0 )
		{
			// run, insert in output
			*pOut++ = uint32( nCount );
			*pOut++ = uint32( *pRLE++ );
		}
		else
		{
			// copy, do the actual copy now
			*pOut++ = uint32( nCount );
			const uint32 *pSource = (const uint32 *)*pRLE++;
			for ( int i = 0; i < nCount; i++ )
			{
				*pOut++ = *pSource++;
			}
		}
	}

	// make sure our buffer contains exactly what the records predicted
	Assert( (uint32)(pOut - data.Base()) == nRLESize );
}


void CPaintmapDataManager::GetPaintmapDataRLE( CUtlVector< uint32 > &data ) const
{
	for ( int i=0; i< host_state.worldbrush->numsurfaces; ++i )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( i );
		if ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED )
		{
			int lightmapID = materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID;

			// get paint data from a surface
			CUtlVector< BYTE > surfPaintData;
			m_pPaintTextureDataArray[lightmapID].GetSurfacePaintData( surfID, surfPaintData );
			
			// encode into RLE format
			CUtlVector< uint32 > rleData;
			uint nDwordCount = surfPaintData.Count() / 4;
			EncodeDataRLE( (uint32*)surfPaintData.Base(), nDwordCount, rleData );

			// write surface index, RLE size, and RLE data to output data
			data.AddToTail( i );
			data.AddToTail( rleData.Count() );
			data.AddMultipleToTail( rleData.Count(), rleData.Base() );
		}
	}
}


void DecodeDataRLE( const uint32* pRLEStart, uint rleCount, SurfaceHandle_t surfID, uint32* pOutput, uint nDwordCount )
{
	// NOTE: This assumes the size is always correct, will assert if not
	const uint32 *pRLE = pRLEStart;
	const uint32 *pEndOfData = pRLE + rleCount;

#if _DEBUG
	const uint32* pOutStart = pOutput;
#endif

	while ( pRLE < pEndOfData )
	{
		int32 nCount = int32(*pRLE++);
		if ( nCount < 0 ) // run, output repeated value nCount times
		{
			nCount = -nCount;
			uint32 nRunVal = *pRLE++;

			for ( int i = 0; i < nCount; i++ )
			{
				*pOutput++ = nRunVal;
			}
		}
		else // copy raw dwords from input
		{
			for ( int i = 0; i < nCount; i++ )
			{
				*pOutput++ = *pRLE++;
			}
		}
	}

	Assert( pRLE == pEndOfData );
	Assert( pOutput - pOutStart == nDwordCount );
}


void CPaintmapDataManager::LoadPaintmapDataRLE( const CUtlVector< uint32 > &data )
{
	const uint32* pCurrent = data.Base();
	const uint32* pEnd = pCurrent + data.Count();

	while ( pCurrent < pEnd )
	{
		// assumes that the surface index is always the same (it'll change if the map's changed)
		int surfIndex = *pCurrent++;
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfIndex );
		int lightmapID = materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID;

		uint nRLESize = *pCurrent++;
		
		// alloc output. make sure the size is divisible by 4 to copy data from array of uint32
		CUtlVector< BYTE > rawPaintData;
		int width	= MSurf_LightmapExtents( surfID )[0] + 1;
		int height	= MSurf_LightmapExtents( surfID )[1] + 1;
		int nByteCount = width * height;
		uint nAllocSize = nByteCount + ( ( 4 - ( nByteCount % 4 ) ) % 4 );
		Assert( ( nAllocSize % 4 ) == 0 );
		rawPaintData.SetCount( nAllocSize );
		uint nDWord = nAllocSize / 4;
		DecodeDataRLE( pCurrent, nRLESize, surfID, (uint32*)rawPaintData.Base(), nDWord );
		pCurrent += nRLESize;

		m_pPaintTextureDataArray[lightmapID].SetSurfacePaintData( surfID, rawPaintData );
	}
}

void CPaintTextureData::AddDirtyRect( const Rect_t& rect )
{
	MarkAsDirty( PAINTMAP_DIRTY_SUBRECT );
	m_dirtyRects.AddToTail( rect );
}


CUtlVectorFixedGrowable<Rect_t, 1024>* CPaintTextureData::GetDirtyRectList()
{
	return &m_dirtyRects;
}


void CPaintTextureData::RemoveDirty()
{
	m_nDirtyFlag = PAINTMAP_CLEAN;
	m_dirtyRects.RemoveAll();
}


PaintDirtyFlags_t CPaintTextureData::GetDirtyFlag() const
{
	return m_nDirtyFlag;
}


void CPaintTextureData::MarkAsDirty( PaintDirtyFlags_t nDirtyFlag /*= PAINTMAP_DIRTY_FULLRECT*/ )
{
	m_nDirtyFlag = ( m_nDirtyFlag == PAINTMAP_DIRTY_FULLRECT ) ? m_nDirtyFlag : nDirtyFlag;
}


void ProjectPointOntoSurfaceTexture( const SurfaceCtx_t& ctx, SurfaceHandle_t surfID, const Vector& vPoint, Vector2D& uv )
{
	mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );

	uv.x = DotProduct ( vPoint, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3];
	uv.x -= MSurf_LightmapMins( surfID )[0];
	uv.x += 0.5f;

	uv.y = DotProduct ( vPoint, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3];
	uv.y -= MSurf_LightmapMins( surfID )[1];
	uv.y += 0.5f;

	uv *= ctx.m_Scale;
	uv += ctx.m_Offset;

	// convert back to the old space
	uv /= ctx.m_Scale;
}

bool ComputePaintRect( SurfaceHandle_t surfID, const Vector &vPosition, float flSphereRadius, PaintRect_t& paintRect )
{
#if !defined( LINUX )
	// find dist from plane
	VPlane forwardFacingPlane = MSurf_GetForwardFacingPlane( surfID );
	float distFromPlane = forwardFacingPlane.DistTo( vPosition );
	AssertMsg( distFromPlane < flSphereRadius, "How did this surface intersect with the query sphere?" );

	float circleRadius = FastSqrt( flSphereRadius * flSphereRadius - distFromPlane * distFromPlane );

	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, surfID );
	Vector2D uvCenter, uvExtents;
	{
		ProjectPointOntoSurfaceTexture( ctx, surfID, vPosition, uvCenter );
		mtexinfo_t *pTexInfo = MSurf_TexInfo( surfID );
		uvExtents.x = circleRadius * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D().Length();
		uvExtents.y = circleRadius * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D().Length();
	}

	Assert( uvExtents.x > 0 && uvExtents.y > 0 );

	Vector2D uvMins, uvMaxs;
	uvMins = uvCenter - uvExtents;
	uvMaxs = uvCenter + uvExtents;

	int sOffset = MSurf_OffsetIntoLightmapPage( surfID )[0];
	int tOffset = MSurf_OffsetIntoLightmapPage( surfID )[1];
	int sMax = ( MSurf_LightmapExtents( surfID )[0] );
	int tMax = ( MSurf_LightmapExtents( surfID )[1] );

	if ( ( sOffset <= uvMaxs.x && uvMins.x <= sOffset + sMax ) && ( tOffset <= uvMaxs.y && uvMins.y <= tOffset + tMax ) )
	{
		// init paintRect
		float flRoundedCircleRadius = floor( fpmax( uvExtents.x, uvExtents.y ) + 0.5f );
		int surfWidth = sMax + 1;
		int surfHeight = tMax + 1;

		paintRect.flCenterAlpha = distFromPlane / flSphereRadius;
		paintRect.flCircleRadius = flRoundedCircleRadius;
		paintRect.uvCenter = uvCenter;
		paintRect.surfID = surfID;

		int startX = MAX( ( int )( uvCenter.x - flRoundedCircleRadius - 0.5f ), sOffset );
		int startY = MAX( ( int )( uvCenter.y - flRoundedCircleRadius - 0.5f ), tOffset );
		int endX = MIN( ( int )( uvCenter.x + flRoundedCircleRadius + 0.5f ), sOffset + surfWidth );
		int endY = MIN( ( int )( uvCenter.y + flRoundedCircleRadius + 0.5f ), tOffset + surfHeight );
		Rect_t rect;
		rect.x = startX;
		rect.y = startY;
		rect.width = endX - startX;
		rect.height = endY - startY;

		paintRect.rect = rect;

		return ( rect.width > 0 && rect.height > 0 );
	}
#endif

	return false;
}

bool R_PaintSurface( SurfaceHandle_t surfID, const Vector &vPosition, float flSphereRadius, const VPlane& basePlane, BYTE colorIndex, float flPaintCoatPercent )
{
	bool bAddedPaint = false;
	int lightmapID = materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID;

	PaintRect_t paintRect;
	paintRect.colorIndex = colorIndex;
	paintRect.flPaintCoatPercent = flPaintCoatPercent;
	if ( ComputePaintRect( surfID, vPosition, flSphereRadius, paintRect ) )
	{
		if ( g_PaintManager.m_pPaintTextureDataArray[ lightmapID ].Paint( paintRect ) )
		{
			// HACK SUPER HACK! mark surface as painted to optimize rendering
			MSurf_Flags( surfID ) |= SURFDRAW_PAINTED;
			MarkSurfaceBrushes( MSurf_Index(surfID) );

			bAddedPaint = true;
		}
	}
	
	return bAddedPaint;
}

//-----------------------------------------------------------------------------
// find a surface to paint by traversing through all surfaces in a model
//-----------------------------------------------------------------------------

struct paintinfo_t
{
	Vector				m_vPosition;
	VPlane				m_plane;	// closest plane to the sphere
	worldbrushdata_t	*m_pBrush;		// The shared brush data for this model
	float				m_flSize;			// radius of surf searching sphere
	float				m_flCurrentDistance; // distance away from the center of the sphere
	CUtlVector<SurfaceHandle_t>	m_aApplySurfs;

	// debug stuff
	bool				m_bPainting;
};


Vector FindClosestPointToTriangle( const Vector& p, const Vector& a, const Vector& b, const Vector& c )
{
	Vector ab = b - a;
	Vector ac = c - a;
	Vector bc = c - b;

	// project p onto ab,
	// p' = a + projAB * ab, projAB = projABnom/(projABnom+projABdenom)
	float projABnom = DotProduct( p - a, ab ), projABdenom = DotProduct( p - b, a - b );

	// project p onto ac,
	// p' = a + projAC * ac,  = projACnom/(projACnom+projACdenom)
	float projACnom = DotProduct( p - a, ac ), projACdenom = DotProduct( p - c, a - c );

	if ( projABnom <= 0.0f && projACnom <= 0.0f )
	{
		return a; // Vertex region early out
	}

	// project p onto bc,
	// p' = b + projBC * bc, projBC = projBCnom/(projBCnom+projBCdenom)
	float projBCnom = DotProduct( p - b, bc ), projBCdenom = DotProduct( p - c, b - c );

	if ( projABdenom <= 0.0f && projBCnom <= 0.0f )
	{
		return b; // Vertex region early out
	}

	if ( projACdenom <= 0.0f && projBCdenom <= 0.0f )
	{
		return c; // Vertex region early out
	}


	Vector n = CrossProduct( ab, ac );

	// P is outside (or on) AB if the triple product [N PA PB] = N.(PAxPB) <= 0
	float tpC = DotProduct( n, CrossProduct( a - p, b - p ) );
	// If P outside AB and within feature region of AB, return projection of P onto AB
	if ( tpC <= 0.0f && projABnom >= 0.0f && projABdenom >= 0.0f )
	{
		return a + projABnom / ( projABnom + projABdenom ) * ab;
	}

	// P is outside (or on) BC if the triple product [N PB PC] = N.(PBxPC) <= 0
	float tpA = DotProduct( n, CrossProduct(b - p, c - p) );
	// If P outside BC and within feature region of BC, return projection of P onto BC
	if ( tpA <= 0.0f && projBCnom >= 0.0f && projBCdenom >= 0.0f )
	{
		return b + projBCnom / (projBCnom + projBCdenom) * bc;
	}

	// P is outside (or on) CA if the triple product [N PC PA] = N.(PCxPA) <= 0
	float tpB = DotProduct( n, CrossProduct(c - p, a - p) );
	// If P outside CA and within feature region of CA, return projection of P onto CA
	if ( tpB <= 0.0f && projACnom >= 0.0f && projACdenom >= 0.0f )
	{
		return a + projACnom / ( projACnom + projACdenom ) * ac;
	}

	// P must project inside the triangle. Compute the position using barycentric coordinates
	float u = tpA / ( tpA + tpB + tpC );
	float v = tpB / ( tpA + tpB + tpC );
	float w = 1.0f - u - v; // = tpC / ( tpA + tpB + tpC )

	return u * a + v * b + w * c;
}


bool IsSphereIntersectForwardFacingTriangle( const Vector& vCenter, const VPlane& plane, float flRadius, const Vector& a, const Vector& b, const Vector& c, Vector& vClosestPoint )
{
	vClosestPoint = FindClosestPointToTriangle( vCenter, a, b, c );
	float flDistFromPlane = DotProduct( vClosestPoint, plane.m_Normal ) - plane.m_Dist;
	if ( flDistFromPlane < 0.f )
	{
		vClosestPoint = vClosestPoint + ( flDistFromPlane + 0.1f ) * plane.m_Normal;
	}

	Vector vDist = vCenter - vClosestPoint;

	float flDistSquare = DotProduct( vDist, vDist );

	return flDistSquare < Square( flRadius );
}


void R_AddPaintToSurface( SurfaceHandle_t surfID, paintinfo_t *paintinfo )
{
	const unsigned int ignoreFlags = SURFDRAW_NOPAINT | SURFDRAW_TRANS; 
	if ( MSurf_Flags( surfID ) & ignoreFlags )
		return;

	// Displacement surfaces get decals in R_DecalLeaf.
	if ( SurfaceHasDispInfo( surfID ) )
		return;

	if ( paintinfo->m_aApplySurfs.Find( surfID ) != -1 )
		return;

	// in multiplayer, materialSortInfoArray is initialized after paint power user objects are active
	if ( materialSortInfoArray == NULL )
		return;

	// don't do it if it's full bright
	if ( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID < 0 )
		return;

	VPlane plane = MSurf_GetForwardFacingPlane( surfID );
	float flDistFromPlane = DotProduct( paintinfo->m_vPosition, plane.m_Normal ) - plane.m_Dist;

	// ignore if point is behind the plane or too far from the forward facing plane
	if ( flDistFromPlane > paintinfo->m_flSize || flDistFromPlane < 0.f )
		return;

	// check if the sphere actually intersecting with the surface using barycentric test
	int nFirstVertex = MSurf_FirstVertIndex( surfID );
	int numVert = MSurf_VertCount( surfID );

	int vertIndex = host_state.worldbrush->vertindices[nFirstVertex];
	Vector vOrigin = host_state.worldbrush->vertexes[vertIndex].position;
	bool bIntersect = false;
	Vector vClosestPoint;
	for (int v = 1; v < numVert - 1; ++v )
	{
		vertIndex = host_state.worldbrush->vertindices[nFirstVertex+v];
		Vector v1 = host_state.worldbrush->vertexes[vertIndex].position;
		vertIndex = host_state.worldbrush->vertindices[nFirstVertex+v+1];
		Vector v2 = host_state.worldbrush->vertexes[vertIndex].position;

		Vector vIntersectPoint;
		if ( IsSphereIntersectForwardFacingTriangle( paintinfo->m_vPosition, plane, paintinfo->m_flSize, vOrigin, v1, v2, vIntersectPoint ) )
		{
			if ( !bIntersect )
			{
				vClosestPoint = vIntersectPoint;
			}
			else
			{
				Vector vecA = vClosestPoint - paintinfo->m_vPosition;
				Vector vecB = vIntersectPoint - paintinfo->m_vPosition;

				if ( DotProduct( vecB, vecB ) < DotProduct( vecA, vecA ) )
				{
					vClosestPoint = vIntersectPoint;
				}
			}

			bIntersect = true;
		}
	}

	if ( bIntersect )
	{
		Vector vDist = vClosestPoint - paintinfo->m_vPosition;
		float flDistFromClosestPointSquare = DotProduct( vDist, vDist );
		if ( flDistFromClosestPointSquare < paintinfo->m_flCurrentDistance )
		{
			paintinfo->m_flCurrentDistance = flDistFromClosestPointSquare;
			paintinfo->m_plane = plane;
		}

		paintinfo->m_aApplySurfs.AddToTail( surfID );
	}
}


//-----------------------------------------------------------------------------
// iterate over all surfaces on a model, looking for surfaces to paint
//-----------------------------------------------------------------------------
void R_PaintLeaf( mleaf_t *pLeaf, paintinfo_t *paintinfo )
{
	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];
	for ( int i = 0; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];

		R_AddPaintToSurface( surfID, paintinfo );
	}
}

void R_PaintNode( mnode_t *node, paintinfo_t* paintinfo )
{
	cplane_t	*splitplane;
	float		dist;

	if (!node )
		return;
	if ( node->contents >= 0 )
	{
		R_PaintLeaf( (mleaf_t *)node, paintinfo );
		return;
	}

	splitplane = node->plane;
	dist = DotProduct (paintinfo->m_vPosition, splitplane->normal) - splitplane->dist;

	if (dist > paintinfo->m_flSize)
	{
		R_PaintNode (node->children[0], paintinfo);
	}
	else if (dist < -paintinfo->m_flSize)
	{
		R_PaintNode (node->children[1], paintinfo);
	}
	else 
	{
		R_PaintNode (node->children[0], paintinfo);
		R_PaintNode (node->children[1], paintinfo);
	}
}


bool IsSurfaceInFrontOfPlane( SurfaceHandle_t surfID, const VPlane& plane )
{
	// check if the surface is on the same plane as the main plane
	VPlane trianglePlane = MSurf_GetForwardFacingPlane( surfID );
	if ( AlmostEqual( DotProduct( plane.m_Normal, trianglePlane.m_Normal ), 1.f ) && AlmostEqual( plane.m_Dist, trianglePlane.m_Dist) )
	{
		return true;
	}

	// if center of the first triangle is behind the main plane, ignore it.
	int numVert = MSurf_VertCount( surfID );
	if ( numVert < 3 )
	{
		return false;
	}

	int nFirstVertex = MSurf_FirstVertIndex( surfID );

	int vertIndex = host_state.worldbrush->vertindices[nFirstVertex];
	Vector vOrigin = host_state.worldbrush->vertexes[vertIndex].position;
	vertIndex = host_state.worldbrush->vertindices[nFirstVertex + 1];
	Vector v1 = host_state.worldbrush->vertexes[vertIndex].position;
	vertIndex = host_state.worldbrush->vertindices[nFirstVertex + 2];
	Vector v2 = host_state.worldbrush->vertexes[vertIndex].position;

	Vector vCenter = vOrigin + 0.25f * ( ( v1 - vOrigin ) + ( v2 - vOrigin ) );
	vCenter += 0.1f * trianglePlane.m_Normal; // push it a bit off the triangle plane to account for floating point error
	if ( plane.DistTo( vCenter ) < 0.f )
	{
		return false;
	}

	// debug paint rejection
	/*{
		CDebugOverlay::AddTriangleOverlay( vOrigin, v1, v2, 255, 0,0,128, true, 10.0f );
		CDebugOverlay::AddBoxOverlay( vCenter, Vector(-1,-1,-1), Vector(1,1,1), vec3_angle, 255, 255,0,128, 10.0f );
	}*/

	return true;
}


bool ShootPaintSphere( const model_t *pModel, const Vector& vPosition, BYTE colorIndex, float flSphereRadius, float flPaintCoatPercent )
{
	if ( !g_PaintManager.m_bShouldRegister || g_PaintManager.m_pPaintTextureDataArray == NULL )
	{
		return false;
	}

	Assert( pModel );
	if ( !pModel )
		return false; 

	paintinfo_t paintinfo;
	paintinfo.m_vPosition = vPosition;
	paintinfo.m_flSize = flSphereRadius;
	paintinfo.m_flCurrentDistance = FLT_MAX;
	paintinfo.m_pBrush = pModel->brush.pShared;
	paintinfo.m_aApplySurfs.RemoveAll();
	paintinfo.m_bPainting = true;

	mnode_t *pnodes = paintinfo.m_pBrush->nodes + pModel->brush.firstnode;
	R_PaintNode( pnodes, &paintinfo );

	//do the actual painting
	bool bChangedPaint = false;
	int numSurf = paintinfo.m_aApplySurfs.Count();
	for ( int i=0; i<numSurf; ++i )
	{
		// check if surface is behind main surface
		if ( IsSurfaceInFrontOfPlane( paintinfo.m_aApplySurfs[i], paintinfo.m_plane ) )
		{
			if ( R_PaintSurface( paintinfo.m_aApplySurfs[i], vPosition, flSphereRadius, paintinfo.m_plane, colorIndex, flPaintCoatPercent ) )
			{
				bChangedPaint = true;
			}
		}
	}
	return bChangedPaint;
}


//-----------------------------------------------------------------------------
// iterate over all surfaces on a model, to find painted surface at the collision point
//-----------------------------------------------------------------------------
void GetPaintColorFromSurface( SurfaceHandle_t surfID, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, const VPlane& basePlane, CUtlVector<BYTE>& surfColors )
{
	if ( (MSurf_Flags( surfID ) & SURFDRAW_PAINTED) == 0 )
		return;

	// only detects paint from surface that has similar normal
	if ( DotProduct( vContactNormal, MSurf_GetForwardFacingPlane( surfID ).m_Normal ) < 0.9f )
		return;


	int lightmapID = materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID;

	PaintRect_t paintRect;
	if ( ComputePaintRect( surfID, vPosition, flSphereRadius, paintRect ) )
	{
		g_PaintManager.m_pPaintTextureDataArray[ lightmapID ].GetPixels( paintRect.rect, surfColors );
	}
}


void TracePaintSphere( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, CUtlVector<BYTE>& surfColors )
{
	if ( !g_PaintManager.m_bShouldRegister || g_PaintManager.m_pPaintTextureDataArray == NULL )
	{
		return;
	}

	Assert( pModel );
	if ( !pModel )
		return;

	//clear paint
 	surfColors.RemoveAll();

	paintinfo_t paintinfo;
	paintinfo.m_vPosition = vPosition;
	paintinfo.m_flSize = flSphereRadius;
	paintinfo.m_flCurrentDistance = FLT_MAX;
	paintinfo.m_pBrush = pModel->brush.pShared;
	paintinfo.m_aApplySurfs.RemoveAll();
	paintinfo.m_bPainting = false;

	mnode_t *pnodes = paintinfo.m_pBrush->nodes + pModel->brush.firstnode;
	R_PaintNode( pnodes, &paintinfo );

	int numSurf = paintinfo.m_aApplySurfs.Count();
	for ( int i=0; i<numSurf; ++i )
	{
		GetPaintColorFromSurface( paintinfo.m_aApplySurfs[i], vPosition, vContactNormal, flSphereRadius, paintinfo.m_plane, surfColors );
	}
}


void R_RedownloadAllPaintmaps()
{
	if ( g_PaintManager.m_bShouldRegister )
	{
		for ( int i=0; i<g_PaintManager.m_iPaintmaps; ++i )
		{
			g_PaintManager.m_pPaintTextureDataArray[i].MarkAsDirty();
		}

		g_PaintManager.UpdatePaintmapTextures();
	}
}


#if 0
CON_COMMAND_F( dump_paintmaps, "dump paintmap data to \"paintmap_#.txt\"", FCVAR_CHEAT )
{
	for ( int i=0; i<g_PaintManager.m_iPaintmaps; ++i )
	{
		char filename[64];
		V_snprintf( filename, sizeof(filename), "paintmap_%i.txt", i );

		CUtlBuffer buf;
		const BYTE *pData = g_PaintManager.GetPaintmapData(i);
		int w,h;
		g_PaintManager.GetPaintmapSize( i, w, h );
		int size = w*h;
		for ( int b=0; b<size; ++b)
		{
			buf.PutChar( pData[b] );
		}

		g_pFullFileSystem->WriteFile( filename, NULL, buf );

		buf.Purge();
	}
}
#endif

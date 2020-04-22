//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================

// tier 1
#include "tier1/strtools.h"
#include "utlvector.h"
#include "mathlib/vmatrix.h"
#include "FileSystem.h"

#include "bitmap/tgaloader.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "pixelwriter.h"

#include "vguimaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystemutil.h"
#include "toolutils/enginetools_int.h"
#include "bitmap/imageformat.h"
#include "vtf/vtf.h"
#include "tier1/keyvalues.h"
#include "tier1/utllinkedlist.h"
#include "materialsystem/imesh.h"
#include <vstdlib/random.h>
#include "tier0/vprof.h"

#include "bitmap/psheet.h"
#include "materialsystem/imaterialvar.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>

#define MAX_LAYERS	10

enum 
{
	SUBLAYER_STATIC,
	SUBLAYER_DYNAMIC,
	SUBLAYER_FONT,
	SUBLAYER_MAX,
};

//-----------------------------------------------------------------------------
// Class to render a zillion boxes with a tga material.
//-----------------------------------------------------------------------------
class CTGARenderer : public ITGARenderer
{
public:
	void Init( int screenWidth, int screenHeight, int numTiles );
	void Render();
	void Shutdown();

private:

	struct UIQuadInfo
	{
	public:
		UIQuadInfo()
		{
			sublayer = -1;
			sheetSequenceNumber = -1;
			color.r = 255;
			color.g = 255;
			color.b = 255;
			color.a = 255;	

			textureName = "";
		}

		UIQuadInfo( int inXPos, int inYPos, int inWidth, int inHeight, int inSublayer, int inSheetSeqNo )
		{
			Assign( inXPos, inYPos, inWidth, inHeight, inSublayer, inSheetSeqNo, color );
		}

		UIQuadInfo( int inXPos, int inYPos, int inWidth, int inHeight, int inSublayer, int inSheetSeqNo, color32 inColor )
		{
			Assign( inXPos, inYPos, inWidth, inHeight, inSublayer, inSheetSeqNo, inColor );
		}

		void Assign( int inXPos, int inYPos, int inWidth, int inHeight, int inSublayer, int inSheetSeqNo, color32 inColor )
		{
			x1Pos = inXPos;
			y1Pos = inYPos;
			width = inWidth;
			height = inHeight;
			x2Pos = x1Pos + width;
			y2Pos = y1Pos + height;
			Assert( inSheetSeqNo != -1 ); 
			sublayer = inSublayer;
			sheetSequenceNumber = inSheetSeqNo;
			color = inColor;
		}

		int x1Pos;
		int y1Pos;
		int x2Pos;
		int y2Pos;
		int width;
		int height;
		CUtlString textureName; // name if the texture that is inside the sheet
		int sheetSequenceNumber;
		color32 color;
		int sublayer;
		
	};

	struct SheetInfo
	{
		CUtlVector< CUtlString > m_SheetTexEntry;
	};

	SheetInfo m_sheetDict[SUBLAYER_MAX];

	void AddSheetTextureEntry( int sublayer, const char *pTextureName );
	int FindSheetTextureEntry( int sublayer, const char *pTextureName );

	bool InitSheetTexture( int sublayer, const char *pBaseTextureName );
	CSheet *CTGARenderer::LoadSheet( IMaterial *pMaterial );
	CSheet *CTGARenderer::LoadSheet( char const *pszFname, ITexture *pTexture );
	void GenerateUIMesh( CMatRenderContextPtr &pRenderContext, IMesh* pMesh, CUtlLinkedList< UIQuadInfo > &quads, CSheet *pSheet );

	void InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, int layer, int sublayer, color32 color, const char *pTextureName = NULL );
	void InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, int layer, int sublayer, const char *pTextureName );
	void InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, int layer, int sublayer, int sheetSeqNo, color32 color );
	void InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, int layer, int sublayer, int sheetSeqNo );
	UIQuadInfo CreateQuad( int xPos, int yPos, int width, int height, int sublayer, const char *pTextureName = NULL );
	UIQuadInfo CreateQuad( int xPos, int yPos, int width, int height, int sublayer, color32 color, const char *pTextureName = NULL );
	void AddQuad( UIQuadInfo &quadInfo, int layer );

	struct LayerTextureInfo
	{
		CMaterialReference m_SublayerMaterial;
		CUtlReference< CSheet > m_Sheet;
	};
	
	LayerTextureInfo m_LayerTextureInfo[SUBLAYER_MAX];


	struct LayerInfo
	{
		CUtlLinkedList< UIQuadInfo > *pQuads;
	};

	// For 3 draw call per layer test.
	struct Layer
	{
	public:
		// Layer 0 is static
		// Layer 1 is dynamic
		// Layer 2 is font
		LayerInfo layerInfo[SUBLAYER_MAX]; 
	};

	// Test with 10 layers, 3 draw calls per layer.
	CUtlVector< Layer > m_Layers;

	bool m_bInitialized;	

};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CTGARenderer s_TGARenderer;
extern ITGARenderer *g_pTGARenderer = &s_TGARenderer;

// 3 drawcall one texture test.
void CTGARenderer::Init( int screenWidth, int screenHeight, int numTiles )
{
	m_bInitialized = false;

	// Build texture dictionary
	AddSheetTextureEntry( SUBLAYER_STATIC, "pixel.tga" );
	AddSheetTextureEntry( SUBLAYER_STATIC, "decalposter003a.tga" );
	AddSheetTextureEntry( SUBLAYER_STATIC, "decalgraffiti037a.tga" );
	AddSheetTextureEntry( SUBLAYER_STATIC, "Climb_node.tga" );
	AddSheetTextureEntry( SUBLAYER_STATIC, "gibshooter.tga" );
	AddSheetTextureEntry( SUBLAYER_STATIC, "ErrorIcon.tga" );;

	AddSheetTextureEntry( SUBLAYER_DYNAMIC, "glassclock001a.tga" );
	AddSheetTextureEntry( SUBLAYER_DYNAMIC, "decalgraffiti038a.tga" );
	AddSheetTextureEntry( SUBLAYER_DYNAMIC, "Air_node_hint.tga" );
	AddSheetTextureEntry( SUBLAYER_DYNAMIC, "info_lighting.tga" );
	AddSheetTextureEntry( SUBLAYER_DYNAMIC, "SelfIllumIcon.tga" );

	AddSheetTextureEntry( SUBLAYER_FONT, "glasswindow006a.tga" );
	AddSheetTextureEntry( SUBLAYER_FONT, "decalgraffiti043a.tga" );
	AddSheetTextureEntry( SUBLAYER_FONT, "Ground_node.tga" );
	AddSheetTextureEntry( SUBLAYER_FONT, "info_target.tga" );
	AddSheetTextureEntry( SUBLAYER_FONT, "translucenticon.tga" );

	// Load the sheet textures.
	bool bSuccess = InitSheetTexture( SUBLAYER_STATIC, "debug/sheettest/debug_meta1" );
	if ( !bSuccess )
	{
		Warning( "CDataModel: Unable to load sheet for %s\n", "debug/sheettest/debug_meta1" );
		return;
	}
	bSuccess = InitSheetTexture( SUBLAYER_DYNAMIC, "debug/sheettest/debug_meta2" );
	if ( !bSuccess )
	{
		Warning( "CDataModel: Unable to load sheet for %s\n", "debug/sheettest/debug_meta2" );
		return;
	}
	bSuccess = InitSheetTexture( SUBLAYER_FONT, "debug/sheettest/debug_meta3" );
	if ( !bSuccess )
	{
		Warning( "CDataModel: Unable to load sheet for %s\n", "debug/sheettest/debug_meta3" );
		return;
	}

	// Create an array of quads for each sublayer on each layer.
	m_Layers.RemoveAll();
	for ( int i = 0; i < MAX_LAYERS; ++i )
	{
		m_Layers.AddToTail();
		for ( int j = 0; j < SUBLAYER_MAX; ++j )
		{	
			m_Layers[i].layerInfo[j].pQuads = new CUtlLinkedList< UIQuadInfo >;
		}
	}


	UIQuadInfo testQuad;
	testQuad = CreateQuad( 0, 0, 64, 64, SUBLAYER_STATIC, "gibshooter.tga" );
	AddQuad( testQuad, 0 );


	testQuad = CreateQuad( 0, 128, 128, 128, SUBLAYER_DYNAMIC, "Air_node_hint.tga" );
	AddQuad( testQuad, 0 );


	m_bInitialized = true;
}


bool CTGARenderer::InitSheetTexture( int sublayer, const char *pBaseTextureName )
{
	CUtlString materialName;
	switch (sublayer)
	{
	case SUBLAYER_STATIC:
		materialName = "statictx";
		break;
	case SUBLAYER_DYNAMIC:
		materialName = "dynamictx";
		break;
	case SUBLAYER_FONT:
		materialName = "fonttx";
		break;
	default:
		Warning( "CTGARenderer: Invalid sublayer for %s\n", pBaseTextureName );
		Assert(0);
		break;
	};

	KeyValues *pVMTKeyValues = new KeyValues( "GameControls" );
	pVMTKeyValues->SetString( "$basetexture", pBaseTextureName );
	m_LayerTextureInfo[sublayer].m_SublayerMaterial.Init( materialName, TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_LayerTextureInfo[sublayer].m_SublayerMaterial->Refresh();
	CSheet *pSheet = LoadSheet( m_LayerTextureInfo[sublayer].m_SublayerMaterial );
	if ( pSheet == NULL )
	{
		Warning( "CTGARenderer: Unable to load sheet for %s %s\n", materialName.Get(), pBaseTextureName );
		return false;
	}
	else
	{
		m_LayerTextureInfo[sublayer].m_Sheet.Set( pSheet );
		return true;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, int layer, int sublayer, int sheetSeqNo, color32 color )
{

	CUtlLinkedList< UIQuadInfo > *pQuads = m_Layers[layer].layerInfo[sublayer].pQuads;

	for ( int i = 0; i < count; ++i )
	{
		UIQuadInfo quadInfo( xPos, yPos, width, height, sublayer, sheetSeqNo, color );
		pQuads->AddToTail( quadInfo );

		xPos += width;
		if ( xPos >= screenWidth )
		{
			xPos = 0;
			yPos += height;
		}

		if ( yPos >= screenHeight )
		{
			// just wrap.
			yPos = 0;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, int layer, int sublayer, int sheetSeqNo )
{
	for ( int i = 0; i < count; ++i )
	{
		UIQuadInfo quadInfo( xPos, yPos, width, height, sublayer, sheetSeqNo );
		AddQuad( quadInfo, layer );

		xPos += width;
		if ( xPos >= screenWidth )
		{
			xPos = 0;
			yPos += height;
		}

		if ( yPos >= screenHeight )
		{
			// just wrap.
			yPos = 0;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, 
							 int layer, int sublayer, color32 color, const char *pTextureName )
{
	int sheetSeqNo = FindSheetTextureEntry( sublayer, pTextureName );
	Assert( sheetSeqNo != -1 );
	InitTiles( screenWidth, screenHeight, xPos, yPos, width, height, count, layer, sublayer, sheetSeqNo, color );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::InitTiles( int screenWidth, int screenHeight, int &xPos, int &yPos, int width, int height, int count, 
							 int layer, int sublayer, const char *pTextureName )
{
	int sheetSeqNo = FindSheetTextureEntry( sublayer, pTextureName );
	InitTiles( screenWidth, screenHeight, xPos, yPos, width, height, count, layer, sublayer, sheetSeqNo );
}


//-----------------------------------------------------------------------------
// Create a quad on a sublayer
// Texture name is optional.
//-----------------------------------------------------------------------------
CTGARenderer::UIQuadInfo CTGARenderer::CreateQuad( int xPos, int yPos, int width, int height, int sublayer, const char *pTextureName )
{
	int sheetSeqNo = FindSheetTextureEntry( sublayer, pTextureName );
	UIQuadInfo quadInfo( xPos, yPos, width, height, sublayer, sheetSeqNo );

	return quadInfo;
}

//-----------------------------------------------------------------------------
// Create a quad on a sublayer
// Texture name is optional.
//-----------------------------------------------------------------------------
CTGARenderer::UIQuadInfo CTGARenderer::CreateQuad( int xPos, int yPos, int width, int height, int sublayer, color32 color, const char *pTextureName )
{
	int sheetSeqNo = FindSheetTextureEntry( sublayer, pTextureName );
	UIQuadInfo quadInfo( xPos, yPos, width, height, sublayer, sheetSeqNo, color );

	return quadInfo;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::AddQuad( UIQuadInfo &quadInfo, int layer )
{
	CUtlLinkedList< UIQuadInfo > *pQuads = m_Layers[layer].layerInfo[quadInfo.sublayer].pQuads;
	pQuads->AddToTail( quadInfo );	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::AddSheetTextureEntry( int sublayer, const char *pTextureName )
{
	m_sheetDict[sublayer].m_SheetTexEntry.AddToTail( pTextureName );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CTGARenderer::FindSheetTextureEntry( int sublayer, const char *pTextureName )
{
	if ( pTextureName == NULL )
		return 0;

	for ( int i = 0; i < m_sheetDict[sublayer].m_SheetTexEntry.Count(); ++i )
	{
		if ( !Q_strcmp( m_sheetDict[sublayer].m_SheetTexEntry[i], pTextureName ) )
			return i;
	}

	Assert(0);
	return -1;
}


#include "vgui/ISurface.h"
#include "vguimatsurface/imatsystemsurface.h"

//-----------------------------------------------------------------------------
// 3 draw calls per layer.
//-----------------------------------------------------------------------------
void CTGARenderer::Render()
{
	VPROF_BUDGET( "Render", "Render" );
	if ( !m_bInitialized )
	{
		Warning( "CTGARenderer: Unable to render.\n" );
		return;
	}

	int x, y, width, height;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
	pRenderContext->ClearBuffers( true, true );

	pRenderContext->GetViewport( x, y, width, height);

	float flPixelOffsetX = 0.5f;
	float flPixelOffsetY = 0.5f;

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	pRenderContext->Ortho( flPixelOffsetX, flPixelOffsetY, width + flPixelOffsetX, height + flPixelOffsetY, -1.0f, 1.0f ); 

	// make sure there is no translation and rotation laying around
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();


	// each sublayer should correspond to one meta texture binding.
	for ( int i = 0; i < MAX_LAYERS; ++i )
	{
		for ( int j = 0; j < SUBLAYER_MAX; ++j )
		{	
			if ( m_Layers[i].layerInfo[j].pQuads->Count() == 0 )
			{
				continue;
			}

			pRenderContext->Bind( m_LayerTextureInfo[j].m_SublayerMaterial, NULL );
			IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
			GenerateUIMesh( pRenderContext, pMesh, *(m_Layers[i].layerInfo[j].pQuads), m_LayerTextureInfo[j].m_Sheet );
			pMesh->Draw();
		}
	}

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::Shutdown()
{
	int count = 0;
	for ( int i = 0; i < MAX_LAYERS; ++i )
	{
		for ( int j = 0; j < SUBLAYER_MAX; j++ )
		{
			count += m_Layers[i].layerInfo[j].pQuads->Count();
		}
	}

	Warning( "Total generated quads = %d\n", count );
	m_Layers.RemoveAll();
}


//--------------------------------------------------------------------------------
// UI sheets
//--------------------------------------------------------------------------------
CSheet *CTGARenderer::LoadSheet( char const *pszFname, ITexture *pTexture )
{
	CSheet *pNewSheet = NULL;

	// get compact sheet representation held by texture
	size_t numBytes;
	void const *pSheet =  pTexture->GetResourceData( VTF_RSRC_SHEET, &numBytes );
	if ( pSheet )
	{
		// expand compact sheet into fatter runtime form
		CUtlBuffer bufLoad( pSheet, numBytes, CUtlBuffer::READ_ONLY );
		pNewSheet = new CSheet( bufLoad );
	}

	//m_SheetList[ pszFname ] = pNewSheet;
	return pNewSheet;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSheet *CTGARenderer::LoadSheet( IMaterial *pMaterial )
{
	if ( !pMaterial )
		return NULL;

	bool bFoundVar = false;
	IMaterialVar *pVar = pMaterial->FindVar( "$basetexture", &bFoundVar, true );

	if ( bFoundVar && pVar && pVar->IsDefined() )
	{
		ITexture *pTex = pVar->GetTextureValue();
		if ( pTex && !pTex->IsError() )
			return LoadSheet( pTex->GetName(), pTex );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTGARenderer::GenerateUIMesh( CMatRenderContextPtr &pRenderContext, IMesh* pMesh, CUtlLinkedList< UIQuadInfo > &quads, CSheet *pSheet )
{
	VPROF_BUDGET( "GenerateUIMesh", "GenerateUIMesh" );

	if ( quads.Count() == 0 )
		return;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, quads.Count() );

	int x, y, width, height;
	pRenderContext->GetViewport( x, y, width, height);

	{
		VPROF_BUDGET( "meshBuilder", "meshBuilder" );
		for( int i = quads.Head(); i != quads.InvalidIndex(); i = quads.Next( i ) )	
		{	
			Assert( pSheet );
			Assert( pSheet->m_SheetInfo[quads[i].sheetSequenceNumber].m_pSamples );
			SheetSequenceSample_t *pSample = pSheet->m_SheetInfo[quads[i].sheetSequenceNumber].m_pSamples;
			Assert( pSample );
			const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
			Assert( pSample0 );
			color32 c = quads[i].color;

			// Top left
			meshBuilder.Position3f( quads[i].x1Pos, quads[i].y1Pos, 0.0f );
			meshBuilder.Color4ub( c.r, c.g, c.b, c.a );
			meshBuilder.TexCoord3f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, 0 );
			meshBuilder.AdvanceVertex();

			// Top right
			meshBuilder.Position3f( quads[i].x2Pos, quads[i].y1Pos, 0.0f );
			meshBuilder.Color4ub( c.r, c.g, c.b, c.a );
			meshBuilder.TexCoord3f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0, 0 );
			meshBuilder.AdvanceVertex();

			// Bottom right
			meshBuilder.Position3f( quads[i].x2Pos, quads[i].y2Pos, 0.0f );
			meshBuilder.Color4ub( c.r, c.g, c.b, c.a );
			meshBuilder.TexCoord3f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0, 0 );
			meshBuilder.AdvanceVertex();

			// Bottom left
			meshBuilder.Position3f( quads[i].x1Pos, quads[i].y2Pos, 0.0f );
			meshBuilder.Color4ub( c.r, c.g, c.b, c.a );
			meshBuilder.TexCoord3f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0, 0 );
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
}







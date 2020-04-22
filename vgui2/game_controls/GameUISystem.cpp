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

#include "gameuisystem.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "gameuidefinition.h"
#include "gamelayer.h"
#include "tier0/vprof.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"
#include "gameuisystemsurface.h"
#include "inputgameui.h"
#include "gameuisystemmgr.h"
#include "gameuiscript.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_LAYERS	10

#define DEBUG_DRAW_REPORT	1
#define HIDE_ALL_TEXT	0
#define HIDE_FONT_TEXTURES 1


// A list of script handles exposed by the system
static int32 g_iSerialHandle = 0;
static CUtlMap< int32, CGameUISystem * > g_mapScriptHandles( DefLessFunc( int32 ) );


CGameUISystem::CGameUISystem() :
	m_GameUIDef( this ),
	m_iScriptHandle( ++g_iSerialHandle )
{
	m_bDrawReport = true;
	g_mapScriptHandles.InsertOrReplace( m_iScriptHandle, this );
}

CGameUISystem::~CGameUISystem()
{
	g_mapScriptHandles.Remove( m_iScriptHandle );
}

CGameUISystem * CGameUISystem::FromScriptHandle( int32 iScriptHandle )
{
	unsigned short usIdx = g_mapScriptHandles.Find( iScriptHandle );
	return ( usIdx == g_mapScriptHandles.InvalidIndex() ) ? NULL : g_mapScriptHandles.Element( usIdx );
}

char const * CGameUISystem::GetName()
{
	return m_GameUIDef.GetName();
}

//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CGameUISystem::Init( KeyValues *kvLoadSettings )
{
	DevMsg( "CGameUISystem[%p]::Init( name = %s )\n", this, GetName() );
	KeyValuesDumpAsDevMsg( kvLoadSettings );

	return true;
}

void CGameUISystem::Release()
{
	DevMsg( "CGameUISystem[%p]::Release( name = %s )\n", this, GetName() );

	g_pGameUISystemMgrImpl->OnScreenReleased( this );

	m_GameUIDef.Shutdown();

	delete this;
}

//-----------------------------------------------------------------------------
// Creates an empty game UI.
//-----------------------------------------------------------------------------
void CGameUISystem::LoadEmptyGameUI( const char *pName )
{
	m_GameUIDef.Shutdown();
	m_GameUIDef.CreateDefault( pName );
}

//-----------------------------------------------------------------------------
// Read the game UI config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CGameUISystem::LoadGameUIDefinition( CUtlBuffer &buf, const char *pFileName )
{
	DECLARE_DMX_CONTEXT();

	CDmxElement *pRoot = NULL;
	if ( !UnserializeDMX( buf, &pRoot, pFileName ) )
	{
		Warning( "Unable to read game UI config %s! UtlBuffer is the wrong type!\n", pFileName );
		return false;
	}

	bool bOk = m_GameUIDef.Unserialize( pRoot );
	CleanupDMX( pRoot );
	if ( !bOk )
		return false;

	m_GameUIDef.InitializeScripts();

	// Start the animations all at the same time.
	//Start();	

	//TextTest();
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CGameUISystem::ExecuteScript( KeyValues *kvEvent, KeyValues **ppResult )
{
	return m_GameUIDef.ExecuteScript( kvEvent, ppResult );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystem::SetStageSize( int nWide, int nTall )
{ 
	m_GameUIDef.SetStageSize( nWide, nTall); 
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystem::GetStageSize( Vector2D &stageSize ) 
{ 
	m_GameUIDef.GetStageSize( stageSize ); 
}



//-----------------------------------------------------------------------------
// 3 draw calls per layer.
// Render in source 1
//-----------------------------------------------------------------------------
void CGameUISystem::Render( const Rect_t &viewport )
{
	if ( !m_GameUIDef.GetVisible() )
		return;

	VPROF_BUDGET( "Render", "Render" );
	Assert( g_pMaterialSystem );

	m_GameUIDef.UpdateGeometry();
	m_GameUIDef.UpdateRenderTransforms( viewport );
	CUtlVector< LayerRenderLists_t > renderLists;
	m_GameUIDef.GetRenderData( renderLists );


	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Clear back buffer to green. Useful for debugging graphics that you think should be there and are not.
	//pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
	//pRenderContext->ClearBuffers( true, true );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	float flPixelOffsetX = .5;
	float flPixelOffsetY = .5;
	pRenderContext->Ortho( viewport.x + flPixelOffsetX, viewport.y + flPixelOffsetY, viewport.width  + flPixelOffsetX, viewport.height + flPixelOffsetY, -1.0f, 1.0f ); 

	// make sure there is no translation and rotation laying around
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	
	int nStaticDrawCalls = 0;
	int nDynamicDrawCalls = 0;
	int nFontDrawCalls = 0;

	int nLists = renderLists.Count();
	for ( int i = 0; i < nLists; ++i )
	{	
		if ( renderLists[i].m_LayerType == SUBLAYER_STATIC )
		{
			for ( int j = 0; j < renderLists[i].m_RenderGeometryLists.Count(); ++j )
			{
				RenderStaticLayer( renderLists[i], j );
				nStaticDrawCalls++;
			}	

		}
		else if ( renderLists[i].m_LayerType == SUBLAYER_DYNAMIC )
		{
			// For dynamic texture viewing.
			//int x = 900;
			//int y = 0;
			for ( int j = 0; j < renderLists[i].m_RenderGeometryLists.Count(); ++j )
			{
				RenderDynamicLayer( renderLists[i], j );
				nDynamicDrawCalls++;

				// For dynamic texture viewing.
				//g_pGameUISystemMgrImpl->DrawDynamicTexture( renderLists[i].m_RenderGeometryLists[j][0].m_pImageAlias, x, y );
				//y += 256 + 30;
			}
		}
		else if ( renderLists[i].m_LayerType == SUBLAYER_FONT )
		{
			// For font texture viewing.
			//int x = 900;
			//int y = 30;
			for ( int j = 0; j < renderLists[i].m_RenderGeometryLists.Count(); ++j )
			{
				RenderTextLayer( renderLists[i].m_RenderGeometryLists[j] );
				nFontDrawCalls++;

				// For font texture viewing.
				//g_pGameUISystemSurface->DrawFontTexture( renderLists[i].m_RenderGeometryLists[j][0].m_FontTextureID, x, y );
				//y += 256 + 30;
			}
		}
	}


#if ( DEBUG_DRAW_REPORT )
	if ( m_bDrawReport )
	{
		m_bDrawReport = false;
		
		Msg( "Total static draw calls in UI: %d\n", nStaticDrawCalls );
		Msg( "Total dynamic draw calls in UI: %d\n", nDynamicDrawCalls );
		Msg( "Total font draw calls in UI: %d\n", nFontDrawCalls );
		Msg( "Total draw calls in UI: %d\n",  nStaticDrawCalls + nDynamicDrawCalls + nFontDrawCalls );
	}
#endif

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
}


//-----------------------------------------------------------------------------
// Render a static layer in source 1
//-----------------------------------------------------------------------------
void CGameUISystem::RenderStaticLayer( LayerRenderLists_t &renderList, int geometryIndex )
{
	// Do not call draw on an empty mesh.
	int nTotalTriCount = 0;
	for( int i = 0; i < renderList.m_RenderGeometryLists[geometryIndex].Count(); ++i )	
	{
		nTotalTriCount += renderList.m_RenderGeometryLists[geometryIndex][i].GetTriangleCount();
	}

	if ( nTotalTriCount == 0 )
		return;

	if ( renderList.m_pSheet == NULL )
	{
		Assert(0);
		return;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( renderList.m_pMaterial, NULL ); 
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	GenerateUIMesh( pRenderContext, pMesh, renderList.m_RenderGeometryLists[geometryIndex], 
		renderList.m_pSheet );
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Render a dynamic layer in source 1
//-----------------------------------------------------------------------------
void CGameUISystem::RenderDynamicLayer( LayerRenderLists_t &renderList, int geometryIndex )
{
	// Do not call draw on an empty mesh.
	int nTotalTriCount = 0;
	for( int i = 0; i < renderList.m_RenderGeometryLists[geometryIndex].Count(); ++i )	
	{
		nTotalTriCount += renderList.m_RenderGeometryLists[geometryIndex][i].GetTriangleCount();
	}

	if ( nTotalTriCount == 0 )
		return;

	if ( renderList.m_pMaterial == NULL )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, renderList.m_pMaterial );   // this fxn will also bind the material
	if ( !pMesh )
		return;

	
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nTotalTriCount );									  

	CUtlVector< CRenderGeometry > &renderGeometry = renderList.m_RenderGeometryLists[geometryIndex];
	int nGraphicCount = renderGeometry.Count();
	int nIndex = 0;
	for( int i = 0; i < nGraphicCount; ++i )	
	{	
		for ( int j = 0; j < renderGeometry[i].m_Positions.Count(); ++j )
		{
			// First anim frame
			float flX = renderGeometry[i].m_Positions[j].x;
			float flY = renderGeometry[i].m_Positions[j].y;
			meshBuilder.Position3f( flX, flY, 0.0f );
			color32 c = renderGeometry[i].m_VertexColors[j];
			meshBuilder.Color4ub( c.r, c.g, c.b, c.a );

			float texCoordX = renderGeometry[i].m_TextureCoords[j].x;
			float texCoordY = renderGeometry[i].m_TextureCoords[j].y;
			
			meshBuilder.TexCoord3f( 0, texCoordX, texCoordY, 0 );
			meshBuilder.AdvanceVertex();
		}

		Assert( renderGeometry[i].m_Positions.Count() == 4 );

		
		// FIXME make this work with generic convex shapes.
		// Quads only.
		meshBuilder.FastIndex( nIndex );
		meshBuilder.FastIndex( nIndex + 1 );
		meshBuilder.FastIndex( nIndex + 2 );

		meshBuilder.FastIndex( nIndex );
		meshBuilder.FastIndex( nIndex + 2 );
		meshBuilder.FastIndex( nIndex + 3 );

		nIndex += (4);	
	}
	

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Render a font layer in source 1
//-----------------------------------------------------------------------------
void CGameUISystem::RenderTextLayer( CUtlVector< CRenderGeometry > &renderGeometry )
{
	if ( renderGeometry.Count() == 0 )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	// get the character texture from the cache 
	IMaterial *pMaterial = g_pGameUISystemSurface->GetMaterial( renderGeometry[0].m_FontTextureID );   /// Everything in a text rendering layer uses the same font texture, and the texture id is in the seq #
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );   // this fxn will also bind the material
	if ( !pMesh )
		return;
	
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, renderGeometry.Count() );

	for ( int i = 0; i < renderGeometry.Count(); ++i )
	{
		Assert( renderGeometry[i].m_Positions.Count() == 4 );
		for ( int j = 0; j < renderGeometry[i].m_Positions.Count(); ++j )
		{
			meshBuilder.Position3f( renderGeometry[i].m_Positions[j].x, renderGeometry[i].m_Positions[j].y, 0.0f );
			meshBuilder.Color4ub( renderGeometry[i].m_VertexColors[j].r, renderGeometry[i].m_VertexColors[j].g, renderGeometry[i].m_VertexColors[j].b, renderGeometry[i].m_VertexColors[j].a );
			meshBuilder.TexCoord3f( 0, renderGeometry[i].m_TextureCoords[j].x, renderGeometry[i].m_TextureCoords[j].y, 0 );
			meshBuilder.AdvanceVertex();
		}	
	}

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Create geometry mesh in source 1
//-----------------------------------------------------------------------------
void CGameUISystem::GenerateUIMesh( IMatRenderContext *pRenderContext, 
								   IMesh* pMesh, 
								   CUtlVector< CRenderGeometry > &renderGeometry,
								   CSheet *pSheet )
{
	int nTotalTriCount = 0;
	for( int i = 0; i < renderGeometry.Count(); ++i )	
	{
		nTotalTriCount += renderGeometry[i].GetTriangleCount();
	}

	Assert( nTotalTriCount != 0 );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nTotalTriCount * 2 );

	int x, y, width, height;
	pRenderContext->GetViewport( x, y, width, height);									  

	static long flAge = 0;
	{
		VPROF_BUDGET( "meshBuilder", "meshBuilder" );
		 
		int nGraphicCount = renderGeometry.Count();
		int nIndex = 0;
		for( int i = 0; i < nGraphicCount; ++i )	
		{	
			const SheetSequenceSample_t *pSample = NULL;
			int seqNum = renderGeometry[i].m_SheetSequenceNumber;
			if ( renderGeometry[i].m_bAnimate )
			{
				DmeTime_t flStartTime = renderGeometry[i].GetAnimStartTime();
				DmeTime_t flAgeInSeconds = ( g_pGameUISystemMgrImpl->GetTime() - flStartTime ); 
				float m_flAnimationRate = renderGeometry[i].m_AnimationRate;
				float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
				flAgeScale /= pSheet->m_SheetInfo[seqNum].m_flFrameSpan;
				
				pSample = pSheet->GetSampleForSequence( flAgeInSeconds.GetSeconds(), flAgeScale, seqNum, true );
			}
			else
			{
				pSample = pSheet->m_SheetInfo[seqNum].m_pSamples;
			}

			Assert( pSample );
			const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
			Assert( pSample0 );

			for ( int j = 0; j < renderGeometry[i].m_Positions.Count(); ++j )
			{
				// First anim frame
				float flX = renderGeometry[i].m_Positions[j].x;
				float flY = renderGeometry[i].m_Positions[j].y;
				meshBuilder.Position3f( flX, flY, 0.0f );
				color32 c = renderGeometry[i].m_VertexColors[j];
				c.a *= ( 1 - pSample->m_fBlendFactor );
				meshBuilder.Color4ub( c.r, c.g, c.b, c.a );
				float sampleWidth = pSample0->m_fRight_U0 - pSample0->m_fLeft_U0;
				float sampleHeight = pSample0->m_fBottom_V0 - pSample0->m_fTop_V0;
				float texCoordX = pSample0->m_fLeft_U0 + renderGeometry[i].m_TextureCoords[j].x * sampleWidth;
				float texCoordY = pSample0->m_fTop_V0 + renderGeometry[i].m_TextureCoords[j].y * sampleHeight;
				meshBuilder.TexCoord3f( 0, texCoordX, texCoordY, 0 );
				meshBuilder.AdvanceVertex();

				// Ugh, right now we have to do this second frame because of the total triangle count being set before we figure out if we need it.
				// Second anim frame
				//if ( pSample->m_fBlendFactor < 1.0 )
				{
					meshBuilder.Position3f( flX, flY, 0.0f );
					c = renderGeometry[i].m_VertexColors[j];
					c.a *= ( pSample->m_fBlendFactor );
					meshBuilder.Color4ub( c.r, c.g, c.b, c.a );
					float texCoordX = pSample0->m_fLeft_U1 + renderGeometry[i].m_TextureCoords[j].x * sampleWidth;
					float texCoordY = pSample0->m_fTop_V1 + renderGeometry[i].m_TextureCoords[j].y * sampleHeight;
					meshBuilder.TexCoord3f( 0, texCoordX, texCoordY, 0 );
					meshBuilder.AdvanceVertex();
				}

			}

			//if ( pSample->m_fBlendFactor < 1.0 )
			{
				// FIME make this work with generic convex shapes.
				// Quads only.
				meshBuilder.FastIndex( nIndex );
				meshBuilder.FastIndex( nIndex + 2 );
				meshBuilder.FastIndex( nIndex + 4 );

				meshBuilder.FastIndex( nIndex );
				meshBuilder.FastIndex( nIndex + 4 );
				meshBuilder.FastIndex( nIndex + 6 );

				meshBuilder.FastIndex( nIndex + 1 );
				meshBuilder.FastIndex( nIndex + 3 );
				meshBuilder.FastIndex( nIndex + 5 );

				meshBuilder.FastIndex( nIndex + 1 );
				meshBuilder.FastIndex( nIndex + 5 );
				meshBuilder.FastIndex( nIndex + 7 );

				nIndex += (4 * 2);
			}
			/*
			else
			{
				meshBuilder.FastIndex( nIndex );
				meshBuilder.FastIndex( nIndex + 1 );
				meshBuilder.FastIndex( nIndex + 2 );

				meshBuilder.FastIndex( nIndex );
				meshBuilder.FastIndex( nIndex + 2 );
				meshBuilder.FastIndex( nIndex + 3 );
				nIndex += (4 * 1);
			}
			*/	
		}
	}

	meshBuilder.End();
}


//-----------------------------------------------------------------------------
// 3 draw calls per layer.
// Render the UI in source 2
//-----------------------------------------------------------------------------
void CGameUISystem::Render( IRenderContext *pRenderContext, const Rect_t &viewport )
{
	if ( !m_GameUIDef.GetVisible() )
		return;

	m_GameUIDef.UpdateGeometry();
	m_GameUIDef.UpdateRenderTransforms( viewport );

	CUtlVector< LayerRenderLists_t > renderLists;
	m_GameUIDef.GetRenderData( renderLists );

	// Note this is not scaling correctly!

	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
	pRenderContext->SetBlendMode( RENDER_BLEND_ALPHABLENDING );

	pRenderContext->BindVertexShader( g_pGameUISystemMgrImpl->m_hVertexShader, g_pGameUISystemMgrImpl->m_hInputLayout );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, g_pGameUISystemMgrImpl->m_hPixelShader );

	float pViewportInfo[4] = { viewport.x + 0.5f, viewport.y + 0.5f, viewport.width, viewport.height };
	pRenderContext->SetConstantBufferData( g_pGameUISystemMgrImpl->m_hConstBuffer, pViewportInfo, 4 * sizeof( float ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, g_pGameUISystemMgrImpl->m_hConstBuffer, 0, 0 );

	int nLists = renderLists.Count();
	for ( int i = 0; i < nLists; ++i )
	{	
		if ( renderLists[i].m_LayerType == SUBLAYER_STATIC )
		{
			for ( int j = 0; j < renderLists[i].m_RenderGeometryLists.Count(); ++j )
			{
				RenderStaticLayer( pRenderContext, renderLists[i], j );
			}	
		}
		else if ( renderLists[i].m_LayerType == SUBLAYER_FONT )
		{
			for ( int j = 0; j < renderLists[i].m_RenderGeometryLists.Count(); ++j )
			{
				RenderTextLayer( pRenderContext, renderLists[i].m_RenderGeometryLists[j] );
			}				
		}
	}
}

//-----------------------------------------------------------------------------
// Render a font layer in source 2
//-----------------------------------------------------------------------------
void CGameUISystem::RenderTextLayer( IRenderContext *pRenderContext, CUtlVector< CRenderGeometry > &renderGeometry )
{
	if ( renderGeometry.Count() == 0 )
		return;
	
	// get the character texture from the cache 
	HRenderTexture fontTextureHandle = g_pGameUISystemSurface->GetTextureHandle( renderGeometry[0].m_FontTextureID );
	pRenderContext->BindTexture( 0, fontTextureHandle );

	CDynamicVertexData< GameUIVertex_t > vb( pRenderContext, renderGeometry.Count() * 4, "gamelayer", "game_controls" );
	vb.Lock();

	for ( int i = 0; i < renderGeometry.Count(); ++i )
	{
		Assert( renderGeometry[i].m_Positions.Count() == 4 );
		for ( int j = 0; j < renderGeometry[i].m_Positions.Count(); ++j )
		{
			vb->m_vecPosition.Init( renderGeometry[i].m_Positions[j].x, renderGeometry[i].m_Positions[j].y, 0.0f );
			vb->m_color = renderGeometry[i].m_VertexColors[j];
			vb->m_vecTexCoord.Init( renderGeometry[i].m_TextureCoords[j].x, renderGeometry[i].m_TextureCoords[j].y );
			vb.AdvanceVertex();
		}
	}

	vb.Unlock();
	vb.Bind( 0, 0 );

	CDynamicIndexData< uint16 > ib( pRenderContext, renderGeometry.Count() * 6, "gamelayer", "game_controls" );
	ib.Lock();
	int nIndex = 0;
	for( int i = 0; i < renderGeometry.Count(); ++i )	
	{
		ib.Index( nIndex );
		ib.Index( nIndex + 1 );
		ib.Index( nIndex + 2 );

		ib.Index( nIndex );
		ib.Index( nIndex + 2 );
		ib.Index( nIndex + 3 );
			
		nIndex += 4;
	}
	ib.Unlock();
	ib.Bind( 0 );

	pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, renderGeometry.Count() * 6 );

	// For debugging.
	//int x = 300;
	//int y = 300;
	//g_pGameUISystemSurface->DrawFontTexture( pRenderContext, renderGeometry[0].m_FontTextureID, x, y );
	//x += 256;
}

//-----------------------------------------------------------------------------
// Renders the static layer in source 2
//-----------------------------------------------------------------------------
void CGameUISystem::RenderStaticLayer( IRenderContext *pRenderContext, LayerRenderLists_t &renderList, int geometryIndex )
{
	int nGraphicCount = renderList.m_RenderGeometryLists[geometryIndex].Count();
	if ( !nGraphicCount )
		return;

	int nTotalIndexCount = 0;
	int nTotalVertexCount = 0;
	int nGeometryCount = renderList.m_RenderGeometryLists[geometryIndex].Count();
	for( int i = 0; i < nGeometryCount; ++i )	
	{
		nTotalIndexCount += renderList.m_RenderGeometryLists[geometryIndex][i].GetTriangleCount() * 3;
		nTotalVertexCount += renderList.m_RenderGeometryLists[geometryIndex][i].GetVertexCount();
	}
	if ( nTotalIndexCount == 0 )
		return;

	pRenderContext->BindTexture( 0, renderList.m_hTexture );

	CDynamicVertexData< GameUIVertex_t > vb( pRenderContext, nTotalVertexCount * 2, "gamelayer", "game_controls" );
	vb.Lock();
	for( int i = 0; i < nGeometryCount; ++i )	
	{
		CRenderGeometry *pGeometry = &renderList.m_RenderGeometryLists[geometryIndex][i];

		const SheetSequenceSample_t *pSample = NULL;
		int seqNum = pGeometry->m_SheetSequenceNumber;
		if ( pGeometry->m_bAnimate )
		{
			DmeTime_t flStartTime = pGeometry->GetAnimStartTime();
			DmeTime_t flAgeInSeconds = ( g_pGameUISystemMgrImpl->GetTime() - flStartTime ); 
			float m_flAnimationRate = pGeometry->m_AnimationRate;
			float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
			flAgeScale /= renderList.m_pSheet->m_SheetInfo[seqNum].m_flFrameSpan;

			pSample = renderList.m_pSheet->GetSampleForSequence( flAgeInSeconds.GetSeconds(), flAgeScale, seqNum, true );
		}
		else
		{
			pSample = renderList.m_pSheet->m_SheetInfo[seqNum].m_pSamples;
		}

		Assert( pSample );
		const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
		Assert( pSample0 );

		for ( int j = 0; j < pGeometry->m_Positions.Count(); ++j )
		{
			// First anim frame
			float flX = pGeometry->m_Positions[j].x;
			float flY = pGeometry->m_Positions[j].y;
			vb->m_vecPosition.Init( flX, flY, 0.0f );
			color32 c = pGeometry->m_VertexColors[j];
			c.a *= ( 1 - pSample->m_fBlendFactor );
			vb->m_color = c;
			float sampleWidth = pSample0->m_fRight_U0 - pSample0->m_fLeft_U0;
			float sampleHeight = pSample0->m_fBottom_V0 - pSample0->m_fTop_V0;
			float texCoordX = pSample0->m_fLeft_U0 + pGeometry->m_TextureCoords[j].x * sampleWidth;
			float texCoordY = pSample0->m_fTop_V0 + pGeometry->m_TextureCoords[j].y * sampleHeight;
			vb->m_vecTexCoord.Init( texCoordX, texCoordY );
			vb.AdvanceVertex();

			// Second anim frame
			//if ( pSample->m_fBlendFactor < 1.0 )
			{
				c = pGeometry->m_VertexColors[j];
				c.a *= ( pSample->m_fBlendFactor );
				vb->m_color = c;
				float texCoordX = pSample0->m_fLeft_U1 + pGeometry->m_TextureCoords[j].x * sampleWidth;
				float texCoordY = pSample0->m_fTop_V1 + pGeometry->m_TextureCoords[j].y * sampleHeight;
				vb->m_vecTexCoord.Init( texCoordX, texCoordY );
				vb.AdvanceVertex();
			}
		}
	}
	vb.Unlock();
	vb.Bind( 0, 0 );

	CDynamicIndexData< uint16 > ib( pRenderContext, nTotalIndexCount * 2, "gamelayer", "game_controls" );
	ib.Lock();
	int nIndex = 0;
	for( int i = 0; i < nGeometryCount; ++i )	
	{
		ib.Index( nIndex );
		ib.Index( nIndex + 2 );
		ib.Index( nIndex + 4 );

		ib.Index( nIndex );
		ib.Index( nIndex + 4 );
		ib.Index( nIndex + 6 );

		ib.Index( nIndex + 1 );
		ib.Index( nIndex + 3 );
		ib.Index( nIndex + 5 );

		ib.Index( nIndex + 1 );
		ib.Index( nIndex + 5 );
		ib.Index( nIndex + 7 );
			
		nIndex += (4 * 2);
		

		/*
		// FIXME: Deal with sometimes only rendering 1 triangle above
		CGeometry *pGeometry = geometry[i];
		int nTriangleCount = pGeometry->m_Triangles.Count();
		for ( int j = 0; j < nTriangleCount; ++j )
		{
			CTriangle *pTriangle = &pGeometry->m_Triangles[j];
			//if ( pSample->m_fBlendFactor < 1.0 )
			{
				ib.Index( nIndex + pTriangle->m_PointIndex[0] * 2 );
				ib.Index( nIndex + pTriangle->m_PointIndex[1] * 2 );
				ib.Index( nIndex + pTriangle->m_PointIndex[2] * 2 );

				ib.Index( nIndex + pTriangle->m_PointIndex[0] * 2 + 1 );
				ib.Index( nIndex + pTriangle->m_PointIndex[1] * 2 + 1 );
				ib.Index( nIndex + pTriangle->m_PointIndex[2] * 2 + 1 );
			}
			else
			{
			//ib.Index( nIndex + pTriangle->m_PointIndex[0] );
			//ib.Index( nIndex + pTriangle->m_PointIndex[1] );
			//ib.Index( nIndex + pTriangle->m_PointIndex[2] );
			//nIndex += 3;
			}
			
		}
		nIndex += (3 * 2) * nTriangleCount;
		*/
		
	}
	ib.Unlock();
	ib.Bind( 0 );

	pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, nTotalIndexCount * 2 );
}

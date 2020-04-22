//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "smoke_fog_overlay.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "view.h"
#include "precache_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static IMaterial *g_pSmokeFogMaterial = NULL;


float		g_SmokeFogOverlayAlpha;
Vector		g_SmokeFogOverlayColor;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheSmokeFogOverlay )
PRECACHE( MATERIAL, "particle/screenspace_fog" )
PRECACHE_REGISTER_END()

void InitSmokeFogOverlay()
{
	TermSmokeFogOverlay();
	
	g_SmokeFogOverlayAlpha = 0;
	g_SmokeFogOverlayColor.Init( 0.0f, 0.0f, 0.0f );

	if(materials)
	{
		g_pSmokeFogMaterial = materials->FindMaterial( "particle/screenspace_fog", TEXTURE_GROUP_CLIENT_EFFECTS );
		if(g_pSmokeFogMaterial)
			g_pSmokeFogMaterial->IncrementReferenceCount();
	}
}


void TermSmokeFogOverlay()
{
	if(g_pSmokeFogMaterial)
	{
		g_pSmokeFogMaterial->DecrementReferenceCount();
		g_pSmokeFogMaterial = NULL;
	}
}


void DrawSmokeFogOverlay()
{
	if(g_SmokeFogOverlayAlpha == 0 || !g_pSmokeFogMaterial || !materials)
		return;
	
	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "DrawSmokeFogOverlay()" );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 0, 0, 1, 1, -99999, 99999 );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	IMesh* pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, g_pSmokeFogMaterial );
	CMeshBuilder meshBuilder;

	static float dist = 10;

	// g_SmokeFogOverlayColor is the sum total of all the smoke colors weighted by their alpha contribution and added.
	// Dividing by the g_SmokeFogOverlayAlpha gives us the weighted average color of all the smoke plumes that are influencing
	// the fog overlay. 
	Vector vColor = g_SmokeFogOverlayColor/* / g_SmokeFogOverlayAlpha*/;

	vColor.x = MIN(MAX(vColor.x, 0), 1);
	vColor.y = MIN(MAX(vColor.y, 0), 1);
	vColor.z = MIN(MAX(vColor.z, 0), 1);
	float alpha = MIN(MAX(g_SmokeFogOverlayAlpha, 0), 1);

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( 0, 0, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0, 1, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1, 1, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1, 0, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

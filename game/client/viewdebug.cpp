//========== Copyright (c) 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"

#include "tier0/vprof.h"

#include "view_scene.h"
#include "viewrender.h"
#include "viewdebug.h"
#include "view.h"
#include "smoke_fog_overlay.h"
#include "materialsystem/imaterialvar.h"
#include "foundryhelpers_client.h"
#include "c_env_cascade_light.h"
#include "materialsystem/icustommaterialmanager.h"

#ifdef PORTAL
//#include "C_Portal_Player.h"
#include "portal_render_targets.h"
#include "portalrender.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// debugging overlays
//-----------------------------------------------------------------------------
static ConVar cl_drawmaterial( "cl_drawmaterial", "", FCVAR_CHEAT, "Draw a particular material over the frame" );
static ConVar mat_showwatertextures( "mat_showwatertextures", "0", FCVAR_CHEAT );
static ConVar mat_wateroverlaysize( "mat_wateroverlaysize", "128" );
static ConVar mat_showframebuffertexture( "mat_showframebuffertexture", "0", FCVAR_CHEAT );
static ConVar mat_framebuffercopyoverlaysize( "mat_framebuffercopyoverlaysize", "128" );
static ConVar mat_showcamerarendertarget( "mat_showcamerarendertarget", "0", FCVAR_CHEAT );
static ConVar mat_camerarendertargetoverlaysize( "mat_camerarendertargetoverlaysize", "128", FCVAR_CHEAT );
static ConVar mat_hsv( "mat_hsv", "0", FCVAR_CHEAT );
static ConVar mat_yuv( "mat_yuv", "0", FCVAR_CHEAT );
static ConVar cl_overdraw_test( "cl_overdraw_test", "0", FCVAR_CHEAT | FCVAR_NEVER_AS_STRING );
static ConVar mat_drawTexture( "mat_drawTexture", "", 0, "Enable debug view texture" );
static ConVar mat_drawTextureScale( "mat_drawTextureScale", "1.0", 0, "Debug view texture scale" );
#ifdef _GAMECONSOLE
static ConVar mat_drawColorRamp( "mat_drawColorRamp", "0", 0, "Draw color test pattern (0=Off, 1=[0..255], 2=[0..127]" );
#endif

//-----------------------------------------------------------------------------
// debugging
//-----------------------------------------------------------------------------
// (the engine owns this cvar).
ConVar mat_wireframe( "mat_wireframe", "0", FCVAR_CHEAT );
const ConVar *sv_cheats = NULL;
ConVar	mat_showlightmappage(  "mat_showlightmappage", "-1" ); // set this to the lightmap page that you want to see on screen, set to -1 to show nothing.
ConVar cl_drawshadowtexture( "cl_drawshadowtexture", "0", FCVAR_CHEAT );
extern ConVar cl_csm_debug_2d;
extern ConVar cl_csm_debug_3d;
ConVar cl_shadowtextureoverlaysize( "cl_shadowtextureoverlaysize", "256", FCVAR_CHEAT );

static ConVar r_flashlightdrawdepth( "r_flashlightdrawdepth", "0" );
static ConVar cl_custommaterial_debug_graph( "cl_custommaterial_debug_graph", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Lightmap debugging mode view
//-----------------------------------------------------------------------------
class CLightmapDebugView : public CRendering3dView
{
public:
	explicit CLightmapDebugView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	void Draw()
	{
		extern bool s_bCanAccessCurrentView;
		AllowCurrentViewAccess( true );
		Frustum frustum;
		CMatRenderContextPtr pRenderContext( materials );
		render->Push3DView( pRenderContext, *this, 0, NULL, frustum );
		BuildWorldRenderLists( true, -1, true, true );
		render->PopView( pRenderContext, frustum );
		AllowCurrentViewAccess( false );

		render->DrawLightmaps( m_pWorldRenderList, mat_showlightmappage.GetInt() );
	}


};


//-----------------------------------------------------------------------------
// Renders a material orthographically to screen...
//-----------------------------------------------------------------------------
static void RenderMaterial( const char *pMaterialName )
{
	// So it's not in the very top left
	float x = 100.0f, y = 100.0f;
	// float x = 0.0f, y = 0.0f;

	IMaterial *pMaterial = materials->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER, false );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( x, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x + pMaterial->GetMappingWidth(), y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x + pMaterial->GetMappingWidth(), y + pMaterial->GetMappingHeight(), 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x, y + pMaterial->GetMappingHeight(), 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

static void OverlayWaterTexture( IMaterial *pMaterial, int xOffset, int yOffset, bool bFlip )
{
	// screen safe
	float xBaseOffset = IsPC() ? 0 : 32;
	float yBaseOffset = IsPC() ? 0 : 32;
	float offsetS = ( 0.5f / 256.0f );
	float offsetT = ( 0.5f / 256.0f );
	float fFlip0 = bFlip ? 1.0f : 0.0f;
	float fFlip1 = bFlip ? 0.0f : 1.0f;

	if( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		float w = mat_wateroverlaysize.GetFloat();
		float h = mat_wateroverlaysize.GetFloat();

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( xBaseOffset + xOffset * w, yBaseOffset + yOffset * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, fFlip1 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( xBaseOffset + ( xOffset + 1 ) * w, yBaseOffset + yOffset * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, fFlip1 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( xBaseOffset + ( xOffset + 1 ) * w, yBaseOffset + ( yOffset + 1 ) * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, fFlip0 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( xBaseOffset + xOffset * w, yBaseOffset + ( yOffset + 1 ) * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, fFlip0 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

static void OverlayWaterTextures( void )
{
	OverlayWaterTexture( materials->FindMaterial( "debug/debugreflect", NULL ), 0, 0, false );
	OverlayWaterTexture( materials->FindMaterial( "debug/debugrefract", NULL ), 0, 1, true );
}

void OverlayCameraRenderTarget( const char *pszMaterialName, float flX, float flY, float w, float h )
{
	float offsetS = ( 0.5f / 256.0f );
	float offsetT = ( 0.5f / 256.0f );
	IMaterial *pMaterial;
	pMaterial = materials->FindMaterial( pszMaterialName, TEXTURE_GROUP_OTHER, true );
	if( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( flX, flY, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( flX+w, flY, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( flX+w, flY+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( flX, flY+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}


static void OverlayFrameBufferTexture( int nFrameBufferIndex )
{
	float offsetS = ( 0.5f / 256.0f );
	float offsetT = ( 0.5f / 256.0f );
	IMaterial *pMaterial;
	char buf[MAX_PATH];
	Q_snprintf( buf, MAX_PATH, "debug/debugfbtexture%d", nFrameBufferIndex );
	pMaterial = materials->FindMaterial( buf, TEXTURE_GROUP_OTHER, true );
	if( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		float w = mat_framebuffercopyoverlaysize.GetFloat();
		float h = mat_framebuffercopyoverlaysize.GetFloat();

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( w * nFrameBufferIndex, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w * ( nFrameBufferIndex + 1 ), 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w * ( nFrameBufferIndex + 1 ), h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w * nFrameBufferIndex, h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Debugging aid to display a texture
//-----------------------------------------------------------------------------
static void OverlayShowTexture( const char* textureName, float scale )
{
	bool			foundVar;
	IMaterial		*pMaterial;
	IMaterialVar	*BaseTextureVar;
	ITexture		*pTex;
	float			x, y, w, h;

	// ___error is created in code in CMaterialSystem::CreateDebugMaterials()
	pMaterial = materials->FindMaterial( "___error", TEXTURE_GROUP_OTHER, true );
	BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	if (!foundVar)
		return;

	CMatRenderContextPtr pRenderContext( materials );

	if ( textureName && textureName[0] )
	{
		pTex = materials->FindTexture( textureName, TEXTURE_GROUP_OTHER, false );
		BaseTextureVar->SetTextureValue( pTex );

		w = pTex->GetActualWidth() * scale;
		h = pTex->GetActualHeight() * scale;
	}
	else
	{
		w = h = 64.0f * scale;
	}

	// Center relative to current viewport
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );
	x = ( nViewportWidth - w ) * 0.5f;
	y = ( nViewportHeight - h ) * 0.5f;

	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( x+w, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( x+w, y+h, 0.0f );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( x, y+h, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.End();
	pMesh->Draw();
}

#define DEBUG_CUSTOM_MATERIAL_COUNT_HISTORY 60
int m_iDebugCustomMaterialCountHistory[60] = {0};
float m_flDebugNextCustomMaterialCheckTime = 0.0f;

static void DebugOverlayNumActiveCustomMaterialsGraph()
{
	int iNumMaterials = g_pMaterialSystem->GetCustomMaterialManager()->DebugGetNumActiveCustomMaterials();

	IMaterial		*pMaterial;
	float			x, y;
	pMaterial = materials->FindMaterial( "vgui/white", TEXTURE_GROUP_OTHER, true );

	int backBufferWidth, backBufferHeight;
	materials->GetBackBufferDimensions( backBufferWidth, backBufferHeight );
	x = 64;
	y = (backBufferHeight / 5) * 3;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	int temp_x = x + ( 4*DEBUG_CUSTOM_MATERIAL_COUNT_HISTORY );
	int temp_y = (backBufferHeight / 5);
	int iNumPrims = DEBUG_CUSTOM_MATERIAL_COUNT_HISTORY-1;

	//draw graph axis
	meshBuilder.Begin( pMesh, MATERIAL_LINES, iNumPrims + 2 );
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( temp_x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( temp_x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( temp_x, temp_y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	//draw graph segments in same batch
	for ( int i=0; i<iNumPrims; i++ )
	{
		temp_x = 4 + x + (i * 4);
		temp_y = y - (m_iDebugCustomMaterialCountHistory[i] / 2);
		meshBuilder.Position3f( temp_x, temp_y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 255, 255, 255 );
		meshBuilder.AdvanceVertex();
		temp_y = y - (m_iDebugCustomMaterialCountHistory[i+1] / 2);
		meshBuilder.Position3f( temp_x+4, temp_y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 255, 255, 255 );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	//update graph once per second
	if ( gpGlobals->curtime > m_flDebugNextCustomMaterialCheckTime || gpGlobals->curtime + 1.0f < m_flDebugNextCustomMaterialCheckTime )
	{
		m_flDebugNextCustomMaterialCheckTime = gpGlobals->curtime + 1.0f;

		for ( int i=0; i<iNumPrims; i++ )
			m_iDebugCustomMaterialCountHistory[i] = m_iDebugCustomMaterialCountHistory[i+1];
		m_iDebugCustomMaterialCountHistory[iNumPrims] = iNumMaterials;

		//draw the actual number of active materials at the head of the graph as text
		float fl_x = (float)(temp_x+10) / (float)(backBufferWidth);
		float fl_y = (float)(temp_y-10) / (float)(backBufferHeight);
		char szNumMaterials[32];
		V_snprintf( szNumMaterials, sizeof(szNumMaterials), "%i Active", iNumMaterials );
		NDebugOverlay::ScreenText( fl_x, fl_y, szNumMaterials, 255, 255, 255, 255, 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Debugging aid to display a color ramp
//-----------------------------------------------------------------------------
#if defined( _GAMECONSOLE )
static void OverlayColorRamp( bool bHalfSpace )
{
	IMaterial		*pMaterial;
	float			x, y, w, h;

	pMaterial = materials->FindMaterial( "vgui/white", TEXTURE_GROUP_OTHER, true );
	
	int backBufferWidth, backBufferHeight;
	materials->GetBackBufferDimensions( backBufferWidth, backBufferHeight );

	w = ( backBufferWidth == 1280 ) ? 1024 : 512;
	h = 80;
	x = ( backBufferWidth - w )/2;
	y = ( backBufferHeight - 4*h )/2;
	
	int numBands = 32;
	int color0 = 0;
	int color1 = bHalfSpace ? 127 : 255;
	int colorStep = (color1 - color0 + 1)/numBands;

	CMatRenderContextPtr pRenderContext( materials );
	
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;

	// draw ticks
	int xx = x;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, numBands+1 );
	for ( int i=0; i<numBands+1; i++ )
	{
		meshBuilder.Position3f( xx, y-10, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 255, 255, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 255, 255, 0 );
		meshBuilder.AdvanceVertex();
		xx += w/numBands;
	}
	meshBuilder.End();
	pMesh->Draw();

	// black to white band
	xx = x;
	int color = color0;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, numBands );
	for ( int i=0; i<numBands+1; i++ )
	{
		meshBuilder.Position3f( xx, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		color += colorStep;
		if ( color > 255 )
			color = 255;
		xx += w/numBands;
	}
	meshBuilder.End();
	pMesh->Draw();

	// white to black band
	color = color1;
	y += h;
	xx = x;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, numBands );
	for ( int i=0; i<numBands+1; i++ )
	{
		meshBuilder.Position3f( xx, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color3ub( color, color, color );
		meshBuilder.AdvanceVertex();
		color -= colorStep;
		if ( color < 0 )
			color = 0;
		xx += w/numBands;
	}
	meshBuilder.End();
	pMesh->Draw();

	// red band
	color = color1;
	y += h;
	xx = x;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, numBands );
	for ( int i=0; i<numBands+1; i++ )
	{
		meshBuilder.Position3f( xx, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( color, 0, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color3ub( color, 0, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color3ub( color, 0, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color3ub( color, 0, 0 );
		meshBuilder.AdvanceVertex();
		color -= colorStep;
		if ( color < 0 )
			color = 0;
		xx += w/numBands;
	}
	meshBuilder.End();
	pMesh->Draw();

	// green band
	color = color1;
	y += h;
	xx = x;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, numBands );
	for ( int i=0; i<numBands+1; i++ )
	{
		meshBuilder.Position3f( xx, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 0, color, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color3ub( 0, color, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color3ub( 0, color, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color3ub( 0, color, 0 );
		meshBuilder.AdvanceVertex();

		color -= colorStep;
		if ( color < 0 )
			color = 0;
		xx += w/numBands;
	}
	meshBuilder.End();
	pMesh->Draw();

	// blue band
	color = color1;
	y += h;
	xx = x;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, numBands );
	for ( int i=0; i<numBands+1; i++ )
	{
		meshBuilder.Position3f( xx, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 0, 0, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color3ub( 0, 0, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx+w/numBands, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color3ub( 0, 0, color );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3f( xx, y+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color3ub( 0, 0, color );
		meshBuilder.AdvanceVertex();
		color -= colorStep;
		if ( color < 0 )
			color = 0;
		xx += w/numBands;
	}
	meshBuilder.End();
	pMesh->Draw();
}
#endif

#if defined( PORTAL )
ConVar cl_debugoverlaysthroughportals( "cl_debugoverlaysthroughportals", "0" );
#endif
//-----------------------------------------------------------------------------
// Draws all the debugging info
//-----------------------------------------------------------------------------
void CDebugViewRender::Draw3DDebuggingInfo( const CViewSetup &view )
{
	VPROF("CViewRender::Draw3DDebuggingInfo");

	// Draw anything Foundry wants to.
	FoundryHelpers_DrawAll();

	if ( cl_csm_debug_3d.GetInt() )
	{
		g_CascadeLightManager.Draw3DDebugInfo();	
	}

	// Draw 3d overlays
#if defined( PORTAL )
	if( (g_pPortalRender->GetViewRecursionLevel() == 0) || cl_debugoverlaysthroughportals.GetBool() )
	{
		render->Draw3DDebugOverlays();
	}
#else
	render->Draw3DDebugOverlays();
#endif

	// Draw the line file used for debugging leaks
	render->DrawLineFile();
}


//-----------------------------------------------------------------------------
// Draws all the debugging info
//-----------------------------------------------------------------------------
void CDebugViewRender::Draw2DDebuggingInfo( const CViewSetup &view )
{
	// HDRFIXME: Assert NULL rendertarget
	if ( mat_yuv.GetInt() )
	{
		IMaterial *pMaterial;
		pMaterial = materials->FindMaterial( "debug/yuv", TEXTURE_GROUP_OTHER, true );
		if( !IsErrorMaterial( pMaterial ) )
		{
			DrawScreenEffectMaterial( pMaterial, view.x, view.y, view.width, view.height );
		}
	}

	if ( mat_hsv.GetInt() )
	{
		IMaterial *pMaterial;
		pMaterial = materials->FindMaterial( "debug/hsv", TEXTURE_GROUP_OTHER, true );
		if( !IsErrorMaterial( pMaterial ) )
		{
			DrawScreenEffectMaterial( pMaterial, view.x, view.y, view.width, view.height );
		}
	}

	// Draw debugging lightmaps
	if ( mat_showlightmappage.GetInt() != -1 )
	{
		CLightmapDebugView clientView( assert_cast<CViewRender *>( ::view ) );
		clientView.Setup( view );
		clientView.Draw();
	}

	if ( cl_drawshadowtexture.GetInt() )
	{
		int nSize = cl_shadowtextureoverlaysize.GetInt();
		g_pClientShadowMgr->RenderShadowTexture( nSize, nSize );
	}

	if ( cl_csm_debug_2d.GetInt() )
	{
		g_CascadeLightManager.Draw2DDebugInfo();
	}

	if ( cl_custommaterial_debug_graph.GetInt() )
	{
		DebugOverlayNumActiveCustomMaterialsGraph();
	}

	const char *pDrawMaterial = cl_drawmaterial.GetString();
	if ( pDrawMaterial && pDrawMaterial[0] )
	{
		RenderMaterial( pDrawMaterial ); 
	}

	if ( mat_showwatertextures.GetBool() )
	{
		OverlayWaterTextures();
	}

	if ( mat_showcamerarendertarget.GetBool() )
	{
		float w = mat_wateroverlaysize.GetFloat();
		float h = mat_wateroverlaysize.GetFloat();
#ifdef PORTAL
		g_pPortalRender->OverlayPortalRenderTargets( w, h );
#else
		OverlayCameraRenderTarget( "debug/debugcamerarendertarget", 0, 0, w, h );
#endif
	}

	if ( mat_showframebuffertexture.GetBool() )
	{
		// HDRFIXME: Get rid of these rendertarget sets assuming that the assert at the top of this function is true.
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport( NULL );
		OverlayFrameBufferTexture( 0 );
		OverlayFrameBufferTexture( 1 );
		pRenderContext->PopRenderTargetAndViewport( );
	}

	const char *pDrawTexture = mat_drawTexture.GetString();
	if ( pDrawTexture && pDrawTexture[0] )
	{		
		OverlayShowTexture( pDrawTexture, mat_drawTextureScale.GetFloat() );
	}

#ifdef _GAMECONSOLE
	if ( mat_drawColorRamp.GetBool() )
	{
		OverlayColorRamp( mat_drawColorRamp.GetInt() == 2 );
	}
#endif

	if ( r_flashlightdrawdepth.GetBool() )
	{
		shadowmgr->DrawFlashlightDepthTexture( );
	}
}

//-----------------------------------------------------------------------------
// A console command allowing you to draw a material as an overlay
//-----------------------------------------------------------------------------
CON_COMMAND_F( r_screenoverlay, "Draw specified material as an overlay", FCVAR_CHEAT|FCVAR_SERVER_CAN_EXECUTE )
{
	if( args.ArgC() == 2 )
	{
		if ( !Q_stricmp( "off", args[1] ) )
		{
			view->SetScreenOverlayMaterial( NULL );
		}
		else
		{
			IMaterial *pMaterial = materials->FindMaterial( args[1], TEXTURE_GROUP_OTHER, false );
			if ( !IsErrorMaterial( pMaterial ) )
			{
				view->SetScreenOverlayMaterial( pMaterial );
			}
			else
			{
				view->SetScreenOverlayMaterial( NULL );
			}
		}
	}
	else
	{
		IMaterial *pMaterial;
		pMaterial = view->GetScreenOverlayMaterial();
		Warning( "r_screenoverlay: %s\n", pMaterial ? pMaterial->GetName() : "off" );
	}
}

// Used to verify frame syncing.
void CDebugViewRender::GenerateOverdrawForTesting()
{
	if ( IsGameConsole() )
		return;

	if ( !cl_overdraw_test.GetInt() )
		return;

	for ( int i=0; i < 40; i++ )
	{
		g_SmokeFogOverlayAlpha = 20 / 255.0;
		
		g_SmokeFogOverlayColor.Init( 0.33f, 0.33f, 0.33f );

		DrawSmokeFogOverlay();
	}
	g_SmokeFogOverlayAlpha = 0;
}



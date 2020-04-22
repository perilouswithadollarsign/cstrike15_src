//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

  glapp.c - Simple OpenGL shell
  
	There are several options allowed on the command line.  They are:
	-height : what window/screen height do you want to use?
	-width  : what window/screen width do you want to use?
	-bpp    : what color depth do you want to use?
	-window : create a rendering window rather than full-screen
	-fov    : use a field of view other than 90 degrees
*/


//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           MatSysWindow.cpp
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include <mxtk/mx.h>
#include <mxtk/mxMessageBox.h>
#include <mxtk/mxTga.h>
#include <mxtk/mxPcx.h>
#include <mxtk/mxBmp.h>
#include <mxtk/mxMatSysWindow.h>
// #include "gl.h"
// #include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tier1/convar.h"
#include <string.h>
#include "MatSysWin.h"
#include "MDLViewer.h"
#include "StudioModel.h"
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "FileSystem.h"
#include <KeyValues.h>
#include "materialsystem/IMesh.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "tier0/dbg.h"
#include "istudiorender.h"
#include "tier0/icommandline.h"
#include "mathlib/vmatrix.h"
#include "studio_render.h"
#include "vstdlib/cvar.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundsystem/isoundsystem.h"
#include "soundchars.h"
#include "mathlib/softbodyenvironment.h"
#include "valve_ipc_win32.h"

extern char g_appTitle[];
extern bool g_bInError;
extern int g_dxlevel;

extern ISoundEmitterSystemBase *g_pSoundEmitterBase;

extern CValveIpcClientUtl g_HlmvIpcClient;
extern bool g_bHlmvMaster;

extern CSoftbodyEnvironment g_SoftbodyEnvironment;


void UpdateSounds()
{
	static double prev = 0;
	double curr = (double) mx::getTickCount () / 1000.0;
	if ( prev != 0 )
	{
		double dt = (curr - prev);
		g_pSoundSystem->Update( dt * g_viewerSettings.speedScale );
	}
	prev = curr;
}


// FIXME: move all this to mxMatSysWin

class DummyMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy( const char *proxyName )	{return NULL;}
	virtual void DeleteProxy( IMaterialProxy *pProxy )				{}
	virtual CreateInterfaceFn GetFactory()							{return NULL;}
};
DummyMaterialProxyFactory	g_DummyMaterialProxyFactory;


static void ReleaseMaterialSystemObjects( int nChangeFlags )
{
	StudioModel::ReleaseStudioModel();
}

static void RestoreMaterialSystemObjects( int nChangeFlags )
{
	StudioModel::RestoreStudioModel();
	g_ControlPanel->OnLoadModel();
}

static ConVar mat_bumpmap( "mat_bumpmap", "1" );
static ConVar mat_specular( "mat_specular", "1" );
static ConVar mat_parallaxmap( "mat_parallaxmap", "0" );
static ConVar mat_displacementmap( "mat_displacementmap", "1" );

void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig)
{
	mat_bumpmap.SetValue( g_viewerSettings.enableNormalMapping );
	mat_displacementmap.SetValue( g_viewerSettings.enableDisplacementMapping );
	mat_specular.SetValue( g_viewerSettings.enableSpecular );
	mat_parallaxmap.SetValue( g_viewerSettings.enableParallaxMapping );
}

MatSysWindow *g_MatSysWindow = 0;

Vector g_vright( 50, 50, 0 );		// needs to be set to viewer's right in order for chrome to work

IMaterial *g_materialBackground = NULL;
IMaterial *g_materialWireframe = NULL;
IMaterial *g_materialWireframeVertexColor = NULL;
IMaterial *g_materialWireframeVertexColorNoCull = NULL;
IMaterial *g_materialDebugCopyBaseTexture = NULL;
IMaterial *g_materialFlatshaded = NULL;
IMaterial *g_materialSmoothshaded = NULL;
IMaterial *g_materialBones = NULL;
IMaterial *g_materialLines = NULL;
IMaterial *g_materialFloor = NULL;
IMaterial *g_materialVertexColor = NULL;
IMaterial *g_materialShadow = NULL;
IMaterial *g_materialArcActive = NULL;
IMaterial *g_materialArcInActive = NULL;
IMaterial *g_materialDebugText = NULL;

MatSysWindow::MatSysWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: mxMatSysWindow (parent, x, y, w, h, label, style)
{
	g_pMaterialSystem->SetMaterialProxyFactory( &g_DummyMaterialProxyFactory );

	m_pCubemapTexture = NULL;
	m_hWnd = (HWND)getHandle();

	MaterialSystem_Config_t config;
	config = g_pMaterialSystem->GetCurrentConfigForVideoCard();
	InitMaterialSystemConfig(&config);
	if ( g_dxlevel != 0 )
	{
		config.dxSupportLevel = g_dxlevel;
	}
	
//	config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );

	if (!g_pMaterialSystem->SetMode( ( void * )m_hWnd, config ) )
	{
		return;
	}
	
	g_pMaterialSystem->OverrideConfig( config, false );

	g_pMaterialSystem->AddReleaseFunc( ReleaseMaterialSystemObjects );
	g_pMaterialSystem->AddRestoreFunc( RestoreMaterialSystemObjects );

	m_pCubemapTexture = g_pMaterialSystem->FindTexture( "hlmv/cubemap", NULL, true );
	m_pCubemapTexture->IncrementReferenceCount();
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->BindLocalCubemap( m_pCubemapTexture );

	g_materialBackground	= g_pMaterialSystem->FindMaterial("hlmv/background", TEXTURE_GROUP_OTHER, true);
	g_materialWireframe		= g_pMaterialSystem->FindMaterial("debug/debugmrmwireframe", TEXTURE_GROUP_OTHER, true);
	g_materialWireframeVertexColor = g_pMaterialSystem->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER, true);

	// test: create this from code - you need a vmt to make $nocull 1 happen, can't do it from the render context
	{
		KeyValues *pVMTKeyValues = new KeyValues( "Wireframe" );
		pVMTKeyValues->SetInt("$ignorez", 1);
		pVMTKeyValues->SetInt("$nocull", 1);
		pVMTKeyValues->SetInt("$vertexcolor", 1);
		pVMTKeyValues->SetInt("$decal", 1);
		g_materialWireframeVertexColorNoCull = g_pMaterialSystem->CreateMaterial( "debug/wireframenocull", pVMTKeyValues );
	}
	{
		KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
		pVMTKeyValues->SetString("$basetexture", "vgui/white" );
		g_materialDebugCopyBaseTexture = g_pMaterialSystem->CreateMaterial( "debug/copybasetexture", pVMTKeyValues );

	}

	g_materialFlatshaded	= g_pMaterialSystem->FindMaterial("debug/debugdrawflatpolygons", TEXTURE_GROUP_OTHER, true);
	g_materialSmoothshaded	= g_pMaterialSystem->FindMaterial("debug/debugmrmfullbright2", TEXTURE_GROUP_OTHER, true);
	g_materialBones			= g_pMaterialSystem->FindMaterial("debug/debugskeleton", TEXTURE_GROUP_OTHER, true);
	g_materialLines			= g_pMaterialSystem->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER, true);
	g_materialFloor			= g_pMaterialSystem->FindMaterial("hlmv/floor", TEXTURE_GROUP_OTHER, true);
	g_materialVertexColor   = g_pMaterialSystem->FindMaterial("debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true);
	g_materialShadow		= g_pMaterialSystem->FindMaterial("hlmv/shadow", TEXTURE_GROUP_OTHER, true);
	g_materialDebugText		= g_pMaterialSystem->FindMaterial("hlmv/debugtext", TEXTURE_GROUP_OTHER, true);

	{
		KeyValues *pVMTKeyValues = new KeyValues( "UnLitGeneric" );
		pVMTKeyValues->SetInt("$nocull", 1);
		pVMTKeyValues->SetInt("$vertexcolor", 1);
		pVMTKeyValues->SetFloat("$alpha", 0.5f);
		g_materialArcActive = g_pMaterialSystem->CreateMaterial( "hlmv/arc_active", pVMTKeyValues );
	}

	{
		KeyValues *pVMTKeyValues = new KeyValues( "UnLitGeneric" );
		pVMTKeyValues->SetInt("$nocull", 1);
		pVMTKeyValues->SetInt("$vertexcolor", 1);
		pVMTKeyValues->SetFloat("$alpha", 0.2f);
		g_materialArcInActive = g_pMaterialSystem->CreateMaterial( "hlmv/arc_inactive", pVMTKeyValues );
	}

	if (!parent)
		setVisible (true);
	else
		mx::setIdleWindow (this);
}



MatSysWindow::~MatSysWindow ()
{
	if (m_pCubemapTexture)
	{
		m_pCubemapTexture->DecrementReferenceCount();
	}
	mx::setIdleWindow (0);
}



int
MatSysWindow::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	static float oldrx = 0, oldry = 0, oldtz = 50, oldtx = 0, oldty = 0;
	static float oldlrx = 0, oldlry = 0;
	static int oldx, oldy;

	switch (event->event)
	{

	case mxEvent::Idle:
	{
		static double prev;
		double curr = (double) mx::getTickCount () / 1000.0;
		double dt = (curr - prev);

		// clamp to 100fps
		if (dt >= 0.0 && dt < 0.01)
		{
			Sleep( 10 - dt * 1000.0 );
			return 1;
		}

		if ( prev != 0.0 )
		{
	//		dt = 0.001;

			g_pStudioModel->AdvanceFrame ( dt * g_viewerSettings.speedScale );
			if ( g_viewerSettings.animateWeapons )
			{
				const char *pszMainSequenceName = g_pStudioModel->GetSequenceName( g_pStudioModel->GetSequence() );
				if ( pszMainSequenceName )
				{
					for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
					{
						if ( !g_pStudioExtraModel[ i ] )
							continue;

						// match weapon sequence and frame to marine
						int iSequence = g_pStudioExtraModel[ i ]->LookupSequence( pszMainSequenceName );
						if ( iSequence == -1 )
						{
							g_pStudioExtraModel[ i ]->SetFrame( 0 );
						}
						else
						{
							g_pStudioExtraModel[ i ]->SetSequence( iSequence );
							g_pStudioExtraModel[ i ]->SetFrame( g_pStudioModel->GetCycle() * g_pStudioExtraModel[ i ]->GetMaxFrame() );
						}
					}
				}
			}
			g_ControlPanel->updateFrameSlider( );
			g_ControlPanel->updateGroundSpeed( );
		}
		prev = curr;
		g_pStudioModel->SetSoftbodyOrientation();
		for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
		{
			if ( g_pStudioExtraModel[ i ] != NULL )
			{
				g_pStudioExtraModel[ i ]->SetSoftbodyOrientation();
			}
		}
		if ( g_viewerSettings.simulateSoftbodies )
		{
			g_SoftbodyEnvironment.Step( dt * g_viewerSettings.speedScale );
		}

		if (!g_viewerSettings.pause)
			redraw ();

		g_ControlPanel->updateTransitionAmount();

		UpdateSounds();

		return 1;
	}
	break;

	case mxEvent::MouseUp:
	{
		g_viewerSettings.mousedown = false;

		if ( g_pWidgetControl )
			g_pWidgetControl->m_WidgetState = WIDGET_STATE_NONE;
	}
	break;

	case mxEvent::MouseDown:
	{
		g_viewerSettings.mousedown = true;

		if ( g_pWidgetControl != NULL && g_viewerSettings.highlightHitbox >= 0 )
		{
			g_pWidgetControl->WidgetMouseDown( event->x, event->y );
			g_pWidgetControl->SetStateUsingInputColor( getViewportPixelColor( event->x, event->y ) );
			if ( g_pWidgetControl->m_WidgetState != WIDGET_STATE_NONE )
				return 1;
		}

		oldrx = g_pStudioModel->m_angles[0];
		oldry = g_pStudioModel->m_angles[1];
		oldtx = g_pStudioModel->m_origin[0];
		oldty = g_pStudioModel->m_origin[1];
		oldtz = g_pStudioModel->m_origin[2];
		oldx = event->x;
		oldy = event->y;
		oldlrx = g_viewerSettings.lightrot[1];
		oldlry = g_viewerSettings.lightrot[0];
		g_viewerSettings.pause = false;

		float r = 1.0/3.0 * min( w(), h() );

		float d = sqrt( ( float )( (event->x - w()/2) * (event->x - w()/2) + (event->y - h()/2) * (event->y - h()/2) ) );

		if ( d < r || !g_viewerSettings.allowOrbitYaw )
			g_viewerSettings.rotating = false;
		else
			g_viewerSettings.rotating = true;

		return 1;
	}
	break;

	case mxEvent::MouseDrag:
	{
		if (event->buttons & mxEvent::MouseLeftButton)
		{

			if ( g_pWidgetControl && g_pWidgetControl->m_WidgetState != WIDGET_STATE_NONE )
				{
					g_pWidgetControl->WidgetMouseDrag( event->x, event->y );
					break;
				}

			if ( g_viewerSettings.dotaMode )
			{
				if (event->modifiers & mxEvent::KeyShift)
				{
					g_pStudioModel->m_origin[1] = oldty - (float) (event->x - oldx);
					g_pStudioModel->m_origin[2] = oldtz + (float) (event->y - oldy);
				}
				else
				{
					float rx = (float) (event->x - oldx);
					oldx = event->x;

					QAngle movement;
					matrix3x4_t tmp1, tmp2, tmp3;

					movement = QAngle( 0, rx, 0 );
					AngleMatrix( g_pStudioModel->m_angles, tmp1 );
					AngleMatrix( movement, tmp2 );
					ConcatTransforms( tmp1, tmp2, tmp3 );
					MatrixAngles( tmp3, g_pStudioModel->m_angles );
				}
			}
			else if (event->modifiers & mxEvent::KeyShift)
			{
				g_pStudioModel->m_origin[1] = oldty - (float) (event->x - oldx) / 8.0;
				g_pStudioModel->m_origin[2] = oldtz + (float) (event->y - oldy) / 8.0;
			}
			else if (event->modifiers & mxEvent::KeyCtrl)
			{
				float ry = (float) (event->y - oldy);
				float rx = (float) (event->x - oldx);
				oldx = event->x;
				oldy = event->y;

				QAngle movement = QAngle( ry, rx, 0 );

				matrix3x4_t tmp1, tmp2, tmp3;
				AngleMatrix( g_viewerSettings.lightrot, tmp1 );
				AngleMatrix( movement, tmp2 );
				ConcatTransforms( tmp2, tmp1, tmp3 );
				MatrixAngles( tmp3, g_viewerSettings.lightrot );

				// g_viewerSettings.lightrot[0] = oldlrx + (float) (event->y - oldy);
				// g_viewerSettings.lightrot[1] = oldlry + (float) (event->x - oldx);
			}
			else
			{
				if (!g_viewerSettings.rotating)
				{
					float ry = (float) (event->y - oldy);
					float rx = (float) (event->x - oldx);
					oldx = event->x;
					oldy = event->y;

					QAngle movement;
					matrix3x4_t tmp1, tmp2, tmp3;

					movement = QAngle( 0, rx, 0 );
					AngleMatrix( g_pStudioModel->m_angles, tmp1 );
					AngleMatrix( movement, tmp2 );
					ConcatTransforms( tmp1, tmp2, tmp3 );
					MatrixAngles( tmp3, g_pStudioModel->m_angles );

					movement = QAngle( ry, 0, 0 );
					AngleMatrix( g_pStudioModel->m_angles, tmp1 );
					AngleMatrix( movement, tmp2 );
					ConcatTransforms( tmp2, tmp1, tmp3 );
					MatrixAngles( tmp3, g_pStudioModel->m_angles );
				}
				else
				{
					float ang1 = (180 / 3.1415) * atan2( oldx - w()/2.0, oldy - h()/2.0 );
					float ang2 = (180 / 3.1415) * atan2( event->x - w()/2.0, event->y - h()/2.0 );
					oldx = event->x;
					oldy = event->y;

					QAngle movement = QAngle( 0, 0, ang2 - ang1 );

					matrix3x4_t tmp1, tmp2, tmp3;
					AngleMatrix( g_pStudioModel->m_angles, tmp1 );
					AngleMatrix( movement, tmp2 );
					ConcatTransforms( tmp2, tmp1, tmp3 );
					MatrixAngles( tmp3, g_pStudioModel->m_angles );
				}
			}
		}
		else if (event->buttons & mxEvent::MouseRightButton)
		{
			if ( !g_viewerSettings.dotaMode )
			{
				g_pStudioModel->m_origin[0] = oldtx + (float) (event->y - oldy);
			}
		}

		if ( g_bHlmvMaster && g_HlmvIpcClient.Connect() )
		{
			CUtlBuffer cmd;
			CUtlBuffer res;

			matrix3x4_t m;
			g_pStudioModel->GetModelTransform( m );

			cmd.Printf( "%s %f %f %f %f %f %f %f %f %f %f %f %f",
				"hlmvModelTransform",
				m.m_flMatVal[0][0], m.m_flMatVal[0][1], m.m_flMatVal[0][2], m.m_flMatVal[0][3],
				m.m_flMatVal[1][0], m.m_flMatVal[1][1], m.m_flMatVal[1][2], m.m_flMatVal[1][3],
				m.m_flMatVal[2][0], m.m_flMatVal[2][1], m.m_flMatVal[2][2], m.m_flMatVal[2][3] );

			g_HlmvIpcClient.ExecuteCommand( cmd, res );
			g_HlmvIpcClient.Disconnect();
		}

		redraw ();

		return 1;
	}
	break;

	case mxEvent::KeyDown:
	{
		switch (event->key)
		{
			case VK_SPACE:
			{
				int iSeq = g_pStudioModel->GetSequence ();
				if (iSeq == g_pStudioModel->SetSequence (iSeq + 1))
				{
					g_pStudioModel->SetSequence (0);
				}
			}
			break;
		}
	}
	break;

	case mxEvent::DropFile:
	{
		V_strlower( event->szChars );
		g_ControlPanel->AddQCRecordPath( event->szChars );
		break;
	}
	break;

	} // switch (event->event)

	return 1;
}


void DrawBackground()
{
	if (!g_viewerSettings.showBackground)
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind(g_materialBackground);
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	{
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		float dist=-15000.0f;
		float tMin=0, tMax=1;
		
		meshBuilder.Position3f(-dist, dist, dist);
		meshBuilder.TexCoord2f( 0, tMin,tMax );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( dist, dist, dist);
		meshBuilder.TexCoord2f( 0, tMax,tMax );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( dist,-dist, dist);
		meshBuilder.TexCoord2f( 0, tMax,tMin );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f(-dist,-dist, dist);
		meshBuilder.TexCoord2f( 0, tMin,tMin );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

void DrawHelpers()
{
	if (g_viewerSettings.mousedown && g_viewerSettings.showOrbitCircle )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->Bind( g_materialBones );

		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->LoadIdentity();
		pRenderContext->MatrixMode(MATERIAL_VIEW);
		pRenderContext->LoadIdentity();

		IMesh* pMesh = pRenderContext->GetDynamicMesh();

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, 360 / 5 );

		if (g_viewerSettings.rotating)
			meshBuilder.Color3ub( 255, 255, 0 );
		else
			meshBuilder.Color3ub(   0, 255, 0 );

		for (int i = 0; i < 360; i += 5)
		{
			float a = i * (3.151492653/180.0f);

			if (g_viewerSettings.rotating)
				meshBuilder.Color3ub( 255, 255, 0 );
			else
				meshBuilder.Color3ub(   0, 255, 0 );

			meshBuilder.Position3f( sin( a ), cos( a ), -3.0f );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
		pMesh->Draw();
	}
}


void DrawGroundPlane()
{
	if (!g_viewerSettings.showGround)
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind(g_materialFloor);
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();;
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();;
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity( );

	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( -90,  0, 0, 1 );

    pRenderContext->Translate( -g_pStudioModel->m_origin[0],  -g_pStudioModel->m_origin[1],  -g_pStudioModel->m_origin[2] );

	pRenderContext->Rotate( g_pStudioModel->m_angles[1],  0, 0, 1 );
    pRenderContext->Rotate( g_pStudioModel->m_angles[0],  0, 1, 0 );
    pRenderContext->Rotate( g_pStudioModel->m_angles[2],  1, 0, 0 );

	static Vector tMap( 0, 0, 0 );
	static Vector dxMap( 1, 0, 0 );
	static Vector dyMap( 0, 1, 0 );

	Vector deltaPos;
	QAngle deltaAngles;

	g_pStudioModel->GetMovement( g_pStudioModel->m_prevGroundCycles, deltaPos, deltaAngles );

	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float scale = 10.0;
	float dist=-100.0f;

	float dpdd = scale / dist;

	tMap.x = tMap.x + dxMap.x * deltaPos.x * dpdd + dxMap.y * deltaPos.y * dpdd;	
	tMap.y = tMap.y + dyMap.x * deltaPos.x * dpdd + dyMap.y * deltaPos.y * dpdd;

	while (tMap.x < 0.0) tMap.x +=  1.0;
	while (tMap.x > 1.0) tMap.x += -1.0;
	while (tMap.y < 0.0) tMap.y +=  1.0;
	while (tMap.y > 1.0) tMap.y += -1.0;

	VectorYawRotate( dxMap, -deltaAngles.y, dxMap );
	VectorYawRotate( dyMap, -deltaAngles.y, dyMap );

	// ARRGHHH, this is right but I don't know what I've done
	meshBuilder.Position3f( -dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (-dxMap.x - dyMap.x) * scale, tMap.y + (dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x - dyMap.x) * scale, tMap.y + (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x + dyMap.x) * scale, tMap.y + (-dxMap.y - dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x - (dxMap.x - dyMap.x) * scale, tMap.y - (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	// draw underside
	meshBuilder.Position3f( -dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (-dxMap.x - dyMap.x) * scale, tMap.y + (dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x - (dxMap.x - dyMap.x) * scale, tMap.y - (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x + dyMap.x) * scale, tMap.y + (-dxMap.y - dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x - dyMap.x) * scale, tMap.y + (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	
	
	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PopMatrix();
}




void DrawMovementBoxes()
{
	if (!g_viewerSettings.showMovement)
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind(g_materialFloor);
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity( );

	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( -90,  0, 0, 1 );

    pRenderContext->Translate( -g_pStudioModel->m_origin[0],  -g_pStudioModel->m_origin[1],  -g_pStudioModel->m_origin[2] );

	pRenderContext->Rotate( g_pStudioModel->m_angles[1],  0, 0, 1 );
    pRenderContext->Rotate( g_pStudioModel->m_angles[0],  0, 1, 0 );
    pRenderContext->Rotate( g_pStudioModel->m_angles[2],  1, 0, 0 );

	static matrix3x4_t mStart( 1, 0, 0, 0 ,  0, 1, 0, 0 ,  0, 0, 1, 0 );
	matrix3x4_t mTemp;
	static float prevframes[5];

	Vector deltaPos;
	QAngle deltaAngles;

	g_pStudioModel->GetMovement( prevframes, deltaPos, deltaAngles );

	AngleMatrix( deltaAngles, deltaPos, mTemp );
	MatrixInvert( mTemp, mTemp );
	ConcatTransforms( mTemp, mStart, mStart );

	Vector bboxMin, bboxMax;
	g_pStudioModel->ExtractBbox( bboxMin, bboxMax  );

	static float prevCycle = 0.0;

	if (fabs( g_pStudioModel->GetFrame( 0 ) - prevCycle) > 0.5)
	{
		SetIdentityMatrix( mStart );
	}
	prevCycle = g_pStudioModel->GetFrame( 0 );

	// starting position
	{
		float color[] = { 0.7, 1, 0, 0.5 };
		float wirecolor[] = { 1, 1, 0, 1.0 };
		g_pStudioModel->drawTransparentBox( bboxMin, bboxMax, mStart, color, wirecolor );
	}

	// current position
	{
		float color[] = { 1, 0.7, 0, 0.5 };
		float wirecolor[] = { 1, 0, 0, 1.0 };
		SetIdentityMatrix( mTemp );
		g_pStudioModel->drawTransparentBox( bboxMin, bboxMax, mTemp, color, wirecolor );
	}

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PopMatrix();
}



char const *HLMV_TranslateSoundName( char const *soundname, StudioModel *model )
{
	if ( Q_stristr( soundname, ".wav" ) )
		return PSkipSoundChars( soundname );

	if ( model )
	{
		return PSkipSoundChars( g_pSoundEmitterBase->GetWavFileForSound( soundname, model->GetFileName() ) );
	}

	return PSkipSoundChars( g_pSoundEmitterBase->GetWavFileForSound( soundname, NULL ) );
}


void PlaySound( const char *pSoundName, StudioModel *pStudioModel )
{
	// Play Sound
	if (!g_viewerSettings.playSounds)
		return;
	
	if ( pSoundName == NULL || pSoundName[ 0 ] == '\0' )
		return;

	const char *pSoundFileName = HLMV_TranslateSoundName( pSoundName, pStudioModel );

	char filename[ 256 ];
	sprintf( filename, "sound/%s", pSoundFileName );
	CAudioSource *pAudioSource = g_pSoundSystem->FindOrAddSound( filename );
	if ( pAudioSource == NULL )
		return;

	float volume = VOL_NORM;
	gender_t gender = GENDER_NONE;
	if ( pStudioModel )
	{
		gender = g_pSoundEmitterBase->GetActorGender( pStudioModel->GetFileName() );
	}

	CSoundParameters params;
	if ( !Q_stristr( pSoundName, ".wav" ) &&
		g_pSoundEmitterBase->GetParametersForSound( pSoundName, params, gender ) )
	{
		volume = params.volume;
	}

	g_pSoundSystem->PlaySound( pAudioSource, volume, NULL );
}


// copied from baseentity.cpp
// HACK:  This must match the #define in cl_animevent.h in the client .dll code!!!
#define CL_EVENT_SOUND				5004
#define CL_EVENT_FOOTSTEP_LEFT		6004
#define CL_EVENT_FOOTSTEP_RIGHT		6005
#define CL_EVENT_MFOOTSTEP_LEFT		6006
#define CL_EVENT_MFOOTSTEP_RIGHT	6007

// copied from scriptevent.h
#define SCRIPT_EVENT_SOUND			1004		// Play named wave file (on CHAN_BODY)
#define SCRIPT_EVENT_SOUND_VOICE	1008		// Play named wave file (on CHAN_VOICE)


void PlaySounds( StudioModel *pStudioModel )
{
	if ( pStudioModel == NULL )
		return;

	int iLayer = g_ControlPanel->getFrameSelection();
	float flFrame = pStudioModel->GetFrame( iLayer );
	float flTime = flFrame / 30.0f; // pStudioModel->GetSequenceTime()

	float prevtime = flTime - pStudioModel->GetTimeDelta();
	float currtime = flTime;

	float duration = pStudioModel->GetDuration();

	prevtime = fmod( prevtime, duration );
	currtime = fmod( currtime, duration );

	float prevcycle = prevtime / duration;
	float currcycle = currtime / duration;

	CStudioHdr *pStudioHdr = pStudioModel->GetStudioHdr();
	if ( pStudioHdr == NULL )
		return;

	int seq = pStudioModel->GetSequence();
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( seq );

	for ( int i = 0; i < (int)seqdesc.numevents; ++i )
	{
		mstudioevent_t *pEvent = seqdesc.pEvent( i );
#if defined( _DEBUG )
		const char *pEventName = pEvent->pszEventName();
		NOTE_UNUSED( pEventName );
#endif
		if ( pEvent->cycle <= prevcycle || pEvent->cycle > currcycle )
			continue;

		// largely copied from BuildAnimationEventSoundList in baseentity.cpp
		switch ( pEvent->event )
		{
		case 0:
			if ( Q_strcmp( pEvent->pszEventName(), "AE_CL_PLAYSOUND" ) == 0 )
			{
				PlaySound( pEvent->pszOptions(), pStudioModel );
				continue;
			}
			break;

		case CL_EVENT_SOUND: // Old-style client .dll animation event
			// fall-through intentional
		case SCRIPT_EVENT_SOUND:
			// fall-through intentional
		case SCRIPT_EVENT_SOUND_VOICE:
			PlaySound( pEvent->pszOptions(), pStudioModel );
			break;

		case CL_EVENT_FOOTSTEP_LEFT:
		case CL_EVENT_FOOTSTEP_RIGHT:
			{
				char soundname[256];
				char const *options = pEvent->pszOptions();
				if ( !options || !options[0] )
				{
					options = "NPC_CombineS";
				}

				Q_snprintf( soundname, 256, "%s.RunFootstepLeft", options );
				PlaySound( soundname, pStudioModel );
				Q_snprintf( soundname, 256, "%s.RunFootstepRight", options );
				PlaySound( soundname, pStudioModel );
				Q_snprintf( soundname, 256, "%s.FootstepLeft", options );
				PlaySound( soundname, pStudioModel );
				Q_snprintf( soundname, 256, "%s.FootstepRight", options );
				PlaySound( soundname, pStudioModel );
			}
			break;
/*
		case AE_CL_PLAYSOUND:
			if ( !( pEvent->type & AE_TYPE_CLIENT ) )
				break;

			if ( pEvent->options[0] )
			{
				PlaySound( pEvent->options, pStudioModel );
			}
			else
			{
				Warning( "-- Error --:  empty soundname, .qc error on AE_CL_PLAYSOUND in model %s, sequence %s, animevent # %i\n", 
					pStudioHdr->name(), seqdesc.pszLabel(), i + 1 );
			}
			break;
*/
		default:
			break;
		}
	}
}


void
MatSysWindow::draw ()
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( g_bInError || !g_pStudioModel->GetStudioRender() )
		return;

	static bool bInDraw = false;
	if (bInDraw)
		return;

	bInDraw = true;

	UpdateSounds(); // need to call this multiple times per frame to avoid audio stuttering

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	g_pMaterialSystem->BeginFrame( 0 );
	g_pStudioModel->GetStudioRender()->BeginFrame();

	pRenderContext->ClearColor3ub(g_viewerSettings.bgColor[0] * 255, g_viewerSettings.bgColor[1] * 255, g_viewerSettings.bgColor[2] * 255);
	// pRenderContext->ClearColor3ub(0, 0, 0 );
	pRenderContext->ClearBuffers(true, true);

	pRenderContext->Viewport( 0, 0, w(), h() );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity( );
	pRenderContext->PerspectiveX(g_viewerSettings.fov, (float)w() / (float)h(), 1.0f, 20000.0f);
	
	DrawBackground();
	DrawGroundPlane();
	DrawMovementBoxes();
	DrawHelpers();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity( );
	// FIXME: why is this needed?  Doesn't SetView() override this?
	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( -90,  0, 0, 1 );

	g_pStudioModel->ClearLookTargets();
	g_pStudioModel->AddLookTarget( Vector( 0, 0, 0 ), g_pStudioModel->GetSolveHeadTurn() ? 1.0f : 0.0f );
	int polycount = g_pStudioModel->DrawModel( false, PASS_MODELONLY );

	g_pStudioModel->GetStudioRender()->EndFrame();

	UpdateSounds(); // need to call this multiple times per frame to avoid audio stuttering

	g_ControlPanel->setModelInfo();

	int lod;
	float metric;
	metric = g_pStudioModel->GetLodMetric();
	lod = g_pStudioModel->GetLodUsed();
	g_ControlPanel->setLOD( lod, true, false );
	g_ControlPanel->setLODMetric( metric );

	g_ControlPanel->setPolycount( polycount );
	g_ControlPanel->setTransparent( g_pStudioModel->m_bIsTransparent );

	g_ControlPanel->updatePoseParameters( );
	
	// draw what ever else is loaded
	int i;
	for (i = 0; i < HLMV_MAX_MERGED_MODELS; i++)
	{
		if (g_pStudioExtraModel[i] != NULL)
		{
			g_pStudioModel->GetStudioRender()->BeginFrame();
			g_pStudioExtraModel[i]->DrawModel( true );
			g_pStudioModel->GetStudioRender()->EndFrame();
		}
	}

	g_pStudioModel->GetStudioRender()->BeginFrame();
	g_pStudioModel->DrawModel( false, PASS_EXTRASONLY );
	g_pStudioModel->GetStudioRender()->EndFrame();

	g_pStudioModel->IncrementFramecounter();

	PlaySounds( g_pStudioModel );
	UpdateSounds(); // need to call this multiple times per frame to avoid audio stuttering

    g_pMaterialSystem->SwapBuffers();
	
	g_pMaterialSystem->EndFrame();

	bInDraw = false;
}



/*
int
MatSysWindow::loadTexture (const char *filename, int name)
{
	if (!filename || !strlen (filename))
	{
		if (d_textureNames[name])
		{
			glDeleteTextures (1, (const GLuint *) &d_textureNames[name]);
			d_textureNames[name] = 0;

			if (name == 0)
				strcpy (g_viewerSettings.backgroundTexFile, "");
			else
				strcpy (g_viewerSettings.groundTexFile, "");
		}

		return 0;
	}

	mxImage *image = 0;

	char ext[16];
	strcpy (ext, mx_getextension (filename));

	if (!mx_strcasecmp (ext, ".tga"))
		image = mxTgaRead (filename);
	else if (!mx_strcasecmp (ext, ".pcx"))
		image = mxPcxRead (filename);
	else if (!mx_strcasecmp (ext, ".bmp"))
		image = mxBmpRead (filename);

	if (image)
	{
		if (name == 0)
			strcpy (g_viewerSettings.backgroundTexFile, filename);
		else
			strcpy (g_viewerSettings.groundTexFile, filename);

		d_textureNames[name] = name + 1;

		if (image->bpp == 8)
		{
			mstudiotexture_t texture;
			texture.width = image->width;
			texture.height = image->height;

			g_pStudioModel->UploadTexture (&texture, (byte *) image->data, (byte *) image->palette, name + 1);
		}
		else
		{
			glBindTexture (GL_TEXTURE_2D, d_textureNames[name]);
			glTexImage2D (GL_TEXTURE_2D, 0, 3, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		delete image;

		return name + 1;
	}

	return 0;
}
*/

void MatSysWindow::dumpViewport (const char *filename)
{
	dumpViewportWithLabel( filename, "" );
}


Color MatSysWindow::getViewportPixelColor( int x, int y )
{
	redraw ();

	HDC hDC = GetDC ((HWND) getHandle ());
	COLORREF color = GetPixel(hDC, x, y);
	ReleaseDC ((HWND) getHandle (), hDC);
	
	Color temp;

	temp.SetColor( GetRValue( color ), GetGValue( color ), GetBValue( color ), 255 );

	return temp;
}

// Save HDC as a bitmap:
void MatSysWindow::dumpViewportWithLabel(const char *filename, const char *label)
{

	redraw ();
	//int w = w2 ();
	//int h = h2 ();

	RECT client;
	HDC hDC = GetDC ((HWND) getHandle ());
	RECT rcClient;
	bool bOk = false;
	HBITMAP hImage = NULL;

	GetClientRect ((HWND) getHandle (), &rcClient);
	client = rcClient;

	if ((hImage = CreateCompatibleBitmap (hDC, rcClient.right, rcClient.bottom)) != NULL)
	{
		HDC hMemDC;
		HBITMAP hDCBmp;

		if ((hMemDC = CreateCompatibleDC (hDC)) != NULL)
		{
			hDCBmp = (HBITMAP) SelectObject (hMemDC, hImage);
			BitBlt (hMemDC, 0, 0, rcClient.right, rcClient.bottom, hDC, 0, 0, SRCCOPY);

			if ( strlen(label) > 0 )
			{
				HFONT font = CreateFont( 24, 0, 0, 0, 700, false, false, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial" );
				SetBkColor(hMemDC,  RGB(60,60,60) );
				SetTextColor(hMemDC, RGB(255,255,255) );
				HFONT hFontOld = (HFONT)SelectObject( hMemDC, font );
				DrawText( hMemDC, label, strlen(label), &rcClient, DT_TOP|DT_CENTER);
				SelectObject( hMemDC, hFontOld );
			}

			SelectObject (hMemDC, hDCBmp);
			DeleteDC (hMemDC);
			bOk = true;
		}
	}

	ReleaseDC ((HWND) getHandle (), hDC);

	if (! bOk)
	{
		if (hImage)
		{
			DeleteObject (hImage);
			hImage = NULL;
		}
	}

	if ( !hImage )
	{
		mxMessageBox (this, "Screenshot failure: Couldn't capture the model window.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
		return;
	}

	if ( hImage != NULL) {
		UINT uiBytesPerRow = 3 * client.right; // RGB takes 24 bits
		UINT uiRemainderForPadding;

		if ((uiRemainderForPadding = uiBytesPerRow % sizeof (DWORD)) > 0)
		{
			uiBytesPerRow += (sizeof (DWORD) - uiRemainderForPadding);
		}

		UINT uiBytesPerAllRows = uiBytesPerRow * client.bottom;
		PBYTE pDataBits;

		if ((pDataBits = new BYTE [uiBytesPerAllRows]) != NULL)
		{
			BITMAPINFOHEADER bmi = {0};
			BITMAPFILEHEADER bmf = {0};
			HDC hDC = GetDC ((HWND) getHandle ());

			// Prepare to get the data out of HBITMAP:
			bmi.biSize = sizeof (bmi);
			bmi.biPlanes = 1;
			bmi.biBitCount = 24;
			bmi.biHeight = client.bottom;
			bmi.biWidth = client.right;

			GetDIBits (hDC, hImage, 0, client.bottom, pDataBits, (BITMAPINFO*) &bmi, DIB_RGB_COLORS);

			ReleaseDC ((HWND) getHandle (), hDC);

			// Fill the file header:
			bmf.bfOffBits = sizeof (bmf) + sizeof (bmi);
			bmf.bfSize = bmf.bfOffBits + uiBytesPerAllRows;
			bmf.bfType = 0x4D42;

			FILE* pFile;
			if ((pFile = fopen (filename, "wb")) != NULL)
			{
				fwrite (&bmf, sizeof (bmf), 1, pFile);
				fwrite (&bmi, sizeof (bmi), 1, pFile);
				fwrite (pDataBits, sizeof (BYTE), uiBytesPerAllRows, pFile);
				fclose (pFile);
			}
			delete [] pDataBits;
		}
		DeleteObject (hImage);
	}

}


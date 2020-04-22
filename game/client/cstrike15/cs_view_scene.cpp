//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Responsible for drawing the scene
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "ivieweffects.h"
#include "iinput.h"
#include "model_types.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "viewrender.h"
#include "iclientmode.h"
#include "voice_status.h"
#include "radio_status.h"
#include "glow_overlay.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "detailobjectsystem.h"
#include "tier0/vprof.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "view_scene.h"
#include "particles_ez.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "cs_view_scene.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "shake.h"
#include "precache_register.h"
#include "engine/IEngineSound.h"
#include <vgui/ISurface.h>
#include "hltvreplaysystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheCSViewScene )
PRECACHE( MATERIAL, "effects/overlaysmoke" )
PRECACHE( MATERIAL, "effects/flashbang" )
PRECACHE( MATERIAL, "effects/flashbang_white" )
PRECACHE( MATERIAL, "effects/nightvision" )
PRECACHE_REGISTER_END()

static CCSViewRender g_ViewRender;
IViewRender *GetViewRenderInstance()
{
	return &g_ViewRender;
}


CCSViewRender::CCSViewRender()
{
	m_pFlashTexture = NULL;
}

void CCSViewRender::Init( void )
{
	CViewRender::Init();
}

void CCSViewRender::PerformNightVisionEffect( const CViewSetup &view )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return;

	if (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
	{
		CBaseEntity *target = pPlayer->GetObserverTarget();
		if (target && target->IsPlayer())
		{
			pPlayer = (C_CSPlayer *)target;
		}
	}

	if ( pPlayer && pPlayer->m_flNightVisionAlpha > 0 )
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/nightvision", TEXTURE_GROUP_CLIENT_EFFECTS, true );

		if ( pMaterial )
		{
			int iMaxValue = 255;
			byte overlaycolor[4] = { 0, 255, 0, 255 };
			
			UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height );

			if ( pPlayer->m_bNightVisionOn )
			{
				pPlayer->m_flNightVisionAlpha += 15;

				pPlayer->m_flNightVisionAlpha = MIN( pPlayer->m_flNightVisionAlpha, iMaxValue );
			}
			else 
			{
				pPlayer->m_flNightVisionAlpha -= 40;

				pPlayer->m_flNightVisionAlpha = MAX( pPlayer->m_flNightVisionAlpha, 0 );
			}

			overlaycolor[3] = pPlayer->m_flNightVisionAlpha;
	
			render->ViewDrawFade( overlaycolor, pMaterial );

			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
			render->ViewDrawFade( overlaycolor, pMaterial );
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
		}
	}
}


//Adrian - Super Nifty Flashbang Effect(tm)
// this does the burn in for the flashbang effect.
void CCSViewRender::PerformFlashbangEffect( const CViewSetup &view )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
		 return;

	C_CSPlayer *pFlashBangPlayer = pLocalPlayer;
	//bool bReduceEffect = false;

	float flAlphaScale = 1.0f;
	if ( pLocalPlayer->GetObserverMode() != OBS_MODE_NONE )
	{
		// If spectating, use values from target
		CBaseEntity *pTarget = pLocalPlayer->GetObserverTarget();
		if ( pTarget )
		{
			C_CSPlayer *pPlayerTmp = ToCSPlayer(pTarget);
			if ( pPlayerTmp )
			{
				pFlashBangPlayer = pPlayerTmp;
			}
		}

		// reduce the effect 
		if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && CanSeeSpectatorOnlyTools() )
		{
			// Reduce alpha. 
			// Note: the logic to reduce the duration of the effect is done when the effect is started (RecvProxy_FlashTime).
			flAlphaScale = 0.6f;
		}
		else if ( pLocalPlayer->GetObserverMode() == OBS_MODE_FIXED || pLocalPlayer->GetObserverMode() == OBS_MODE_ROAMING )
		{
			// Reduce alpha. 
			// Note: the logic to reduce the duration of the effect is done when the effect is started (RecvProxy_FlashTime).
			flAlphaScale = 0.2f;
		}
		else if ( pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
		{
			// Reduce alpha. 
			// Note: the logic to reduce the duration of the effect is done when the effect is started (RecvProxy_FlashTime).
			flAlphaScale = 0.6f;
		}
	}

	if ( !pFlashBangPlayer->IsFlashBangActive() || CSGameRules()->IsIntermission() )
	{
		if ( !pLocalPlayer->m_bFlashDspHasBeenCleared )
		{
			CLocalPlayerFilter filter;
			enginesound->SetPlayerDSP( filter, 0, true );
			pLocalPlayer->m_bFlashDspHasBeenCleared = true;
		}
		return;
	}

	// bandaid to insure that flashbang dsp effect doesn't continue on indefinitely
	if( pFlashBangPlayer->GetFlashTimeElapsed() > 1.6f && !pLocalPlayer->m_bFlashDspHasBeenCleared )
	{
		CLocalPlayerFilter filter;
		enginesound->SetPlayerDSP( filter, 0, true );
		pLocalPlayer->m_bFlashDspHasBeenCleared = true;
	}

	byte overlaycolor[4] = { 255, 255, 255, 255 };

	// draw the screenshot overlay portion of the flashbang effect
	IMaterial *pMaterial = materials->FindMaterial( "effects/flashbang", TEXTURE_GROUP_CLIENT_EFFECTS, true );
	if ( pMaterial )
	{
		// This is for handling split screen where we could potentially enter this function more than once a frame.
		// Since this bit of code grabs both the left and right viewports of the buffer, it only needs to be done once per frame per flash.
		static float lastTimeGrabbed = 0.0f;
		if ( gpGlobals->curtime == lastTimeGrabbed )
		{
			pLocalPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
			pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
		}

		if ( !pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed )
		{
			CMatRenderContextPtr pRenderContext( materials );
			int nScreenWidth, nScreenHeight;
			pRenderContext->GetRenderTargetDimensions( nScreenWidth, nScreenHeight );

			// update m_pFlashTexture
			lastTimeGrabbed = gpGlobals->curtime;
			bool foundVar;

			IMaterialVar* m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
			m_pFlashTexture = GetFullFrameFrameBufferTexture( 1 );

			// When grabbing the texture for the super imposed frame, we grab the whole buffer, not just the viewport.
			// We were having issues with trying to grab only the right side of the buffer for the second player in split screen.
			Rect_t srcRect;
			srcRect.x = 0;
			srcRect.y = 0;
			srcRect.width = nScreenWidth;
			srcRect.height = nScreenHeight;
			m_BaseTextureVar->SetTextureValue( m_pFlashTexture );
			pRenderContext->CopyRenderTargetToTextureEx( m_pFlashTexture, 0, &srcRect, NULL );
			pRenderContext->SetFrameBufferCopyTexture( m_pFlashTexture );

			pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
			pLocalPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
		}

		if ( !CanSeeSpectatorOnlyTools() )
		{
			overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = pFlashBangPlayer->m_flFlashScreenshotAlpha * flAlphaScale;
			if ( m_pFlashTexture != NULL )
			{
				static int NUM_AFTER_IMAGE_PASSES = 4;
				for ( int pass = 0; pass < NUM_AFTER_IMAGE_PASSES; ++pass )
				{
					render->ViewDrawFade( overlaycolor, pMaterial, false );
				}
			}
		}
	}

	// draw pure white overlay part of the flashbang effect.
	pMaterial = materials->FindMaterial( "effects/flashbang_white", TEXTURE_GROUP_CLIENT_EFFECTS, true );
	if ( pMaterial )
	{
		overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = pFlashBangPlayer->m_flFlashOverlayAlpha * flAlphaScale;
		render->ViewDrawFade( overlaycolor, pMaterial );
	}
}


#ifdef _DEBUG
	//These are the outer ranges of the smoke overlay volume, at which there is 0% overlay.
	ConVar cl_smoke_origin_height_cv( "cl_smoke_origin_height", "68", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_HIDDEN, "The center of the visible smoke torus is this many units above the origin of the grenade that's on the ground." );
	ConVar cl_smoke_torus_ring_radius_cv( "cl_smoke_torus_ring_radius", "61", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_HIDDEN, "The radius of the overall ring of the entire visible smoke volume, measured from the exact local center of the visible cloud." );
	ConVar cl_smoke_torus_ring_subradius_cv( "cl_smoke_torus_ring_subradius", "88", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_HIDDEN, "The radius of the extruded circle that follows the main ring and forms the torus shape." );
	ConVar cl_smoke_edge_feather_cv( "cl_smoke_edge_feather", "21", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_HIDDEN, "This many units towards the inner threshold and you will hit 100% overlay opacity." );
	ConVar cl_smoke_lower_speed_cv( "cl_smoke_lower_speed", "4.5", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_HIDDEN, "How fast the smoke screen overlay clears." );
	ConVar cl_show_smoke_overlay_thresholds( "cl_show_smoke_overlay_thresholds", "0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_HIDDEN, "Show visualization of smoke overlay inner and outer thresholds." );

	#define cl_smoke_origin_height cl_smoke_origin_height_cv.GetFloat()
	#define cl_smoke_torus_ring_radius cl_smoke_torus_ring_radius_cv.GetFloat()
	#define cl_smoke_torus_ring_subradius cl_smoke_torus_ring_subradius_cv.GetFloat()
	#define cl_smoke_edge_feather cl_smoke_edge_feather_cv.GetFloat()
	#define cl_smoke_lower_speed cl_smoke_lower_speed_cv.GetFloat()
#else
	#define cl_smoke_origin_height 68.0f
	#define cl_smoke_torus_ring_radius 61.0f
	#define cl_smoke_torus_ring_subradius 88.0f
	#define cl_smoke_edge_feather 21.0f
	#define cl_smoke_lower_speed 4.5f
#endif

//If your eyes are less than this distance from the center of the visible smoke torus, we should check exact distance and may need to apply the screen overlay.
#define cl_smoke_must_be_at_least_this_close_to_have_any_effect (cl_smoke_torus_ring_radius + cl_smoke_torus_ring_subradius)

void DrawCappedTorus( Vector vecOrigin, float flRadiusA, float flRadiusB, Color colWireColor, float flDuration )
{
	// Draw a debug wireframe 'capped' torus. It's a torus with the middle filled in, like a cheese-boule shape that's meant to match a smoke volume

	Vector vecPointA, vecPointB, vecPointC;
	vecPointA.Init();
	vecPointB.Init();
	vecPointC.Init();

	int i = 0;
	int j = 0;
	bool bTri = true;

	for ( int n=0; n<1643; n++ )
	{
		bTri ? i++ : j++;
		bTri = !bTri;

		float phi = i * 6.28318f / 41.0f;
		float theta = j * 6.28318f / 20.0f;
	
		Vector vecTorusNode;
		vecTorusNode.Init(	cos(phi) * ( flRadiusA + cos(theta) * flRadiusB ),
							sin(phi) * ( flRadiusA + cos(theta) * flRadiusB ),
							sin(theta) * flRadiusB );

		vecTorusNode += vecOrigin;

		vecPointC = vecPointB;
		vecPointB = vecPointA;
		vecPointA = vecTorusNode;

		if ( cos(theta) < 0 )
		{
			vecPointA.x = vecOrigin.x;
			vecPointA.y = vecOrigin.y;
			vecPointA.z = ( vecPointA.z > vecOrigin.z ) ? ( vecOrigin.z + flRadiusB ) : ( vecOrigin.z - flRadiusB );

			if ( vecPointA.x == vecPointB.x && vecPointA.y == vecPointB.y )
				continue;
		}

		if ( n > 1 )
			debugoverlay->AddLineOverlay( vecPointB, vecPointA, colWireColor.r(), colWireColor.g(), colWireColor.b(), false, flDuration );

		//triangulate
		//if ( n > 2 )
		//{
		//	if ( !bTri )
		//	{
		//		debugoverlay->AddTriangleOverlay( vecPointA, vecPointB, vecPointC, 255, 255, 255, 10, false, flDuration );
		//	}
		//	else
		//	{
		//		debugoverlay->AddTriangleOverlay( vecPointC, vecPointB, vecPointA, 255, 255, 255, 10, false, flDuration );
		//	}
		//}

	}

}

//-----------------------------------------------------------------------------
// Purpose: Renders pre-viewmodel smoke screen
//-----------------------------------------------------------------------------
#include "c_cs_player.h" // for clientSmokeGrenadeRecord_t
extern CUtlVector<EHANDLE> g_SmokeGrenadeHandles;
void CCSViewRender::RenderSmokeOverlay( bool bPreViewModel )
{
	if ( bPreViewModel )
	{
		// update the overlay

		//Assume we have no smoke overlay
		float flOptimalSmokeOverlayAlpha = 0;
	
		if ( g_SmokeGrenadeHandles.Count() > 0 )
		{
			Vector vecPlayerEyePos = MainViewOrigin(GET_ACTIVE_SPLITSCREEN_SLOT());

			Vector vecClosestVecToSmoke;

			int iClosestSmokeIndex = -1;
			float flTempSmokeDistance = cl_smoke_must_be_at_least_this_close_to_have_any_effect;

			#ifdef _DEBUG
			if ( cl_show_smoke_overlay_thresholds.GetBool() )
				flTempSmokeDistance = 2000.0f;
			#endif

			// we need to find the closest smoke to prevent a latter grenade in the list lifting the opacity of a potentially closer one
			for( int it=0; it < g_SmokeGrenadeHandles.Count(); it++ )
			{
				CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[it].Get() );
				if ( !pGrenade )
					continue;

				Vector toGrenade = ( pGrenade->GetAbsOrigin() + Vector( 0, 0, cl_smoke_origin_height) ) - vecPlayerEyePos;
				if ( toGrenade.Length() < flTempSmokeDistance )
				{
					//save the new closest smoke
					flTempSmokeDistance = toGrenade.Length();
					iClosestSmokeIndex = it;

					//remember its vector
					vecClosestVecToSmoke = toGrenade;
				}
			}

			//only continue if we actually found a close-enough smoke
			if ( iClosestSmokeIndex != -1 )
			{
				Vector vecSmokePos = Vector( 0, 0, 0 );
				CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[iClosestSmokeIndex].Get() );
				if ( pGrenade )
					vecSmokePos = pGrenade->GetAbsOrigin();

				//draw debug visualization
#ifdef _DEBUG
				if ( cl_show_smoke_overlay_thresholds.GetBool() )
				{
					Vector vecSmokeVisualOrigin = vecSmokePos + Vector( 0, 0, cl_smoke_origin_height );

					//draw inner threshold
					DrawCappedTorus( vecSmokeVisualOrigin, cl_smoke_torus_ring_radius, cl_smoke_torus_ring_subradius - cl_smoke_edge_feather, Color( 200, 0, 0 ), gpGlobals->frametime );

					//draw outer threshold
					DrawCappedTorus( vecSmokeVisualOrigin, cl_smoke_torus_ring_radius, cl_smoke_torus_ring_subradius, Color( 100, 0, 0 ), gpGlobals->frametime );
				}
#endif

				//linear interpolation between two capped torusoids

				//are we within the Z axis bounds?
				if ( abs( vecClosestVecToSmoke.z ) < cl_smoke_torus_ring_subradius )
				{

					float flvecClosestVecToSmokeLength2D = vecClosestVecToSmoke.Length2D();

					//are we within the cylindrical cap-space on the XY axis?
					if ( flvecClosestVecToSmokeLength2D < cl_smoke_torus_ring_radius )
					{
						//if so we can just use z delta
						flOptimalSmokeOverlayAlpha = ( cl_smoke_torus_ring_subradius - abs( vecClosestVecToSmoke.z ) ) / cl_smoke_edge_feather;

#ifdef _DEBUG
						if ( cl_show_smoke_overlay_thresholds.GetBool() )
						{
							//draw the closest point on the inner torusoid threshold
							Vector vecSmokeVisualOrigin = vecSmokePos + Vector( 0, 0, cl_smoke_origin_height );
							Vector vecClosestPoint = Vector( -vecClosestVecToSmoke.x, -vecClosestVecToSmoke.y, vecClosestVecToSmoke.z < 0 ? cl_smoke_torus_ring_subradius - cl_smoke_edge_feather : -cl_smoke_torus_ring_subradius + cl_smoke_edge_feather );

							debugoverlay->AddBoxOverlay( vecClosestPoint + vecSmokeVisualOrigin, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), QAngle( 0, 0, 0 ), 0, 200, 0, 255, gpGlobals->frametime );

							//line to eyes
							//debugoverlay->AddLineOverlay( vecPlayerEyePos, vecClosestPoint + vecSmokeVisualOrigin, 0,200,0, false, gpGlobals->frametime );
						}
#endif

					}
					else if ( flvecClosestVecToSmokeLength2D < cl_smoke_torus_ring_radius + cl_smoke_torus_ring_subradius )
					{
						//are we within the outer possible horizontal range of the torusoid? if so we need the distance to the nearest point on the primary radius
						Vector vecRingPosOnSmokePlane = Vector( vecClosestVecToSmoke.x, vecClosestVecToSmoke.y, 0 ).Normalized() * cl_smoke_torus_ring_radius;

#ifdef _DEBUG
						if ( cl_show_smoke_overlay_thresholds.GetBool() )
						{
							//draw the closest point on the inner torusoid threshold
							Vector vecRingPosToInnerSurface = ( vecClosestVecToSmoke - vecRingPosOnSmokePlane ).Normalized() * ( cl_smoke_torus_ring_subradius - cl_smoke_edge_feather );

							Vector vecSmokeVisualOrigin = vecSmokePos + Vector( 0, 0, cl_smoke_origin_height );
							debugoverlay->AddBoxOverlay( vecSmokeVisualOrigin - vecRingPosOnSmokePlane - vecRingPosToInnerSurface, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), QAngle( 0, 0, 0 ), 0, 200, 0, 255, gpGlobals->frametime );

							//line to eyes
							//debugoverlay->AddLineOverlay( vecPlayerEyePos, vecSmokeVisualOrigin - vecRingPosOnSmokePlane - vecRingPosToInnerSurface, 0,200,0, false, gpGlobals->frametime );
						}
#endif
						//and check the distance value
						float flDistanceToClosestRingPoint = ( vecClosestVecToSmoke - vecRingPosOnSmokePlane ).Length();
						if ( flDistanceToClosestRingPoint < cl_smoke_torus_ring_subradius )
						{
							flOptimalSmokeOverlayAlpha = ( cl_smoke_torus_ring_subradius - flDistanceToClosestRingPoint ) / cl_smoke_edge_feather;
						}
					}

					//clamp to 0-1 range
					flOptimalSmokeOverlayAlpha = clamp( flOptimalSmokeOverlayAlpha, 0.0f, 1.0f );
				}

				#ifdef _DEBUG
				if ( cl_show_smoke_overlay_thresholds.GetBool() )
				{
					debugoverlay->AddTextOverlay(vecPlayerEyePos, gpGlobals->frametime, "Overlay: %f", flOptimalSmokeOverlayAlpha );
				}
				#endif

			}
		}

		//alpha can instantly increase, but decreases to the ideal at a constant rate.
		if ( flOptimalSmokeOverlayAlpha < m_flSmokeOverlayAmount )
		{
			flOptimalSmokeOverlayAlpha = Approach( flOptimalSmokeOverlayAlpha, m_flSmokeOverlayAmount, gpGlobals->frametime * cl_smoke_lower_speed );
		}
		m_flSmokeOverlayAmount = flOptimalSmokeOverlayAlpha;

	}

	if ( m_flSmokeOverlayAmount <= 0 )
	{
		return;
	}
	else
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/overlaysmoke", TEXTURE_GROUP_CLIENT_EFFECTS, true );
		if ( pMaterial )
		{
			byte overlaycolor[4] = { 90, 90, 90, (byte)( m_flSmokeOverlayAmount * ( bPreViewModel ? 255 : 128 ) ) };
			render->ViewDrawFade( overlaycolor, pMaterial );
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CCSViewRender::Render2DEffectsPreHUD( const CViewSetup &view )
{
	PerformNightVisionEffect( view );	// this needs to come before the HUD is drawn, or it will wash the HUD out
}


//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CCSViewRender::Render2DEffectsPostHUD( const CViewSetup &view )
{
	PerformFlashbangEffect( view );

	float flFade = g_HltvReplaySystem.GetReplayVideoFadeAmount();

	// replay camera post-process
	if ( g_HltvReplaySystem.WantsReplayEffect() )
	{
		// demo playback uses full-screen quad to fade in/out. Hltv replay uses the pixel shader here.
		CMatRenderContextPtr pRenderContext( materials );		
		int x, y, w, h;
		pRenderContext->GetViewport( x, y, w, h );

		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 95 ) // bail if ps30 is unsupported
			return;

		UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height, false );

		// overlay
		IMaterial *pMatReplay = materials->FindMaterial( "dev/replay", TEXTURE_GROUP_OTHER, true );
		if ( pMatReplay )
		{
			IMaterialVar* pVar = pMatReplay->FindVar( "$c0_x", NULL );
			pVar->SetFloatValue( 1.0f / w );
	
			pVar = pMatReplay->FindVar( "$c0_y", NULL );
			pVar->SetFloatValue( 1.0f / h );

			pVar = pMatReplay->FindVar( "$c0_z", NULL );
			pVar->SetFloatValue( gpGlobals->realtime );

			pVar = pMatReplay->FindVar( "$c0_w", NULL );
			pVar->SetFloatValue( flFade );
			//if( flFade > 0 && g_HltvReplaySystem.GetHltvReplayDelay() ) Msg( "%.2f dev/replay fade %.2f\n", Plat_FloatTime(), flFade );	   // replayfade

			pRenderContext->DrawScreenSpaceRectangle( pMatReplay, x, y, w, h, 0, 0, w-1, h-1, w, h );
		}
	}
	else if ( flFade > 0 )
	{
		//Msg( "%.2f simple fade %.2f\n", Plat_FloatTime(), flFade ); // replayfade
		float flFadeAdj = flFade;// sinf( flFade * ( M_PI * .5f ) );
		CMatRenderContextPtr pRenderContext( materials );
		int x, y, w, h;
		pRenderContext->GetViewport( x, y, w, h );

		//bars
		if ( IMaterial *pMatFade = materials->FindMaterial( "dev/replay_bars", TEXTURE_GROUP_OTHER, true ) )
		{
			if ( IMaterialVar* pVar = pMatFade->FindVar( "$alpha", NULL ) )
			{
				pVar->SetFloatValue( flFadeAdj );
				pRenderContext->DrawScreenSpaceRectangle( pMatFade, 0, 0, w, h, 0, 0, 0, 0, 1, 1 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders voice feedback and other sprites attached to players
// Input  : none
//-----------------------------------------------------------------------------
void CCSViewRender::RenderPlayerSprites()
{
	CViewRender::RenderPlayerSprites();
	RadioManager()->DrawHeadLabels();
}


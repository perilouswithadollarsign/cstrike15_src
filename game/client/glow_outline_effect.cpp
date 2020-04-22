//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Functionality to render a glowing outline around client renderable objects.
//
//===============================================================================

#include "cbase.h"
#include "glow_outline_effect.h"
#include "model_types.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imaterialvar.h"
#include "view_shared.h"
#include "c_cs_player.h"
#include "tier2/renderutils.h"

#define FULL_FRAME_TEXTURE "_rt_FullFrameFB"

#define GLOWBOX_PASS_COLOR 0
#define GLOWBOX_PASS_STENCIL 1
#define GLOW_PULSE_DURATION 0.2f

ConVar glow_outline_effect_enable( "glow_outline_effect_enable", "1", FCVAR_CHEAT, "Enable entity outline glow effects." );
ConVar glow_outline_effect_width( "glow_outline_width", "6.0f", FCVAR_CHEAT, "Width of glow outline effect in screen space." );

ConVar glow_muzzle_debug( "glow_muzzle_debug", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show muzzle glow shapes outside of the glow pass." );


CGlowObjectManager &GlowObjectManager()
{
	static CGlowObjectManager s_GlowObjectManager;
	return s_GlowObjectManager;
}

void CGlowObjectManager::RenderGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot )
{
	if ( glow_outline_effect_enable.GetBool() )
	{
		CMatRenderContextPtr pRenderContext( materials );

		int nX, nY, nWidth, nHeight;
		pRenderContext->GetViewport( nX, nY, nWidth, nHeight );

		PIXEvent _pixEvent( pRenderContext, "EntityGlowEffects" );
		ApplyEntityGlowEffects( pSetup, nSplitScreenSlot, pRenderContext, glow_outline_effect_width.GetFloat(), nX, nY, nWidth, nHeight );
	}
}


static void SetRenderTargetAndViewPort( ITexture *rt, int w, int h )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget(rt);
	pRenderContext->Viewport(0,0,w,h);
}

void CGlowObjectManager::RenderGlowBoxes( int iPass, CMatRenderContextPtr &pRenderContext )
{
	for ( int n = m_GlowBoxDefinitions.Count() - 1; n >= 0 ; n-- )
	{
		if ( m_GlowBoxDefinitions[n].m_flTerminationTimeIndex < gpGlobals->curtime )
		{
			m_GlowBoxDefinitions.FastRemove(n);
		}
		else
		{
			float flLifeLeft = (m_GlowBoxDefinitions[n].m_flTerminationTimeIndex - gpGlobals->curtime) / (m_GlowBoxDefinitions[n].m_flTerminationTimeIndex - m_GlowBoxDefinitions[n].m_flBirthTimeIndex);
			if ( flLifeLeft > 0.95 )
				flLifeLeft = (0.05f - ( flLifeLeft - 0.95f )) / 0.05f; //fade in the first 5% of lifetime
			else
				flLifeLeft = MIN( flLifeLeft * 4.0f, 1.0f ); //fade out the last 25% of lifetime

			m_GlowBoxDefinitions[n].m_colColor[3] = flLifeLeft * 255;
			
			if ( iPass == GLOWBOX_PASS_COLOR )
			{
				Vector vecForward;
				AngleVectors( m_GlowBoxDefinitions[n].m_angOrientation, &vecForward );
				Vector vecLineEnd = m_GlowBoxDefinitions[n].m_vPosition + ( vecForward * m_GlowBoxDefinitions[n].m_vMaxs.x );

				RenderLine( m_GlowBoxDefinitions[n].m_vPosition, vecLineEnd, m_GlowBoxDefinitions[n].m_colColor, false );
			}
			else if ( iPass == GLOWBOX_PASS_STENCIL )
			{

				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				RenderBox( m_GlowBoxDefinitions[n].m_vPosition, m_GlowBoxDefinitions[n].m_angOrientation, m_GlowBoxDefinitions[n].m_vMins, m_GlowBoxDefinitions[n].m_vMaxs, m_GlowBoxDefinitions[n].m_colColor, false );
			}			
		}
	}
}

// *** Keep in sync with matsys_interface.cpp, where the texture is declared ***
// Resolution for glow target chosen to be the largest that we can fit in EDRAM after 720p color/depth textures.		
#define GLOW_360_RT_WIDTH ( MIN( 1120, pSetup->width ) )
#define GLOW_360_RT_HEIGHT ( MIN( 624, pSetup->height ) )

void CGlowObjectManager::RenderGlowModels( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, CUtlVector<GlowObjectDefinition_t> &vecGlowObjects )
{
	//==========================================================================================//
	// This renders solid pixels with the correct coloring for each object that needs the glow.	//
	// After this function returns, this image will then be blurred and added into the frame	//
	// buffer with the objects stenciled out.													//
	//==========================================================================================//
	pRenderContext->PushRenderTargetAndViewport();

	// Save modulation color and blend
	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );
	float flOrigBlend = render->GetBlend();

	ITexture *pRtFullFrame = materials->FindTexture( FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );

	if ( IsX360() )
	{
		ITexture *pRtGlowTexture360 = materials->FindTexture( "_rt_Glows360", TEXTURE_GROUP_RENDER_TARGET );

		SetRenderTargetAndViewPort( pRtGlowTexture360, GLOW_360_RT_WIDTH, GLOW_360_RT_HEIGHT );
	}
	else
	{
		SetRenderTargetAndViewPort( pRtFullFrame, pSetup->width, pSetup->height );
	}

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, false );

	// Set override material for glow color
	IMaterial *pMatGlowColor = NULL;

	pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );

	//==================//
	// Draw the objects //
	//==================//
	for ( int i = 0; i < vecGlowObjects.Count(); ++ i )
	{
		if ( vecGlowObjects[i].IsUnused() || !vecGlowObjects[i].ShouldDraw( nSplitScreenSlot ) || vecGlowObjects[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

		if ( vecGlowObjects[i].m_bFullBloomRender )
		{

			// Disabled because stencil test on off-screen buffers doesn't work with MSAA on.
			// Also, the normal model render does not seem to work on the off-screen buffer

			//g_pStudioRender->ForcedMaterialOverride( NULL );

			// 			ShaderStencilState_t stencilState;
			// 			stencilState.m_bEnable = true;
			// 			stencilState.m_nReferenceValue = vecGlowObjects[i].m_nFullBloomStencilTestValue;
			// 			stencilState.m_nTestMask = 0xFF;
			// 			stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			// 			stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			// 
			// 			pRenderContext->SetStencilState( stencilState );
		}
		else
		{

			// Disabled because stencil test on off-screen buffers doesn't work with MSAA on
			// Most features still work, but some (e.g. partial occlusion) don't
			// 			ShaderStencilState_t stencilState;
			// 			stencilState.m_bEnable = true;
			// 			stencilState.m_nReferenceValue = 1;
			// 			stencilState.m_nTestMask = 0x1;
			// 			stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			// 			stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			// 
			// 			pRenderContext->SetStencilState( stencilState );
		}

		render->SetBlend( vecGlowObjects[i].m_flGlowAlpha );
		Vector vGlowColor = vecGlowObjects[i].m_vGlowColor * vecGlowObjects[i].m_flGlowAlpha;

		// if pulse overdrive is non-zero, add its contribution to render alpha
		if ( vecGlowObjects[i].m_flGlowPulseOverdrive > 0 )
		{
			render->SetBlend( vecGlowObjects[i].m_flGlowAlpha + vecGlowObjects[i].m_flGlowPulseOverdrive );
		}

		// if set, cap glow alpha according to alpha of the non-glowing entity
		float flRenderAlpha = (float)vecGlowObjects[i].m_pEntity->GetRenderAlpha() * 0.00392;
		if ( vecGlowObjects[i].m_bGlowAlphaCappedByRenderAlpha && vecGlowObjects[i].m_flGlowAlpha > flRenderAlpha )
		{
			render->SetBlend( flRenderAlpha );
			vGlowColor *= flRenderAlpha;
		}

		// if set, alpha is multiplied by the ratio of entity velocity over the given maximum (e.g. faster = more opaque)
		if ( vecGlowObjects[i].m_flGlowAlphaFunctionOfMaxVelocity > 0.0f )
		{
			float flVelocityToAlpha = (vecGlowObjects[i].m_pEntity->GetAbsVelocity().Length() / vecGlowObjects[i].m_flGlowAlphaFunctionOfMaxVelocity);
			render->SetBlend( flVelocityToAlpha );
			vGlowColor *= flVelocityToAlpha;
		}

		// if set, cap cumulative glow alpha to a maximum value
		if ( render->GetBlend() > 0 && vecGlowObjects[i].m_flGlowAlphaMax < 1.0f )
		{
			float flCappedAlpha = MIN( render->GetBlend(), vecGlowObjects[i].m_flGlowAlphaMax );
			render->SetBlend( flCappedAlpha );
			vGlowColor *= flCappedAlpha;
		}

		render->SetColorModulation( &vGlowColor[0] ); // This only sets rgb, not alpha

		vecGlowObjects[i].DrawModel();

		// align and render the glow-only muzzle flash model for glowing weapon fire
		if ( vecGlowObjects[i].m_flGlowPulseOverdrive >= 0.25f )
		{
			C_CSPlayer* localPlayer = GetLocalOrInEyeCSPlayer();
			C_CSPlayer* tempPlayer = ToCSPlayer( vecGlowObjects[i].m_pEntity );
			if ( tempPlayer && localPlayer && (localPlayer->GetAbsOrigin() - tempPlayer->GetAbsOrigin()).Length() > 20 )
			{
				CWeaponCSBase* tempWeapon = tempPlayer->GetActiveCSWeapon();
				if ( tempWeapon )
				{
					//move muzzle flash shape to muzzle location
					if ( tempPlayer->m_hMuzzleFlashShape && !(tempWeapon->HasSilencer() && tempWeapon->IsSilenced()) )
					{
						tempPlayer->m_hMuzzleFlashShape->SetAbsOrigin( tempPlayer->m_vecLastMuzzleFlashPos );
						tempPlayer->m_hMuzzleFlashShape->SetAbsAngles( tempPlayer->m_angLastMuzzleFlashAngle );

						//pick a random flash shape
						//tempPlayer->m_hMuzzleFlashShape->SetBodygroup(0, RandomInt( 0, tempPlayer->m_hMuzzleFlashShape->GetNumBodyGroups()-1 ));

						//unhide and render the muzzle flash shape
						tempPlayer->m_hMuzzleFlashShape->RemoveEffects( EF_NODRAW );
						RenderableInstance_t instance;
						instance.m_nAlpha = (uint8)( vecGlowObjects[i].m_flGlowAlpha * 255.0f );
						tempPlayer->m_hMuzzleFlashShape->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
						if ( glow_muzzle_debug.GetInt() == 0 )
							tempPlayer->m_hMuzzleFlashShape->AddEffects( EF_NODRAW );
					}					
				}
			}
		}

		// dampen overdrive here. Do this at the end, otherwise our framerate may be low enough that we don't see the effect for even one frame
		if ( vecGlowObjects[i].m_flGlowPulseOverdrive > 0 )
		{
			vecGlowObjects[i].m_flGlowPulseOverdrive -= MAX(0, ( vecGlowObjects[i].m_flGlowPulseOverdrive * ( gpGlobals->frametime / GLOW_PULSE_DURATION ) ) ); //return to default over 1/5th a second
		}

	}

	RenderGlowBoxes(GLOWBOX_PASS_COLOR, pRenderContext);

	g_pStudioRender->ForcedMaterialOverride( NULL );
	render->SetColorModulation( vOrigColor.Base() );
	render->SetBlend( flOrigBlend );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	pRenderContext->SetStencilState( stencilStateDisable );

	if ( IsX360() )
	{
		Rect_t rect;
		rect.x = rect.y = 0;
		rect.width = GLOW_360_RT_WIDTH;
		rect.height = GLOW_360_RT_HEIGHT;

		pRenderContext->CopyRenderTargetToTextureEx( pRtFullFrame, 0, &rect, &rect );
	}

	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::DownSampleAndBlurRT( const CViewSetup *pSetup, CMatRenderContextPtr &pRenderContext, float flBloomScale, ITexture *pRtFullFrame, ITexture *pRtQuarterSize0, ITexture *pRtQuarterSize1 )
{
	static bool s_bFirstPass = true;

	//===================================
	// Setup state for downsample/bloom
	//===================================

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 16 ); // Max out pixel shader threads
#endif

	pRenderContext->PushRenderTargetAndViewport();

	// Get viewport
	int nSrcWidth = pSetup->width;
	int nSrcHeight = pSetup->height;
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

	// Get material and texture pointers
	IMaterial *pMatDownsample = materials->FindMaterial( "dev/glow_downsample", TEXTURE_GROUP_OTHER, true);
	IMaterial *pMatBlurX = materials->FindMaterial( "dev/glow_blur_x", TEXTURE_GROUP_OTHER, true );
	IMaterial *pMatBlurY = materials->FindMaterial( "dev/glow_blur_y", TEXTURE_GROUP_OTHER, true );

	

	//============================================
	// Downsample _rt_FullFrameFB to _rt_SmallFB0
	//============================================

	// First clear the full target to black if we're not going to touch every pixel
	if ( ( pRtQuarterSize0->GetActualWidth() != ( pSetup->width / 4 ) ) || ( pRtQuarterSize0->GetActualHeight() != ( pSetup->height / 4 ) ) )
	{
		SetRenderTargetAndViewPort( pRtQuarterSize0, pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight() );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );
	}

	// Set the viewport
	SetRenderTargetAndViewPort( pRtQuarterSize0, pSetup->width / 4, pSetup->height / 4 );

	IMaterialVar *pbloomexpvar = pMatDownsample->FindVar( "$bloomexp", 0 );
	if ( pbloomexpvar != NULL )
	{
		pbloomexpvar->SetFloatValue( 2.5f );
	}

	IMaterialVar *pbloomsaturationvar = pMatDownsample->FindVar( "$bloomsaturation", 0 );
	if ( pbloomsaturationvar != NULL )
	{
		pbloomsaturationvar->SetFloatValue( 1.0f );
	}

	// note the -2's below. Thats because we are downsampling on each axis and the shader
	// accesses pixels on both sides of the source coord
	int nFullFbWidth = nSrcWidth;
	int nFullFbHeight = nSrcHeight;
	if ( IsX360() )
	{
		nFullFbWidth = GLOW_360_RT_WIDTH;
		nFullFbHeight = GLOW_360_RT_HEIGHT;
	}
	pRenderContext->DrawScreenSpaceRectangle( pMatDownsample, 0, 0, nSrcWidth/4, nSrcHeight/4,
		0, 0, nFullFbWidth - 4, nFullFbHeight - 4,
		pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight() );

	if ( IsX360() )
	{
		// Need to reset viewport to full size so we can also copy the cleared black pixels around the border
		SetRenderTargetAndViewPort( pRtQuarterSize0, pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight() );
		pRenderContext->CopyRenderTargetToTextureEx( pRtQuarterSize0, 0, NULL, NULL );
	}

	//============================//
	// Guassian blur x rt0 to rt1 //
	//============================//

	// First clear the full target to black if we're not going to touch every pixel
	if ( s_bFirstPass || ( pRtQuarterSize1->GetActualWidth() != ( pSetup->width / 4 ) ) || ( pRtQuarterSize1->GetActualHeight() != ( pSetup->height / 4 ) ) )
	{
		// On the first render, this viewport may require clearing
		s_bFirstPass = false;
		SetRenderTargetAndViewPort( pRtQuarterSize1, pRtQuarterSize1->GetActualWidth(), pRtQuarterSize1->GetActualHeight() );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );
	}

	// Set the viewport
	SetRenderTargetAndViewPort( pRtQuarterSize1, pSetup->width / 4, pSetup->height / 4  );

	pRenderContext->DrawScreenSpaceRectangle( pMatBlurX, 0, 0, nSrcWidth/4, nSrcHeight/4,
		0, 0, nSrcWidth/4-1, nSrcHeight/4-1,
		pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight() );

	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( pRtQuarterSize1, 0, NULL, NULL );
	}

	//============================//
	// Gaussian blur y rt1 to rt0 //
	//============================//
	SetRenderTargetAndViewPort( pRtQuarterSize0, pSetup->width / 4, pSetup->height / 4 );
	IMaterialVar *pBloomAmountVar = pMatBlurY->FindVar( "$bloomamount", NULL );
	pBloomAmountVar->SetFloatValue( flBloomScale );
	pRenderContext->DrawScreenSpaceRectangle( pMatBlurY, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
		pRtQuarterSize1->GetActualWidth(), pRtQuarterSize1->GetActualHeight() );

	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( pRtQuarterSize1, 0, NULL, NULL ); // copy to rt1 instead of rt0 because rt1 has linear reads enabled and works more easily with screenspace_general to fix 360 bloom issues
	}

	// Pop RT
	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::ApplyEntityGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, float flBloomScale, int x, int y, int w, int h )
{
	

	// gather up special glow styles
	
	CUtlVector<GlowObjectDefinition_t> vecGlowObjectsRimGlow3DStyle;
	vecGlowObjectsRimGlow3DStyle.RemoveAll();
	
	CUtlVector<GlowObjectDefinition_t> vecGlowObjectsEdgeHighlightStyle;
	vecGlowObjectsEdgeHighlightStyle.RemoveAll();
	
	FOR_EACH_VEC_BACK( m_GlowObjectDefinitions, i )
	{
		if ( !m_GlowObjectDefinitions[i].IsUnused() && m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) )
		{
			if ( m_GlowObjectDefinitions[i].m_nRenderStyle == GLOWRENDERSTYLE_RIMGLOW3D )
			{
				vecGlowObjectsRimGlow3DStyle.AddToTail( m_GlowObjectDefinitions[i] );
			}
			else if ( m_GlowObjectDefinitions[i].m_nRenderStyle == GLOWRENDERSTYLE_EDGE_HIGHLIGHT || m_GlowObjectDefinitions[i].m_nRenderStyle == GLOWRENDERSTYLE_EDGE_HIGHLIGHT_PULSE )
			{
				vecGlowObjectsEdgeHighlightStyle.AddToTail( m_GlowObjectDefinitions[i] );
			}
		}
	}


	if ( vecGlowObjectsRimGlow3DStyle.Count() )
	{
		//todo: expose pulse width and frequency parameters
		float flPulse = 0.5f + 0.5f * sin( gpGlobals->curtime * 12.0f );

		IMaterial *pMatRim = materials->FindMaterial( "dev/glow_rim3d", TEXTURE_GROUP_OTHER, true );
		g_pStudioRender->ForcedMaterialOverride( pMatRim );

		for ( int i = 0; i < vecGlowObjectsRimGlow3DStyle.Count(); ++ i )
		{
			if ( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha <= 0 )
				continue;

			IMaterialVar *pMatVar = pMatRim->FindVar( "$envmaptint", 0 );
			if ( pMatVar != NULL )
			{
				pMatVar->SetVecComponentValue( clamp( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha * vecGlowObjectsRimGlow3DStyle[i].m_vGlowColor.x, 0, 1 ), 0 );
				pMatVar->SetVecComponentValue( clamp( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha * vecGlowObjectsRimGlow3DStyle[i].m_vGlowColor.y, 0, 1 ), 1 );
				pMatVar->SetVecComponentValue( clamp( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha * vecGlowObjectsRimGlow3DStyle[i].m_vGlowColor.z, 0, 1 ), 2 );
			}

			pMatVar = pMatRim->FindVar( "$envmapfresnelminmaxexp", 0 );
			if ( pMatVar != NULL )
			{
				pMatVar->SetVecComponentValue( 0, 0 );
				pMatVar->SetVecComponentValue( 1.5f, 1 );
				pMatVar->SetVecComponentValue( 3.0f + flPulse * 2.0f, 2 );
			}

			vecGlowObjectsRimGlow3DStyle[i].DrawModel();
		}

		g_pStudioRender->ForcedMaterialOverride( NULL );
	}
	
	

	if ( vecGlowObjectsEdgeHighlightStyle.Count() )
	{
		// render players into the fullscreen rt all white
		// render in-world 3d imposter players into scene using screenspace UV lookup from fullscreen RT, use glow object alpha and colorize here.
		// in the above step, multisample the rt to edge detect.

		// push the rt
		pRenderContext->PushRenderTargetAndViewport();

		// remember the color modulation so we can put it back again at the end
		Vector vOrigColor;
		render->GetColorModulation( vOrigColor.Base() );

		ITexture *pRtOutput = materials->FindTexture( "_rt_FullScreen", TEXTURE_GROUP_RENDER_TARGET );
		SetRenderTargetAndViewPort( pRtOutput, pSetup->width, pSetup->height );

		pRenderContext->ClearColor3ub( 0, 0, 0 );	
		pRenderContext->ClearBuffers( true, true, true );

		// Set override material for drawing the shapes
		IMaterial *pMatTeamIdShape = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
		g_pStudioRender->ForcedMaterialOverride( pMatTeamIdShape );
	
		// we want to render fully opaque, so bash blend to 1
		render->SetBlend( 1 );

		// set override color
		Vector vecOverrideColor = Vector( 1, 1, 1 );
		render->SetColorModulation( vecOverrideColor.Base() );

		// draw players
		FOR_EACH_VEC( vecGlowObjectsEdgeHighlightStyle, i )
		{		
			vecGlowObjectsEdgeHighlightStyle[i].DrawModel();
		}

		g_pStudioRender->ForcedMaterialOverride( NULL );

		pRenderContext->PopRenderTargetAndViewport();


		IMaterial *pMatTemp = materials->FindMaterial( "dev/glow_edge_highlight", TEXTURE_GROUP_OTHER, false );
		g_pStudioRender->ForcedMaterialOverride( pMatTemp );

		// draw players
		FOR_EACH_VEC( vecGlowObjectsEdgeHighlightStyle, i )
		{
			Vector vecTempColor = vecGlowObjectsEdgeHighlightStyle[i].m_vGlowColor * clamp( vecGlowObjectsEdgeHighlightStyle[i].m_flGlowAlpha, 0.0f, 1.0f ) * 1.4f; // boost a bit

			if ( vecGlowObjectsEdgeHighlightStyle[i].m_nRenderStyle == GLOWRENDERSTYLE_EDGE_HIGHLIGHT_PULSE )
			{
				float flPulse = 1.5f + 0.5f * sin( gpGlobals->curtime * 16.0f );
				vecTempColor *= (flPulse * 0.5f);
			}

			render->SetColorModulation( vecTempColor.Base() );
			vecGlowObjectsEdgeHighlightStyle[i].DrawModel();
		}

		// restore the modulation from before
		render->SetColorModulation( vOrigColor.Base() );
		
	}


	//=======================================================//
	// Render objects into stencil buffer					 //
	//=======================================================//

	// Set override shader to the same simple shader we use to render the glow models
	IMaterial *pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	float flSavedBlend = render->GetBlend();

	// Set alpha to 0 so we don't touch any color pixels
	render->SetBlend( 0.0f );
	pRenderContext->OverrideDepthEnable( true, false );

	RenderableInstance_t instance;
	instance.m_nAlpha = 255;

	int iNumGlowObjects = 0;

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++ i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) || m_GlowObjectDefinitions[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		// Full bloom rendered objects should not be stenciled out here
		if ( m_GlowObjectDefinitions[i].m_bFullBloomRender )
		{
			++ iNumGlowObjects;
			continue;
		}

		if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded || m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
		{
			if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded && m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				m_GlowObjectDefinitions[i].DrawModel();
			}
			else if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				m_GlowObjectDefinitions[i].DrawModel();
			}
			else if ( m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 2;
				stencilState.m_nTestMask = 0x1;
				stencilState.m_nWriteMask = 0x3;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
				stencilState.m_PassOp = SHADER_STENCILOP_INCREMENT_CLAMP;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				m_GlowObjectDefinitions[i].DrawModel();
			}
		}

		iNumGlowObjects++;
	}

	int iTempHealthBarRenderMaskIndex = 4;

	// Need to do a 2nd pass to warm stencil for objects which are rendered only when occluded
	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++ i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) || m_GlowObjectDefinitions[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		// Full bloom rendered objects should not be stenciled out here
		if ( m_GlowObjectDefinitions[i].m_bFullBloomRender )
			continue;

		if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded && !m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
		{
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = true;
			stencilState.m_nReferenceValue = 2;

			C_CSPlayer* pPlayer = ToCSPlayer( m_GlowObjectDefinitions[i].m_pEntity );
			if ( pPlayer )
			{
				pPlayer->m_iHealthBarRenderMaskIndex = iTempHealthBarRenderMaskIndex;
				stencilState.m_nReferenceValue = iTempHealthBarRenderMaskIndex;
				iTempHealthBarRenderMaskIndex ++;
			}

			stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
			stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			pRenderContext->SetStencilState( stencilState );

			m_GlowObjectDefinitions[i].DrawModel();
		}
	}

	RenderGlowBoxes(GLOWBOX_PASS_STENCIL, pRenderContext);
	iNumGlowObjects += m_GlowBoxDefinitions.Count();

	g_pStudioRender->ForcedMaterialOverride( NULL );
	render->SetBlend( 0.0f );

	pRenderContext->OverrideDepthEnable( true, false, false ); // health bars render over everything

	IMaterial *pMatGlowHealthColor = NULL;
	pMatGlowHealthColor = materials->FindMaterial( "dev/glow_health_color", TEXTURE_GROUP_OTHER, true );

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++ i )
	{

		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) || m_GlowObjectDefinitions[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		C_CSPlayer* pPlayer = ToCSPlayer( m_GlowObjectDefinitions[i].m_pEntity );
		if ( pPlayer && pPlayer != GetLocalOrInEyeCSPlayer() && pPlayer->IsAlive() )
		{
			
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = true;
			stencilState.m_nWriteMask = 0x0;
			stencilState.m_nTestMask = 0xFFFFFF;
			stencilState.m_nReferenceValue = pPlayer->m_iHealthBarRenderMaskIndex;
			stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			pRenderContext->SetStencilState( stencilState );

			if ( pPlayer->m_flHealthFadeAlpha > 0 )
				pPlayer->m_flHealthFadeAlpha -= gpGlobals->frametime * 0.4f;

			if ( pPlayer->m_flHealthFadeValue != pPlayer->GetHealth() )
				pPlayer->m_flHealthFadeAlpha = 1.0f;

			pPlayer->m_flHealthFadeValue = (float)pPlayer->GetHealth();

			if ( pPlayer->m_flHealthFadeAlpha > 0 ) //only need to update the effect if we can see it
			{
				float flGlowPulseSpeed = Lerp( pPlayer->m_flHealthFadeValue/100.0f, 30.0f, 10.0f );
				pMatGlowHealthColor->AlphaModulate( pPlayer->m_flHealthFadeAlpha * (0.4*(sin(flGlowPulseSpeed*gpGlobals->curtime)+1.4)) );

				Vector vecPlayerScreenSpaceOrigin;
				if ( ScreenTransform( pPlayer->GetAbsOrigin(), vecPlayerScreenSpaceOrigin ) == 0 )
				{

					ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceOrigin);

				float flHealthLeft = (100.0f-pPlayer->m_flHealthFadeValue)/100.0f;
					float flHealthHeightOffset = 72.0f;

				if ( pPlayer->GetFlags() & FL_DUCKING )
						flHealthHeightOffset = 55.0f;

					Vector vecPlayerScreenSpaceHealthPos;
					if ( ScreenTransform( pPlayer->GetAbsOrigin() + Vector (0,0,flHealthLeft*flHealthHeightOffset), vecPlayerScreenSpaceHealthPos ) == 0 )
				{

						ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceHealthPos);

						Vector vecPlayerScreenSpaceSizeA;
						ScreenTransform( pPlayer->GetAbsOrigin() + Vector(0,0,100), vecPlayerScreenSpaceSizeA );
						ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceSizeA);

						Vector vecPlayerScreenSpaceSizeB;
						ScreenTransform( pPlayer->GetAbsOrigin() + Vector(100,0,0), vecPlayerScreenSpaceSizeB );
						ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceSizeB);

						Vector vecPlayerScreenSpaceSizeAPixels;
						VectorSubtract( vecPlayerScreenSpaceOrigin, vecPlayerScreenSpaceSizeA, vecPlayerScreenSpaceSizeAPixels );
						Vector vecPlayerScreenSpaceSizeBPixels;
						VectorSubtract( vecPlayerScreenSpaceOrigin, vecPlayerScreenSpaceSizeB, vecPlayerScreenSpaceSizeBPixels );

						float flPlayerScreenSpaceCoverage = Max( vecPlayerScreenSpaceSizeAPixels.Length(), vecPlayerScreenSpaceSizeBPixels.Length() );

						if ( flPlayerScreenSpaceCoverage < ScreenHeight() * 2 )
						{
							float flHealthWidth = flPlayerScreenSpaceCoverage;
							float flHealthHeight = flPlayerScreenSpaceCoverage;

							float flHealthPosX = vecPlayerScreenSpaceHealthPos.x - (flPlayerScreenSpaceCoverage * 0.5);
							float flHealthPosY = vecPlayerScreenSpaceHealthPos.y;
							
							pRenderContext->DrawScreenSpaceRectangle( pMatGlowHealthColor, 
								flHealthPosX, flHealthPosY,
								flHealthWidth, flHealthHeight,
								0, 0, 0, 0, 1, 1 );

						}

					}

				}

			}

		}

	}


	//clear out custom render settings
	pRenderContext->OverrideDepthEnable( false, false );
	render->SetBlend( flSavedBlend );
	pRenderContext->SetStencilState( stencilStateDisable );

	// If there aren't any objects to glow, don't do all this other stuff
	// this fixes a bug where if there are glow objects in the list, but none of them are glowing,
	// the whole screen blooms.
	if ( iNumGlowObjects <= 0 )
		return;

	//=============================================
	// Render the glow colors to _rt_FullFrameFB 
	//=============================================
	{
		PIXEvent pixEvent( pRenderContext, "RenderGlowModels" );
		RenderGlowModels( pSetup, nSplitScreenSlot, pRenderContext, m_GlowObjectDefinitions );
	}

	ITexture *pRtFullFrame = materials->FindTexture( FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtQuarterSize0 = materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtQuarterSize1 = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
	
	DownSampleAndBlurRT( pSetup, pRenderContext, flBloomScale, pRtFullFrame, pRtQuarterSize0, pRtQuarterSize1 );


	{
		//=======================================================================================================//
		// At this point, pRtQuarterSize0 is filled with the fully colored glow around everything as solid glowy //
		// blobs. Now we need to stencil out the original objects by only writing pixels that have no            //
		// stencil bits set in the range we care about.                                                          //
		//=======================================================================================================//
		IMaterial *pMatHaloAddToScreen = materials->FindMaterial( "dev/halo_add_to_screen", TEXTURE_GROUP_OTHER, true );

		// Do not fade the glows out at all (weight = 1.0)
		IMaterialVar *pDimVar = pMatHaloAddToScreen->FindVar( "$C0_X", NULL );
		pDimVar->SetFloatValue( 1.0f );

		ShaderStencilState_t stencilState;
		stencilState.m_bEnable = true;
		stencilState.m_nWriteMask = 0x0; // We're not changing stencil
		//stencilState.m_nTestMask = 0x3;
		stencilState.m_nReferenceValue = 0x0;
		stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
		stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		pRenderContext->SetStencilState( stencilState );

		// Get viewport
		int nSrcWidth = pSetup->width;
		int nSrcHeight = pSetup->height;
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		// Draw quad
		pRenderContext->DrawScreenSpaceRectangle( pMatHaloAddToScreen, 0, 0, nViewportWidth, nViewportHeight,
			0.0f, -0.5f, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
			pRtQuarterSize1->GetActualWidth(),
			pRtQuarterSize1->GetActualHeight() );

		// Disable stencil
		pRenderContext->SetStencilState( stencilStateDisable );
	}

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	RenderableInstance_t instance;
	instance.m_nAlpha = (uint8)( m_flGlowAlpha * 255.0f );

	m_pEntity->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
	C_BaseEntity *pAttachment = m_pEntity->FirstMoveChild();

	while ( pAttachment != NULL )
	{
		if ( pAttachment->ShouldDraw() )
		{
			pAttachment->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
		}
		pAttachment = pAttachment->NextMovePeer();
	}
}

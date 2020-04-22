//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "transitionpanel.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ilocalize.h"
#include "vgui/iinput.h"
#include "ienginevgui.h"
#include "tier1/fmtstr.h"
#include "materialsystem/IMesh.h"
#include "shaderapi/ishaderapi.h"
#include "gameconsole.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

ConVar ui_transition_debug( "ui_transition_debug", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_transition_time( "ui_transition_time", IsGameConsole() ? "0.25" : "0.20", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_transition_delay( "ui_transition_delay", "0.3", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_transition_effect( "ui_transition_effect", "1", FCVAR_DEVELOPMENTONLY, "" );

#define TILE_NEAR_PLANE		1.0f
#define TILE_FAR_PLANE		257.0f
#define TILE_Z				( -128.0f )
#define GRID_WIDTH_WC		( 2.0f * -TILE_Z )
#define HALF_GRID_WIDTH_WC	( -TILE_Z )

CBaseModTransitionPanel::CBaseModTransitionPanel( const char *pPanelName ) :
	BaseClass( NULL, pPanelName )
{
	SetScheme( GAMEUI_BASEMODPANEL_SCHEME );
	SetPostChildPaintEnabled( true );

	m_pFromScreenRT = NULL;
	m_pCurrentScreenRT = NULL;

	// needs to start at some number >1
	m_nFrameCount = 100;
	m_nNumTransitions = 0;

	m_bTransitionActive = false;
	m_bAllowTransitions = false;
	m_bQuietTransitions = false;

	m_pFromScreenMaterial = materials->FindMaterial( "console/rt_background", TEXTURE_GROUP_OTHER, true );
	m_pFromScreenMaterial->IncrementReferenceCount();
	m_pFromScreenMaterial->GetMappingWidth();

	m_pCurrentScreenMaterial = materials->FindMaterial( "console/rt_foreground", TEXTURE_GROUP_OTHER, true );
	m_pCurrentScreenMaterial->IncrementReferenceCount();
	m_pCurrentScreenMaterial->GetMappingWidth();

	m_pFromScreenRT = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	m_pCurrentScreenRT = materials->FindTexture( "_rt_DepthDoubler", TEXTURE_GROUP_RENDER_TARGET );

	m_bForwardHint = true;
	m_WindowTypeHint = WT_NONE;
	m_PreviousWindowTypeHint = WT_NONE;
	m_flDirection = 1.0f;
}

CBaseModTransitionPanel::~CBaseModTransitionPanel()
{
}

void CBaseModTransitionPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_nTileWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TileWidth" ) ) );
	m_nTileHeight = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TileHeight" ) ) );

	m_nPinFromBottom = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.PinFromBottom" ) ) );
	m_nPinFromLeft = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.PinFromLeft" ) ) );

	BuildTiles();

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	SetPos( 0, 0 );
	SetSize( screenWide, screenTall );
}

void CBaseModTransitionPanel::OnKeyCodePressed( KeyCode keycode )
{
}

void CBaseModTransitionPanel::BuildTiles()
{
	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
	float flInverseAspect = 1.0f/aspectRatioInfo.m_flFrameBufferAspectRatio;

	m_nNumColumns = ( screenWide + m_nTileWidth - 1 ) / m_nTileWidth;
	m_nNumRows = ( screenTall + m_nTileHeight - 1 ) / m_nTileHeight;

	m_nXOffset = m_nPinFromLeft % m_nTileWidth;
	if ( m_nXOffset )
	{
		m_nXOffset -= m_nTileWidth;
		m_nNumColumns++;
	}

	m_nYOffset = ( screenTall - m_nPinFromBottom ) % m_nTileHeight;
	if ( m_nYOffset )
	{
		m_nYOffset -= m_nTileHeight;
		m_nNumRows++;
	}

	m_Tiles.SetCount( m_nNumRows * m_nNumColumns );

	float flTileWidth =  ( (float)m_nTileWidth/(float)screenWide ) * GRID_WIDTH_WC;
	float flTileHeight = ( (float)m_nTileHeight/(float)screenTall ) * GRID_WIDTH_WC;

	int nIndex = 0;
	int y = m_nYOffset;
	for ( int row = 0; row < m_nNumRows; row++ )
	{
		int x = m_nXOffset;
		for ( int col = 0; col < m_nNumColumns; col++ )
		{
			float flx = (float)(GRID_WIDTH_WC * x)/(float)screenWide - HALF_GRID_WIDTH_WC;
			float fly = HALF_GRID_WIDTH_WC - (float)(GRID_WIDTH_WC * y)/(float)screenTall;

			if ( IsX360() || IsPlatformWindowsPC() )
			{
				// need to do 1/2 pixel push due to dx9 pixel centers
				flx -= 0.5f/(float)screenWide * GRID_WIDTH_WC;
				fly += 0.5f/(float)screenTall * GRID_WIDTH_WC;
			}

			float s0 = (float)x / (float)screenWide;
			float s1 = (float)(x + m_nTileWidth) / (float)screenWide;
			float t0 = (float)y / (float)screenTall;
			float t1 = (float)(y + m_nTileHeight) / (float)screenTall;

			// clockwise winding from ul,ur,lr,ll
			m_Tiles[nIndex].m_RectParms.m_Position0.x = flx;
			m_Tiles[nIndex].m_RectParms.m_Position0.y = fly * flInverseAspect;
			m_Tiles[nIndex].m_RectParms.m_Position0.z = TILE_Z;
			m_Tiles[nIndex].m_RectParms.m_TexCoord0.x = s0;
			m_Tiles[nIndex].m_RectParms.m_TexCoord0.y = t0;

			m_Tiles[nIndex].m_RectParms.m_Position1.x = flx + flTileWidth;
			m_Tiles[nIndex].m_RectParms.m_Position1.y = fly * flInverseAspect;
			m_Tiles[nIndex].m_RectParms.m_Position1.z = TILE_Z;
			m_Tiles[nIndex].m_RectParms.m_TexCoord1.x = s1;
			m_Tiles[nIndex].m_RectParms.m_TexCoord1.y = t0;

			m_Tiles[nIndex].m_RectParms.m_Position2.x = flx + flTileWidth;
			m_Tiles[nIndex].m_RectParms.m_Position2.y = ( fly - flTileHeight ) * flInverseAspect;
			m_Tiles[nIndex].m_RectParms.m_Position2.z = TILE_Z;
			m_Tiles[nIndex].m_RectParms.m_TexCoord2.x = s1;
			m_Tiles[nIndex].m_RectParms.m_TexCoord2.y = t1;

			m_Tiles[nIndex].m_RectParms.m_Position3.x = flx;
			m_Tiles[nIndex].m_RectParms.m_Position3.y = ( fly - flTileHeight ) * flInverseAspect;
			m_Tiles[nIndex].m_RectParms.m_Position3.z = TILE_Z;
			m_Tiles[nIndex].m_RectParms.m_TexCoord3.x = s0;
			m_Tiles[nIndex].m_RectParms.m_TexCoord3.y = t1;

			m_Tiles[nIndex].m_RectParms.m_Center = ( m_Tiles[nIndex].m_RectParms.m_Position0 + m_Tiles[nIndex].m_RectParms.m_Position2 ) / 2.0f;

			x += m_nTileWidth;
			nIndex++;
		}

		y += m_nTileHeight;
	}
}

void CBaseModTransitionPanel::SetExpectedDirection( bool bForward, WINDOW_TYPE wt )
{
	// direction can only be determined by taking hints from the caller
	// at the moment the transition is triggered, we have to trust this
	m_bForwardHint = bForward;

	// can't rely on this going backwards where the nav gets subverted
	// this is used to isolate some edge states for windows that aren't tile based
	// attract and mainmenu don't have tiles but still need to drive transitioning TO tile based screens
	if ( m_PreviousWindowTypeHint != m_WindowTypeHint )
	{
		m_PreviousWindowTypeHint = m_WindowTypeHint;
	}
	m_WindowTypeHint = wt;
}

int CBaseModTransitionPanel::GetTileIndex( int x, int y )
{
	int nTile = ( y - m_nYOffset ) / m_nTileHeight * m_nNumColumns + ( x - m_nXOffset ) / m_nTileWidth;
	if ( !m_Tiles.IsValidIndex( nTile ) )
		return -1;

	return nTile;
}

void CBaseModTransitionPanel::TouchTile( int nTile, WINDOW_TYPE wt, bool bForce )
{
	if ( !m_Tiles.IsValidIndex( nTile ) )
		return;

	// touch tile
	m_Tiles[nTile].m_nCurrentWindow = wt;
	m_Tiles[nTile].m_nFrameCount = m_nFrameCount;

	if ( !m_nNumTransitions && wt == WT_MAINMENU )
	{
		// A special case due to mainmenu not being tile based, but floating text,
		// so prevent the main menu from doing the effect on its first entrance after a cleared state.
		// i.e. The main menu should not be the first transition from an unexpected previous state. An unknown
		// previous state would be exiting the game, the screen that we transition from is likely blending the movie in,
		// not known or guaranteed to get a stable snap. The main menu transition (as the first transition) would look
		// "wrong" because the screen snap would be a frame of unintended graphics.
		//
		// EXCEPT...
		// There could have been a confirmation window that was opened before the gameui gets activated, which gives us a known frame to transition from.
		// This is valid and occurs at least with a disconnect, where the confirmation is opened, but the ui is about to be activated.
		// The gameui gets activated, which clears the transition state (as expected), and the UI immediately shows the confirmation.
		// In this case, as the confirmation is dismissed, and the main menu is activated, we want the flip to occur.
		// AND...
		// Going from an 'initial state' from the attract screen, we allow the main menu to do a transition, because the attract screen
		// provided a known stable previous state.
		//
		
		bool bPreventTransition =	( m_Tiles[nTile].m_nPreviousWindow == WT_NONE )
									&& !( m_bForwardHint && m_PreviousWindowTypeHint == WT_ATTRACTSCREEN );

#if defined( PORTAL2_PUZZLEMAKER )
		// FIXME: Hack for weird menu glitch leaving the editor in certain cases
		bPreventTransition = bPreventTransition && ( m_PreviousWindowTypeHint != WT_EDITORMAINMENU );
#endif
					
		if ( bPreventTransition )
		{
			// inhibit the transition by avoiding the dirty state
			m_Tiles[nTile].m_nPreviousWindow = m_Tiles[nTile].m_nCurrentWindow;
		}
	}

	if ( wt == WT_NONE || bForce )
	{
		// special behavior to force flip a tile to the background
		// fiddle with the frame count so logic treats this as not-the-current-dialog's tile (same as dialog shrinking)
		// due to NONE, the tile will flip and clear itself
		m_Tiles[nTile].m_nFrameCount--;
	}
}

void CBaseModTransitionPanel::MarkTile( int x, int y, WINDOW_TYPE wt, bool bForce )
{
	if ( !IsEffectEnabled() )
		return;

	TouchTile( GetTileIndex( x, y ), wt, bForce );
}

void CBaseModTransitionPanel::MarkTilesInRect( int x, int y, int wide, int tall, WINDOW_TYPE wt, bool bForce )
{
	if ( !IsEffectEnabled() )
		return;

	if ( wide == -1 && tall == -1 )
	{
		// hint to use screen extents
		int screenWide, screenTall;
		surface()->GetScreenSize( screenWide, screenTall );

		wide = screenWide;
		tall = screenTall;
	}

	int nRowStartTile = GetTileIndex( x, y );
	int nRowEndTile = GetTileIndex( x + wide, y );
	int nEndTile = GetTileIndex( x + wide, y + tall );
	
	int nTile = nRowStartTile;
	do 
	{
		for ( int i = 0; i <= nRowEndTile - nRowStartTile; i++ )
		{
			TouchTile( nTile + i, wt, bForce );
		}
		nTile += m_nNumColumns;
	}
	while ( nTile < nEndTile );
}

void CBaseModTransitionPanel::PreventTransitions( bool bPrevent )
{
	if ( bPrevent )
	{
		m_bAllowTransitions = false;
	}
	else if ( !m_bAllowTransitions )
	{
		// only reset if we were disabled
		SetInitialState();
	}
}

void CBaseModTransitionPanel::SuspendTransitions( bool bSuspend )
{
	// quiet transitions still maintain state, but do no graphical effect
	// but can continue the graphical effect as needed
	m_bQuietTransitions = bSuspend;
}

void CBaseModTransitionPanel::TerminateEffect()
{
	if ( !m_bTransitionActive )
		return;

	m_bTransitionActive = false;

	for ( int i = m_Tiles.Count() - 1; i >= 0; i-- )
	{
		// only stopping the effect, the window type states MUST stay preserved
		m_Tiles[i].m_bDirty = false;
		m_Tiles[i].m_flStartTime = 0;
		m_Tiles[i].m_flEndTime = 0;
	}

	m_Sounds.Purge();

	if ( GetVPanel() == vgui::input()->GetModalSubTree() )
	{
		vgui::input()->ReleaseModalSubTree();
	}
}

void CBaseModTransitionPanel::SetInitialState()
{
	m_bAllowTransitions = true;
	m_bQuietTransitions = false;

	m_bForwardHint = true;
	m_WindowTypeHint = WT_NONE;
	m_PreviousWindowTypeHint = WT_NONE;

	m_Sounds.Purge();

	for ( int i = m_Tiles.Count() - 1; i >= 0; i-- )
	{
		m_Tiles[i].m_bDirty = false;
		m_Tiles[i].m_flStartTime = 0;
		m_Tiles[i].m_flEndTime = 0;
		m_Tiles[i].m_nCurrentWindow = WT_NONE;
		m_Tiles[i].m_nPreviousWindow = WT_NONE;
		m_Tiles[i].m_nFrameCount = 0;
	}

	m_nNumTransitions = 0;
}

void CBaseModTransitionPanel::ScanTilesForTransition()
{
	// track incoming state needed for edge-triggers
	bool bTransitionActive = m_bTransitionActive;

	// scan to start effect
	for ( int i = m_Tiles.Count() - 1; i >= 0; i-- )
	{
		if ( m_Tiles[i].m_nPreviousWindow != m_Tiles[i].m_nCurrentWindow )
		{
			if ( !m_Tiles[i].m_bDirty || m_Tiles[i].m_nFrameCount == m_nFrameCount )
			{
				// the window changed marks a tile going to a new tile
				m_Tiles[i].m_nPreviousWindow = m_Tiles[i].m_nCurrentWindow;
				if ( m_Tiles[i].m_nCurrentWindow == WT_NONE )
				{
					m_Tiles[i].m_nFrameCount = 0;
				}

				if ( !m_bQuietTransitions )
				{
					m_Tiles[i].m_bDirty = true;
					m_bTransitionActive = true;
				}
			}
		}
		else if ( !m_Tiles[i].m_bDirty && m_Tiles[i].m_nFrameCount && m_Tiles[i].m_nFrameCount != m_nFrameCount )
		{
			//  a stale frame count denotes a tile returning to the background
			m_Tiles[i].m_nPreviousWindow = WT_NONE;
			m_Tiles[i].m_nCurrentWindow = WT_NONE;
			m_Tiles[i].m_nFrameCount = 0;

			if ( !m_bQuietTransitions )
			{
				m_Tiles[i].m_bDirty = true;
				m_bTransitionActive = true;
			}
		}
	}

	if ( !bTransitionActive )
	{
		m_nFrameCount++;
	}

	bool bOverlayActive = ( CUIGameData::Get() && ( CUIGameData::Get()->IsXUIOpen() || CUIGameData::Get()->IsSteamOverlayActive() ) ) ||
							CBaseModPanel::GetSingleton().IsOpaqueOverlayActive();
	if ( m_bTransitionActive && ( bOverlayActive || !m_bAllowTransitions ) )
	{
		// when the overlay is active, abort the starting the starting effect
		// this is done to ensure the states are tracked, but the effect is prevented
		// this is due to windows changing behind the overlay, due to sign outs, etc
		// the window states are still tracked, so when the overlay goes away, no effect gets triggered
		TerminateEffect();
	}

	if ( m_bTransitionActive )
	{
		if ( !bTransitionActive )
		{
			m_nNumTransitions++;

			vgui::input()->SetModalSubTree( GetVPanel(), GetVPanel(), true );
			vgui::input()->SetModalSubTreeShowMouse( true );

			// snap off the current expected direction
			m_flDirection = m_bForwardHint ? 1.0f : -1.0f;
		}

		// seed the tiles
		// wanted a forward marching of tiles top to bottom
		float flEffectTime = Plat_FloatTime();
		float flOffsetTime = ui_transition_delay.GetFloat() * ui_transition_time.GetFloat();
		float flNextTime = 0;
		float flRowTime = 0;

		for ( int nRow = 0; nRow < m_nNumRows; nRow++ )
		{
			bool bRowHasDirtyTile = false;

			int nCol = ( m_flDirection > 0 ) ? 0 : m_nNumColumns - 1;
			int nColEnd = ( m_flDirection > 0 ) ? m_nNumColumns : -1;
			int nColStep = ( m_flDirection > 0 ) ? 1 : -1;

			while ( nCol != nColEnd )
			{
				int nIndex = nRow * m_nNumColumns + nCol;

				if ( !m_Tiles[nIndex].m_flStartTime && m_Tiles[nIndex].m_bDirty )
				{
					m_Tiles[nIndex].m_flStartTime = flEffectTime + flNextTime;
					m_Tiles[nIndex].m_flEndTime = m_Tiles[nIndex].m_flStartTime + ui_transition_time.GetFloat();
					flNextTime += flOffsetTime; 

					bRowHasDirtyTile = true;

					m_Sounds.AddToTail( nIndex );
				}

				nCol += nColStep;
			}

			if ( bRowHasDirtyTile )
			{
				flRowTime += flOffsetTime;
				flNextTime = flRowTime;
			}
		}
	}
}

bool CBaseModTransitionPanel::IsEffectEnabled()
{
	if ( !m_bAllowTransitions ||
		!ui_transition_effect.GetBool() ||
		!enginevgui->IsGameUIVisible() ||
		GameUI().IsInLevel() || 
		engine->IsConnected() ||
		CBaseModPanel::GetSingleton().IsLevelLoading() ||
		( !IsGameConsole() && GameConsole().IsConsoleVisible() ) ||
		materials->IsStereoActiveThisFrame() ) // Disable effect when in nvidia's stereo mode
	{
		// effect not allowed in game or loading into game
		if ( m_bTransitionActive )
		{
			// one-time clean up if some other state changed us immediately
			TerminateEffect();
		}

		return false;
	}

	return true;
}

bool CBaseModTransitionPanel::IsEffectActive()
{
	return m_bTransitionActive;
}

void CBaseModTransitionPanel::SaveCurrentScreen( ITexture *pRenderTarget )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->CopyRenderTargetToTextureEx( pRenderTarget, 0, NULL, NULL );
	pRenderContext->SetFrameBufferCopyTexture( pRenderTarget, 0 );
}

void CBaseModTransitionPanel::DrawEffect()
{
	float flEffectTime = Plat_FloatTime();

	bool bFinished = true;
	for ( int i = 0; i < m_Tiles.Count(); i++ )
	{
		if ( m_Tiles[i].m_bDirty )
		{
			float flLerp = RemapValClamped( flEffectTime, m_Tiles[i].m_flStartTime, m_Tiles[i].m_flEndTime, 0, 1.0f );

			m_Tiles[i].m_RectParms.m_flLerp = flLerp;

			if ( flLerp != 1.0f )
			{
				bFinished = false;
			}
			else
			{
				m_Tiles[i].m_bDirty = false;
				m_Tiles[i].m_flStartTime = 0;
				m_Tiles[i].m_flEndTime = 0;
			}
		}
	}

	// scan to start any sounds
	if ( !bFinished )
	{
		for ( int i = m_Sounds.Count() - 1; i >= 0 ; i-- )
		{
			int nTileIndex = m_Sounds[i];
			if ( !m_Tiles.IsValidIndex( nTileIndex ) )
				continue;
			
			if ( flEffectTime < m_Tiles[nTileIndex].m_flStartTime )
			{
				// not ready to start
				continue;
			}

			if ( m_Tiles[nTileIndex].m_flStartTime && flEffectTime >= m_Tiles[nTileIndex].m_flStartTime )
			{
				// trigger sound
				UISound_t uiSound = ( rand() % 2 ) ? UISOUND_TILE_CLICK1 : UISOUND_TILE_CLICK2;
				CBaseModPanel::GetSingleton().PlayUISound( uiSound );
			}

			// this tile's sound only gets triggered once
			m_Sounds.FastRemove( i );
		}
	}

	int screenWide, screenTall;
	GetSize( screenWide, screenTall );
	surface()->DrawSetColor( 0, 0, 0, 255 );
	surface()->DrawFilledRect( 0, 0, screenWide, screenTall );

	StartPaint3D();

	DrawTiles3D();

	EndPaint3D();

	if ( bFinished )
	{
		// effect over
		m_bTransitionActive = false;

		m_Sounds.Purge();

		if ( GetVPanel() == vgui::input()->GetModalSubTree() )
		{
			vgui::input()->ReleaseModalSubTree();
		}
	}
}

void CBaseModTransitionPanel::Paint()
{
	ScanTilesForTransition();

	if ( m_bTransitionActive )
	{
		SaveCurrentScreen( m_pCurrentScreenRT );
		DrawEffect();
	}
}

void CBaseModTransitionPanel::PostChildPaint()
{
	if ( !m_bTransitionActive )
	{
		// keep saving the current frame buffer to use as the 'from'
		SaveCurrentScreen( m_pFromScreenRT );
	}
}

void CBaseModTransitionPanel::StartPaint3D()
{
	// Save off the matrices in case the painting method changes them.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
	pRenderContext->PerspectiveX( 90, aspectRatioInfo.m_flFrameBufferAspectRatio, TILE_NEAR_PLANE, TILE_FAR_PLANE );

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	// Don't draw the 3D scene w/ stencil
	ShaderStencilState_t state;
	state.m_bEnable = false;
	pRenderContext->SetStencilState( state );

	pRenderContext->ClearBuffers( false, true, true );
	pRenderContext->OverrideDepthEnable( true, true, true );
}

void CBaseModTransitionPanel::EndPaint3D()
{
	// Reset stencil to set stencil everywhere we draw 
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( !IsGameConsole() )
	{
		pRenderContext->OverrideDepthEnable( false, true, true );
	}

	ShaderStencilState_t state;
	state.m_bEnable = true;
	state.m_FailOp = SHADER_STENCILOP_KEEP;
	state.m_ZFailOp = SHADER_STENCILOP_KEEP;
	state.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	state.m_CompareFunc = SHADER_STENCILFUNC_GEQUAL;
	state.m_nReferenceValue = 0;
	state.m_nTestMask = 0xFFFFFFFF;
	state.m_nWriteMask = 0xFFFFFFFF;
	pRenderContext->SetStencilState( state );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	surface()->DrawSetTexture( -1 );
}

void CBaseModTransitionPanel::DrawTiles3D()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	for ( int i = 0; i < m_Tiles.Count(); i++ )
	{
		// draw the tile using the current material
		// dirty tile's will alter as needed
		IMaterial *pMaterial = m_pCurrentScreenMaterial;

		Vector4D vecPrimaryColor = Vector4D( 1, 1, 1, 1 );
		if ( ui_transition_debug.GetBool() )
		{
			vecPrimaryColor = Vector4D( 0.8, 0.8, 1, 1 );
		}

		Vector4D vecBrighterEdgeColor = vecPrimaryColor;
		Vector4D vecDarkerEdgeColor =  vecPrimaryColor;

		bool bLeftEdgeIsBrighter = true;

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->LoadIdentity();

		if ( m_Tiles[i].m_bDirty )
		{
			float flLerp = m_Tiles[i].m_RectParms.m_flLerp;
			float flAngle = RemapValClamped( flLerp, 0, 1.0f, 0, m_flDirection * 180.0f );

			// rotate the tile's normal (pointing toward viewer) to determine front/back visibility
			Vector vecNormal = Vector( 0, 0, 1 );
			QAngle angleRotation = QAngle( flAngle, 0, 0 ); 
			Vector vecOutNormal;
			VectorRotate( vecNormal, angleRotation, vecOutNormal );

			// dot with essentially the eye vector (which is at 0,0,0)
			float flDot = -DotProduct( vecOutNormal, m_Tiles[i].m_RectParms.m_Center.Normalized() );
			if ( flDot < 0 )
			{
				// backside is visible, flip to keep the texcoords in CW screenspace order
				flAngle += 180.0f;
			}

			// rotate the tile
			pRenderContext->Translate( m_Tiles[i].m_RectParms.m_Center.x, m_Tiles[i].m_RectParms.m_Center.y, m_Tiles[i].m_RectParms.m_Center.z );
			pRenderContext->Rotate( flAngle, 0, 1, 0 );
			pRenderContext->Translate( -m_Tiles[i].m_RectParms.m_Center.x, -m_Tiles[i].m_RectParms.m_Center.y, -m_Tiles[i].m_RectParms.m_Center.z );

			if ( flDot >= 0 )
			{
				pMaterial = m_pFromScreenMaterial;

				if ( ui_transition_debug.GetBool() )
				{
					vecPrimaryColor = Vector4D( 1, 0.8, 0.8, 1 );
				}
			}

			// do a cheap lighting occlusion by simply darkening the interior edge
			float flColorLerp;
			if ( flLerp < 0.5f )
			{
				flColorLerp = RemapValClamped( flLerp, 0, 0.5f, 1.0f, 0 );
			}
			else
			{
				flColorLerp = RemapValClamped( flLerp, 0.5f, 1.0, 0, 1.0f );
			}

			vecBrighterEdgeColor = vecPrimaryColor;
			vecDarkerEdgeColor.x = vecBrighterEdgeColor.x * flColorLerp;
			vecDarkerEdgeColor.y = vecBrighterEdgeColor.y * flColorLerp;
			vecDarkerEdgeColor.z = vecBrighterEdgeColor.z * flColorLerp;
			vecDarkerEdgeColor.w = vecBrighterEdgeColor.w;

			// classify the interior edge
			if ( m_flDirection == 1.0f )
			{
				bLeftEdgeIsBrighter = ( flDot > 0 );
			}
			else
			{
				bLeftEdgeIsBrighter = ( flDot <= 0 );
			}
		}

		IMesh* pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, pMaterial );
		CMeshBuilder meshBuilder;

		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3fv( m_Tiles[i].m_RectParms.m_Position0.Base() );
		meshBuilder.Color4fv( bLeftEdgeIsBrighter ? vecBrighterEdgeColor.Base() : vecDarkerEdgeColor.Base() );
		meshBuilder.TexCoord2fv( 0, m_Tiles[i].m_RectParms.m_TexCoord0.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( m_Tiles[i].m_RectParms.m_Position1.Base() );
		meshBuilder.Color4fv( !bLeftEdgeIsBrighter ? vecBrighterEdgeColor.Base() : vecDarkerEdgeColor.Base()  );
		meshBuilder.TexCoord2fv( 0, m_Tiles[i].m_RectParms.m_TexCoord1.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( m_Tiles[i].m_RectParms.m_Position2.Base() );
		meshBuilder.Color4fv( !bLeftEdgeIsBrighter ? vecBrighterEdgeColor.Base() : vecDarkerEdgeColor.Base()  );
		meshBuilder.TexCoord2fv( 0, m_Tiles[i].m_RectParms.m_TexCoord2.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( m_Tiles[i].m_RectParms.m_Position3.Base() );
		meshBuilder.Color4fv( bLeftEdgeIsBrighter ? vecBrighterEdgeColor.Base() : vecDarkerEdgeColor.Base()  );
		meshBuilder.TexCoord2fv( 0, m_Tiles[i].m_RectParms.m_TexCoord3.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

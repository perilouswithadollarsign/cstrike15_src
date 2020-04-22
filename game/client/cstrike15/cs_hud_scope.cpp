//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "c_cs_player.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <keyvalues.h>
#include <vgui_controls/AnimationController.h>
#include "predicted_viewmodel.h"
#include "HUD/sfhudreticle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cl_flinch_compensate_crosshair;
extern ConVar cl_crosshair_sniper_width;
ConVar cl_crosshair_sniper_show_normal_inaccuracy( "cl_crosshair_sniper_show_normal_inaccuracy", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "Include standing inaccuracy when determining sniper crosshair blur" );


//extern ConVar cl_flinch_scale;

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudScope : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudScope, vgui::Panel );

public:
	explicit CHudScope( const char *pElementName );
	
	void	Init( void );
	void	LevelInit( void );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );

private:
	CMaterialReference m_ScopeMaterial;	
	CMaterialReference m_ScopeLineBlurMaterial;	
	CMaterialReference m_DustOverlayMaterial;

	int m_iScopeArcTexture;
	int m_iScopeLineBlurTexture;
	int m_iScopeDustTexture;

	float m_fAnimInset;
	float m_fLineSpreadDistance;
};

DECLARE_HUDELEMENT_DEPTH( CHudScope, 70 );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudScope::CHudScope( const char *pElementName ) : CHudElement(pElementName), BaseClass(NULL, "HudScope")
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );
	
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	SetIgnoreGlobalHudDisable( true );

	m_fAnimInset = 1;
	m_fLineSpreadDistance = 1;
}

//-----------------------------------------------------------------------------
// Purpose: standard hud element init function
//-----------------------------------------------------------------------------
void CHudScope::Init( void )
{
	m_iScopeArcTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_iScopeArcTexture, "sprites/scope_arc", true, false);

	m_iScopeLineBlurTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_iScopeLineBlurTexture, "sprites/scope_line_blur", true, false);

	m_iScopeDustTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_iScopeDustTexture, "overlays/scope_lens", true, false);
}

//-----------------------------------------------------------------------------
// Purpose: standard hud element init function
//-----------------------------------------------------------------------------
void CHudScope::LevelInit( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudScope::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);

	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);
	SetBounds(0, 0, screenWide, screenTall);
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudScope::Paint( void )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer == NULL )
		return;

	if ( pPlayer && pPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
		return;

	switch ( pPlayer->GetObserverMode() )
	{
	case OBS_MODE_NONE:
		break;

	case OBS_MODE_IN_EYE:
		pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
		if ( pPlayer == NULL )
			return;
		break;

	default:
		return;		// no scope for other observer modes
	}

	CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
		
	if( !pWeapon || pWeapon->GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE )
		return;

	Assert( m_iScopeArcTexture );
	Assert( m_iScopeLineBlurTexture );
	Assert( m_iScopeDustTexture );

	const float kScopeMinFOV = 25.0f; // Clamp scope FOV to this value to prevent blur from getting too big when double-scoped
	float flTargetFOVForZoom = MAX( pWeapon->GetZoomFOV( pWeapon->GetCSZoomLevel() ), kScopeMinFOV );
	if ( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped == false )
	{
		m_fAnimInset = 2;
		m_fLineSpreadDistance = 20;
	}

	// see if we're zoomed with a sniper rifle
	if( flTargetFOVForZoom != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped )
	{
		int screenWide, screenTall;
		GetHudSize( screenWide, screenTall );

		CBaseViewModel *baseViewModel = pPlayer->GetViewModel( 0 );
		if ( baseViewModel == NULL )
			return;
		CPredictedViewModel *viewModel = dynamic_cast<CPredictedViewModel *>(baseViewModel);
		if ( viewModel == NULL )
			return;

		float fHalfFov = DEG2RAD( flTargetFOVForZoom ) * 0.5f;
		float fInaccuracyIn640x480Pixels = 320.0f / tanf( fHalfFov ); // 640 = "reference screen width"

		// Get the weapon's inaccuracy
		float fWeaponInaccuracy = pWeapon->GetInaccuracy() + pWeapon->GetSpread();

		// Optional: Ignore "default" inaccuracy
		if ( !cl_crosshair_sniper_show_normal_inaccuracy.GetBool() )
			fWeaponInaccuracy -= pWeapon->GetInaccuracyStand( Secondary_Mode ) + pWeapon->GetSpread();

		fWeaponInaccuracy = MAX( fWeaponInaccuracy, 0 );

		float fRawSpreadDistance = fWeaponInaccuracy * fInaccuracyIn640x480Pixels; // 
		float fSpreadDistance = clamp( fRawSpreadDistance, 0, 100 );

#if 0
		// This lets you verify inaccuracy vs screenshots of cl_weapon_debug_show_accuracy 2;
		// the number after screen= should max the radius (in pixels) of the drawn circle.
		float fInaccuracyInScreenPixels = fRawSpreadDistance * screenTall / 480; // 480 = "reference screen width"
		Msg( "fWeaponInaccuracy = %8.5f, referenceScreen = %8.5f, screen = %8.5f, fov = %8.5f\n", fWeaponInaccuracy, fRawSpreadDistance, fInaccuracyInScreenPixels, flTargetFOVForZoom );
#endif

		// reduce the goal  (* 0.4 / 30.0f)
		// then animate towards it at speed 19.0f
		// (where did these numbers come from?)
		float flInsetGoal = fSpreadDistance * (0.4f / 30.0f);
		m_fAnimInset = Approach( flInsetGoal, m_fAnimInset, abs( ( ( flInsetGoal )-m_fAnimInset )*gpGlobals->frametime ) * 19.0f );

		// Approach speed chosen so we get 90% there in 3 frames if we are running at 192 fps vs a 64tick client/server.
		// If our fps is lower we will reach the target faster, if higher it is slightly slower
		// (since this is a framerate-dependent approach function).
		m_fLineSpreadDistance = RemapValClamped( gpGlobals->frametime * 140.0f, 0.0f, 1.0f, m_fLineSpreadDistance, fRawSpreadDistance );

		float flAccuracyFishtail = pWeapon->GetAccuracyFishtail();
		int offsetX = viewModel->GetBobState().m_flRawLateralBob * (screenTall/14) + flAccuracyFishtail;
		int offsetY = viewModel->GetBobState().m_flRawVerticalBob * (screenTall/14);

		float flInacDisplayBlur = m_fAnimInset * 0.04f;
		if ( flInacDisplayBlur > 0.22 )
			flInacDisplayBlur = 0.22;

		// calculate the bounds in which we should draw the scope
		int inset = (screenTall / 14) + (flInacDisplayBlur * (screenTall*0.5));
		int y1 = inset;
		int x1 = (screenWide - screenTall) / 2 + inset; 
		int y2 = screenTall - inset;
		int x2 = screenWide - x1;

		y1 += offsetY;
		y2 += offsetY;
		x1 += offsetX;
		x2 += offsetX;

		int x = (screenWide / 2) + offsetX;
		int y = (screenTall / 2) + offsetY;

		float uv1 = 0.5f / 256.0f, uv2 = 1.0f - uv1;

		vgui::Vertex_t vert[4];	

		Vector2D uv11( uv1, uv1 );
		Vector2D uv12( uv1, uv2 );
		Vector2D uv21( uv2, uv1 );
		Vector2D uv22( uv2, uv2 );

		//Msg( "flRawInacc = %f, flAnimInset = %f\n", flRawInacc, m_fAnimInset );

		int xMod = ( screenWide / 2 ) + offsetX + (flInacDisplayBlur * screenTall);
		int yMod = ( screenTall / 2 ) + offsetY + (flInacDisplayBlur * screenTall);

		int iMiddleX = (screenWide / 2 ) + offsetX;
		int iMiddleY = (screenTall / 2 ) + offsetY;

		vgui::surface()->DrawSetTexture( m_iScopeDustTexture );
		vgui::surface()->DrawSetColor( 255, 255, 255, 200 );

		vert[0].Init( Vector2D( iMiddleX + xMod, iMiddleY + yMod ), uv21 );
		vert[1].Init( Vector2D( iMiddleX - xMod, iMiddleY + yMod ), uv11 );
		vert[2].Init( Vector2D( iMiddleX - xMod, iMiddleY - yMod ), uv12 );
		vert[3].Init( Vector2D( iMiddleX + xMod, iMiddleY - yMod ), uv22 );
		vgui::surface()->DrawTexturedPolygon( 4, vert );

		//Only sniper rifles use this style of vgui hud scope
		//if (pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE)
		{
			// The powf here makes the blur not quite spread out quite as much as the actual inaccuracy;
			// doing so is a bit too sudden and also leads to just a huge blur because the snipers are
			// *extremely* inaccurate while scoped and moving.  This way we get a slightly smoother animation
			// as well as not quite blowing up the blurred area by such a large amount.
			float fBlurWidth = powf(m_fLineSpreadDistance, 0.75f);
			float fScreenBlurWidth = fBlurWidth * screenTall / 640.0f;  // scale from 'reference screen size' to actual screen
			
			int nSniperCrosshairThickness = cl_crosshair_sniper_width.GetInt();
			if ( nSniperCrosshairThickness < 1 )
				nSniperCrosshairThickness = 1;

			float kMaxVarianceWithFullAlpha = 1.8f; // Tuned to look good
			float fBlurAlpha;
			if ( fScreenBlurWidth <= nSniperCrosshairThickness + 0.5f )
				fBlurAlpha = ( fBlurWidth < 1.0f ) ? 1.0f : 1.0f / fBlurWidth;
			else
				fBlurAlpha = ( fBlurWidth < kMaxVarianceWithFullAlpha ) ? 1.0f : kMaxVarianceWithFullAlpha / fBlurWidth;

			// This is a break from physical reality to make the look a bit better.  An actual Gaussian
			// blur spreads the energy out over the entire blurred area, dropping the total opacity by the amount
			// of the spread.  However, this leads to not being able to see the effect at all.  We solve this in 
			// 2 ways:
			//   (1) use sqrt on the alpha to bring it closer to 1, kind of like a gamma curve.
			//   (2) clamp the alpha at the lower end to 55% to make sure you can see *something* no matter
			//       how spread out it gets.
			fBlurAlpha = sqrtf( fBlurAlpha );
			int iBlurAlpha = Clamp( ( int )( fBlurAlpha * 255.0f ), 140, 255 );

			//DevMsg( "blur: %8.5f fov: %8.5f alpha: %8.5f\n", fBlurWidth, flTargetFOVForZoom, fBlurAlpha );

			if ( fScreenBlurWidth <= nSniperCrosshairThickness + 0.5f )
			{
				vgui::surface()->DrawSetColor( 0, 0, 0, iBlurAlpha );

				//Draw the reticle with primitives
				if ( nSniperCrosshairThickness <= 1 )
				{
					vgui::surface()->DrawLine( 0, y, screenWide + offsetX, y );
					vgui::surface()->DrawLine( x, 0, x, screenTall + offsetY );
				}
				else
				{
					int nStep = nSniperCrosshairThickness / 2;
					vgui::surface()->DrawFilledRect( 0, y - nStep, screenWide + offsetX, y + nSniperCrosshairThickness - nStep );
					vgui::surface()->DrawFilledRect( x - nStep, 0, x + nSniperCrosshairThickness - nStep, screenTall + offsetY );
				}

			}
			else
			{
				// Draw blurred line
				vgui::surface()->DrawSetColor( 0, 0, 0, iBlurAlpha );
				vgui::surface()->DrawSetTexture( m_iScopeLineBlurTexture );

				// vertical blurred line
				vert[0].Init( Vector2D( iMiddleX - fScreenBlurWidth, offsetY ), uv11 );
				vert[1].Init( Vector2D( iMiddleX + fScreenBlurWidth, offsetY ), uv21 );
				vert[2].Init( Vector2D( iMiddleX + fScreenBlurWidth, screenTall + offsetY ), uv22 );
				vert[3].Init( Vector2D( iMiddleX - fScreenBlurWidth, screenTall + offsetY ), uv12 );
				vgui::surface()->DrawTexturedPolygon( 4, vert );

				// horizontal blurred line
				vert[0].Init( Vector2D( screenWide + offsetX, iMiddleY - fScreenBlurWidth ), uv12 );
				vert[1].Init( Vector2D ( screenWide + offsetX, iMiddleY + fScreenBlurWidth ), uv22 );
				vert[2].Init( Vector2D( offsetX, iMiddleY + fScreenBlurWidth ), uv21 );
				vert[3].Init( Vector2D( offsetX, iMiddleY - fScreenBlurWidth ), uv11 );
				vgui::surface()->DrawTexturedPolygon(4, vert);
			}


			//vgui::surface()->DrawSetColor(0,0,0,MAX( 128, 255 - (int)(m_flOldInacc*3000)));
			vgui::surface()->DrawSetColor(0,0,0,255);
			//Draw the outline
			vgui::surface()->DrawSetTexture(m_iScopeArcTexture);

			// bottom right
			vert[0].Init( Vector2D( x, y ), uv11 );
			vert[1].Init( Vector2D( x2, y ), uv21 );
			vert[2].Init( Vector2D( x2, y2 ), uv22 );
			vert[3].Init( Vector2D( x, y2 ), uv12 );
			vgui::surface()->DrawTexturedPolygon( 4, vert );

			// top right
			vert[0].Init( Vector2D( x - 1, y1 ), uv12 );
			vert[1].Init( Vector2D ( x2, y1 ), uv22 );
			vert[2].Init( Vector2D( x2, y + 1 ), uv21 );
			vert[3].Init( Vector2D( x - 1, y + 1 ), uv11 );
			vgui::surface()->DrawTexturedPolygon(4, vert);

			// bottom left
			vert[0].Init( Vector2D( x1, y ), uv21 );
			vert[1].Init( Vector2D( x, y ), uv11 );
			vert[2].Init( Vector2D( x, y2 ), uv12 );
			vert[3].Init( Vector2D( x1, y2), uv22 );
			vgui::surface()->DrawTexturedPolygon(4, vert);

			// top left
			vert[0].Init( Vector2D( x1, y1 ), uv22 );
			vert[1].Init( Vector2D( x, y1 ), uv12 );
			vert[2].Init( Vector2D( x, y ), uv11 );
			vert[3].Init( Vector2D( x1, y ), uv21 );
			vgui::surface()->DrawTexturedPolygon(4, vert);

			vgui::surface()->DrawFilledRect(0, 0, screenWide, y1);				// top
			vgui::surface()->DrawFilledRect(0, y2, screenWide, screenTall);		// bottom
			vgui::surface()->DrawFilledRect(0, y1, x1, screenTall);				// left
			vgui::surface()->DrawFilledRect(x2, y1, screenWide, screenTall);	// right
		}
	}
}

//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_crosshair.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "ivrenderview.h"

#ifdef PORTAL2
#include "ivieweffects.h"
#include "c_basehlplayer.h"
#endif // PORTAL2

#ifdef SIXENSE
#include "sixense/in_sixense.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if !defined( CSTRIKE15 )

ConVar crosshair( "crosshair", "1", FCVAR_ARCHIVE );
ConVar cl_observercrosshair( "cl_observercrosshair", "1", FCVAR_ARCHIVE );

#endif


using namespace vgui;

int ScreenTransform( const Vector& point, Vector& screen );

DECLARE_HUDELEMENT( CHudCrosshair );

CHudCrosshair::CHudCrosshair( const char *pElementName ) :
  CHudElement( pElementName ), BaseClass( NULL, "HudCrosshair" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	m_pCrosshair = 0;

	m_clrCrosshair = Color( 0, 0, 0, 0 );

	m_vecCrossHairOffsetAngle.Init();

	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

void CHudCrosshair::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pDefaultCrosshair = HudIcons().GetIcon( "crosshair_default" );
	SetPaintBackgroundEnabled( false );

    SetSize( ScreenWidth(), ScreenHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudCrosshair::ShouldDraw( void )
{
#if defined ( CSTRIKE15 )
	return false;
#else

	bool bNeedsDraw;

	if ( m_bHideCrosshair )
		return false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( pWeapon && !pWeapon->ShouldDrawCrosshair() )
		return false;

	/* disabled to avoid assuming it's an HL2 player.
	// suppress crosshair in zoom.
	if ( pPlayer->m_HL2Local.m_bZooming )
		return false;
	*/

	// draw a crosshair only if alive or spectating in eye
	if ( IsGameConsole() )
	{
		bNeedsDraw = m_pCrosshair &&
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() &&
			( !pPlayer->IsSuitEquipped() || g_pGameRules->IsMultiplayer() ) &&
			GetClientMode()->ShouldDrawCrosshair() &&
			!( pPlayer->GetFlags() & FL_FROZEN ) &&
			( pPlayer->IsViewEntity() ) &&
			( pPlayer->IsAlive() ||	( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) || ( cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING ) );
	}
	else
	{
		bNeedsDraw = m_pCrosshair &&
			crosshair.GetInt() &&
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() &&
			GetClientMode()->ShouldDrawCrosshair() &&
			!( pPlayer->GetFlags() & FL_FROZEN ) &&
			( pPlayer->IsViewEntity() ) &&
			!pPlayer->IsInVGuiInputMode() &&
			( pPlayer->IsAlive() ||	( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) || ( cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING ) );
	}

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
#endif
}

void CHudCrosshair::Paint( void )
{
	if ( !m_pCrosshair )
		return;

	if ( !IsCurrentViewAccessAllowed() )
		return;

#ifdef SIXENSE
	float x=0, y=0;

	if( g_pSixenseInput->IsEnabled() && C_BasePlayer::GetLocalPlayer() && (C_BasePlayer::GetLocalPlayer()->GetObserverMode()==OBS_MODE_NONE) )
	{
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		if ( player != NULL )
		{

			// Never autoaim a predicted weapon (for now)
			Vector	aimVector;
			AngleVectors( CurrentViewAngles() - g_pSixenseInput->GetViewAngleOffset(), &aimVector );

			// calculate where the bullet would go so we can draw the cross appropriately
			Vector vecStart = player->Weapon_ShootPosition();
			Vector vecEnd = player->Weapon_ShootPosition() + aimVector * MAX_TRACE_LENGTH;


			trace_t tr;
			UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, player, COLLISION_GROUP_NONE, &tr );

			Vector screen;
			screen.Init();
			ScreenTransform(tr.endpos, screen);

			x = ScreenWidth() / 2;
			y = ScreenHeight() / 2;

			x += 0.5 * screen[0] * ScreenWidth() + 0.5;
			y += 0.5 * screen[1] * ScreenHeight() + 0.5;
			y = ScreenHeight() - y;
		}

	} 
	else 
	{
		x = ScreenWidth() / 2;
		y = ScreenHeight() / 2;
	}

#else
	float x, y;
	x = ScreenWidth()/2;
	y = ScreenHeight()/2;
#endif

	float flApparentZ = vgui::STEREO_NOOP;
	bool bStereoActive = materials->IsStereoActiveThisFrame();

	m_curViewAngles = CurrentViewAngles();
	m_curViewOrigin = CurrentViewOrigin();

	Vector screen;
	screen.Init();

	// TrackIR
	if ( IsHeadTrackingEnabled() )
	{
		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( !pPlayer )
			return;

		// TrackIR
		// get the direction the player is aiming
		Vector aimVector = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		// calculate where the bullet would go so we can draw the cross appropriately
		Vector vecEnd = pPlayer->Weapon_ShootPosition() + aimVector * MAX_TRACE_LENGTH;

		trace_t tr;
		UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

		QAngle angles;
		Vector forward;
		Vector point;

		// this code is wrong
		angles = m_curViewAngles + m_vecCrossHairOffsetAngle;
		AngleVectors( angles, &forward );

		// need to project forward into an object to see how far this 
		// vector should be!!
		//forward *= 1000;

		//VectorAdd( m_curViewOrigin, forward, point );
		//ScreenTransform( point, screen );

		if ( bStereoActive && ( !tr.allsolid || !tr.startsolid ) )
		{
			// NOTE: This isn't exactly right, because the trace above starts with the gun and 
			// not with the camera origin, so this will be slightly off. It shouldn't really matter.
			flApparentZ = ( tr.endpos - tr.startpos ).Length();
		}

		ScreenTransform( tr.endpos, screen );
	}
	// TrackIR
	else
	{
		Vector forward;

		// MattB - m_vecCrossHairOffsetAngle is the autoaim angle.
		// if we're not using autoaim, just draw in the middle of the 
		// screen
		if ( m_vecCrossHairOffsetAngle != vec3_angle )
		{
			QAngle angles;
			Vector point;

			// this code is wrong
			angles = m_curViewAngles + m_vecCrossHairOffsetAngle;
			AngleVectors( angles, &forward );
			VectorAdd( m_curViewOrigin, forward, point );
			ScreenTransform( point, screen );
		}
		else
		{
			AngleVectors( m_curViewAngles, &forward );
		}

		if ( bStereoActive )
		{
			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			if ( pPlayer && m_clrCrosshair.a() != 0.0f ) // if we have a player and we're not going to ignore the results...
			{
				Vector vecEnd = m_curViewOrigin + ( forward * MAX_TRACE_LENGTH );

				trace_t tr;
				UTIL_TraceLine( m_curViewOrigin, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

				if ( !tr.allsolid || !tr.startsolid )
				{
					// NOTE: This isn't exactly right, because the trace above starts with the gun and 
					// not with the camera origin, so this will be slightly off. It shouldn't really matter.
					flApparentZ = ( tr.endpos - tr.startpos ).Length();
				}
			}
		}
	}

	x += 0.5f * screen[0] * ScreenWidth() + 0.5f;
	y += 0.5f * screen[1] * ScreenHeight() + 0.5f;

#ifdef PORTAL2
	// Find any full-screen fades
	byte color[4];
	bool blend;
	GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );
	m_clrCrosshair[3] = SimpleSplineRemapValClamped( color[3], 0, 64, 255, 0 );
#endif // PORTAL2

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	float flWeaponScale = 1.f;
	float flW = m_pCrosshair->Width();
	float flH = m_pCrosshair->Height();
	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->GetWeaponCrosshairScale( flWeaponScale );
	}

	m_pCrosshair->DrawSelfCropped( 
		x - 0.5f * m_pCrosshair->Width() * flWeaponScale + 0.5, 
		y - 0.5f * m_pCrosshair->Height() * flWeaponScale + 0.5,
		0, 0, flW, flH, flW*flWeaponScale, flH*flWeaponScale,
		m_clrCrosshair );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudCrosshair::SetCrosshairAngle( const QAngle& angle )
{
	VectorCopy( angle, m_vecCrossHairOffsetAngle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudCrosshair::SetCrosshair( CHudTexture *texture, const Color& clr )
{
	m_pCrosshair = texture;
	m_clrCrosshair = clr;
}

//-----------------------------------------------------------------------------
// Purpose: Resets the crosshair back to the default
//-----------------------------------------------------------------------------
void CHudCrosshair::ResetCrosshair()
{
	SetCrosshair( m_pDefaultCrosshair, Color( 255, 255, 255, 255 ) );
}

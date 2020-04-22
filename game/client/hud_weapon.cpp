//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "hud_crosshair.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#if defined( CSTRIKE15 )

extern bool IsTakingAFreezecamScreenshot( void );

extern ConVar cl_drawhud;
//extern ConVar sfcrosshair;
extern ConVar crosshair;
extern ConVar cl_crosshairstyle;

#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudWeapon : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeapon, vgui::Panel );
public:
	explicit CHudWeapon( const char *pElementName );

	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );
	virtual void	PerformLayout();
	virtual bool    ShouldDraw();

private:
	CHudCrosshair *m_pCrosshair;
};

DECLARE_HUDELEMENT( CHudWeapon );

CHudWeapon::CHudWeapon( const char *pElementName ) :
  CHudElement( pElementName ), BaseClass( NULL, "HudWeapon" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	m_pCrosshair = NULL;

	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

bool CHudWeapon::ShouldDraw()
{
#if defined( CSTRIKE15 )

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	if ( !crosshair.GetBool() || cl_crosshairstyle.GetInt() < 2 || IsTakingAFreezecamScreenshot() || !cl_drawhud.GetBool() )
	//if ( !crosshair.GetBool() || sfcrosshair.GetBool() || IsTakingAFreezecamScreenshot() || !cl_drawhud.GetBool() )
	{
		return false;
	}

#endif

	return CHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scheme - 
//-----------------------------------------------------------------------------
void CHudWeapon::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );

	m_pCrosshair = GET_HUDELEMENT( CHudCrosshair );
	//Assert( m_pCrosshair );
}

//-----------------------------------------------------------------------------
// Performs layout
//-----------------------------------------------------------------------------
void CHudWeapon::PerformLayout()
{
	BaseClass::PerformLayout();

	vgui::Panel *pParent = GetParent();

	int w, h;
	pParent->GetSize( w, h );
	SetPos( 0, 0 );
	SetSize( w, h );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeapon::Paint( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return;

	MDLCACHE_CRITICAL_SECTION();

	C_BaseCombatWeapon *pWeapon = player->GetActiveWeapon();

		// Draw the targeting zone around the pCrosshair
	
	if ( pWeapon )
	{
		pWeapon->Redraw();
	}
	else
	{

		if ( m_pCrosshair )
		{
			m_pCrosshair->ResetCrosshair();
		}
	}
}

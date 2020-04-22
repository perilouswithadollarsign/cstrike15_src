//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//

//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hud_subtitles.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

DECLARE_HUDELEMENT( CHudSubtitles );

CHudSubtitles::CHudSubtitles( const char *pElementName ) :
	CHudElement( pElementName ),
	BaseClass( NULL, "HudSubtitles" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	SetScheme( "basemodui_scheme" );
	SetProportional( true );

	int nScreenWide, nScreenTall;
	vgui::surface()->GetScreenSize( nScreenWide, nScreenTall );

	SetPos( 0, 0 );
	SetSize( nScreenWide, nScreenTall );

	m_pSubtitlePanel = NULL;
	m_bIsPaused = false;
}

CHudSubtitles::~CHudSubtitles()
{
	delete m_pSubtitlePanel;
	m_pSubtitlePanel = NULL;
}

void CHudSubtitles::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

void CHudSubtitles::StartCaptions( const char *pFilename )
{
	bool bUseCaptioning = ShouldUseCaptioning();
	if ( !bUseCaptioning )
		return;

	StopCaptions();

	// start the subtitle sequence
	m_pSubtitlePanel = new CSubtitlePanel( this, pFilename, GetTall() );
	if ( !m_pSubtitlePanel->StartCaptions() )
	{
		StopCaptions();
	}
}

void CHudSubtitles::StopCaptions()
{
	delete m_pSubtitlePanel;
	m_pSubtitlePanel = NULL;
}

void CHudSubtitles::LevelShutdown()
{
	StopCaptions();
}

void CHudSubtitles::Reset()
{
	StopCaptions();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudSubtitles::ShouldDraw()
{
	bool bIsPaused = engine->IsPaused();
	if ( m_bIsPaused != bIsPaused )
	{
		m_bIsPaused = bIsPaused;
		if ( m_pSubtitlePanel )
		{
			m_pSubtitlePanel->Pause( m_bIsPaused );
		}
	}

	bool bNeedsDraw = m_pSubtitlePanel && m_pSubtitlePanel->HasCaptions();
	if ( !CHudElement::ShouldDraw() )
	{
		bNeedsDraw = false;
	}

	return bNeedsDraw;
}

CON_COMMAND( hud_subtitles, "Plays the Subtitles: <filename>" )
{
	if ( args.ArgC() < 2 )
		return;

	CHudSubtitles *pHudSubtitles = GET_FULLSCREEN_HUDELEMENT( CHudSubtitles );
	if ( pHudSubtitles )
	{
		pHudSubtitles->StartCaptions( args[1] );
	}
}
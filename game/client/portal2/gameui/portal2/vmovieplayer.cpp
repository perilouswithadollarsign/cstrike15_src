//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vmovieplayer.h"
#include "vfooterpanel.h"
#include "vgui/ISurface.h"
#include "filesystem.h"
#include "characterset.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

static int g_nAttractMovieSequence;
CUtlVector< CUtlString > g_AttractMovieList;

ConVar ui_show_attract_moviename( "ui_show_attract_moviename", "0", FCVAR_DEVELOPMENTONLY, "" );

CMoviePlayer::CMoviePlayer( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetPaintBackgroundEnabled( true );
	SetFooterEnabled( IsGameConsole() );

	AddFrameListener( this );

	m_flMovieStartTime = 0;
	m_flMovieExitTime = 0;
	m_bReadyToStartMovie = false;
	m_bMovieLetterbox = false;
	m_MoveToEditorMainMenuOnClose = false;

	m_nAttractMovie = 0;
	m_MovieHandle = BIKHANDLE_INVALID;

	// get attract vids, once
	if ( !g_AttractMovieList.Count() )
	{
		// cache the movie
		CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( !g_pFullFileSystem->ReadFile( "media/attractvids.txt", "GAME", buffer ) )
		{
			return;
		}

		characterset_t breakSet;
		CharacterSetBuild( &breakSet, "" );
		char moviePath[MAX_PATH];
		while ( 1 )
		{
			int nTokenSize = buffer.ParseToken( &breakSet, moviePath, sizeof( moviePath ) );
			if ( nTokenSize <= 0 )
			{
				break;
			}

			g_AttractMovieList.AddToTail( moviePath );
		}
	}
}

CMoviePlayer::~CMoviePlayer()
{
	// an ESC on the PC can get us here without proper shutdown
	// this is correct for all platforms
	if ( m_MovieHandle != BIKHANDLE_INVALID )
	{
		// stop the movie
		g_pBIK->DestroyMaterial( m_MovieHandle );
		m_MovieHandle = BIKHANDLE_INVALID;
	}

	RemoveFrameListener( this );
}

void CMoviePlayer::SetDataSettings( KeyValues *pSettings )
{
	m_OverrideVideoName = pSettings->GetString( "video" );
	m_bMovieLetterbox = pSettings->GetBool( "letterbox", false );
	m_MoveToEditorMainMenuOnClose = pSettings->GetBool( "editormenu_onclose", false );
}

void CMoviePlayer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	SetPos( 0, 0 );
	SetSize( screenWide, screenTall );
	
	if ( m_OverrideVideoName.IsEmpty() )
	{
		m_nAttractMovie = g_nAttractMovieSequence++;
		g_nAttractMovieSequence %= g_AttractMovieList.Count();
	}

	if ( !m_flMovieStartTime )
	{
		m_flMovieStartTime = Plat_FloatTime();
	}
}

bool CMoviePlayer::OnInputActivityStopMovie()
{
	if ( !m_flMovieExitTime &&
		m_MovieHandle != BIKHANDLE_INVALID &&  
		m_flMovieStartTime && 
		Plat_FloatTime() >= m_flMovieStartTime + 2.0f )
	{
		// only allow stopping the movie after it has started
		CloseMovieAndStartExit();
		return true;
	}

	return false;
}

void CMoviePlayer::OnKeyCodePressed( KeyCode code )
{
	if ( OnInputActivityStopMovie() )
	{
		// handled
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

void CMoviePlayer::OnMousePressed( vgui::MouseCode code )
{
	if ( OnInputActivityStopMovie() )
	{
		// handled
		return;
	}
	
	BaseClass::OnMousePressed( code );
}

bool CMoviePlayer::IsMoviePlayerOpaque()
{
	return m_bReadyToStartMovie;
}

bool CMoviePlayer::OpenMovie()
{
	if ( g_pBIK )
	{
		const char *pVideoName = NULL;
		if ( !m_OverrideVideoName.IsEmpty() )
		{
			pVideoName = m_OverrideVideoName.Get();
		}
		else if ( g_AttractMovieList.IsValidIndex( m_nAttractMovie ) )
		{
			pVideoName = g_AttractMovieList[m_nAttractMovie].Get();
		}

		if ( pVideoName && pVideoName[0] )
		{
			m_MovieHandle = g_pBIK->CreateMaterial( "VideoBIKMaterial_AttractMovie", pVideoName, "GAME", 0 );
			if ( m_MovieHandle != BIKHANDLE_INVALID )
			{
				BaseModUI::CBaseModPanel::GetSingleton().CalculateMovieParameters( m_MovieHandle, m_bMovieLetterbox );
			}
		}
	}

	return ( m_MovieHandle != BIKHANDLE_INVALID );
}

void CMoviePlayer::CloseMovieAndStartExit()
{
	if ( m_MovieHandle != BIKHANDLE_INVALID )
	{
		// stop the movie
		g_pBIK->DestroyMaterial( m_MovieHandle );
		m_MovieHandle = BIKHANDLE_INVALID;
	}

	// allow BaseModPanel to restart it's movie
	m_bReadyToStartMovie = false;

	// cannot immediately exit, the menu background needs time to restart
	m_flMovieExitTime = Plat_FloatTime() + 1.0f;

	if ( IsGameConsole() )
	{
		CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
		if ( pFooter )
		{
			vgui::ipanel()->MoveToBack( pFooter->GetVPanel() );
			pFooter->SetButtons( FB_NONE );
		}
	}

	// If we've been asked to send a command to the base panel, do that now
	if ( m_MoveToEditorMainMenuOnClose )
	{
		// Move us to the editor's main menu
		BASEMODPANEL_SINGLETON.MoveToEditorMainMenu();
		BASEMODPANEL_SINGLETON.OpenFrontScreen();
	}
}

void CMoviePlayer::RunFrame()
{
	if ( !m_flMovieExitTime )
	{
		if ( !IsVisible() )
		{
			// somewhow we got obscured
			// force the exit to happen right now with no fade out
			CloseMovieAndStartExit();
			m_flMovieExitTime = Plat_FloatTime();
		}
		else if ( CUIGameData::Get() && ( CUIGameData::Get()->IsXUIOpen() || CUIGameData::Get()->IsSteamOverlayActive() ) )
		{
			// start a graceful exit
			CloseMovieAndStartExit();
		}
	}
}

void CMoviePlayer::OnThink()
{
	BaseClass::OnThink();

	if ( m_flMovieExitTime || 
		!m_bReadyToStartMovie ||
		BaseModUI::CBaseModPanel::GetSingleton().IsMenuBackgroundMovieValid() )
	{
		// already exiting
		// not ready to start movie
		// background movie has not shutdown yet
		return;
	}

	// try to start the movie, once
	if ( m_MovieHandle == BIKHANDLE_INVALID )
	{
		// screen is obscured, start our streaming movie
		if ( !OpenMovie() )
		{
			// immediate failure
			CloseMovieAndStartExit();
		}
	}

	if ( IsGameConsole() && m_MovieHandle != BIKHANDLE_INVALID && m_flMovieStartTime && Plat_FloatTime() >= m_flMovieStartTime + 2.0f )
	{
		CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
		if ( pFooter && m_bReadyToStartMovie )
		{
			int x = vgui::scheme()->GetProportionalScaledValue( 65 );
			int y = vgui::scheme()->GetProportionalScaledValue( 46 );

			pFooter->SetButtons( FB_ABUTTON | FB_STEAM_NOPROFILE );
			pFooter->SetPosition( x, GetTall() - y );
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Done" );

			// pull the footer forward so it appears
			vgui::ipanel()->MoveToFront( pFooter->GetVPanel() );
		}
	}
}

void CMoviePlayer::PaintBackground()
{
	float flLerp = 0;
	if ( m_flMovieStartTime && !m_flMovieExitTime )
	{
		flLerp = RemapValClamped( Plat_FloatTime(), m_flMovieStartTime, m_flMovieStartTime + 0.5f, 0, 1.0f );
		if ( flLerp >= 1.0f )
		{
			// screen is obscured
			m_bReadyToStartMovie = true;
		}

		// fade out the background movie music until it stops
		CBaseModPanel::GetSingleton().UpdateBackgroundMusicVolume( 1.0 - flLerp );
	}

	if ( m_MovieHandle != BIKHANDLE_INVALID )
	{
		bool bRendered = BaseModUI::CBaseModPanel::GetSingleton().RenderMovie( m_MovieHandle );
		if ( !bRendered )
		{
			CloseMovieAndStartExit();
		}
		else
		{
			flLerp = 0;
		}
	}

	if ( m_flMovieExitTime )
	{
		flLerp = RemapValClamped( Plat_FloatTime(), m_flMovieExitTime, m_flMovieExitTime + 0.5f, 1.0f, 0 );
	}

	// draw an overlay
	surface()->DrawSetColor( Color( 0, 0, 0, flLerp * 255.0f ) );
	surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );

	if ( ui_show_attract_moviename.GetBool() )
	{
		int xPos = vgui::scheme()->GetProportionalScaledValue( 60 );
		int yPos = vgui::scheme()->GetProportionalScaledValue( 40 );

		const char *pMovieName = "";
		if ( !m_OverrideVideoName.IsEmpty() )
		{
			pMovieName = m_OverrideVideoName.Get();
		}
		else
		{
			pMovieName = g_AttractMovieList[m_nAttractMovie].Get();
		}

		BaseModUI::CBaseModPanel::GetSingleton().DrawColoredText( xPos, yPos, Color( 255, 255, 255, 255 ), pMovieName );
	}

	if ( m_flMovieExitTime && flLerp == 0 )
	{
		// fade up has completed to reveal prior menu, can now officialy exit
		NavigateBack();
	}
}

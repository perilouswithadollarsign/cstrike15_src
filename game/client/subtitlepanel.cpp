#include "cbase.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include "subtitlepanel.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define MAX_CHEAP_LINES	10

ConVar cheap_captions_test( "cheap_captions_test", "0", 0 );
ConVar cheap_captions_fadetime( "cheap_captions_fadetime", "0.5", 0 );

//-----------------------------------------------------------------------------
// Purpose: Determines if we should be playing with captions
//-----------------------------------------------------------------------------
bool ShouldUseCaptioning()
{
	if ( cheap_captions_test.GetBool() )
	{
		return true;
	}

	extern ConVar closecaption;
	return closecaption.GetBool();
}

CCaptionSequencer::CCaptionSequencer() : 
	m_bCaptions( false )
{
	Reset();
}

void CCaptionSequencer::Reset()
{
	// captioning start when rendering stable, not simply at movie start
	m_CaptionStartTime = 0;

	m_flPauseTime = 0;
	m_flTotalPauseTime = 0;
	m_bPaused = false;

	// initial priming state to fetch a caption
	m_bShowingCaption = false;
	m_bCaptionStale = true;

	m_CurCaptionString[0] = '\0';
	m_CurCaptionStartTime = 0.0f;
	m_CurCaptionEndTime = 0.0f;
	m_CurCaptionColor = 0xFFFFFFFF;

	if ( m_CaptionBuf.TellPut() )
	{
		// reset to start
		m_CaptionBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	}
	else
	{
		m_CaptionBuf.Purge();
	}
}

bool CCaptionSequencer::Init( const char *pFilename )
{
	Reset();

	m_bCaptions = false;	
	if ( g_pFullFileSystem->ReadFile( pFilename, "GAME", m_CaptionBuf ) )
	{
		m_bCaptions = true;	
	}

	return m_bCaptions;
}

void CCaptionSequencer::SetStartTime( float flStarTtime )
{
	// Start our captions now
	m_CaptionStartTime = flStarTtime;
}

void CCaptionSequencer::Pause( bool bPause )
{
	if ( m_bPaused == bPause )
		return;

	m_bPaused = bPause;

	if ( !m_bPaused )
	{
		// determine elapsed time paused
		m_flTotalPauseTime += Plat_FloatTime() - m_flPauseTime;
	}
	else
	{
		// stop the clock
		m_flPauseTime = Plat_FloatTime();
	}
}

float CCaptionSequencer::GetElapsedTime()
{
	float flElapsed;
	if ( !m_bPaused )
	{
		flElapsed = Plat_FloatTime() - m_flTotalPauseTime - m_CaptionStartTime;
	}
	else
	{
		// hold the clock
		flElapsed = m_flPauseTime - m_flTotalPauseTime - m_CaptionStartTime;
	}

	return flElapsed;
}

bool CCaptionSequencer::GetCaptionToken( char *token, int tokenLen )
{
	if ( !token || !tokenLen )
		return false;

	if ( !m_CaptionBuf.IsValid() )
	{
		// end of data
		return false;
	}

	m_CaptionBuf.GetLine( token, tokenLen );
	char *pCRLF = V_stristr( token, "\r" );
	if ( pCRLF )
	{
		*pCRLF = '\0';
	}
	m_CaptionBuf.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );

	return true;
}

bool CCaptionSequencer::GetNextCaption( void )
{
	char buff[MAX_CAPTION_LENGTH];

	if ( !GetCaptionToken( m_CurCaptionString, sizeof( m_CurCaptionString ) ) )
	{
		// end of captions
		m_CurCaptionString[0] = '\0';
		return false;
	}

	// hex color		
	GetCaptionToken( buff, sizeof( buff ) );
	sscanf( buff, "%x", &m_CurCaptionColor );

	// float start time
	GetCaptionToken( buff, sizeof( buff ) );
	m_CurCaptionStartTime = atof( buff );

	// float end time
	GetCaptionToken( buff, sizeof( buff ) );
	m_CurCaptionEndTime = atof( buff );

	// have valid caption
	m_bCaptionStale = false;
	return true;
}

const char *CCaptionSequencer::GetCurrentCaption( int *pColorOut )
{
	if ( !m_bCaptions )
	{
		return NULL;
	}

	if ( m_CaptionStartTime )
	{
		// get a timeline
		float elapsed = GetElapsedTime();

		// Get a new caption because we've just finished one
		if ( !m_bShowingCaption && m_bCaptionStale )
		{
			GetNextCaption();
		}

		if ( m_bShowingCaption )
		{
			if ( elapsed > m_CurCaptionEndTime )	// Caption just turned off
			{
				m_bShowingCaption = false;			// Don't draw caption
				m_bCaptionStale = true;				// Trigger getting a new one on the next frame
			}
		}
		else
		{
			if ( elapsed > m_CurCaptionStartTime )	// Turn Caption on
			{
				m_bShowingCaption = true;
			}
		}

		if ( m_bShowingCaption && m_CurCaptionString[0] )
		{
			if ( pColorOut )
			{
				*pColorOut = m_CurCaptionColor;
			}
			return m_CurCaptionString;
		}
	}

	return NULL;
}

float CCaptionSequencer::GetAlpha()
{
	if ( !m_bShowingCaption || m_bPaused )
		return 0;

	float flElapsed = GetElapsedTime();

	float flAlpha = RemapValClamped( flElapsed, m_CurCaptionStartTime, m_CurCaptionStartTime + cheap_captions_fadetime.GetFloat(), 0.0f, 255.0f );
	flAlpha = RemapValClamped( flElapsed, m_CurCaptionEndTime - cheap_captions_fadetime.GetFloat(), m_CurCaptionEndTime, flAlpha, 0.0f );

	return flAlpha;
}

CSubtitlePanel::CSubtitlePanel( vgui::Panel *pParent, const char *pCaptionFile, int nPlaybackHeight ) : 
	vgui::Panel( pParent, "SubtitlePanel" ) 
{
	SetScheme( "basemodui_scheme" );
	SetProportional( true );

	int nParentWide = pParent->GetWide();
	int nParentTall = pParent->GetTall();
	SetBounds( 0, 0, nParentWide, nParentTall );

	SetPaintBackgroundEnabled( true );

	m_hFont = vgui::INVALID_FONT;

	vgui::HScheme hScheme = vgui::scheme()->GetScheme( "basemodui_scheme" );
	vgui::IScheme *pNewScheme = vgui::scheme()->GetIScheme( hScheme );
	if ( pNewScheme )
	{	
		m_hFont = pNewScheme->GetFont( IsGameConsole() ? "CloseCaption_Console" : "CloseCaption_Normal", true );
	}

	m_nFontTall = vgui::surface()->GetFontTall( m_hFont );

	m_pSubtitleLabel = new vgui::Label( this, "SubtitleLabel", L"" );
	m_pSubtitleLabel->SetFont( m_hFont );

	int nWidth = nParentWide * 0.60f;
	int xPos = ( nParentWide - nWidth ) / 2;

	// assume video is centered
	// must be scaled according to playback height, due to letterboxing
	// don't want to cut into or overlap border, need to be within video, and title safe
	// so pushes up according to font height
	int yOffset = ( nPlaybackHeight - nParentTall )/2;
	int yPos = ( 0.85f * nPlaybackHeight ) - yOffset;

	// captions are anchored to a baseline and grow upward
	// any resolution changes then are title safe
	// tall enough for 10 lines
	m_pSubtitleLabel->SetPos( xPos, yPos - m_nFontTall * MAX_CHEAP_LINES );
	m_pSubtitleLabel->SetTall( m_nFontTall * MAX_CHEAP_LINES );
	m_pSubtitleLabel->SetWide( nWidth );
	m_pSubtitleLabel->SetContentAlignment( vgui::Label::a_south );
	m_pSubtitleLabel->SetWrap( true );

	const char *pFixedCaptionFile = pCaptionFile;

	char captionFilename[MAX_PATH];
	if ( !V_stristr( pCaptionFile, ".txt" ) )
	{
		// Strip any possible extension, add on the '_captions.txt' ending
		V_StripExtension( pCaptionFile, captionFilename, MAX_PATH );
		V_strncat( captionFilename, "_captions.txt", MAX_PATH );
		pFixedCaptionFile = captionFilename;
	}

	// Setup our captions
	m_bHasCaptions = m_Captions.Init( pFixedCaptionFile );

	// prevent any thinking or drawing when captions absent
	SetVisible( m_bHasCaptions );
}

bool CSubtitlePanel::StartCaptions()
{
	if ( !m_bHasCaptions )
		return false;

	m_Captions.SetStartTime( Plat_FloatTime() );
	return true;
}

void CSubtitlePanel::Pause( bool bPause )
{
	m_Captions.Pause( bPause );
}

bool CSubtitlePanel::HasCaptions()
{
	return m_bHasCaptions;
}

void CSubtitlePanel::OnThink()
{
	int nColor = 0xFFFFFFFF;
	const char *pCaptionText = m_Captions.GetCurrentCaption( &nColor );
	m_pSubtitleLabel->SetText( pCaptionText );
	
	SetAlpha( ShouldUseCaptioning() ? m_Captions.GetAlpha() : 0 );

	if ( pCaptionText )
	{
		int r = ( nColor >> 24 ) & 0xFF;
		int g = ( nColor >> 16 ) & 0xFF;
		int b = ( nColor >> 8 ) & 0xFF;
		int a = ( nColor >> 0 ) & 0xFF;
		m_pSubtitleLabel->SetFgColor( Color( r, g, b, a ) );
	}
}

void CSubtitlePanel::PaintBackground()
{
	int nMsgWide, nMsgTall;
	m_pSubtitleLabel->GetContentSize( nMsgWide, nMsgTall );
	if ( !nMsgWide || !nMsgTall )
	{
		return;
	}

	int nLabelX, nLabelY, nLabelWide, nLabelTall;
	m_pSubtitleLabel->GetBounds( nLabelX, nLabelY, nLabelWide, nLabelTall );

	// widen box to seat label better
	int nBoxWide = nLabelWide + GetWide() * 0.05f;
	int nBoxTall = nMsgTall + m_nFontTall;

	// center horizontally
	int nBoxX = ( GetWide() - nBoxWide )/2;

	// determine the top line of the south anchored text and center
	int nBoxY = ( nLabelY + nLabelTall ) - nMsgTall/2 - nBoxTall/2;

	DrawBox( nBoxX, nBoxY, nBoxWide, nBoxTall, Color( 0, 0, 0, 150 ), 1.0f );
}

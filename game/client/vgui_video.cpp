//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#include "cbase.h"
#include <vgui/IVGui.h>
#include "vgui/IInput.h"
#include <vgui/ISurface.h>
#include "ienginevgui.h"
#include "iclientmode.h"
#include "vgui_video.h"
#include "engine/IEngineSound.h"
#include "vgui/ILocalize.h"

#if defined(PORTAL2)
#include "portal2/basemodui.h"
#endif

#include "subtitlepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#ifdef PORTAL2
using namespace BaseModUI;
#endif

ConVar bink_preload_videopanel_movies( "bink_preload_videopanel_movies", "1", 0, "Preload Bink movies used by VideoPanel." );

static CUtlVector< VideoPanel * > g_vecVideoPanels;

enum VideoAllowInterrupt_t
{
	VIDEO_NO_INTERRUPT = 0,
	VIDEO_ALLOW_INTERRUPT = 1,
	VIDEO_ALLOW_INTERRUPT_DEV_ONLY,
};

// This is a hack due to the fact that the user can type quit with the video panel up, but it's parented to the GameUI dll root panel, which is already gone so
// we would crash in the destructor.
void VGui_ClearVideoPanels()
{
	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		if ( g_vecVideoPanels[ i ] )
		{
			delete g_vecVideoPanels[ i ];
		}
	}
	g_vecVideoPanels.RemoveAll();
}

void VGui_ClearTransitionVideoPanels()
{
	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		if ( g_vecVideoPanels[ i ] && g_vecVideoPanels[ i ]->IsTransitionVideo() )
		{
			g_vecVideoPanels[ i ]->StopPlayback();
		}
	}
}

void VGui_StopAllVideoPanels()
{
	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		if ( g_vecVideoPanels[ i ] )
		{
			// instant termination required
			// no exit commands are allowed to run
			// no deferring shutdown or fade out possible
			g_vecVideoPanels[ i ]->StopPlayback( true );
		}
	}
}

bool VGui_IsPlayingFullScreenVideo()
{
	if ( !enginevgui )
	{
		// not sure this interface is available when we get called
		return false;
	}

#ifdef PORTAL2
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEDLL );
#else
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEUIDLL );
#endif

	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		VideoPanel *pVideoPanel = g_vecVideoPanels[i];
		if ( !pVideoPanel )
			continue;

		if ( pVideoPanel->GetVParent() != pParent )
			continue;

		if ( pVideoPanel->IsVisible() && pVideoPanel->IsEnabled() && pVideoPanel->IsPlaying() )
		{
			int wide;
			int tall;
			pVideoPanel->GetSize( wide, tall );

			int screenWide;
			int screenTall;
			vgui::surface()->GetScreenSize( screenWide, screenTall );

			if ( wide == screenWide && tall == screenTall )
			{
				return true;
			}
		}
	}

	return false;
}

VideoPanel::VideoPanel( unsigned int nXPos, unsigned int nYPos, unsigned int nHeight, unsigned int nWidth ) : 
	BaseClass( NULL, "VideoPanel" ),
	m_BIKHandle( BIKHANDLE_INVALID ),
	m_nPlaybackWidth( 0 ),
	m_nPlaybackHeight( 0 ),
	m_nShutdownCount( 0 ),
	m_bLooping( false ),
	m_bStopAllSounds( true ),
	m_nAllowInterruption( VIDEO_NO_INTERRUPT ),
	m_bStarted( false ),
	m_bIsTransitionVideo( false ),
	m_bShouldPreload( false ),
	m_bEnablePartnerUI( false )
{
	m_flStartPlayTime = 0;
	m_flFadeInTime = 0;
	m_flFadeInEndTime = 0;
	m_flFadeOutTime = 0;
	m_flFadeOutEndTime = 0;

	m_flCurrentVolume = 0;

	m_bBlackBackground = true;

	m_pWaitingForPlayers = NULL;
	m_pPnlGamerPic = NULL;
	m_pLblGamerTag = NULL;
	m_pLblGamerTagStatus = NULL;

	m_pSubtitlePanel = NULL;

#ifdef PORTAL2
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEDLL );
#else
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEUIDLL );
#endif // PORTAL2
	SetParent( pParent );
	SetVisible( false );

	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );

	SetVisible( true );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetPostChildPaintEnabled( true );

	// Set us up
	SetTall( nHeight );
	SetWide( nWidth );
	SetPos( nXPos, nYPos );

	SetScheme( "basemodui_scheme" );
	SetProportional( true );

	// Let us update
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	g_vecVideoPanels.AddToTail( this );
}

//-----------------------------------------------------------------------------
// Properly shutdown out materials
//-----------------------------------------------------------------------------
VideoPanel::~VideoPanel( void )
{
	delete m_pSubtitlePanel;

	g_vecVideoPanels.FindAndRemove( this );

#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )
	// Shut down this video
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		bik->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
		m_pMaterial = NULL;
	}
#endif
}

void VideoPanel::LoadLayout()
{
	LoadControlSettings( "resource/UI/VideoPanel.res" );
	MakeReadyForUse();
}

void VideoPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadLayout();

	// partner overlay ui presentation
	m_pWaitingForPlayers = dynamic_cast< vgui::Label* >( FindChildByName( "WaitingForPlayers" ) );
	m_pPnlGamerPic = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
	m_pLblGamerTag = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTag" ) );
	m_pLblGamerTagStatus = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTagStatus" ) );

#ifdef PORTAL2
	SetupPartnerInScience( m_bEnablePartnerUI );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Keeps a tab on when the movie is ending and allows a frame to pass to prevent threading issues
//-----------------------------------------------------------------------------
void VideoPanel::OnTick( void ) 
{ 
	if ( m_nShutdownCount > 0 )
	{
		m_nShutdownCount++;
		if ( m_nShutdownCount > 10 )
		{
			OnClose();
			m_nShutdownCount = 0;
		}
	}
	else
	{
		ConVarRef volumeConVar( "volume" );
		float flMasterVolume = volumeConVar.IsValid() ? volumeConVar.GetFloat() : 0;
		if ( m_flCurrentVolume != flMasterVolume )
		{
			// for safety, only update on real change
			m_flCurrentVolume = flMasterVolume;
			bik->UpdateVolume( m_BIKHandle );
		}
	}

	bool bPartnerUIVisible = m_bEnablePartnerUI;
	if ( engine->IsPaused() || enginevgui->IsGameUIVisible() )
	{
		bPartnerUIVisible = false;
	}

#ifdef PORTAL2
	if ( m_pWaitingForPlayers && m_pWaitingForPlayers->IsVisible() != bPartnerUIVisible )
	{
		SetupPartnerInScience( bPartnerUIVisible );
	}
#endif

	BaseClass::OnTick(); 
}

void VideoPanel::OnVideoOver()
{
	StopPlayback();
}

//-----------------------------------------------------------------------------
// Purpose: Begins playback of a movie
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VideoPanel::BeginPlayback( const char *pFilename )
{
	if ( !pFilename || pFilename[ 0 ] == '\0' )
		return false;

#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )

	// Destroy any previously allocated video
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		bik->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
		m_pMaterial = NULL;
	}

	// Load and create our BINK video
	int nFlags = ( m_bLooping ? BIK_LOOP : 0 );

	// only preload the transition videos
	// in-game videos are precached with aan alternate system
	if ( ( bink_preload_videopanel_movies.GetBool() && m_bIsTransitionVideo ) || m_bShouldPreload )
	{
		nFlags |= BIK_PRELOAD;
	}

	char szMaterialName[ FILENAME_MAX ];
	Q_snprintf( szMaterialName, sizeof( szMaterialName ), "VideoBIKMaterial%i", g_pBIK->GetGlobalMaterialAllocationNumber() );

	m_BIKHandle = bik->CreateMaterial( szMaterialName, pFilename, "GAME", nFlags );
	if ( m_BIKHandle == BIKHANDLE_INVALID )
	{
		return false;
	}

	m_bStarted = true;

	m_flCurrentVolume = 0;

	// We want to be the sole audio source
	if ( m_bStopAllSounds )
	{
		enginesound->NotifyBeginMoviePlayback();
	}

	int nWidth, nHeight;
	bik->GetFrameSize( m_BIKHandle, &nWidth, &nHeight );
	bik->GetTexCoordRange( m_BIKHandle, &m_flU, &m_flV );
	m_pMaterial = bik->GetMaterial( m_BIKHandle );

	DevMsg( "Bink video \"%s\" size: %d %d\n", pFilename, nWidth, nHeight );

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();

	float flPhysicalFrameRatio = aspectRatioInfo.m_flFrameBuffertoPhysicalScalar * ( ( float )GetWide() / ( float )GetTall() );
	float flVideoRatio = ( ( float )nWidth / ( float )nHeight );

	// m_nPlaybackWidth and m_nPlaybackHeight are the dimensions of a panel that contains the entire bink movie in device pixels.
	// This code-path assumes that we want to letterbox.
	if ( flVideoRatio > flPhysicalFrameRatio )
	{
		m_nPlaybackWidth = GetWide();
		// Have to account for the difference between physical and pixel aspect ratios.
		m_nPlaybackHeight = ( ( float )GetWide() / aspectRatioInfo.m_flPhysicalToFrameBufferScalar ) / flVideoRatio;
	}
	else if ( flVideoRatio < flPhysicalFrameRatio )
	{
		// Have to account for the difference between physical and pixel aspect ratios.
		m_nPlaybackWidth = ( float )GetTall() * flVideoRatio * aspectRatioInfo.m_flPhysicalToFrameBufferScalar;
		m_nPlaybackHeight = GetTall();
	}
	else
	{
		m_nPlaybackWidth = GetWide();
		m_nPlaybackHeight = GetTall();
	}

	SetupCaptioning( pFilename, m_nPlaybackHeight );

	m_flStartPlayTime = gpGlobals->realtime;

	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::Activate( void )
{
	SetVisible( true );
	SetEnabled( true );

	vgui::surface()->SetMinimized( GetVPanel(), false );

	// Set the correct order
	vgui::VPANEL vpanelGameUI = enginevgui->GetPanel( PANEL_GAMEUIBACKGROUND );
	vgui::VPANEL vpanelThis = GetVPanel();
	vgui::ipanel()->SetZPos( vpanelThis, vgui::ipanel()->GetZPos( vpanelGameUI ) - 1 ); // place it just under gameui

	static ConVarRef cv_vguipanel_active( "vgui_panel_active" );
	cv_vguipanel_active.SetValue( cv_vguipanel_active.GetInt() + 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::DoModal( void )
{
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	static ConVarRef con_enable("con_enable");	
	bool bConsoleEnabled = (con_enable.IsValid() && con_enable.GetBool()) ? true : false;
	bool bAllowDevInterrupt = (bConsoleEnabled && m_nAllowInterruption == VIDEO_ALLOW_INTERRUPT_DEV_ONLY);

	bool bInterruptKeyPressed = ( code == KEY_ESCAPE );
	if ( (m_nAllowInterruption == VIDEO_ALLOW_INTERRUPT || bAllowDevInterrupt) && bInterruptKeyPressed )
	{
		StopPlayback();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle keys that should cause us to close
//-----------------------------------------------------------------------------
void VideoPanel::OnKeyCodePressed( vgui::KeyCode keycode )
{
	vgui::KeyCode code = GetBaseButtonCode( keycode );

	bool bDevInteruptKeyPressed = ( code == KEY_ESCAPE || code == KEY_BACKQUOTE );

	// All these keys will interrupt playback
	bool bInterruptKeyPressed =	  ( bDevInteruptKeyPressed || 
									code == KEY_SPACE || 
									code == KEY_ENTER ||
									code == KEY_XBUTTON_A || 
									code == KEY_XBUTTON_B ||
									code == KEY_XBUTTON_X || 
									code == KEY_XBUTTON_Y || 
									code == KEY_XBUTTON_START || 
									code == KEY_XBUTTON_BACK );
	
	static ConVarRef con_enable("con_enable");	
	bool bConsoleEnabled = (con_enable.IsValid() && con_enable.GetBool()) ? true : false;
	bool bAllowDevInterrupt = (bConsoleEnabled && m_nAllowInterruption == VIDEO_ALLOW_INTERRUPT_DEV_ONLY);

	// These keys cause the panel to shutdown
	if ( ( m_nAllowInterruption == VIDEO_ALLOW_INTERRUPT && bInterruptKeyPressed ) || ( bAllowDevInterrupt && bDevInteruptKeyPressed ) )
	{
		StopPlayback();
	}
	else
	{
		BaseClass::OnKeyCodePressed( keycode );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle mouse input that should cause us to close
//-----------------------------------------------------------------------------
void VideoPanel::OnMousePressed( vgui::MouseCode code )
{
	static ConVarRef con_enable("con_enable");	
	bool bConsoleEnabled = (con_enable.IsValid() && con_enable.GetBool()) ? true : false;
	bool bAllowDevInterrupt = (bConsoleEnabled && m_nAllowInterruption == VIDEO_ALLOW_INTERRUPT_DEV_ONLY);

	if ( (m_nAllowInterruption == VIDEO_ALLOW_INTERRUPT || bAllowDevInterrupt) )
	{
		StopPlayback();
	}
	else
	{
		BaseClass::OnMousePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::StopPlayback( bool bTerminate )
{
	SetVisible( false );

	if ( !bTerminate )
	{
		// Start the deferred shutdown process
		m_nShutdownCount = 1;
	}
	else
	{
		// caller wants instant termination
		// prevent any exit command
		m_nShutdownCount = 0;
		m_ExitCommand.Clear();
		OnClose();

		// Destroy any previously allocated video
		if ( m_BIKHandle != BIKHANDLE_INVALID )
		{
			bik->DestroyMaterial( m_BIKHandle );
			m_BIKHandle = BIKHANDLE_INVALID;
			m_pMaterial = NULL;
		}
	}
}

void VideoPanel::SetFadeInTime( float flTime ) 
{ 
	m_flFadeInTime = flTime;
	m_flFadeInEndTime = gpGlobals->realtime + flTime; 
	SetAlpha( 0 );
}

void VideoPanel::SetFadeOutTime( float flTime ) 
{ 
	m_flFadeOutTime = flTime;
	m_flFadeOutEndTime = gpGlobals->realtime + flTime; 
}


#if defined( PORTAL2 )
void VideoPanel::EnablePartnerUI( bool bEnablePartnerUI )
{
	m_bEnablePartnerUI = false;

	if ( bEnablePartnerUI )
	{
		if ( GameRules() && GameRules()->IsMultiplayer() && ( XBX_GetNumGameUsers() == 1 ) )
		{
			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			if ( pSession ) 
			{
				const char *pNetworkString = pSession->GetSessionSettings()->GetString( "system/network" );	
				if ( !V_stricmp( pNetworkString, "lan" ) || !V_stricmp( pNetworkString, "live" ) )
				{
					// only allowing the partner ui for lan/live coop games
					m_bEnablePartnerUI = true;
				}
			}	
		}	
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::OnClose()
{
	DevMsg( "VideoPanel::OnClose() played video for %.2f seconds\n", gpGlobals->realtime - m_flStartPlayTime );

	if ( m_bStopAllSounds )
	{
		enginesound->NotifyEndMoviePlayback();
	}

	BaseClass::OnClose();

	if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
	{
		vgui::input()->ReleaseAppModalSurface();
	}

	// Fire an exit command if we're asked to do so
	if ( !m_ExitCommand.IsEmpty() )
	{
		engine->ClientCmd( m_ExitCommand.Get() );
	}

	MarkForDeletion();

	static ConVarRef cv_vguipanel_active( "vgui_panel_active" );
	cv_vguipanel_active.SetValue( cv_vguipanel_active.GetInt() - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::GetPanelPos( int &xpos, int &ypos )
{
	xpos = ( (float) ( GetWide() - m_nPlaybackWidth ) / 2 );
	ypos = ( (float) ( GetTall() - m_nPlaybackHeight ) / 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Update and draw the frame
//-----------------------------------------------------------------------------
float VideoPanel::DrawMovieFrame( void )
{
	// No video to play, so do nothing
	if ( m_BIKHandle == BIKHANDLE_INVALID || m_pMaterial == NULL )
		return 0;

#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )
	// Update our frame, but only if Bink is ready for us to process another frame.
	// We aren't really swapping here, but ReadyForSwap is a good way to throttle.
	// We'd rather throttle this way so that we don't limit the overall frame rate of the system.
	// TODO: is this valid with threaded bink changes? 
	// if ( g_pBIK->ReadyForSwap( m_BIKHandle ) )
	{
		if ( g_pBIK->Update( m_BIKHandle ) == false )
		{
			// Issue a close command
			OnVideoOver();
			OnClose();
		}
	}
#else
	return 0;
#endif

	// Sit in the "center"
	int xpos, ypos;
	GetPanelPos( xpos, ypos );
	LocalToScreen( xpos, ypos );

	float alpha = ((float)GetFgColor()[3]/255.0f);
	float frac = 1.0f;

	if ( m_flFadeOutTime > 0 )
	{
		m_flFadeInTime = 0;
		frac = ( m_flFadeOutEndTime - gpGlobals->realtime ) / m_flFadeOutTime;
	}
	else if ( m_flFadeInTime > 0 )
	{
		m_flFadeOutTime = 0;
		frac = 1.0f - (( m_flFadeInEndTime - gpGlobals->realtime ) / m_flFadeInTime);
	}

	//alpha = frac * 255;
	alpha = clamp( frac, 0.0f, 1.0f );

	// if this is a level transition movie and something happened in between transition, we don't want users to be stuck here
	// so we just kill the movie after 60 seconds to be sure, this may be too short, we'll have to see
	
	bool bForceTimeout = !engine->IsConnected() && IsTransitionVideo();

	if ( bForceTimeout || (alpha <= 0 && m_flFadeOutTime > 0) )
	{
		m_flFadeOutTime = 0;
		// Issue a close command
		StopPlayback();
		return alpha;
	}
	else if ( m_flFadeInEndTime < gpGlobals->realtime )
	{
		m_flFadeInTime = 0;
	}

	// Black out the background (we could omit drawing under the video surface, but this is straight-forward)
	if ( m_bBlackBackground )
	{
		vgui::surface()->DrawSetColor(  0, 0, 0, (alpha*255) );
		vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
	}

	// Draw the polys to draw this out
	CMatRenderContextPtr pRenderContext( materials );
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( m_pMaterial, NULL );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float flLeftX = xpos;
	float flRightX = xpos + (m_nPlaybackWidth-1);

	float flTopY = ypos;
	float flBottomY = ypos + (m_nPlaybackHeight-1);

	// Map our UVs to cut out just the portion of the video we're interested in
	float flLeftU = 0.0f;
	float flTopV = 0.0f;

	// We need to subtract off a pixel to make sure we don't bleed
	float flRightU = m_flU - ( 1.0f / (float) m_nPlaybackWidth );
	float flBottomV = m_flV - ( 1.0f / (float) m_nPlaybackHeight );

	// Get the current viewport size
	int vx, vy, vw, vh;
	pRenderContext->GetViewport( vx, vy, vw, vh );

	// map from screen pixel coords to -1..1
	flRightX = FLerp( -1, 1, 0, vw, flRightX );
	flLeftX = FLerp( -1, 1, 0, vw, flLeftX );
	flTopY = FLerp( 1, -1, 0, vh ,flTopY );
	flBottomY = FLerp( 1, -1, 0, vh, flBottomY );

	for ( int corner=0; corner<4; corner++ )
	{
		bool bLeft = (corner==0) || (corner==3);
		meshBuilder.Position3f( (bLeft) ? flLeftX : flRightX, (corner & 2) ? flBottomY : flTopY, 0.0f );
		meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		meshBuilder.TexCoord2f( 0, (bLeft) ? flLeftU : flRightU, (corner & 2) ? flBottomV : flTopV );
		meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
		meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
		meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, alpha );
		meshBuilder.AdvanceVertex();
	}
	
	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	return alpha;
}

void VideoPanel::Paint()
{
	BaseClass::Paint();

#ifdef PORTAL2
	float flAlpha = DrawMovieFrame();

	if ( m_bEnablePartnerUI )
	{
		SetPartnerInScienceAlpha( flAlpha * 255.0f );
	}
#else
	DrawMovieFrame();
#endif
}

void VideoPanel::PostChildPaint()
{
	if ( engine->IsPaused() || (enginevgui->IsGameUIVisible()
#ifdef PORTAL2
		&& !CBaseModPanel::GetSingleton().IsLevelLoading()
#endif
		) )
	{
		vgui::surface()->DrawSetColor( 0, 0, 0, 150 );
		vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
	}
}

bool VideoPanel::IsPlaying()
{
	if ( m_BIKHandle == BIKHANDLE_INVALID || m_pMaterial == NULL )
		return false;

	return true;
}

#if defined( PORTAL2 )
void VideoPanel::SetupPartnerInScience( bool bEnable )
{
	if ( bEnable )
	{
		CBaseModPanel::GetSingleton().SetupPartnerInScience();
	}

	if ( m_pWaitingForPlayers )
	{
		m_pWaitingForPlayers->SetVisible( bEnable );
	}

	if ( m_pPnlGamerPic )
	{
		if ( bEnable )
		{
			vgui::IImage *pAvatarImage = CBaseModPanel::GetSingleton().GetPartnerImage();
			if ( pAvatarImage )
			{
				m_pPnlGamerPic->SetImage( pAvatarImage );
			}
			else
			{
				m_pPnlGamerPic->SetImage( "icon_lobby" );
			}		
		}

		m_pPnlGamerPic->SetVisible( bEnable );
	}

	if ( m_pLblGamerTag )
	{
		if ( bEnable )
		{
			CUtlString partnerName = CBaseModPanel::GetSingleton().GetPartnerName();
			m_pLblGamerTag->SetText( partnerName.Get() );
		}
		m_pLblGamerTag->SetVisible( bEnable );
	}

	if ( m_pLblGamerTagStatus )
	{
		m_pLblGamerTagStatus->SetVisible( bEnable );
		m_pLblGamerTagStatus->SetText( CBaseModPanel::GetSingleton().GetPartnerDescKey() );
	}
}

void VideoPanel::SetPartnerInScienceAlpha( int alpha )
{
	if ( m_pWaitingForPlayers )
	{
		m_pWaitingForPlayers->SetAlpha( alpha );
	}

	if ( m_pPnlGamerPic )
	{
		m_pPnlGamerPic->SetAlpha( alpha );
	}

	if ( m_pLblGamerTag )
	{
		m_pLblGamerTag->SetAlpha( alpha );
	}

	if ( m_pLblGamerTagStatus )
	{
		m_pLblGamerTagStatus->SetAlpha( alpha );
	}
}
#endif

void VideoPanel::SetupCaptioning( const char *pFilename, int nPlaybackHeight )
{
	if ( m_bIsTransitionVideo )
		return;

	bool bUseCaptioning = ShouldUseCaptioning();
	if ( !bUseCaptioning )
		return;

	delete m_pSubtitlePanel;
	m_pSubtitlePanel = new CSubtitlePanel( this, pFilename, nPlaybackHeight );

	// Start the caption sequence
	m_pSubtitlePanel->StartCaptions();
}

//-----------------------------------------------------------------------------
// Purpose: Create and playback a video in a panel
// Input  : nWidth - Width of panel (in pixels) 
//			nHeight - Height of panel
//			*pVideoFilename - Name of the file to play
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VideoPanel_Create( unsigned int nXPos, unsigned int nYPos, 
						unsigned int nWidth, unsigned int nHeight, 
						const char *pVideoFilename, 
						const char *pExitCommand /*= NULL*/,
						int nAllowInterruption /*= 0*/,
						float flFadeInTime /*= 1*/,
						bool bLoop /*= false*/,
						bool bIsTransitionVideo,
						bool bAddPartnerUI )
{
	// Create the base video panel
	VideoPanel *pVideoPanel = new VideoPanel( nXPos, nYPos, nHeight, nWidth );
	if ( pVideoPanel == NULL )
		return false;

	// Toggle if we want the panel to allow interruption
	pVideoPanel->SetAllowInterrupt( nAllowInterruption );

	// let the panel know that it's a level transition video
	pVideoPanel->SetIsTransitionVideo( bIsTransitionVideo );

	// Set the command we'll call (if any) when the video is interrupted or completes
	pVideoPanel->SetExitCommand( pExitCommand );

	pVideoPanel->SetLooping( bLoop );

#if defined( PORTAL2 )
	pVideoPanel->EnablePartnerUI( bAddPartnerUI );
#endif

	// Start it going
	if ( pVideoPanel->BeginPlayback( pVideoFilename ) == false )
	{
		delete pVideoPanel;
		return false;
	}

	pVideoPanel->SetFadeInTime( flFadeInTime );

	// Take control
	pVideoPanel->DoModal();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Take a raw filename and ensure it points to the correct directory and file extension
//-----------------------------------------------------------------------------
void ComposeBinkFilename( const char *lpszFilename, char *lpszOut, int nOutSize )
{
	Q_strncpy( lpszOut, "media/", nOutSize );	// Assume we must play out of the media directory
	char strFilename[MAX_PATH];
	Q_StripExtension( lpszFilename, strFilename, MAX_PATH );
	Q_strncat( lpszOut, strFilename, nOutSize );
	Q_strncat( lpszOut, ".bik", nOutSize );		// Assume we're a .bik extension type
}

//-----------------------------------------------------------------------------
// Purpose: Create a video panel with the supplied commands
//-----------------------------------------------------------------------------
void CreateVideoPanel( const char *lpszFilename, const char *lpszExitCommand, int nWidth, int nHeight, int nAllowInterruption, float flFadeTime = 0, bool bLoop = false, bool bIsTransitionVideo = false, bool bAddPartnerUI = false )
{
	char strFullpath[MAX_PATH];
	ComposeBinkFilename( lpszFilename, strFullpath, sizeof(strFullpath) );

	// Use the full screen size if they haven't specified an override
	unsigned int nScreenWidth = ( nWidth != 0 ) ? nWidth : ScreenWidth();
	unsigned int nScreenHeight = ( nHeight != 0 ) ? nHeight : ScreenHeight();

	// Create the panel and go!
	if ( VideoPanel_Create( 0, 0, nScreenWidth, nScreenHeight, strFullpath, lpszExitCommand, nAllowInterruption, flFadeTime, bLoop, bIsTransitionVideo, bAddPartnerUI ) == false )
	{
		Warning( "Unable to play video: %s\n", strFullpath );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback
//-----------------------------------------------------------------------------

CON_COMMAND( playvideo, "Plays a video: <filename> [width height]" )
{
	if ( args.ArgC() < 2 )
		return;

	unsigned int nScreenWidth = Q_atoi( args[2] );
	unsigned int nScreenHeight = Q_atoi( args[3] );
	
	CreateVideoPanel( args[1], NULL, nScreenWidth, nScreenHeight, VIDEO_ALLOW_INTERRUPT );
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback
//-----------------------------------------------------------------------------

CON_COMMAND( playvideo_nointerrupt, "Plays a video without ability to skip: <filename> [width height]" )
{
	if ( args.ArgC() < 2 )
		return;

	unsigned int nScreenWidth = Q_atoi( args[2] );
	unsigned int nScreenHeight = Q_atoi( args[3] );

	CreateVideoPanel( args[1], NULL, nScreenWidth, nScreenHeight, VIDEO_NO_INTERRUPT );
}

//-----------------------------------------------------------------------------
// Purpose: Plays a video fullscreen without ability to skip and fades in
//-----------------------------------------------------------------------------

CON_COMMAND( playvideo_end_level_transition, "Plays a video fullscreen without ability to skip (unless dev 1) and fades in: <filename> <time>" )
{
	if ( args.ArgC() < 2 )
		return;

	float flTime = Q_atoi( args[2] );
	if ( flTime <= 0 )
	{
		Warning( "Fade time needs to be greater than zero! Setting to 0.1f\n" );
		flTime = 0.1f;
	}

	FOR_EACH_VEC( g_vecVideoPanels, itr )
	{
		if ( g_vecVideoPanels[itr]->IsTransitionVideo() )
		{
			// We're already playing a transition video... don't start another
			return;	
		}
	}

	// this con command is only used for the coop transition videos
	bool bAddPartnerUI = true;

	CreateVideoPanel( args[1], NULL, 0, 0, VIDEO_ALLOW_INTERRUPT_DEV_ONLY, flTime, true, true, bAddPartnerUI );
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback and fire a command on completion
//-----------------------------------------------------------------------------

CON_COMMAND( playvideo_exitcommand, "Plays a video and fires and exit command when it is stopped or finishes: <filename> <exit command>" )
{
	if ( args.ArgC() < 2 )
		return;

	// Pull out the exit command we want to use
	char *pExitCommand = Q_strstr( args.GetCommandString(), args[2] );

	CreateVideoPanel( args[1], pExitCommand, 0, 0, VIDEO_ALLOW_INTERRUPT );
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback and fire a command on completion
//-----------------------------------------------------------------------------

CON_COMMAND( playvideo_exitcommand_nointerrupt, "Plays a video (without interruption) and fires and exit command when it is stopped or finishes: <filename> <exit command>" )
{
	if ( args.ArgC() < 2 )
		return;

	// Pull out the exit command we want to use
	char *pExitCommand = Q_strstr( args.GetCommandString(), args[2] );

	CreateVideoPanel( args[1], pExitCommand, 0, 0, VIDEO_NO_INTERRUPT );
}

//-----------------------------------------------------------------------------
// Purpose: Cause all playback to stop
//-----------------------------------------------------------------------------

CON_COMMAND( stopvideos, "Stops all videos playing to the screen" )
{
	FOR_EACH_VEC( g_vecVideoPanels, itr )
	{
		g_vecVideoPanels[itr]->StopPlayback();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cause all playback to fade out
//-----------------------------------------------------------------------------

CON_COMMAND( stopvideos_fadeout, "Fades out all videos playing to the screen: <time>" )
{
	if ( args.ArgC() < 1 )
		return;

	float flTime = Q_atoi( args[1] );
	if ( flTime <= 0 )
	{
		Warning( "Fade time needs to be greater than zero!  Setting to 0.1f\n" );
		flTime = 0.1f;
	}

	FOR_EACH_VEC( g_vecVideoPanels, itr )
	{
		g_vecVideoPanels[itr]->SetFadeOutTime( flTime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cause all transition videos to fade out
//-----------------------------------------------------------------------------

CON_COMMAND( stop_transition_videos_fadeout, "Fades out all transition videos playing to the screen: <time>" )
{
	if ( args.ArgC() < 1 )
		return;

	float flTime = Q_atoi( args[1] );
	if ( flTime <= 0 )
	{
		Warning( "Fade time needs to be greater than zero!  Setting to 0.1f\n" );
		flTime = 0.1f;
	}

	FOR_EACH_VEC( g_vecVideoPanels, itr )
	{
		if ( g_vecVideoPanels[itr]->IsTransitionVideo() )
			g_vecVideoPanels[itr]->SetFadeOutTime( flTime );
	}
}
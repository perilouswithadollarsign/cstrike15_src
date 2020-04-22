//========= Copyright � 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "c_vguiscreen.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IVGui.h>
#include "ienginevgui.h"
#include "fmtstr.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/ISurface.h>
#include "avi/ibik.h"
#include "engine/IEngineSound.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "c_movie_display.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using namespace vgui;

struct VideoPlaybackInfo_t
{
	VideoPlaybackInfo_t( void ) : 
		m_pMaterial ( NULL ), 
		m_nSourceHeight(0), m_nSourceWidth(0),
		m_flU(0.0f),m_flV(0.0f) {}

	IMaterial		*m_pMaterial;
	int				m_nSourceHeight, m_nSourceWidth;		// Source movie's dimensions
	float			m_flU, m_flV;							// U,V ranges for video on its sheet
};

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CMovieDisplayScreen : public CVGuiScreenPanel
{
	DECLARE_CLASS( CMovieDisplayScreen, CVGuiScreenPanel );

public:
	CMovieDisplayScreen( vgui::Panel *parent, const char *panelName );
	~CMovieDisplayScreen( void );

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick( void );
	virtual void Paint( void );

private:
	bool	IsActive( void );
	bool	IsAGroupPeer( CMovieDisplayScreen* pScreen );

	void	SetupMovie( void );
	void	UpdateMovie( void );
	bool	BeginPlayback( const char *pFilename );
	void	CalculatePlaybackDimensions( int nSrcWidth, int nSrcHeight );
	
	void	TakeOverAsMaster();

	inline void GetPanelPos( int &xpos, int &ypos )
	{
		xpos = ( (float) ( GetWide() - m_nPlaybackWidth ) / 2 );
		ypos = ( (float) ( GetTall() - m_nPlaybackHeight ) / 2 );
	}

private:

	// BINK playback info
	BIKMaterial_t			m_BIKHandle;
	VideoPlaybackInfo_t		m_playbackInfo;
	CHandle<C_VGuiScreen>	m_hVGUIScreen;
	CHandle<C_MovieDisplay>	m_hScreenEntity;
	
	int				m_nTextureId;
	int				m_nPlaybackHeight;		// Playback dimensions (proper ration adjustments)
	int				m_nPlaybackWidth;
	bool			m_bBlackBackground;
	bool			m_bSlaved;
	bool			m_bInitialized;

	bool			m_bLastActiveState;		// HACK: I'd rather get a real callback...

	// VGUI specifics
	ImagePanel		*m_pScanLineImage;

	bool bIsAlreadyVisible;
};

DECLARE_VGUI_SCREEN_FACTORY( CMovieDisplayScreen, "movie_display_screen" );

CUtlVector <CMovieDisplayScreen *>	g_MovieDisplays;

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CMovieDisplayScreen::CMovieDisplayScreen( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, "CMovieDisplayScreen" ) 
{
	m_pScanLineImage = new vgui::ImagePanel( this, "ScanLines");
	m_pScanLineImage->SetImage( "elevator_video_lines" );

	m_BIKHandle = BIKHANDLE_INVALID;
	m_nTextureId = -1;
	m_bBlackBackground = true;
	m_bSlaved = false;
	m_bInitialized = false;

	// Add ourselves to the global list of movie displays
	g_MovieDisplays.AddToTail( this );

	m_bLastActiveState = IsActive();
}

//-----------------------------------------------------------------------------
// Purpose: Clean up the movie
//-----------------------------------------------------------------------------
CMovieDisplayScreen::~CMovieDisplayScreen( void )
{
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		if ( bik )
			bik->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
	}

	// Clean up our texture reference
	if ( m_nTextureId != -1 )
	{
		g_pMatSystemSurface->DestroyTextureID( m_nTextureId );
		m_nTextureId = -1;
	}
	
	// Remove ourselves from the global list of movie displays
	g_MovieDisplays.FindAndRemove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Setup our scheme
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::ApplySchemeSettings( IScheme *pScheme )
{
	assert( pScheme );

	BaseClass::ApplySchemeSettings(pScheme);

	m_pScanLineImage->SetShouldScaleImage( true );
	int wide, tall;
	this->GetSize( wide, tall );
	m_pScanLineImage->SetSize( wide, tall );
	m_pScanLineImage->SetDrawColor( Color( 100, 100, 100, 255 ) );
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CMovieDisplayScreen::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if ( !BaseClass::Init( pKeyValues, pInitData ) )
		return false;

	// Save this for simplicity later on
	m_hVGUIScreen = dynamic_cast<C_VGuiScreen *>( GetEntity() );
	if ( m_hVGUIScreen != NULL )
	{
		// Also get the associated entity
		m_hScreenEntity = dynamic_cast<C_MovieDisplay *>(m_hVGUIScreen->GetOwnerEntity());
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to check our active state
//-----------------------------------------------------------------------------
bool CMovieDisplayScreen::IsActive( void )
{
	bool bScreenActive = false;
	if ( m_hVGUIScreen != NULL )
	{
		bScreenActive = m_hVGUIScreen->IsActive();
	}

	return bScreenActive;
}

//-----------------------------------------------------------------------------
// Purpose: Either become the master of a group of screens, or become a slave to another
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::SetupMovie( void )
{
	// Only bother if we haven't been setup yet
	if ( m_bInitialized || !IsActive() )
		return;

#if defined( _GAMECONSOLE )
	Assert( bik );
#endif

	if ( !bik )
		return;

	CMovieDisplayScreen *pMasterScreen = NULL;
	for ( int i = 0; i < g_MovieDisplays.Count(); i++ )
	{
		// Must be a valid peer and not be us
		if ( !IsAGroupPeer( g_MovieDisplays[i] ) || g_MovieDisplays[i] == this )
			continue;

		// See if we've found a master display
		if ( g_MovieDisplays[i]->m_bInitialized && g_MovieDisplays[i]->m_bSlaved == false )
		{
			m_bSlaved = true;

			// Share the info from the master
			m_playbackInfo = g_MovieDisplays[i]->m_playbackInfo;

			// We need to calculate our own playback dimensions as we may be a different size than our parent
			CalculatePlaybackDimensions( m_playbackInfo.m_nSourceWidth, m_playbackInfo.m_nSourceHeight );

			// Bind our texture
			m_nTextureId = surface()->CreateNewTextureID( true );
			g_pMatSystemSurface->DrawSetTextureMaterial( m_nTextureId, m_playbackInfo.m_pMaterial );

			// Hold this as the master screen
			pMasterScreen = g_MovieDisplays[i];
			break;
		}
	}

	// We need to try again, we have no screen entity!
	if ( m_hScreenEntity == NULL )
		return;

	// No master found, become one
	if ( pMasterScreen == NULL && !m_hScreenEntity->IsForcedSlave() )
	{
		const char *szFilename = m_hScreenEntity->GetMovieFilename();
		BeginPlayback( szFilename );
		m_bSlaved = false;
		m_bInitialized = true;			// we are the new master - we are done
	}
	else if ( pMasterScreen != NULL )	// we are a slave then we are done.
	{
		m_bInitialized = true;
	}
}

bool CMovieDisplayScreen::IsAGroupPeer( CMovieDisplayScreen* pScreen )
{
	// Must be valid 
	if ( pScreen == NULL )
		return false;

	// Must have an associated movie entity
	if ( pScreen->m_hScreenEntity == NULL )
		return false;

	// Must have a group name to care
	const char *szGroupName = m_hScreenEntity->GetGroupName();
	if ( szGroupName[0] == NULL )
		return false;

	// Group names must match!
	// FIXME: Use an ID instead?
	const char *szTestGroupName = pScreen->m_hScreenEntity->GetGroupName();
	if ( Q_strnicmp( szTestGroupName, szGroupName, 128 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Try to take over as the master for playback.
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::TakeOverAsMaster( void )
{
	// Tell all of the screens that are peers to us that they need to be set up again
	for ( int i = 0; i < g_MovieDisplays.Count(); i++ )
	{
		if ( IsAGroupPeer( g_MovieDisplays[i] ) )
		{
			if ( g_MovieDisplays[i]->m_nTextureId != -1 )
			{
				g_pMatSystemSurface->DestroyTextureID( g_MovieDisplays[i]->m_nTextureId );
				g_MovieDisplays[i]->m_nTextureId = -1;
			}

			// make it so we will reinitialize ourselves.
			g_MovieDisplays[i]->m_bInitialized = false;
		}
	}	

	// Even if we don't become the master we should stop trying
	m_hScreenEntity->SetMasterAttempted();

	SetupMovie();
}

//-----------------------------------------------------------------------------
// Purpose: Deal with the details of the video playback
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::UpdateMovie( void )
{
	// Only the master in a group updates the bink file
	if ( m_bSlaved )
		return;

	// Must have a movie to play
	if ( m_BIKHandle == BIKHANDLE_INVALID )
		return;

	// Get the current activity state of the screen
	bool bScreenActive = IsActive();

	// Pause if the game has paused
	bool bPaused = ( engine->IsPaused() || engine->Con_IsVisible() );
	if ( bPaused )
	{
		bScreenActive = false;
	}

	// See if we've changed our activity state
	if ( bik && bScreenActive != m_bLastActiveState )
	{
		if ( bScreenActive )
		{
			if ( /*m_bRestartOnResume*/ 1 && !bPaused )
			{
				bik->SetFrame( m_BIKHandle, 1.0f );
			}
			
			bik->Unpause( m_BIKHandle );
		}
		else
		{
			bik->Pause( m_BIKHandle );
		}
	}

	// Updated
	m_bLastActiveState = bScreenActive;

	// Update the frame if we're currently enabled
	if ( bScreenActive  )
	{
		// Update our frame
		if ( bik && bik->Update( m_BIKHandle ) == false )
		{
			// Issue a close command
			// OnVideoOver();
			// StopPlayback();
		}
	}
}

//-----------------------------------------------------------------------------
// Update the display string
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::OnTick()
{
	BaseClass::OnTick(); 

	if( m_hScreenEntity && m_hScreenEntity->GetWantsToBeMaster() )
	{
		TakeOverAsMaster();
	}

	// Create our playback or slave to another screen already playing
	SetupMovie();

	// Now update the movie
	UpdateMovie();
}

//-----------------------------------------------------------------------------
// Purpose: Adjust the playback dimensions to properly account for our screen dimensions
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::CalculatePlaybackDimensions( int nSrcWidth, int nSrcHeight )
{
	// Change our stretching type
	if ( m_hScreenEntity && m_hScreenEntity->IsStretchingToFill() )
	{
		m_nPlaybackWidth = GetWide();
		m_nPlaybackHeight = GetTall();
		return;
	}

	float flFrameRatio = ( (float) GetWide() / (float) GetTall() );
	float flVideoRatio = ( (float) nSrcWidth / (float) nSrcHeight );

	if ( flVideoRatio > flFrameRatio )
	{
		m_nPlaybackWidth = GetWide();
		m_nPlaybackHeight = ( GetWide() / flVideoRatio );
	}
	else if ( flVideoRatio < flFrameRatio )
	{
		m_nPlaybackWidth = ( GetTall() * flVideoRatio );
		m_nPlaybackHeight = GetTall();
	}
	else
	{
		m_nPlaybackWidth = GetWide();
		m_nPlaybackHeight = GetTall();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Begins playback of a movie
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMovieDisplayScreen::BeginPlayback( const char *pFilename )
{
#if defined( _GAMECONSOLE )
	Assert( bik );
#endif

	if ( !bik )
		return false;

	if ( m_BIKHandle == BIKHANDLE_INVALID )
	{
		// Create a globally unique name for this material
		char szMaterialName[256];
		
		// Append our group name if we have one
		const char *szGroupName = m_hScreenEntity->GetGroupName();
		if ( szGroupName[0] != NULL )
		{
			Q_snprintf( szMaterialName, sizeof(szMaterialName), "%s_%s", pFilename, szGroupName );
		}
		else
		{
			Q_strncpy( szMaterialName, pFilename, sizeof(szMaterialName) );
		}

		// Load and create our BINK video
		int nFlags = BIK_NO_AUDIO;	// FIXME: Allow?

		nFlags |= BIK_PRELOAD;
		if ( m_hScreenEntity->IsLooping() )
		{
			nFlags |= BIK_LOOP;
		}
		
		if ( bik )
			m_BIKHandle = bik->CreateMaterial( szMaterialName, pFilename, "GAME", nFlags );
		if ( m_BIKHandle == BIKHANDLE_INVALID )
			return false;
	}

	// NOTE: This class shouldn't turn off all sounds (Bank)
	//if ( ( nFlags & BIK_NO_AUDIO ) != 0 )
	//{
	//	// We want to be the sole audio source
	//	enginesound->NotifyBeginMoviePlayback();
	//}

	// Get our basic info from the movie
	bik->GetFrameSize( m_BIKHandle, &m_playbackInfo.m_nSourceWidth, &m_playbackInfo.m_nSourceHeight );
	bik->GetTexCoordRange( m_BIKHandle, &m_playbackInfo.m_flU, &m_playbackInfo.m_flV );
	m_playbackInfo.m_pMaterial = bik->GetMaterial( m_BIKHandle );

	// Get our playback dimensions
	CalculatePlaybackDimensions( m_playbackInfo.m_nSourceWidth, m_playbackInfo.m_nSourceHeight );
	
	// Bind our texture
	m_nTextureId = surface()->CreateNewTextureID( true );
	g_pMatSystemSurface->DrawSetTextureMaterial( m_nTextureId, m_playbackInfo.m_pMaterial );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update and draw the frame
//-----------------------------------------------------------------------------
void CMovieDisplayScreen::Paint( void )
{
	// Masters must keep the video updated
	if ( m_bSlaved == false && m_BIKHandle == BIKHANDLE_INVALID )
	{
		BaseClass::Paint();
		return;
	}

	// Sit in the "center"
	int xpos, ypos;
	GetPanelPos( xpos, ypos );

	bool bStretchToFill = ( m_hScreenEntity != NULL ) ? m_hScreenEntity->IsStretchingToFill() : false;
	bool bUsingCustomUVs = ( m_hScreenEntity != NULL ) ? m_hScreenEntity->IsUsingCustomUVs() : false;

	// Black out the background (we could omit drawing under the video surface, but this is straight-forward)
	if ( m_bBlackBackground && !bStretchToFill )
	{
		surface()->DrawSetColor(  0, 0, 0, 255 );
		surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
	}

	// Draw it
	surface()->DrawSetTexture( m_nTextureId );
	surface()->DrawSetColor(  255, 255, 255, 255 );

	if ( bUsingCustomUVs )
	{
		surface()->DrawTexturedSubRect( xpos, ypos, xpos+m_nPlaybackWidth, ypos+m_nPlaybackHeight, 
			m_hScreenEntity->GetUMin(), 
			m_hScreenEntity->GetVMin(), 
			m_hScreenEntity->GetUMax(), 
			m_hScreenEntity->GetVMax() );
	}
	else
	{
		surface()->DrawTexturedSubRect( xpos, ypos, xpos+m_nPlaybackWidth, ypos+m_nPlaybackHeight, 0.f, 0.f, m_playbackInfo.m_flU, m_playbackInfo.m_flV );
	}

	// Parent's turn
	BaseClass::Paint();
}

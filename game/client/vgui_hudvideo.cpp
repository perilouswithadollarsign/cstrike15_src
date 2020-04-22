//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#include "cbase.h"
#include "vgui_hudvideo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


#define BIK_MEDIA_FOLDER "media/"
#define BIK_EXTENTION ".bik"


HUDVideoPanel::HUDVideoPanel( vgui::Panel *parent, const char *name ) : 
	BaseClass( 0, 0, 32, 32 )
{
	SetParent( parent );
	SetName( name );

	m_bStopAllSounds = false;
	m_nAllowInterruption = 0;

	m_szLoopVideo[ 0 ] = '\0';
	m_szLastTempVideo[ 0 ] = '\0';

	m_nNumLoopAlternatives = 0;
	m_fAlternateChance = 1.0f;
	m_bIsLoopVideo = true;
	m_bIsTransition = false;
}

void HUDVideoPanel::Paint( void )
{
	BaseClass::Paint();

	if ( !m_bStarted )
	{
		BeginPlayback( VarArgs( BIK_MEDIA_FOLDER "%s" BIK_EXTENTION, m_szLoopVideo ) );
	}
}

void HUDVideoPanel::Activate( void )
{
	SetVisible( true );
	SetEnabled( true );
}

void HUDVideoPanel::DoModal( void )
{
	Activate();
}

void HUDVideoPanel::OnVideoOver()
{
	if ( m_bIsTransition )
	{
		m_bIsTransition = false;
		BeginPlayback( VarArgs( BIK_MEDIA_FOLDER "%s" BIK_EXTENTION, m_szLastTempVideo ) );
	}
	else if ( !m_bIsLoopVideo )
	{
		m_bIsLoopVideo = true;
		BeginPlayback( VarArgs( BIK_MEDIA_FOLDER "%s" BIK_EXTENTION, m_szLoopVideo ) );
	}
	else if ( m_nNumLoopAlternatives > 0 && RandomFloat() < m_fAlternateChance )
	{
		char szAltName[ FILENAME_MAX ];
		Q_snprintf( szAltName, sizeof( szAltName ), "%s_alt%02i", m_szLoopVideo, RandomInt( 0, m_nNumLoopAlternatives - 1 ) );
		PlayTempVideo( szAltName );
	}
	else
	{
		bik->SetFrame( m_BIKHandle, 0.0f );
	}
}

void HUDVideoPanel::ReturnToLoopVideo( void )
{
	if ( !m_bIsLoopVideo )
	{
		m_bIsLoopVideo = true;
		BeginPlayback( VarArgs( BIK_MEDIA_FOLDER "%s" BIK_EXTENTION, m_szLoopVideo ) );
	} 
}

void HUDVideoPanel::PlayTempVideo( const char *pFilename, const char *pTransitionFilename /*= NULL*/ )
{
	if ( !pFilename || Q_strcmp( GetCurrentVideo(), pFilename ) == 0 )
	{
		return;
	}

	Q_strncpy( m_szLastTempVideo, pFilename, sizeof( m_szLastTempVideo ) );

	m_bIsLoopVideo = false;

	m_bIsTransition = ( pTransitionFilename != NULL && pTransitionFilename[ 0 ] != '\0' );

	BeginPlayback( VarArgs( BIK_MEDIA_FOLDER "%s" BIK_EXTENTION, ( m_bIsTransition ? pTransitionFilename : m_szLastTempVideo ) ) );
}

void HUDVideoPanel::SetLoopVideo( const char *pFilename, int nNumLoopAlternatives /*= 0*/, float fAlternateChance /*= 1.0f*/ )
{
	m_nNumLoopAlternatives = nNumLoopAlternatives;
	m_fAlternateChance = fAlternateChance;

	if ( !pFilename || Q_strcmp( m_szLoopVideo, pFilename ) == 0 )
	{
		return;
	}

	Q_strncpy( m_szLoopVideo, pFilename, sizeof( m_szLoopVideo ) );

	if ( m_bIsLoopVideo && m_bStarted )
	{
		BeginPlayback( VarArgs( BIK_MEDIA_FOLDER "%s" BIK_EXTENTION, m_szLoopVideo ) );
	}
}

const char* HUDVideoPanel::GetCurrentVideo( void ) const
{
	return ( m_bIsLoopVideo ? GetLoopVideo() : GetLastTempVideo() );
}

//-----------------------------------------------------------------------------
// Purpose: Create and playback a video in a panel
//-----------------------------------------------------------------------------
HUDVideoPanel *HUDVideoPanel_Create( vgui::Panel *pParent,
							unsigned int iWide, unsigned int iTall, 
							const char *pVideoFilename, 
							const char *pExitCommand /*= NULL*/,
							float flFadeInTime /*= 1*/,
							bool bLoop /*= false*/, 
							bool bPreloadVideo /*= false*/ )
{
	// Create the base video panel
	HUDVideoPanel *pVideoPanel = new HUDVideoPanel( pParent, "HUDVideoPanel" );
	if ( pVideoPanel == NULL )
		return NULL;

	pVideoPanel->SetSize( iWide, iTall );
	pVideoPanel->SetExitCommand( pExitCommand );
	pVideoPanel->SetFadeInTime( flFadeInTime );
	pVideoPanel->SetLooping( bLoop );
	pVideoPanel->SetIsTransitionVideo( false );
	//pVideoPanel->SetShouldPreload ( bPreloadVideo );

	// Start it going
	if ( pVideoPanel->BeginPlayback( pVideoFilename ) == false )
	{
		delete pVideoPanel;
		return NULL;
	}


	return pVideoPanel;
}

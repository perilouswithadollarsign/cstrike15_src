//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#ifndef VGUI_VIDEO_H
#define VGUI_VIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Frame.h"
#include "avi/ibik.h"

class CSubtitlePanel;

class VideoPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( VideoPanel, vgui::Frame );
public:

	VideoPanel( unsigned int nXPos, unsigned int nYPos, unsigned int nHeight, unsigned int nWidth );

	~VideoPanel( void );

	virtual void Activate( void );
	virtual void Paint( void );
	virtual void DoModal( void );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnClose( void );
	virtual void GetPanelPos( int &xpos, int &ypos );

	void SetExitCommand( const char *pExitCommand )
	{
		if ( pExitCommand && pExitCommand[0] )
		{
			m_ExitCommand = pExitCommand;
		}
	}

	bool BeginPlayback( const char *pFilename );
	
	// terminate is a forceful instant shutdown, no exit commands fire
	void StopPlayback( bool bTerminate = false );

	void SetLooping( bool bLooping ) { m_bLooping = bLooping; }
	void SetFadeInTime( float flTime );
	void SetFadeOutTime( float flTime );

	void SetBlackBackground( bool bBlack ){ m_bBlackBackground = bBlack; }
	void SetAllowInterrupt( int nAllowInterrupt ) { m_nAllowInterruption = nAllowInterrupt; }
	void SetIsTransitionVideo( bool bIsTransitionVideo ) { m_bIsTransitionVideo = bIsTransitionVideo; }
	bool IsTransitionVideo( void ) { return m_bIsTransitionVideo; }
	void SetShouldPreload( bool bShouldPreload ) { m_bShouldPreload = bShouldPreload; }

#if defined( PORTAL2 )
	void EnablePartnerUI( bool bEnablePartnerUI );
#endif

	bool IsPlaying();

protected:

	virtual void OnTick( void );
	virtual void OnCommand( const char *pcCommand ) { BaseClass::OnCommand( pcCommand ); }
	virtual void OnVideoOver();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PostChildPaint();

protected:
	float			DrawMovieFrame();

	BIKMaterial_t	m_BIKHandle;
	IMaterial		*m_pMaterial;
	int				m_nPlaybackHeight;			// Calculated to address ratio changes
	int				m_nPlaybackWidth;
	CUtlString		m_ExitCommand;	// This call is fired at the engine when the video finishes or is interrupted

	float			m_flU;	// U,V ranges for video on its sheet
	float			m_flV;

	float			m_flStartPlayTime;
	float			m_flFadeInTime;
	float			m_flFadeInEndTime;
	float			m_flFadeOutTime;
	float			m_flFadeOutEndTime;

	int				m_nAllowInterruption;
	int				m_nShutdownCount;

	bool			m_bLooping;
	bool			m_bStopAllSounds;
	bool			m_bBlackBackground;
	bool			m_bIsTransitionVideo;
	bool			m_bShouldPreload;
	bool			m_bStarted;

private:
	void			LoadLayout();
#if defined( PORTAL2 )
	void			SetupPartnerInScience( bool bEnable );
	void			SetPartnerInScienceAlpha( int alpha );
#endif
	void			SetupCaptioning( const char *pFilename, int nPlaybackHeight );

	bool				m_bEnablePartnerUI;

	vgui::Label			*m_pWaitingForPlayers;
	vgui::ImagePanel	*m_pPnlGamerPic;
	vgui::Label			*m_pLblGamerTag;
	vgui::Label			*m_pLblGamerTagStatus;

	float				m_flCurrentVolume;

	CSubtitlePanel		*m_pSubtitlePanel;
};

// Creates a VGUI panel which plays a video and executes a client command at its finish (if specified)
extern bool VideoPanel_Create( unsigned int nXPos, unsigned int nYPos, 
							   unsigned int nWidth, unsigned int nHeight, 
							   const char *pVideoFilename, 
							   const char *pExitCommand = NULL );

extern void VGui_StopAllVideoPanels();

#endif // VGUI_VIDEO_H

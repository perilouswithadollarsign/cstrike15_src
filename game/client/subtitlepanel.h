//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
//===================================================================================//

#ifndef _SUBTITLEPANEL_H_
#define _SUBTITLEPANEL_H_
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

namespace vgui
{
	class IScheme;
};

#define MAX_CAPTION_LENGTH	256
class CCaptionSequencer
{
public:
	CCaptionSequencer();

	void		Reset();
	bool		Init( const char *pFilename );
	void		SetStartTime( float flStarTtime );
	bool		GetCaptionToken( char *token, int tokenLen );
	bool		GetNextCaption();
	const char	*GetCurrentCaption( int *pColorOut );
	void		Pause( bool bPause );
	float		GetAlpha();

private:
	float		GetElapsedTime();

	bool				m_bCaptions;
	bool				m_bShowingCaption;
	bool				m_bCaptionStale;
	bool				m_bPaused;

	float				m_CaptionStartTime;
	float				m_CurCaptionStartTime;
	float				m_CurCaptionEndTime;
	float				m_flPauseTime;
	float				m_flTotalPauseTime;

	vgui::HScheme		m_hCaptionFont;
	CUtlBuffer			m_CaptionBuf;

	char				m_CurCaptionString[MAX_CAPTION_LENGTH];
	unsigned int		m_CurCaptionColor;
};

// Panel for drawing subtitles
class CSubtitlePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CSubtitlePanel, vgui::Panel );

public:
	CSubtitlePanel( vgui::Panel *pParent, const char *pMovieName, int nPlaybackHeight );
	
	bool	StartCaptions();
	bool	HasCaptions();
	void	Pause( bool bPause );

protected:
	virtual void OnThink();
	virtual void PaintBackground();

private:
	CCaptionSequencer	m_Captions;

	vgui::HFont			m_hFont;
	vgui::Label			*m_pSubtitleLabel;

	int					m_nFontTall;

	bool				m_bHasCaptions;
};

extern bool ShouldUseCaptioning();

#endif // _SUBTITLEPANEL_H_

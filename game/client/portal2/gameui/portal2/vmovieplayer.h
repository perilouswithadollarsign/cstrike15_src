//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VMOVIEPLAYER_H__
#define __VMOVIEPLAYER_H__

#include "basemodui.h"

namespace BaseModUI {

class CMoviePlayer : public CBaseModFrame, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( CMoviePlayer, CBaseModFrame );

public:
	CMoviePlayer( vgui::Panel *pParent, const char *pPanelName );
	~CMoviePlayer();

	bool IsMoviePlayerOpaque();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PaintBackground();
	virtual void OnThink();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void RunFrame();
	virtual void SetDataSettings( KeyValues *pSettings );
	virtual void OnMousePressed( vgui::MouseCode code );

private:
	bool	OpenMovie();
	void	CloseMovieAndStartExit();
	bool	OnInputActivityStopMovie();

	bool			m_bReadyToStartMovie;
	bool			m_bMovieLetterbox;
	float			m_flMovieStartTime;
	float			m_flMovieExitTime;
	BIKMaterial_t	m_MovieHandle;
	int				m_nAttractMovie;
	CUtlString		m_OverrideVideoName;
	bool			m_MoveToEditorMainMenuOnClose;
};

};

#endif

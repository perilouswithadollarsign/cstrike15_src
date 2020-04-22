//===== Copyright  Valve Corporation, All rights reserved. ======//
//
//  Portal leaderboard graph panel
//
//================================================================//
#ifndef __VPORTALCHALLENGESTATSPANEL_H__
#define __VPORTALCHALLENGESTATSPANEL_H__
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>


class CPortalChallengeStatsPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CPortalChallengeStatsPanel, vgui::EditablePanel );
public:
	CPortalChallengeStatsPanel( vgui::Panel *parent, const char *name );
	~CPortalChallengeStatsPanel();

	void SetTitleText( const char *pTitle );
	void SetPortalScore( int nPortals );
	void SetTimeScore( float flTotalSeconds );
	void SetTimeScore( int nTotalSeconds );

protected:
	// vgui overrides for rounded corner background
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void PaintBackground();

private:

	vgui::Label *m_pTitleLabel;
	vgui::Label *m_pPortalsLabel;
	vgui::Label *m_pTimeLabel;
	vgui::Label	*m_pPortalScore;
	vgui::Label	*m_pTimeScore;
};

#endif // __VPORTALCHALLENGESTATSPANEL_H__

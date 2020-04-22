//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VLOADINGPROGRESS_H__
#define __VLOADINGPROGRESS_H__

#include "basemodui.h"
#include "vgui/IScheme.h"
#include "const.h"
#include "loadingtippanel.h"

namespace BaseModUI {

class LoadingProgress : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( LoadingProgress, CBaseModFrame );

public:
	LoadingProgress( vgui::Panel *parent, const char *panelName );
	~LoadingProgress();

	virtual void		Close();

	void				SetProgress( float progress );
	float				GetProgress();

	bool				ShouldShowPosterForLevel( KeyValues *pMissionInfo, KeyValues *pChapterInfo );
	void				SetPosterData( KeyValues *pChapterInfo, const char *pszGameMode );

	bool				IsDrawingProgressBar( void ) { return m_bDrawProgress; }

	bool				LoadingProgressWantsIsolatedRender( bool bContextValid );

protected:
	virtual void		OnThink();
	virtual void		ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void		PaintBackground();
	virtual void		PostChildPaint();

private:
	void				SetupControlStates( void );
	void				UpdateWorkingAnim();
	void				StopTransitionEffect();
	void				DrawLoadingBar();
	void				EvictImages();
	void				SetupPartnerInScience();
	void				SetupCommunityMapLoad();
	void				ShowEmployeeBadge( bool bState );

	vgui::ImagePanel	*m_pWorkingAnim;

	bool				m_bValid;

	int					m_textureID_LoadingDots;

	bool				m_bDrawBackground;
	bool				m_bDrawProgress;
	bool				m_bDrawSpinner;

	float				m_flPeakProgress;
	float				m_flLastEngineTime;

	KeyValues			*m_pChapterInfo;

	struct BackgroundImage_t
	{
		int m_nTextureID;
		bool m_bOwnedByPanel;
	};
	CUtlVector< BackgroundImage_t >	m_BackgroundImages;

	bool				m_bUseAutoTransition;

	int					m_nCurrentImage;
	int					m_nNextImage;
	int					m_nTargetImage;

	float				m_flTransitionStartTime;
	float				m_flLastTransitionTime;
	float				m_flAutoTransitionTime;
	float				m_flFadeInStartTime;

	int					m_nProgressX;
	int					m_nProgressY;
	int					m_nProgressNumDots;
	int					m_nProgressDotGap;
	int					m_nProgressDotWidth;
	int					m_nProgressDotHeight;
};

};

#endif // __VLOADINGPROGRESS_H__
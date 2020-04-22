//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VVIDEO_H__
#define __VVIDEO_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"
#include "OptionsSubVideo.h"

// Matched to number of entries in .RES file
#define MAX_DYNAMIC_VIDEO_MODES 43

namespace BaseModUI {

class DropDownMenu;
class SliderControl;

struct ResolutionMode_t
{
	int m_nWidth;
	int m_nHeight;
};

class Video : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( Video, CBaseModFrame );

public:
	Video(vgui::Panel *parent, const char *panelName);
	~Video();

	void SetDefaults();
	void DiscardChangesAndClose();
	void AcceptPowerSavingsWarningCallback( void );

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void	OnKeyCodePressed(vgui::KeyCode code);
	virtual void	OnCommand( const char *command );
	virtual void	OnThink();

private:
	void	GetSettings( bool bRecommendedSettings );
	bool	GetRecommendedSettings( void );
	void	SetupState( bool bRecommendedSettings );
	void	UpdateFooter();
	void	PrepareResolutionList();
	void	ApplyChanges();
	void	GetResolutionName( vmode_t *pMode, char *pOutBuffer, int nOutBufferSize, bool &bIsNative );
	void	ShowPowerSavingsWarning();
	void	SetPowerSavingsState();

private:
	int					m_nNumResolutionModes;
	ResolutionMode_t	m_nResolutionModes[ MAX_DYNAMIC_VIDEO_MODES ];

	KeyValues::AutoDelete m_autodelete_pResourceLoadConditions;

	SliderControl			*m_sldBrightness;
	BaseModHybridButton		*m_drpAspectRatio;
	BaseModHybridButton		*m_drpResolution;
	BaseModHybridButton		*m_drpDisplayMode;
	BaseModHybridButton		*m_drpPowerSavingsMode;
	BaseModHybridButton		*m_drpSplitScreenDirection;
	BaseModHybridButton		*m_btnAdvanced;
	
	bool	m_bDirtyValues;
	bool	m_bEnableApply;
	bool	m_bPreferRecommendedResolution;
	int		m_iAspectRatio;
	int		m_iResolutionWidth;
	int		m_iResolutionHeight;
	bool	m_bWindowed;
	bool	m_bNoBorder;
	int		m_nPowerSavingsMode;

	int		m_iCurrentResolutionWidth;
	int		m_iCurrentResolutionHeight;
	bool	m_bCurrentWindowed;

	int		m_iRecommendedAspectRatio;
	int		m_iRecommendedResolutionWidth;
	int		m_iRecommendedResolutionHeight;
	bool	m_bRecommendedWindowed;
	bool	m_bRecommendedNoBorder;
	int		m_nRecommendedPowerSavingsMode;

	float	m_flOriginalGamma;

	bool	m_bAcceptPowerSavingsWarning;
};

};

#endif // __VVIDEO_H__

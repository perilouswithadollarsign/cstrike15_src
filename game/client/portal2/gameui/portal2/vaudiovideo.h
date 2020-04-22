//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VAUDIOVIDEO_H__
#define __VAUDIOVIDEO_H__

#include "basemodui.h"

namespace BaseModUI 
{

class SliderControl;
class BaseModHybridButton;

class AudioVideo : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( AudioVideo, CBaseModFrame );

public:
	AudioVideo(vgui::Panel *parent, const char *panelName);
	~AudioVideo();

	Panel* NavigateBack();

protected:
	virtual void Activate();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnCommand( const char *pCommand );

private:
	void UpdateFooter();

	SliderControl			*m_sldBrightness;
	BaseModHybridButton		*m_drpColorMode;
	BaseModHybridButton		*m_drpSplitScreenDirection;
	SliderControl			*m_sldGameVolume;
	SliderControl			*m_sldMusicVolume;
	BaseModHybridButton		*m_drpLanguage;
	BaseModHybridButton		*m_drpCaptioning;

	bool m_bOldForceEnglishAudio;
	bool m_bDirtyVideoConfig;
};

};

#endif

//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VAUDIO_H__
#define __VAUDIO_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"
#include "OptionsSubAudio.h"

#define MAX_DYNAMIC_AUDIO_LANGUAGES 15

typedef struct IVoiceTweak_s IVoiceTweak;

namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

struct AudioLangauge_t
{
	ELanguage languageCode;
};

class Audio : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( Audio, CBaseModFrame );

public:
	Audio(vgui::Panel *parent, const char *panelName);
	~Audio();

	void ResetLanguage();

	static const char* GetUpdatedAudioLanguage() { return m_pchUpdatedAudioLanguage; }

protected:
	virtual void Activate();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );
	virtual Panel *NavigateBack();

private:
	void	UpdateFooter( bool bEnableCloud );
	void	UpdateEnhanceStereo( void );
	void	PrepareLanguageList();
	void	UseSelectedLanguage();
	void	DiscoverAudioLanguages();
	void	UpdatePttBinding();

	static void AcceptLanguageChangeCallback();
	static void CancelLanguageChangeCallback();

private:
	IVoiceTweak			*m_pVoiceTweak;

	KeyValues::AutoDelete m_autodelete_pResourceLoadConditions;

	SliderControl		*m_sldGameVolume;
	SliderControl		*m_sldMusicVolume;
	BaseModHybridButton	*m_drpSpeakerConfiguration;
	BaseModHybridButton	*m_drpSoundQuality;
	BaseModHybridButton	*m_drpLanguage;
	BaseModHybridButton	*m_drpCaptioning;
	BaseModHybridButton	*m_drpVoiceCommunication;
	BaseModHybridButton *m_drpPuzzlemakerSounds;
	
	ELanguage			m_nSelectedAudioLanguage;
	ELanguage			m_nCurrentAudioLanguage;
	int					m_nNumAudioLanguages;
	AudioLangauge_t		m_nAudioLanguages[ MAX_DYNAMIC_AUDIO_LANGUAGES ];

	static const char	*m_pchUpdatedAudioLanguage;
};

};

#endif

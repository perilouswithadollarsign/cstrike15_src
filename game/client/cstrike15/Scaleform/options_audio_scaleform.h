//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __OPTIONS_AUDIO_SCALEFORM_H__ )
#define __OPTIONS_AUDIO_SCALEFORM_H__
#ifdef _WIN32
#pragma once
#endif

#include "messagebox_scaleform.h"
#include "GameEventListener.h"
#include "options_scaleform.h"

class COptionsAudioScaleform : public COptionsScaleform
{
public:
	COptionsAudioScaleform( );
	virtual ~COptionsAudioScaleform( );

	// CGameEventListener callback
	virtual void FireGameEvent( IGameEvent *event ) { }

protected:
	// For Dialog specific updates to choice widgets
	virtual bool HandleUpdateChoice( OptionChoice_t * pOptionChoice, int nCurrentChoice );
	 
	// Sets the option to whatever the current ConVar value
	// bForceDefaultValue signals that unique algorithms should be used to select the value
	virtual void SetChoiceWithConVar( OptionChoice_t * pOption, bool bForceDefaultValue = false );

	// Determine whether enhanced stereo should be turned on (for headphones at high sound quality)
	void UpdateEnhanceStereo( void );

	// Returns sys_sound_quality  dsp_slow_cpu and Snd_PitchQuality 
	int  FindSoundQuality( void );

	// Sets dsp_slow_cpu and Snd_PitchQuality
	void SetSoundQuality( int nIndex );

	void SetVoiceConfig( int nIndex );

	int FindVoiceConfig( void );

	virtual void PerformPostLayout( void );

	bool InitUniqueWidget(const char * szWidgetID, OptionChoice_t * pOptionChoice);
};

#endif // __OPTIONS_AUDIO_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM

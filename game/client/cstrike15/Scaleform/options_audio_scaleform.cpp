//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "options_audio_scaleform.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"
#include "inputsystem/iinputsystem.h"
#include "IGameUIFuncs.h"
#include "c_playerresource.h"
#include <vstdlib/vstrtools.h>
#include "matchmaking/imatchframework.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "iachievementmgr.h"
#include "gameui_interface.h"
#include "gameui_util.h"
#include "vgui_int.h"
#include "materialsystem/materialsystem_config.h"
#include "vgui/ISurface.h"
#include "soundsystem/isoundsystem.h"
#ifdef _WIN32
#include "dsound.h"
#endif

#ifndef _GAMECONSOLE
#include "steam/steam_api.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar sys_sound_quality		( "sys_sound_quality", "-1", FCVAR_CHEAT, "Convar used exclusively by the options screen to set sound quality. Changing this convar manually will have no effect." );
ConVar sys_voice				( "sys_voice", "-1", FCVAR_NONE, "Convar used exclusively by the options screen to set voice options. Changing this convar manually will have no effect." );
extern ConVar windows_speaker_config;


COptionsAudioScaleform::COptionsAudioScaleform()
{
}

COptionsAudioScaleform::~COptionsAudioScaleform()
{
}

bool COptionsAudioScaleform::InitUniqueWidget(const char * szWidgetID, OptionChoice_t * pOptionChoice)
{
	if (g_pSoundSystem && !V_strcmp(szWidgetID, "Audio Device"))
	{
		CUtlVector<audio_device_description_t> devices;
		g_pSoundSystem->GetAudioDevices(devices);

		for (int i = 0; i != devices.Count(); ++i)
		{
			audio_device_description_t& desc = devices[i];

			int nChoice = pOptionChoice->m_Choices.AddToTail();
			OptionChoiceData_t * pNewOptionChoice = &(pOptionChoice->m_Choices[nChoice]);

			char buf[sizeof(pNewOptionChoice->m_wszLabel) / sizeof(*pNewOptionChoice->m_wszLabel)];
			V_strncpy(buf, desc.m_friendlyName, sizeof(buf));

			V_strtowcs(buf, -1, pNewOptionChoice->m_wszLabel, sizeof(pNewOptionChoice->m_wszLabel));
			V_wcstostr(desc.m_deviceName, -1, pNewOptionChoice->m_szValue, sizeof(pNewOptionChoice->m_szValue));
		}

		return true;
	}

	return false;
}

bool COptionsAudioScaleform::HandleUpdateChoice( OptionChoice_t * pOptionChoice, int nCurrentChoice )
{
	if ( pOptionChoice && 
		 nCurrentChoice >= 0 &&
		 nCurrentChoice < pOptionChoice->m_Choices.Count() )
	{
		pOptionChoice->m_nChoiceIndex = nCurrentChoice;
		int iConVarSlot = pOptionChoice->m_bSystemValue ? 0 : m_iSplitScreenSlot;

		SplitScreenConVarRef varOption( pOptionChoice->m_szConVar );
		varOption.SetValue( iConVarSlot, pOptionChoice->m_Choices[nCurrentChoice].m_szValue );

		if ( !V_strcmp( pOptionChoice->m_szConVar, "snd_surround_speakers" ) )
		{
			static ConVarRef snd_use_hrtf("snd_use_hrtf");
			snd_use_hrtf.SetValue(nCurrentChoice == 0 ? 1 : 0);

			UpdateEnhanceStereo();
		}
		else if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_sound_quality" ) )
		{
			SetSoundQuality( V_atoi( pOptionChoice->m_Choices[nCurrentChoice].m_szValue ) );
		}
		else if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_voice" ) )
		{
			SetVoiceConfig( V_atoi( pOptionChoice->m_Choices[nCurrentChoice].m_szValue ) );
		}						

		return true;
	}

	return false;
}


void COptionsAudioScaleform::PerformPostLayout()
{
	FindVoiceConfig();
}


void COptionsAudioScaleform::SetChoiceWithConVar(OptionChoice_t * pOption, bool bForceDefaultValue)
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);
	SplitScreenConVarRef varOption(pOption->m_szConVar);
	int iConVarSlot = pOption->m_bSystemValue ? 0 : m_iSplitScreenSlot;

	int nResult = -1;

	if (bForceDefaultValue)
	{
		varOption.SetValue(iConVarSlot, varOption.GetDefault());
	}

	if (!V_strcmp(pOption->m_szConVar, "sound_device_override"))
	{
		nResult = 2;
	}
	else if (!V_strcmp(pOption->m_szConVar, "snd_surround_speakers"))
	{
		static ConVarRef snd_use_hrtf("snd_use_hrtf");
		if (snd_use_hrtf.GetInt() > 0)
		{
			nResult = 0;
		}
		else
		{

#ifdef _WIN32
			static ConVarRef windows_speaker_config("windows_speaker_config");

			switch (windows_speaker_config.GetInt())
			{
			case DSSPEAKER_HEADPHONE:
				nResult = 1;
				break;

			case DSSPEAKER_STEREO:
			default:
				nResult = 2;
				break;

			case DSSPEAKER_QUAD:
				nResult = 3;
				break;

			case DSSPEAKER_5POINT1:
				nResult = 4;
				break;
			}

#else

			// default to headphones
			nResult = 1;
#endif
		}
	}
	else if (  !V_strcmp( pOption->m_szConVar, "sys_sound_quality" ) )
	{
		nResult = FindSoundQuality();
	}
	else if (  !V_strcmp( pOption->m_szConVar, "sys_voice" ) )
	{
		nResult = FindVoiceConfig();
	}
	else
	{
		nResult = FindChoiceFromString( pOption, varOption.GetString( iConVarSlot ) );
	}
	
	if ( nResult == -1 )
	{
		// Unexpected ConVar value, try matching with the default
		Warning( "ConVar did not match any of the options found in data file: %s\n", pOption->m_szConVar );

		nResult = FindChoiceFromString( pOption, varOption.GetDefault() );

		if ( nResult == -1 )
		{
			// Completely unexpected ConVar value. Display whatever choice is at the zero index so that
			// the client does not draw undefined characters
			Assert( false );
			Warning( "ConVar default not match any of the options found in data file: %s\n", pOption->m_szConVar );

			nResult = 0;						
		}
	}

	pOption->m_nChoiceIndex = nResult;
}



int COptionsAudioScaleform::FindVoiceConfig( void )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	int nResult = 0;

	SplitScreenConVarRef voice_modenable( "voice_modenable" );
	SplitScreenConVarRef voice_enable( "voice_enable" );
	// TODO: we're cutting open mic for now
	SplitScreenConVarRef voice_vox( "voice_vox" );

	bool bVoiceEnabled = voice_enable.GetBool( m_iSplitScreenSlot ) && voice_modenable.GetBool( m_iSplitScreenSlot );

	if ( !bVoiceEnabled )
	{
		//disabled
		nResult = 0;
	}
	else
	{
		// push to talk
		nResult = 1;
	}

	// Update navigation
	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RefreshAudioNav", 0, NULL );
	}

	return nResult;
}

void COptionsAudioScaleform::SetVoiceConfig( int nIndex )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	SplitScreenConVarRef voice_modenable( "voice_modenable" );
	SplitScreenConVarRef voice_enable( "voice_enable" );
	//TODO: Open mic doesn't work. We're chosing to disable it instead of fix. 
	SplitScreenConVarRef voice_vox( "voice_vox" );

	switch ( nIndex )
	{
	case 0: // disabled
		voice_modenable.SetValue( m_iSplitScreenSlot, 0 );
		voice_enable.SetValue( m_iSplitScreenSlot, 0 );
		voice_vox.SetValue( m_iSplitScreenSlot, 0 );
		break;

	case 1: // push to talk
		voice_modenable.SetValue( m_iSplitScreenSlot, 1 );
		voice_enable.SetValue( m_iSplitScreenSlot, 1 );
		voice_vox.SetValue( m_iSplitScreenSlot, 0 );
		break;

	default:
		AssertMsg( false, "SetVoiceConfig index out of range." );
		break;
	}

	// Update navigation
	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RefreshAudioNav", 0, NULL );
	}
}



int  COptionsAudioScaleform::FindSoundQuality( void )
{
	SplitScreenConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	SplitScreenConVarRef dsp_slow_cpu( "dsp_slow_cpu" );

	int nResult = 0;

	if ( !dsp_slow_cpu.GetBool( 0 ) )
	{
		nResult = 1;
	}
	if ( Snd_PitchQuality.GetBool( 0 ) )
	{
		nResult = 2;

	}
	return nResult;
}


void COptionsAudioScaleform::SetSoundQuality( int nIndex )
{
	SplitScreenConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	SplitScreenConVarRef dsp_slow_cpu( "dsp_slow_cpu" );

	switch( nIndex )
	{
	case 0: // low quality
		dsp_slow_cpu.SetValue( 0, true );
		Snd_PitchQuality.SetValue( 0, false );
		break;

	case 1: // medium quality
		dsp_slow_cpu.SetValue( 0, false );
		Snd_PitchQuality.SetValue( 0, false );
		break;

	case 2: // high quality
		dsp_slow_cpu.SetValue( 0, false );
		Snd_PitchQuality.SetValue( 0, true );
		break;

	default:
		AssertMsg( false, "SetSoundQuality index out of range" );
		break;
	}

	UpdateEnhanceStereo();
}


void COptionsAudioScaleform::UpdateEnhanceStereo()
{
	// headphones at high quality get enhanced stereo turned on
	SplitScreenConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	SplitScreenConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
	SplitScreenConVarRef snd_surround_speakers("Snd_Surround_Speakers");
	SplitScreenConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );

	if ( !dsp_slow_cpu.GetBool( 0 ) && Snd_PitchQuality.GetBool( 0 ) && snd_surround_speakers.GetInt( 0 ) == 0 )
	{
#ifdef CSTRIKE15
		dsp_enhance_stereo.SetValue( 0, 0 );
#else
		dsp_enhance_stereo.SetValue( 0, 1 );
#endif
	}
	else
	{
		dsp_enhance_stereo.SetValue( 0, 0 );
	}
}





#endif // INCLUDE_SCALEFORM

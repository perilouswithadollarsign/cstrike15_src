//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "VAudioVideo.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "VHybridButton.h"
#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

AudioVideo::AudioVideo(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	m_sldBrightness = NULL;
	m_drpColorMode = NULL;
	m_drpSplitScreenDirection = NULL;
	m_sldGameVolume = NULL;
	m_sldMusicVolume = NULL;
	m_drpLanguage = NULL;
	m_drpCaptioning = NULL;

	// Store the old vocal language setting
	CGameUIConVarRef force_audio_english( "force_audio_english" );
	m_bOldForceEnglishAudio = force_audio_english.GetBool();
	m_bDirtyVideoConfig = false;

	int nNumTilesTall = 4;
	if ( IsX360() && XBX_IsAudioLocalized() )
	{
		nNumTilesTall = 5;
	}

	SetDialogTitle( "#L4D360UI_AudioVideo", NULL, false, 7, nNumTilesTall, 0 );

	SetFooterEnabled( true );
}

AudioVideo::~AudioVideo()
{
	CGameUIConVarRef force_audio_english( "force_audio_english" );
	if ( m_bOldForceEnglishAudio != force_audio_english.GetBool() )
	{
		engine->AudioLanguageChanged();
	}
}

void AudioVideo::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void AudioVideo::OnKeyCodePressed(KeyCode code)
{
	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		{
			if ( m_bDirtyVideoConfig || 
				( m_sldBrightness && m_sldBrightness->IsDirty() ) ||
				( m_sldGameVolume && m_sldGameVolume->IsDirty() ) || 
				( m_sldMusicVolume && m_sldMusicVolume->IsDirty() ) )
			{
				// Ready to write that data... go ahead and nav back
				BaseClass::OnKeyCodePressed(code);
			}
			else
			{
				// Don't need to write data... go ahead and nav back
				BaseClass::OnKeyCodePressed(code);
			}
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

void AudioVideo::OnCommand(const char *command)
{
	if ( !Q_stricmp( command, "#L4D360UI_ColorMode_Television" ) )
	{
		CGameUIConVarRef mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled" );
		mat_monitorgamma_tv_enabled.SetValue( 1 );
		m_bDirtyVideoConfig = true;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_ColorMode_LCD" ) )
	{
		CGameUIConVarRef mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled" );
		mat_monitorgamma_tv_enabled.SetValue( 0 );
		m_bDirtyVideoConfig = true;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_SplitScreenDirection_Default" ) )
	{
		if ( m_drpSplitScreenDirection && m_drpSplitScreenDirection->IsEnabled() )
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			ss_splitmode.SetValue( 0 );
			m_bDirtyVideoConfig = true;
		}
	}
	else if ( !Q_stricmp( command, "#L4D360UI_SplitScreenDirection_Horizontal" ) )
	{
		if ( m_drpSplitScreenDirection && m_drpSplitScreenDirection->IsEnabled() )
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			ss_splitmode.SetValue( 1 );
			m_bDirtyVideoConfig = true;
		}
	}
	else if ( !Q_stricmp( command, "#L4D360UI_SplitScreenDirection_Vertical" ) )
	{
		if ( m_drpSplitScreenDirection && m_drpSplitScreenDirection->IsEnabled() )
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			ss_splitmode.SetValue( 2 );
			m_bDirtyVideoConfig = true;
		}
	}
	else if ( Q_stricmp( "#L4D360UI_AudioOptions_CaptionOff", command ) == 0 )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");
		closecaption.SetValue( 0 );
		cc_subtitles.SetValue( 0 );
		m_bDirtyVideoConfig = true;
	}
	else if ( Q_stricmp( "#L4D360UI_AudioOptions_CaptionSubtitles", command ) == 0 )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");
		closecaption.SetValue( 1 );
		cc_subtitles.SetValue( 1 );
		m_bDirtyVideoConfig = true;
	}
	else if ( Q_stricmp( "#L4D360UI_AudioOptions_CaptionOn", command ) == 0 )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");
		closecaption.SetValue( 1 );
		cc_subtitles.SetValue( 0 );
		m_bDirtyVideoConfig = true;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Gore_High" ) )
	{
		CGameUIConVarRef z_wound_client_disabled( "z_wound_client_disabled" );
		z_wound_client_disabled.SetValue( 0 );
		m_bDirtyVideoConfig = true;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Gore_Low" ) )
	{
		CGameUIConVarRef z_wound_client_disabled( "z_wound_client_disabled" );
		z_wound_client_disabled.SetValue( 1 );
		m_bDirtyVideoConfig = true;
	}
	else if ( !Q_stricmp( command, "CurrentXBXLanguage" ) )
	{
		CGameUIConVarRef force_audio_english( "force_audio_english" );
		const char *pchLanguage = XBX_GetLanguageString();
		bool bIsEnglish = !V_stricmp( pchLanguage, "English" ) ;
		force_audio_english.SetValue( bIsEnglish );
		m_bDirtyVideoConfig = true;
	}
	else if ( !Q_stricmp( command, "CurrentXBXLanguage_English" ) )
	{
		CGameUIConVarRef force_audio_english( "force_audio_english" );
		force_audio_english.SetValue( 1 );
		m_bDirtyVideoConfig = true;
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void AudioVideo::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		int visibleButtons = FB_BBUTTON;
		if ( IsGameConsole() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		footer->SetButtons( visibleButtons );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );
	}
}

Panel* AudioVideo::NavigateBack()
{
	// For cert we can only write one config in this spot!! (and have to wait 3 seconds before writing another)
	if ( m_bDirtyVideoConfig || 
		 ( m_sldBrightness && m_sldBrightness->IsDirty() ) ||
		 ( m_sldGameVolume && m_sldGameVolume->IsDirty() ) || 
		 ( m_sldMusicVolume && m_sldMusicVolume->IsDirty() ) )
	{
		// Trigger TitleData3 update ( XBX_GetPrimaryUserId() )
		engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_video_ss %d", XBX_GetPrimaryUserId() ) );
	}

	return BaseClass::NavigateBack();
}

void AudioVideo::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_sldBrightness = dynamic_cast< SliderControl* >( FindChildByName( "SldBrightness" ) );
	m_drpColorMode = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpColorMode" ) );
	m_drpSplitScreenDirection = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpSplitScreenDirection" ) );
	m_sldGameVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldGameVolume" ) );
	m_sldMusicVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldMusicVolume" ) );
	m_drpCaptioning = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpCaptioning" ) );
	m_drpLanguage = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpLanguage" ) );

	if ( m_sldBrightness )
	{
		m_sldBrightness->Reset();

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_sldBrightness->NavigateTo();
	}

	if ( m_drpColorMode )
	{
		CGameUIConVarRef mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled" );

		if ( mat_monitorgamma_tv_enabled.GetBool() )
		{
			m_drpColorMode->SetCurrentSelection( "#L4D360UI_ColorMode_Television" );
		}
		else
		{
			m_drpColorMode->SetCurrentSelection( "#L4D360UI_ColorMode_LCD" );
		}
	}

	if ( m_drpSplitScreenDirection )
	{
		const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
		bool bWidescreen = aspectRatioInfo.m_bIsWidescreen;

		if ( !bWidescreen )
		{
			m_drpSplitScreenDirection->SetEnabled( false );
			m_drpSplitScreenDirection->SetCurrentSelection( "#L4D360UI_SplitScreenDirection_Horizontal" );
		}
		else
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			int iSplitMode = ss_splitmode.GetInt();

			switch ( iSplitMode )
			{
			case 1:
				m_drpSplitScreenDirection->SetCurrentSelection( "#L4D360UI_SplitScreenDirection_Horizontal" );
				break;
			case 2:
				m_drpSplitScreenDirection->SetCurrentSelection( "#L4D360UI_SplitScreenDirection_Vertical" );
				break;
			default:
				m_drpSplitScreenDirection->SetCurrentSelection( "#L4D360UI_SplitScreenDirection_Default" );
			}
		}
	}

	if ( m_sldGameVolume )
	{
		m_sldGameVolume->Reset();
	}

	if ( m_sldMusicVolume )
	{
		m_sldMusicVolume->Reset();
	}

	if ( m_drpCaptioning )
	{
		CGameUIConVarRef closecaption( "closecaption" );
		CGameUIConVarRef cc_subtitles( "cc_subtitles" );

		if ( !closecaption.GetBool() )
		{
			m_drpCaptioning->SetCurrentSelection( "#L4D360UI_AudioOptions_CaptionOff" );
		}
		else
		{
			if ( cc_subtitles.GetBool() )
			{
				m_drpCaptioning->SetCurrentSelection( "#L4D360UI_AudioOptions_CaptionSubtitles" );
			}
			else
			{
				m_drpCaptioning->SetCurrentSelection( "#L4D360UI_AudioOptions_CaptionOn" );
			}
		}
	}

	if ( m_drpLanguage )
	{
		bool bIsLocalized = XBX_IsAudioLocalized();
		if ( !bIsLocalized )
		{
			// hidden if we don't have an audio localization for our current non-english language
			// the audio is in english, there is no other choice for their audio
			m_drpLanguage->SetVisible( false );
		}
		else
		{
			// We're in a language other than english that has full audio localization (not all of them do)
			// let them select either their native language or english for thier audio if they're not in game
			char szLanguage[256];
			Q_snprintf( szLanguage, sizeof( szLanguage ), "#GameUI_Language_%s", XBX_GetLanguageString() );

			// find by command, change ui string
			m_drpLanguage->ModifySelectionString( "CurrentXBXLanguage", szLanguage );

			CGameUIConVarRef force_audio_english( "force_audio_english" );
			m_drpLanguage->SetCurrentSelection( ( force_audio_english.GetBool() ) ? ( "#GameUI_Language_English" ) : ( szLanguage ) );

			// Can't change language while in game
			m_drpLanguage->SetEnabled( !engine->IsInGame() || GameUI().IsInBackgroundLevel() );
		}
	}
	
	m_bDirtyVideoConfig = false;

	UpdateFooter();
}

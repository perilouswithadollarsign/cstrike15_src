//========= Copyright � 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VAudio.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "ivoicetweak.h"
#include "materialsystem/materialsystem_config.h"

#include "vgui_controls/ImagePanel.h"

#include "igameuifuncs.h"
#include "inputsystem/iinputsystem.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VIDEO_LANGUAGE_COMMAND_PREFIX "_language"

using namespace vgui;
using namespace BaseModUI;

// This member is static so that the updated audio language can be referenced during shutdown
const char* Audio::m_pchUpdatedAudioLanguage = "";

Audio::Audio( Panel *parent, const char *panelName ):
BaseClass( parent, panelName ),
m_autodelete_pResourceLoadConditions( (KeyValues*) NULL )
{
#if !defined( NO_VOICE )
	m_pVoiceTweak = engine->GetVoiceTweakAPI();
#else
	m_pVoiceTweak = NULL;
#endif

	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#GameUI_Audio" );

	m_pResourceLoadConditions = new KeyValues( "audio" );
	m_autodelete_pResourceLoadConditions.Assign( m_pResourceLoadConditions );

	m_sldGameVolume = NULL;
	m_sldMusicVolume = NULL;
	m_drpSpeakerConfiguration = NULL;
	m_drpSoundQuality = NULL;
	m_drpLanguage = NULL;
	m_drpCaptioning = NULL;
	m_drpVoiceCommunication = NULL;
	m_drpPuzzlemakerSounds = NULL;

	m_nSelectedAudioLanguage = k_Lang_None;
	m_nCurrentAudioLanguage = k_Lang_None;
	m_nNumAudioLanguages = 0;

	DiscoverAudioLanguages();

	SetFooterEnabled( true );
	UpdateFooter( true );
}

Audio::~Audio()
{
	if ( m_pchUpdatedAudioLanguage[ 0 ] != '\0' )
	{
		PostMessage( &(CBaseModPanel::GetSingleton()), new KeyValues( "command", "command", "RestartWithNewLanguage" ), 0.01f );
	}
}

void Audio::Activate()
{
	BaseClass::Activate();
	UpdatePttBinding();
	UpdateFooter( true );
}

void Audio::UpdatePttBinding()
{
	if ( BaseModHybridButton *btnPTT = dynamic_cast< BaseModHybridButton * >( FindChildByName( "DrpVoicePTT" ) ) )
	{
		char const *keyName = "#L4D360UI_MsgBx_MustBindButtonsTitle";

		for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
		{
			ButtonCode_t bc = ( ButtonCode_t )i;

			bool bIsJoystickCode = IsJoystickCode( bc );
			// Skip Joystick buttons entirely
			if ( bIsJoystickCode )
				continue;

			// Look up binding
			const char *binding = gameuifuncs->GetBindingForButtonCode( bc );
			if ( !binding )
				continue;

			// See if there is an item for this one?
			if ( V_stricmp( "+voicerecord", binding ) )
				continue;

			// Bind it by name
			keyName = g_pInputSystem->ButtonCodeToString( bc );
		}

		char chCaps[2] = {0};
		if ( keyName && keyName[0] && !keyName[1] )
		{
			chCaps[0] = keyName[0];
			keyName = V_strupr( chCaps );
		}
		btnPTT->SetCurrentSelection( keyName );
	}
}

void Audio::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		if ( m_nSelectedAudioLanguage != m_nCurrentAudioLanguage && m_drpLanguage && m_drpLanguage->IsEnabled() )
		{
			UseSelectedLanguage();

			// Pop up a dialog to confirm changing the language
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#GameUI_ChangeLanguageRestart_Title";
			data.pMessageText = "#GameUI_ChangeLanguageRestart_Info";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptLanguageChangeCallback;
			data.bCancelButtonEnabled = true;
			data.pfnCancelCallback = &CancelLanguageChangeCallback;

			// WARNING! WARNING! WARNING!
			// The nature of Generic Confirmation is that it will be silently replaced
			// with another Generic Confirmation if a system event occurs
			// e.g. user unplugs controller, user changes storage device, etc.
			// If that happens neither OK nor CANCEL callbacks WILL NOT BE CALLED
			// The state machine cannot depend on either callback advancing the
			// state because in some situations neither callback can fire and the
			// confirmation dismissed/closed/replaced.
			// State machine must implement OnThink and check if the required
			// confirmation box is still present!
			// This code implements no fallback - it will result in minor UI
			// bug that the language box will be changed, but the title not restarted.
			// Vitaliy -- 9/26/2009
			//
			confirmation->SetUsageData( data );
		}
		else
		{
			// Ready to write that data... go ahead and nav back
			BaseClass::OnKeyCodePressed(code);
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

void Audio::OnCommand(const char *command)
{
	if ( !V_stricmp( "#GameUI_Headphones", command ) )
	{
		CGameUIConVarRef snd_surround_speakers( "Snd_Surround_Speakers" );
		snd_surround_speakers.SetValue( "0" );

		// headphones at high quality get enhanced stereo turned on
		UpdateEnhanceStereo();
	}
	else if ( !V_stricmp( "#GameUI_2Speakers", command ) )
	{
		CGameUIConVarRef snd_surround_speakers( "Snd_Surround_Speakers" );
		snd_surround_speakers.SetValue( "2" );

		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if ( !V_stricmp( "#GameUI_4Speakers", command ) )
	{
		CGameUIConVarRef snd_surround_speakers( "Snd_Surround_Speakers" );
		snd_surround_speakers.SetValue( "4" );

		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if ( !V_stricmp( "#GameUI_5Speakers", command ) )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
		snd_surround_speakers.SetValue( "5" );

		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if ( !V_stricmp( "#GameUI_High", command ) )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
		dsp_slow_cpu.SetValue( false );
		Snd_PitchQuality.SetValue( true );

		UpdateEnhanceStereo();
	}
	else if ( !V_stricmp( "#GameUI_Medium", command ) )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
		dsp_slow_cpu.SetValue( false );
		Snd_PitchQuality.SetValue( false );

		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if ( !V_stricmp( "#GameUI_Low", command ) )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
		dsp_slow_cpu.SetValue( true );
		Snd_PitchQuality.SetValue( false );

		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if ( !V_stricmp( "#L4D360UI_AudioOptions_CaptionOff", command ) )
	{
		CGameUIConVarRef closecaption( "closecaption" );
		CGameUIConVarRef cc_subtitles( "cc_subtitles" );
		closecaption.SetValue( 0 );
		cc_subtitles.SetValue( 0 );
	}
	else if ( !V_stricmp( "#L4D360UI_AudioOptions_CaptionSubtitles", command ) )
	{
		CGameUIConVarRef closecaption( "closecaption" );
		CGameUIConVarRef cc_subtitles( "cc_subtitles" );
		closecaption.SetValue( 1 );
		cc_subtitles.SetValue( 1 );
	}
	else if ( !V_stricmp( "#L4D360UI_AudioOptions_CaptionOn", command ) )
	{
		CGameUIConVarRef closecaption( "closecaption" );
		CGameUIConVarRef cc_subtitles( "cc_subtitles" );
		closecaption.SetValue( 1 );
		cc_subtitles.SetValue( 0 );
	}
	else if ( StringHasPrefix( command, VIDEO_LANGUAGE_COMMAND_PREFIX ) )
	{
		int iCommandNumberPosition = Q_strlen( VIDEO_LANGUAGE_COMMAND_PREFIX );
		int iSelectedLanguage = clamp( command[ iCommandNumberPosition ] - '0', 0, m_nNumAudioLanguages - 1 );

		m_nSelectedAudioLanguage = m_nAudioLanguages[ iSelectedLanguage ].languageCode;
	}
	else if ( !Q_strcmp( command, "PuzzlemakerSoundsEnabled" ) )
	{
		CGameUIConVarRef puzzlemaker_play_sounds( "puzzlemaker_play_sounds" );
		puzzlemaker_play_sounds.SetValue( true );
	}
	else if ( !Q_strcmp( command, "PuzzlemakerSoundsDisabled" ) )
	{
		CGameUIConVarRef puzzlemaker_play_sounds( "puzzlemaker_play_sounds" );
		puzzlemaker_play_sounds.SetValue( false );
	}
	else if ( !V_stricmp( "BtnSetupMicrophone", command ) )
	{
		CUIGameData::Get()->ExecuteOverlayCommand( "VoiceSettings", "#HL2_SetupMicrophoneSteam" );
	}
	else if ( Q_stricmp( "VoiceCommunicationDisabled", command ) == 0 )
	{
		CGameUIConVarRef voice_modenable( "voice_modenable" );
		CGameUIConVarRef voice_enable( "voice_enable" );
		voice_modenable.SetValue( 0 );
		voice_enable.SetValue( 0 );
		SetControlEnabled( "BtnSetupMicrophone", false );
		SetControlEnabled( "DrpVoicePTT", false );
	}
	else if ( !V_stricmp( "VoiceCommunicationPushToTalk", command ) )
	{
		CGameUIConVarRef voice_modenable( "voice_modenable" );
		CGameUIConVarRef voice_enable( "voice_enable" );
		voice_modenable.SetValue( 1 );
		voice_enable.SetValue( 1 );
		SetControlEnabled( "BtnSetupMicrophone", true );
		SetControlEnabled( "DrpVoicePTT", true );

		CGameUIConVarRef voice_vox( "voice_vox" );
		voice_vox.SetValue( 0 );
	}
	else if ( !V_stricmp( "VoiceCommunicationOpenMic", command ) )
	{
		CGameUIConVarRef voice_modenable( "voice_modenable" );
		CGameUIConVarRef voice_enable( "voice_enable" );
		voice_modenable.SetValue( 1 );
		voice_enable.SetValue( 1 );
		SetControlEnabled( "BtnSetupMicrophone", true );
		SetControlEnabled( "DrpVoicePTT", false );

		CGameUIConVarRef voice_vox( "voice_vox" );
		voice_vox.SetValue( 1 );
	}
	else if ( Q_stricmp( "Back", command ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( !Q_strcmp( command, "DrpVoicePTT" ) )
	{
		CBaseModFrame *pKeyBindings = CBaseModPanel::GetSingleton().OpenWindow( WT_KEYBINDINGS, this, true );
		pKeyBindings->PostMessage( pKeyBindings, new KeyValues( "RequestKeyBindingEdit", "binding", "+voicerecord" ) );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void Audio::UpdateEnhanceStereo( void )
{
	// headphones at high quality get enhanced stereo turned on
	CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
	CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
	CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );

	if ( !dsp_slow_cpu.GetBool() && Snd_PitchQuality.GetBool() && snd_surround_speakers.GetInt() == 0 )
	{
		dsp_enhance_stereo.SetValue( 1 );
	}
	else
	{
		dsp_enhance_stereo.SetValue( 0 );
	}
}

void Audio::UpdateFooter( bool bEnableCloud )
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_BBUTTON );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );

		footer->SetShowCloud( bEnableCloud );
	}
}

Panel *Audio::NavigateBack()
{
	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );

	UpdateFooter( false );

	return BaseClass::NavigateBack();
}

void Audio::UseSelectedLanguage()
{
	m_pchUpdatedAudioLanguage = GetLanguageShortName( m_nSelectedAudioLanguage );
}

void Audio::ResetLanguage()
{
	m_pchUpdatedAudioLanguage = "";
	m_nSelectedAudioLanguage = m_nCurrentAudioLanguage;
	PrepareLanguageList();
}

void Audio::AcceptLanguageChangeCallback() 
{
	Audio *pSelf = static_cast< Audio * >( CBaseModPanel::GetSingleton().GetWindow( WT_AUDIO ) );
	if ( pSelf )
	{
		pSelf->BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
}

void Audio::CancelLanguageChangeCallback()
{
	Audio *pSelf = static_cast< Audio * >( CBaseModPanel::GetSingleton().GetWindow( WT_AUDIO ) );
	if ( pSelf )
	{
		pSelf->ResetLanguage();
	}
}

void Audio::DiscoverAudioLanguages()
{
	char szCurrentUILanguage[50] = "";
	char szCurrentGameLanguage[50] = "";
	char szAvailableGameLanguages[1024] = "";

	engine->GetUILanguage( szCurrentUILanguage, sizeof( szCurrentUILanguage ) );
	ELanguage uiLanguage = PchLanguageToELanguage( szCurrentUILanguage );

	if ( uiLanguage == k_Lang_None || uiLanguage == k_Lang_English )
	{
		// not allowing to change the audio option unless you are in one of the non-english supported game languages
		return;
	}

#if !defined( NO_STEAM )
	if ( steamapicontext->SteamApps() )
	{
		V_strncpy( szCurrentGameLanguage, steamapicontext->SteamApps()->GetCurrentGameLanguage(), sizeof( szCurrentGameLanguage ) );
        if ( IsPlatformOSX() )
        {
            V_strncpy(szAvailableGameLanguages, "english,german,french,spanish,russian", sizeof(szAvailableGameLanguages) );
        }
        else
        {
            V_strncpy( szAvailableGameLanguages, steamapicontext->SteamApps()->GetAvailableGameLanguages(), sizeof( szAvailableGameLanguages ) );
        }
	}
#endif

	if ( !szAvailableGameLanguages[0] )
	{
		// there are no available other audio languages
		return;
	}

	if ( !szCurrentGameLanguage[0] )
	{
		// fallback to current ui language
		V_strncpy( szCurrentGameLanguage, szCurrentUILanguage, sizeof( szCurrentGameLanguage ) );
	}

	CSplitString languagesList( szAvailableGameLanguages, "," );
	CUtlVector< ELanguage > gameLanguages;
	for ( int i = 0; i < languagesList.Count(); i++ )
	{
		gameLanguages.AddToTail( PchLanguageToELanguage( languagesList[i] ) );
	}

	// english must be one of the expected audio choices
	if ( gameLanguages.Find( k_Lang_English ) == gameLanguages.InvalidIndex() )
	{
		return;
	}

	// the ui language must e one of the available localized audio choices
	if ( gameLanguages.Find( uiLanguage ) == gameLanguages.InvalidIndex() )
	{ 
		return;
	}

	m_nAudioLanguages[0].languageCode = uiLanguage;
	m_nAudioLanguages[1].languageCode = k_Lang_English;
	m_nNumAudioLanguages = 2;

	// Get the current spoken language
	m_nCurrentAudioLanguage = PchLanguageToELanguage( szCurrentGameLanguage );
	if ( m_nCurrentAudioLanguage != uiLanguage && m_nCurrentAudioLanguage != k_Lang_English )
	{
		// unexpected choice not available, force it to the ui language
		m_nCurrentAudioLanguage = uiLanguage;
	}

	m_nSelectedAudioLanguage = m_nCurrentAudioLanguage;

	m_pResourceLoadConditions->SetInt( "?localizedaudio", 1 );	
}

void Audio::PrepareLanguageList()
{
	if ( !m_drpLanguage || !m_nNumAudioLanguages )
		return;

	// Set up the base string for each button command
	char szCurrentButton[32];
	Q_strncpy( szCurrentButton, VIDEO_LANGUAGE_COMMAND_PREFIX, sizeof( szCurrentButton ) );

	int iCommandNumberPosition = Q_strlen( szCurrentButton );
	szCurrentButton[iCommandNumberPosition + 1] = '\0';

	int iSelectedLanguage = 0;
	for ( int i = 0; i < m_nNumAudioLanguages; i++ )
	{
		szCurrentButton[iCommandNumberPosition] = i + '0';
		m_drpLanguage->ModifySelectionString( szCurrentButton, GetLanguageVGUILocalization( m_nAudioLanguages[i].languageCode ) );

		if ( m_nCurrentAudioLanguage == m_nAudioLanguages[i].languageCode )
		{
			iSelectedLanguage = i;
		}
	}

	// Enable the valid possible choices
	for ( int i = 0; i < m_nNumAudioLanguages; ++i )
	{
		szCurrentButton[iCommandNumberPosition] = i + '0';
		char szString[256];
		if ( m_drpLanguage->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
		{
			m_drpLanguage->EnableListItem( szString, true );
		}
	}

	// Disable the remaining possible choices
	for ( int i = m_nNumAudioLanguages; i < MAX_DYNAMIC_AUDIO_LANGUAGES; ++i )
	{
		szCurrentButton[ iCommandNumberPosition ] = i + '0';
		char szString[256];
		if ( m_drpLanguage->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
		{
			m_drpLanguage->EnableListItem( szString, false );
		}
	}

	// Set the current selection
	szCurrentButton[iCommandNumberPosition] = iSelectedLanguage + '0';
	char szString[256];
	if ( m_drpLanguage->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
	{
		m_drpLanguage->SetCurrentSelection( szString );
	}
}

void Audio::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_sldGameVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldGameVolume" ) );
	m_sldMusicVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldMusicVolume" ) );
	m_drpSpeakerConfiguration = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpSpeakerConfiguration" ) );
	m_drpSoundQuality = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpSoundQuality" ) );
	m_drpLanguage = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpLanguage" ) );
	m_drpCaptioning = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpCaptioning" ) );
	m_drpVoiceCommunication = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpVoiceCommunication" ) );
	m_drpPuzzlemakerSounds = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpPuzzlemakerSounds" ) );

	PrepareLanguageList();

	if ( m_sldGameVolume )
	{
		m_sldGameVolume->Reset();
	}

	if ( m_sldMusicVolume )
	{
		m_sldMusicVolume->Reset();
	}

	if ( m_drpSpeakerConfiguration )
	{
		CGameUIConVarRef snd_surround_speakers( "Snd_Surround_Speakers" );

		switch ( snd_surround_speakers.GetInt() )
		{
		case 2:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_2Speakers" );
			break;
		case 4:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_4Speakers" );
			break;
		case 5:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_5Speakers" );
			break;
		case 0:
		default:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_Headphones" );
			break;
		}
	}

	if ( m_drpSoundQuality )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );

		int quality = 0;
		if ( !dsp_slow_cpu.GetBool() )
		{
			quality = 1;
		}
		if ( Snd_PitchQuality.GetBool() )
		{
			quality = 2;
		}

		switch ( quality )
		{
		case 1:
			m_drpSoundQuality->SetCurrentSelection( "#GameUI_Medium" );
			break;
		case 2:
			m_drpSoundQuality->SetCurrentSelection( "#GameUI_High" );
			break;
		case 0:
		default:
			m_drpSoundQuality->SetCurrentSelection( "#GameUI_Low" );
			break;
		}
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

	if ( m_drpPuzzlemakerSounds )
	{
		CGameUIConVarRef puzzlemaker_play_sounds( "puzzlemaker_play_sounds" );

		if ( puzzlemaker_play_sounds.GetBool() )
		{
			m_drpPuzzlemakerSounds->SetCurrentSelection( "#L4D360UI_Enabled" );
		}
		else
		{
			m_drpPuzzlemakerSounds->SetCurrentSelection( "#L4D360UI_Disabled" );
		}
	}

	if ( m_drpVoiceCommunication )
	{
		if ( !m_pVoiceTweak )
		{
			m_drpVoiceCommunication->SetEnabled( false );
			m_drpVoiceCommunication->SetCurrentSelection( "#L4D360UI_Disabled" );
			SetControlEnabled( "BtnSetupMicrophone", false );
			SetControlEnabled( "DrpVoicePTT", false );
		}
		else
		{
			CGameUIConVarRef voice_modenable( "voice_modenable" );
			CGameUIConVarRef voice_enable( "voice_enable" );
			CGameUIConVarRef voice_vox( "voice_vox" );

			bool bVoiceEnabled = voice_enable.GetBool() && voice_modenable.GetBool();
			SetControlEnabled( "BtnSetupMicrophone", bVoiceEnabled );

			if ( !bVoiceEnabled )
			{
				m_drpVoiceCommunication->SetCurrentSelection( "#L4D360UI_Disabled" );
				SetControlEnabled( "DrpVoicePTT", false );
			}
			else
			{
				if ( voice_vox.GetBool() )
				{
					m_drpVoiceCommunication->SetCurrentSelection( "#L4D360UI_OpenMic" );
					SetControlEnabled( "DrpVoicePTT", false );
				}
				else
				{
					m_drpVoiceCommunication->SetCurrentSelection( "#L4D360UI_PushToTalk" );
					SetControlEnabled( "DrpVoicePTT", true );
					
				}
			}
		}
	}

	if ( m_sldGameVolume )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_sldGameVolume->NavigateTo();
		m_ActiveControl = m_sldGameVolume;
	}

	UpdatePttBinding();
	UpdateFooter( true );
}

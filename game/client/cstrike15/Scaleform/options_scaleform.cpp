//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "options_scaleform.h"
#include "options_audio_scaleform.h"
#include "options_video_scaleform.h"
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
#include "platforminputdevice.h"

#ifndef _GAMECONSOLE
#include "steam/steam_api.h"
#endif

#define CSGO_TOTAL_OPTION_SLOTS_PER_SCREEN 20

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

const char *UTIL_Parse( const char *data, char *token, int sizeofToken );

int SortByPriority( COptionsScaleform::Option_t * const *pLeft, COptionsScaleform::Option_t * const *pRight )
{
	return ( ( *pLeft )->m_nPriority - ( *pRight )->m_nPriority );
}

COptionsScaleform* COptionsScaleform::m_pInstanceOptions = NULL;
COptionsScaleform::DialogType_e COptionsScaleform::m_DialogType( DIALOG_TYPE_NONE );
CUtlString COptionsScaleform::m_strMessage = "";

CUtlQueue<COptionsScaleform::DialogQueue_t>	COptionsScaleform::m_DialogQueue;

// Must match DialogType_e
static const char * s_rgszDialogScripts[] =
{
	"scripts/mouse_keyboard_options" PLATFORM_EXT ".txt",		// DIALOG_TYPE_KEYBOARD and DIALOG_TYPE_MOUSE
	"scripts/controller_options.txt",			// DIALOG_TYPE_CONTROLLER
#if defined( _X360 ) || defined( _PS3 )
	"scripts/game_options.consoles.txt", // DIALOG_TYPE_SETTINGS
#else
	"scripts/game_options.txt", // DIALOG_TYPE_SETTINGS
#endif // _X360
	"scripts/motion_controller_options.txt",				// DIALOG_TYPE_MOTION_CONTROLLER
	"scripts/motion_controller_move_options.txt",			// DIALOG_TYPE_MOTION_CONTROLLER_MOVE
	"scripts/motion_controller_sharpshooter_options.txt",	// DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER
	"scripts/video_options.txt",							// DIALOG_TYPE_VIDEO
	"scripts/video_advanced_options.txt",					// DIALOG_TYPE_VIDEO_ADVANCED
	"scripts/audio_options.txt",							// DIALOG_TYPE_AUDIO
};

COMPILE_TIME_ASSERT( ARRAYSIZE( s_rgszDialogScripts ) == ( COptionsScaleform::DIALOG_TYPE_COUNT - 1 ) ); // These must be updated in parallel.


SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnCancel ),
	SFUI_DECL_METHOD( OnUpdateValue ),
	SFUI_DECL_METHOD( OnHighlightWidget ),
	SFUI_DECL_METHOD( OnLayoutComplete ),
	SFUI_DECL_METHOD( OnPopulateGlyphRequest ),
	SFUI_DECL_METHOD( OnClearBind ),
	SFUI_DECL_METHOD( OnResetToDefaults ),
	SFUI_DECL_METHOD( OnRequestScroll ),
	SFUI_DECL_METHOD( OnResizeVertical ),
	SFUI_DECL_METHOD( OnResizeHorizontal ),
	SFUI_DECL_METHOD( OnSetSizeVertical ),
	SFUI_DECL_METHOD( OnSetSizeHorizontal ),
	SFUI_DECL_METHOD( OnSetNextMenu ),
	SFUI_DECL_METHOD( OnApplyChanges ),
	SFUI_DECL_METHOD( OnSetupMic ),
	SFUI_DECL_METHOD( OnMCCalibrate ),
	SFUI_DECL_METHOD( OnSaveProfile ),
	SFUI_DECL_METHOD( OnRefreshValues ),
	SFUI_DECL_METHOD( GetTotalOptionsSlots ),
	SFUI_DECL_METHOD( GetCurrentScrollOffset ),
	SFUI_DECL_METHOD( GetSafeZoneXMin ),
SFUI_END_GAME_API_DEF( COptionsScaleform, OptionsMenu );

COptionsScaleform::COptionsScaleform() :
	m_bVisible ( false ),
	m_bLoading ( false ),
	m_nScrollPos( 0 ),
	m_pConfirmDialog( NULL ),
	m_bNavButtonsEnabled( false ),
	m_bResetRequired( false ),
	m_bOptionsChanged( false ),
	m_NoticeType( NOTICE_TYPE_NONE ),
	m_pDeadZonePanel( NULL )
{
	memset( m_rgOptionsBySlot, 0, sizeof( m_rgOptionsBySlot ) );
	memset( m_rgTextBySlot, 0, sizeof( m_rgTextBySlot ) );
	
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

}



COptionsScaleform::~COptionsScaleform()
{
	StopListeningForAllEvents();

	m_vecOptions.PurgeAndDeleteElements();

	m_pInstanceOptions = NULL;
	m_DialogType = DIALOG_TYPE_NONE;

	if ( m_DialogQueue.Count() > 0 )
	{
		BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunMenuCommand", "command", "OpenOptionsQueued" ) );
		return;
	}

	if ( GameUI().IsInLevel() )
	{
		if ( ( ( CCStrike15BasePanel* )BasePanel() )->IsScaleformPauseMenuEnabled()  )
		{
			( ( CCStrike15BasePanel* )BasePanel() )->ShowMainMenu( false );
			( ( CCStrike15BasePanel* )BasePanel() )->RestorePauseMenu();
		}
	}
	else
	{
		if (( ( CCStrike15BasePanel* )BasePanel() )-> IsScaleformMainMenuEnabled()  )
		{
			( ( CCStrike15BasePanel* )BasePanel() )->ShowMainMenu( false );
			( ( CCStrike15BasePanel* )BasePanel() )->RestoreMainMenuScreen();
		}
	}


}


void COptionsScaleform::LoadDialog( DialogType_e type )
{
	if ( !m_pInstanceOptions )
	{
		if ( type == DIALOG_TYPE_NONE )
		{
			if (  m_DialogQueue.Count() > 0 )
			{
				DialogQueue_t dialogQueue = m_DialogQueue.RemoveAtHead();
				type = dialogQueue.m_Type;
				m_strMessage = dialogQueue.m_strMessage;
			}
			else
			{
				AssertMsg( false, "Trying to invoke a queued dialog with none in queue");
			}
		}

		m_DialogType = type;

#if defined( _PS3 )

		// Load the bindings for the specific device.
		engine->ExecuteClientCmd( VarArgs( "cl_read_ps3_bindings %d %d", GET_ACTIVE_SPLITSCREEN_SLOT(), GetDeviceFromDialogType( m_DialogType ) ) );

#endif

		// this is a convenient place to make sure scaleform has the correct keybindings
		g_pScaleformUI->RefreshKeyBindings();
		g_pScaleformUI->ShowActionNameWhenActionIsNotBound( false );

		/*
		if ( m_DialogType == DIALOG_TYPE_VIDEO )
		{
			engine->ExecuteClientCmd( "mat_updateconvars" );
		}
		*/

		if ( m_DialogType == DIALOG_TYPE_VIDEO ||  m_DialogType == DIALOG_TYPE_VIDEO_ADVANCED )
		{
			engine->ExecuteClientCmd( "mat_updateconvars" );
			m_pInstanceOptions = new COptionsVideoScaleform( );
		}
		else if ( m_DialogType == DIALOG_TYPE_AUDIO )
		{
			m_pInstanceOptions = new COptionsAudioScaleform( );
		}
		else
		{
			m_pInstanceOptions = new COptionsScaleform( );
		}
		
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, COptionsScaleform, m_pInstanceOptions, OptionsMenu );
	}
	else
	{
		AssertMsg( false, "Trying to load an option dialog when an instance already exists!" );
	}
}


void COptionsScaleform::UnloadDialog( void )
{
	// m_pInstanceControls is deleted in PostUnloadFlash. RemoveFlashElement is called at the end of the hide animation.
	if ( m_pInstanceOptions )
	{
		// Flash elements are removed after hide animation completes
		m_pInstanceOptions->Hide();
	}

#if defined( _PS3 )
	
	// We need to restore our settings based on our active device since we may have loaded other settings by entering this screen.
	InputDevice_t currentDevice = g_pInputSystem->GetCurrentInputDevice();
	// open the message box, but make sure we don't have a selected device and aren't already sampling for a device
	if( currentDevice != INPUT_DEVICE_NONE  )
	{
		// Load the bindings for the specific device.
		engine->ExecuteClientCmd( VarArgs( "cl_read_ps3_bindings %d %d", GET_ACTIVE_SPLITSCREEN_SLOT(), (int)currentDevice ) );
	}

#endif // _PS3

}


void COptionsScaleform::ShowMenu( bool bShow, DialogType_e type )
{
	if ( type == DIALOG_TYPE_CONTROLLER )
	{
		if( steamapicontext && steamapicontext->SteamController() )
		{
			ControllerHandle_t handles[ MAX_STEAM_CONTROLLERS ];
			int nControllers = steamapicontext->SteamController()->GetConnectedControllers( handles );
			if ( nControllers > 0 )
			{
				steamapicontext->SteamController()->ShowBindingPanel( handles[ 0 ] );
				return;
			}
		}
	}

	//TODO tear down existing instance if it already exists
	if ( bShow && !m_pInstanceOptions)
	{
		LoadDialog( type );
	}
	else
	{
		if ( bShow != m_pInstanceOptions->m_bVisible )
		{
			if ( bShow )
			{
				m_pInstanceOptions->Show();
			}
			else
			{
				m_pInstanceOptions->Hide();
			}
		}
	}
}


void COptionsScaleform::FlashLoaded( void )
{
	if ( m_FlashAPI && m_pScaleformUI )
	{
		ReadOptionsFromFile( s_rgszDialogScripts[m_DialogType] );

		WITH_SFVALUEARRAY( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, m_vecOptions.Count() );
			m_pScaleformUI->ValueArray_SetElement( args, 1, m_DialogType );

			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "Init", args, 2 );
			}
		}

		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	}
}


void COptionsScaleform::FlashReady( void )
{
	if ( m_FlashAPI && m_pScaleformUI )
	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
		m_bLoading = false;

		LockInputToSlot( m_iSplitScreenSlot );

		SFVALUE topPanel = m_pScaleformUI->Value_GetMember( m_FlashAPI, "TopPanel" );

		if ( topPanel )
		{
			SFVALUE panel = m_pScaleformUI->Value_GetMember( topPanel, "Panel" );

			if ( panel )
			{
				SFVALUE titlePanel = m_pScaleformUI->Value_GetMember( panel, "TitleText" );

				ISFTextObject * pTitleText = NULL;

				if ( titlePanel )
				{
					pTitleText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( titlePanel, "Title" );
					m_pScaleformUI->ReleaseValue( titlePanel );
				}

				if ( pTitleText )
				{
					const char * szName = NULL;
					
					IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetActiveUserId() );
					if ( pProfile )
					{
						szName = pProfile->GetName();
					}

					if ( !szName )
					{ 
						szName = "Player1";
					}

					wchar_t wcName[MAX_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConvertANSIToUnicode( szName,  wcName, sizeof( wcName ) );

					wchar_t wcTitle[128];
					wcTitle[0] = NULL;

					g_pVGuiLocalize->ConstructString( wcTitle, sizeof( wcTitle ), g_pVGuiLocalize->Find( "#SFUI_Controls_Title" ), 1, wcName );
					pTitleText->SetText( wcTitle );

					SafeReleaseSFTextObject( pTitleText );
				}

				SFVALUE controldummy = m_pScaleformUI->Value_GetMember( panel, "Control_Dummy" );
				if ( controldummy )
				{
					int nMaxSize = m_vecOptions.Count();
					for ( int i = 0; i < nMaxSize; i++ )
					{
						char szLabelName[64];
						V_snprintf( szLabelName, sizeof( szLabelName ), "Control_%i", i );
						SFVALUE controlpanel = m_pScaleformUI->Value_GetMember( controldummy, szLabelName );

						if ( controlpanel )
							m_rgTextBySlot[i] = m_pScaleformUI->TextObject_MakeTextObjectFromMember( controlpanel, "Control_Text" );

						m_pScaleformUI->ReleaseValue( controlpanel );
					}

					m_pScaleformUI->ReleaseValue( controldummy );
				}

				m_pScaleformUI->ReleaseValue( panel );
			}

			m_pScaleformUI->ReleaseValue( topPanel );
		}

		m_pDeadZonePanel = m_pScaleformUI->Value_GetMember( m_FlashAPI, "DeadZone" );
		
		if ( m_pDeadZonePanel )
		{
			m_pScaleformUI->Value_SetVisible( m_pDeadZonePanel, false );
		}

		// Perform initial layout
		LayoutDialog( 0, true );
	}
}


void COptionsScaleform::Show( void )
{
	if ( FlashAPIIsValid() && !m_bVisible )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowPanel", 0, NULL );
		}

		m_bVisible = true;
	}
}


void COptionsScaleform::Hide( void )
{
	if ( FlashAPIIsValid() && m_bVisible )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanel", 0, NULL );
		}

		m_bVisible = false;
	}
}


bool COptionsScaleform::PreUnloadFlash( void )
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	UnlockInput();
	g_pScaleformUI->ShowActionNameWhenActionIsNotBound( true );

	int nMaxSize = m_vecOptions.Count( );
	for ( int i = 0; i < nMaxSize; i++ )
	{
		SafeReleaseSFTextObject( m_rgTextBySlot[i] );
	}

	SafeReleaseSFVALUE( m_pDeadZonePanel );

	return CControlsFlashBaseClass::PreUnloadFlash();
}


void COptionsScaleform::PostUnloadFlash( void )
{
	if ( m_pInstanceOptions )
	{			
		delete m_pInstanceOptions;
	}
	else
	{
		Assert( false );
	}
}


void COptionsScaleform::OnApplyChanges( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SaveChanges();
}


void COptionsScaleform::OnCancel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_bResetRequired &&
		( ( m_DialogType == DIALOG_TYPE_VIDEO ) || ( m_DialogType == DIALOG_TYPE_VIDEO_ADVANCED ) ) )
	{
		m_NoticeType = NOTICE_TYPE_DISCARD_CHANGES;
		( ( CCStrike15BasePanel* )BasePanel() )->OnOpenMessageBox( "#SFUI_Settings_Video",
																	/*m_DialogType == DIALOG_TYPE_VIDEO ? "#SFUI_Settings_Changed_Resolution_Discard" : */"#SFUI_Settings_Changed_Discard",
																	"#SFUI_Settings_Discard_Nav",
																	( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL ),
																	this,
																	&m_pConfirmDialog );
	}
	else
	{
		if ( m_bOptionsChanged )
		{
			SaveChanges();
		}

		Hide();
	}
}


void COptionsScaleform::OnUpdateValue( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	int nWidgetIndex = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	int nValue = m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );
	m_pScaleformUI->Params_SetResult( obj, UpdateValue( nWidgetIndex, nValue ) );	
}

void COptionsScaleform::OnHighlightWidget( SCALEFORM_CALLBACK_ARGS_DECL )
{
#ifdef OSX
	//test on OSX to see if this will make a common crash go away.
	//maybe this is being somehow called with this as a dangling pointer?
	if ( m_pInstanceOptions != this )
	{
		return;
	}
#endif

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	int nWidgetIndex = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	int nMaxSize = m_vecOptions.Count( );
	if ( nWidgetIndex >= 0 && nWidgetIndex < nMaxSize )
	{
		Option_t * pOption = m_rgOptionsBySlot[nWidgetIndex];

		if ( IsMotionControllerDialog() )
		{
			bool bDeadZone = false;

			if ( pOption->m_szConVar && !V_strcmp( pOption->m_szConVar, "mc_dead_zone_radius" ) )
			{
				bDeadZone = true;
			}

			if ( m_pDeadZonePanel )
			{
				m_pScaleformUI->Value_SetVisible( m_pDeadZonePanel, bDeadZone );
			}
		}
	}
}

bool COptionsScaleform::UpdateValue( int nWidgetIndex, int nValue )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	m_bOptionsChanged = true;

	// the widget index is a bad hardcoded thing, if more options get added, this needs to be updated
	if ( m_DialogType == DIALOG_TYPE_VIDEO_ADVANCED || m_DialogType == DIALOG_TYPE_VIDEO && nWidgetIndex > 10 )
	{
		// changes to any of the advanced video options requires that the render device be reset
		m_bResetRequired = true;
	}

	int nMaxSize = m_vecOptions.Count( );
	if ( nWidgetIndex >= 0 && nWidgetIndex < nMaxSize )
	{
		Option_t * pOption = m_rgOptionsBySlot[nWidgetIndex];

		if ( pOption )
		{
			int iConVarSlot = pOption->m_bSystemValue ? 0 : m_iSplitScreenSlot;

			switch( pOption->m_Type )
			{
			case OPTION_TYPE_SLIDER:
				{
					OptionSlider_t * pOptionSlider = static_cast<OptionSlider_t *>( pOption );

					if ( nValue < 0 || nValue > 100 )
					{
						Assert( false );
						Warning ( "Widget updated with out of range value: %s - %i\n", pOptionSlider->m_szConVar, nValue);
					}

					nValue = clamp( nValue, 0, 100 );

					if ( !pOptionSlider->m_bLeftMin )
					{
						// Calculate the final value as if the left side of the slider = pOptionSlider->m_fMinValue
						nValue = 100 - nValue;
					}

					float fPercent = ( 0.01f * nValue );

					if ( pOptionSlider->m_fMaxValue <= 0.0f )
					{
						fPercent = 1.0f - fPercent;
					}

					float fRange = pOptionSlider->m_fMaxValue - pOptionSlider->m_fMinValue;
					pOptionSlider->m_fSlideValue = ( ( fPercent * fRange ) + pOptionSlider->m_fMinValue );

					SplitScreenConVarRef varOption( pOptionSlider->m_szConVar );
					varOption.SetValue( iConVarSlot, pOptionSlider->m_fSlideValue );

					WITH_SFVALUEARRAY( data, 1 )
					{
						m_pScaleformUI->ValueArray_SetElement( data, 0, nWidgetIndex );

						WITH_SLOT_LOCKED
						{
							g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RefreshInputField", data, 1 );
						}
					}
				}
				break;

			case OPTION_TYPE_CHOICE:
				{
					OptionChoice_t * pOptionChoice = static_cast< OptionChoice_t * >( pOption );

					int nCurrentChoice = pOptionChoice->m_nChoiceIndex;
					int nNumChoices = pOptionChoice->m_Choices.Count();

					nCurrentChoice += nValue;

					if ( nCurrentChoice < 0 )
					{
						nCurrentChoice = nNumChoices - 1;
					}
					else if ( nCurrentChoice >= nNumChoices )
					{
						nCurrentChoice = 0;
					}

					if ( HandleUpdateChoice( pOptionChoice, nCurrentChoice ) )
					{
						UpdateWidget( nWidgetIndex, static_cast< Option_t * >( pOptionChoice ) );
						DisableConditionalWidgets();
					}
				}
				break;

			case OPTION_TYPE_DROPDOWN:
				{
					OptionChoice_t * pOptionChoice = static_cast<OptionChoice_t *>( pOption );

					int nCurrentChoice = pOptionChoice->m_nChoiceIndex;
					int nNumChoices = pOptionChoice->m_Choices.Count();

					nCurrentChoice = nValue;

					if ( nCurrentChoice < 0 )
					{
						nCurrentChoice = nNumChoices - 1;
					}
					else if ( nCurrentChoice >= nNumChoices )
					{
						nCurrentChoice = 0;
					}

					if ( HandleUpdateChoice( pOptionChoice, nCurrentChoice ) )
					{
						UpdateWidget( nWidgetIndex,  static_cast<Option_t *>( pOptionChoice ) );
						DisableConditionalWidgets();
					}
				}
				break;

			case OPTION_TYPE_BIND:
				{
					OptionBind_t * pOptionBind = static_cast<OptionBind_t *>( pOption );

					if ( nValue == BIND_CMD_BIND )
					{
						ButtonCode_t code = g_pScaleformUI->GetCurrentKey();

						// Don't allow actions to be bound to ~
						if ( code == KEY_BACKQUOTE )
						{
							return false;
						}

						// do not allow primary navigation keys to be bound to filtered actions
						static const char *szNoFilterList[] = 
						{
							"screenshot",
						};

						static const int kNumNoFilterEntries = sizeof( szNoFilterList ) / sizeof( szNoFilterList[0] );

						if ( pOptionBind->m_szCommand && pOptionBind->m_szCommand[0] )
						{
							for ( int idx=0; idx < kNumNoFilterEntries; ++idx )
							{
								if ( StringHasPrefix( pOptionBind->m_szCommand, szNoFilterList[idx] ) )
								{
									if ( code == JOYSTICK_FIRST ||
										 code == MOUSE_LEFT ||
										 code == MOUSE_RIGHT ||
										 code == KEY_SPACE )
									{
										return false;
									}
								}
							}
						}


						UnbindOption( pOptionBind );

						char szCommand[ 256 ];
						V_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", g_pInputSystem->ButtonCodeToString( code ), pOptionBind->m_szCommand );
						engine->ExecuteClientCmd( szCommand );

						// Refresh the key glyphs associated with each action
						m_pScaleformUI->RefreshKeyBindings();

						RefreshValues( false );
					}

				}
				break;

			case OPTION_TYPE_CATEGORY:
				{
					UpdateWidget( nWidgetIndex, pOption );
				}
				break;

			default:
				{
					Warning ( "Attempted to update widget of bad type: %s - %i\n", pOption->m_szConVar, pOption->m_Type );
					Assert( false );
				}
				break;
			}
		}
		else
		{
			Assert( false );
		}
	}
	else
	{
		Assert( false );
		Warning( "Attempted to update widget that is outside of expected index range. Current number of expected widgets: %i\n", SF_FULL_SCREEN_SLOT );
	}

	return true;
}

bool COptionsScaleform::HandleUpdateChoice( OptionChoice_t * pOptionChoice, int nCurrentChoice )
{
	if ( pOptionChoice && 
		 nCurrentChoice >= 0 &&
		 nCurrentChoice < pOptionChoice->m_Choices.Count() )
	{
		pOptionChoice->m_nChoiceIndex = nCurrentChoice;
		int iConVarSlot = pOptionChoice->m_bSystemValue ? 0 : m_iSplitScreenSlot;

		SplitScreenConVarRef varOption( pOptionChoice->m_szConVar );
		varOption.SetValue( iConVarSlot, pOptionChoice->m_Choices[nCurrentChoice].m_szValue );

		return true;
	}

	return false;
}



void COptionsScaleform::OnLayoutComplete( SCALEFORM_CALLBACK_ARGS_DECL )
{
	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RefreshWidgetLayout", 0, NULL );
	}

	Show();

	PerformPostLayout();

	DisableConditionalWidgets();
}

void COptionsScaleform::DisableConditionalWidgets()
{
#ifndef POSIX
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	for ( int iOption = m_nScrollPos; iOption < m_vecOptions.Count(); ++iOption )
	{
		Option_t * pOption = m_vecOptions[iOption];

		int nWidgetID = -1;
		bool bDisable = false;

		HandleDisableConditionalWidgets( pOption, nWidgetID, bDisable );

		if ( nWidgetID != -1 )
		{
			WITH_SFVALUEARRAY( args, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, nWidgetID );
				m_pScaleformUI->ValueArray_SetElement( args, 1, bDisable );

				WITH_SLOT_LOCKED
				{
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "DisableWidget", args, 2 );
				}
			}
		}
	}
#endif
}

void COptionsScaleform::HandleDisableConditionalWidgets( Option_t * pOption, int & nWidgetIDOut, bool & bDisableOut )
{
	if ( pOption->m_szConVar )
	{
		nWidgetIDOut = pOption->m_nWidgetSlotID;
		bDisableOut = false;

		if ( !V_strcmp( pOption->m_szConVar, "m_customaccel_exponent" ) )
		{
			SplitScreenConVarRef m_customaccel( "m_customaccel" );
			bDisableOut = !m_customaccel.GetBool( m_iSplitScreenSlot );
		}
	}
}

void COptionsScaleform::GetTotalOptionsSlots( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, m_vecOptions.Count() );	
}

void COptionsScaleform::GetCurrentScrollOffset( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, m_nScrollPos );	
}

void COptionsScaleform::OnRequestScroll( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bSuccess = false;
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	int nScrollDirection = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	int nSize = m_vecOptions.Count() - CSGO_TOTAL_OPTION_SLOTS_PER_SCREEN;

	if ( ( nScrollDirection > 0 && ( nSize >= m_nScrollPos + nScrollDirection ) ) || 
		 ( nScrollDirection < 0 && ( ( m_nScrollPos + nScrollDirection ) >= 0 ) ) )
	{
		LayoutDialog( m_nScrollPos + nScrollDirection );
		vgui::surface()->PlaySound( "UI/buttonrollover.wav" );
		bSuccess = true;
	}

	m_pScaleformUI->Params_SetResult( obj, bSuccess );	
}


void COptionsScaleform::OnPopulateGlyphRequest( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	ISFTextObject * pText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( m_pScaleformUI->Params_GetArg( obj, 0 ), "Text" );

	if ( pText )
	{
		pText->SetTextHTML( m_pScaleformUI->Params_GetArgAsString( obj, 1 ) );
		SafeReleaseSFTextObject( pText );
	}
}


void COptionsScaleform::OnClearBind( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	int nWidgetIndex = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	int nMaxSize = m_vecOptions.Count( );
	Assert( nWidgetIndex < nMaxSize );

	if ( nWidgetIndex >= 0 && nWidgetIndex < nMaxSize )
	{
		Option_t * pOption = m_rgOptionsBySlot[nWidgetIndex];

		if ( pOption->m_Type == OPTION_TYPE_BIND )
		{
			OptionBind_t * pOptionBind = static_cast<OptionBind_t *>( pOption );
			
			UnbindOption( pOptionBind );
		}
	}
}


bool COptionsScaleform::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( buttonPressed & MESSAGEBOX_FLAG_OK  )
	{
		switch( m_NoticeType )
		{
		case NOTICE_TYPE_RESET_TO_DEFAULT:
			ResetToDefaults();
			break;

		case NOTICE_TYPE_INFO:
			break;

		default:
			AssertMsg( false, "Invalid message box notice type" );
		}

		m_NoticeType = NOTICE_TYPE_NONE;
	}

	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "changeUIDevice", 0, NULL );
		}
	}	

	return true;
}

void COptionsScaleform::OnResetToDefaults( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_NoticeType = NOTICE_TYPE_RESET_TO_DEFAULT;
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	const char * szTitle = NULL;
	const char * szBody = NULL;
	const char * szNav = NULL;

	switch( m_DialogType )
	{
	case DIALOG_TYPE_KEYBOARD:
	case DIALOG_TYPE_CONTROLLER:
	case DIALOG_TYPE_MOTION_CONTROLLER:
	case DIALOG_TYPE_MOTION_CONTROLLER_MOVE:
	case DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER:
		szTitle = "#SFUI_Controls_Confirm_Default_Title";
		szBody = "#SFUI_Controls_Confirm_Default_Msg";
		szNav = "#SFUI_Controls_Confirm_Default_Nav";
		break;

	default:
		szTitle = "#SFUI_Settings_Confirm_Default_Title";
		szBody = "#SFUI_Settings_Confirm_Default_Msg";
		szNav = "#SFUI_Settings_Confirm_Default_Nav";
		break;
	}

	( ( CCStrike15BasePanel* )BasePanel() )->OnOpenMessageBox( szTitle, szBody, szNav, ( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_AUTO_CLOSE_ON_DISCONNECT ), this, &m_pConfirmDialog );
}

void COptionsScaleform::BuildClanTagsLabels( CUtlVector<OptionChoiceData_t> &choices )
{
	// Build out the clan dropdown
	OptionChoiceData_t choiceElement;

	ConVarRef cl_clanid( "cl_clanid" );
	//const char *pClanID = cl_clanid.GetString();

#ifndef NO_STEAM
	ISteamFriends *pFriends = steamapicontext->SteamFriends();
	if ( pFriends )
	{
		V_strncpy( choiceElement.m_szValue, "0", sizeof( choiceElement.m_szValue ) );
		V_wcsncpy( choiceElement.m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_ClanTag_None" ), sizeof( choiceElement.m_wszLabel ) );
		choices.AddToTail( choiceElement );
		/* Removed for partner depot */
	}

#endif
}

void COptionsScaleform::ReadOptionsFromFile( const char * szFileName )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	KeyValues *pOptionKeys = new KeyValues( "options" );
	bool bResult = pOptionKeys->LoadFromFile( g_pFullFileSystem, szFileName, NULL );

	bool bDevMode = !IsCert() && !IsRetail();
	
	if ( bResult )
	{
		KeyValues *pKey = NULL;
		for ( pKey = pOptionKeys->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
		{
			// Skip disabled options
			if ( pKey->GetInt( "disable", 0 ) != 0 )
			{
				continue;
			}

			// Skip options that are only available in non-cert, non-retail builds
			if ( !bDevMode && ( pKey->GetInt( "devonly", 0 ) != 0 ) )
			{
				continue;
			}

			bool bSkip = false;
			KeyValues *pRestrictionsKey = pKey->FindKey( "restrictions" );

			if ( pRestrictionsKey )
			{
				KeyValues *pSubKey = NULL;

				for ( pSubKey = pRestrictionsKey->GetFirstSubKey(); pSubKey; pSubKey = pRestrictionsKey->GetNextKey() )
				{
					char const *szRestrictionName = pSubKey->GetName();
					if ( szRestrictionName[0] == '-' )
					{
						bool bParameterPresent = ( CommandLine()->FindParm( szRestrictionName ) != 0 );
						bool bParameterSkip = pSubKey->GetBool( ( const char * ) NULL, true );
						if ( bParameterPresent == bParameterSkip )
						{
							bSkip = true;
							break;
						}
					}
					else if ( const ConVar *pVar = g_pCVar->FindVar( szRestrictionName ) )
					{
						if ( !V_strcmp( pVar->GetString(), pSubKey->GetString() ) )
						{
							bSkip = true;
							break;
						}
					}
				}
			}

			if ( bSkip )
			{
				continue;
			}

			// Get the type and instantiate an appropriate option
			OptionType_e type = OPTION_TYPE_TOTAL; 

			const char * szType = pKey->GetString( "type", "" );

			if ( !V_strcmp( szType, "slider" ) )
			{
				type = OPTION_TYPE_SLIDER;
			}
			else if ( !V_strcmp( szType, "choice" ) )
			{
				type = OPTION_TYPE_CHOICE;
			}
			else if ( !V_strcmp( szType, "dropdown" ) )
			{
				type = OPTION_TYPE_DROPDOWN;
			}
			else if ( !V_strcmp( szType, "bind" ) )
			{
				type = OPTION_TYPE_BIND;
			}
			else if ( !V_strcmp( szType, "category" ) )
			{
				type = OPTION_TYPE_CATEGORY;
			}			

			Option_t * pOption = NULL;

			switch ( type )
			{
			case OPTION_TYPE_SLIDER:
				pOption = new OptionSlider_t();
				break;

			case OPTION_TYPE_CHOICE:
			case OPTION_TYPE_DROPDOWN:
				pOption = new OptionChoice_t();
				break;

			case OPTION_TYPE_BIND:
				pOption = new OptionBind_t();
				break;

			case OPTION_TYPE_CATEGORY:
				pOption = new Option_t();
				break;

			default:
				Warning ( "Bad widget type read from file: %s\n", pKey->GetString( "name", "" ) );
				Assert( false );
				break;
			}

			// Update the option with values from the data file
			if ( pOption )
			{
				// Shared values
				pOption->m_Type = type;
				g_pVGuiLocalize->ConvertANSIToUnicode( pKey->GetString( "name", "" ),  pOption->m_wcLabel, sizeof( pOption->m_wcLabel ) );
				V_strncpy( pOption->m_szConVar, pKey->GetString( "convar", "" ), sizeof( pOption->m_szConVar ) );

				g_pVGuiLocalize->ConvertANSIToUnicode( pKey->GetString( "tooltip", "" ), pOption->m_wcTooltip, sizeof( pOption->m_wcTooltip ) );

				pOption->m_bSystemValue = pKey->GetBool( "systemvalue" );
				pOption->m_bRefreshInventoryIconsWhenIncreased = pKey->GetBool( "refresh_inventory_icons_when_increased" );
				pOption->m_nPriority = pKey->GetInt( "priority", 0 );

				// Type specific values
				if ( pOption->m_Type == OPTION_TYPE_SLIDER )
				{
					OptionSlider_t * pOptionSlider = static_cast<OptionSlider_t *>( pOption );

					pOptionSlider->m_bLeftMin = pKey->GetBool( "leftmin", true );

					SplitScreenConVarRef varOption( pOptionSlider->m_szConVar );

					if ( varOption.IsValid() )
					{
						pOptionSlider->m_fMinValue =  pKey->GetBool( "customrange", false ) ? pKey->GetFloat( "minvalue" ) : varOption.GetMin();
						pOptionSlider->m_fMaxValue = pKey->GetBool( "customrange", false ) ? pKey->GetFloat( "maxvalue" ) : varOption.GetMax();
					}
					else
					{
						Assert( false );
						Warning( "Data File Error. Convar associated with control not found: %s", pOptionSlider->m_szConVar );
					}

					SetSliderWithConVar( pOptionSlider );

					if ( pOptionSlider->m_fMaxValue <= pOptionSlider->m_fMinValue )
					{
						Warning( "Datafile error. maxvalue and minvalue cannot be the same. nimvalue cannot be < maxvalue. Control: %s\n", pOptionSlider->m_szConVar );
					}
					else if ( ( pOptionSlider->m_fSlideValue > pOptionSlider->m_fMaxValue ) ||
							  ( pOptionSlider->m_fSlideValue < pOptionSlider->m_fMinValue ) )
					{
						Warning( "Datafile error. maxvalue and minvalue not within range of ConvVar value. ConVar: %s (%.1f) not in range %.1f to %.1f\n", pOptionSlider->m_szConVar, 
																																						   pOptionSlider->m_fSlideValue,
																																						   pOptionSlider->m_fMinValue,
																																						   pOptionSlider->m_fMaxValue);
					}
				}
				else if ( pOption->m_Type == OPTION_TYPE_CHOICE || pOption->m_Type == OPTION_TYPE_DROPDOWN )
				{
					OptionChoice_t * pOptionChoice = static_cast<OptionChoice_t *>( pOption );

					KeyValues *pChoicesKey = pKey->FindKey( "choices" );
					if ( pOption->m_Type == OPTION_TYPE_DROPDOWN )
						pChoicesKey = pKey->FindKey( "dropdown" );

					if ( pChoicesKey )
					{				
						// special case the splitscreen mode because
						// it can only have the value "0" when running
						// on an SD display

						if ( !InitUniqueWidget( pKey->GetName(), pOptionChoice ) )
						{
							KeyValues *pSubKey = NULL;

							for ( pSubKey = pChoicesKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
							{
								int nChoice = pOptionChoice->m_Choices.AddToTail();
								OptionChoiceData_t * pNewOptionChoice = &( pOptionChoice->m_Choices[ nChoice ]);

								wchar_t wszTemp[ SF_OPTIONS_MAX ];
								wchar_t *pwchLabel = g_pVGuiLocalize->Find( pSubKey->GetName() );
								if ( !pwchLabel || pwchLabel[ 0 ] == L'\0' )
								{
									g_pVGuiLocalize->ConvertANSIToUnicode( pSubKey->GetName(), wszTemp, sizeof( wszTemp ) );
									pwchLabel = wszTemp;
								}

								V_wcsncpy( pNewOptionChoice->m_wszLabel, pwchLabel, sizeof( pNewOptionChoice->m_wszLabel ) );
								V_strncpy( pNewOptionChoice->m_szValue, pSubKey->GetString(), sizeof( pNewOptionChoice->m_szValue ) );

								//
								// Autodetect options support
								//
								if ( !V_stricmp( "#SFUI_Settings_Choice_Autodetect", pSubKey->GetName() )
									&& V_strstr( pOption->m_szConVar, "_optionsui" ) )
								{
									static KeyValues *s_kvOptionsUiDefaults = NULL;
									if ( !s_kvOptionsUiDefaults )
									{
										s_kvOptionsUiDefaults = new KeyValues( "defaults" );
										if ( !s_kvOptionsUiDefaults->LoadFromFile( filesystem, "cfg/videodefaults.txt", "USRLOCAL" ) )
											s_kvOptionsUiDefaults->Clear();
									}

									// This option is Auto - also concatenate it with the actual value that was auto-detected
									CFmtStr fmtDefaultSetting( "setting.%.*s", V_strlen( pOption->m_szConVar ) - V_strlen( "_optionsui" ), pOption->m_szConVar );
									if ( char const *szOptionsUiDefault = s_kvOptionsUiDefaults->GetString( fmtDefaultSetting, NULL ) )
									{
										int nResult = FindChoiceFromString( pOptionChoice, szOptionsUiDefault );
										if ( nResult >= 0 && nResult < nChoice )
										{
											V_wcsncat( pNewOptionChoice->m_wszLabel, L" : <font color='#707070'>", sizeof( pNewOptionChoice->m_wszLabel ) );
											V_wcsncat( pNewOptionChoice->m_wszLabel, pOptionChoice->m_Choices[nResult].m_wszLabel, sizeof( pNewOptionChoice->m_wszLabel ) );
											V_wcsncat( pNewOptionChoice->m_wszLabel, L"</font>", sizeof( pNewOptionChoice->m_wszLabel ) );
										}
									}
								}
							}

							if ( pOptionChoice->m_Choices.Count() < 2 )
							{
								Assert( false );
								Warning( "Type is choice but there is only one option: %s\n", pOptionChoice->m_szConVar );
							}
						}

						SetChoiceWithConVar( pOptionChoice );
					}
					else
					{
						Assert( false );
						Warning( "\"choices\" key not found for widget: %s\n", pOptionChoice->m_szConVar );
					}
				}
				else if ( pOption->m_Type == OPTION_TYPE_BIND )
				{
					OptionBind_t * pOptionBind = static_cast<OptionBind_t *>( pOption );
					V_strncpy( pOptionBind->m_szCommand, pKey->GetString( "command", "" ), sizeof( pOptionBind->m_szCommand ) );
				}			

				m_vecOptions.AddToTail( pOption );	
			}
		}

		m_vecOptions.Sort( SortByPriority );
	}
	else
	{
		Warning( "Failed to read file: %s\n", szFileName );
	}

	pOptionKeys->deleteThis();

	Assert( bResult && m_vecOptions.Count() > 0 );
}

bool COptionsScaleform::InitUniqueWidget( const char * szWidgetID, OptionChoice_t * pOptionChoice  )
{
	bool bFound = false;
	if ( !V_strcmp( szWidgetID, "ClanTag" ) )
	{
		//m_pResolutionWidget = pOptionChoice;
		BuildClanTagsLabels( pOptionChoice->m_Choices );
		bFound = true;
	}

	return bFound;
}

int COptionsScaleform::FindChoiceFromString( OptionChoice_t * pOption, const char * szMatch )
{
	int nResult = -1;

	for ( int nChoice = 0; nChoice < pOption->m_Choices.Count(); ++nChoice )
	{
		if ( V_stricmp( pOption->m_Choices[ nChoice ].m_szValue, szMatch ) == 0 )
		{
			nResult = nChoice;
			break;
		}

		// We need to compare values in case we have "0" & "0.00000". 
		if ( ( szMatch[0] >= '0' && szMatch[0] <= '9' ) || szMatch[0] == '-' )
		{
			float flVal = V_atof( szMatch );
			float flChoiceVal = V_atof( pOption->m_Choices[ nChoice ].m_szValue );
			if ( flVal == flChoiceVal )
			{
				nResult = nChoice;
				break;
			}
		}
	}

	return nResult;
}

void COptionsScaleform::SetChoiceWithConVar( OptionChoice_t * pOption, bool bForceDefaultValue )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	SplitScreenConVarRef varOption( pOption->m_szConVar );
	int iConVarSlot = pOption->m_bSystemValue ? 0 : m_iSplitScreenSlot;

	if ( bForceDefaultValue )
	{
		varOption.SetValue( iConVarSlot, varOption.GetDefault() );
	}

	int nResult = -1;

	nResult = FindChoiceFromString( pOption, varOption.GetString( iConVarSlot ) );

	if ( !V_strcmp( pOption->m_szConVar, "rate" ) )
	{
		int nCurrentValue = varOption.GetInt( iConVarSlot );

		// Find the value with a bigger value than user's setting
		for ( int nChoice = 0; nChoice < pOption->m_Choices.Count(); ++nChoice )
		{
			int nChoiceRateValue = V_atoi( pOption->m_Choices[ nChoice ].m_szValue );
			if ( ( nCurrentValue <= nChoiceRateValue )	// found a setting with a bigger value
				|| ( nChoice == pOption->m_Choices.Count() - 1 ) ) // ... or last setting is "Unrestricted"
			{
				nResult = nChoice;
				break;
			}
		}
	}
	else if ( !V_strcmp( pOption->m_szConVar, "cl_clanid" ) )
	{
		//ResolutionModes_t current = FindCurrentResolution();
		ConVarRef cl_clanid( "cl_clanid" );
		const char *pClanID = cl_clanid.GetString();

		//char szResolutionName[ 256 ];

		//GetResolutionName( current.m_nWidth, current.m_nHeight, szResolutionName, sizeof( szResolutionName ) );

		for ( int nChoice = 0; nChoice < pOption->m_Choices.Count(); ++nChoice )
		{
			if ( V_stricmp( pOption->m_Choices[ nChoice ].m_szValue, pClanID ) == 0 )
			{
				nResult = nChoice;
				break;
			}
		}

		if ( nResult == -1 )
		{
			nResult =  pOption->m_Choices.Count() - 1;
		}

		pOption->m_nChoiceIndex = nResult;
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


void COptionsScaleform::SetSliderWithConVar( OptionSlider_t * pOption )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	int iConVarSlot = pOption->m_bSystemValue ? 0 : m_iSplitScreenSlot;

	const char * szConVar = pOption->m_szConVar;
	if ( szConVar && szConVar[0] )
	{
		SplitScreenConVarRef varOption( szConVar );
		pOption->m_fSlideValue = varOption.GetFloat( iConVarSlot );
	}
}


void COptionsScaleform::LayoutDialog( const int nVecOptionsOffset, const bool bInit )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	int nSize = m_vecOptions.Count();
	
	Assert( nSize > 0 && "ReadOptionsFromFile successful?");

	//int nLowerBound = nVecOptionsOffset;
	// It is possible for the number of options to be < than the number of total available slots. Account for this.
	//int nUpperBound = MIN( nSize, nVecOptionsOffset + ( nSize < SF_OPTIONS_SLOTS_COUNT ? nSize : SF_OPTIONS_SLOTS_COUNT ) );

	if ( bInit )
	{
		int nWidgetIndex = 0;

		for ( int nOptionID = 0; nOptionID < ( nSize ? nSize : SF_OPTIONS_SLOTS_COUNT_MAX ); nOptionID++ )
		{
			UpdateWidget( nWidgetIndex, m_vecOptions[nOptionID] );
			m_rgOptionsBySlot[nWidgetIndex] = m_vecOptions[nOptionID];
			m_vecOptions[nOptionID]->m_nWidgetSlotID = nWidgetIndex;

			nWidgetIndex++;
		}
	}
		
	m_nScrollPos = nVecOptionsOffset;

	WITH_SFVALUEARRAY( args, 1 )
	{
		m_pScaleformUI->ValueArray_SetElement( args, 0, nVecOptionsOffset );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "LayoutUpdateHighlight", args, 1 );
	}
}

void COptionsScaleform::UpdateWidget( const int nWidgetIndex, Option_t const * const pOption )
{
	int nMaxSize = m_vecOptions.Count( );
	Assert( nWidgetIndex < nMaxSize );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( nWidgetIndex >= 0 && nWidgetIndex < nMaxSize )
	{
		WITH_SFVALUEARRAY( data, 6 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, nWidgetIndex );
			m_pScaleformUI->ValueArray_SetElement( data, 1, pOption->m_Type );

			if ( m_rgTextBySlot[nWidgetIndex] )
			{
				WITH_SLOT_LOCKED
				{
					m_rgTextBySlot[nWidgetIndex]->SetText( pOption->m_wcLabel );
				}
			}

			switch( pOption->m_Type )
			{
				case OPTION_TYPE_SLIDER:
					{
						OptionSlider_t const * const pOptionSlider = static_cast<OptionSlider_t const * const>( pOption );

						if ( pOptionSlider->m_fMaxValue != pOptionSlider->m_fMinValue )
						{
							float fPercent = ( pOptionSlider->m_fSlideValue - pOptionSlider->m_fMinValue ) / ( pOptionSlider->m_fMaxValue - pOptionSlider->m_fMinValue );

							if ( pOptionSlider->m_fMaxValue <= 0.0f )
							{
								fPercent = 1.0f - fPercent;
							}

							int nPercent = static_cast<int>( fPercent * 100.f );

							if ( !pOptionSlider->m_bLeftMin )
							{
								// Calculate the final value as if the left side of the slider = pOptionSlider->m_fMinValue
								nPercent = 100 - nPercent;
							}

							m_pScaleformUI->ValueArray_SetElement( data, 2, nPercent );
							m_pScaleformUI->ValueArray_SetElement( data, 3, pOptionSlider->m_szConVar );

						}
						else
						{
							Assert( false );
							Warning( "Datafile error. maxvalue and minvalue cannot be the same. Control: %s\n", pOptionSlider->m_szConVar );
						}
					}
					break;

				case OPTION_TYPE_CATEGORY:
					{
											 m_pScaleformUI->ValueArray_SetElement( data, 2, "" );
											 m_pScaleformUI->ValueArray_SetElement( data, 3, "" );
					}
					break;

				case OPTION_TYPE_CHOICE:
					{
						OptionChoice_t const * const pOptionChoice = static_cast< OptionChoice_t const * const >( pOption );

						Assert( pOptionChoice->m_nChoiceIndex != -1 );

						if ( pOptionChoice->m_nChoiceIndex != -1 )
						{
							if ( pOptionChoice->m_nChoiceIndex < pOptionChoice->m_Choices.Count() )
							{
								OptionChoiceData_t const & value = pOptionChoice->m_Choices[pOptionChoice->m_nChoiceIndex];

								m_pScaleformUI->ValueArray_SetElement( data, 2, value.m_wszLabel );
								m_pScaleformUI->ValueArray_SetElement( data, 3, pOptionChoice->m_szConVar );
							}
						}
					}
					break;

				case OPTION_TYPE_DROPDOWN:
					{
						OptionChoice_t const * const pOptionChoice = static_cast<OptionChoice_t const * const>( pOption );

						Assert( pOptionChoice->m_nChoiceIndex != -1 );

						if ( pOptionChoice->m_nChoiceIndex != -1 )
						{
							if ( pOptionChoice->m_nChoiceIndex < pOptionChoice->m_Choices.Count() )
							{
								//OptionChoiceData_t const & value = pOptionChoice->m_Choices[pOptionChoice->m_nChoiceIndex];

								m_pScaleformUI->ValueArray_SetElement( data, 2, pOptionChoice->m_nChoiceIndex );
								m_pScaleformUI->ValueArray_SetElement( data, 3, pOptionChoice->m_szConVar );

								SFVALUE dropdownData = CreateFlashArray( pOptionChoice->m_Choices.Count( ) );
								for ( int i = 0; i < pOptionChoice->m_Choices.Count(); i++ )
								{
									OptionChoiceData_t const & dropValue = pOptionChoice->m_Choices[i];
									m_pScaleformUI->Value_SetArrayElement( dropdownData, i, dropValue.m_wszLabel );
								}

								m_pScaleformUI->ValueArray_SetElement( data, 4, dropdownData );
							}		
						}
					}
					break;

				case OPTION_TYPE_BIND:
					{
						OptionBind_t const * const pOptionBind = static_cast<OptionBind_t const * const>( pOption );
				
						char szCommand[ 32];
						szCommand[0] = 0;
					
						V_snprintf( szCommand, sizeof( szCommand ), "${%s}", pOptionBind->m_szCommand );

						bool bBindValueSet = false;
						for ( int nCode = BUTTON_CODE_NONE; nCode < BUTTON_CODE_LAST; ++nCode )
						{
							ButtonCode_t code = static_cast<ButtonCode_t>( nCode );

							// Only clear the binding for the current input device being configured. This allows a profile to maintain a configuration for multiple controller types.
							if ( m_DialogType == DIALOG_TYPE_KEYBOARD || m_DialogType == DIALOG_TYPE_MOUSE ||
								 m_DialogType == DIALOG_TYPE_AUDIO )
							{
								if ( !IsKeyCode( code ) && !IsMouseCode( code ) )
								{
									continue;
								}
							}
							else if ( m_DialogType == DIALOG_TYPE_CONTROLLER || IsMotionControllerDialog() )
							{
								if ( !IsJoystickCode( code ) )
								{
									continue;
								}
							}

							const char * szBinding = gameuifuncs->GetBindingForButtonCode( code );

							// Check if there's a binding for this key
							if ( !szBinding || !szBinding[0] )
								continue;


							// If we use this binding, display the key in our list

							if ( ActionsAreTheSame( szBinding, pOptionBind->m_szCommand ) )
							{
								if ( m_DialogType == DIALOG_TYPE_KEYBOARD || m_DialogType == DIALOG_TYPE_MOUSE ||
									 m_DialogType == DIALOG_TYPE_AUDIO )
								{
									const char * szKeyString = g_pInputSystem->ButtonCodeToString( code );
									wchar_t wcKey[MAX_PLAYER_NAME_LENGTH];
									g_pVGuiLocalize->ConvertANSIToUnicode( szKeyString,  wcKey, sizeof( wcKey ) );

									bBindValueSet = true;
									m_pScaleformUI->ValueArray_SetElement( data, 2, wcKey );
								}
								else if ( m_DialogType == DIALOG_TYPE_CONTROLLER ||
										  IsMotionControllerDialog() )
								{
									const wchar_t * szGlyphResult = m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( szCommand, 0, true );

									bBindValueSet = true;
									m_pScaleformUI->ValueArray_SetElement( data, 2, szGlyphResult );
								}
							}
						}

						if ( !bBindValueSet )
						{
							m_pScaleformUI->ValueArray_SetElement( data, 2, "" );
						}
					}
					break;
		
				default:
					Assert( false );
					break;
			}
	
			m_pScaleformUI->ValueArray_SetElement( data, 5, pOption->m_wcTooltip );

			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onUpdateWidget", data, 6 );
			}
		}
	}
}



bool COptionsScaleform::ActionsAreTheSame( const char *szAction1, const char *szAction2 )
{
	if ( V_stricmp( szAction1, szAction2 ) == 0 )
		return true;

	if ( ( V_stricmp( szAction1, "+duck" ) == 0 || V_stricmp( szAction1, "toggle_duck" ) == 0 ) && 
		( V_stricmp( szAction2, "+duck" ) == 0 || V_stricmp( szAction2, "toggle_duck" ) == 0 ) )
	{
		// +duck and toggle_duck are interchangable
		return true;
	}

	if ( ( V_stricmp( szAction1, "+zoom" ) == 0 || V_stricmp( szAction1, "toggle_zoom" ) == 0 ) && 
		( V_stricmp( szAction2, "+zoom" ) == 0 || V_stricmp( szAction2, "toggle_zoom" ) == 0 ) )
	{
		// +zoom and toggle_zoom are interchangable
		return true;
	}

	return false;
}


void COptionsScaleform::ResetToDefaults( void )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	m_bOptionsChanged = false;
	m_bResetRequired = false;

	// reset all convars to default values defined by code in case we don't reset them via cfg
	RefreshValues( true );

	// if this is a control screen, reset binds as well
	if ( m_DialogType == DIALOG_TYPE_KEYBOARD ||
		m_DialogType == DIALOG_TYPE_MOUSE ||
		m_DialogType == DIALOG_TYPE_CONTROLLER ||
		IsMotionControllerDialog() )
	{

#if defined( _PS3 )

		// Reset the convars etc. related to controllers.  Does NOT reset bindings.
		engine->ExecuteClientCmd( "exec controller.ps3.cfg"  );

		// Now reset the bindings for the active device.
		engine->ExecuteClientCmd( VarArgs( "cl_reset_ps3_bindings %d %d", m_iSplitScreenSlot, GetDeviceFromDialogType( m_DialogType ) ) );

#else

		// Reset all bind options with defaults
		const char * szConfigFile = "cfg/controller" PLATFORM_EXT ".cfg";

		if ( m_DialogType == DIALOG_TYPE_KEYBOARD || m_DialogType == DIALOG_TYPE_MOUSE )
		{
			szConfigFile = "cfg/config_default.cfg";
		}

		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( !g_pFullFileSystem->ReadFile( szConfigFile, NULL, buf ) )
		{
			Assert( false );
			Warning( "Unable to locate config file used for default settings: %s\n", szConfigFile );
			return;
		}

		const char *data = ( const char * )buf.Base();

		while ( data != NULL )
		{
			char cmd[64];
			data = UTIL_Parse( data, cmd, sizeof( cmd ) );
			if ( V_strlen( cmd ) <= 0 )
				break;

			if ( !V_stricmp(cmd, "bind") ||
				!V_stricmp(cmd, "cmd2 bind") )
			{
				// FIXME:  If we ever support > 2 player splitscreen this will need to be reworked.
				int nJoyStick = 0;
				if ( !V_stricmp(cmd, "cmd2 bind") )
				{
					nJoyStick = 1;
				}

				// Key name
				char szKeyName[256];
				data = UTIL_Parse( data, szKeyName, sizeof(szKeyName) );
				if ( szKeyName[ 0 ] == '\0' )
					break; // Error

				char szBinding[256];
				data = UTIL_Parse( data, szBinding, sizeof(szBinding) );
				if ( szKeyName[ 0 ] == '\0' )  
					break; // Error

				// Skip it if it's a bind for the other slit
				if ( nJoyStick != m_iSplitScreenSlot )
					continue;

				// Bind it
				char szCommand[ 256 ];
				V_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", szKeyName, szBinding );
				engine->ExecuteClientCmd( szCommand );
			}
			else if ( m_DialogType == DIALOG_TYPE_CONTROLLER ||
					  IsMotionControllerDialog() )
			{
				// L4D: Use Defaults also resets cvars listed in config_default.cfg
				CGameUIConVarRef var( cmd );
				if ( var.IsValid() )
				{
					char szValue[256] = "";
					data = UTIL_Parse( data, szValue, sizeof(szValue) );
					var.SetValue( szValue );
				}
			}
		}

#endif // _PS3

	}

	// Refresh the dialog
	RefreshValues( false );

	WriteUserSettings( m_iSplitScreenSlot );
}


void COptionsScaleform::UnbindOption( OptionBind_t const * const pOptionBind )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	for ( int nCode = BUTTON_CODE_NONE; nCode < BUTTON_CODE_LAST; ++nCode )
	{
		ButtonCode_t code = static_cast<ButtonCode_t>( nCode );
			
		// Only clear the binding for the current input device being configured. This allows a profile to maintain a configuration for multiple controller types.
		if ( m_DialogType == DIALOG_TYPE_KEYBOARD || m_DialogType == DIALOG_TYPE_MOUSE ||
			 m_DialogType == DIALOG_TYPE_AUDIO )
		{
			if ( !IsKeyCode( code ) && !IsMouseCode( code ) )
			{
				continue;
			}
		}
		else if ( m_DialogType == DIALOG_TYPE_CONTROLLER ||
				  IsMotionControllerDialog() )
		{
			if ( !IsJoystickCode( code ) )
			{
				continue;
			}
		}

		const char * szBinding = gameuifuncs->GetBindingForButtonCode( code );

		// Check if there's a binding for this key
		if ( !szBinding || !szBinding[0] )
			continue;

		// If we use this binding, display the key in our list

		if ( ActionsAreTheSame( szBinding, pOptionBind->m_szCommand ) )
		{
			char szCommand[ 256 ];
			V_snprintf( szCommand, sizeof( szCommand ), "unbind %s", g_pInputSystem->ButtonCodeToString( code ) );
			engine->ExecuteClientCmd( szCommand );
		}
	}
}

void COptionsScaleform::OnNotifyStartEvent( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "StartKeyPressed", 0, NULL );
		}
	}
}

void COptionsScaleform::NotifyStartEvent( void )
{
	if ( m_pInstanceOptions )
	{
		m_pInstanceOptions->OnNotifyStartEvent();
	}
}


void COptionsScaleform::OnResizeVertical( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int nResizeDirection = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	ConVarRef varOption( "safezoney" );

	float fNewSafe =  ( varOption.GetFloat() + ( nResizeDirection * 0.005f ) );

	fNewSafe = clamp( fNewSafe, varOption.GetMin(), varOption.GetMax() );
	varOption.SetValue( fNewSafe );
}

void COptionsScaleform::OnResizeHorizontal( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int nResizeDirection = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	ConVarRef varOption( "safezonex" );

	float fNewSafe =  ( varOption.GetFloat() + ( nResizeDirection * 0.005f ) );

	fNewSafe = clamp( fNewSafe, engine->GetSafeZoneXMin(), varOption.GetMax() );
	varOption.SetValue( fNewSafe );
}

void COptionsScaleform::OnSetSizeVertical( SCALEFORM_CALLBACK_ARGS_DECL )
{
	ConVarRef varOption( "safezoney" );
	float fNewSafe = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	fNewSafe = clamp( fNewSafe, varOption.GetMin(), varOption.GetMax() );
	varOption.SetValue( fNewSafe );
}

void COptionsScaleform::OnSetSizeHorizontal( SCALEFORM_CALLBACK_ARGS_DECL )
{
	ConVarRef varOption( "safezonex" );
	float fNewSafe = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	fNewSafe = clamp( fNewSafe, engine->GetSafeZoneXMin(), varOption.GetMax() );
	varOption.SetValue( fNewSafe );
}

void COptionsScaleform::OnSetNextMenu( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int nDialogID = m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	const char * szMessage = m_pScaleformUI->Params_GetArgAsString( obj, 1 );

	DialogQueue_t dialog;
	dialog.m_Type = static_cast<DialogType_e>( nDialogID );
	dialog.m_strMessage = szMessage;

	m_DialogQueue.Insert( dialog );
}

void COptionsScaleform::ApplyChangesToSystemConVar( const char *pConVarName, int value )
{
	SplitScreenConVarRef convar( pConVarName );
	convar.SetValue( 0, value );
}


CEG_NOINLINE void COptionsScaleform::SaveChanges( void )
{
	// Saves out the current profile
	if ( m_bOptionsChanged )
	{
		PreSaveChanges();

		CEG_ENCRYPT_FUNCTION( COptionsScaleform_ApplyChanges );

		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
		WriteUserSettings( XBX_GetActiveUserId() );

		m_bOptionsChanged = false;
	}
}

bool COptionsScaleform::SplitRestartConvar( const char * szConVarRestartIn, char * szConVarOut, int nOutLength )
{
	int nSuffixLength = 8;
	const char * szTest = V_strstr( szConVarRestartIn, "_restart" );
	if ( !szTest )
	{
		szTest = V_strstr( szConVarRestartIn, "_optionsui" );
		nSuffixLength = 10;
	}

	if ( szTest )
	{
		int nStringLength = V_strlen( szConVarRestartIn );
		nStringLength -= nSuffixLength;

		V_StrLeft(  szConVarRestartIn, nStringLength, szConVarOut, nOutLength );

		return true;
	}

	return false;
}


void COptionsScaleform::OnSaveProfile( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Save the values to the user's profile.
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	WriteUserSettings( XBX_GetActiveUserId() );
}

void COptionsScaleform::GetSafeZoneXMin( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, engine->GetSafeZoneXMin() );
}


void COptionsScaleform::OnSetupMic( SCALEFORM_CALLBACK_ARGS_DECL )
{
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamFriends() &&
		steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
	{
		steamapicontext->SteamFriends()->ActivateGameOverlay( "VoiceSettings" );
	}
#endif
}

void COptionsScaleform::RefreshValues( bool bForceDefault )
{
	for ( int iOption = 0; iOption < m_vecOptions.Count(); ++iOption )
	{
		Option_t * pOption = m_vecOptions[ iOption ];
		int iConVarSlot = pOption->m_bSystemValue ? 0 : m_iSplitScreenSlot;

		if ( pOption->m_Type == OPTION_TYPE_CHOICE || pOption->m_Type == OPTION_TYPE_DROPDOWN )
		{
			OptionChoice_t * pOptionChoice = static_cast<OptionChoice_t *>( pOption );

			if ( pOptionChoice->m_szConVar )
			{
				SplitScreenConVarRef varOption( pOptionChoice->m_szConVar );

				if ( bForceDefault )
				{
					pOptionChoice->m_nChoiceIndex = -1;
				}

				SetChoiceWithConVar( pOptionChoice, bForceDefault );
			}
		}
		else if (  pOption->m_Type == OPTION_TYPE_SLIDER )
		{
			OptionSlider_t * pOptionSlider = static_cast<OptionSlider_t *>( pOption );

			if ( pOptionSlider->m_szConVar)
			{
				SplitScreenConVarRef varOption( pOptionSlider->m_szConVar );

				if ( bForceDefault )
				{
					varOption.SetValue( iConVarSlot, varOption.GetDefault() );
				}

				SetSliderWithConVar( pOptionSlider );
			}
		}
		else if (  pOption->m_Type == OPTION_TYPE_BIND )
		{
			if ( bForceDefault )
			{
				// Default for bind widgets is unbound
				UnbindOption( static_cast<OptionBind_t const * const>( pOption ) );
			}
		}
	}

	LayoutDialog( m_nScrollPos );
	DisableConditionalWidgets();
}


void COptionsScaleform::PerformPostLayout( void )
{
	// REI: Disabled this, it puts the scrollbar in an unusable state right now.
	//      (Also, right now we never use this, it used to be triggered by some
	//      transitions from Audio -> Keybindings screens

	/*
	if (  !V_strcmp( m_strMessage.String(), "ShowPTT" ) )
	{
		m_strMessage.Clear();

		FOR_EACH_VEC( m_vecOptions, i )
		{
			Option_t * pOption = m_vecOptions[i];

			if ( pOption && pOption->m_Type == OPTION_TYPE_BIND )
			{
				OptionBind_t * pOptionBind = static_cast<OptionBind_t *>( pOption );

				if ( !V_strcmp( pOptionBind->m_szCommand, "+voicerecord" ) )
				{
					m_nScrollPos = i;
					LayoutDialog( m_nScrollPos );
					break;
				}
			}
		}
	}
	*/
}

bool COptionsScaleform::IsBindMenuRaised()
{
	if ( m_DialogType == DIALOG_TYPE_KEYBOARD ||
	     m_DialogType == DIALOG_TYPE_MOUSE ||
		 m_DialogType == DIALOG_TYPE_CONTROLLER ||
		 m_DialogType == DIALOG_TYPE_AUDIO ||
		 IsMotionControllerDialog() )
	{
		return true;
	}

	return false;
}

void COptionsScaleform::OnMCCalibrate( SCALEFORM_CALLBACK_ARGS_DECL )
{
	BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunMenuCommand", "command", "OpenMotionCalibrationDialog" ) );
}

void COptionsScaleform::OnRefreshValues( SCALEFORM_CALLBACK_ARGS_DECL )
{
	RefreshValues( false );
}

void COptionsScaleform::OnEvent( KeyValues *kvEvent )
{
	/* Removed for partner depot */
}

void COptionsScaleform::WriteUserSettings( int iSplitScreenSlot )
{

#if defined( _PS3 )

	// Save out the current bindings for the active device.
	engine->ClientCmd_Unrestricted( VarArgs( "cl_write_ps3_bindings %d %d", iSplitScreenSlot, GetDeviceFromDialogType( m_DialogType ) ) );

#endif

	// Save the values to the user's profile.
	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", iSplitScreenSlot ) );
}

int COptionsScaleform::GetDeviceFromDialogType( DialogType_e eDialogType )
{
	switch ( eDialogType )
	{
		case DIALOG_TYPE_NONE:
			return (int) INPUT_DEVICE_NONE;

		case DIALOG_TYPE_KEYBOARD:
		case DIALOG_TYPE_CONTROLLER:
			return (int) INPUT_DEVICE_GAMEPAD;

		case DIALOG_TYPE_SETTINGS:
			return (int) INPUT_DEVICE_NONE;

		case DIALOG_TYPE_MOTION_CONTROLLER:
		case DIALOG_TYPE_MOTION_CONTROLLER_MOVE:
			return (int) INPUT_DEVICE_PLAYSTATION_MOVE;
		
		case DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER:
			return (int) INPUT_DEVICE_SHARPSHOOTER;

		case DIALOG_TYPE_VIDEO:
		case DIALOG_TYPE_VIDEO_ADVANCED:
		case DIALOG_TYPE_AUDIO:
		case DIALOG_TYPE_SCREENSIZE:
			return (int) INPUT_DEVICE_NONE;
		default:
			Warning( "Dialog type %d not handled in switch statement.", (int)eDialogType );
			break;

			//MDS_TODO Add INPUT_DEVICE_SHARPSHOOTER for a new SharpShooterBindings screen.
	}

	return (int) INPUT_DEVICE_NONE;
}

bool COptionsScaleform::IsMotionControllerDialog( void )
{
	return ( m_DialogType == DIALOG_TYPE_MOTION_CONTROLLER ||
			 m_DialogType == DIALOG_TYPE_MOTION_CONTROLLER_MOVE ||
			 m_DialogType == DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER );
}
#endif // INCLUDE_SCALEFORM

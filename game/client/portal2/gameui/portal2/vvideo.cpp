//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "VVideo.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "modes.h"
#include "videocfg/videocfg.h"
#include "VGenericConfirmation.h"

#include "materialsystem/materialsystem_config.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define VIDEO_ANTIALIAS_COMMAND_PREFIX "_antialias"
#define VIDEO_RESOLUTION_COMMAND_PREFIX "_res"

int GetScreenAspectMode( int width, int height );

Video::Video( Panel *parent, const char *panelName ):
BaseClass( parent, panelName ),
m_autodelete_pResourceLoadConditions( (KeyValues*) NULL )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#GameUI_Video" );

	m_pResourceLoadConditions = new KeyValues( "video" );
	m_autodelete_pResourceLoadConditions.Assign( m_pResourceLoadConditions );

	// only gamma is not part of the apply logic
	// save off original value for discard logic
	CGameUIConVarRef mat_monitorgamma( "mat_monitorgamma" );
	m_flOriginalGamma = mat_monitorgamma.GetFloat();

	m_sldBrightness = NULL;
	m_drpAspectRatio = NULL;
	m_drpResolution = NULL;
	m_drpDisplayMode = NULL;
	m_drpPowerSavingsMode = NULL;
	m_drpSplitScreenDirection = NULL;
	m_btnAdvanced = NULL;

	m_bAcceptPowerSavingsWarning = false;

	m_nNumResolutionModes = 0;

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
	m_iCurrentResolutionWidth = config.m_VideoMode.m_Width;
	m_iCurrentResolutionHeight = config.m_VideoMode.m_Height;
	m_bCurrentWindowed = config.Windowed();

	GetRecommendedSettings();

	m_bPreferRecommendedResolution = false;
	m_bDirtyValues = false;
	m_bEnableApply = false;

	SetFooterEnabled( true );
	UpdateFooter();
}

Video::~Video()
{
}

void Video::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	if ( m_bCurrentWindowed )
	{
		m_pResourceLoadConditions->SetInt( "?windowed", 1 );
	}

	BaseClass::ApplySchemeSettings( pScheme );

	m_sldBrightness = dynamic_cast< SliderControl* >( FindChildByName( "SldBrightness" ) );
	m_drpAspectRatio = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpAspectRatio" ) );
	m_drpResolution = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpResolution" ) );
	m_drpDisplayMode = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpDisplayMode" ) );
	m_drpPowerSavingsMode = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpPowerSavingsMode" ) );
	m_drpSplitScreenDirection = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpSplitScreenDirection" ) );

	SetupState( false );

	if ( m_sldBrightness )
	{
		m_sldBrightness->Reset();

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_sldBrightness->NavigateTo();
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

	UpdateFooter();
}

bool Video::GetRecommendedSettings( void )
{
	m_iRecommendedResolutionWidth = 640;
	m_iRecommendedResolutionHeight = 480; 
	m_iRecommendedAspectRatio = GetScreenAspectMode( m_iRecommendedResolutionWidth, m_iRecommendedResolutionHeight );
	m_bRecommendedWindowed = false;
	m_bRecommendedNoBorder = false;
	
	// Off by default since we aren't dynamically adjusting the state and don't want to get users into a hole.
	// In the future, we could set this via steamapicontext->SteamUtils()->GetCurrentBatteryPower() == 255 ? 0 : 1; // 255 indicates AC power, else battery
	m_nRecommendedPowerSavingsMode = 0;

#if !defined( _GAMECONSOLE )
	KeyValues *pConfigKeys = new KeyValues( "VideoConfig" );
	if ( !pConfigKeys )
		return false;

	if ( !ReadCurrentVideoConfig( pConfigKeys, true ) )
	{
		pConfigKeys->deleteThis();
		return false;
	}

	m_iRecommendedResolutionWidth = pConfigKeys->GetInt( "setting.defaultres", m_iRecommendedResolutionWidth );
	m_iRecommendedResolutionHeight = pConfigKeys->GetInt( "setting.defaultresheight", m_iRecommendedResolutionHeight );
	m_iRecommendedAspectRatio = GetScreenAspectMode( m_iRecommendedResolutionWidth, m_iRecommendedResolutionHeight );
	m_bRecommendedWindowed = !pConfigKeys->GetBool( "setting.fullscreen", !m_bRecommendedWindowed );
	m_bRecommendedNoBorder = pConfigKeys->GetBool( "setting.nowindowborder", m_bRecommendedNoBorder );

	pConfigKeys->deleteThis();
#endif

	return true;
}

void Video::Activate()
{
	BaseClass::Activate();

	UpdateFooter();
}

static void AcceptDefaultsOkCallback()
{
	Video *pSelf = 
		static_cast< Video* >( CBaseModPanel::GetSingleton().GetWindow( WT_VIDEO ) );
	if ( pSelf )
	{
		pSelf->SetDefaults();
	}
}

static void DiscardChangesOkCallback()
{
	Video *pSelf = 
		static_cast< Video* >( CBaseModPanel::GetSingleton().GetWindow( WT_VIDEO ) );
	if ( pSelf )
	{
		pSelf->DiscardChangesAndClose();
	}
}

void Video::DiscardChangesAndClose()
{
	if ( !m_bCurrentWindowed )
	{
		// the brightness slider is not part of apply, so need to restore when discarding
		CGameUIConVarRef mat_monitorgamma( "mat_monitorgamma" );
		if ( m_flOriginalGamma != mat_monitorgamma.GetFloat() )
		{
			mat_monitorgamma.SetValue( m_flOriginalGamma );
		}
	}

	BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
}

void Video::SetDefaults()
{
	SetupState( true );
}

void Video::SetupState( bool bUseRecommendedSettings )
{
	if ( !bUseRecommendedSettings )
	{
		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
		m_iResolutionWidth = config.m_VideoMode.m_Width;
		m_iResolutionHeight = config.m_VideoMode.m_Height;
		m_iAspectRatio = GetScreenAspectMode( m_iResolutionWidth, m_iResolutionHeight );
		m_bWindowed = config.Windowed();
#if !defined( POSIX )
		m_bNoBorder = config.NoWindowBorder();
#else
		m_bNoBorder = false;
#endif
		CGameUIConVarRef mat_powersavingsmode( "mat_powersavingsmode" );
		m_nPowerSavingsMode = clamp( mat_powersavingsmode.GetInt(), 0, 1 );
	}
	else
	{
		m_iResolutionWidth = m_iRecommendedResolutionWidth;
		m_iResolutionHeight = m_iRecommendedResolutionHeight;
		m_iAspectRatio = m_iRecommendedAspectRatio;
		m_bWindowed = m_bRecommendedWindowed;
#if !defined( POSIX )
		m_bNoBorder = m_bRecommendedNoBorder;
#else
		m_bNoBorder = false;
#endif
		m_nPowerSavingsMode = m_nRecommendedPowerSavingsMode;

		m_bDirtyValues = true;
		m_bPreferRecommendedResolution = true;
	}
	
	PrepareResolutionList();

	SetControlEnabled( "SldBrightness", !m_bCurrentWindowed );

	if ( m_drpAspectRatio )
	{
		switch ( m_iAspectRatio )
		{
		default:
		case 0:
			m_drpAspectRatio->SetCurrentSelection( "#GameUI_AspectNormal" );
			break;
		case 1:
			m_drpAspectRatio->SetCurrentSelection( "#GameUI_AspectWide16x9" );
			break;
		case 2:
			m_drpAspectRatio->SetCurrentSelection( "#GameUI_AspectWide16x10" );
			break;
		}
	}

	if ( m_drpDisplayMode )
	{
		if ( m_bWindowed )
		{
#if !defined( POSIX )
			if ( m_bNoBorder )
			{
				m_drpDisplayMode->SetCurrentSelection( "#L4D360UI_VideoOptions_Windowed_NoBorder" );
			}
			else
#endif
			{
				m_drpDisplayMode->SetCurrentSelection( "#GameUI_Windowed" );
			}
		}
		else
		{
			m_drpDisplayMode->SetCurrentSelection( "#GameUI_Fullscreen" );
		}
	}

	SetPowerSavingsState();
}

void Video::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		// apply changes and close
		ApplyChanges();
		BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		break;

	case KEY_XBUTTON_B:
		if ( m_bDirtyValues || ( m_sldBrightness && m_sldBrightness->IsDirty() ) )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation *pConfirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_VideoSettingsConf";
			data.pMessageText = "#PORTAL2_VideoSettingsDiscardQ";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &DiscardChangesOkCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Discard";
			data.bCancelButtonEnabled = true;
			pConfirmation->SetUsageData( data );	
		}
		else
		{
			// cancel
			BaseClass::OnKeyCodePressed( code );
		}
		break;

	case KEY_XBUTTON_X:
		// use defaults
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation *pConfirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_VideoSettingsConf";
			data.pMessageText = "#PORTAL2_VideoSettingsUseDefaultsQ";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptDefaultsOkCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Reset";
			data.bCancelButtonEnabled = true;
			pConfirmation->SetUsageData( data );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void Video::OnCommand( const char *command )
{
	if ( !V_stricmp( command, "#GameUI_AspectNormal" ) )
	{
		m_iAspectRatio = 0;
		m_bDirtyValues = true;
		PrepareResolutionList();
	}
	else if ( !V_stricmp( command, "#GameUI_AspectWide16x9" ) )
	{
		m_iAspectRatio = 1;
		m_bDirtyValues = true;
		PrepareResolutionList();
	}
	else if ( !V_stricmp( command, "#GameUI_AspectWide16x10" ) )
	{
		m_iAspectRatio = 2;
		m_bDirtyValues = true;
		PrepareResolutionList();
	}
	else if ( StringHasPrefix( command, VIDEO_RESOLUTION_COMMAND_PREFIX ) )
	{
		int iCommandNumberPosition = Q_strlen( VIDEO_RESOLUTION_COMMAND_PREFIX );
		int iResolution = clamp( command[ iCommandNumberPosition ] - '0', 0, m_nNumResolutionModes - 1 );

		m_iResolutionWidth = m_nResolutionModes[ iResolution ].m_nWidth;
		m_iResolutionHeight = m_nResolutionModes[ iResolution ].m_nHeight;

		m_bDirtyValues = true;
	}
	else if ( !V_stricmp( command, "#GameUI_Windowed" ) )
	{
		m_bWindowed = true;
		m_bNoBorder = false;
		m_bDirtyValues = true;
		PrepareResolutionList();
	}
#if !defined( POSIX )
	else if ( !V_stricmp( command, "#L4D360UI_VideoOptions_Windowed_NoBorder" ) )
	{
		m_bWindowed = true;
		m_bNoBorder = true;
		m_bDirtyValues = true;
		PrepareResolutionList();
	}
#endif
	else if ( !V_stricmp( command, "#GameUI_Fullscreen" ) )
	{
		m_bWindowed = false;
		m_bNoBorder = false;
		m_bDirtyValues = true;
		PrepareResolutionList();
	}
	else if ( !V_stricmp( command, "ShowAdvanced" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_ADVANCEDVIDEO, this, true );
	}
	else if ( !V_stricmp( command, "PowerSavingsDisabled" ) )
	{
		if ( !m_bAcceptPowerSavingsWarning )
		{
			// show the warning first, and restore the current state
			ShowPowerSavingsWarning();
			SetPowerSavingsState();
		}
		else
		{
			m_nPowerSavingsMode = 0;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "PowerSavingsEnabled" ) )
	{
		if ( !m_bAcceptPowerSavingsWarning )
		{
			// show the warning first, and restore the current state
			ShowPowerSavingsWarning();
			SetPowerSavingsState();
		}
		else
		{
			m_nPowerSavingsMode = 1;
			m_bDirtyValues = true;
		}
	}
	else if ( !Q_stricmp( command, "#L4D360UI_SplitScreenDirection_Default" ) )
	{
		if ( m_drpSplitScreenDirection && m_drpSplitScreenDirection->IsEnabled() )
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			ss_splitmode.SetValue( 0 );
			m_bDirtyValues = true;
		}
	}
	else if ( !Q_stricmp( command, "#L4D360UI_SplitScreenDirection_Horizontal" ) )
	{
		if ( m_drpSplitScreenDirection && m_drpSplitScreenDirection->IsEnabled() )
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			ss_splitmode.SetValue( 1 );
			m_bDirtyValues = true;
		}
	}
	else if ( !Q_stricmp( command, "#L4D360UI_SplitScreenDirection_Vertical" ) )
	{
		if ( m_drpSplitScreenDirection && m_drpSplitScreenDirection->IsEnabled() )
		{
			CGameUIConVarRef ss_splitmode( "ss_splitmode" );
			ss_splitmode.SetValue( 2 );
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

static vmode_t s_pWindowedModes[] = 
{
	// NOTE: These must be sorted by ascending width, then ascending height
	{ 640, 480, 32, 60 },
	{ 852, 480, 32, 60 },
	{ 1280, 720, 32, 60 },
	{ 1920, 1080, 32, 60 },
};

static void GenerateWindowedModes( CUtlVector< vmode_t > &windowedModes, int nCount, vmode_t *pFullscreenModes )
{
	int nFSMode = 0;
	for ( int i = 0; i < ARRAYSIZE( s_pWindowedModes ); ++i )
	{
		while ( true )
		{
			if ( nFSMode >= nCount )
				break;

			if ( pFullscreenModes[nFSMode].width > s_pWindowedModes[i].width )
				break;

			if ( pFullscreenModes[nFSMode].width == s_pWindowedModes[i].width )
			{
				if ( pFullscreenModes[nFSMode].height > s_pWindowedModes[i].height )
					break;

				if ( pFullscreenModes[nFSMode].height == s_pWindowedModes[i].height )
				{
					// Don't add the matching fullscreen mode
					++nFSMode;
					break;
				}
			}

			windowedModes.AddToTail( pFullscreenModes[nFSMode] );
			++nFSMode;
		}

		windowedModes.AddToTail( s_pWindowedModes[i] );
	}

	for ( ; nFSMode < nCount; ++nFSMode )
	{
		windowedModes.AddToTail( pFullscreenModes[nFSMode] );
	}
}

static bool HasResolutionMode( const ResolutionMode_t * RESTRICT pModes, const int numModes, const vmode_t * RESTRICT pTestMode )
{
	for ( int i = 0 ; i < numModes ; ++i )
	{
		if ( pModes[ i ].m_nWidth  == pTestMode->width   &&
			 pModes[ i ].m_nHeight == pTestMode->height )
		{
			return true;
		}
	}

	return false;
}

void Video::GetResolutionName( vmode_t *pMode, char *pOutBuffer, int nOutBufferSize, bool &bIsNative )
{
	int desktopWidth, desktopHeight;
	gameuifuncs->GetDesktopResolution( desktopWidth, desktopHeight );
	V_snprintf( pOutBuffer, nOutBufferSize, "%i x %i", pMode->width, pMode->height );
	bIsNative = ( pMode->width == desktopWidth ) && ( pMode->height == desktopHeight );
}

void Video::PrepareResolutionList()
{
	if ( !m_drpResolution )
		return;

	int iOverflowPosition = 1;
	m_nNumResolutionModes = 0;

	// Set up the base string for each button command
	char szCurrentButton[ 32 ];
	V_strncpy( szCurrentButton, VIDEO_RESOLUTION_COMMAND_PREFIX, sizeof( szCurrentButton ) );

	int iCommandNumberPosition = V_strlen( szCurrentButton );
	szCurrentButton[ iCommandNumberPosition + 1 ] = '\0';

	// get full video mode list
	vmode_t *plist = NULL;
	int count = 0;
	gameuifuncs->GetVideoModes( &plist, &count );

	int desktopWidth, desktopHeight;
	gameuifuncs->GetDesktopResolution( desktopWidth, desktopHeight );

	// Add some extra modes in if we're running windowed
	CUtlVector< vmode_t > windowedModes;
	if ( m_bWindowed )
	{
		GenerateWindowedModes( windowedModes, count, plist );
		count = windowedModes.Count();
		plist = windowedModes.Base();
	}

	if ( m_drpAspectRatio )
	{
		m_drpAspectRatio->EnableListItem( "#GameUI_AspectNormal", false );
		m_drpAspectRatio->EnableListItem( "#GameUI_AspectWide16x9", false );
		m_drpAspectRatio->EnableListItem( "#GameUI_AspectWide16x10", false );
	}

	// iterate all the video modes adding them to the dropdown
	for ( int i = 0; i < count; i++, plist++ )
	{
		// don't show modes bigger than the desktop for windowed mode
		if ( m_bWindowed && ( plist->width > desktopWidth || plist->height > desktopHeight ) )
			continue;

		// skip a mode if it is somehow already in the list (bug 30693)
		if ( HasResolutionMode( m_nResolutionModes, m_nNumResolutionModes, plist ) )
			continue;

		bool bIsNative;
		char szResolutionName[ 256 ];
		GetResolutionName( plist, szResolutionName, sizeof( szResolutionName ), bIsNative );

		int iAspectMode = GetScreenAspectMode( plist->width, plist->height );

		if ( iAspectMode >= 0 )
		{
			if ( m_drpAspectRatio )
			{
				switch ( iAspectMode )
				{
				default:
				case 0:
					m_drpAspectRatio->EnableListItem( "#GameUI_AspectNormal", true );
					break;
				case 1:
					m_drpAspectRatio->EnableListItem( "#GameUI_AspectWide16x9", true );
					break;
				case 2:
					m_drpAspectRatio->EnableListItem( "#GameUI_AspectWide16x10", true );
					break;
				}
			}
		}

		// filter the list for those matching the current aspect
		if ( iAspectMode == m_iAspectRatio )
		{
			if ( m_nNumResolutionModes == MAX_DYNAMIC_VIDEO_MODES )
			{
				// No more will fit in this drop down, and it's too late in the product to make this a scrollable list
				// Instead lets make the list less fine grained by removing middle entries
				if ( iOverflowPosition >= MAX_DYNAMIC_VIDEO_MODES )
				{
					// Wrap the second entry
					iOverflowPosition = 1;
				}

				int iShiftPosition = iOverflowPosition;

				while ( iShiftPosition < MAX_DYNAMIC_VIDEO_MODES - 1 )
				{
					// Copy the entry in front of us over this entry
					szCurrentButton[ iCommandNumberPosition ] = ( iShiftPosition + 1 ) + '0';

					char szResName[ 256 ];
					if ( m_drpResolution->GetListSelectionString( szCurrentButton, szResName, sizeof(szResName) ) )
					{
						szCurrentButton[ iCommandNumberPosition ] = iShiftPosition + '0';
						m_drpResolution->ModifySelectionString( szCurrentButton, szResName );
					}

					m_nResolutionModes[ iShiftPosition ].m_nWidth = m_nResolutionModes[ iShiftPosition + 1 ].m_nWidth;
					m_nResolutionModes[ iShiftPosition ].m_nHeight = m_nResolutionModes[ iShiftPosition + 1 ].m_nHeight;

					iShiftPosition++;
				}

				iOverflowPosition += 2;
				m_nNumResolutionModes--;
			}

			szCurrentButton[ iCommandNumberPosition ] = m_nNumResolutionModes + '0';

			if ( bIsNative )
			{
#if defined(POSIX)
				V_strncat( szResolutionName, " %S", sizeof( szResolutionName ) );
#else
				V_strncat( szResolutionName, " %s", sizeof( szResolutionName ) );
#endif
				m_drpResolution->ModifySelectionStringParms( szCurrentButton, "#L4D360UI_Native" );
			}

			m_drpResolution->ModifySelectionString( szCurrentButton, szResolutionName );

			m_nResolutionModes[ m_nNumResolutionModes ].m_nWidth = plist->width;
			m_nResolutionModes[ m_nNumResolutionModes ].m_nHeight = plist->height;

			m_nNumResolutionModes++;
		}
	}

	// Enable the valid possible choices
	for ( int i = 0; i < m_nNumResolutionModes; ++i )
	{
		szCurrentButton[ iCommandNumberPosition ] = i + '0';
		char szString[256];
		if ( m_drpResolution->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
		{
			m_drpResolution->EnableListItem( szString, true );
		}
	}

	// Disable the remaining possible choices
	for ( int i = m_nNumResolutionModes; i < MAX_DYNAMIC_VIDEO_MODES; ++i )
	{
		szCurrentButton[ iCommandNumberPosition ] = i + '0';
		char szString[256];
		if ( m_drpResolution->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
		{
			m_drpResolution->EnableListItem( szString, false );
		}
	}

	// Find closest selection
	int selectedItemID;
	for ( selectedItemID = 0; selectedItemID < m_nNumResolutionModes; ++selectedItemID )
	{
		if ( m_nResolutionModes[ selectedItemID ].m_nHeight > m_iResolutionHeight )
			break;

		if (( m_nResolutionModes[ selectedItemID ].m_nHeight == m_iResolutionHeight ) &&
			( m_nResolutionModes[ selectedItemID ].m_nWidth > m_iResolutionWidth ))
			break;
	}

	// Go back to the one that matched or was smaller (and prevent -1 when none are smaller)
	selectedItemID = MAX( selectedItemID - 1, 0 );

	if ( m_nResolutionModes[ selectedItemID ].m_nWidth != m_iResolutionWidth ||
		m_nResolutionModes[ selectedItemID ].m_nHeight != m_iResolutionHeight )
	{
		// not an exact match, try to find closest to current or recommended
		int nDesiredPixels = m_iCurrentResolutionWidth * m_iCurrentResolutionHeight;
		if ( m_bPreferRecommendedResolution )
		{
			nDesiredPixels = m_iRecommendedResolutionWidth * m_iRecommendedResolutionHeight;
		}
		int nBetterMode = -1;
		int nBest = INT_MAX;
		for ( int i = 0; i < m_nNumResolutionModes; ++i )
		{
			int nPixels = m_nResolutionModes[i].m_nWidth * m_nResolutionModes[i].m_nHeight;
			int nClosest = abs( nDesiredPixels - nPixels );
			if ( nBest > nClosest )
			{
				nBest = nClosest;
				nBetterMode = i;
			}
		}
			
		if ( nBetterMode != -1 )
		{
			selectedItemID = nBetterMode;
		}
	}

	// Select the currently set type
	szCurrentButton[ iCommandNumberPosition ] = selectedItemID + '0';
	char szString[256];
	if ( m_drpResolution->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
	{
		m_drpResolution->SetCurrentSelection( szString );
	}

	m_iResolutionWidth = m_nResolutionModes[ selectedItemID ].m_nWidth;
	m_iResolutionHeight = m_nResolutionModes[ selectedItemID ].m_nHeight;
}

void Video::ApplyChanges()
{
	if ( m_bDirtyValues || 
		( m_sldBrightness && m_sldBrightness->IsDirty() ) )
	{
		// Make sure there is a genuine state change required
		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
		if ( config.m_VideoMode.m_Width != m_iResolutionWidth || 
			 config.m_VideoMode.m_Height != m_iResolutionHeight || 
#if !defined( POSIX )
			 config.NoWindowBorder() != m_bNoBorder ||
#endif
			 config.Windowed() != m_bWindowed )
		{
			// set mode
			char szCmd[ 256 ];
			V_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %i %i %i %i\n", m_iResolutionWidth, m_iResolutionHeight, m_bWindowed ? 1 : 0, m_bNoBorder ? 1 : 0 );
			engine->ClientCmd_Unrestricted( szCmd );
		}
	
		CGameUIConVarRef mat_powersavingsmode( "mat_powersavingsmode" );
		mat_powersavingsmode.SetValue( m_nPowerSavingsMode );

		// save changes
		engine->ClientCmd_Unrestricted( "mat_savechanges\n" );
		engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );

		// Update the current video config file.
#if !defined( _GAMECONSOLE )
		int nAspectRatioMode = GetScreenAspectMode( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );
		UpdateCurrentVideoConfig( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height, nAspectRatioMode, !config.Windowed(), config.NoWindowBorder() );
#endif

		m_bDirtyValues = false;
	}
}

void Video::OnThink()
{
	BaseClass::OnThink();

	if ( m_bEnableApply != m_bDirtyValues )
	{
		// enable the apply button
		m_bEnableApply = m_bDirtyValues;
		UpdateFooter();
	}
}

void Video::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON | FB_XBUTTON;
		if ( m_bEnableApply )
		{
			visibleButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#GameUI_Apply" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
		pFooter->SetButtonText( FB_XBUTTON, "#GameUI_UseDefaults" );
	}
}

void AcceptPowerSavingsWarningCallback()
{
	Video *pSelf = static_cast< Video* >( CBaseModPanel::GetSingleton().GetWindow( WT_VIDEO ) );
	if ( pSelf )
	{
		pSelf->AcceptPowerSavingsWarningCallback();
	}
}

void Video::ShowPowerSavingsWarning()
{
	GenericConfirmation* pConfirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#GameUI_PowerSavingsMode";
	data.pMessageText = "#PORTAL2_VideoOptions_PowerSavings_Info";

	data.bOkButtonEnabled = true;
	data.pfnOkCallback = ::AcceptPowerSavingsWarningCallback;

	pConfirmation->SetUsageData( data );
}

void Video::AcceptPowerSavingsWarningCallback()
{
	m_bAcceptPowerSavingsWarning = true;
}

void Video::SetPowerSavingsState()
{
	if ( m_drpPowerSavingsMode )
	{
		m_drpPowerSavingsMode->SetCurrentSelection( m_nPowerSavingsMode != 0 ? "#L4D360UI_Enabled" : "#L4D360UI_Disabled" );	
	}
}
//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "vadvancedvideo.h"
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

CAdvancedVideo::CAdvancedVideo(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#GameUI_VideoAdvanced_Title" );

	m_drpModelDetail = NULL;
#ifndef POSIX	
	m_drpPagedPoolMem = NULL;
#endif
	m_drpQueuedMode = NULL;
	m_drpAntialias = NULL;
	m_drpFiltering = NULL;
	m_drpVSync = NULL;
	m_drpShaderDetail = NULL;
	m_drpCPUDetail = NULL;

	m_bDirtyValues = false;
	m_bEnableApply = false;

	// reset all warnings
	m_VideoWarning = VW_NONE;
	for ( int i = 0; i < VW_MAXWARNINGS; i++ )
	{
		m_bAcceptWarning[i] = false;
	}

	m_nNumAAModes = 0;

	m_iModelTextureDetail = 0;
	m_iPagedPoolMem = 0;
	m_nAASamples = 0;
	m_nAAQuality = 0;
	m_iFiltering = 1;
	m_bVSync = false;
	m_bTripleBuffered = false;
	m_iGPUDetail = 0;
	m_iCPUDetail = 0;
	m_iQueuedMode = -1;

	SetFooterEnabled( true );
	UpdateFooter();
}

CAdvancedVideo::~CAdvancedVideo()
{
}

void CAdvancedVideo::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_drpModelDetail = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpModelDetail" ) );
#ifndef POSIX
	m_drpPagedPoolMem = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpPagedPoolMem" ) );
#endif
	m_drpAntialias = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpAntialias" ) );
	m_drpFiltering = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpFiltering" ) );
	m_drpVSync = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpVSync" ) );
	m_drpQueuedMode = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpQueuedMode" ) );
	m_drpShaderDetail = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpShaderDetail" ) );
	m_drpCPUDetail = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpCPUDetail" ) );

	SetupState( false );

	if ( m_drpModelDetail )
	{
		if ( m_ActiveControl )
			m_ActiveControl->NavigateFrom();
		m_drpModelDetail->NavigateTo();
		m_ActiveControl = m_drpModelDetail;
	}

	UpdateFooter();
}

void AcceptWarningCallback()
{
	CAdvancedVideo *pSelf = static_cast< CAdvancedVideo* >( CBaseModPanel::GetSingleton().GetWindow( WT_ADVANCEDVIDEO ) );
	if ( pSelf )
	{
		pSelf->AcceptWarningCallback();
	}
}

void CAdvancedVideo::ShowWarning( VideoWarning_e videoWarning )
{
	GenericConfirmation* pConfirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this ) );

	GenericConfirmation::Data_t data;

	switch ( videoWarning )
	{
	case VW_ANTIALIASING:
		data.pWindowTitle = "#L4D360UI_VideoOptions_Antialiasing";
		data.pMessageText = "#PORTAL2_VideoOptions_Antialiasing_Info";
		break;

	case VW_FILTERING:
		data.pWindowTitle = "#GameUI_Filtering_Mode";
		data.pMessageText = "#PORTAL2_VideoOptions_Filtering_Info";
		break;

	case VW_VSYNC:
		data.pWindowTitle = "#GameUI_Wait_For_VSync";
		data.pMessageText = "#PORTAL2_VideoOptions_WaitForVSync_Info";
		break;

	case VW_MULTICORE:
		data.pWindowTitle = "#L4D360UI_VideoOptions_Queued_Mode";
		data.pMessageText = "#PORTAL2_VideoOptions_QueuedMode_Info";
		break;

	case VW_SHADERDETAIL:
		data.pWindowTitle = "#GameUI_Shader_Detail";
		data.pMessageText = "#PORTAL2_VideoOptions_ShaderDetail_Info";
		break;

	case VW_CPUDETAIL:
		data.pWindowTitle = "#L4D360UI_VideoOptions_CPU_Detail";
		data.pMessageText = "#PORTAL2_VideoOptions_CPUDetail_Info";
		break;

	case VW_MODELDETAIL:
		data.pWindowTitle = "#L4D360UI_VideoOptions_Model_Texture_Detail";
		data.pMessageText = "#PORTAL2_VideoOptions_ModelDetail_Info";
		break;

	case VW_PAGEDPOOL:
		data.pWindowTitle = "#L4D360UI_VideoOptions_Paged_Pool_Mem";
		data.pMessageText = "#L4D360UI_VideoOptions_Paged_Pool_Mem_Info";
		break;

	default:
		return;
	}

	data.bOkButtonEnabled = true;
	data.pfnOkCallback = ::AcceptWarningCallback;

	m_VideoWarning = videoWarning;
	pConfirmation->SetUsageData( data );
}

void CAdvancedVideo::AcceptWarningCallback()
{
	m_bAcceptWarning[m_VideoWarning] = true;
	m_VideoWarning = VW_NONE;
}

void CAdvancedVideo::GetCurrentSettings( void )
{
	CGameUIConVarRef gpu_mem_level( "gpu_mem_level" );
	m_iModelTextureDetail = clamp( gpu_mem_level.GetInt(), 0, 2);

	CGameUIConVarRef mem_level( "mem_level" );
	m_iPagedPoolMem = clamp( mem_level.GetInt(), 0, 2);

	CGameUIConVarRef mat_antialias( "mat_antialias" );
	CGameUIConVarRef mat_aaquality( "mat_aaquality" );
	m_nAASamples = mat_antialias.GetInt();
	m_nAAQuality = mat_aaquality.GetInt();

	CGameUIConVarRef mat_forceaniso( "mat_forceaniso" );
	m_iFiltering = mat_forceaniso.GetInt();

	CGameUIConVarRef mat_vsync( "mat_vsync" );
	m_bVSync = mat_vsync.GetBool();

	CGameUIConVarRef mat_triplebuffered( "mat_triplebuffered" );
	m_bTripleBuffered = mat_triplebuffered.GetBool();

	CGameUIConVarRef mat_queue_mode( "mat_queue_mode" );
	m_iQueuedMode = mat_queue_mode.GetInt();

	CGameUIConVarRef gpu_level( "gpu_level" );
	m_iGPUDetail = clamp( gpu_level.GetInt(), 0, 3 );

	CGameUIConVarRef cpu_level( "cpu_level" );
	m_iCPUDetail = clamp( cpu_level.GetInt(), 0, 2 );
}

bool CAdvancedVideo::GetRecommendedSettings( void )
{
#if !defined( _GAMECONSOLE )
	KeyValues *pConfigKeys = new KeyValues( "VideoConfig" );
	if ( !pConfigKeys )
		return false;

	if ( !ReadCurrentVideoConfig( pConfigKeys, true ) )
	{
		pConfigKeys->deleteThis();
		return false;
	}

	m_iModelTextureDetail = clamp( pConfigKeys->GetInt( "setting.gpu_mem_level", 0 ), 0, 2 );
	m_iPagedPoolMem = clamp( pConfigKeys->GetInt( "setting.mem_level", 0 ), 0, 2 );
	m_nAASamples = pConfigKeys->GetInt( "setting.mat_antialias", 0 );
	m_nAAQuality = pConfigKeys->GetInt( "setting.mat_aaquality", 0 );
	m_iFiltering = pConfigKeys->GetInt( "setting.mat_forceaniso", 1 );
	m_bVSync = pConfigKeys->GetBool( "setting.mat_vsync", true );
	m_bTripleBuffered = pConfigKeys->GetBool( "setting.mat_triplebuffered", false );
	m_iGPUDetail = pConfigKeys->GetInt( "setting.gpu_level", 0 );
	m_iCPUDetail = pConfigKeys->GetInt( "setting.cpu_level", 0 );
	m_iQueuedMode = pConfigKeys->GetInt( "setting.mat_queue_mode", -1 );

	pConfigKeys->deleteThis();

	m_bDirtyValues = true;
#endif

	return true;
}

void CAdvancedVideo::ProcessAAList()
{
	if ( !m_drpAntialias )
		return;

	// We start with no entries
	m_nNumAAModes = 0;

	char szCurrentButton[ 32 ];
	V_strncpy( szCurrentButton, VIDEO_ANTIALIAS_COMMAND_PREFIX, sizeof( szCurrentButton ) );

	int iCommandNumberPosition = Q_strlen( szCurrentButton );
	szCurrentButton[ iCommandNumberPosition + 1 ] = '\0';

	// Always have the possibility of no AA
	Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
	szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
	m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_None" );

	m_nAAModes[m_nNumAAModes].m_nNumSamples = 1;
	m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
	m_nNumAAModes++;

	// Add other supported AA settings
	if ( materials->SupportsMSAAMode( 2 ) )
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_2X" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 2;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
		m_nNumAAModes++;
	}

	if ( materials->SupportsMSAAMode( 4 ) )
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_4X" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 4;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
		m_nNumAAModes++;
	}

	if ( materials->SupportsMSAAMode( 6 ) )
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_6X" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 6;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
		m_nNumAAModes++;
	}

	// nVidia CSAA "8x"
	if ( materials->SupportsCSAAMode( 4, 2 ) )							
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_8X_CSAA" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 4;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 2;
		m_nNumAAModes++;
	}

	// nVidia CSAA "16x"
	if ( materials->SupportsCSAAMode( 4, 4 ) )							
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_16X_CSAA" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 4;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 4;
		m_nNumAAModes++;
	}

	if ( materials->SupportsMSAAMode( 8 ) )
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_8X" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 8;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
		m_nNumAAModes++;
	}

	// nVidia CSAA "16xQ"
	if ( materials->SupportsCSAAMode( 8, 2 ) )							
	{
		Assert( m_nNumAAModes < MAX_DYNAMIC_AA_MODES );
		szCurrentButton[ iCommandNumberPosition ] = m_nNumAAModes + '0';
		m_drpAntialias->ModifySelectionString( szCurrentButton, "#GameUI_16XQ_CSAA" );

		m_nAAModes[m_nNumAAModes].m_nNumSamples = 8;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 2;
		m_nNumAAModes++;
	}

	// Enable the valid possible choices
	for ( int i = 0; i < m_nNumAAModes; ++i )
	{
		szCurrentButton[ iCommandNumberPosition ] = i + '0';
		char szString[256];
		if ( m_drpAntialias->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
		{
			m_drpAntialias->EnableListItem( szString, true );
		}
	}
	
	// Disable the remaining possible choices
	for ( int i = m_nNumAAModes; i < MAX_DYNAMIC_AA_MODES; ++i )
	{
		szCurrentButton[ iCommandNumberPosition ] = i + '0';
		char szString[256];
		if ( m_drpAntialias->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
		{
			m_drpAntialias->EnableListItem( szString, false );
		}
	}

	// Select the currently set type
	m_iAntiAlias = FindMSAAMode( m_nAASamples, m_nAAQuality );
}

void CAdvancedVideo::SetAntiAliasingState()
{
	char szCurrentButton[ 32 ];
	V_strncpy( szCurrentButton, VIDEO_ANTIALIAS_COMMAND_PREFIX, sizeof( szCurrentButton ) );

	int iCommandNumberPosition = Q_strlen( szCurrentButton );
	szCurrentButton[ iCommandNumberPosition + 1 ] = '\0';

	szCurrentButton[ iCommandNumberPosition ] = m_iAntiAlias + '0';
	char szString[256];
	if ( m_drpAntialias->GetListSelectionString( szCurrentButton, szString, sizeof( szString ) ) )
	{
		m_drpAntialias->SetCurrentSelection( szString );
	}
}

void CAdvancedVideo::SetFilteringState()
{
	if ( m_drpFiltering )
	{
		switch ( m_iFiltering )
		{
		case 0:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Bilinear" );
			break;
		case 1:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Trilinear" );
			break;
		case 2:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Anisotropic2X" );
			break;
		case 4:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Anisotropic4X" );
			break;
		case 8:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Anisotropic8X" );
			break;
		case 16:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Anisotropic16X" );
			break;
		default:
			m_drpFiltering->SetCurrentSelection( "#GameUI_Trilinear" );
			m_iFiltering = 1;
			break;
		}
	}
}

void CAdvancedVideo::SetVSyncState()
{
	if ( m_drpVSync )
	{
		if ( m_bVSync )
		{
			if ( m_bTripleBuffered )
			{
				m_drpVSync->SetCurrentSelection( "#L4D360UI_VideoOptions_VSync_TripleBuffered" );
			}
			else
			{
				m_drpVSync->SetCurrentSelection( "#L4D360UI_VideoOptions_VSync_DoubleBuffered" );
			}
		}
		else
		{
			m_drpVSync->SetCurrentSelection( "#L4D360UI_Disabled" );
		}
	}
}

void CAdvancedVideo::SetQueuedModeState()
{
	if ( m_drpQueuedMode )
	{
		// Only allow the options on multi-processor machines.
		if ( GetCPUInformation().m_nPhysicalProcessors >= 2 )
		{
			if ( m_iQueuedMode != 0 )
			{
				m_drpQueuedMode->SetCurrentSelection( "#L4D360UI_Enabled" );
			}
			else
			{
				m_drpQueuedMode->SetCurrentSelection( "#L4D360UI_Disabled" );
			}
		}
		else
		{
			m_drpQueuedMode->SetEnabled( false );
		}
	}
}

void CAdvancedVideo::SetShaderDetailState()
{
	if ( m_drpShaderDetail )
	{
		switch ( m_iGPUDetail )
		{
		case 0:
			m_drpShaderDetail->SetCurrentSelection( "#GameUI_Low" );
			break;
		case 1:
			m_drpShaderDetail->SetCurrentSelection( "#GameUI_Medium" );
			break;
		case 2:
			m_drpShaderDetail->SetCurrentSelection( "#GameUI_High" );
			break;
		case 3:
			m_drpShaderDetail->SetCurrentSelection( "#GameUI_Ultra" );
			break;
		}
	}
}

void CAdvancedVideo::SetCPUDetailState()
{
	if ( m_drpCPUDetail )
	{
		switch ( m_iCPUDetail )
		{
		case 0:
			m_drpCPUDetail->SetCurrentSelection( "#GameUI_Low" );
			break;
		case 1:
			m_drpCPUDetail->SetCurrentSelection( "#GameUI_Medium" );
			break;
		case 2:
			m_drpCPUDetail->SetCurrentSelection( "#GameUI_High" );
			break;
		}
	}
}

void CAdvancedVideo::SetModelDetailState()
{
	if ( m_drpModelDetail )
	{
		switch ( m_iModelTextureDetail )
		{
		case 0:
			m_drpModelDetail->SetCurrentSelection( "#GameUI_Low" );
			break;
		case 1:
			m_drpModelDetail->SetCurrentSelection( "#GameUI_Medium" );
			break;
		case 2:
			m_drpModelDetail->SetCurrentSelection( "#GameUI_High" );
			break;
		}
	}
}

void CAdvancedVideo::SetPagedPoolState()
{
#ifndef POSIX
	if ( m_drpPagedPoolMem )
	{
		switch ( m_iPagedPoolMem )
		{
		case 0:
			m_drpPagedPoolMem->SetCurrentSelection( "#GameUI_Low" );
			break;
		case 1:
			m_drpPagedPoolMem->SetCurrentSelection( "#GameUI_Medium" );
			break;
		case 2:
			m_drpPagedPoolMem->SetCurrentSelection( "#GameUI_High" );
			break;
		}
	}
#endif
}

static void AcceptDefaultsOkCallback()
{
	CAdvancedVideo *pSelf = 
		static_cast< CAdvancedVideo* >( CBaseModPanel::GetSingleton().GetWindow( WT_ADVANCEDVIDEO ) );
	if ( pSelf )
	{
		pSelf->SetDefaults();
	}
}

static void DiscardChangesOkCallback()
{
	CAdvancedVideo *pSelf = 
		static_cast< CAdvancedVideo* >( CBaseModPanel::GetSingleton().GetWindow( WT_ADVANCEDVIDEO ) );
	if ( pSelf )
	{
		pSelf->DiscardChangesAndClose();
	}
}

void CAdvancedVideo::DiscardChangesAndClose()
{
	BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
}

void CAdvancedVideo::SetDefaults()
{
	SetupState( true );
}

void CAdvancedVideo::SetupState( bool bRecommendedSettings )
{
	if ( !bRecommendedSettings )
	{
		GetCurrentSettings();
	}
	else if ( !GetRecommendedSettings() )
	{
		return;
	}

	ProcessAAList();

	SetAntiAliasingState();
	SetFilteringState();
	SetVSyncState();
	SetQueuedModeState();
	SetShaderDetailState();
	SetCPUDetailState();
	SetModelDetailState();
	SetPagedPoolState();

	UpdateFooter();
}

void CAdvancedVideo::Activate()
{
	BaseClass::Activate();

	UpdateFooter();
}

void CAdvancedVideo::OnKeyCodePressed(KeyCode code)
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
		if ( m_bDirtyValues )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation *pConfirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_AdvancedVideoConf";
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
			data.pWindowTitle = "#PORTAL2_AdvancedVideoConf";
			data.pMessageText = "#PORTAL2_VideoSettingsUseDefaultsQ";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptDefaultsOkCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Reset";
			data.bCancelButtonEnabled = true;
			pConfirmation->SetUsageData( data );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

void CAdvancedVideo::OnCommand(const char *command)
{
	if ( !V_stricmp( command, "ModelDetailHigh" ) )
	{
		if ( !m_bAcceptWarning[VW_MODELDETAIL] )
		{
			ShowWarning( VW_MODELDETAIL );
			SetModelDetailState();
		}
		else
		{
			m_iModelTextureDetail = 2;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "ModelDetailMedium" ) )
	{
		if ( !m_bAcceptWarning[VW_MODELDETAIL] )
		{
			ShowWarning( VW_MODELDETAIL );
			SetModelDetailState();
		}
		else
		{
			m_iModelTextureDetail = 1;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "ModelDetailLow" ) )
	{
		if ( !m_bAcceptWarning[VW_MODELDETAIL] )
		{
			ShowWarning( VW_MODELDETAIL );
			SetModelDetailState();
		}
		else
		{
			m_iModelTextureDetail = 0;
			m_bDirtyValues = true;
		}
	}
#ifndef POSIX
	else if ( !V_stricmp( command, "PagedPoolMemHigh" ) )
	{
		if ( !m_bAcceptWarning[VW_PAGEDPOOL] )
		{
			// show the warning first, and restore the current state
			ShowWarning( VW_PAGEDPOOL );
			SetPagedPoolState();
		}
		else
		{
			m_iPagedPoolMem = 2;
			m_bDirtyValues = true;
		}
		
	}
	else if ( !V_stricmp( command, "PagedPoolMemMedium" ) )
	{
		if ( !m_bAcceptWarning[VW_PAGEDPOOL] )
		{
			// show the warning first, and restore the current state
			ShowWarning( VW_PAGEDPOOL );
			SetPagedPoolState();
		}
		else
		{
			m_iPagedPoolMem = 1;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "PagedPoolMemLow" ) )
	{
		if ( !m_bAcceptWarning[VW_PAGEDPOOL] )
		{
			// show the warning first, and restore the current state
			ShowWarning( VW_PAGEDPOOL );
			SetPagedPoolState();
		}
		else
		{
			m_iPagedPoolMem = 0;
			m_bDirtyValues = true;
		}
	}
#endif
	else if ( StringHasPrefix( command, VIDEO_ANTIALIAS_COMMAND_PREFIX ) )
	{
		if ( !m_bAcceptWarning[VW_ANTIALIASING] )
		{
			ShowWarning( VW_ANTIALIASING );
			SetAntiAliasingState();
		}
		else
		{
			int iCommandNumberPosition = Q_strlen( VIDEO_ANTIALIAS_COMMAND_PREFIX );
			m_iAntiAlias = clamp( command[ iCommandNumberPosition ] - '0', 0, m_nNumAAModes - 1 );
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "#GameUI_Bilinear" ) )
	{
		if ( !m_bAcceptWarning[VW_FILTERING] )
		{
			ShowWarning( VW_FILTERING );
			SetFilteringState();
		}
		else
		{
			m_iFiltering = 0;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "#GameUI_Trilinear" ) )
	{
		if ( !m_bAcceptWarning[VW_FILTERING] )
		{
			ShowWarning( VW_FILTERING );
			SetFilteringState();
		}
		else
		{
			m_iFiltering = 1;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "#GameUI_Anisotropic2X" ) )
	{
		if ( !m_bAcceptWarning[VW_FILTERING] )
		{
			ShowWarning( VW_FILTERING );
			SetFilteringState();
		}
		else
		{
			m_iFiltering = 2;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "#GameUI_Anisotropic4X" ) )
	{
		if ( !m_bAcceptWarning[VW_FILTERING] )
		{
			ShowWarning( VW_FILTERING );
			SetFilteringState();
		}
		else
		{
			m_iFiltering = 4;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "#GameUI_Anisotropic8X" ) )
	{
		if ( !m_bAcceptWarning[VW_FILTERING] )
		{
			ShowWarning( VW_FILTERING );
			SetFilteringState();
		}
		else
		{
			m_iFiltering = 8;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "#GameUI_Anisotropic16X" ) )
	{
		if ( !m_bAcceptWarning[VW_FILTERING] )
		{
			ShowWarning( VW_FILTERING );
			SetFilteringState();
		}
		else
		{
			m_iFiltering = 16;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "VSyncTripleBuffered" ) )
	{
		if ( !m_bAcceptWarning[VW_VSYNC] )
		{
			ShowWarning( VW_VSYNC );
			SetVSyncState();
		}
		else
		{
			m_bVSync = true;
			m_bTripleBuffered = true;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "VSyncEnabled" ) )
	{
		if ( !m_bAcceptWarning[VW_VSYNC] )
		{
			ShowWarning( VW_VSYNC );
			SetVSyncState();
		}
		else
		{
			m_bVSync = true;
			m_bTripleBuffered = false;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "VSyncDisabled" ) )
	{
		if ( !m_bAcceptWarning[VW_VSYNC] )
		{
			ShowWarning( VW_VSYNC );
			SetVSyncState();
		}
		else
		{
			m_bVSync = false;
			m_bTripleBuffered = false;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "QueuedModeEnabled" ) )
	{
		if ( !m_bAcceptWarning[VW_MULTICORE] )
		{
			ShowWarning( VW_MULTICORE );
			SetQueuedModeState();
		}
		else
		{
			m_iQueuedMode = -1;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "QueuedModeDisabled" ) )
	{
		if ( !m_bAcceptWarning[VW_MULTICORE] )
		{
			ShowWarning( VW_MULTICORE );
			SetQueuedModeState();
		}
		else
		{
			m_iQueuedMode = 0;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "ShaderDetailVeryHigh" ) )
	{
		if ( !m_bAcceptWarning[VW_SHADERDETAIL] )
		{
			ShowWarning( VW_SHADERDETAIL );
			SetShaderDetailState();
		}
		else
		{
			m_iGPUDetail = 3;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "ShaderDetailHigh" ) )
	{
		if ( !m_bAcceptWarning[VW_SHADERDETAIL] )
		{
			ShowWarning( VW_SHADERDETAIL );
			SetShaderDetailState();
		}
		else
		{
			m_iGPUDetail = 2;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "ShaderDetailMedium" ) )
	{
		if ( !m_bAcceptWarning[VW_SHADERDETAIL] )
		{
			ShowWarning( VW_SHADERDETAIL );
			SetShaderDetailState();
		}
		else
		{
			m_iGPUDetail = 1;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "ShaderDetailLow" ) )
	{
		if ( !m_bAcceptWarning[VW_SHADERDETAIL] )
		{
			ShowWarning( VW_SHADERDETAIL );
			SetShaderDetailState();
		}
		else
		{
			m_iGPUDetail = 0;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "CPUDetailHigh" ) )
	{
		if ( !m_bAcceptWarning[VW_CPUDETAIL] )
		{
			ShowWarning( VW_CPUDETAIL );
			SetCPUDetailState();
		}
		else
		{
			m_iCPUDetail = 2;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "CPUDetailMedium" ) )
	{
		if ( !m_bAcceptWarning[VW_CPUDETAIL] )
		{
			ShowWarning( VW_CPUDETAIL );
			SetCPUDetailState();
		}
		else
		{
			m_iCPUDetail = 1;
			m_bDirtyValues = true;
		}
	}
	else if ( !V_stricmp( command, "CPUDetailLow" ) )
	{
		if ( !m_bAcceptWarning[VW_CPUDETAIL] )
		{
			ShowWarning( VW_CPUDETAIL );
			SetCPUDetailState();
		}
		else
		{
			m_iCPUDetail = 0;
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

int CAdvancedVideo::FindMSAAMode( int nAASamples, int nAAQuality )
{
	// Run through the AA Modes supported by the device
	for ( int nAAMode = 0; nAAMode < m_nNumAAModes; nAAMode++ )
	{
		// If we found the mode that matches what we're looking for, return the index
		if ( ( m_nAAModes[nAAMode].m_nNumSamples == nAASamples) && ( m_nAAModes[nAAMode].m_nQualityLevel == nAAQuality) )
		{
			return nAAMode;
		}
	}

	return 0;	// Didn't find what we're looking for, so no AA
}

void CAdvancedVideo::ApplyChanges()
{
	if ( !m_bDirtyValues )
	{
		// No need to apply settings
		return;
	}

	CGameUIConVarRef gpu_mem_level( "gpu_mem_level" );
	gpu_mem_level.SetValue( m_iModelTextureDetail );

	CGameUIConVarRef mem_level( "mem_level" );
	mem_level.SetValue( m_iPagedPoolMem );

	CGameUIConVarRef mat_antialias( "mat_antialias" );
	CGameUIConVarRef mat_aaquality( "mat_aaquality" );
	mat_antialias.SetValue( m_nAAModes[ m_iAntiAlias ].m_nNumSamples );
	mat_aaquality.SetValue( m_nAAModes[ m_iAntiAlias ].m_nQualityLevel );

	CGameUIConVarRef mat_forceaniso( "mat_forceaniso" );
	mat_forceaniso.SetValue( m_iFiltering );

	CGameUIConVarRef mat_vsync( "mat_vsync" );
	mat_vsync.SetValue( m_bVSync );

	CGameUIConVarRef mat_triplebuffered( "mat_triplebuffered" );
	mat_triplebuffered.SetValue( m_bTripleBuffered );

	CGameUIConVarRef mat_queue_mode( "mat_queue_mode" );
	mat_queue_mode.SetValue( m_iQueuedMode );

	CGameUIConVarRef cpu_level( "cpu_level" );
	cpu_level.SetValue( m_iCPUDetail );

	CGameUIConVarRef gpu_level( "gpu_level" );
	gpu_level.SetValue( m_iGPUDetail );

	// apply changes
	engine->ClientCmd_Unrestricted( "mat_savechanges\n" );
	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );

	m_bDirtyValues = false;
}

void CAdvancedVideo::OnThink()
{
	BaseClass::OnThink();

	if ( m_bEnableApply != m_bDirtyValues )
	{
		// enable the apply button
		m_bEnableApply = m_bDirtyValues;
		UpdateFooter();
	}
}

void CAdvancedVideo::UpdateFooter()
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
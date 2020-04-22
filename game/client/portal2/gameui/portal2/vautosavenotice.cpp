//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vautosavenotice.h"
#include "vfooterpanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/ImagePanel.h"
#include "inputsystem/iinputsystem.h"
#include "filesystem.h"
#include "filesystem/IXboxInstaller.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

enum InstallStatus_e
{
	INSTALL_STATUS_DISABLED = 0,
	INSTALL_STATUS_INPROGRESS = 1,
	INSTALL_STATUS_INSTALLED = 2
};

ConVar ui_autosavenotice_timeout( "ui_autosavenotice_timeout", "10.0", FCVAR_DEVELOPMENTONLY, "" );

CAutoSaveNotice::CAutoSaveNotice( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetFooterEnabled( true );

	AddFrameListener( this );

	SetDialogTitle( "#PORTAL2_AutoSave" );

	m_pAutoSaveIcon = NULL;
	m_pStatusLabel = NULL;

	m_flAutoContinueTimeout = 0;

	m_flLastEngineTime = 0;
	m_nCurrentSpinnerValue = 0;

	m_nRevolutions = 0;
	m_nInstallStatus = INSTALL_STATUS_DISABLED;

	UpdateFooter();
}

CAutoSaveNotice::~CAutoSaveNotice()
{
	RemoveFrameListener( this );
}

void CAutoSaveNotice::SetDataSettings( KeyValues *pSettings )
{
	m_MapName = pSettings->GetString( "map" );
	m_LoadFilename = pSettings->GetString( "loadfilename" );
	m_ReasonString = pSettings->GetString( "reason" );
}

void CAutoSaveNotice::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void CAutoSaveNotice::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pAutoSaveIcon = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "AutoSaveIcon" ) );
	m_pStatusLabel = dynamic_cast< vgui::Label* >( FindChildByName( "LblStatus" ) );

	// start the auto timeout
	m_flAutoContinueTimeout = Plat_FloatTime();
}

void CAutoSaveNotice::OnKeyCodePressed( KeyCode code )
{
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		{
			m_flAutoContinueTimeout = 0;

			KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetString( "map", m_MapName.Get() );
			pSettings->SetString( "loadfilename", m_LoadFilename.Get() );
			pSettings->SetString( "reason", m_ReasonString.Get() );
			CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
			return;
		}
		break;

	case KEY_XBUTTON_B:
		// fall through
		m_flAutoContinueTimeout = 0;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void CAutoSaveNotice::RunFrame()
{
	if ( m_flAutoContinueTimeout &&
		CBaseModPanel::GetSingleton().GetActiveWindowType() != WT_AUTOSAVENOTICE || 
		CUIGameData::Get()->IsXUIOpen() )
	{
		// restart the timeout
		m_flAutoContinueTimeout = Plat_FloatTime();
	}
}

void CAutoSaveNotice::OnThink()
{
	BaseClass::OnThink();

#if defined( _X360 )
	float flPercent = 0;
	if ( !g_pXboxInstaller->IsInstallEnabled() )
	{
		m_nInstallStatus = INSTALL_STATUS_DISABLED;
	}
	else
	{
		if ( !g_pXboxInstaller->IsFullyInstalled() )
		{
			// in progress
			m_nInstallStatus = INSTALL_STATUS_INPROGRESS;
		}
		else
		{
			// completed
			m_nInstallStatus = INSTALL_STATUS_INSTALLED;
		}

		const CopyStats_t *pCopyStats = g_pXboxInstaller->GetCopyStats();	
		if ( g_pXboxInstaller->GetTotalSize() )
		{
			flPercent = (float)pCopyStats->m_BytesCopied/(float)g_pXboxInstaller->GetTotalSize() * 100.0f;
		}
	}

	if ( m_pStatusLabel && m_nInstallStatus != INSTALL_STATUS_DISABLED )
	{
		bool bVisible = g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_LEFT_SHOULDER, 0 ) ) &&
							g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_RIGHT_SHOULDER, 0 ) );
				
		m_pStatusLabel->SetVisible( bVisible );
		if ( bVisible )
		{
			// update status
			m_pStatusLabel->SetText( CFmtStr( "%d%%", (int)flPercent ) );
		}
	}
#endif

	// hold off the auto timeout while the install is busy or we are occluded
	if ( m_nInstallStatus != INSTALL_STATUS_INPROGRESS && 
		CBaseModPanel::GetSingleton().GetActiveWindowType() == WT_AUTOSAVENOTICE &&
		!CUIGameData::Get()->IsXUIOpen() )
	{
		if ( m_flAutoContinueTimeout && Plat_FloatTime() >= m_flAutoContinueTimeout + ui_autosavenotice_timeout.GetFloat() )
		{
			// mimic the user pressing A, once
			m_flAutoContinueTimeout = 0;
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
	}

	ClockAnim();
}

void CAutoSaveNotice::ClockAnim()
{
	// clock the anim at 10hz
	float time = Plat_FloatTime();
	if ( ( m_flLastEngineTime + 0.1f ) < time )
	{
		m_flLastEngineTime = time;
		if ( m_pAutoSaveIcon )
		{
			m_pAutoSaveIcon->SetFrame( m_nCurrentSpinnerValue++ );

			if ( !( m_nCurrentSpinnerValue % m_pAutoSaveIcon->GetNumFrames() ) )
			{
				// track revolutions
				// the install status code 0..N, cause the spinner to do N revolutions, then pause
				m_nRevolutions++;
				if ( m_nInstallStatus && !( m_nRevolutions % m_nInstallStatus ) )
				{
					m_flLastEngineTime += 0.5f;
				}
			}
		}
	}
}

void CAutoSaveNotice::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON );

		pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_StartGame" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

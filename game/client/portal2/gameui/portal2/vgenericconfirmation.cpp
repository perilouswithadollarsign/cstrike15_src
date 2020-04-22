//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGenericConfirmation.h"
#include "VFooterPanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/imagepanel.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

int GenericConfirmation::sm_currentUsageId = 0;

GenericConfirmation::Data_t::Data_t() : 
	pWindowTitle( NULL ),
	pMessageText( NULL ),
	pMessageTextW( NULL ),
	bOkButtonEnabled( false ),
	pfnOkCallback( NULL ),
	pOkButtonText( NULL ),
	bCancelButtonEnabled( false ),
	pfnCancelCallback( NULL ),
	pCancelButtonText( NULL ),
	bCheckBoxEnabled( false ),
	pCheckBoxLabelText( NULL ),
	pCheckBoxCvarName( NULL )
{
}

GenericConfirmation::GenericConfirmation( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false ),
	m_pLblMessage( 0 ),
	m_data(),
	m_usageId( 0 )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetTitle( "", false );
	SetMoveable( false );

	m_hMessageFont = vgui::INVALID_FONT;

	m_nTextOffsetX = 0;
	m_nIconOffsetY = 0;

	SetFooterEnabled( true );

	m_bValid = false;
}

GenericConfirmation::~GenericConfirmation()
{
	delete m_pLblMessage;
}

void GenericConfirmation::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hMessageFont = pScheme->GetFont( pScheme->GetResourceString( "ConfirmationDialog.TextFont" ), true );

	m_nTextOffsetX = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "ConfirmationDialog.TextOffsetX" ) ) );
	m_nIconOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "ConfirmationDialog.IconOffsetY" ) ) );

	m_bValid = true;

	FixLayout();
	UpdateFooter();
}

void GenericConfirmation::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "OK" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( Q_stricmp( command, "cancel" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
}

void GenericConfirmation::OnKeyCodePressed( KeyCode keycode )
{
	int userId = GetJoystickForCode( keycode );
	vgui::KeyCode code = GetBaseButtonCode( keycode );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	switch ( code )
	{
	case KEY_XBUTTON_A:
		if ( m_OkButtonEnabled )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
			if ( !NavigateBack() )
			{
				Close();
			}

			if ( m_data.pfnOkCallback != 0 )
			{
				m_data.pfnOkCallback();
			}
		}
		break;

	case KEY_XBUTTON_B:
		if ( m_CancelButtonEnabled )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			if ( !NavigateBack() )
			{
				Close();
			}

			if ( m_data.pfnCancelCallback != 0 )
			{
				m_data.pfnCancelCallback();
			}
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(keycode);
		break;
	}
}

#ifndef _GAMECONSOLE
void GenericConfirmation::OnKeyCodeTyped( vgui::KeyCode code )
{
	// For PC, this maps space bar and enter to OK and esc to cancel
	switch ( code )
	{
	case KEY_SPACE:
	case KEY_ENTER:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );

	case KEY_ESCAPE:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}

	BaseClass::OnKeyTyped( code );
}
#endif

void GenericConfirmation::OnOpen()
{
	BaseClass::OnOpen();

	UpdateFooter();

	m_bNeedsMoveToFront = true;
}

void GenericConfirmation::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int buttons = 0;
		if ( m_data.bOkButtonEnabled )
		{
			buttons |= FB_ABUTTON;
		}
		if ( m_data.bCancelButtonEnabled )
		{
			buttons |= FB_BBUTTON;
		}

		pFooter->SetButtons( buttons, FF_ABXYDL_ORDER, FOOTER_GENERICCONFIRMATION );
		pFooter->SetButtonText( FB_ABUTTON, m_data.pOkButtonText ? m_data.pOkButtonText : "#L4D360UI_Ok", false, FOOTER_GENERICCONFIRMATION );
		pFooter->SetButtonText( FB_BBUTTON, m_data.pCancelButtonText ? m_data.pCancelButtonText : "#L4D360UI_Cancel", false, FOOTER_GENERICCONFIRMATION );
	}
}

void GenericConfirmation::FixLayout()
{
	if ( !m_bValid )
	{
		// want to delay this until ApplySchemeSettings() gets called
		return;
	}

	if ( m_pLblMessage )
	{
		m_pLblMessage->SetFont( m_hMessageFont );

		if ( UsesAlternateTiles() )
		{
			m_pLblMessage->SetFgColor( Color( 201, 211, 207, 255 ) );
		}
	}

	int screenWidth, screenHeight;
	CBaseModPanel::GetSingleton().GetSize( screenWidth, screenHeight );

	int nTileWidth, nTileHeight;
	GetDialogTileSize( nTileWidth, nTileHeight );

	int nDesiredDialogWidth = 7 * nTileWidth;
	int nDesiredDialogHeight = 2 * nTileHeight;

	int nMsgWide = 0;
	int nMsgTall = 0;
	if ( m_pLblMessage )
	{
		// account for the size of the message
		m_pLblMessage->GetContentSize( nMsgWide, nMsgTall );
		if ( nMsgWide > nDesiredDialogWidth )
		{
			m_pLblMessage->SetWrap( true );
			m_pLblMessage->SetWide( nDesiredDialogWidth );
			m_pLblMessage->SetTextInset( 0, 0 );
			m_pLblMessage->GetContentSize( nMsgWide, nMsgTall );
		}
	}

	// account for one column due to icon plus spacing
	int nRequiredDialogWidth = nMsgWide + nTileWidth + nTileWidth/2;
	int nRequiredDialogHeight = nMsgTall + nTileHeight/2;

	if ( nDesiredDialogWidth < nRequiredDialogWidth )
	{
		nDesiredDialogWidth = nRequiredDialogWidth;
	}

	if ( nDesiredDialogHeight < nRequiredDialogHeight )
	{
		nDesiredDialogHeight = nRequiredDialogHeight;
	}

	int nDialogTileWidth = ( nDesiredDialogWidth + nTileWidth - 1 )/nTileWidth;
	int nDialogTileHeight = ( nDesiredDialogHeight + nTileHeight - 1 )/nTileHeight;

	// set size in tile units
	SetDialogTitle( m_data.pWindowTitle, NULL, false, nDialogTileWidth, nDialogTileHeight );
	SetupAsDialogStyle();

	int x, y, nActualDialogWidth, nActualDialogHeight;
	GetBounds( x, y, nActualDialogWidth, nActualDialogHeight );

	if ( m_pLblMessage )
	{
		// center the message
		int msgWide, msgTall;
		m_pLblMessage->GetContentSize( msgWide, msgTall );

		// account for the possible dialog title
		if ( m_data.pWindowTitle )
		{
			y = ( ( nActualDialogHeight - nTileHeight ) - msgTall ) / 2;
		}
		else
		{
			y = ( nActualDialogHeight - msgTall ) / 2;
		}
		m_pLblMessage->SetBounds( nTileWidth + m_nTextOffsetX, y, msgWide, msgTall );

		vgui::ImagePanel *pIcon = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "InfoIcon" ) );
		if ( pIcon )
		{
			vgui::HFont hFont = m_pLblMessage->GetFont();
			int nFontTall = 0;
			if ( hFont != vgui::INVALID_FONT )
			{
				nFontTall = vgui::surface()->GetFontTall( hFont );
			}

			int nIconX, nIconY, nIconWide, nIconTall;
			pIcon->GetBounds( nIconX, nIconY, nIconWide, nIconTall );
			pIcon->SetPos( nIconX, y + ( nFontTall - nIconTall ) / 2 + m_nIconOffsetY );
		}
	}
}

void GenericConfirmation::PaintBackground()
{
	BaseClass::PaintBackground();

	if ( m_bNeedsMoveToFront )
	{
		vgui::ipanel()->MoveToFront( GetVPanel() );
		m_bNeedsMoveToFront = false;
	}
}

// returns the usageId, which gets incremented each time this function is called
int GenericConfirmation::SetUsageData( const Data_t &data )
{
	m_data = data;

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] GenericConfirmation::SetWindowTitle : %s\n", data.pWindowTitle!=NULL ? data.pWindowTitle : "<NULL>" );
		Msg( "[GAMEUI] GenericConfirmation::SetMessageText : %s\n", data.pMessageText!=NULL ? data.pMessageText : "<NULL>" );
	}

	if ( m_pLblMessage )
	{
		m_pLblMessage->DeletePanel();
		m_pLblMessage = NULL;
	}

	if ( data.pMessageTextW )
	{
		m_pLblMessage = new Label( this, "LblMessage", data.pMessageTextW );
	}
	else
	{
		m_pLblMessage = new Label( this, "LblMessage", data.pMessageText );
	}

	// tell our base version so input is disabled
	m_OkButtonEnabled = data.bOkButtonEnabled;
	m_CancelButtonEnabled = data.bCancelButtonEnabled;

	// the window may have to change based on altered data
	FixLayout();
	UpdateFooter();

	return m_usageId = ++sm_currentUsageId;
}

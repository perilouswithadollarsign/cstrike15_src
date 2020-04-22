//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGenericWaitScreen.h"
#include "EngineInterface.h"
#include "tier1/KeyValues.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"

#include "vfooterpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

GenericWaitScreen::GenericWaitScreen( Panel *parent, const char *panelName ):
BaseClass( parent, panelName, false, false, false )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	AddFrameListener( this );

	m_LastEngineTime = 0;
	m_CurrentSpinnerValue = 0;

	m_callbackPanel = 0;
	m_callbackMessage = NULL;
	
	m_MsgStartDisplayTime = -1.0f;
	m_MsgMinDisplayTime = 0.0f;
	m_MsgMaxDisplayTime = 0.0f;
	m_pfnMaxTimeOut = NULL;
	m_bClose = false;
	m_currentDisplayText = "";
	m_bTextSet = true;
	m_bValid = false;
	m_bNeedsLayoutFixed = true;

	m_pWorkingAnim = NULL;
	m_pLblMessage = NULL;

	m_hMessageFont = vgui::INVALID_FONT;

	m_nTextOffsetX = 0;

	m_pAsyncOperationAbortable = NULL;
	m_pWaitscreenCallbackInterface = NULL;

	SetFooterEnabled( true );

#if defined( PORTAL2_PUZZLEMAKER )
	m_hTargetFileHandle = k_UGCHandleInvalid;
#endif // PORTAL2_PUZZLEMAKER
}

GenericWaitScreen::~GenericWaitScreen()
{
	RemoveFrameListener( this );

	delete m_pLblMessage;
	delete m_pWorkingAnim;
}

void GenericWaitScreen::SetMessageText( const char *pMessage )
{
	m_currentDisplayText = pMessage;
	m_bTextSet = false;

	if ( m_pLblMessage )
	{
		m_bTextSet = true;
		m_pLblMessage->SetText( pMessage );
		m_bNeedsLayoutFixed = true;
	}
}

void GenericWaitScreen::SetMessageText( const wchar_t *pMessage )
{
	if ( m_pLblMessage )
	{
		m_bTextSet = true;
		m_pLblMessage->SetText( pMessage );
		m_bNeedsLayoutFixed = true;
	}
}

void GenericWaitScreen::AddMessageText( const char *pMessage, float minDisplayTime )
{
	float time = Plat_FloatTime();
	if ( time > m_MsgStartDisplayTime + m_MsgMinDisplayTime )
	{
		SetMessageText( pMessage );
		m_MsgStartDisplayTime = time;
		m_MsgMinDisplayTime = minDisplayTime;
	}
	else
	{
		WaitMessage msg;
		msg.mDisplayString = pMessage;
		msg.minDisplayTime = minDisplayTime;
		m_MsgVector.AddToTail( msg );
	}
}

void GenericWaitScreen::AddMessageText( const wchar_t *pMessage, float minDisplayTime )
{
	float time = Plat_FloatTime();
	if ( time > m_MsgStartDisplayTime + m_MsgMinDisplayTime )
	{
		SetMessageText( pMessage );
		m_MsgStartDisplayTime = time;
		m_MsgMinDisplayTime = minDisplayTime;
	}
	else
	{
		WaitMessage msg;
		msg.mWchMsgText = pMessage;
		msg.minDisplayTime = minDisplayTime;
		m_MsgVector.AddToTail( msg );
	}
}

void GenericWaitScreen::SetCloseCallback( vgui::Panel *pPanel, const char *pMessage )
{
	if ( pMessage )
	{
		m_callbackPanel = pPanel;
		m_callbackMessage = pMessage;
	}
	else 
	{
		m_bClose = true;
	}

	CheckIfNeedsToClose();
}

void GenericWaitScreen::ClearData()
{
	m_MsgVector.Purge();
	m_callbackPanel = 0;
	m_callbackMessage = NULL;
	m_MsgStartDisplayTime = -1.0f;
	m_MsgMinDisplayTime = 0.0f;
	m_MsgMaxDisplayTime = 0.0f;
	m_pfnMaxTimeOut = NULL;
	m_bClose = false;
}

void GenericWaitScreen::OnThink()
{
	BaseClass::OnThink();

	if ( m_pWaitscreenCallbackInterface )
		m_pWaitscreenCallbackInterface->OnThink();
	
	SetUpText();
	ClockAnim();
	UpdateFooter();
	CheckIfNeedsToClose();
	FixLayout();
}

void GenericWaitScreen::RunFrame()
{
	CheckIfNeedsToClose();
}

void GenericWaitScreen::SetDataSettings( KeyValues *pSettings )
{
	m_pAsyncOperationAbortable = ( IMatchAsyncOperation * ) pSettings->GetPtr( "options/asyncoperation", NULL );
	m_pWaitscreenCallbackInterface = ( IWaitscreenCallbackInterface * ) pSettings->GetPtr( "options/waitscreencallback", NULL );

#if defined( PORTAL2_PUZZLEMAKER )
	m_hTargetFileHandle = pSettings->GetUint64( "options/filehandle", k_UGCHandleInvalid );
#endif // PORTAL2_PUZZLEMAKER
}

void GenericWaitScreen::OnKeyCodePressed(vgui::KeyCode code)
{
	CBaseModPanel::GetSingleton().SetLastActiveUserId( GetJoystickForCode( code ) );

	// Let the callback interface get a first crack at keycode
	if ( m_pWaitscreenCallbackInterface && !m_pWaitscreenCallbackInterface->OnKeyCodePressed( code ) )
			return;

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		if ( m_pAsyncOperationAbortable &&
			 AOS_RUNNING == m_pAsyncOperationAbortable->GetState() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			
			m_pAsyncOperationAbortable->Abort();
			m_pAsyncOperationAbortable = NULL;
		}
		break;
	}
}

void GenericWaitScreen::PaintBackground()
{
	BaseClass::PaintBackground();
}

void GenericWaitScreen::ApplySchemeSettings( vgui::IScheme * pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hMessageFont = pScheme->GetFont( "ConfirmationText", true );

	m_nTextOffsetX = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TitleOffsetX" ) ) );

	m_pWorkingAnim = new ImagePanel( this, "WorkingAnim" );
	if ( m_pWorkingAnim )
	{
#if defined( PORTAL2_PUZZLEMAKER )
		const char *lpszSpinnerImage = ( m_hTargetFileHandle != k_UGCHandleInvalid ) ? "progress" : "spinner";
		m_flCustomProgress = -1.f; // init to not use
		m_pWorkingAnim->SetImage( lpszSpinnerImage );
#else  // PORTAL2_PUZZLEMAKER
		m_pWorkingAnim->SetImage( "spinner" );
#endif // PORTAL2_PUZZLEMAKER

		m_pWorkingAnim->SetShouldScaleImage( true );
	}

	m_pLblMessage = new Label( this, "LblMessage", "" );
	if ( m_pLblMessage )
	{
		m_pLblMessage->SetFont( m_hMessageFont );
	}

	m_bValid = true;
	OnThink();
}

void GenericWaitScreen::ClockAnim()
{
	if ( m_pWorkingAnim == NULL )
		return;

#if defined( PORTAL2_PUZZLEMAKER )
	// Update our progress bar version if we have one
	if ( m_hTargetFileHandle != k_UGCHandleInvalid )
	{
		float flProgress = ( m_flCustomProgress >= 0.f ) ? m_flCustomProgress : WorkshopManager().GetUGCFileDownloadProgress( m_hTargetFileHandle );
		int nAnimFrame = (int) RemapValClamped( flProgress, 0.0f, 1.0f, 0, m_pWorkingAnim->GetNumFrames()-1 );

		m_pWorkingAnim->SetFrame( nAnimFrame );
		m_pWorkingAnim->SetVisible( true ); 
		
		return;
	}
#endif // PORTAL2_PUZZLEMAKER
	
	// clock the anim at 10hz
	float time = Plat_FloatTime();
	if ( ( m_LastEngineTime + 0.1f ) < time )
	{
		m_LastEngineTime = time;
		m_pWorkingAnim->SetFrame( m_CurrentSpinnerValue++ );
	}
}

void GenericWaitScreen::CheckIfNeedsToClose()
{
	float time = Plat_FloatTime();
	if ( time <= m_MsgStartDisplayTime + m_MsgMinDisplayTime )
		return;

	// timer has expired, update message or close
	if ( m_MsgVector.Count() )
	{
		// use last message
		int iLastElement = m_MsgVector.Count() - 1;
		WaitMessage &msg = m_MsgVector[iLastElement];

		if ( msg.mWchMsgText )
			SetMessageText( msg.mWchMsgText );
		else
			SetMessageText( msg.mDisplayString.Get() );
		
		m_MsgStartDisplayTime = time;
		m_MsgMinDisplayTime = msg.minDisplayTime;
		m_MsgVector.Remove( iLastElement );
	}
	else if ( !m_callbackMessage.IsEmpty() )
	{
		if ( !m_callbackPanel )
			m_callbackPanel = GetNavBack();

		if ( m_callbackPanel )
			m_callbackPanel->PostMessage( m_callbackPanel, new KeyValues( m_callbackMessage.Get() ) );

		NavigateBack();
	}
	else if ( m_bClose )
	{
		//just nav back.
		NavigateBack();
	}
	else if ( m_MsgMaxDisplayTime > 0 )
	{
		if ( m_MsgStartDisplayTime + m_MsgMaxDisplayTime < time )
		{
			NavigateBack();
			
			if ( m_pfnMaxTimeOut )
				m_pfnMaxTimeOut();
		}
	}
}

void GenericWaitScreen::SetMaxDisplayTime( float flMaxTime, void (*pfn)() )
{
	m_MsgMaxDisplayTime = flMaxTime;
	m_pfnMaxTimeOut = pfn;
}

void GenericWaitScreen::SetUpText()
{
	if ( !m_bTextSet )
	{
		SetMessageText( m_currentDisplayText.Get() );
	}
}

void GenericWaitScreen::FixLayout()
{
	if ( !m_bValid || !m_bNeedsLayoutFixed )
	{
		// want to delay this until ApplySchemeSettings() gets called
		return;
	}

	m_bNeedsLayoutFixed = false;

	int screenWidth, screenHeight;
	CBaseModPanel::GetSingleton().GetSize( screenWidth, screenHeight );

	int nTileWidth, nTileHeight;
	GetDialogTileSize( nTileWidth, nTileHeight );

	int nDesiredDialogWidth = 7 * nTileWidth;
	int nDesiredDialogHeight = 2 * nTileHeight;

	int nSpinnerSize = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 85 );

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

	// account for gaps around the spinner and message
	int nRequiredDialogWidth = nMsgWide + 2*nTileWidth + nTileWidth/2;
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
	SetDialogTitle( "", NULL, false, nDialogTileWidth, nDialogTileHeight );
	SetupAsDialogStyle();

	int x, y, nActualDialogWidth, nActualDialogHeight;
	GetBounds( x, y, nActualDialogWidth, nActualDialogHeight );

	if ( m_pLblMessage )
	{
		// center the message
		int msgWide, msgTall;
		m_pLblMessage->GetContentSize( msgWide, msgTall );

		y = ( nActualDialogHeight - msgTall ) / 2;
		
		m_pLblMessage->SetBounds( 2 * nTileWidth + m_nTextOffsetX, y, msgWide, msgTall );
	}

	if ( m_pWorkingAnim )
	{
		// center the spinner
		m_pWorkingAnim->SetBounds( nTileWidth - nSpinnerSize/2, ( nActualDialogHeight - nSpinnerSize ) / 2, nSpinnerSize, nSpinnerSize );
	}
}

void GenericWaitScreen::UpdateFooter()
{
	bool bCanCancel = ( m_pAsyncOperationAbortable &&
		AOS_RUNNING == m_pAsyncOperationAbortable->GetState() );

	CBaseModFooterPanel *pFooter = ( CBaseModFooterPanel * ) BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( !pFooter )
		return;
	
	if ( bCanCancel )
	{
		pFooter->SetButtons( FB_BBUTTON, FF_ABXYDL_ORDER, FOOTER_GENERICWAITSCREEN );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel", false, FOOTER_GENERICWAITSCREEN );
	}
	else
	{
		pFooter->SetButtons( 0, FF_ABXYDL_ORDER, FOOTER_GENERICWAITSCREEN );
	}
}

#if defined( PORTAL2_PUZZLEMAKER )
void GenericWaitScreen::SetTargetFileHandle( UGCHandle_t hFileHandle, float flCustomProgress /*= -1.f*/ )
{
	m_hTargetFileHandle = hFileHandle;
	m_flCustomProgress = flCustomProgress;

	const char *lpszSpinnerImage = ( m_hTargetFileHandle != k_UGCHandleInvalid ) ? "progress" : "spinner";
	if ( m_pWorkingAnim )
	{
		m_pWorkingAnim->SetImage( lpszSpinnerImage );
	}
}
#endif //

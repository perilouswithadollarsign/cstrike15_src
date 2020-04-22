//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vsteamlinkdialog.h"
#include "VAttractScreen.h"
#include "VFooterPanel.h"
#include "tier1/KeyValues.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"

#include "vhybridbutton.h"
#include "vgenericconfirmation.h"

#ifdef _PS3
#include "sysutil/sysutil_oskdialog.h"
#include "sys/memory.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

static char g_chSteamUserName[256];
static char g_chSteamUserPwd[256];
#ifdef _PS3
static wchar_t g_sysOSKmessage[256];
static wchar_t g_sysOSKstring[256];
static uint64 g_sysOSKstatus;
static char *g_sysOSKinputConvert;
static int g_sysOSKloaded;
#endif

//=============================================================================
SteamLinkDialog::SteamLinkDialog( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, false, true ),
	m_bResetSteamConnectionReason( true ),
	m_pAvatarImage( NULL ),
	m_xuidAvatarImage( 0ull )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetDialogTitle( NULL, L"", false, 0, 0, 4 );

	SetFooterEnabled( true );

	V_memset( m_chError, 0, sizeof( m_chError ) );
	m_bVirtualKeyboardStarted = false;
	m_bAutomaticNameInput = false;
}

SteamLinkDialog::~SteamLinkDialog()
{
#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( m_bResetSteamConnectionReason )
		CUIGameData::Get()->SetConnectionToSteamReason( NULL );
#endif

	if ( m_pAvatarImage )
	{
		CUIGameData::Get()->AccessAvatarImage( m_xuidAvatarImage, CUIGameData::kAvatarImageRelease );
		m_pAvatarImage = NULL;
		m_xuidAvatarImage = 0ull;
	}
}

void SteamLinkDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	UpdateFooter();
}


//=============================================================================
void SteamLinkDialog::OnCommand( const char *command )
{
	int iUser = BaseModUI::CBaseModPanel::GetSingleton().GetLastActiveUserId();
	iUser;

	if( !Q_strcmp( command, "BtnLink" ) )
	{
		vgui::Label *pError = dynamic_cast< vgui::Label * >( FindChildByName( "LblMustBeSignedIn" ) );
		if ( pError )
		{
			pError->SetText( "#L4D360UI_Steam_LinkUserPwdHint" );
		}

		SwitchToUserPwdMode( true );

		OnCommand( "BtnLinkTxtName" );
		m_bAutomaticNameInput = true;
		return;
	}
	else if( !Q_strcmp( command, "BtnAnonymous" ) )
	{
		m_bResetSteamConnectionReason = false;
		NavigateBack();
		#if !defined( NO_STEAM )
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		CUIGameData::Get()->InitiateConnectionToSteam( "", "" );
		#endif
		return;
	}
	else if( !Q_strcmp( command, "BtnLinkUserPwd" ) )
	{
		m_bResetSteamConnectionReason = false;
		NavigateBack();
		#if !defined( NO_STEAM )
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		CUIGameData::Get()->InitiateConnectionToSteam( g_chSteamUserName, g_chSteamUserPwd );
		#endif
		return;
	}
	else if ( !Q_strcmp( command, "BtnLinkTxtName" ) )
	{
		// Invoke virtual keyboard
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		InvokeVirtualKeyboard( "#L4D360UI_Steam_Username_Header", g_chSteamUserName, false );
	}
	else if ( !Q_strcmp( command, "BtnLinkTxtPwd" ) )
	{
		// Invoke virtual keyboard
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		InvokeVirtualKeyboard( "#L4D360UI_Steam_Password_Header", g_chSteamUserPwd, true );
	}

	BaseClass::OnCommand( command );
}

#ifdef _PS3
static void sysutil_callback( uint64_t status, uint64_t param, void * userdata )
{
	DevMsg( "OSK sysutil_callback( 0x%16llx, 0x%16llx )\n", status, param );

	(void)param;
	(void)userdata;

	switch(status)
	{
	case CELL_SYSUTIL_OSKDIALOG_LOADED:
		g_sysOSKloaded = 2;
		break;
	case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
		g_sysOSKloaded = 0;
		cellSysutilUnregisterCallback( 1 );
		break;

	case CELL_SYSUTIL_OSKDIALOG_FINISHED:
	case CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED:
	case CELL_SYSUTIL_OSKDIALOG_INPUT_ENTERED:
		if ( g_sysOSKloaded >= 2 )
		{
			g_sysOSKloaded = 1;
			CellOskDialogCallbackReturnParam outputInfo;
			outputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
			outputInfo.numCharsResultString = ARRAYSIZE( g_sysOSKstring ) - 2;
			outputInfo.pResultString = ( uint16* ) g_sysOSKstring;
			if ( ( cellOskDialogUnloadAsync( &outputInfo ) >= 0 ) && outputInfo.pResultString )
				g_sysOSKstatus = outputInfo.result;
		}
		break;
	case CELL_SYSUTIL_OSKDIALOG_INPUT_DEVICE_CHANGED:
		if(param == CELL_OSKDIALOG_INPUT_DEVICE_KEYBOARD ){
			/*E If the input device becomes the hardware keyboard, */
			/*   stop receiving input from the on-screen keyboard dialog */
			// ret = cellOskDialogSetDeviceMask( CELL_OSKDIALOG_DEVICE_MASK_PAD );
		}
		break;
	}
}
#endif

void SteamLinkDialog::InvokeVirtualKeyboard( char const *szTitleFmt, char const *szDefaultText, bool bPassword )
{
#ifdef _PS3
	if ( g_sysOSKloaded )
	{
		DevWarning( "InvokeVirtualKeyboard called before OSK unloaded, ignored!\n" );
		return;
	}

	m_bVirtualKeyboardStarted = true;
	m_bAutomaticNameInput = false;

	g_pVGuiLocalize->ConstructString( g_sysOSKmessage, sizeof( g_sysOSKmessage ),
		g_pVGuiLocalize->Find( szTitleFmt ), 1, L"" );

	g_pVGuiLocalize->ConvertANSIToUnicode( szDefaultText, g_sysOSKstring, sizeof( g_sysOSKstring ) );
	g_sysOSKinputConvert = ( char * ) szDefaultText;

	CellOskDialogInputFieldInfo inputFieldInfo;
	inputFieldInfo.message = ( uint16* ) g_sysOSKmessage;
	inputFieldInfo.init_text = ( uint16* ) g_sysOSKstring;
	inputFieldInfo.limit_length = ARRAYSIZE( g_sysOSKstring ) - 2;

	cellOskDialogSetKeyLayoutOption( CELL_OSKDIALOG_10KEY_PANEL | CELL_OSKDIALOG_FULLKEY_PANEL );

	cellOskDialogSetLayoutMode( CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP );

	CellOskDialogPoint pos;
	pos.x = pos.y = 0;
	
	CellOskDialogParam params;
	params.allowOskPanelFlg =
		( bPassword ? CELL_OSKDIALOG_PANELMODE_PASSWORD : 0 ) |
		CELL_OSKDIALOG_PANELMODE_ALPHABET |
		CELL_OSKDIALOG_PANELMODE_NUMERAL |
		CELL_OSKDIALOG_PANELMODE_LATIN;
	params.firstViewPanel = bPassword ? CELL_OSKDIALOG_PANELMODE_PASSWORD : CELL_OSKDIALOG_PANELMODE_ALPHABET;
	params.controlPoint = pos;
	params.prohibitFlgs = CELL_OSKDIALOG_NO_RETURN;

	g_sysOSKstatus = CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED;
	cellSysutilRegisterCallback( 1, sysutil_callback, NULL );
	cellOskDialogLoadAsync( SYS_MEMORY_CONTAINER_ID_INVALID, &params, &inputFieldInfo );
#endif
}

void SteamLinkDialog::SwitchToUserPwdMode( bool bForceName )
{
	SetControlVisible( "BtnLink", false );
	SetControlVisible( "BtnAnonymous", false );

	SetControlVisible( "BtnLinkTxtName", true );
	SetControlVisible( "BtnLinkTxtNameBorder", true );
	SetControlVisible( "BtnLinkTxtPwd", true );
	SetControlVisible( "BtnLinkTxtPwdBorder", true );
	SetControlVisible( "BtnLinkUserPwd", false );

	if ( m_ActiveControl )
		m_ActiveControl->NavigateFrom();

	wchar_t wszConstructedString[256];
	wchar_t wszBufUserName[256];
	g_pVGuiLocalize->ConvertANSIToUnicode( g_chSteamUserName, wszBufUserName, sizeof( wszBufUserName ) );
	if ( Q_wcslen( wszBufUserName ) > 15 )
	{
		wszBufUserName[15] = L'.';
		wszBufUserName[16] = L'.';
		wszBufUserName[17] = L'.';
		wszBufUserName[18] = 0;
	}

	wchar_t *pwszUserName = g_chSteamUserName[0] ? wszBufUserName : g_pVGuiLocalize->Find( "#L4D360UI_Steam_EmptyEntry" );
	if ( !pwszUserName ) pwszUserName = L"";

	g_pVGuiLocalize->ConstructString( wszConstructedString, sizeof( wszConstructedString),
		g_pVGuiLocalize->Find( "#L4D360UI_Steam_Username" ), 1, pwszUserName );
	if ( BaseModHybridButton *btn = dynamic_cast< BaseModHybridButton * > ( FindChildByName( "BtnLinkTxtName" ) ) )
		btn->SetText( wszConstructedString );

	wchar_t *pwszUserPwd = L"";
	if ( g_chSteamUserPwd[0] )
	{
		int nLetters = V_strlen( g_chSteamUserPwd );
		for ( int k = 0; k < MIN( 15, MIN( nLetters, (int)ARRAYSIZE( wszBufUserName ) - 1 ) ); ++ k )
		{
			wszBufUserName[k] = L'*';
		}
		wszBufUserName[nLetters] = 0;
		pwszUserPwd = wszBufUserName;
	}
	else
	{
		pwszUserPwd = g_pVGuiLocalize->Find( "#L4D360UI_Steam_EmptyEntry" );
	}
	if ( !pwszUserPwd ) pwszUserPwd = L"";

	g_pVGuiLocalize->ConstructString( wszConstructedString, sizeof( wszConstructedString),
		g_pVGuiLocalize->Find( "#L4D360UI_Steam_Password" ), 1, pwszUserPwd );
	if ( BaseModHybridButton *btn = dynamic_cast< BaseModHybridButton * > ( FindChildByName( "BtnLinkTxtPwd" ) ) )
		btn->SetText( wszConstructedString );

	if ( !g_chSteamUserName[0] || bForceName )
		FindChildByName( "BtnLinkTxtName" )->NavigateTo();
	else if ( !g_chSteamUserPwd[0] )
		FindChildByName( "BtnLinkTxtPwd" )->NavigateTo();
	else
	{
		SetControlVisible( "BtnLinkUserPwd", true );
		FindChildByName( "BtnLinkUserPwd" )->NavigateTo();
	}
}

//=============================================================================
void SteamLinkDialog::LoadLayout()
{
	BaseClass::LoadLayout();

#ifdef _GAMECONSOLE
	if( vgui::Panel * panel = FindChildByName( "BtnLink" ) )
	{
		panel->NavigateTo();
	}

	vgui::Label *pError = dynamic_cast< vgui::Label * >( FindChildByName( "LblMustBeSignedIn" ) );
	if ( !m_chError[0] )
		g_chSteamUserPwd[0] = 0;
	if ( m_chError[0] && pError )
	{
		pError->SetText( m_chError );
		SwitchToUserPwdMode( true );
	}
#endif // _GAMECONSOLE

#if defined( _PS3 ) && !defined( NO_STEAM )
	CSteamID sPsnId = steamapicontext->SteamUser()->GetConsoleSteamID();
	if ( sPsnId.IsValid() )
	{
		vgui::ImagePanel *imgAvatar = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
		if ( !m_pAvatarImage && imgAvatar )
		{
			XUID xuidMyPSN = sPsnId.ConvertToUint64();
			m_pAvatarImage = CUIGameData::Get()->AccessAvatarImage( xuidMyPSN, CUIGameData::kAvatarImageRequest );
			if ( m_pAvatarImage )
			{
				imgAvatar->SetImage( m_pAvatarImage );
				m_xuidAvatarImage = xuidMyPSN;
			}
		}

		if ( vgui::Label *pLblPlayerGamerTag = dynamic_cast< vgui::Label * > ( FindChildByName( "LblPSNaccountLine2" ) ) )
		{
			const char *pszName = steamapicontext->SteamFriends()->GetFriendPersonaName( sPsnId );
			if ( pszName && *pszName )
			{
				pLblPlayerGamerTag->SetText( pszName );
			}
		}
	}
#endif
}

//=============================================================================
void SteamLinkDialog::SetDataSettings( KeyValues *pSettings )
{
	Q_strncpy( m_chError, pSettings->GetString( "error" ), sizeof( m_chError ) );
}

void SteamLinkDialog::OnThink()
{
#ifdef _PS3
	if ( m_bVirtualKeyboardStarted &&
		!g_sysOSKloaded &&
		g_sysOSKinputConvert &&
		g_sysOSKstatus == CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK )
	{
		// OSK dialog has finished successfully
		g_pVGuiLocalize->ConvertUnicodeToANSI( g_sysOSKstring, g_sysOSKinputConvert, ARRAYSIZE( g_sysOSKstring ) );
		g_sysOSKinputConvert = NULL;

		SwitchToUserPwdMode();

		if ( m_bAutomaticNameInput )
			OnCommand( "BtnLinkTxtPwd" );
	}

	if ( !steamapicontext->SteamUtils()->BIsPSNOnline() )
	{
		// Go to the main menu
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box and then enqueue main menu
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
		CBaseModPanel::GetSingleton().OpenFrontScreen( true );

		// Set message
		GenericConfirmation::Data_t data;
		data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";	// "Disconnect"
		data.pMessageText = "#SessionError_PSN";
		data.bOkButtonEnabled = true;
		confirmation->SetUsageData(data);
	}
#endif
	BaseClass::OnThink();
}

void SteamLinkDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON | FB_ABUTTON );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Ok" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}
}


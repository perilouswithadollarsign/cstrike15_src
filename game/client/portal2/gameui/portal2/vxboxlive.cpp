//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VXboxLIVE.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "VGenericConfirmation.h"
#include "vgui_controls/Button.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

XboxLiveOptions::XboxLiveOptions( Panel *parent, const char *panelName ):
BaseClass( parent, panelName ),
m_nMsgBoxId( 0 )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#Portal2UI_PlayOnline" );

	if ( IsPS3() )
	{
		V_snprintf( m_ResourceName, sizeof( m_ResourceName ), "resource/ui/basemodui/steamextras.res" );
	}

	SetFooterEnabled( true );
	UpdateFooter();

#ifdef _PS3
	m_bVirtualKeyboardStarted = false;
#endif
}

XboxLiveOptions::~XboxLiveOptions()
{
}

void XboxLiveOptions::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void XboxLiveOptions::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void XboxLiveOptions::OnClose()
{
	if ( m_nMsgBoxId )
	{
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
		if ( confirmation != NULL && confirmation->GetUsageId() == m_nMsgBoxId )
		{
			confirmation->Close();
		}
	}

	BaseClass::OnClose();
}

static void QuickMatchImpl()
{
	static char s_szGameMode[64];
	static char s_szMapName[64];
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;
	V_strncpy( s_szGameMode, pMatchSession->GetSessionSettings()->GetString( "game/mode", "coop" ), sizeof( s_szGameMode ) );
	V_strncpy( s_szMapName, pMatchSession->GetSessionSettings()->GetString( "game/map", "default" ), sizeof( s_szMapName ) );
	CUIGameData::Get()->InitiateOnlineCoopPlay( NULL, "quickmatch", s_szGameMode, s_szMapName );
}

void XboxLiveOptions::OnCommand( const char *pCommand )
{
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling options menu command %s from user%d ctrlr%d\n", pCommand, iUserSlot, iController );
	}

	if ( char const *szInviteType = StringAfterPrefix( pCommand, "#L4D360UI_InviteUI_" ) )
	{
		if ( IsGameConsole() )
		{
			CUIGameData::Get()->OpenInviteUI( szInviteType );
		}
		else
		{
			CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
	}
	else if ( !V_stricmp( pCommand, "BtnQuickMatch" ) )
	{
		GenericConfirmation::Data_t data;
		data.pWindowTitle = "#Portal2UI_PlayOnline";
		data.pMessageText = "#Portal2UI_pvp_QuickMatch_Confirm";

		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;
		data.pfnOkCallback = QuickMatchImpl;

		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		if ( confirmation )
		{
			m_nMsgBoxId = confirmation->SetUsageData(data);
		}
	}
	else if ( !V_stricmp( pCommand, "RedeemSteamCode" ) )
	{
		RedeemSteamCode();
	}
}


#ifdef _PS3
static char g_chSteamCode[256];
static wchar_t g_sysOSKmessage[256];
static wchar_t g_sysOSKstring[256];
static uint64 g_sysOSKstatus;
static char *g_sysOSKinputConvert;
static int g_sysOSKloaded;
#include "sysutil/sysutil_oskdialog.h"
#include "sys/memory.h"

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
	}
}
#endif

void XboxLiveOptions::RedeemSteamCode()
{
#ifdef _PS3
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	if ( g_sysOSKloaded )
	{
		DevWarning( "InvokeVirtualKeyboard called before OSK unloaded, ignored!\n" );
		return;
	}

	m_bVirtualKeyboardStarted = true;

	g_pVGuiLocalize->ConstructString( g_sysOSKmessage, sizeof( g_sysOSKmessage ),
		g_pVGuiLocalize->Find( "#L4D360UI_Steam_RedeemToken" ), 1, L"" );

	g_pVGuiLocalize->ConvertANSIToUnicode( g_chSteamCode, g_sysOSKstring, sizeof( g_sysOSKstring ) );
	g_sysOSKinputConvert = ( char * ) g_chSteamCode;

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
		CELL_OSKDIALOG_PANELMODE_ALPHABET |
		CELL_OSKDIALOG_PANELMODE_NUMERAL |
		CELL_OSKDIALOG_PANELMODE_LATIN;
	params.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ALPHABET;
	params.controlPoint = pos;
	params.prohibitFlgs = CELL_OSKDIALOG_NO_RETURN;

	g_sysOSKstatus = CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED;
	cellSysutilRegisterCallback( 1, sysutil_callback, NULL );
	cellOskDialogLoadAsync( SYS_MEMORY_CONTAINER_ID_INVALID, &params, &inputFieldInfo );
#endif
}

void XboxLiveOptions::OnThink()
{
#ifdef _PS3
	if ( m_bVirtualKeyboardStarted &&
		!g_sysOSKloaded &&
		g_sysOSKinputConvert &&
		g_sysOSKstatus == CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK )
	{
		// OSK dialog has finished successfully
		g_pVGuiLocalize->ConvertUnicodeToANSI( g_sysOSKstring, g_sysOSKinputConvert, ARRAYSIZE( g_sysOSKstring ) );
	#if !defined( NO_STEAM )
		if ( g_sysOSKinputConvert && *g_sysOSKinputConvert )
		{
			SteamAPICall_t hCall = steamapicontext->SteamApps()->RegisterActivationCode( g_sysOSKinputConvert );
			m_CallbackOnRegisterActivationCodeResponse.Set( hCall, this, &XboxLiveOptions::Steam_OnRegisterActivationCodeResponse );
			CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_Steam_RedeemTokenProgress", 0, NULL );
		}
	#endif
		g_sysOSKinputConvert = NULL;
		m_bVirtualKeyboardStarted = false;
	}
#endif
	BaseClass::OnThink();
}

#if defined( _PS3 ) && !defined( NO_STEAM )
void XboxLiveOptions::Steam_OnRegisterActivationCodeResponse( RegisterActivationCodeResponse_t *p, bool bError )
{
	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#L4D360UI_Steam";
	data.pMessageText = "#L4D360UI_Steam_Error_LinkUnexpected";
	data.bOkButtonEnabled = true;

	if ( !bError && ( p->m_eResult == k_ERegisterActivactionCodeResultOK ) )
	{
		switch ( p->m_unPackageRegistered )
		{
		case PORTAL2_DLC_PKGID_COOP_BOT_SKINS:
			data.pMessageText = "#L4D360UI_Steam_RedeemTokenOK_DLC02";
			break;
		case PORTAL2_DLC_PKGID_COOP_BOT_HELMETS:
			data.pMessageText = "#L4D360UI_Steam_RedeemTokenOK_DLC03";
			break;
		case PORTAL2_DLC_PKGID_COOP_BOT_ANTENNA:
			data.pMessageText = "#L4D360UI_Steam_RedeemTokenOK_DLC04";
			break;
		case PORTAL2_DLC_PKGID_PCSTEAMPLAY:
			data.pMessageText = "#L4D360UI_Steam_RedeemTokenOK_DLC00";
			break;
		default:
			data.pMessageText = "#L4D360UI_Steam_RedeemTokenOK_Generic";
			break;
		}
		g_chSteamCode[0] = 0;
	}
	if ( !bError && ( p->m_eResult == k_ERegisterActivactionCodeResultFail ) )
	{
		data.pMessageText = "#L4D360UI_Steam_RedeemTokenError_Fail";
	}
	else if ( !bError && ( p->m_eResult == k_ERegisterActivactionCodeResultAlreadyRegistered ) )
	{
		data.pMessageText = "#L4D360UI_Steam_RedeemTokenError_Already";
	}
	else if ( !bError && ( p->m_eResult == k_ERegisterActivactionCodeAlreadyOwned ) )
	{
		data.pMessageText = "#L4D360UI_Steam_RedeemTokenError_Owned";
	}

	if ( GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) ) )
	{
		m_nMsgBoxId = confirmation->SetUsageData(data);
	}
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}
#endif




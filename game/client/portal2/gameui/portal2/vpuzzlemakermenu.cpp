//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include "vpuzzlemakermenu.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "vpuzzlemakerbetatests.h"
#include "transitionpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
using namespace BaseModUI;

#if !defined( NO_STEAM )
static uint32 GetObfuscatedAccountID( CSteamID &steamID )
{
	uint32 accountID = steamID.GetAccountID();
	if ( accountID == 0 )
	{
		return 0;
	}

	return accountID ^ ( accountID / 2 ) ^ 2825520 ;
}
#endif

CPuzzleMakerMenu::CPuzzleMakerMenu( Panel *pParent, const char *pPanelName ):
BaseClass( pParent, pPanelName ), m_pAvatar( NULL ), m_nSteamID( 0 )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#PORTAL2_EditorMenu_Welcome" );
#if !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		char szEmployeeLabel[16];
		char szFullEmployeeLabel[128];
		wchar_t *pEmployeeText = g_pVGuiLocalize->Find( "#PORTAL2_EditorMenu_EmployeeLabel" );
		V_wcstostr( pEmployeeText, V_wcslen( pEmployeeText ) + 1, szEmployeeLabel, sizeof( szEmployeeLabel ) );
		CSteamID playerID = steamapicontext->SteamUser()->GetSteamID();
		uint32 obfuscatedID = GetObfuscatedAccountID( playerID );
		
		V_snprintf( szFullEmployeeLabel, sizeof( szFullEmployeeLabel ), "%s #%d", szEmployeeLabel, obfuscatedID );
		SetDialogSubTitle( szFullEmployeeLabel );
		m_nSteamID = playerID.ConvertToUint64();
	}
#endif
	//SetDialogSubTitle( "TEMP Employee #12345" );

	m_pEmployeeImage = NULL;
	m_pAvatarSpinner = NULL;
	m_flRetryAvatarTime = -1.0f;

	// clear the community map id
	BASEMODPANEL_SINGLETON.SetCurrentCommunityMapID( 0 );

	SetFooterEnabled( true );
	UpdateFooter();
}

CPuzzleMakerMenu::~CPuzzleMakerMenu()
{
	if ( m_pAvatar && m_nSteamID )
	{
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID, BaseModUI::CUIGameData::kAvatarImageRelease, CGameUiAvatarImage::LARGE );
	}
}


void CPuzzleMakerMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pAvatarSpinner = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "AvatarSpinner" ) );

	m_pEmployeeImage = dynamic_cast< ImagePanel*>( FindChildByName( "ImgPlayerAvatar" ) );
	if ( m_pEmployeeImage )
	{
#if !defined( NO_STEAM )
		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			m_pAvatar = BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID, BaseModUI::CUIGameData::kAvatarImageRequest, CGameUiAvatarImage::LARGE );
			if ( m_pAvatar == NULL )
			{
				m_flRetryAvatarTime = gpGlobals->curtime + 1.0f;
				if ( m_pAvatarSpinner )
				{
					m_pAvatarSpinner->SetVisible( true );
				}
			}
			else
			{
				m_pAvatarSpinner->SetVisible( false );
				m_pEmployeeImage->SetImage( m_pAvatar );
			}
		}

#endif
	}

#if !defined( NO_STEAM )
	Label *pLblPlayerName = static_cast< vgui::Label *>( FindChildByName( "LblBadgePlayerName" ) );
	if ( pLblPlayerName && steamapicontext->SteamFriends() )
	{
		pLblPlayerName->SetText( steamapicontext->SteamFriends()->GetPersonaName() );
	}
#endif

	/*
	ImagePanel *pImgBadgeUpgrade = static_cast< ImagePanel *>( FindChildByName( "ImgBadgeUpgrade" ) );
	if ( pImgBadgeUpgrade )
	{
		// TEMP - pick a random badge from 1 to 4
		int nBadgeType = RandomInt(0,4);
		if (nBadgeType == 0 )
		{
			pImgBadgeUpgrade->SetVisible( false );
		}
		else
		{
			char szBadgeName[ 16 ];
			V_snprintf( szBadgeName, sizeof(szBadgeName), "upgrade%.2d", nBadgeType );
			pImgBadgeUpgrade->SetImage( szBadgeName );
		}
		
	}
	*/

	UpdateFooter();
}

void CPuzzleMakerMenu::Activate()
{
	BaseClass::Activate();

	// Reset our avatar if we're returning to this dialog, because who knows what's happened in the meantime
	if ( m_pAvatar && m_nSteamID )
	{
		m_pEmployeeImage->SetVisible( false );
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID, BaseModUI::CUIGameData::kAvatarImageRelease, CGameUiAvatarImage::LARGE );
		m_pAvatar = NULL;
		m_flRetryAvatarTime = gpGlobals->curtime + 0.1f;
		
		if ( m_pAvatarSpinner )
		{
			m_pAvatarSpinner->SetVisible( true );
		}
	}

	UpdateFooter();
}

void CPuzzleMakerMenu::PaintBackground( void )
{
	BaseClass::PaintBackground();

	// Because we have some oddly floating pieces in the UI, we mark our whole region as being "dirty" to avoid weird cut-offs
	// in the transition effect.
	int x, y;
	GetPos( x, y );
	BASEMODPANEL_SINGLETON.GetTransitionEffectPanel()->MarkTilesInRect( x, y, GetWide(), GetTall(), GetWindowType() );
}

void CPuzzleMakerMenu::OnKeyCodePressed( KeyCode code )
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
		BaseClass::OnKeyCodePressed( code );
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

void CPuzzleMakerMenu::FixFooter()
{
	CPuzzleMakerMenu *pSelf = static_cast<CPuzzleMakerMenu*>( CBaseModPanel::GetSingleton().GetWindow( WT_EDITORMAINMENU ) );
	if( pSelf )
	{
		pSelf->InvalidateLayout( false, true );
	}
}

void CPuzzleMakerMenu::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( !V_stricmp( "PlayTestChambers", command ) )
	{
		BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( QUEUEMODE_USER_QUEUE );
		CBaseModPanel::GetSingleton().OpenWindow( WT_COMMUNITYMAP, this, true );
	}
	else if ( !V_stricmp( "PlayCoopChambers", command ) )
	{
		BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( QUEUEMODE_USER_COOP_QUEUE );
		CBaseModPanel::GetSingleton().OpenWindow( WT_COMMUNITYMAP, this, true );
	}
	else if ( !V_stricmp( "MyChambers", command ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_EDITORCHAMBERLIST, this, true );
	}
	else if ( !V_stricmp( "MyWorkshop", command ) )
	{
#if !defined(NO_STEAM)
		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			CSteamID userID = steamapicontext->SteamUser()->GetSteamID();
			OverlayResult_t result = BASEMODPANEL_SINGLETON.ViewAuthorsWorkshop( userID );
			if( result != RESULT_OK )
			{
				if( result == RESULT_FAIL_OVERLAY_DISABLED )
				{
					GenericConfirmation* confirmation = 
						static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );
					GenericConfirmation::Data_t data;
					data.pWindowTitle = "#L4D360UI_SteamOverlay_Title";
					data.pMessageText = "#L4D360UI_SteamOverlay_Text";
					data.bOkButtonEnabled = true;
					data.pfnOkCallback = CPuzzleMakerMenu::FixFooter;
					confirmation->SetUsageData(data);
				}
			}
		}
#endif // !NO_STEAM
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CPuzzleMakerMenu::OnThink()
{
	UpdateSpinner();

	// if we didn't find the avatar right off keep trying
	if ( m_flRetryAvatarTime > 0.0f && m_flRetryAvatarTime < gpGlobals->curtime )
	{
		if ( m_pAvatar == NULL && m_pEmployeeImage != NULL && steamapicontext && steamapicontext->SteamUser() )
		{
			m_pAvatar = BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_nSteamID, BaseModUI::CUIGameData::kAvatarImageRequest, CGameUiAvatarImage::LARGE );

			if ( m_pAvatar != NULL )
			{
				m_flRetryAvatarTime = -1.0f;
				m_pEmployeeImage->SetImage( m_pAvatar );
				m_pEmployeeImage->SetVisible( true );

				if ( m_pAvatarSpinner )
				{
					m_pAvatarSpinner->SetVisible( false );
				}
			}
		}
	}

	BaseClass::OnThink();
}

void CPuzzleMakerMenu::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CPuzzleMakerMenu::UpdateSpinner( void )
{
	if ( m_pAvatarSpinner )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pAvatarSpinner->SetFrame( nAnimFrame );
	}
}

vgui::Panel *CPuzzleMakerMenu::NavigateBack( void )
{
	BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( QUEUEMODE_INVALID );
	BASEMODPANEL_SINGLETON.SetForceUseAlternateTileSet( false );
	BASEMODPANEL_SINGLETON.GetTransitionEffectPanel()->SetExpectedDirection( false, WT_EDITORMAINMENU );
	return BaseClass::NavigateBack();
}

#endif // PORTAL2_PUZZLEMAKER

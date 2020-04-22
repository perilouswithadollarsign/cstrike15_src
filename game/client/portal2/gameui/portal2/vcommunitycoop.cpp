//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include "vcommunitycoop.h"
#include "vfooterpanel.h"
#include "engineinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

CCommunityCoop::CCommunityCoop( Panel *pParent, const char *pPanelName )
			  : BaseClass( pParent, pPanelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#PORTAL2_CommunityPuzzle_CoopTitle" );

	SetFooterEnabled( true );
	UpdateFooter();
}


CCommunityCoop::~CCommunityCoop()
{
}


void CCommunityCoop::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	UpdateFooter();
}


void CCommunityCoop::Activate()
{
	BaseClass::Activate();

	UpdateFooter();
}


void CCommunityCoop::UpdateFooter( void )
{
	CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}


void CCommunityCoop::OnCommand( char const *pszCommand )
{
	if ( !V_stricmp( pszCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		return;
	}
	else if ( !V_stricmp( pszCommand, "StartPlaying" ) )
	{
		StartPlayingCoop();
		return;
	}

	BaseClass::OnCommand( pszCommand );
}


void CCommunityCoop::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		BASEMODPANEL_SINGLETON.PlayUISound( UISOUND_BACK );
		CloseUI();
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}


void CCommunityCoop::CloseUI( void )
{
	GameUI().AllowEngineHideGameUI();
	engine->ExecuteClientCmd("gameui_hide");
	CBaseModPanel::GetSingleton().CloseAllWindows();
}


void CCommunityCoop::StartPlayingCoop( void )
{
	unsigned int nNumCommunityMaps = BASEMODPANEL_SINGLETON.GetNumCommunityMapsInQueue();
	if ( nNumCommunityMaps )
	{
		const PublishedFileInfo_t *pResult = BASEMODPANEL_SINGLETON.GetCommunityMap( 0 );
		if ( pResult )
		{
			//CReliableBroadcastRecipientFilter filter;
			//filter.AddAllPlayers();
			//UserMessageBegin( filter, "CheckHasCommunityCoopMap" );
			//MessageWriteUBitLong( pResult->m_nPublishedFileId, 64 );
			//MessageEnd();
		}
	}
}


void CCommunityCoop::CheckHasMapMsgReceived( PublishedFileId_t fileID )
{
	const PublishedFileInfo_t *pFileInfo = BASEMODPANEL_SINGLETON.GetCommunityMapByFileID( fileID );
	if ( pFileInfo )
	{

	}
}


//static void __MsgFunc_CheckHasCommunityCoopMap( bf_read & msg )
//{
//	PublishedFileInfo_t mapID = msg.ReadUBitLong( 64 );
//	CCommunityCoop *pCommunityCoop = static_cast<CCommunityCoop*>( BASEMODPANEL_SINGLETON.GetWindow( WT_COMMUNITYCOOP ) );
//	pCommunityCoop->CheckHasMapMsgReceived();
//}
//USER_MESSAGE_REGISTER( CheckHasCommunityCoopMap );


void cc_cm_show_friend_invite_screen( const CCommand &args )
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( pIMatchSession && pIMatchSession->GetSessionSettings()->GetInt( "members/numPlayers" ) < 2 )
	{
		engine->ExecuteClientCmd("gameui_activate");
		BaseModUI::CBaseModFrame *pInGameMenu = BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );
		BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_PVP_LOBBY, pInGameMenu, true ); // derive from session
		GameUI().PreventEngineHideGameUI();
	}
}
static ConCommand cm_show_friend_invite_screen( "cm_show_friend_invite_screen", cc_cm_show_friend_invite_screen );


void cc_close_all_ui( const CCommand &args )
{
	BASEMODPANEL_SINGLETON.CloseAllWindows();
	GameUI().AllowEngineHideGameUI();
	engine->ExecuteClientCmd( "gameui_hide" );
}
static ConCommand cl_close_all_ui( "cl_close_all_ui", cc_close_all_ui );


void cc_cm_start_community_coop( const CCommand &args )
{
	engine->ExecuteClientCmd("gameui_activate");
	BaseModUI::CBaseModFrame *pInGameMenu = BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );
	BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_COMMUNITYCOOP, pInGameMenu, true );
	GameUI().PreventEngineHideGameUI();
}
static ConCommand cm_start_community_coop( "cm_start_community_coop", cc_cm_start_community_coop );


#endif // PORTAL2_PUZZLEMAKER

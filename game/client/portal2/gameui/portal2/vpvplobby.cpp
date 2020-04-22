//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vpvplobby.h"
#include "KeyValues.h"
#include "VGenericConfirmation.h"
#include "VGenericPanelList.h"
#include "VFooterPanel.h"
#include "gameui_util.h"
#include "GameUI/IGameUI.h"
#include "EngineInterface.h"
#include "VHybridButton.h"
#include "VGameSettings.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/RichText.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "FileSystem.h"

#include "fmtstr.h"
#include "smartptr.h"

#include "cegclientwrapper.h"

#include "globalvars_base.h"
extern CGlobalVarsBase *gpGlobals;


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

ConVar ui_pvplobby_item_highlight_normal( "ui_pvplobby_item_highlight_normal", "94 94 94 160", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_item_highlight_tracking( "ui_pvplobby_item_highlight_tracking", "160 160 160 160", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_item_highlight_invited( "ui_pvplobby_item_highlight_invited", "0 180 255 160", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_img_clrfade( "ui_pvplobby_img_clrfade", IsGameConsole() ? "160 160 160 255" : "255 255 255 200", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_img_clrrow( "ui_pvplobby_img_clrrow", "160 160 160 255", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_item_text_invited( "ui_pvplobby_item_text_invited", IsGameConsole() ? "0 180 255 255" : "0 180 255 255", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_item_text_command( "ui_pvplobby_item_text_command", "0 0 0 255", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_show_offline( "ui_pvplobby_show_offline", IsX360() ? "1" : "0", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_refresh_period( "ui_pvplobby_refresh_period", "15", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_refresh_blink( "ui_pvplobby_refresh_blink", "0.8", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_debug( "ui_pvplobby_debug", "0", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_spinner_time( "ui_pvplobby_spinner_time", "3", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_update_time( "ui_pvplobby_update_time", "1", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_animate_time( "ui_pvplobby_animate_time", "0.35", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_friends_cap( "ui_pvplobby_friends_cap", "500", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_friends_invitetime( "ui_pvplobby_friends_invitetime", "15", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_invite_fake( "ui_pvplobby_invite_fake", "0", FCVAR_DEVELOPMENTONLY );
ConVar ui_pvplobby_invite_anim( "ui_pvplobby_invite_anim", "0.5", FCVAR_DEVELOPMENTONLY );
#ifndef _CERT
extern ConVar ui_coop_map_default;
#endif

#define PVP_INVITEJOIN_XKEY KEY_XBUTTON_A
#define PVP_INVITEJOIN_FB FB_ABUTTON

#define PVP_VIEWGAMERCARD_XKEY KEY_XBUTTON_X
#define PVP_VIEWGAMERCARD_FB FB_XBUTTON

#define PVP_QUICKMATCH_XKEY KEY_XBUTTON_Y
#define PVP_QUICKMATCH_FB FB_YBUTTON


static Color ColorFromString( char const *szStringVal )
{
	int r=255,g=255,b=255,a=255;
	sscanf( szStringVal, "%d %d %d %d", &r,&g,&b,&a );
	return Color(r,g,b,a);
}

//=============================================================================

namespace BaseModUI
{



class FriendsListItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( FriendsListItem, vgui::EditablePanel );

public:
	enum Type_t
	{
		FGT_UNKNOWN,
		FGT_PLAYER,
		FGT_SYSLINK
	};

	enum Status_t
	{
		FLI_ONLINE  = ( 1 << 0 ),
		FLI_INGAME  = ( 1 << 1 ),
		FLI_VOICE	= ( 1 << 2 ),
		FLI_AWAY	= ( 1 << 3 ),
		FLI_BUSY	= ( 1 << 4 ),
	};

	enum StatusPriority_t
	{
		SPRI_CURRENT,
		SPRI_VALVE_GAME,
		SPRI_ONLINE,
		SPRI_OFFLINE
	};

	struct Info
	{
		Type_t mInfoType;
		char Name[64];
		XUID mFriendXUID, mOnlineXUID;
		uint32 uiStatus;
		uint64 uiAppId;
		StatusPriority_t ePriority;

		Info()
		{
			mInfoType = FGT_UNKNOWN;
			Name[0] = 0;
			mFriendXUID = 0;
		}
	};

public:
	FriendsListItem( vgui::Panel *parent, const char *panelName ) : BaseClass( parent, panelName ),
		m_pListCtrlr( ( GenericPanelList * ) parent )
	{
		SetProportional( true );
		SetPaintBackgroundEnabled( true );

		m_sweep = 0;
		m_bSelected = false;
		m_bHasMouseover = false;
		m_bInvited = false;
		m_flInviteMouseOver = 0;
		m_flInviteTime = 0;
		m_pAvatarImage = NULL;
		m_xuidAvatarImage = 0ull;

		m_TextColor = Color( 0, 0, 0, 255 );
		m_FocusColor = Color( 0, 0, 0, 255 );
		m_CursorColor = Color( 0, 0, 0, 255 );
		
		m_hTextFont = vgui::INVALID_FONT;
		m_hSmallTextFont = vgui::INVALID_FONT;
		m_hButtonFont = vgui::INVALID_FONT;
	}

	~FriendsListItem()
	{
		if ( m_pAvatarImage )
		{
			CUIGameData::Get()->AccessAvatarImage( m_xuidAvatarImage, CUIGameData::kAvatarImageRelease );
			m_pAvatarImage = NULL;
			m_xuidAvatarImage = 0ull;
		}
	}

public:
	void SetGameIndex( const Info& fi )
	{
		m_FullInfo = fi;
		SetAvatarXUID( fi.mFriendXUID );
		SetGamerTag( fi.Name );
		SetPlayerStatusLine();
	}
	const Info& GetFullInfo()
	{
		return m_FullInfo;
	}

	void SetGamerTag( char const* gamerTag )
	{
		if ( vgui::Label *pLblPlayerGamerTag = dynamic_cast< vgui::Label * > ( FindChildByName( "LblGamerTag" ) ) )
		{
			if ( GetFullInfo().mFriendXUID )
			{
				wchar_t wszName[sizeof( GetFullInfo().Name )+1];
				g_pVGuiLocalize->ConvertANSIToUnicode( gamerTag ? gamerTag : "", wszName, sizeof( wszName ) );
				pLblPlayerGamerTag->SetText( wszName );
			}
			else if ( GetFullInfo().Name[0] == '#' )
			{
				pLblPlayerGamerTag->SetText( GetFullInfo().Name[1] == '0' ? "#Portal2UI_pvp_NoFriendsPSN1" : "#Portal2UI_pvp_NoFriendsSteam1" );
			}
		}
	}

	void SetPlayerStatusLine()
	{
		if ( vgui::Label *pLblPlayerStatus = dynamic_cast< vgui::Label * > ( FindChildByName( "LblGamerTagStatus" ) ) )
		{
			if ( GetFullInfo().mFriendXUID )
			{
				char const *fmtKey = NULL;
				switch ( GetFullInfo().ePriority )
				{
				case SPRI_OFFLINE: default: fmtKey = "#Portal2UI_pvp_FriendInfo_Offline"; break;
				case SPRI_ONLINE:
					if ( GetFullInfo().uiStatus & FLI_AWAY )
						fmtKey = "#Portal2UI_pvp_FriendInfo_Away";
					else if ( GetFullInfo().uiStatus & FLI_BUSY )
						fmtKey = "#Portal2UI_pvp_FriendInfo_Busy";
					else if ( GetFullInfo().uiStatus & FLI_INGAME )
						fmtKey = "#Portal2UI_pvp_FriendInfo_InGame";
					else
						fmtKey = "#Portal2UI_pvp_FriendInfo_Online";
					break;
				case SPRI_VALVE_GAME:
					switch( GetFullInfo().uiAppId )
					{
					case 0x454108D4: fmtKey = "#Portal2UI_pvp_FriendInfo_PlayingL4D2"; break;
					case 0x45410830: fmtKey = "#Portal2UI_pvp_FriendInfo_PlayingL4D1"; break;
					case 0x4541080F: fmtKey = "#Portal2UI_pvp_FriendInfo_PlayingOB"; break;
					default: fmtKey = "#Portal2UI_pvp_FriendInfo_InGame"; break;
					} break;
				case SPRI_CURRENT: fmtKey = "#Portal2UI_pvp_FriendInfo_PlayingPortal2"; break;
				}
				wchar_t wszText[256] = {0};
				if ( m_bInvited )
				{
					wchar_t const *wszKey = g_pVGuiLocalize->Find( fmtKey );
					wchar_t const *wszSuffix = g_pVGuiLocalize->Find( "#Portal2UI_pvp_Invite_Status" );
					if ( wszKey && wszSuffix )
					{
						Q_snwprintf( wszText, ARRAYSIZE( wszText ), PRI_WS_FOR_WS PRI_WS_FOR_WS, wszKey, wszSuffix );
					}
				}
				if ( wszText[0] )
					pLblPlayerStatus->SetText( wszText, true );
				else
					pLblPlayerStatus->SetText( fmtKey );
			}
			else if ( GetFullInfo().Name[0] == '#' )
			{
				pLblPlayerStatus->SetText( GetFullInfo().Name[1] == '0' ? "#Portal2UI_pvp_NoFriendsPSN2" : "#Portal2UI_pvp_NoFriendsSteam2" );
			}
		}
	}
	void SetAvatarXUID( XUID xuid )
	{
		vgui::ImagePanel *imgAvatar = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
		if ( !imgAvatar )
			return;

		if ( !m_pAvatarImage )
		{
			if ( xuid && ( m_FullInfo.mInfoType == FriendsListItem::FGT_PLAYER ) )
				m_pAvatarImage = CUIGameData::Get()->AccessAvatarImage( xuid, CUIGameData::kAvatarImageRequest );
			if ( m_pAvatarImage )
			{
				imgAvatar->SetImage( m_pAvatarImage );
				m_xuidAvatarImage = xuid;
			}
			else
			{
				imgAvatar->SetImage( "icon_lobby" );
			}
			imgAvatar->SetVisible( xuid != 0ull );
		}

#ifndef NO_STEAM
		vgui::ImagePanel *imgAvatarBorder = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPicBorder" ) );
		if ( xuid && imgAvatarBorder )
		{
			bool bMoveToFront = true;
			char const *szImageFile = "steam_avatar_border_online";
			if ( m_FullInfo.uiStatus & FLI_INGAME )
				szImageFile = "steam_avatar_border_ingame";
#ifdef _PS3
			if ( CSteamID( xuid ).BConsoleUserAccount() )
			{
				bMoveToFront = false;
				szImageFile = "steam_avatar_border_psn";
			}
#endif
			if ( bMoveToFront )
				imgAvatarBorder->MoveToFront();
			imgAvatarBorder->SetImage( szImageFile );
			imgAvatarBorder->SetVisible( true );
		}
#endif
	}

	enum Flags_t
	{
		// FONT: NORMAL
		DLIL_FONT_NORMAL = 0,
		DLIL_FONT_SMALL = ( 1 << 0 ),
		DLIL_FONT_BUTTON = ( 1 << 1 ),
		// ALIGN: DLIL_ALIGN_WEST,
		DLIL_ALIGN_WEST = 0,
		DLIL_ALIGN_EAST = ( 1 << 2 ),
		DLIL_ALIGN_HCENTER = ( 1 << 3 ),
		// COLOR: DLIL_COLOR_DEFAULT
		DLIL_COLOR_DEFAULT = 0,
		DLIL_COLOR_NORMAL = ( 1 << 4 ),
		DLIL_COLOR_FOCUS = ( 1 << 5 ),
		// FOCUS: DLIL_FOCUS_DEFAULT
		DLIL_FOCUS_DEFAULT = 0,
		DLIL_FOCUS_NONE = ( 1 << 6 ),
	};
	struct DrawListItemLabelSettings_t
	{
		uint32 uiFlags;
		Color clrNormal;
		Color clrFocus;
	};
	void DrawListItemLabel( vgui::Label* label, DrawListItemLabelSettings_t const &settings );

	void SetSweep( bool sweep ) { m_sweep = sweep; }
	bool IsSweep() const { return m_sweep != 0; }

	virtual void PaintBackground();

	CEG_NOINLINE void OnKeyCodePressed( vgui::KeyCode code )
	{
		int iUserSlot = GetJoystickForCode( code );
		CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );
		switch( GetBaseButtonCode( code ) )
		{
		case PVP_INVITEJOIN_XKEY: SendInvite(); return;
		case PVP_VIEWGAMERCARD_XKEY: ViewGamercard(); return;
		case PVP_QUICKMATCH_XKEY:
			if ( PvpLobby *pLobby = ( PvpLobby * ) CBaseModPanel::GetSingleton().GetWindow( WT_PVP_LOBBY ) )
				pLobby->QuickMatch();
			return;

		case KEY_XBUTTON_LEFT:
		case KEY_XBUTTON_LEFT_SHOULDER:
		case KEY_XBUTTON_LTRIGGER:
		case KEY_XSTICK1_LEFT:
		case KEY_XSTICK2_LEFT:
			if ( PvpLobby *pLobby = ( PvpLobby * ) CBaseModPanel::GetSingleton().GetWindow( WT_PVP_LOBBY ) )
				pLobby->ActivateFriendsList( 0 );
			return;

		case KEY_XBUTTON_RIGHT:
		case KEY_XBUTTON_RIGHT_SHOULDER:
		case KEY_XBUTTON_RTRIGGER:
		case KEY_XSTICK1_RIGHT:
		case KEY_XSTICK2_RIGHT:
			if ( PvpLobby *pLobby = ( PvpLobby * ) CBaseModPanel::GetSingleton().GetWindow( WT_PVP_LOBBY ) )
				pLobby->ActivateFriendsList( 1 );
			return;

		case KEY_ENTER:
			SendInvite();
			return;
		}

		CEG_PROTECT_MEMBER_FUNCTION( FriendsListItem_OnKeyCodePressed );

		BaseClass::OnKeyCodePressed( code );
	}

	void OnKeyCodeTyped( vgui::KeyCode code ) { BaseClass::OnKeyCodeTyped( code ); }

	CEG_NOINLINE void OnMousePressed( vgui::MouseCode code )
	{
		CEG_PROTECT_MEMBER_FUNCTION( FriendsListItem_OnMousePressed );
		if ( code == MOUSE_LEFT )
		{
			m_pListCtrlr->SelectPanelItemByPanel( this );

			if ( vgui::ImagePanel *pInvite = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "Cmd_INVITE" ) ) )
			{
				int x, y, w, h;
				if ( GetHighlightBounds( pInvite, x,y,w,h ) )
				{
					SendInvite();
					// pInvite->SetText( "#Portal2UI_pvp_Invite_Item_Invited" );
					pInvite->SetImage( "invited" );
					m_flInviteMouseOver = ui_pvplobby_invite_anim.GetFloat();
				}
				else if ( GetHighlightBounds( FindChildByName( "PnlGamerPic" ), x,y,w,h ) )
				{
					char steamCmd[64];
					Q_snprintf( steamCmd, sizeof( steamCmd ), "steamid/%llu", GetFullInfo().mFriendXUID );
					CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
				}
			}
		}
		BaseClass::OnMousePressed( code );
	}

	CEG_NOINLINE void SendInvite()
	{
#ifdef _X360
		IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pIMatchSession ) return;
		if ( !Q_stricmp( pIMatchSession->GetSessionSettings()->GetString( "system/network", "LIVE" ), "LIVE" ) )
		{
			// Check user privileges
			if ( !CUIGameData::Get()->CanSendLiveGameInviteToUser( GetFullInfo().mFriendXUID ) )
			{
				if ( PvpLobby *pLobby = ( PvpLobby * ) CBaseModPanel::GetSingleton().GetWindow( WT_PVP_LOBBY ) )
				{
					pLobby->AllowFriendsAccess();
				}
				else
				{
					DevWarning( "Cannot send invite to %s\n", GetFullInfo().Name );
				}
				return;
			}

			// Send the invite
			if ( m_flInviteTime && ( ( Plat_FloatTime() - m_flInviteTime ) < ui_pvplobby_friends_invitetime.GetFloat() ) ) // rate limit invites
				return;
			DevMsg( "XInviteSend: %s\n", GetFullInfo().Name );
			m_flInviteTime = Plat_FloatTime();
			wchar_t const *wszText = g_pVGuiLocalize->Find( "#Portal2UI_pvp_Invite_Text" );
			DWORD dwResult = xonline->XInviteSend( XBX_GetPrimaryUserId(), 1, &GetFullInfo().mFriendXUID, wszText ? wszText : L"", NULL );
			if ( dwResult != ERROR_SUCCESS )
				DevMsg( "XInviteSend error 0x%08X\n", dwResult );
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		}
		else
		{
			DevMsg( "Join the game: %s\n", GetFullInfo().Name );
			IPlayerFriend *item = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetFriendByXUID( GetFullInfo().mFriendXUID );
			if ( item )
			{
				CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
				item->Join();
			}
		}
#elif !defined( NO_STEAM )
		if ( !GetFullInfo().mFriendXUID )
			return;
		if ( m_flInviteTime && ( ( Plat_FloatTime() - m_flInviteTime ) < ui_pvplobby_friends_invitetime.GetFloat() ) ) // rate limit invites
			return;
		DevMsg( "Invite: %s\n", GetFullInfo().Name );

		CEG_PROTECT_MEMBER_FUNCTION( FriendsListItem_SendInvite );

		m_flInviteTime = Plat_FloatTime();
		if ( !ui_pvplobby_invite_fake.GetBool() )
		{
			steamapicontext->SteamMatchmaking()->InviteUserToLobby(
				( g_pMatchFramework->GetMatchSession() ? g_pMatchFramework->GetMatchSession()->GetSessionSystemData() : NULL )
				->GetUint64( "xuidReserve" ), GetFullInfo().mFriendXUID );
		}
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
#endif
		m_bInvited = true;
		SetPlayerStatusLine();
	}

	void ViewGamercard()
	{
		if ( IsPS3() ) return; // gamercards not supported on PS3
		IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pIMatchSession ) return;
		IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
		if ( !pPlayerLocal || ( pPlayerLocal->GetOnlineState() == IPlayer::STATE_OFFLINE ) ) return;
		XUID xuidGamercard = GetFullInfo().mFriendXUID;
		if ( Q_stricmp( pIMatchSession->GetSessionSettings()->GetString( "system/network", "LIVE" ), "LIVE" ) )
			xuidGamercard = GetFullInfo().mOnlineXUID;
		if ( xuidGamercard )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
#ifdef _X360
			XShowGamerCardUI( XBX_GetPrimaryUserId(), xuidGamercard );
#else
			char steamCmd[64];
			Q_snprintf( steamCmd, sizeof( steamCmd ), "steamid/%llu", xuidGamercard );
			CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
#endif
		}
	}

	CEG_NOINLINE void OnMouseDoublePressed( vgui::MouseCode code )
	{
		CEG_PROTECT_MEMBER_FUNCTION( FriendsListItem_OnMouseDoublePressed );
		SendInvite();
	}

	virtual void OnCursorEntered()
	{ 
		if( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();
	}

	virtual void OnCursorExited() { SetHasMouseover( false ); }

	virtual void NavigateTo( void )
	{
		m_pListCtrlr->SelectPanelItemByPanel( this );
#if !defined( _GAMECONSOLE )
		SetHasMouseover( true );
		RequestFocus();
#endif
		BaseClass::NavigateTo();
	}

	virtual void NavigateFrom( void )
	{
		SetHasMouseover( false );
		BaseClass::NavigateFrom();
#ifdef _GAMECONSOLE
		OnClose();
#endif
	}

	bool GetHighlightBounds( vgui::Panel *pCmd, int &x, int &y, int &w, int &h )
	{
		if ( !pCmd )
			return false;

		pCmd->GetPos( x, y);
		pCmd->GetSize( w, h );

		/*
		if ( vgui::Panel *pBG = FindChildByName( "PnlGamerPic" ) )
		{
			int bgX, bgY;
			int bgW, bgH;
			pBG->GetPos( bgX, bgY );
			pBG->GetSize( bgW, bgH );

			y = bgY;
			h = bgH;
		}
		*/

		int cX, cY;
		surface()->SurfaceGetCursorPos( cX, cY );
		this->ScreenToLocal( cX, cY );

		return ( ( cX > x ) && ( cX < (x + w) ) && ( cY > y ) && ( cY < ( y + h ) ) );
	}

	bool IsSelected( void ) { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover( void ) { return m_bHasMouseover; }
	void SetHasMouseover( bool bHasMouseover ) { m_bHasMouseover = bHasMouseover; }

protected:
	void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		KeyValues *pResLoadConditions = NULL;
		if ( PvpLobby *pLobby = ( PvpLobby * ) CBaseModPanel::GetSingleton().GetWindow( WT_PVP_LOBBY ) )
		{
			pResLoadConditions = pLobby->GetResourceLoadConditions();
		}
		LoadControlSettings( CFmtStr( "Resource/UI/BaseModUI/%s.res", GetName() ), NULL, NULL, pResLoadConditions );

		m_hTextFont = pScheme->GetFont( "FriendsList", true );
		m_hSmallTextFont = pScheme->GetFont( IsGameConsole() ? "FriendsListSmall" : "FriendsListStatusLine", true );
		m_hButtonFont = pScheme->GetFont( pScheme->GetResourceString( "FooterPanel.TextFont" ), true );

#ifdef PORTAL2_PUZZLEMAKER
		if ( BASEMODPANEL_SINGLETON.ForceUseAlternateTileSet() )
		{
			m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
			m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
			//m_CursorColor = GetSchemeColor( "HybridButton.CursorColorAlt", pScheme );
			m_CursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
			//m_LockedColor = Color( 64, 64, 64, 128 ); //GetSchemeColor( "HybridButton.LockedColor", pScheme );
			//m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
			//m_LostFocusColor = m_CursorColor;
			//m_LostFocusColor.SetColor( m_LostFocusColor.r(), m_LostFocusColor.g(), m_LostFocusColor.b(), 50); //Color( 120, 120, 120, 255 );
			//m_BaseColor = Color( 255, 255, 255, 0 );
		}
		else
#endif // PORTAL2_PUZZLEMAKER
		{
			m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
			m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
			m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
		}

		SetGameIndex( m_FullInfo );
	}

	void PerformLayout()
	{
		BaseClass::PerformLayout();
		// set all our children (image panel and labels) to not accept mouse input so they
		// don't eat any mouse input and it all goes to us
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			Panel *panel = GetChild( i );
			Assert( panel );
			panel->SetMouseInputEnabled( false );
		}
	}

private:
	Info m_FullInfo;

	Color m_OutOfFocusBgColor;
	Color m_FocusBgColor;

	GenericPanelList	*m_pListCtrlr;

	vgui::HFont	m_hTextFont;
	vgui::HFont	m_hSmallTextFont;
	vgui::HFont m_hButtonFont;
	
	Color	m_TextColor;
	Color	m_FocusColor;
	Color	m_CursorColor;

	vgui::IImage *m_pAvatarImage;
	XUID m_xuidAvatarImage;

	CPanelAnimationVar( Color, m_SelectedColor, "selected_color", "255 0 0 128" );
	bool m_sweep : 1;
	bool m_bSelected : 1;
	bool m_bHasMouseover : 1;
	bool m_bInvited : 1;
	float m_flInviteTime;
	float m_flInviteMouseOver;

};

void FriendsListItem::DrawListItemLabel( vgui::Label* label, DrawListItemLabelSettings_t const &settings )
{
	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	if ( label )
	{
		bool bHasFocus = HasMouseover() ||
			( !IsGameConsole() && HasFocus() ) ||
			( IsGameConsole() && ( HasFocus() || IsSelected() ) );
		
		if ( settings.uiFlags & DLIL_FOCUS_NONE )
			bHasFocus = false;

		Color col = ( settings.uiFlags & DLIL_COLOR_NORMAL ) ? settings.clrNormal : m_TextColor;
		if ( bHasFocus )
		{
			col = ( settings.uiFlags & DLIL_COLOR_FOCUS ) ? settings.clrFocus : m_FocusColor;
		}

		int x, y;
		wchar_t szUnicode[512];

		label->GetText( szUnicode, sizeof( szUnicode ) );
		label->GetPos( x, y );

		HFont drawFont = m_hTextFont;
		if ( settings.uiFlags & DLIL_FONT_SMALL )
			drawFont = m_hSmallTextFont;
		else if ( settings.uiFlags & DLIL_FONT_BUTTON )
			drawFont = m_hButtonFont;

		int len = V_wcslen( szUnicode );
		int textWide, textTall;
		surface()->GetTextSize( drawFont, szUnicode, textWide, textTall );	// this is just ballpark numbers as they don't count & characters

		int labelWide = 0, labelTall = label->GetTall();
		if ( settings.uiFlags & DLIL_ALIGN_HCENTER )
		{
			labelWide = label->GetWide();
		}
		else
		{
			// If we drew labels properly I wouldn't be here on a saturday writing code like this:
			// Cannot ask surface about whole text size as it will skip & characters that can be
			// in player names
			int lblxparent, lblyparent;
			label->GetPos( lblxparent, lblyparent );
			labelWide = label->GetParent()->GetWide() - lblxparent - 5;
		}
		if ( labelWide > 0 )
		{
			textWide = 0;
			HFont wideFont = drawFont;
			for ( int i=0;i<len;i++ )
			{
				textWide += surface()->GetCharacterWidth( wideFont, szUnicode[i] );

				if ( textWide > labelWide )
				{
					/*
					int dotWide = surface()->GetCharacterWidth( wideFont, '.' );
					for ( int k = 3; k -- > 0; )
					{
						if ( i > k )
						{
							textWide += dotWide - surface()->GetCharacterWidth( wideFont, szUnicode[i-k-1] );
							szUnicode[i-k-1] = '.';
						}
					}
					*/
					textWide -= surface()->GetCharacterWidth( wideFont, szUnicode[i] );

					szUnicode[i] = 0;
					len = i;

					break;
				}
			}
		}

		// vertical center
		y += ( labelTall - textTall ) / 2;

		if ( settings.uiFlags & DLIL_ALIGN_EAST )
		{
			x += labelWide - textWide;
		}
		else if ( settings.uiFlags & DLIL_ALIGN_HCENTER )
		{
			x += (labelWide - textWide)/2;
		}

		vgui::surface()->DrawSetTextFont( drawFont );
		vgui::surface()->DrawSetTextPos( x, y );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawPrintText( szUnicode, len );
	}
}

void FriendsListItem::PaintBackground()
{
	if ( !m_pListCtrlr->IsPanelItemVisible( this ) )
		return;

	// if we're hilighted, background
	if ( ( IsGameConsole() && ( HasFocus() || IsSelected() ) ) ||
		( !IsGameConsole() && HasFocus() ) )
	{
		int x, y, wide, tall;
		GetBounds( x, y, wide, tall );

		surface()->DrawSetColor( m_CursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}

	vgui::ImagePanel *pImgAvatar = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
	if ( pImgAvatar )
	{
		Color color;
		if ( HasFocus() || ( IsGameConsole() && IsSelected() ) )
		{
			color = Color( 255, 255, 255, 255 );
			if ( !IsGameConsole() )
			{
				int x, y, w, h;
				if ( !GetHighlightBounds( pImgAvatar, x, y, w, h ) )
				{
					color = ColorFromString( ui_pvplobby_img_clrrow.GetString() );
				}
			}
		}
		else
		{
			color = ColorFromString( ui_pvplobby_img_clrfade.GetString() );
		}
		pImgAvatar->SetDrawColor( color );
	}

	DrawListItemLabelSettings_t dlils;
	dlils.uiFlags = DLIL_FONT_NORMAL;
	if ( m_bInvited )
	{
		dlils.uiFlags |= DLIL_COLOR_NORMAL | DLIL_COLOR_FOCUS;
		dlils.clrNormal = dlils.clrFocus = ColorFromString( ui_pvplobby_item_text_invited.GetString() );
	}
	DrawListItemLabel( dynamic_cast< vgui::Label * >( FindChildByName( "LblGamerTag" ) ), dlils );

	dlils.uiFlags = DLIL_FONT_SMALL;
	if ( m_bInvited )
	{
		dlils.uiFlags |= DLIL_COLOR_NORMAL | DLIL_COLOR_FOCUS;
		dlils.clrNormal = dlils.clrFocus = ColorFromString( ui_pvplobby_item_text_invited.GetString() );
	}
	DrawListItemLabel( dynamic_cast< vgui::Label * > ( FindChildByName( "LblGamerTagStatus" ) ), dlils );

	/*
	if ( vgui::Label *pInvite = dynamic_cast< vgui::Label * >( FindChildByName( "Cmd_INVITE" ) ) )
	{
		int x, y, w, h;
		bool bTrackingCursor = GetHighlightBounds( pInvite, x,y,w,h );
		Color clr = ColorFromString( bTrackingCursor ? ui_pvplobby_item_highlight_tracking.GetString() : ui_pvplobby_item_highlight_normal.GetString() );
		if ( !m_bInvited || HasFocus() )
		{
			surface()->DrawSetColor( clr );
			// surface()->DrawFilledRectFade( (x + w) / 2, y, x + w, y + h, clr.a(), 0, true );
			// surface()->DrawFilledRectFade( x, y, (x + w) / 2, y + h, 0, clr.a(), true );
			surface()->DrawFilledRect( x, y, x + w, y + h );
		}

		dlils.uiFlags = DLIL_ALIGN_HCENTER | DLIL_COLOR_NORMAL | DLIL_FONT_BUTTON | ( m_bInvited ? DLIL_COLOR_FOCUS : 0 );
		dlils.clrFocus = dlils.clrNormal = ( m_bInvited ? ColorFromString( ui_pvplobby_item_text_invited.GetString() ) :
			ColorFromString( ui_pvplobby_item_text_command.GetString() ) );
		DrawListItemLabel( pInvite, dlils );
	}
	*/
	if ( vgui::ImagePanel *pInvite = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "Cmd_INVITE" ) ) )
	{
		char const *szDesiredImage = "btn_invite";
		int x, y, w, h;
		bool bTrackingCursor = GetHighlightBounds( pInvite, x,y,w,h );
		if ( m_bInvited )
		{
			if ( bTrackingCursor )
			{
				m_flInviteMouseOver -= gpGlobals->absoluteframetime;
				if ( m_flInviteMouseOver <= 0 )
				{
					// Switch to another icon
					m_flInviteMouseOver = ui_pvplobby_invite_anim.GetFloat();
					szDesiredImage = V_strcmp( pInvite->GetImageName(), "invited" ) ? "invited" : "btn_invite_over";
				}
				else
				{
					szDesiredImage = NULL;
				}
			}
			else
			{
				m_flInviteMouseOver = ui_pvplobby_invite_anim.GetFloat();
				szDesiredImage = "invited";
			}
		}
		else if ( bTrackingCursor )
		{
			szDesiredImage = "btn_invite_over";
		}
		if ( szDesiredImage )
		{
			pInvite->SetImage( szDesiredImage );
		}
	}
}




// Destroying the lobby without any confirmations
static void LeaveLobbyImpl()
{
	g_pMatchFramework->CloseSession();

#ifdef PORTAL2_PUZZLEMAKER
	if ( BASEMODPANEL_SINGLETON.IsCommunityCoop() )
	{
		BASEMODPANEL_SINGLETON.MoveToCommunityMapQueue();
	}
#endif // PORTAL2_PUZZLEMAKER

	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
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


//////////////////////////////////////////////////////////////////////////
//
// Game lobby implementation
//

CEG_NOINLINE PvpLobby::PvpLobby(vgui::Panel *parent, const char *panelName, KeyValues *pSettings ) :
	BaseClass( parent, panelName, true, true, false ),
#ifndef NO_STEAM
	m_CallbackPersonaStateChanged( this, &PvpLobby::Steam_OnPersonaStateChanged ),
#endif
	m_pSettings( NULL ),
	m_nMsgBoxId( 0 ),
	m_xuidPlayerFlyout( 0 ),
	m_bNoCommandHandling( false ),
	m_bSubscribedForEvents( false ),
	m_pLobbyDetailsLayout( NULL ),
	m_autodelete_pLobbyDetailsLayout( m_pLobbyDetailsLayout ),
	m_autodelete_pResourceLoadConditions( (KeyValues*) NULL ),
	m_nRefreshListCap( 0 ),
	m_flLastRefreshTime( 0 ),
	m_flAutoRefreshTime( 0 ),
	m_flSearchStartedTime( 0 ),
	m_flSearchEndTime( 0 ),
	m_eAddFriendsRule( ADD_FRIENDS_ALL )
{
	memset( m_StateText, 0, sizeof( m_StateText ) );
	memset( m_FormattedStateText, 0, sizeof( m_FormattedStateText ) );

	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetFooterEnabled( true );

	m_pListFriendsArray[0] = new GenericPanelList( this, "ListFriends", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pListFriendsArray[0]->SetPaintBackgroundEnabled( false );

	if ( IsPS3() )
	{
		m_pListFriendsArray[1] = new GenericPanelList( this, "ListFriendsSteam", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
		m_pListFriendsArray[1]->SetPaintBackgroundEnabled( false );
	}
	else
	{
		m_pListFriendsArray[1] = NULL;
	}

	m_pListFriends = m_pListFriendsArray[0];

	m_bAnimatingLists = false;
	m_flAnimationTimeStamp = 0;
	m_nAnimatingTargetWidth[0] = 0;
	m_nAnimatingTargetWidth[1] = 0;
	m_nAnimatingSourceWidth[0] = 0;
	m_nAnimatingSourceWidth[1] = 0;

	SetupLobbySettings( pSettings );

	CEG_PROTECT_MEMBER_FUNCTION( PvpLobby_PvpLobby );
}

PvpLobby::~PvpLobby()
{
	if ( m_bSubscribedForEvents )
	{
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
		m_bSubscribedForEvents = false;
	}

	for ( int k = 0; k < ARRAYSIZE( m_pListFriendsArray ); ++ k )
	{
		delete m_pListFriendsArray[k];
	}
}

void PvpLobby::SetupLobbySettings( KeyValues *pSettings )
{
	// Maintain a reference to session settings
	m_pSettings = pSettings;

	Assert( !m_pResourceLoadConditions );
	m_pResourceLoadConditions = new KeyValues( "pvplobby" );
	m_autodelete_pResourceLoadConditions.Assign( m_pResourceLoadConditions );

	char const *szNetwork = m_pSettings->GetString( "system/network", "" );
	if ( !Q_stricmp( szNetwork, "LIVE" ) )
		m_pResourceLoadConditions->SetInt( "?network_live", 1 );
	else if ( !Q_stricmp( szNetwork, "lan" ) )
		m_pResourceLoadConditions->SetInt( "?network_lan", 1 );

	if ( IsPS3() )
	{
		// const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
		// SetDialogTitle( NULL, L"", false, 0, 0, aspectRatioInfo.m_bIsWidescreen ? 10 : 10 );
		SetDialogTitle( NULL, L"", false, 0, 0, 9 );
	}
	else if ( !IsGameConsole() )
	{
		SetDialogTitle( "#L4D360UI_InviteUI_friends" );
	}
	else
	{
		const char *pTitle = !Q_stricmp( pSettings->GetString( "system/network", "LIVE" ), "LIVE" ) ? "#Portal2UI_PlayOnline" : "#Portal2UI_PlayLan";
		SetDialogTitle( pTitle );
	}

#ifdef _PS3
	char chBuffer1[2048];
	Q_strncpy( chBuffer1, g_pVGuiLocalize->FindAsUTF8( "#P2COOP_Invite_Title" ), sizeof( chBuffer1 ) );
	steamapicontext->SteamUtils()->SetPSNGameBootInviteStrings(
		chBuffer1,
		g_pVGuiLocalize->FindAsUTF8( "#P2COOP_Invite_Body" )
		);
#endif

#ifdef _X360
	if ( !m_pResourceLoadConditions->GetInt( "?network_lan" ) )
	{
		m_nRefreshListCap = 1;
	}
#endif
}

CEG_NOINLINE void PvpLobby::OnCommand(const  char *command )
{
	CEG_PROTECT_MEMBER_FUNCTION( PvpLobby_OnCommand );

	if ( m_bNoCommandHandling )
		return;

	if ( ! Q_strcmp( command, "Back" ) )
	{
		LeaveLobby();
	}
	else if ( !Q_strcmp( command, "BtnQuickMatch" ) )
	{
		QuickMatch();
	}
	else if ( char const *szInviteType = StringAfterPrefix( command, "InviteUI_" ) )
	{
		FlyoutMenu::CloseActiveMenu();
		if ( IsX360() )
		{
			CUIGameData::Get()->OpenInviteUI( szInviteType );
		}
		else
		{
			CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void PvpLobby::OnThink()
{
	ClockSpinner();
	UpdateFooterButtons();

	if ( FindChildByName( "ListFriendsStatus" ) )
	{
		bool bAutoRefreshed = false;
		if ( ( m_nRefreshListCap > 0 ) || !m_flLastRefreshTime || ( ( Plat_FloatTime() - m_flLastRefreshTime ) > ui_pvplobby_refresh_period.GetFloat() ) )
		{
			if ( UpdateFriendsList() && !IsGameConsole() )
			{
				m_flAutoRefreshTime = m_flLastRefreshTime;
				m_pListFriends->SetVisible( false );
				bAutoRefreshed = true;
			}
		}
		if ( m_flAutoRefreshTime && !bAutoRefreshed && ( ( Plat_FloatTime() - m_flAutoRefreshTime ) > ui_pvplobby_refresh_blink.GetFloat() ) )
		{
			m_pListFriends->SetVisible( true );
			m_flAutoRefreshTime = 0;
			
			if ( !IsGameConsole() && HasFocus() )
			{
				OnItemSelected( "" );
				RequestFocus();
				m_pListFriends->NavigateTo();
			}
		}
	}

	if ( m_bAnimatingLists )
	{
		if ( !m_nAnimatingTargetWidth[0] || !m_nAnimatingTargetWidth[1] )
		{
			m_nAnimatingTargetWidth[0] = m_pListFriendsArray[0]->GetWide();
			m_nAnimatingTargetWidth[1] = m_pListFriendsArray[1]->GetWide();
		}

		int iWide0target = ( m_pListFriends == m_pListFriendsArray[ADD_FRIENDS_PSN] ) ? m_nAnimatingTargetWidth[0] : m_nAnimatingTargetWidth[1];
		// int iWide1target = ( m_pListFriends == m_pListFriendsArray[ADD_FRIENDS_STEAM] ) ? m_nAnimatingTargetWidth[0] : m_nAnimatingTargetWidth[1];

		float flAnimationFactor = ( Plat_FloatTime() - m_flAnimationTimeStamp ) / ui_pvplobby_animate_time.GetFloat();

		int iWide0 = m_nAnimatingSourceWidth[0] + flAnimationFactor * ( iWide0target - m_nAnimatingSourceWidth[0] );
		iWide0 = MIN( iWide0, m_nAnimatingTargetWidth[0] );
		iWide0 = MAX( iWide0, m_nAnimatingTargetWidth[1] );

		m_pListFriendsArray[ADD_FRIENDS_PSN]->SetWide( iWide0 );
		m_pListFriendsArray[ADD_FRIENDS_STEAM]->SetWide( m_nAnimatingTargetWidth[0] + m_nAnimatingTargetWidth[1] - iWide0 );
		int x, y;
		m_pListFriendsArray[ADD_FRIENDS_STEAM]->GetPos( x, y );
		m_pListFriendsArray[ADD_FRIENDS_STEAM]->SetPos( iWide0, y );

		if ( vgui::ImagePanel *imgSteamLogo = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "SteamTitleLogo" ) ) )
		{
			imgSteamLogo->GetPos( x, y );
			imgSteamLogo->SetPos( iWide0, y );
		}
		if ( vgui::ImagePanel *imgListsSeparator = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ListsSeparator" ) ) )
		{
			imgListsSeparator->GetPos( x, y );
			imgListsSeparator->SetPos( iWide0 - imgListsSeparator->GetWide(), y );
		}

		if ( flAnimationFactor > 1.0f )
			m_bAnimatingLists = false; // animation finished
	}

	BaseClass::OnThink();
}

void PvpLobby::NavigateToChild( Panel *pNavigateTo )
{
	for( int i = 0; i != GetChildCount(); ++i )
	{
		vgui::Panel *pChild = GetChild(i);
		if( pChild )
			pChild->NavigateFrom();
	}

	// TODO: NavigateTo_ChatHandler( pNavigateTo );
}

void PvpLobby::OnKeyCodePressed( vgui::KeyCode code )
{
	int userID = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( userID );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B: // the player is leaving the lobby, so cleanup
		LeaveLobby();
		return;

	case PVP_QUICKMATCH_XKEY:
		QuickMatch();
		return;

	case PVP_INVITEJOIN_XKEY:
		AllowFriendsAccess();
		return;

#if !defined( _GAMECONSOLE )
	case KEY_XBUTTON_LEFT_SHOULDER:
		engine->ExecuteClientCmd( "open_econui" );
		return;
#endif

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void PvpLobby::OnOpen()
{
	SetVisible( true );
	BaseClass::OnOpen();

	m_pListFriends->SetScrollBarVisible( !IsGameConsole() );

	if ( m_pResourceLoadConditions->GetInt( "?network_lan" ) )
	{
		g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->EnableFriendsUpdate( true );	// force a system link probe
	}

	if ( !m_flLastRefreshTime )
	{
		UpdateFriendsList();
	}
}

void PvpLobby::OnClose()
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

void PvpLobby::MsgNoLongerHosting( KeyValues *pSettings )
{
	LeaveLobbyImpl();	// Lobby is no valid at this point

	KeyValues *kvDlcNotify = new KeyValues( "OnMatchSessionUpdate" );
	KeyValues::AutoDelete autodelete_kvDlcNotify( kvDlcNotify );

	kvDlcNotify->SetString( "state", "error" );
	kvDlcNotify->SetString( "error", "n/a" );

	CUIGameData::Get()->OnEvent( kvDlcNotify );
	return;
}

void PvpLobby::MsgNoValidMissionChapter( KeyValues *pSettings )
{
	LeaveLobbyImpl();	// Lobby is no valid at this point

	if ( IsX360() )
	{
		// The required DLC is missing on the client, fire a notification that would
		// allow us to download the DLC from the marketplace
		uint64 uiDlcMask = pSettings->GetUint64( "game/dlcrequired" );
		uint64 uiDlcInstalled = g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" );

		uint64 uiDlcNotify = ( uiDlcMask &~uiDlcInstalled );

		KeyValues *kvDlcNotify = new KeyValues( "OnMatchSessionUpdate" );
		KeyValues::AutoDelete autodelete_kvDlcNotify( kvDlcNotify );

		kvDlcNotify->SetString( "state", "error" );
		kvDlcNotify->SetString( "error", "dlcrequired" );
		kvDlcNotify->SetUint64( "dlcrequired", uiDlcNotify );
		kvDlcNotify->SetUint64( "dlcmask", uiDlcMask );
		kvDlcNotify->SetString( "action", "kicked" );

		CUIGameData::Get()->OnEvent( kvDlcNotify );
		return;
	}

	const char *szCampaignWebsite = pSettings->GetString( "game/missioninfo/website", NULL );

	if ( szCampaignWebsite && *szCampaignWebsite )
	{
		if ( pSettings )
			pSettings->SetString( "game/missioninfo/from", "Lobby" );

		CBaseModPanel::GetSingleton().OpenWindow( WT_DOWNLOADCAMPAIGN,
			CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() ),
			true, pSettings );
	}
	else
	{
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_Lobby_MissingContent";
		data.pMessageText = "#L4D360UI_Lobby_MissingContent_Message";
		data.bOkButtonEnabled = true;

		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, true ) );

		confirmation->SetUsageData(data);
	}
}

void PvpLobby::MsgChangeGameSettings()
{
	if ( CBaseModFrame *pSpinner = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
		pSpinner->Close();
	else
		return;

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
	OnCommand( "ChangeGameSettings" );
}

CEG_NOINLINE void PvpLobby::ApplySchemeSettings( vgui::IScheme* pScheme )
{
	CEG_PROTECT_VIRTUAL_FUNCTION( PvpLobby_ApplySchemeSettings );

	BaseClass::ApplySchemeSettings( pScheme );

	// Subscribe to the matchmaking events
	if ( !m_bSubscribedForEvents )
	{
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
		m_bSubscribedForEvents = true;
	}

	if ( !IsGameConsole() )
	{
		OnItemSelected( "" );
		RequestFocus();
		m_pListFriends->NavigateTo();
	}
}

CEG_NOINLINE void PvpLobby::Activate()
{
	BaseClass::Activate();

	CEG_PROTECT_MEMBER_FUNCTION( PvpLobby_Activate );

	m_pListFriends->NavigateTo();
}

void PvpLobby::SetStateText( const char *pText, const wchar_t *pFormattedText )
{
	if ( pText )
	{
		V_strncpy( m_StateText, pText, sizeof( m_StateText ) );
	}
	else if ( pFormattedText )
	{
		m_StateText[0] = 0;
		V_wcsncpy( m_FormattedStateText, pFormattedText, sizeof( m_FormattedStateText ) );
	}
	else
	{
		m_StateText[0] = 0;
		m_FormattedStateText[0] = 0;
	}
}

void PvpLobby::ClockSpinner()
{
	if ( IsPS3() )
		// We always display placeholder list items on PS3
		return;

	bool bSearchStatusVisibility = true;
	bool bBlink = ( (int)Plat_FloatTime() ) & 0x01;

	if ( m_pListFriends->IsVisible() )
	{
		if ( m_pListFriends->GetPanelItemCount() )
			bSearchStatusVisibility = false;
	}

	if ( vgui::Label *pStatus = dynamic_cast< vgui::Label * >( FindChildByName( "ListSearchStatus" ) ) )
	{
		pStatus->SetFgColor( bBlink ? Color( 0, 0, 0, 255 ) : Color( 255, 255, 255, 255 ) );
		pStatus->SetVisible( bSearchStatusVisibility );
		pStatus->SetText( m_pResourceLoadConditions->GetInt( "?network_lan" ) ? "#Portal2UI_Matchmaking_RefreshingSystemLink" : "#Portal2UI_Matchmaking_RefreshingFriends" );
	}

	vgui::ImagePanel *pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) );
	if ( pWorkingAnim )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		pWorkingAnim->SetFrame( nAnimFrame );
		pWorkingAnim->SetVisible( bSearchStatusVisibility );
	}
}

void PvpLobby::UpdateFooterButtons()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( !footer )
		return;

	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;

	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );
	bool bIsLiveGame = !Q_stricmp( szNetwork, "LIVE" );

	FriendsListItem *pListItem = m_pListFriends ? dynamic_cast< FriendsListItem * >( m_pListFriends->GetSelectedPanelItem() ) : NULL;
	bool bPlayerHighlighted = !!pListItem;
	bool bShouldShowGamerCard =
		!IsPS3() && // ps3 doesn't support gamercards
		bPlayerHighlighted && pPlayerLocal && ( pPlayerLocal->GetOnlineState() != IPlayer::STATE_OFFLINE );
	if ( bShouldShowGamerCard && !bIsLiveGame )
	{
		bShouldShowGamerCard = !!pListItem->GetFullInfo().mOnlineXUID; // System Link results should have a LIVE XUID
	}

	// Figure out if we need to show the action button
	bool bSendInviteToUser = ( bPlayerHighlighted && pListItem->GetFullInfo().mFriendXUID && ( !IsX360() || !bIsLiveGame ||
		CUIGameData::Get()->CanSendLiveGameInviteToUser( pListItem->GetFullInfo().mFriendXUID ) ) );
	bool bConvertToFriendsGame = ( !CUIGameData::Get()->CanSendLiveGameInviteToUser( 0ull ) &&
		pMatchSession && !Q_stricmp( "LIVE", pMatchSession->GetSessionSettings()->GetString( "system/network" ) ) &&
		!Q_stricmp( "private", pMatchSession->GetSessionSettings()->GetString( "system/access" ) ) );
	bool bMultiActionButton = bSendInviteToUser || bConvertToFriendsGame;

	// We don't show the A button if we're not in a live game and hovering over a player
	Button_t iAButton = ( ( !bShouldShowGamerCard ) ? ( FB_NONE ) : ( PVP_VIEWGAMERCARD_FB ) );
	Button_t iXButton = ( bMultiActionButton ? ( PVP_INVITEJOIN_FB ) : ( FB_NONE ) );
	Button_t iYButton = ( ( m_pResourceLoadConditions->GetInt( "?network_lan" ) ) ? ( FB_NONE ) : ( PVP_QUICKMATCH_FB ) );

	if ( !IsGameConsole() )
	{
		iAButton = FB_NONE;
		iXButton = FB_NONE;
	}

	int iExtraButtons = FB_NONE;
	if ( !IsGameConsole() )
	{
		iExtraButtons = FB_LSHOULDER;
	}

	footer->SetButtons( iAButton | iXButton | FB_BBUTTON | iYButton | iExtraButtons );

	footer->SetButtonText( PVP_INVITEJOIN_FB, ( bIsLiveGame || !IsX360() ) ?
		( bSendInviteToUser ? "#Portal2UI_pvp_Invite_Footer" : "#Portal2UI_pvp_Friends_Footer" ) :
		"#Portal2UI_pvp_Join_Footer" );

	char const *szQuickmatchText = "#Portal2UI_pvp_QuickMatch";
	if ( IsX360() )
		szQuickmatchText = "#Portal2UI_pvp_OnlineOptions";
	if ( IsPS3() )
		szQuickmatchText = "#Portal2UI_pvp_OnlineOptionsPS3";
	footer->SetButtonText( PVP_QUICKMATCH_FB, szQuickmatchText, false );

	if ( bShouldShowGamerCard )
	{
		footer->SetButtonText( PVP_VIEWGAMERCARD_FB, "#L4D360UI_ViewGamerCard" );
	}
	else
	{
		footer->SetButtonText( PVP_VIEWGAMERCARD_FB, "#L4D360UI_Select" );
	}

	if ( !IsGameConsole() )
	{
		footer->SetButtonText( FB_LSHOULDER, "#PORTAL2_ItemManagement" );
	}

	footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
}

void PvpLobby::LeaveLobby()
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );

	GenericConfirmation::Data_t data;
	char szTxt[256] = {0};

	data.pWindowTitle = "#L4D360UI_LeaveLobbyConf";

#ifdef PORTAL2_PUZZLEMAKER
	if ( BASEMODPANEL_SINGLETON.IsCommunityCoop() )
	{
		data.pWindowTitle = "#PORTAL2_CommunityCoop_LeaveLobbyConf";
	}
#endif

	if ( IsX360() )
	{
		char const *szTxtHosting = "Hosting";
		char const *szTxtNetwork = Q_stricmp( szNetwork, "LIVE" ) ? "SysLink" : "Live";

		Q_snprintf( szTxt, ARRAYSIZE( szTxt ), "#L4D360UI_LeaveLobbyConf%s%s", szTxtHosting, szTxtNetwork );
		data.pMessageText = szTxt;
	}
	else
	{
		data.pMessageText = "#L4D360UI_LeaveLobbyConfHostingLive";
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pOkButtonText = "#PORTAL2_ButtonAction_Exit";
	data.pfnOkCallback = LeaveLobbyImpl;

	GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	if ( confirmation )
	{
		m_nMsgBoxId = confirmation->SetUsageData(data);
	}
}

void PvpLobby::QuickMatch()
{
	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );
	if ( Q_stricmp( szNetwork, "LIVE" ) )
		return;

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	if ( IsX360() || IsPS3() )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_XBOXLIVE, this );
		return;
	}


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

void PvpLobby::AllowFriendsAccess()
{
	if ( CUIGameData::Get()->CanSendLiveGameInviteToUser( 0ull ) )
		// user doesn't have restricted privileges
		return;

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pIMatchSession )
		return;

	if ( !Q_stricmp( "LIVE", pIMatchSession->GetSessionSettings()->GetString( "system/network" ) ) &&
		!Q_stricmp( "private", pIMatchSession->GetSessionSettings()->GetString( "system/access" ) ) )
	{
		KeyValues *pSettings = new KeyValues( "update" );
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "update/system/access", "friends" );

		pIMatchSession->UpdateSessionSettings( pSettings );

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
	}
}

void PvpLobby::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state" );
		if ( !Q_stricmp( szState, "updated" ) )
		{
			// Some properties of the session got updated
			// We care only of the properties that got updated
			if ( KeyValues *kvUpdate = pEvent->FindKey( "update" ) )
			{
				ApplyUpdatedSettings( kvUpdate );
			}
		}
		else if ( !Q_stricmp( szState, "closed" ) )
		{
			m_pSettings = NULL; // Session has closed, stop using the destroyed pointer
		}
	}
	else if ( m_pResourceLoadConditions->GetInt( "?network_lan" ) )
	{
		if ( !Q_stricmp( "OnMatchPlayerMgrUpdate", szEvent ) )
		{
			char const *szUpdate = pEvent->GetString( "update", "" );
			if ( ui_pvplobby_debug.GetBool() )
			{
				DevMsg( "PvpLobby::OnEvent 'OnMatchPlayerMgrUpdate' ( %.2f : %s )\n", Plat_FloatTime(), szUpdate );
			}
			if ( !Q_stricmp( "searchstarted", szUpdate ) )
			{
				m_flSearchStartedTime = Plat_FloatTime();
				m_flSearchEndTime = m_flSearchStartedTime + ui_pvplobby_spinner_time.GetFloat();
				OnThink();
			}
			else if ( !Q_stricmp( "searchfinished", szUpdate ) )
			{
				m_flSearchStartedTime = 0.0f;
				m_flLastRefreshTime = 0;
			}
			else if ( !Q_stricmp( "friend", szUpdate ) )
			{
				// Friend's game details have been updated
				if ( m_flLastRefreshTime && ( m_flLastRefreshTime + ui_pvplobby_refresh_period.GetFloat() - Plat_FloatTime() ) >  ui_pvplobby_update_time.GetFloat() )
					m_flLastRefreshTime = ui_pvplobby_update_time.GetFloat() - ui_pvplobby_refresh_period.GetFloat() + Plat_FloatTime();
			}
		}
	}
}

void PvpLobby::ApplyUpdatedSettings( KeyValues *kvUpdate )
{
	if ( kvUpdate != m_pSettings && kvUpdate->GetString( "game/mode", NULL ) )
	{
		return;
	}

	CAutoPushPop< bool > autoPreventCommandHandling( m_bNoCommandHandling, true );

	// Always apply the lobby leader update
	SetLobbyLeaderText();


	if ( char const *szLock = kvUpdate->GetString( "system/lock", NULL ) )
	{
		RemapText_t arrWaits[] = {
			{ "matching",	"#MatchLobby_matching", RemapText_t::MATCH_FULL },	// leader is searching for opponents
			{ "matchingsearchidle", "#MatchLobby_matching_searchidle", RemapText_t::MATCH_FULL },	// searching for opponents, search idle
			{ "matchingsearcherror", "#MatchLobby_matching_searcherror", RemapText_t::MATCH_FULL },	// searching for opponents error
			{ "matchingsearchawaitingpeer", "#MatchLobby_matching_searchidle", RemapText_t::MATCH_FULL },	// searching for opponents, search idle
			{ "matchingsearchlinked", "#MatchLobby_matching_searchlinked", RemapText_t::MATCH_FULL },	// linked with another team
			{ "matchingsearching", "#MatchLobby_matching", RemapText_t::MATCH_FULL },	// searching for opponents, initiating search
			{ "matchingsearchresult", "#MatchLobby_matching_searchresult", RemapText_t::MATCH_START },	// searching for opponents, have some results
			{ "matchingdedicated", "#MatchLobby_matching_dedicated", RemapText_t::MATCH_FULL },	// looking for dedicated server
			{ "matchingpeerserver", "#MatchLobby_matching_peerserver", RemapText_t::MATCH_FULL },	// peer is looking for server
			{ "starting",	"#MatchLobby_starting", RemapText_t::MATCH_FULL },	// leader is searching for server
			{ "loading",	"#MatchLobby_loading", RemapText_t::MATCH_FULL },	// leader is loading a listen server
			{ "ending",		"#MatchLobby_ending", RemapText_t::MATCH_FULL },	// game ended and loading into lobby
			{ "",	NULL, RemapText_t::MATCH_START },	// empty lock
			{ NULL, NULL, RemapText_t::MATCH_FULL }
		};

		char const *szStateText = RemapText_t::RemapRawText( arrWaits, szLock );
		SetStateText( szStateText, NULL );

		/*
		// Update the spinner
		if ( vgui::ImagePanel *pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) ) )
			pWorkingAnim->SetVisible( szStateText != NULL );
		*/
	}

	// Check the DLC required setting if it changed
	if ( uint64 uiDlcRequiredMask = kvUpdate->GetUint64( "game/dlcrequired" ) )
	{
		// Get client's dlc mask
		uint64 uiDlcInstalledMask = g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" );
		if ( ( uiDlcRequiredMask & uiDlcInstalledMask ) != uiDlcRequiredMask )
		{
			KeyValues *pLeaveLobbyMsg = m_pSettings ? m_pSettings->MakeCopy() : new KeyValues( "" );
			pLeaveLobbyMsg->SetName( "NoValidMissionChapter" );
			PostMessage( this, pLeaveLobbyMsg );
			return;
		}
	}
}

void PvpLobby::SetLobbyLeaderText()
{
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	XUID xuidLeader = pMatchSession ? pMatchSession->GetSessionSystemData()->GetUint64( "xuidHost" ) : 0ull;
	if ( !xuidLeader )
	{
		xuidLeader = m_pSettings->GetUint64( "members/machine0/id", 0ull );
	}
	if ( KeyValues *pPlayerLobbyLeader = SessionMembersFindPlayer( m_pSettings, xuidLeader ) )
	{
		vgui::Label *pLabel = dynamic_cast< vgui::Label * > ( FindChildByName( "LblLeaderLine" ) );
		if( pLabel )
		{
			char const *szName = pPlayerLobbyLeader->GetString( "name", "" );
			szName = CUIGameData::Get()->GetPlayerName( xuidLeader, szName );

			wchar_t nameBuffer[MAX_PLAYER_NAME_LENGTH] = {0};
			g_pVGuiLocalize->ConvertANSIToUnicode( szName, nameBuffer, sizeof(nameBuffer) );

			bool bTeamLobby = StringHasPrefix( m_pSettings->GetString( "game/mode", "" ), "team" );

			wchar_t finalMessage[MAX_PATH] = {0};
			if ( wchar_t* wMsg = g_pVGuiLocalize->Find( bTeamLobby ? "#L4D360UI_Lobby_TeamLeaderLine" : "#L4D360UI_Lobby_LeaderLine" ) )
			{
				g_pVGuiLocalize->ConstructString( finalMessage, sizeof( finalMessage ), wMsg, 1, nameBuffer );
			}

			pLabel->SetText( finalMessage );
		}
	}
}

vgui::Label * PvpLobby::GetSettingsSummaryLabel( char const *szContentType )
{
	if ( !m_pLobbyDetailsLayout )
	{
		m_pLobbyDetailsLayout = new KeyValues( "LobbyDetailsSummary" );
		if ( !m_pLobbyDetailsLayout->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/LobbyDetailsSummary.res", "MOD" ) )
		{
			DevWarning( "Failed to load lobby details summary information!\n" );
			m_pLobbyDetailsLayout->deleteThis();
			m_pLobbyDetailsLayout = NULL;
			m_pLobbyDetailsLayout = new KeyValues( "LobbyDetailsSummary" );
		}
		m_autodelete_pLobbyDetailsLayout.Assign( m_pLobbyDetailsLayout );
	}

	char const *szGameMode = m_pSettings->GetString( "game/mode", NULL );
	if ( !szGameMode )
		return NULL;

	char const *szSummaryField = m_pLobbyDetailsLayout->GetString( CFmtStr( "%s/%s", szGameMode, szContentType ), NULL );
	if ( !szSummaryField )
		return NULL;

	return dynamic_cast< vgui::Label * > ( FindChildByName( CFmtStr( "LblSummaryLine%s", szSummaryField ) ) );
}

bool PvpLobby::SweepAndAddNewFriends()
{
	uint16 numPanelsBegin = m_pListFriends->GetPanelItemCount();

	// Mark all as stale
	for( int i = 0; i < m_pListFriends->GetPanelItemCount(); ++i )
	{
		FriendsListItem *game	= dynamic_cast< FriendsListItem* >( m_pListFriends->GetPanelItem( i ) );
		if( game )
		{
			game->SetSweep( true );
		}
	}

	AddFriendsToList();

	// Remove stale games, it's a vector, so removing an item puts the next item into the current index
	bool bContentsChanged = ( m_pListFriends->GetPanelItemCount() != numPanelsBegin );
	for ( int i = 0; i < m_pListFriends->GetPanelItemCount(); /* */ )
	{
		FriendsListItem *game	= dynamic_cast< FriendsListItem* >( m_pListFriends->GetPanelItem( i ) );
		if( game && game->IsSweep() )
		{
			m_pListFriends->RemovePanelItem( static_cast<unsigned short>( i ) );
			bContentsChanged = true;
			continue;
		}
		++i;
	}

	//
	// Sort the list
	//
	CUtlVector< Panel* > arrPanelsBegin, arrPanelsEnd;
	for ( uint16 i = 0; i < m_pListFriends->GetPanelItemCount(); ++ i )
		arrPanelsBegin.AddToTail( m_pListFriends->GetPanelItem( i ) );
	SortListItems();
	for ( uint16 i = 0; i < m_pListFriends->GetPanelItemCount(); ++ i )
		arrPanelsEnd.AddToTail( m_pListFriends->GetPanelItem( i ) );
	if ( !bContentsChanged )
		bContentsChanged = !!Q_memcmp( arrPanelsBegin.Base(), arrPanelsEnd.Base(), m_pListFriends->GetPanelItemCount() * sizeof( Panel* ) );

	// Handle adding new item to empty result list
	if ( !numPanelsBegin )
		m_pListFriends->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false );

	return bContentsChanged;
}

void PvpLobby::ActivateFriendsList( int iDirection )
{
	if ( !IsPS3() )
		return;
	if ( !m_pListFriendsArray[0] || !m_pListFriendsArray[1] )
		return;

	int iActive = !( m_pListFriends == m_pListFriendsArray[0] );
	if ( iDirection == iActive )
		return;
	if ( iDirection != ADD_FRIENDS_STEAM && iDirection != ADD_FRIENDS_PSN )
		return;

	// Calculate the index of the item that is currently highlighted in the list
	vgui::Panel *pOldItem = m_pListFriends->GetSelectedPanelItem();
	if ( !pOldItem )
		return;
	uint16 usOldItemIndex = 0;
	if ( !m_pListFriends->GetPanelItemIndex( pOldItem, usOldItemIndex ) )
		return;
	int iOldFirstVisibleItem = m_pListFriends->GetFirstVisibleItemNumber();

	// We need to first navigate away from the currently active item in the active list
	m_pListFriends->NavigateFrom();

	// Set the new friends list
	m_pListFriends = m_pListFriendsArray[iDirection];
	int iNewFirstVisibleItem = m_pListFriends->GetFirstVisibleItemNumber();
	int iNewItemIndex = iNewFirstVisibleItem - iOldFirstVisibleItem + usOldItemIndex;
	iNewItemIndex = MAX( 0, iNewItemIndex );
	iNewItemIndex = MIN( iNewItemIndex, m_pListFriends->GetPanelItemCount() - 1 );
	m_pListFriends->SelectPanelItem( iNewItemIndex, GenericPanelList::SD_DOWN, false, true );

	// Animate the lists appropriately
	m_bAnimatingLists = true;
	m_flAnimationTimeStamp = Plat_FloatTime();
	m_nAnimatingSourceWidth[0] = m_pListFriendsArray[0]->GetWide();
	m_nAnimatingSourceWidth[1] = m_pListFriendsArray[1]->GetWide();
}

bool PvpLobby::UpdateFriendsList()
{
	if ( !FindChildByName( "ListFriendsStatus" ) )
		return false;

	bool bContentsChanged = false;
	if ( IsPS3() )
	{
		// Update inactive list first
		AddFriendsRule_t eRuleActive = (AddFriendsRule_t) !( m_pListFriends == m_pListFriendsArray[0] );
		m_eAddFriendsRule = ( AddFriendsRule_t ) !eRuleActive;
		m_pListFriends = m_pListFriendsArray[m_eAddFriendsRule];
		bContentsChanged |= SweepAndAddNewFriends();
		m_pListFriends->NavigateFrom(); // disable any selected items in the inactive list

		// Update active list now and restore the m_pListFriends pointer
		m_eAddFriendsRule = ( AddFriendsRule_t ) eRuleActive;
		m_pListFriends = m_pListFriendsArray[m_eAddFriendsRule];
		bContentsChanged |= SweepAndAddNewFriends();
	}
	else
	{
		bContentsChanged = SweepAndAddNewFriends();
	}

	// Re-trigger selection to update game details
	OnItemSelected( "" );

	if ( ui_pvplobby_debug.GetBool() )
	{
		DevMsg( "PvpLobby::UpdateFriendsList ( %.2f sec since last update )\n", m_flLastRefreshTime ? ( Plat_FloatTime() - m_flLastRefreshTime ) : 0.0f );
	}
	m_flLastRefreshTime = Plat_FloatTime();

	if ( vgui::Label *pListFriendsStatus = dynamic_cast< vgui::Label * >( FindChildByName( "ListFriendsStatus" ) ) )
	{
		pListFriendsStatus->SetVisible( !m_pListFriends->GetPanelItemCount() );
		pListFriendsStatus->SetText( m_pResourceLoadConditions->GetInt( "?network_lan" ) ? "#Portal2UI_Matchmaking_NoSystemLink" : "#Portal2UI_Matchmaking_NoFriends" );
	}

	// Nudge search spinner every time we update friends list
	m_flSearchStartedTime = Plat_FloatTime();
	m_flSearchEndTime = m_flSearchStartedTime + ui_pvplobby_spinner_time.GetFloat();

	return bContentsChanged;
}

#if defined( _X360 )
static FriendsListItem::StatusPriority_t XonlineFriendPriority( const XONLINE_FRIEND *friendData )
{
	uint64 uiTitleIDCurrent = g_pMatchFramework->GetMatchTitle()->GetTitleID();
	if ( friendData->dwTitleID == uiTitleIDCurrent )
		return FriendsListItem::SPRI_CURRENT;
	else switch( friendData->dwTitleID )
	{
	case 0x45410912: return FriendsListItem::SPRI_CURRENT;
	case 0x454108D4: case 0x45410830: case 0x4541080F:
		return FriendsListItem::SPRI_VALVE_GAME;
	default:
		if ( friendData->dwFriendState & XONLINE_FRIENDSTATE_FLAG_ONLINE )
			return FriendsListItem::SPRI_ONLINE;
		else
			return FriendsListItem::SPRI_OFFLINE;
	}
}
static int XonlineFriendsSortFunc( const XONLINE_FRIEND *a, const XONLINE_FRIEND *b )
{
	FriendsListItem::StatusPriority_t priA = XonlineFriendPriority( a );
	FriendsListItem::StatusPriority_t priB = XonlineFriendPriority( b );
	if ( priA != priB )
		return ( priA < priB ) ? -1 : 1;
	return V_stricmp( a->szGamertag, b->szGamertag );
}
#endif

void PvpLobby::AddFriendsToList()
{
	FriendsListItem::Info fi;
	Q_memset( &fi, 0, sizeof( fi ) );
	fi.mInfoType = FriendsListItem::FGT_PLAYER;
#ifndef NO_STEAM
	int numFriends = steamapicontext->SteamFriends() ? steamapicontext->SteamFriends()->GetFriendCount( k_EFriendFlagImmediate ) : 0;
	uint64 uiAppID = steamapicontext->SteamUtils()->GetAppID();
	numFriends = MIN( numFriends, ui_pvplobby_friends_cap.GetInt() );
	int numFriendsActuallyAddedAfterFiltering = 0;
	for ( int index = 0; index < numFriends; ++ index )
	{
		CSteamID steamIDFriend = steamapicontext->SteamFriends()->GetFriendByIndex( index, k_EFriendFlagImmediate );
		XUID xuidFriend = steamIDFriend.ConvertToUint64();
		FriendGameInfo_t fgi;
		bool bInGame = steamapicontext->SteamFriends()->GetFriendGamePlayed( xuidFriend, &fgi );
		EPersonaState eState = steamapicontext->SteamFriends()->GetFriendPersonaState( xuidFriend );
		char const *szFriendName = steamapicontext->SteamFriends()->GetFriendPersonaName( xuidFriend );
		
		fi.uiStatus = 0
			| ( ( eState > k_EPersonaStateOffline ) ? FriendsListItem::FLI_ONLINE : 0 )
			| ( bInGame ? FriendsListItem::FLI_INGAME : 0 )
			| ( 0 ? FriendsListItem::FLI_VOICE : 0 )
			| ( ( eState >= k_EPersonaStateAway ) ? FriendsListItem::FLI_AWAY : 0 )
			| ( ( eState == k_EPersonaStateBusy ) ? FriendsListItem::FLI_BUSY : 0 )
			;
		fi.uiAppId = bInGame ? fgi.m_gameID.AppID() : 0;
		if ( fi.uiAppId == uiAppID )
			fi.ePriority = FriendsListItem::SPRI_CURRENT;
		else if ( fi.uiStatus & FriendsListItem::FLI_ONLINE )
			fi.ePriority = FriendsListItem::SPRI_ONLINE;
		else
			fi.ePriority = FriendsListItem::SPRI_OFFLINE;
		fi.mFriendXUID = xuidFriend;
		Q_strncpy( fi.Name, szFriendName, sizeof( fi.Name ) );
		if ( AddFriendFromDetails( &fi ) )
			numFriendsActuallyAddedAfterFiltering ++;
	}
	if ( IsPS3() && !numFriendsActuallyAddedAfterFiltering )
	{
		// Add a dummy friend
		fi.uiStatus = FriendsListItem::FLI_ONLINE;
		fi.uiAppId = 0;
		fi.ePriority = FriendsListItem::SPRI_ONLINE;
		fi.mFriendXUID = 0ull;
		Q_strncpy( fi.Name, m_eAddFriendsRule ? "#1" : "#0", sizeof( fi.Name ) );
		AddFriendFromDetails( &fi );
	}
#elif defined( _X360 )
	uint64 uiTitleIDCurrent = g_pMatchFramework->GetMatchTitle()->GetTitleID();
	if ( !m_pResourceLoadConditions->GetInt( "?network_lan" ) )
	{


	DWORD cbSize = 0;
	HANDLE hHandle = NULL;
	DWORD ret = xonline->XFriendsCreateEnumerator( XBX_GetPrimaryUserId(), 0, 100, &cbSize, &hHandle );
	if ( ret != ERROR_SUCCESS )
		return;
	
	CUtlBuffer bufFriendsData;
	bufFriendsData.EnsureCapacity( cbSize );
	DWORD dwNumFriends = 0;
	ret = XEnumerate( hHandle, bufFriendsData.Base(), cbSize, &dwNumFriends, NULL );
	CloseHandle( hHandle );
	hHandle = NULL;
	if ( ret != ERROR_SUCCESS )
		return;

	CUtlVector< XONLINE_FRIEND > arrSortedFriends( ( XONLINE_FRIEND * ) bufFriendsData.Base(), dwNumFriends, dwNumFriends );
	if ( ( m_nRefreshListCap > 0 ) && ( dwNumFriends > ( DWORD )( m_nRefreshListCap ) ) )
	{
		arrSortedFriends.Sort( XonlineFriendsSortFunc );
		dwNumFriends = m_nRefreshListCap;
		++ m_nRefreshListCap;
	}
	else
	{
		m_nRefreshListCap = 0;
	}

	for ( XONLINE_FRIEND *friendData = ( XONLINE_FRIEND * ) arrSortedFriends.Base(),
		*friendDataEnd = friendData + dwNumFriends; friendData < friendDataEnd; ++ friendData )
	{
		fi.uiStatus = 0
			| ( ( friendData->dwFriendState & XONLINE_FRIENDSTATE_FLAG_ONLINE ) ? FriendsListItem::FLI_ONLINE : 0 )
			| ( ( friendData->dwFriendState & XONLINE_FRIENDSTATE_FLAG_PLAYING ) ? FriendsListItem::FLI_INGAME : 0 )
			| ( ( friendData->dwFriendState & XONLINE_FRIENDSTATE_FLAG_VOICE ) ? FriendsListItem::FLI_VOICE : 0 )
			| ( XOnlineIsUserAway( friendData->dwFriendState ) ? FriendsListItem::FLI_AWAY : 0 )
			| ( XOnlineIsUserBusy( friendData->dwFriendState ) ? FriendsListItem::FLI_BUSY : 0 )
			;
		fi.uiAppId = friendData->dwTitleID;
		if ( fi.uiAppId == uiTitleIDCurrent )
			fi.ePriority = FriendsListItem::SPRI_CURRENT;
		else switch( fi.uiAppId )
		{
			case 0x45410912: fi.ePriority = FriendsListItem::SPRI_CURRENT; break;
			case 0x454108D4: case 0x45410830: case 0x4541080F:
				fi.ePriority = FriendsListItem::SPRI_VALVE_GAME; break;
			default:
				if ( fi.uiStatus & FriendsListItem::FLI_ONLINE )
					fi.ePriority = FriendsListItem::SPRI_ONLINE;
				else
					fi.ePriority = FriendsListItem::SPRI_OFFLINE;
		}
		fi.mFriendXUID = friendData->xuid;
		Q_strncpy( fi.Name, friendData->szGamertag, sizeof( fi.Name ) );
		AddFriendFromDetails( &fi );
	}

	}
	else
	{

	IPlayerManager *mgr = g_pMatchFramework->GetMatchSystem()->GetPlayerManager();
	
	fi.mInfoType = FriendsListItem::FGT_SYSLINK;
	int numItems = mgr->GetNumFriends();
	for( int i = 0; i < numItems; ++i )
	{
		IPlayerFriend *item = mgr->GetFriendByIndex( i );
		KeyValues *pGameDetails = item->GetGameDetails();
		if ( Q_stricmp( pGameDetails->GetString( "game/state" ), "lobby" ) )
			continue;
		if ( pGameDetails->GetInt( "members/numPlayers" ) != 1 )
			continue;
		if ( Q_stricmp( pGameDetails->GetString( "system/network" ), "lan" ) )
			continue;

		fi.uiStatus = FriendsListItem::FLI_ONLINE | FriendsListItem::FLI_INGAME;
		fi.uiAppId = uiTitleIDCurrent;
		fi.ePriority = FriendsListItem::SPRI_CURRENT;
		Q_strncpy( fi.Name, item->GetName(), sizeof( fi.Name ) );
		fi.mFriendXUID = item->GetXUID();
		fi.mOnlineXUID = pGameDetails->GetUint64( "player/xuidOnline", 0ull );
		AddFriendFromDetails( &fi );
	}

	}
#else
	Q_strncpy( fi.Name, "Friend #1", sizeof( fi.Name ) );
	fi.mFriendXUID = 0x1;
	AddFriendFromDetails( &fi );
	Q_strncpy( fi.Name, "Friend #2", sizeof( fi.Name ) );
	fi.mFriendXUID = 0x2;
	AddFriendFromDetails( &fi );
#endif
}

static bool IsADuplicateFriend( FriendsListItem *item, FriendsListItem::Info const &fi )
{
	FriendsListItem::Info const &ii = item->GetFullInfo();

	return ( ii.mFriendXUID == fi.mFriendXUID );
}

bool PvpLobby::AddFriendFromDetails( const void *pfi )
{
	FriendsListItem::Info const &fi = *(FriendsListItem::Info const *)pfi;
	if ( !fi.Name[0] )
		return false;

	if ( !ui_pvplobby_show_offline.GetBool() && !( fi.uiStatus & FriendsListItem::FLI_ONLINE ) )
		return false;

#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( fi.mFriendXUID ) switch ( m_eAddFriendsRule )
	{
	case ADD_FRIENDS_PSN:
		if ( !CSteamID( fi.mFriendXUID ).BConsoleUserAccount() )
			return false;
		break;
	
	case ADD_FRIENDS_STEAM:
		if ( CSteamID( fi.mFriendXUID ).BConsoleUserAccount() )
			return false;
		break;
	}
#endif

	// See if already in list
	FriendsListItem* game = NULL;

	for( int j = 0; j < m_pListFriends->GetPanelItemCount(); ++j )
	{
		FriendsListItem *item	= dynamic_cast< FriendsListItem* >( m_pListFriends->GetPanelItem( j ) );
		if ( !item )
			continue;

		if ( IsADuplicateFriend( item, fi ) )
		{
			game = item;
			break;
		}
	}

	if ( !game )
	{
		char const *szItemTemplateName = "pvplobbyfriend";
		game = m_pListFriends->AddPanelItem< FriendsListItem >( szItemTemplateName );
	}
	else if ( !game->IsSweep() )
	{
		// Game has already been updated with a valid info
		return true;
	}

	game->SetSweep( false ); // No need to remove

	// Make a copy
	FriendsListItem::Info fiCopy = fi;

	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		Q_strncpy( fiCopy.Name, "WWWWWWWWWWWWWWW", sizeof( fiCopy.Name ) );

	game->SetGameIndex( fiCopy );

	return true;
}

static int __cdecl FriendsListSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	FriendsListItem *fA	= dynamic_cast< FriendsListItem* >(*a);
	FriendsListItem *fB	= dynamic_cast< FriendsListItem* >(*b);

	const FriendsListItem::Info &ia = fA->GetFullInfo();
	const FriendsListItem::Info &ib = fB->GetFullInfo();

	if ( ia.ePriority != ib.ePriority )
		return (ia.ePriority < ib.ePriority) ? -1 : 1;

	return Q_stricmp( ia.Name, ib.Name );
}

void PvpLobby::SortListItems()
{
	m_pListFriends->SortPanelItems( FriendsListSortFunc );
}

void PvpLobby::OnItemSelected( const char* panelName )
{
	if ( !m_bLayoutLoaded )
		return;

	FriendsListItem* pGameListItem = static_cast<FriendsListItem*>( m_pListFriends->GetSelectedPanelItem() );

#if !defined( _GAMECONSOLE )

	// Set active state
	for ( int i = 0; i < m_pListFriends->GetPanelItemCount(); /* */ )
	{
		FriendsListItem *pItem = dynamic_cast< FriendsListItem* >( m_pListFriends->GetPanelItem( i ) );

		if ( pItem )
		{
			pItem->SetSelected( pItem == pGameListItem );
		}
		++i;
	}

#endif

	m_ActiveControl = pGameListItem;

	UpdateFooterButtons();
}

#if !defined( NO_STEAM )
void PvpLobby::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( m_flLastRefreshTime && ui_pvplobby_debug.GetBool() )
	{
		DevMsg( "PvpLobby::Steam_OnPersonaStateChanged ( %.2f sec elapsed )\n", Plat_FloatTime() - m_flLastRefreshTime );
	}
	m_flLastRefreshTime = 0;
}
#endif

}; // namespace BaseModUI


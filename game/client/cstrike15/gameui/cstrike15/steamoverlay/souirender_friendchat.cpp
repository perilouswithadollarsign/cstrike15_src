//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#include "souirender_pch.h"
// additional #includes must be here
#include "souirender_pchend.h"

class SOUIrenderPanel_FriendChat_Individual
{
public:
	explicit SOUIrenderPanel_FriendChat_Individual( XUID xuid ) :
		m_xuid( xuid ),
		m_uiLatestLobbyInvite( 0 )
	{
		char const *szName = steamapicontext->SteamFriends()->GetFriendPersonaName( xuid );
		g_pVGuiLocalize->ConvertANSIToUnicode( szName, m_wszNameBuf, sizeof( m_wszNameBuf ) );
	}

	void OnInviteReceived( uint64 uiLobbyId )
	{
		m_uiLatestLobbyInvite = uiLobbyId;
		AddText( L"invited you to play Portal 2" );
	}

	void AddText( wchar_t const *wszText )
	{
		SOUIrender_AddNotification( m_xuid, wszText );
	}

	void Render()
	{
		;
	}

public:
	XUID m_xuid;
	wchar_t m_wszNameBuf[64];
	uint64 m_uiLatestLobbyInvite;
	struct Message_t
	{
		XUID m_xuid;
		wchar_t *m_msg;
	};
	CUtlVector< Message_t > m_arrLatestMessages;
};

class SOUIrenderPanel_FriendChat : public ISOUIrenderInputHandler
{
public:
	SOUIrenderPanel_FriendChat()
	{
		m_pLatestInvite = NULL;
	}

	virtual void Initialize()
	{
		m_LobbyInvite_t.Register( this, &SOUIrenderPanel_FriendChat::Steam_LobbyInvite_t );
	}

	virtual void Render()
	{
		FOR_EACH_VEC( m_arrFriends, i )
			m_arrFriends[i]->Render();

		if ( m_pLatestInvite )
		{
			// Render text
			int x = 0, y = 0, w = 0, h = 0;
			SteamOverlayFontHandle_t hFont = SOUIrenderGetFontHandle( kSOUIrenderFont_DefaultFixedOutline );
			g_pISteamOverlayRenderHost->TextSetFont( hFont );
			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 0, 255, 255 ) );
			g_pISteamOverlayRenderHost->TextSetPos( AdjustUiPosX( -100, true, 0 ), AdjustUiPosY( -100, true, 0 ) );
			g_pISteamOverlayRenderHost->TextDrawStringW( L"Latest invite:" );
			
			x = AdjustUiPosX( -100, true, 0 ), y = AdjustUiPosY( -80, true, 0 );
			g_pISteamOverlayRenderHost->TextSetPos( x, y );
			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 0, 255, 255 ) );
			g_pISteamOverlayRenderHost->TextDrawStringW( L"From: " );
			g_pISteamOverlayRenderHost->FontGetTextSize( hFont, L"From: ", w, h );
			g_pISteamOverlayRenderHost->TextSetPos( x + w, y );
			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 255, 0, 255 ) );
			g_pISteamOverlayRenderHost->TextDrawStringW( m_pLatestInvite->m_wszNameBuf );

			x = AdjustUiPosX( -100, true, 0 ), y = AdjustUiPosY( -60, true, 0 );
			g_pISteamOverlayRenderHost->TextSetPos( x, y );
			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 0, 255, 255 ) );
			g_pISteamOverlayRenderHost->TextDrawStringW( L"Lobby: " );
			g_pISteamOverlayRenderHost->FontGetTextSize( hFont, L"Lobby: ", w, h );
			g_pISteamOverlayRenderHost->TextSetPos( x + w, y );
			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 255, 0, 255 ) );
			wchar_t wchLobbyId[64];
			Q_snwprintf( wchLobbyId, sizeof( wchLobbyId ), L"0x%016llX", m_pLatestInvite->m_uiLatestLobbyInvite );
			g_pISteamOverlayRenderHost->TextDrawStringW( wchLobbyId );

			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 0, 255, 255 ) );
			g_pISteamOverlayRenderHost->TextSetPos( AdjustUiPosX( -100, true, 0 ), AdjustUiPosY( -40, true, 0 ) );
			g_pISteamOverlayRenderHost->TextDrawStringW( L"Press START to join" );
		}
		else
		{
			// Render text
			g_pISteamOverlayRenderHost->TextSetFont( SOUIrenderGetFontHandle( kSOUIrenderFont_DefaultFixedOutline ) );
			g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 255, 255, 0, 255 ) );
			g_pISteamOverlayRenderHost->TextSetPos( AdjustUiPosX( -100, true, 0 ), AdjustUiPosY( -100, true, 0 ) );
			g_pISteamOverlayRenderHost->TextDrawStringW( L"Your friends invites show up here" );
		}
	}

	virtual uint32 HandleInputEvent( int iCode, int iValue )
	{
		if ( m_pLatestInvite && iValue )
		{
			switch ( SOUIGetBaseButtonCode( iCode ) )
			{
			case KEY_XBUTTON_START:
				if ( m_pLatestInvite && m_pLatestInvite->m_uiLatestLobbyInvite )
				{
					KeyValues *kvEvent = new KeyValues( "OnSteamOverlayCall::LobbyJoin" );
					kvEvent->SetUint64( "sessionid", m_pLatestInvite->m_uiLatestLobbyInvite );
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
				}
				return INPUT_EVENT_RESULT_CLOSEOVERLAY;
			}
		}
		return INPUT_EVENT_RESULT_FALLTHROUGH;
	}

public:
	STEAM_CALLBACK_MANUAL( SOUIrenderPanel_FriendChat, Steam_LobbyInvite_t, LobbyInvite_t, m_LobbyInvite_t );
	CUtlVector< SOUIrenderPanel_FriendChat_Individual * > m_arrFriends;
	SOUIrenderPanel_FriendChat_Individual *m_pLatestInvite;
};
SOUIrenderDeclarePanel( SOUIrenderPanel_FriendChat );



void SOUIrenderPanel_FriendChat::Steam_LobbyInvite_t( LobbyInvite_t *pParam )
{
	DevMsg( "SOUIrenderPanel_FriendChat::Steam_LobbyInvite_t( %llX, %llX ) @ %.2f\n", pParam->m_ulSteamIDUser, pParam->m_ulSteamIDLobby, Plat_FloatTime() );

	// Find the friend or make new friend
	SOUIrenderPanel_FriendChat_Individual *pFriend = NULL;
	FOR_EACH_VEC( m_arrFriends, i )
	{
		if ( m_arrFriends[i]->m_xuid == pParam->m_ulSteamIDUser )
		{
			pFriend = m_arrFriends[i];
			break;
		}
	}
	if ( !pFriend )
	{
		pFriend = new SOUIrenderPanel_FriendChat_Individual( pParam->m_ulSteamIDUser );
		m_arrFriends.AddToTail( pFriend );
	}

	pFriend->OnInviteReceived( pParam->m_ulSteamIDLobby );
	m_pLatestInvite = pFriend;

	// Mark us as preferring focus input
	SOUIrender_RegisterInputHandler( this, true );
}


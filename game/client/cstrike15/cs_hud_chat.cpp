//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "cs_hud_chat.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "engine/IEngineSound.h"
#include "radio_status.h"
#include "bot/shared_util.h"
#include "ihudlcd.h"
#include "voice_status.h"
#include <vgui/IScheme.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// [hpe:jason]  Do not disable this element! It needs to be around to forward various HudChat messages to the new Scaleform UI
DECLARE_HUDELEMENT_FLAGS( CHudChat, HUDELEMENT_SS_FULLSCREEN_ONLY );

//=====================
//CHudChatLine
//=====================

CHudChatLine::CHudChatLine( vgui::Panel *parent, const char *panelName ) : CBaseHudChatLine( parent, panelName )
{
	m_text = NULL;
}

void CHudChatLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//=====================
//CHudChatInputLine
//=====================

void CHudChatInputLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	vgui::HFont hFont = pScheme->GetFont( "ChatFont" );

	m_pPrompt->SetFont( hFont );
	m_pInput->SetFont( hFont );

	m_pInput->SetFgColor( pScheme->GetColor( "Chat.TypingText", pScheme->GetColor( "Panel.FgColor", Color( 255, 255, 255, 255 ) ) ) );
}

//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	
}

void CHudChat::CreateChatInputLine( void )
{
	m_pChatInput = new CHudChatInputLine( this, "ChatInputLine" );
	m_pChatInput->SetVisible( false );
}

void CHudChat::CreateChatLines( void )
{
	m_ChatLine = new CHudChatLine( this, "ChatLine1" );
	m_ChatLine->SetVisible( false );		
}

void CHudChat::Init( void )
{
	BaseClass::Init();
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Radio text from the server
//-----------------------------------------------------------------------------
bool CHudChat::MsgFunc_RadioText( const CCSUsrMsg_RadioText &msg )
{
	int client = msg.client();

	wchar_t szBuf[6][128];
	wchar_t *msg_text = ReadLocalizedString( msg.msg_name().c_str(), szBuf[0], sizeof( szBuf[0] ), false );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg.params(0).c_str(), szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadLocalizedString( msg.params(1).c_str(), szBuf[2], sizeof( szBuf[2] ), true );	// location
	ReadLocalizedString( msg.params(2).c_str(), szBuf[3], sizeof( szBuf[3] ), true );	// radio text
	ReadLocalizedString( msg.params(3).c_str(), szBuf[4], sizeof( szBuf[4] ), true );	// unused :(

	if ( V_strcmp( msg.params( 3 ).c_str(), "auto" ) != 0 && ( GetClientVoiceMgr()->IsPlayerBlocked( client ) || GetClientVoiceMgr()->ShouldHideCommunicationFromPlayer( client ) ) )
		return false;

	g_pVGuiLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );
	ChatPrintf( client, CHAT_FILTER_TEAMCHANGE, "%s", ansiString );

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
bool CHudChat::MsgFunc_SayText2( const CCSUsrMsg_SayText2 &msg )
{
	// Got message during connection
	if ( !GetCSResources() )
		return true;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return true; // cannot print potentially personal details
	}

	int client = msg.ent_idx();
	bool bWantsToChat = msg.chat() != 0;

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString( msg.msg_name().c_str(), szBuf[0], sizeof( szBuf[0] ), false, untranslated_msg_text, sizeof( untranslated_msg_text ) );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg.params(0).c_str(), szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadChatTextString ( msg.params(1).c_str(), szBuf[2], sizeof( szBuf[2] ), true );	// location
	ReadLocalizedString( msg.params(2).c_str(), szBuf[3], sizeof( szBuf[3] ), true );	// radio text
	ReadLocalizedString( msg.params(3).c_str(), szBuf[4], sizeof( szBuf[4] ), true );	// unused :(
	
	if ( V_strcmp( msg.params( 3 ).c_str(), "auto" ) != 0 && ( GetClientVoiceMgr()->IsPlayerBlocked( client ) || GetClientVoiceMgr()->ShouldHideCommunicationFromPlayer( client ) ) )
		bWantsToChat = false;

	CEconQuestDefinition *pQuestDef = CSGameRules()->GetActiveAssassinationQuest();
	if ( pQuestDef && GetCSResources()->IsAssassinationTarget( client ) )
	{
		extern const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest );
		if ( const char* szToken = Helper_GetLocalPlayerAssassinationQuestLocToken( pQuestDef ) )
			V_wcscpy_safe( szBuf[ 1 ], g_pVGuiLocalize->Find( szToken ) );
	}

	g_pVGuiLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;
		bool playChatSound = true;

		if ( client > 0 && g_PR && (g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
			if ( !( iFilter & GetFilterFlags() ) )
			{
				playChatSound = false;
			}
		}

		// print raw chat text
		ChatPrintf( client, iFilter, "%s", ansiString );

		Msg( "%s\n", RemoveColorMarkup(ansiString) );

		if ( playChatSound )
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );
		}
	}
	else
	{
		if ( !GetClientVoiceMgr()->IsPlayerBlocked( client ) && !GetClientVoiceMgr()->ShouldHideCommunicationFromPlayer( client ) )
		{
			// print raw chat text
			ChatPrintf( client, GetFilterForString( untranslated_msg_text), "%s", ansiString );
		}
	}

	return true;
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudChat::GetChatInputOffset( void )
{
	if ( m_pChatInput->IsVisible() )
	{
		return m_iFontHeight;
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads in an Audio message from the server (wav file to be played
//          via the player's voice, i.e. for bot chatter)
//-----------------------------------------------------------------------------
bool CHudChat::MsgFunc_RawAudio( const CCSUsrMsg_RawAudio &msg )
{
	int pitch = msg.pitch();
	int playerIndex = msg.entidx();
	float feedbackDuration = msg.duration();
	const char *szString = msg.voice_filename().c_str();

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName = szString;
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_nPitch = pitch;

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, ep );

	if ( feedbackDuration > 0.0f )
	{
		//Flash them on the radar
		C_CSPlayer *pPlayer = static_cast<C_CSPlayer*>( cl_entitylist->GetEnt(playerIndex) );

		if ( pPlayer )
		{
			// Create the flashy above player's head
			RadioManager()->UpdateVoiceStatus( playerIndex, feedbackDuration );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorGreen;
	}
	else if( g_PR )
	{
		switch ( g_PR->GetTeam( clientIndex ) )
		{
		case 2	: return g_ColorRed;
		case 3	: return g_ColorBlue;
		default	: return g_ColorGrey;
		}
	}

	return g_ColorYellow;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	Color c;
	switch ( colorNum )
	{
	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
		break;

	case COLOR_LOCATION:
		c = g_ColorDarkGreen;
		break;

	case COLOR_ACHIEVEMENT:
		{
			vgui::IScheme *pSourceScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "SourceScheme" ) ); 
			if ( pSourceScheme )
			{
				c = pSourceScheme->GetColor( "SteamLightGreen", GetBgColor() );
			}
			else
			{
				c = g_ColorGrey;
			}
		}
		break;


	default:
		c = g_ColorYellow;
	}

	return Color( c[0], c[1], c[2], 255 );
}

int CHudChat::GetFilterForString( const char *pString )
{
	int iFilter = BaseClass::GetFilterForString( pString );

	if ( iFilter == CHAT_FILTER_NONE )
	{
		if ( !Q_stricmp( pString, "#CStrike_Name_Change" ) ) 
		{
			return CHAT_FILTER_NAMECHANGE;
		}
	}

	return iFilter;
}

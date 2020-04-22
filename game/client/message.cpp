//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Message.cpp
//
// implementation of CHudMessage class
//
#include "cbase.h"

#include "message.h"

#include "client_textmessage.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "VGuiMatSurface/IMatSystemSurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include <ctype.h>


#define NETWORK_MESSAGE1 "__NETMESSAGE__1"
#define NETWORK_MESSAGE2 "__NETMESSAGE__2"
#define NETWORK_MESSAGE3 "__NETMESSAGE__3"
#define NETWORK_MESSAGE4 "__NETMESSAGE__4"
#define NETWORK_MESSAGE5 "__NETMESSAGE__5"
#define NETWORK_MESSAGE6 "__NETMESSAGE__6"
#define MAX_NETMESSAGE	6

// Simultaneous message limit
#define MAX_TEXTMESSAGE_CHARS 2048

static const char *s_NetworkMessageNames[MAX_NETMESSAGE] = { NETWORK_MESSAGE1, NETWORK_MESSAGE2, NETWORK_MESSAGE3, NETWORK_MESSAGE4, NETWORK_MESSAGE5, NETWORK_MESSAGE6 };


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchHudText( const char *pszText )
{
	if ( pszText == NULL )
	{
		(GET_HUDELEMENT( CHudMessage ))->Reset();
	}
	else
	{
		(GET_HUDELEMENT( CHudMessage ))->MessageAdd( pszText );
	}
}

//
//-----------------------------------------------------
//

DECLARE_HUDELEMENT( CHudMessage );
DECLARE_HUD_MESSAGE( CHudMessage, HudText );
DECLARE_HUD_MESSAGE( CHudMessage, GameTitle );
DECLARE_HUD_MESSAGE( CHudMessage, HudMsg );

ITextMessage *textmessage = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMessage::CHudMessage( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudMessage" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );
	if( textmessage == NULL ) //HACKHACK: Fixes center print text in when MAX_SPLITSCREEN_PLAYERS is greater than 1
	{
		textmessage = this;
	}
	m_hFont = g_hFontTrebuchet24;
	m_hDefaultFont = m_hFont;
	// Clear memory out
	ResetCharacters();
}

CHudMessage::~CHudMessage()
{
	textmessage = NULL;
}

void CHudMessage::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::Init(void)
{
	HOOK_HUD_MESSAGE( CHudMessage, HudText );
	HOOK_HUD_MESSAGE( CHudMessage, GameTitle );
	HOOK_HUD_MESSAGE( CHudMessage, HudMsg );

	Reset();
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::VidInit( void )
{
	m_iconTitleHalf = HudIcons().GetIcon( "title_half" );
	m_iconTitleLife = HudIcons().GetIcon( "title_life" );
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::Reset( void )
{
 	memset( m_pMessages, 0, sizeof( m_pMessages[0] ) * maxHUDMessages );
	memset( m_startTime, 0, sizeof( m_startTime[0] ) * maxHUDMessages );
	
	m_gameTitleTime = 0;
	m_pGameTitle = NULL;
	m_bHaveMessage = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CHudMessage::FadeBlend( float fadein, float fadeout, float hold, float localTime )
{
	float fadeTime = fadein + hold;
	float fadeBlend;

	if ( localTime < 0 )
		return 0;

	if ( localTime < fadein )
	{
		fadeBlend = 1 - ((fadein - localTime) / fadein);
	}
	else if ( localTime > fadeTime )
	{
		if ( fadeout > 0 )
			fadeBlend = 1 - ((localTime - fadeTime) / fadeout);
		else
			fadeBlend = 0;
	}
	else
		fadeBlend = 1;

	return fadeBlend;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CHudMessage::XPosition( float x, int width, int totalWidth )
{
	int xPos;

	if ( x == -1 )
	{
		xPos = (ScreenWidth() - width) / 2;
	}
	else
	{
		if ( x < 0 )
			xPos = (1.0 + x) * ScreenWidth() - totalWidth;	// Align to right
		else
			xPos = x * ScreenWidth();
	}

	if ( xPos + width > ScreenWidth() )
		xPos = ScreenWidth() - width;
	else if ( xPos < 0 )
		xPos = 0;

	return xPos;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudMessage::YPosition( float y, int height )
{
	int yPos;

	if ( y == -1 )	// Centered?
		yPos = (ScreenHeight() - height) * 0.5;
	else
	{
		// Alight bottom?
		if ( y < 0 )
			yPos = (1.0 + y) * ScreenHeight() - height;	// Alight bottom
		else // align top
			yPos = y * ScreenHeight();
	}

	if ( yPos + height > ScreenHeight() )
		yPos = ScreenHeight() - height;
	else if ( yPos < 0 )
		yPos = 0;

	return yPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::MessageScanNextChar( void )
{
	int srcRed, srcGreen, srcBlue, destRed, destGreen, destBlue;
	int blend;

	srcRed = m_parms.pMessage->r1;
	srcGreen = m_parms.pMessage->g1;
	srcBlue = m_parms.pMessage->b1;
	blend = 0;	// Pure source

	destRed = destGreen = destBlue = 0;

	switch( m_parms.pMessage->effect )
	{
	// Fade-in / Fade-out
	case 0:
	case 1:
		destRed = destGreen = destBlue = 0;
		blend = m_parms.fadeBlend;
		break;

	case 2:
		m_parms.charTime += m_parms.pMessage->fadein;
		if ( m_parms.charTime > m_parms.time )
		{
			srcRed = srcGreen = srcBlue = 0;
			blend = 0;	// pure source
		}
		else
		{
			float deltaTime = m_parms.time - m_parms.charTime;

			destRed = destGreen = destBlue = 0;
			if ( m_parms.time > m_parms.fadeTime )
			{
				blend = m_parms.fadeBlend;
			}
			else if ( deltaTime > m_parms.pMessage->fxtime )
				blend = 0;	// pure dest
			else
			{
				destRed = m_parms.pMessage->r2;
				destGreen = m_parms.pMessage->g2;
				destBlue = m_parms.pMessage->b2;
				blend = 255 - (deltaTime * (1.0/m_parms.pMessage->fxtime) * 255.0 + 0.5);
			}
		}
		break;
	}
	if ( blend > 255 )
		blend = 255;
	else if ( blend < 0 )
		blend = 0;

	m_parms.r = ((srcRed * (255-blend)) + (destRed * blend)) >> 8;
	m_parms.g = ((srcGreen * (255-blend)) + (destGreen * blend)) >> 8;
	m_parms.b = ((srcBlue * (255-blend)) + (destBlue * blend)) >> 8;

#if 0
	if ( m_parms.pMessage->effect == 1 && m_parms.charTime != 0 )
	{
		textmessage->AddChar( m_parms.pMessage->r2, m_parms.pMessage->g2, m_parms.pMessage->b2, 255, m_parms.text );
	}
#endif
}


void CHudMessage::SetFont( HScheme scheme, const char *pFontName )
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( scheme );

	if ( pScheme )
	{
		bool bProportional = false;
#ifdef PORTAL2
		bProportional = true;
#endif
		vgui::HFont font = pScheme->GetFont( pFontName, bProportional );
		textmessage->SetFont( font );
		m_parms.font = font;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::MessageScanStart( void )
{
	switch( m_parms.pMessage->effect )
	{
	// Fade-in / out with flicker
	case 1:
	case 0:
		m_parms.fadeTime = m_parms.pMessage->fadein + m_parms.pMessage->holdtime;
		

		if ( m_parms.time < m_parms.pMessage->fadein )
		{
			m_parms.fadeBlend = ((m_parms.pMessage->fadein - m_parms.time) * (1.0/m_parms.pMessage->fadein) * 255);
		}
		else if ( m_parms.time > m_parms.fadeTime )
		{
			if ( m_parms.pMessage->fadeout > 0 )
				m_parms.fadeBlend = (((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255);
			else
				m_parms.fadeBlend = 255; // Pure dest (off)
		}
		else
			m_parms.fadeBlend = 0;	// Pure source (on)
		m_parms.charTime = 0;

		if ( m_parms.pMessage->effect == 1 && (rand()%100) < 10 )
			m_parms.charTime = 1;
		break;

	case 2:
		m_parms.fadeTime = (m_parms.pMessage->fadein * m_parms.length) + m_parms.pMessage->holdtime;
		
		if ( m_parms.time > m_parms.fadeTime && m_parms.pMessage->fadeout > 0 )
			m_parms.fadeBlend = (((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255);
		else
			m_parms.fadeBlend = 0;
		break;
	}

	m_parms.font = g_hFontTrebuchet24;

	if ( m_parms.vguiFontName != NULL && 
		m_parms.vguiFontName[ 0 ] )
	{
#ifdef PORTAL2
		SetFont( vgui::scheme()->GetScheme( "basemodui_scheme" ), m_parms.vguiFontName );
#else
		SetFont( vgui::scheme()->GetDefaultScheme(), m_parms.vguiFontName );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::MessageDrawScan( client_textmessage_t *pMessage, float time )
{
	int i, j, length, width;
	const wchar_t *pText;
	wchar_t textBuf[ 1024 ];

	{
		// look up in localization table
		// strip off any trailing newlines
		int len = Q_strlen( pMessage->pMessage );
		int tempLen = len + 2;
		char *localString = (char *)stackalloc( tempLen );
		Q_strncpy( localString, pMessage->pMessage, tempLen );
		if (len > 0 && V_iscntrl(localString[len - 1]))
		{
			localString[len - 1] = 0;
		}

		pText = g_pVGuiLocalize->Find( localString );
		if ( !pText ) 
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pMessage->pMessage, textBuf, sizeof( textBuf ) );
			pText = textBuf;
		}
	}

	const wchar_t *pPerm = pText;

	// Count lines
	m_parms.lines = 1;
	m_parms.time = time;
	m_parms.pMessage = pMessage;
	length = 0;
	width = 0;
	m_parms.totalWidth = 0;
	m_parms.vguiFontName = pMessage->pVGuiSchemeFontName;

	if ( m_parms.font == 0 )
	{
		if ( m_parms.vguiFontName != NULL && 
			m_parms.vguiFontName[ 0 ] )
		{
#ifdef PORTAL2
			SetFont( vgui::scheme()->GetScheme( "basemodui_scheme" ), m_parms.vguiFontName );
#else
			SetFont( vgui::scheme()->GetDefaultScheme(), m_parms.vguiFontName );
#endif
		}
		else
		{
			m_parms.font = g_hFontTrebuchet24;
		}
	}

	while ( *pText )
	{
		if ( *pText == '\n' )
		{
			m_parms.lines++;
			if ( width > m_parms.totalWidth )
				m_parms.totalWidth = width;
			width = 0;
		}
		else
		{
			width += vgui::surface()->GetCharacterWidth( m_parms.font, *pText );
		}
		pText++;
		length++;
	}
	if ( width > m_parms.totalWidth )
		m_parms.totalWidth = width;
	m_parms.length = length;

	int fontHeight = vgui::surface()->GetFontTall( m_parms.font );

	m_parms.totalHeight = ( m_parms.lines * fontHeight );
	
	m_parms.y = YPosition( pMessage->y, m_parms.totalHeight );
	pText = pPerm;

	m_parms.charTime = 0;

	float flBoxPixels = 0.0f;

	MessageScanStart();

	if ( pMessage->bRoundedRectBackdropBox )
	{
		// 2.0f since we have extra space at both the top and bottom
		flBoxPixels = pMessage->flBoxSize * fontHeight;

		// Draw the box
		int boxx = XPosition( pMessage->x, m_parms.totalWidth, m_parms.totalWidth );
		int boxy = YPosition( pMessage->y, m_parms.totalHeight );

		boxx -= flBoxPixels;
		boxy -= flBoxPixels * 0.5f;

		float flAlphaScale = clamp( ( 255.0f - (float)m_parms.fadeBlend ) / 255.0f, 0.0f, 1.0f );
		Color boxColor( 
			pMessage->boxcolor[ 0 ],
			pMessage->boxcolor[ 1 ],
			pMessage->boxcolor[ 2 ], 
			pMessage->boxcolor[ 3 ] * flAlphaScale );

		DrawBox( boxx, boxy, m_parms.totalWidth + 2.0f * flBoxPixels, m_parms.totalHeight + 2.0f * flBoxPixels * 0.5f, boxColor, 1.0f );
	}

	wchar_t line[ 512 ];
	for ( i = 0; i < m_parms.lines; i++ )
	{
		m_parms.lineLength = 0;
		m_parms.width = 0;
		while ( *pText && *pText != '\n' )
		{
			wchar_t c = *pText;
			line[m_parms.lineLength] = c;
			m_parms.width += vgui::surface()->GetCharacterWidth( m_parms.font, c);
			m_parms.lineLength++;
			if ( m_parms.lineLength > (ARRAYSIZE(line)-1) )
			{
				m_parms.lineLength = ARRAYSIZE(line)-1;
			}
			pText++;
		}
		pText++;		// Skip LF
		line[m_parms.lineLength] = 0;

		m_parms.x = XPosition( pMessage->x, m_parms.width, m_parms.totalWidth );

		textmessage->SetPosition( m_parms.x, m_parms.y );

		if (m_parms.fadeBlend > 255)
			m_parms.fadeBlend = 255;
		
		for ( j = 0; j < m_parms.lineLength; j++ )
		{
			m_parms.text = line[j];
			MessageScanNextChar();
			textmessage->AddChar( m_parms.r, m_parms.g, m_parms.b, 255 - m_parms.fadeBlend, m_parms.text );
		}

		m_parms.y += vgui::surface()->GetFontTall( m_parms.font );
	}

	// Restore default font
	textmessage->SetDefaultFont();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMessage::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && 
		( m_bHaveMessage || m_Messages.Count() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::Paint()
{
	int i, drawn;
	client_textmessage_t *pMessage;
	float endTime;

	drawn = 0;

	if ( m_gameTitleTime > 0 )
	{
		float localTime = gpGlobals->curtime - m_gameTitleTime;
		float brightness;

		// Maybe timer isn't set yet
		if ( m_gameTitleTime > gpGlobals->curtime )
		{
			m_gameTitleTime = gpGlobals->curtime;
		}

		if ( localTime > (m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout) )
		{
			m_gameTitleTime = 0;
		}
		else
		{
			brightness = FadeBlend( m_pGameTitle->fadein, m_pGameTitle->fadeout, m_pGameTitle->holdtime, localTime );

			int halfWidth = m_iconTitleHalf->Width();
			int fullWidth = halfWidth + m_iconTitleLife->Width();
			int fullHeight = m_iconTitleHalf->Height();

			int x = XPosition( m_pGameTitle->x, fullWidth, fullWidth );
			int y = YPosition( m_pGameTitle->y, fullHeight );

			m_iconTitleHalf->DrawSelf( x, y, Color( m_pGameTitle->r1, m_pGameTitle->g1, m_pGameTitle->b1, brightness * 255 ) );
			m_iconTitleLife->DrawSelf( x + halfWidth, y, Color( m_pGameTitle->r1, m_pGameTitle->g1, m_pGameTitle->b1, brightness * 255 ) );
			drawn = 1;
		}
	}

	// Fixup level transitions
	for ( i = 0; i < maxHUDMessages; i++ )
	{
		// Assume m_parms.time contains last time
		if ( m_pMessages[i] )
		{
			pMessage = m_pMessages[i];
			if ( m_startTime[i] > gpGlobals->curtime )
				m_startTime[i] = gpGlobals->curtime + m_parms.time - m_startTime[i] + 0.2;	// Server takes 0.2 seconds to spawn, adjust for this
		}
	}

	for ( i = 0; i < maxHUDMessages; i++ )
	{
		if ( m_pMessages[i] )
		{
			pMessage = m_pMessages[i];

			// This is when the message is over
			switch( pMessage->effect )
			{
			case 0:
			case 1:
				endTime = m_startTime[i] + pMessage->fadein + pMessage->fadeout + pMessage->holdtime;
				break;
			
			// Fade in is per character in scanning messages
			case 2:
				endTime = m_startTime[i] + (pMessage->fadein * strlen( pMessage->pMessage )) + pMessage->fadeout + pMessage->holdtime;
				break;

			default:
				endTime = 0;
				break;
			}

			if ( gpGlobals->curtime <= endTime )
			{
				float messageTime = gpGlobals->curtime - m_startTime[i];

				// Draw the message
				// effect 0 is fade in/fade out
				// effect 1 is flickery credits
				// effect 2 is write out (training room)
				MessageDrawScan( pMessage, messageTime );

				drawn++;
			}
			else
			{
				// The message is over
				m_pMessages[i] = NULL;
			}
		}
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = gpGlobals->curtime;

	// Did we draw any messages?
	if ( !drawn )
	{
		m_bHaveMessage = false;
	}

	PaintCharacters();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::MessageAdd( const char *pName )
{
	int i;

	float time = gpGlobals->curtime;

	client_textmessage_t *pMessage = NULL;

	if ( pName[0] == '#' )
	{
		pMessage = TextMessageGet( pName+1 );
	}
	else
	{
		pMessage = TextMessageGet( pName );
	}

	if ( !pMessage )
		return;

	if ( pMessage->pClearMessage )
	{
		for ( i = 0; i < maxHUDMessages; i++ )
		{
			if ( m_pMessages[ i ] && !Q_stricmp( m_pMessages[ i ]->pName, pMessage->pClearMessage ) )
			{
				m_startTime[ i ] = 0.0f;
				m_pMessages[ i ] = NULL;
				break;
			}
		}
	}

	for ( i = 0; i < maxHUDMessages; i++ )
	{
		if ( !m_pMessages[i] )
		{
			m_pMessages[i] = pMessage;
			m_startTime[i] = time;
			break;
		}
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = time;

	m_bHaveMessage = true;
	// Force this now so that SCR_UpdateScreen will paint the panel immediately!!!
	SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMessage::MsgFunc_HudText(const CCSUsrMsg_HudText &msg)
{
	MessageAdd( msg.text().c_str() );
	return true;
}

#include "ivieweffects.h"
#include "shake.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMessage::MsgFunc_GameTitle(const CCSUsrMsg_GameTitle &msg)
{
	m_pGameTitle = TextMessageGet( "GAMETITLE" );
	if ( m_pGameTitle != NULL )
	{
		m_gameTitleTime = gpGlobals->curtime;

		m_bHaveMessage = true;
	}

//	if ( READ_BYTE() )
	{
		ScreenFade_t sf;
		memset( &sf, 0, sizeof( sf ) );
		sf.a = 255;
		sf.r = 0;
		sf.g = 0;
		sf.b = 0;
		sf.duration = (float)(1<<SCREENFADE_FRACBITS) * 5.0f;
		sf.holdTime = (float)(1<<SCREENFADE_FRACBITS) * 1.0f;
		sf.fadeFlags = FFADE_IN | FFADE_PURGE;
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetViewEffects()->Fade( sf );
		}

		Msg( "%i gametitle fade\n", gpGlobals->framecount );
	}

	return true;
}


bool CHudMessage::MsgFunc_HudMsg(const CCSUsrMsg_HudMsg &msg)
{
// Position command $position x y 
// x & y are from 0 to 1 to be screen resolution independent
// -1 means center in each dimension
// Effect command $effect <effect number>
// effect 0 is fade in/fade out
// effect 1 is flickery credits
// effect 2 is write out (training room)
// Text color r g b command $color
// Text color r g b command $color2
// fadein time fadeout time / hold time
// $fadein (message fade in time - per character in effect 2)
// $fadeout (message fade out time)
// $holdtime (stay on the screen for this long)

	int channel = msg.channel() % MAX_NETMESSAGE;	// Pick the buffer
	
	client_textmessage_t *pNetMessage = TextMessageGet( s_NetworkMessageNames[ channel ] );
	
	if ( !pNetMessage || !pNetMessage->pMessage )
		return true;

	pNetMessage->x = msg.pos().x();
	pNetMessage->y = msg.pos().y();

	pNetMessage->r1 = msg.clr1().r();
	pNetMessage->g1 = msg.clr1().g();
	pNetMessage->b1 = msg.clr1().b();
	pNetMessage->a1 = msg.clr1().a();

	pNetMessage->r2 = msg.clr2().r();
	pNetMessage->g2 = msg.clr2().g();
	pNetMessage->b2 = msg.clr2().b();
	pNetMessage->a2 = msg.clr2().a();

	pNetMessage->effect = msg.effect();

	pNetMessage->fadein = msg.fade_in_time();
	pNetMessage->fadeout = msg.fade_out_time();
	pNetMessage->holdtime = msg.hold_time();
	pNetMessage->fxtime	= msg.fx_time();

#ifdef PORTAL2
	// hack to make the chapter title channel define the font size in Portal 2
	if ( channel == 2 || channel == 3 )
	{
		const char *pFontName;
		if ( channel == 2 )
			pFontName = "InGameChapterTitle";
		else
			pFontName = "InGameChapterSubtitle";	
			
		pNetMessage->pVGuiSchemeFontName = pFontName;	
	}

#endif
	
	pNetMessage->pName = s_NetworkMessageNames[ channel ];

	// see tmessage.cpp why 512
	Q_strncpy( (char*)pNetMessage->pMessage, msg.text().c_str(), 512 );

	MessageAdd( pNetMessage->pName );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get font sizes
// Input  : *pWidth - 
// Output : int
//-----------------------------------------------------------------------------
int CHudMessage::GetFontInfo( FONTABC *pABCs, vgui::HFont hFont )
{
	int i;

	if ( !hFont )
	{
		hFont = m_hFont;
	}

	if ( !hFont )
		return 0;

	if ( pABCs )
	{
		for ( i =0; i < 256; i++ )
		{
			int a, b, c;
			vgui::surface()->GetCharABCwide( hFont, (char)i, a, b, c );
			pABCs[i].abcA = a;
			pABCs[i].abcB = b;
			pABCs[i].abcC = c;
			pABCs[i].total = a+b+c;
		}
	}

	return vgui::surface()->GetFontTall( hFont );
}

//-----------------------------------------------------------------------------
// Purpose: Clear all messages out of active list, etc.
//-----------------------------------------------------------------------------
void CHudMessage::ResetCharacters( void )
{
	m_Messages.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Grab next free message, if any
// Output : CTextMessagePanel::message_t
//-----------------------------------------------------------------------------
CHudMessage::message_t *CHudMessage::AllocMessage( void )
{
	message_t *msg;

	if ( m_Messages.Count() >= MAX_TEXTMESSAGE_CHARS )
		return NULL;

	msg = &m_Messages[ m_Messages.AddToTail() ];

	msg->type = TYPE_UNKNOWN;
	msg->x = 0;
	msg->y = 0;
	msg->ch = 0;
	msg->r = 0;
	msg->g = 0;
	msg->b = 0;
	msg->a = 0;
	msg->font = 0;

	SetVisible( true );

	return msg;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CHudMessage::SetPosition( int x, int y )
{
	message_t *msg = AllocMessage();
	if ( !msg )
		return;

	msg->type = TYPE_POSITION;

	// Used fields
	msg->x = x;
	msg->y = y;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a character to the active list, if possible
// Input  : x - 
//			y - 
//			r - 
//			g - 
//			b - 
//			a - 
//			ch - 
// Output : int
//-----------------------------------------------------------------------------
void CHudMessage::AddChar( int r, int g, int b, int a, wchar_t ch )
{
	message_t *msg = AllocMessage();
	if ( !msg )
		return;

	msg->type = TYPE_CHARACTER;

	// Used fields
	msg->r = r;
	msg->g = g;
	msg->b = b;
	msg->a = a;
	msg->ch = ch;
}

//-----------------------------------------------------------------------------
// Purpose: Determine width and height of specified string
// Input  : *wide - 
//			*tall - 
//			*string - 
//-----------------------------------------------------------------------------
void CHudMessage::GetTextExtents( int *wide, int *tall, const char *string )
{
	*wide = g_pMatSystemSurface->DrawTextLen( m_hFont, "%s", (char *)string );
	*tall = vgui::surface()->GetFontTall( m_hFont );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::SetFont( vgui::HFont hCustomFont )
{
	m_hFont = hCustomFont;

	message_t *msg = AllocMessage();
	if ( !msg )
		return;

	msg->type = TYPE_FONT;

	// Used fields
	msg->font = m_hFont;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMessage::SetDefaultFont( void )
{
	SetFont( m_hDefaultFont );
}

//-----------------------------------------------------------------------------
// Purpose: Draw current text items
//-----------------------------------------------------------------------------
void CHudMessage::PaintCharacters() 
{
	int xpos = 0, ypos = 0;
	vgui::surface()->DrawSetTextFont( m_hFont );

	int messageCount = m_Messages.Count();
	for ( int i = 0 ; i < messageCount; ++i )
	{
		message_t *msg = &m_Messages[ i ];

		switch ( msg->type )
		{
		default:
		case TYPE_UNKNOWN:
			Assert( 0 );
			break;
		case TYPE_POSITION:
			xpos = msg->x;
			ypos = msg->y;
			break;
		case TYPE_FONT:
			m_hFont = msg->font;
			vgui::surface()->DrawSetTextFont( m_hFont );
			break;
		case TYPE_CHARACTER:
			if ( m_hFont )
			{
				int a, b, c;
				vgui::surface()->GetCharABCwide( m_hFont, msg->ch, a, b, c );

				if ( msg->ch > 32 )
				{
					vgui::surface()->DrawSetTextColor( msg->r,  msg->g,  msg->b,  msg->a );
					vgui::surface()->DrawSetTextPos( xpos, ypos );
					vgui::surface()->DrawUnicodeChar( msg->ch );
				}
				xpos += a + b + c;
			}
			break;
		}
	}

	ResetCharacters();
}

void CHudMessage::GetLength( int *wide, int *tall, const char *string )
{
	GetTextExtents( wide, tall, string );
}

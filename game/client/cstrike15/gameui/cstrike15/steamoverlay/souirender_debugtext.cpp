//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#include "souirender_pch.h"
// additional #includes must be here
#include "souirender_pchend.h"

static ConVar soui_debugtext( "soui_debugtext", "0", FCVAR_DEVELOPMENTONLY );
class SOUIrenderPanel_DebugText : public ISOUIrenderInputHandler
{
public:
	virtual void Render()
	{
		if ( !soui_debugtext.GetBool() )
			return;

		SOUIrender_RegisterInputHandler( this, true ); // always be the first to handle events

		// Expire input buffers
		if ( m_flTimestamp && ( ( Plat_FloatTime() - m_flTimestamp ) > soui_debugtext.GetFloat() ) )
		{
			m_flTimestamp = 0;
			Q_memset( m_chBuffer, 0, sizeof( m_chBuffer ) );
		}

		g_pISteamOverlayRenderHost->FillSetColor( SteamOverlayColor_t( 0, 0, 0, 128 ) );
		g_pISteamOverlayRenderHost->FillRect( SteamOverlayRect_t(
			AdjustUiPosX( 0, false, -1 ), AdjustUiPosY( 20, true, -1 ),
			AdjustUiPosX( 0, false, 0 ), AdjustUiPosY( 50, true, -1 )
			) );

		g_pISteamOverlayRenderHost->TextSetFont( SOUIrenderGetFontHandle( kSOUIrenderFont_DefaultFixedOutline ) );
		g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 255, 0, 0, 255 ) );
		g_pISteamOverlayRenderHost->TextSetPos( AdjustUiPosX( 40, true, -1 ), AdjustUiPosY( 25, true, -1 ) );

		wchar_t wchBuf[128];
		Q_snwprintf( wchBuf, 128, L"[Steam overlay @ %.2f]" PRI_WS_FOR_WS, Plat_FloatTime(), m_chBuffer );
		g_pISteamOverlayRenderHost->TextDrawStringW( wchBuf );
	}

	virtual uint32 HandleInputEvent( int iCode, int iValue )
	{
		if ( 0 == m_chBuffer[0] )
		{
			m_flTimestamp = Plat_FloatTime();
		}
		if ( 0 == m_chBuffer[50] )
		{
			wchar_t *chBuffer = m_chBuffer + Q_wcslen( m_chBuffer );
			switch ( SOUIGetBaseButtonCode( iCode ) )
			{
			case KEY_XBUTTON_A: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" A%d", iValue ); break;
			case KEY_XBUTTON_B: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" B%d", iValue ); break;
			case KEY_XBUTTON_X: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" X%d", iValue ); break;
			case KEY_XBUTTON_Y: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" Y%d", iValue ); break;
			case KEY_XBUTTON_START: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" ST%d", iValue ); break;
			case KEY_XBUTTON_INACTIVE_START: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" ST*%d", iValue ); break;
			case KEY_XBUTTON_BACK: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" BK%d", iValue ); break;
			case KEY_XBUTTON_LEFT_SHOULDER: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" Ls%d", iValue ); break;
			case KEY_XBUTTON_RIGHT_SHOULDER: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" Rs%d", iValue ); break;
			case KEY_XBUTTON_LEFT: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" <%d", iValue ); break;
			case KEY_XBUTTON_RIGHT: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" >%d", iValue ); break;
			case KEY_XBUTTON_UP: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" ^%d", iValue ); break;
			case KEY_XBUTTON_DOWN: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" v%d", iValue ); break;
			case KEY_XBUTTON_STICK1: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (1)%d", iValue ); break;
			case KEY_XBUTTON_STICK2: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (2)%d", iValue ); break;
				// analog values:
			case KEY_XBUTTON_LTRIGGER: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" Lt%d", iValue ); break;
			case KEY_XBUTTON_RTRIGGER: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" Rt%d", iValue ); break;
			case KEY_XSTICK1_RIGHT: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (1)>%d", iValue ); break;
			case KEY_XSTICK1_LEFT: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (1)<%d", iValue ); break;
			case KEY_XSTICK1_UP: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (1)^%d", iValue ); break;
			case KEY_XSTICK1_DOWN: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (1)v%d", iValue ); break;
			case KEY_XSTICK2_RIGHT: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (2)>%d", iValue ); break;
			case KEY_XSTICK2_LEFT: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (2)<%d", iValue ); break;
			case KEY_XSTICK2_UP: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (2)^%d", iValue ); break;
			case KEY_XSTICK2_DOWN: Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L" (2)v%d", iValue ); break;
			default: Q_snwprintf( chBuffer, ARRAYSIZE( chBuffer ), L" (?x%X)%d", iCode, iValue ); break;
			}

			if ( int userId = SOUIGetJoystickForCode( iCode ) )
			{
				Q_snwprintf( chBuffer, ARRAYSIZE( m_chBuffer ), L"/j%d", userId );
			}
		}
		if ( iCode == -1 && !iValue )
		{
			// Overlay is hiding
			Q_memset( m_chBuffer, 0, sizeof( m_chBuffer ) );
			m_flTimestamp = 0;
		}
		return INPUT_EVENT_RESULT_FALLTHROUGH; // let anybody else handle
	}

	wchar_t m_chBuffer[ 64 ];
	float m_flTimestamp;
};
SOUIrenderDeclarePanel( SOUIrenderPanel_DebugText );

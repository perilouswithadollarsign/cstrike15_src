//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#include "souirender_pch.h"
// additional #includes must be here
#include "souirender_pchend.h"

class CSteamOverlayRender : public ISteamOverlayRender
{
public:
	CSteamOverlayRender();

public:
	virtual void Initialize( ISteamOverlayRenderHost *pSteamOverlayRenderHost );
	virtual void Shutdown();

	virtual void Render();

	virtual bool HandleInputEvent( int iCode, int iValue );

public:
	bool m_bOverlayFullyActive;
	SteamOverlayFontHandle_t m_arrFontHandles[ kSOUIrenderFont_Count ];
	CUtlVector< ISOUIrenderInputHandler * > m_arrInputHandlers;
};

static CSteamOverlayRender g_CSteamOverlayRender;
ISteamOverlayRender *g_pISteamOverlayRender = &g_CSteamOverlayRender;

ISteamOverlayRenderHost *g_pISteamOverlayRenderHost;
UiPosAdjustment_t g_ISteamOverlayRenderHost_PosAdjustment;
SteamOverlayFontHandle_t *g_ISteamOverlayRenderHost_FontHandles;

SOUIrenderDeclareExternPanel( SOUIrenderPanel_FriendChat )
SOUIrenderDeclareExternPanel( SOUIrenderPanel_Notification );
SOUIrenderDeclareExternPanel( SOUIrenderPanel_DebugText );

CSteamOverlayRender::CSteamOverlayRender()
{
	m_bOverlayFullyActive = false;
	Q_memset( m_arrFontHandles, 0, sizeof( m_arrFontHandles ) );
}

void CSteamOverlayRender::Initialize( ISteamOverlayRenderHost *pSteamOverlayRenderHost )
{
	g_pISteamOverlayRenderHost = pSteamOverlayRenderHost;
	
	// Font handles
	m_arrFontHandles[ kSOUIrenderFont_DefaultFixedOutline ] = g_pISteamOverlayRenderHost->FontGetHandle( "DefaultFixedOutline" );
	g_ISteamOverlayRenderHost_FontHandles = &m_arrFontHandles[0];

	// Pos adjustment
	SteamOverlayRenderInfo_t sori;
	g_pISteamOverlayRenderHost->GetRenderInfo( sori );
	g_ISteamOverlayRenderHost_PosAdjustment.m_size[0] = sori.m_nScreenWidth;
	g_ISteamOverlayRenderHost_PosAdjustment.m_size[1] = sori.m_nScreenHeight;
	g_ISteamOverlayRenderHost_PosAdjustment.m_flScaleFactor[0] =
		g_ISteamOverlayRenderHost_PosAdjustment.m_flScaleFactor[1] =
		sori.m_nScreenHeight / 480.0f;

	SOUIrenderReferenceExternPanel( SOUIrenderPanel_FriendChat )->Initialize();
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_Notification )->Initialize();
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_DebugText )->Initialize();
}

void CSteamOverlayRender::Shutdown()
{
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_DebugText )->Shutdown();
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_Notification )->Shutdown();
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_FriendChat )->Shutdown();
}

void SOUIrender_RegisterInputHandler( ISOUIrenderInputHandler *p, bool bAdd )
{
	// Cannot have the same handler registered twice
	while ( g_CSteamOverlayRender.m_arrInputHandlers.FindAndRemove( p ) ) continue;
	if ( bAdd )
		g_CSteamOverlayRender.m_arrInputHandlers.AddToHead( p );
		
}

void CSteamOverlayRender::Render()
{
	if ( m_bOverlayFullyActive )
	{
		{	// Darken background
			g_pISteamOverlayRenderHost->FillSetColor( SteamOverlayColor_t( 0, 0, 0, 220 ) );
			g_pISteamOverlayRenderHost->FillRect( SteamOverlayRect_t(
				AdjustUiPosX( 0, false, -1 ), AdjustUiPosY( 0, false, -1 ),
				AdjustUiPosX( 0, false, 1 ), AdjustUiPosY( 0, false, 1 )
				) );
		}
		SOUIrenderReferenceExternPanel( SOUIrenderPanel_FriendChat )->Render();
	}
	// Panels rendering on top of overlay
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_Notification )->Render();
	SOUIrenderReferenceExternPanel( SOUIrenderPanel_DebugText )->Render();
}

bool CSteamOverlayRender::HandleInputEvent( int iCode, int iValue )
{
	if ( iCode == -1 )
		m_bOverlayFullyActive = !!iValue;

	// Let registered handlers handle the event
	for ( int k = 0; k < m_arrInputHandlers.Count(); ++ k )
	{
		uint32 uiResult = m_arrInputHandlers[k]->HandleInputEvent( iCode, iValue );
		if ( uiResult & ISOUIrenderInputHandler::INPUT_EVENT_RESULT_CLOSEOVERLAY )
			return false;
		if ( uiResult & ISOUIrenderInputHandler::INPUT_EVENT_RESULT_HANDLED )
			return true;
	}

	// End the overlay when unhandled SELECT key or CANCEL key
	if ( ( SOUIGetBaseButtonCode( iCode ) == KEY_XBUTTON_BACK ) && iValue )
		return false;
	if ( ( SOUIGetBaseButtonCode( iCode ) == KEY_XBUTTON_B ) && iValue )
		return false;

	return true;
}

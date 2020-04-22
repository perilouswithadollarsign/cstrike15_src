//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#include "souirender_pch.h"
// additional #includes must be here
#include "souirender_pchend.h"

static ConVar soui_notification_time( "soui_notification_time", "10", FCVAR_DEVELOPMENTONLY );
static ConVar soui_notification_fade( "soui_notification_fade", "1", FCVAR_DEVELOPMENTONLY );
class SOUIrenderPanel_Notification : public ISOUIrenderInputHandler
{
public:
	virtual void Render()
	{
		if ( !m_flTimestamp )
			return;

		float flTime = Plat_FloatTime();
		if ( flTime >= m_flTimestamp + soui_notification_time.GetFloat() )
		{
			m_flTimestamp = 0;
			return;
		}
		
		float flFadeFactor = 255;
		if ( flTime < m_flTimestamp + soui_notification_fade.GetFloat() )
			flFadeFactor = ( flTime - m_flTimestamp ) * 255 / soui_notification_fade.GetFloat();
		else if ( flTime > m_flTimestamp + soui_notification_time.GetFloat() - soui_notification_fade.GetFloat() )
			flFadeFactor = ( m_flTimestamp + soui_notification_time.GetFloat() - flTime ) * 255 / soui_notification_fade.GetFloat();

		enum Consts_t
		{
			kMarginEdgeX = 40,
			kMarginEdgeY = 25,
			kMargin = 20,
			kLineHeight = 20,
			kLineWidth = 200,
			
			kNumLines = 2,
			kLine0y = kMarginEdgeY + kNumLines * kLineHeight,
			kLine1y = kLine0y - kLineHeight,
			kLineLeft = kLineWidth + kMarginEdgeX,

			kImageSize = 40,
			kImageLeft = kLineLeft + kMargin + kImageSize,
			
			kTotalHeight = kLine0y + kMargin,
			kTotalWidth = kImageLeft + kMargin,
		};

		// Render background
		g_pISteamOverlayRenderHost->FillSetColor( SteamOverlayColor_t( 0, 0, 0, flFadeFactor ) );
		g_pISteamOverlayRenderHost->FillRect( SteamOverlayRect_t(
			AdjustUiPosX( -kTotalWidth, true, 1 ), AdjustUiPosY( -kTotalHeight, true, 1 ),
			AdjustUiPosX( 0, false, 1 ), AdjustUiPosY( 0, false, 1 )
			) );

		// Render texture
		g_pISteamOverlayRenderHost->FillSetColor( SteamOverlayColor_t( 255, 255, 255, flFadeFactor ) );
		g_pISteamOverlayRenderHost->TextureBind( m_iTextureHandle );
		g_pISteamOverlayRenderHost->TextureRect( SteamOverlayRect_t(
			AdjustUiPosX( -kImageLeft, true, 1 ), AdjustUiPosY( -kLine0y, true, 1 ),
			AdjustUiPosX( -kImageLeft + kImageSize, true, 1 ), AdjustUiPosY( -kLine0y + kImageSize, true, 1 )
			) );

		// Render text
		g_pISteamOverlayRenderHost->TextSetFont( SOUIrenderGetFontHandle( kSOUIrenderFont_DefaultFixedOutline ) );
		g_pISteamOverlayRenderHost->TextSetColor( SteamOverlayColor_t( 0, 255, 0, 255 ) );
		
		g_pISteamOverlayRenderHost->TextSetPos( AdjustUiPosX( -kLineLeft, true, 1 ), AdjustUiPosY( -kLine0y, true, 1 ) );
		g_pISteamOverlayRenderHost->TextDrawStringW( m_wszNameBuf );
		
		g_pISteamOverlayRenderHost->TextSetPos( AdjustUiPosX( -kLineLeft, true, 1 ), AdjustUiPosY( -kLine1y, true, 1 ) );
		g_pISteamOverlayRenderHost->TextDrawStringW( m_wszBuf );
	}

	void AddNotification( XUID xuid, wchar_t const *wszText )
	{
		m_flTimestamp = Plat_FloatTime();
		m_xuid = xuid;
		Q_wcsncpy( m_wszBuf, wszText, sizeof( m_wszBuf ) );
		
		// Get name string
		char const *szName = steamapicontext->SteamFriends()->GetFriendPersonaName( xuid );
		g_pVGuiLocalize->ConvertANSIToUnicode( szName, m_wszNameBuf, sizeof( m_wszNameBuf ) );

		// Get image
		if ( !m_iTextureHandle || !g_pISteamOverlayRenderHost->TextureIsValid( m_iTextureHandle ) )
		{
			m_iTextureHandle = g_pISteamOverlayRenderHost->TextureCreate();
		}
		bool bTextureValid = false;
		uint32 iWidth = 0, iHeight = 0;
		int iAvatar = steamapicontext->SteamFriends()->GetMediumFriendAvatar( xuid );
		if ( steamapicontext->SteamUtils()->GetImageSize( iAvatar, &iWidth, &iHeight ) &&
			(iWidth>0) && (iHeight>0) )
		{
			CUtlBuffer bufRGBA;
			bufRGBA.EnsureCapacity( iWidth * iHeight * 4 );
			if ( steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, (uint8*) bufRGBA.Base(), iWidth * iHeight * 4 ) )
			{
				bTextureValid = true;
				g_pISteamOverlayRenderHost->TextureSetRGBA( m_iTextureHandle, (uint8*) bufRGBA.Base(), iWidth, iHeight );
			}
		}
		
		if ( !bTextureValid )
		{
			g_pISteamOverlayRenderHost->TextureSetFile( m_iTextureHandle, "icon_lobby" );
		}
	}

public:
	float m_flTimestamp;
	int m_iTextureHandle;
	XUID m_xuid;
	wchar_t m_wszNameBuf[64];
	wchar_t m_wszBuf[256];
};
SOUIrenderDeclarePanel( SOUIrenderPanel_Notification );

void SOUIrender_AddNotification( XUID xuid, wchar_t const *wszText )
{
	SOUIrenderReferencePanel( SOUIrenderPanel_Notification ).AddNotification( xuid, wszText );
}

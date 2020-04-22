//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "UIGameData.h"
#include "UIAvatarImage.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGameUiAvatarImage::CGameUiAvatarImage( void )
{
	m_bValid = false;
	m_flFetchedTime = 0.0f;
	m_flPaintedTime = 0.0f;
	m_iTextureID = ( -1 );
	m_nRefcount = 0;
	m_nAvatarSize = MEDIUM;

#ifdef _X360
	m_eAsyncState = STATE_DEFAULT;
	Q_memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
	m_xXUID = 0;
#endif
}

CGameUiAvatarImage::~CGameUiAvatarImage( void )
{
	if ( vgui::surface() && m_iTextureID != -1 )
	{
		vgui::surface()->DestroyTextureID( m_iTextureID );
		m_iTextureID = -1;
	}
}

void CGameUiAvatarImage::ClearAvatarXUID( void )
{
	m_bValid = false;
	m_flFetchedTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGameUiAvatarImage::SetAvatarXUID( XUID xuid, AvatarSize_t nSize /*= MEDIUM*/ )
{
	ClearAvatarXUID();

#ifndef NO_STEAM
	if ( steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		m_nAvatarSize = nSize;
		int iAvatar;
		if ( nSize == SMALL )
		{
			iAvatar = steamapicontext->SteamFriends()->GetSmallFriendAvatar( xuid );
		}
		else if ( nSize == MEDIUM )
		{
			iAvatar = steamapicontext->SteamFriends()->GetMediumFriendAvatar( xuid );
		}
		else // nSize == LARGE
		{
			iAvatar = steamapicontext->SteamFriends()->GetLargeFriendAvatar( xuid );
		}

		/*
		// See if it's in our list already
		*/

		uint32 wide, tall;
		if ( steamapicontext->SteamUtils()->GetImageSize( iAvatar, &wide, &tall ) )
		{
			bool bUseSteamImage = true;
			if ( wide == 0 || tall == 0 )
			{
				// attempt to handle rare data integrity issue, avatar got lost
				bUseSteamImage = false;
				// mock up solid white as 64x64
				wide = tall = 64;
			}			

			int cubImage = wide * tall * 4;
			m_bufRgbaBuffer.EnsureCapacity( cubImage );
			memset( m_bufRgbaBuffer.Base(), 0xFF, cubImage );
			if ( bUseSteamImage )
			{
				steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, (byte*)m_bufRgbaBuffer.Base(), cubImage );
			}
			InitFromRGBA( (byte*)m_bufRgbaBuffer.Base(), wide, tall );
		}
	}
#elif defined( _X360 )
	X360_ResetAsyncImageState();
	m_xXUID = xuid;
	return true;
#endif

	return m_bValid;
}

void CGameUiAvatarImage::OnFinalRelease()
{
#ifdef _X360
	X360_ResetAsyncImageState();
#endif
	if ( vgui::surface() && m_iTextureID != -1 )
	{
		vgui::surface()->DestroyTextureID( m_iTextureID );
		m_iTextureID = -1;
	}
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUiAvatarImage::InitFromRGBA( const byte *rgba, int width, int height )
{
	// Texture size may be changing, so re-create
	if ( m_iTextureID != -1 )
	{
		vgui::surface()->DestroyTextureID( m_iTextureID );
		m_iTextureID = -1;
	}
	m_iTextureID = vgui::surface()->CreateNewTextureID( true );

	if ( rgba )
		vgui::surface()->DrawSetTextureRGBA( m_iTextureID, rgba, width, height );
	else
		vgui::surface()->DrawSetTextureFile( m_iTextureID, "icon_lobby", true, false );

	int screenWide, screenTall;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	m_nWide = width * ( ( (float) screenWide ) / 640.0f );
	m_nTall = height * ( ( (float) screenTall ) / 480.0f );
	m_Color = Color( 255, 255, 255, 255 );

	m_bValid = true;
	m_flFetchedTime = Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUiAvatarImage::Paint( void )
{
	m_flPaintedTime = Plat_FloatTime();
#ifdef _X360
	X360_UpdateImageState();
#endif
	if ( m_bValid )
	{
		vgui::surface()->DrawSetColor( m_Color );
		vgui::surface()->DrawSetTexture( m_iTextureID );
		vgui::surface()->DrawTexturedRect( m_nX, m_nY, m_nX + m_nWide, m_nY + m_nTall );
	}
}

#ifdef _X360
void CGameUiAvatarImage::X360_ResetAsyncImageState()
{
	switch ( m_eAsyncState )
	{
	case STATE_AWAITING_KEY:
	case STATE_AWAITING_RGBA:
		if ( !XHasOverlappedIoCompleted( &m_xOverlapped ) )
			XCancelOverlapped( &m_xOverlapped );
		Q_memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
		break;
	}
	m_eAsyncState = STATE_DEFAULT;
}

void CGameUiAvatarImage::X360_UpdateImageState()
{
	if ( !ThreadInMainThread() )
		return;

	switch ( m_eAsyncState )
	{
	case STATE_DEFAULT:
		{
			m_xSetting = XPROFILE_GAMERCARD_PICTURE_KEY;
			m_xCbResult = 0;
			DWORD ret = xonline->XUserReadProfileSettingsByXuid( 0, XBX_GetPrimaryUserId(), 1, &m_xXUID, 1, &m_xSetting, &m_xCbResult, NULL, NULL );
			if ( ret == ERROR_INSUFFICIENT_BUFFER )
			{
				m_xBufKey.EnsureCapacity( m_xCbResult );
				XUSER_READ_PROFILE_SETTING_RESULT *pSetting = ( XUSER_READ_PROFILE_SETTING_RESULT * ) m_xBufKey.Base();
				Q_memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
				ret = xonline->XUserReadProfileSettingsByXuid( 0, XBX_GetPrimaryUserId(), 1, &m_xXUID, 1, &m_xSetting, &m_xCbResult, pSetting, &m_xOverlapped );
				if ( ( ret == ERROR_SUCCESS ) || ( ret == ERROR_IO_PENDING ) )
					m_eAsyncState = STATE_AWAITING_KEY;
			}
		}
		break;

	case STATE_AWAITING_KEY:
		if ( XHasOverlappedIoCompleted( &m_xOverlapped ) )
		{
			DWORD dwResult = 0;
			if ( ( ERROR_SUCCESS != XGetOverlappedResult( &m_xOverlapped, &dwResult, false ) ) ||
				( ERROR_SUCCESS != dwResult ) )
			{
				m_eAsyncState = STATE_DEFAULT;
				return;
			}
			XUSER_READ_PROFILE_SETTING_RESULT *pSetting = ( XUSER_READ_PROFILE_SETTING_RESULT * ) m_xBufKey.Base();
			if ( pSetting->dwSettingsLen && pSetting->pSettings )
			{
				int cubImage = 64 * 64 * 4;
				m_bufRgbaBuffer.EnsureCapacity( cubImage );
				memset( m_bufRgbaBuffer.Base(), 0xFF, cubImage );
				DWORD ret = XUserReadGamerPictureByKey( &pSetting->pSettings[0].data, 0, (byte*)m_bufRgbaBuffer.Base(), 64*4, 64,
					NULL ); // sync read
					// &m_xOverlapped );
				if ( ( ret == ERROR_SUCCESS ) || ( ret == ERROR_IO_PENDING ) )
				{
					m_eAsyncState = STATE_AWAITING_RGBA;
					goto state_rgba_ready;
				}
				else
					m_eAsyncState = STATE_DEFAULT;
			}
		}
		break;

	case STATE_AWAITING_RGBA:
	state_rgba_ready:
		if ( XHasOverlappedIoCompleted( &m_xOverlapped ) )
		{
			DWORD dwResult = 0;
			if ( ( ERROR_SUCCESS != XGetOverlappedResult( &m_xOverlapped, &dwResult, false ) ) ||
				( ERROR_SUCCESS != dwResult ) )
			{
				m_eAsyncState = STATE_DEFAULT;
				return;
			}
			// Image returned is ARGB, need to swap
			int cubImage = 64 * 64 * 4;
			for ( byte *pSwap = (byte*)m_bufRgbaBuffer.Base(), *pSwapEnd = pSwap + cubImage; pSwap < pSwapEnd; pSwap += 4 )
			{
				byte saveA = pSwap[0];
				pSwap[0] = pSwap[1];
				pSwap[1] = pSwap[2];
				pSwap[2] = pSwap[3];
				pSwap[3] = saveA;
			}
			InitFromRGBA( (byte*)m_bufRgbaBuffer.Base(), 64, 64 );
			m_eAsyncState = STATE_COMPLETE;
		}
		break;

	case STATE_COMPLETE:
		break;
	}
}
#endif

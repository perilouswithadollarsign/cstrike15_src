//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "vgui_avatarimage.h"
#include "engineinterface.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#if defined( _PS3 )
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#endif

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif
#include "hud.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DECLARE_BUILD_FACTORY( CAvatarImagePanel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAvatarImage::CAvatarImage( void )
{
	ClearAvatarSteamID();

	m_nX = 0;
	m_nY = 0;
	m_wide = m_tall = 0;
	m_avatarWide = m_avatarTall = 0;
	m_Color = Color( 255, 255, 255, 255 );
	m_bLoadPending = false;
	m_fNextLoadTime = 0.0f;
	m_AvatarSize = eAvatarSmall;

	// [tj] Default to drawing the friend icon for avatars
	m_bDrawFriend = true;

	// [menglish] Default icon for avatar icons if there is no avatar icon for the player
	m_iTextureID = -1;

	// set up friend icon
	m_pFriendIcon = HudIcons().GetIcon( "ico_friend_indicator_avatar" );

	m_pDefaultImage = NULL;

	SetAvatarSize(DEFAULT_AVATAR_SIZE, DEFAULT_AVATAR_SIZE);
}

//-----------------------------------------------------------------------------
// Purpose: reset the image to a default state (will render with the default image)
//-----------------------------------------------------------------------------
void CAvatarImage::ClearAvatarSteamID( void ) 
{ 
	m_bValid = false; 
	m_bFriend = false;
	m_bLoadPending = false;
	m_SteamID.Set( 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
}

//-----------------------------------------------------------------------------
// Purpose: Set the CSteamID for this image; this will cause a deferred load
//-----------------------------------------------------------------------------
bool CAvatarImage::SetAvatarSteamID( CSteamID steamIDUser, EAvatarSize avatarSize /*= eAvatarSmall */ )
{
	ClearAvatarSteamID();

	m_SteamID = steamIDUser;
	m_AvatarSize = avatarSize;
	m_bLoadPending = true;

	LoadAvatarImage();
	UpdateFriendStatus();

	return m_bValid;
}

//-----------------------------------------------------------------------------
// Purpose: load the avatar image if we have a load pending
//-----------------------------------------------------------------------------
void CAvatarImage::LoadAvatarImage()
{
	// attempt to retrieve the avatar image from Steam
	if ( m_bLoadPending && steamapicontext->SteamFriends() && steamapicontext->SteamUtils() && gpGlobals->curtime >= m_fNextLoadTime )
	{
		int iAvatar = 0;
		switch ( m_AvatarSize )
		{
		case eAvatarSmall:
			iAvatar = steamapicontext->SteamFriends()->GetSmallFriendAvatar( m_SteamID );
			break;
		case eAvatarMedium:
			iAvatar = steamapicontext->SteamFriends()->GetMediumFriendAvatar( m_SteamID );
			break;
		case eAvatarLarge:
			iAvatar = steamapicontext->SteamFriends()->GetLargeFriendAvatar( m_SteamID );
			break;
		}

		if ( iAvatar != 0 ) // if its zero, user doesn't have an avatar
		{
			uint32 wide = 0, tall = 0;
			if ( steamapicontext->SteamUtils()->GetImageSize( iAvatar, &wide, &tall ) && wide > 0 && tall > 0 )
			{
				int destBufferSize = wide * tall * 4;
				byte *rgbDest = (byte*)stackalloc( destBufferSize );
				if ( steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, rgbDest, destBufferSize ) )
					InitFromRGBA( rgbDest, wide, tall );
				
				stackfree( rgbDest );
			}
		}

		if ( m_bValid )
		{
			// if we have a valid image, don't attempt to load it again
			m_bLoadPending = false;
		}
		else
		{
			// otherwise schedule another attempt to retrieve the image
			m_fNextLoadTime = gpGlobals->curtime + 1.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Query Steam to set the m_bFriend status flag
//-----------------------------------------------------------------------------
void CAvatarImage::UpdateFriendStatus( void )
{
	if ( !m_SteamID.IsValid() )
		return;

	if ( steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
		m_bFriend = steamapicontext->SteamFriends()->HasFriend( m_SteamID, k_EFriendFlagImmediate );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAvatarImage::InitFromRGBA( const byte *rgba, int width, int height )
{
	// Texture size may be changing, so recreate
	if ( m_iTextureID < 0 )
	{
		vgui::surface()->DestroyTextureID( m_iTextureID );
		m_iTextureID = -1;
	}

	m_iTextureID = vgui::surface()->CreateNewTextureID( true );

	vgui::surface()->DrawSetTextureRGBAEx( m_iTextureID, rgba, width, height, IMAGE_FORMAT_RGBA8888 );

	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAvatarImage::Paint( void )
{
	if ( m_bFriend && m_pFriendIcon && m_bDrawFriend)
	{
		m_pFriendIcon->DrawSelf( m_nX, m_nY, m_wide, m_tall, m_Color );
	}

	int posX = m_nX;
	int posY = m_nY;

	if (m_bDrawFriend)
	{
		posX += FRIEND_ICON_AVATAR_INDENT_X * m_avatarWide / DEFAULT_AVATAR_SIZE;
		posY += FRIEND_ICON_AVATAR_INDENT_Y * m_avatarTall / DEFAULT_AVATAR_SIZE;
	}
	
	if ( m_bLoadPending )
	{
		LoadAvatarImage();
	}

	if ( m_bValid )
	{
		vgui::surface()->DrawSetTexture( m_iTextureID );
		vgui::surface()->DrawSetColor( m_Color );
		vgui::surface()->DrawTexturedRect(posX, posY, posX + m_avatarWide, posY + m_avatarTall);
	}
	else if (m_pDefaultImage)
	{
		// draw default
		m_pDefaultImage->SetSize(m_avatarWide, m_avatarTall);
		m_pDefaultImage->SetPos(posX, posY);
		m_pDefaultImage->SetColor(m_Color);
		m_pDefaultImage->Paint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the avatar size; scale the total image and friend icon to fit
//-----------------------------------------------------------------------------
void CAvatarImage::SetAvatarSize(int wide, int tall)
{
	m_avatarWide = wide;
	m_avatarTall = tall;

	if (m_bDrawFriend)
	{
		// scale the size of the friend background frame icon
		m_wide = FRIEND_ICON_SIZE_X * m_avatarWide / DEFAULT_AVATAR_SIZE;
		m_tall = FRIEND_ICON_SIZE_Y * m_avatarTall / DEFAULT_AVATAR_SIZE;
	}
	else
	{
		m_wide = m_avatarWide;
		m_tall = m_avatarTall;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the total image size; scale the avatar portion to fit
//-----------------------------------------------------------------------------
void CAvatarImage::SetSize( int wide, int tall )
{
	m_wide = wide;
	m_tall = tall;

	if (m_bDrawFriend)
	{
		// scale the size of the avatar portion based on the total image size
		m_avatarWide = DEFAULT_AVATAR_SIZE * m_wide / FRIEND_ICON_SIZE_X;
		m_avatarTall = DEFAULT_AVATAR_SIZE * m_tall / FRIEND_ICON_SIZE_Y ;
	}
	else
	{
		m_avatarWide = m_wide;
		m_avatarTall = m_tall;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAvatarImagePanel::CAvatarImagePanel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_bScaleImage = false;
	m_pImage = new CAvatarImage();
	m_bSizeDirty = true;
}

//-----------------------------------------------------------------------------
//=============================================================================
// HPE_BEGIN:
// [menglish] Added parameter to specify a default avatar
//=============================================================================
void CAvatarImagePanel::SetPlayer( C_BasePlayer *pPlayer, EAvatarSize avatarSize )
{
	if ( pPlayer )
	{
		int iIndex = pPlayer->entindex();
		SetPlayerByIndex(iIndex, avatarSize);
	}
	else
		m_pImage->ClearAvatarSteamID();

}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// HPE_BEGIN:
// [tj] Adding a second function so we don't need a player pointer to get an avatar
void CAvatarImagePanel::SetPlayerByIndex( int entindex, EAvatarSize avatarSize )
{
	m_pImage->ClearAvatarSteamID();

	player_info_t pi;
	if ( engine->GetPlayerInfo(entindex, &pi) )
	{
		if ( pi.friendsID != 0 	&& steamapicontext->SteamUtils() )
		{		
			CSteamID steamIDForPlayer( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
			SetPlayerBySteamID( steamIDForPlayer, avatarSize );
		}
		else
		{
			m_pImage->ClearAvatarSteamID();
		}
	}
}

//-----------------------------------------------------------------------------
// HPE_BEGIN:
//-----------------------------------------------------------------------------
void CAvatarImagePanel::SetPlayerBySteamID(CSteamID steamIDForPlayer, EAvatarSize avatarSize )
{
	m_pImage->ClearAvatarSteamID();

	if (steamIDForPlayer.GetAccountID() != 0 )
		m_pImage->SetAvatarSteamID( steamIDForPlayer, avatarSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImagePanel::PaintBackground( void )
{
	if ( m_bSizeDirty )
		UpdateSize();

	m_pImage->Paint();
}

void CAvatarImagePanel::ClearAvatar()
{
	m_pImage->ClearAvatarSteamID();
}

void CAvatarImagePanel::SetDefaultAvatar( vgui::IImage* pDefaultAvatar )
{
	m_pImage->SetDefaultImage(pDefaultAvatar);
}

void CAvatarImagePanel::SetAvatarSize( int width, int height )
{
	if ( m_bScaleImage )
	{
		// panel is charge of image size - setting avatar size this way not allowed
		Assert(false);
		return;
	}
	else
	{
		m_pImage->SetAvatarSize( width, height );
		m_bSizeDirty = true;
	}
}

void CAvatarImagePanel::OnSizeChanged( int newWide, int newTall )
{
	BaseClass::OnSizeChanged(newWide, newTall);
	m_bSizeDirty = true;
}

void CAvatarImagePanel::SetShouldScaleImage( bool bScaleImage )
{
	m_bScaleImage = bScaleImage;
	m_bSizeDirty = true;
}

void CAvatarImagePanel::SetShouldDrawFriendIcon( bool bDrawFriend )
{
	m_pImage->SetDrawFriend(bDrawFriend);
	m_bSizeDirty = true;
}

void CAvatarImagePanel::UpdateSize()
{
	if ( m_bScaleImage )
	{
		// the panel is in charge of the image size
		m_pImage->SetAvatarSize(GetWide(), GetTall());
	}
	else
	{
		// the image is in charge of the panel size
		SetSize(m_pImage->GetAvatarWide(), m_pImage->GetAvatarTall() );
	}

	m_bSizeDirty = false;
}

void CAvatarImagePanel::ApplySettings( KeyValues *inResourceData )
{
	m_bScaleImage = ( inResourceData->GetInt( "scaleImage", 0 ) != 0 );

	BaseClass::ApplySettings(inResourceData);
}

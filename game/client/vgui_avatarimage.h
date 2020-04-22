//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VGUI_AVATARIMAGE_H
#define VGUI_AVATARIMAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Image.h>
#include <vgui_controls/ImagePanel.h>
#include "steam/steam_api.h"
#include "c_baseplayer.h"

// size of the friend background frame (see texture ico_friend_indicator_avatar)
#define FRIEND_ICON_SIZE_X	(55)	
#define FRIEND_ICON_SIZE_Y	(34)

// offset of avatar within the friend icon
#define FRIEND_ICON_AVATAR_INDENT_X	(22)
#define FRIEND_ICON_AVATAR_INDENT_Y	(1)

// size of the standard avatar icon (unless override by SetAvatarSize)
#define DEFAULT_AVATAR_SIZE		(32)

enum EAvatarSize
{
	eAvatarSmall,
	eAvatarMedium,
	eAvatarLarge
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAvatarImage : public vgui::IImage
{
public:
	CAvatarImage( void );

	// Call this to set the steam ID associated with the avatar
	bool SetAvatarSteamID( CSteamID steamIDUser, EAvatarSize avatarSize = eAvatarSmall );
	void UpdateFriendStatus( void );
	void ClearAvatarSteamID( void );

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint( void );

	// Set the position of the image
	virtual void SetPos(int x, int y)
	{
		m_nX = x;
		m_nY = y;
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall)
	{
		wide = m_wide;
		tall = m_tall;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall)
	{
		GetContentSize( wide, tall );
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall);

	void SetAvatarSize(int wide, int tall);

	// Set the draw color
	virtual void SetColor(Color col)
	{
		m_Color = col;
	}

	bool	IsValid() { return m_bValid; }

	int		GetWide() { return m_wide; }
	int		GetTall() { return m_tall; }
	int		GetAvatarWide() { return m_avatarWide; }
	int		GetAvatarTall() { return m_avatarTall; }

	// [tj] simple setter for drawing friend icon
	void	SetDrawFriend(bool drawFriend) { m_bDrawFriend = drawFriend; }

	// [pmf] specify the default (fallback) image
	void 	SetDefaultImage(vgui::IImage* pImage) { m_pDefaultImage = pImage; }

	virtual bool Evict( void ) { return false; }
	virtual int GetNumFrames( void ) { return 0; }
	virtual void SetFrame( int nFrame ) {}
	virtual vgui::HTexture GetID( void ) { return m_iTextureID; }
	virtual void SetRotation( int iRotation ) {}

protected:
	void InitFromRGBA( const byte *rgba, int width, int height );

private:
	void LoadAvatarImage();

	Color m_Color;
	int m_iTextureID;
	int m_nX, m_nY;
	int m_wide, m_tall;
	int	m_avatarWide, m_avatarTall;
	bool m_bValid;
	bool m_bFriend;
	bool m_bLoadPending;
	float m_fNextLoadTime;	// used to throttle load attempts

	EAvatarSize m_AvatarSize;
	CHudTexture *m_pFriendIcon;
	CSteamID	m_SteamID;

	// [tj] Whether or not we should draw the friend icon
	bool m_bDrawFriend;

	// [pmf] image to use as a fallback when get from steam fails (or not called)
	vgui::IImage* m_pDefaultImage;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAvatarImagePanel : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CAvatarImagePanel, vgui::Panel );

	CAvatarImagePanel( vgui::Panel *parent, const char *name );

	// reset the image to its default value, clearing any info retrieved from Steam
	void ClearAvatar();

	// Set the player that this Avatar should display for
	void SetPlayer( C_BasePlayer *pPlayer, EAvatarSize avatarSize = eAvatarSmall );
	void SetPlayerByIndex( int entityIndex, EAvatarSize avatarSize = eAvatarSmall );
	void SetPlayerBySteamID( CSteamID steamIDForPlayer, EAvatarSize avatarSize );

	// sets whether or not the image should scale to fit the size of the ImagePanel (defaults to false)
	void SetShouldScaleImage( bool bScaleImage );

	// sets whether to automatically draw the friend icon behind the avatar for Steam friends
	void SetShouldDrawFriendIcon( bool bDrawFriend );

	// specify the size of the avatar portion of the image (the actual image may be larger than this
	// when it incorporates the friend icon)
	void SetAvatarSize( int width, int height);

	// specify a fallback image to use
	void SetDefaultAvatar(vgui::IImage* pDefaultAvatar);

	virtual void OnSizeChanged(int newWide, int newTall);

	virtual void PaintBackground( void );
	bool	IsValid() { return ( m_pImage && m_pImage->IsValid() ); }

protected:
	CPanelAnimationVar( Color, m_clrOutline, "color_outline", "Black" );
	virtual void ApplySettings(KeyValues *inResourceData);

	void UpdateSize();

private:
	CAvatarImage *m_pImage;
	bool m_bScaleImage;
	bool m_bSizeDirty;
};

#endif // VGUI_AVATARIMAGE_H

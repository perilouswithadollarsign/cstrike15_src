//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFOOTERPANEL_H__
#define __VFOOTERPANEL_H__

#include "basemodui.h"
#include "vgui_controls/button.h"

class CFooterBitmapButton;

namespace BaseModUI
{

typedef unsigned int FooterButtons_t;

enum FooterFormat_t
{
	FF_NONE,
	FF_ABXYDL_ORDER,
	FF_ABYXDL_ORDER,
};

enum Button_t
{
	FB_NONE				= 0x00,
	FB_ABUTTON			= 0x01,
	FB_BBUTTON			= 0x02,
	FB_XBUTTON			= 0x04,
	FB_YBUTTON			= 0x08,
	FB_DPAD				= 0x10,
	FB_LSHOULDER		= 0x20,
	MAX_FOOTER_BUTTONS	= 6,

	FB_STEAM_SELECT		= 0x40,
	FB_STEAM_NOPROFILE	= 0x80,
};

enum FooterType_t
{
	FOOTER_MENUS = 0,
	FOOTER_GENERICCONFIRMATION, 
	FOOTER_GENERICWAITSCREEN,
	MAX_FOOTER_TYPES
};

struct FooterData_t
{
	FooterData_t()
	{
		m_Buttons = FB_NONE;
		m_Format = FF_NONE;
		m_nNeedsUniqueLine = 0;
		m_nX = 0;
		m_nY = 0;
	}

	CUtlString		m_AButtonText;
	CUtlString		m_BButtonText;
	CUtlString		m_XButtonText;
	CUtlString		m_YButtonText;
	CUtlString		m_DPadButtonText;
	CUtlString		m_LShoulderButtonText;

	FooterButtons_t	m_Buttons;
	FooterFormat_t	m_Format;
	uint32			m_nNeedsUniqueLine;
	int				m_nX;
	int				m_nY;
};

class CBaseModFooterPanel : public CBaseModFrame
{
public:
	DECLARE_CLASS_SIMPLE( CBaseModFooterPanel , CBaseModFrame );

	CBaseModFooterPanel( vgui::Panel *pParent, const char *pPanelName );
	~CBaseModFooterPanel();

	void			SetUsesAlternateTiles( bool bUseAlternate ) { m_bUsesAlternateTiles = bUseAlternate; }

	void			SetFooterType( FooterType_t nFooterType );
	void			SetButtons( FooterButtons_t flag, FooterFormat_t format = FF_ABXYDL_ORDER, FooterType_t type = FOOTER_MENUS );
	void			SetButtonText( Button_t button, const char *pText, bool bNeedsUniqueLine = false, FooterType_t type = FOOTER_MENUS );
	void			SetPosition( int x, int y, FooterType_t type = FOOTER_MENUS );

	FooterType_t	GetFooterType();
	FooterButtons_t GetButtons( FooterType_t type = FOOTER_MENUS );
	FooterFormat_t	GetFormat( FooterType_t type = FOOTER_MENUS );
	void			GetPosition( int &x, int &y, FooterType_t type = FOOTER_MENUS );

	bool			HasContent( void );
	void			SetShowCloud( bool bShow );
	bool			GetHelpTextEnabled() { return false; }

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme * pScheme );
	virtual void	PaintBackground();
	virtual void	OnCommand( const char *pCommand );
#if !defined( _GAMECONSOLE )
	virtual void	OnKeyCodeTyped( vgui::KeyCode code );
	virtual void	OnMousePressed( vgui::MouseCode code );
#endif

private:
	struct DrawButtonParams_t
	{
		DrawButtonParams_t() 
		{ 
			Q_memset( this, 0, sizeof( *this ) ); 
			hTextFont = vgui::INVALID_FONT;
		}
		int				x, y, w, h;
		wchar_t const	*pwszText;
		bool			bRightAlign;
		bool			bVerticalAlign;
		vgui::HFont		hTextFont;
	};
	void			DrawButtonAndText( int &x, int &y, const char *pButton, const char *pText, bool bNeedsUniqueLine, DrawButtonParams_t *pParams = NULL );
	void			DetermineFooterFont();
	int				CalculateButtonWidth( const char *pButton, const char *pText );
	void			FixLayout();
	void			GetButtonOrder( FooterFormat_t format, Button_t buttonOrder[MAX_FOOTER_BUTTONS] );

	FooterType_t	m_nFooterType;
	FooterData_t	m_FooterData[MAX_FOOTER_TYPES];

	bool			m_bUsesAlternateTiles;

	CFooterBitmapButton	*m_pButtons[MAX_FOOTER_BUTTONS];

	vgui::HFont		m_hButtonFont;
	vgui::HFont		m_hButtonTextFont;
	vgui::HFont		m_hButtonTextLargeFont;
	vgui::HFont		m_hButtonTextSmallFont;
	int				m_nTextOffsetX;
	int				m_nTextOffsetY;
	int				m_nButtonGapX;
	int				m_nFullButtonGapX;
	int				m_nButtonGapY;
	int				m_nButtonPaddingX;
	Color			m_TextColor;
	Color			m_TextColorAlt;
	Color			m_InGameTextColor;
	Color			m_InGameTextColorAlt;
	bool			m_bInitialized;

	Vector2D		m_LastDrawnButtonBounds;

#ifdef _PS3
	vgui::IImage	*m_pAvatarImage;
	XUID			m_xuidAvatarImage;
	int				m_iAvatarFrameTexture;

	vgui::HFont		m_hAvatarTextFont;
	int				m_nAvatarSize;
	int				m_nAvatarBorderSize;
	int				m_nAvatarOffsetY;
	int				m_nAvatarFriendsY;
	int				m_nAvatarNameY;
#endif
};

};

#endif
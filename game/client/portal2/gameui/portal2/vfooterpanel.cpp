//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VFooterPanel.h"
#include "vgui/IPanel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/ilocalize.h"
#include "tier1/fmtstr.h"
#include "transitionpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;


ConVar cl_footer_no_auto_shrink( "cl_footer_no_auto_shrink", "0", FCVAR_NONE, "Prevents shrinking the font when it would wrap." );
ConVar cl_footer_no_auto_wrap( "cl_footer_no_auto_wrap", "0", FCVAR_NONE, "Prevents shrinking the font when it would wrap." );


class CFooterBitmapButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFooterBitmapButton, vgui::Button );

public:
	CFooterBitmapButton( vgui::Panel *pParent, const char *pName, const char *pText );
	~CFooterBitmapButton();

	void SetUsesAlternateTiles( bool bUseAlternate ) { m_bUsesAlternateTiles = bUseAlternate; }

protected:
	virtual void PaintBackground( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	Color				m_BorderColor;
	Color				m_BorderArmedColor;
	Color				m_BorderDepressedColor;

	Color				m_AltBorderColor;
	Color				m_AltBorderArmedColor;
	Color				m_AltBorderDepressedColor;

	bool				m_bUsesAlternateTiles;

	int					m_nButtonImageId;	
	int					m_nButtonOverImageId;
	int					m_nButtonClickImageId;	

	int					m_nAltButtonImageId;	
	int					m_nAltButtonOverImageId;
	int					m_nAltButtonClickImageId;	
};

CFooterBitmapButton::CFooterBitmapButton( vgui::Panel *pParent, const char *pName, const char *pText ) :
	BaseClass( pParent, pName, pText ),
	m_bUsesAlternateTiles( false ),
	m_nButtonImageId( -1 ),
	m_nButtonOverImageId( -1 ),
	m_nButtonClickImageId( -1 ),
	m_nAltButtonImageId( -1 ),
	m_nAltButtonOverImageId( -1 ),
	m_nAltButtonClickImageId( -1 )
{
	SetPaintBackgroundEnabled( true );
}

CFooterBitmapButton::~CFooterBitmapButton()
{
}

void CFooterBitmapButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_nButtonImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/btn_bg" );
	m_nButtonOverImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/btn_bg_over" );
	m_nButtonClickImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/btn_bg_click" );

	m_nAltButtonImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/btn_bg_alt" );
	m_nAltButtonOverImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/btn_bg_over_alt" );
	m_nAltButtonClickImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/btn_bg_click_alt" );

	m_BorderColor = GetSchemeColor( "FooterPanel.BorderColor", pScheme );
	m_BorderArmedColor = GetSchemeColor( "FooterPanel.BorderArmedColor", pScheme );
	m_BorderDepressedColor = GetSchemeColor( "FooterPanel.BorderDepressedColor", pScheme );
	
	m_AltBorderColor = GetSchemeColor( "FooterPanel.BorderColorAlt", pScheme );
	m_AltBorderArmedColor = GetSchemeColor( "FooterPanel.BorderArmedColorAlt", pScheme );
	m_AltBorderDepressedColor = GetSchemeColor( "FooterPanel.BorderDepressedColorAlt", pScheme );
}

void CFooterBitmapButton::PaintBackground( void )
{
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// Pick our options based on our tile type
	int nButtonImageId = ( m_bUsesAlternateTiles ) ? m_nAltButtonImageId : m_nButtonImageId;
	int nButtonOverImageId = ( m_bUsesAlternateTiles ) ? m_nAltButtonOverImageId : m_nButtonOverImageId;
	int nButtonClickImageId = ( m_bUsesAlternateTiles ) ? m_nAltButtonClickImageId : m_nButtonClickImageId;

	Color borderColor = ( m_bUsesAlternateTiles ) ? m_AltBorderColor : m_BorderColor;
	Color borderDepressedColor = ( m_bUsesAlternateTiles ) ? m_AltBorderDepressedColor : m_BorderDepressedColor;
	Color borderArmedColor = ( m_bUsesAlternateTiles ) ? m_AltBorderArmedColor : m_BorderArmedColor;

	surface()->DrawSetColor( 255, 255, 255, 255 );
	if ( IsDepressed() )
	{
		surface()->DrawSetTexture( nButtonClickImageId );
	}
	else
	{
		surface()->DrawSetTexture( IsArmed() ? nButtonOverImageId : nButtonImageId );
	}
	surface()->DrawTexturedRect( 0, 0, wide, tall );

	if ( IsDepressed() )
	{
		surface()->DrawSetColor( borderDepressedColor );
	}
	else
	{
		surface()->DrawSetColor( IsArmed() ? borderArmedColor : borderColor );
	}
	surface()->DrawOutlinedRect( 0, 0, wide, tall );
}

CBaseModFooterPanel::CBaseModFooterPanel( vgui::Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName, false, false, false, false ),
	m_bUsesAlternateTiles( false )
{
	vgui::ipanel()->SetTopmostPopup( GetVPanel(), true );

	SetProportional( true );
	SetTitle( "", false );

	SetUpperGarnishEnabled( false );

	m_hButtonFont = vgui::INVALID_FONT;
	m_hButtonTextFont = vgui::INVALID_FONT;
	m_hButtonTextLargeFont = vgui::INVALID_FONT;
	m_hButtonTextSmallFont = vgui::INVALID_FONT;
	m_nTextOffsetX = 0;
	m_nTextOffsetY = 0;
	m_nButtonGapX = 0;
	m_nFullButtonGapX = 0;
	m_nButtonGapY = 0;
	m_nButtonPaddingX = 0;

	m_TextColor = Color( 255, 255, 255, 255 );
	m_TextColorAlt = Color( 255, 255, 255, 255 );

	m_InGameTextColor = Color( 255, 255, 255, 255 );
	m_InGameTextColorAlt = Color( 255, 255, 255, 255 );

	m_nFooterType = FOOTER_MENUS;

	COMPILE_TIME_ASSERT( ARRAYSIZE( m_pButtons ) == MAX_FOOTER_BUTTONS );
	for ( int i = 0; i < MAX_FOOTER_BUTTONS; i++ )
	{
		m_pButtons[i] = new CFooterBitmapButton( this, CFmtStr( "Btn%d", i ), CFmtStr( "Btn%d", i ) );
		m_pButtons[i]->SetVisible( false );
	}

	m_LastDrawnButtonBounds.Init();

	m_bInitialized = false;

	m_bUsesAlternateTiles = false;

#ifdef _PS3
	m_pAvatarImage = NULL;
	m_xuidAvatarImage = 0ull;
	m_iAvatarFrameTexture = -1;
	m_nAvatarSize = 0;
	m_nAvatarBorderSize = 0;
	m_nAvatarNameY = 0;
	m_nAvatarFriendsY = 0;
	m_nAvatarOffsetY = 0;
	m_hAvatarTextFont = vgui::INVALID_FONT;
#endif
}

CBaseModFooterPanel::~CBaseModFooterPanel()
{
#ifdef _PS3
	if ( m_pAvatarImage )
	{
		CUIGameData::Get()->AccessAvatarImage( m_xuidAvatarImage, CUIGameData::kAvatarImageRelease );
		m_pAvatarImage = NULL;
		m_xuidAvatarImage = 0ull;
	}
#endif
}

void CBaseModFooterPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );

	const char *pButtonFont = pScheme->GetResourceString( "FooterPanel.ButtonFont" );
	m_hButtonFont = pScheme->GetFont( pButtonFont, true );

	const char *pTextFont = pScheme->GetResourceString( "FooterPanel.TextFont" );
	m_hButtonTextLargeFont = pScheme->GetFont( pTextFont, true );

	m_hButtonTextSmallFont = pScheme->GetFont( "InstructorKeyBindings", true );

	// make sure the default font is set
	m_hButtonTextFont = m_hButtonTextLargeFont;

	m_TextColor = GetSchemeColor( "FooterPanel.TextColor", pScheme );
	m_TextColorAlt = GetSchemeColor( "FooterPanel.TextColorAlt", pScheme );

	m_InGameTextColor = GetSchemeColor( "FooterPanel.InGameTextColor", pScheme );
	m_InGameTextColorAlt = GetSchemeColor( "FooterPanel.InGameTextColorAlt", pScheme );

	m_nTextOffsetX = atoi( pScheme->GetResourceString( "FooterPanel.TextOffsetX" ) );
	m_nTextOffsetX = vgui::scheme()->GetProportionalScaledValue( m_nTextOffsetX );

	m_nTextOffsetY = atoi( pScheme->GetResourceString( "FooterPanel.TextOffsetY" ) );
	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( m_nTextOffsetY );

	m_nFullButtonGapX = atoi( pScheme->GetResourceString( "FooterPanel.ButtonGapX" ) );
	m_nFullButtonGapX = vgui::scheme()->GetProportionalScaledValue( m_nFullButtonGapX );

	m_nButtonGapX = m_nFullButtonGapX;

	m_nButtonGapY = atoi( pScheme->GetResourceString( "FooterPanel.ButtonGapY" ) );
	m_nButtonGapY = vgui::scheme()->GetProportionalScaledValue( m_nButtonGapY );

	m_nButtonPaddingX = atoi( pScheme->GetResourceString( "FooterPanel.ButtonPaddingX" ) );
	m_nButtonPaddingX = vgui::scheme()->GetProportionalScaledValue( m_nButtonPaddingX );

	if ( !IsGameConsole() )
	{
		for ( int i = 0; i < MAX_FOOTER_BUTTONS; i++ )
		{
			m_pButtons[i]->SetFont( m_hButtonTextFont );
			m_pButtons[i]->SetArmedSound( CBaseModPanel::GetSingleton().GetUISoundName( UISOUND_FOCUS ) );
			m_pButtons[i]->SetCommand( CFmtStr( "Btn%d", i ) );

			if ( m_nTextOffsetX || m_nTextOffsetY )
			{
				m_pButtons[i]->SetTextInset( m_nTextOffsetX, m_nTextOffsetY );
			}
		}
	}

#if defined( _PS3 )
	m_nAvatarSize = atoi( pScheme->GetResourceString( "FooterPanel.AvatarSize" ) );
	m_nAvatarSize = vgui::scheme()->GetProportionalScaledValue( m_nAvatarSize );

	m_nAvatarBorderSize = atoi( pScheme->GetResourceString( "FooterPanel.AvatarBorderSize" ) );
	m_nAvatarBorderSize = vgui::scheme()->GetProportionalScaledValue( m_nAvatarBorderSize );

	m_nAvatarOffsetY = atoi( pScheme->GetResourceString( "FooterPanel.AvatarOffsetY" ) );
	m_nAvatarOffsetY = vgui::scheme()->GetProportionalScaledValue( m_nAvatarOffsetY );

	m_nAvatarNameY = atoi( pScheme->GetResourceString( "FooterPanel.AvatarNameY" ) );
	m_nAvatarNameY = vgui::scheme()->GetProportionalScaledValue( m_nAvatarNameY );

	m_nAvatarFriendsY = atoi( pScheme->GetResourceString( "FooterPanel.AvatarFriendsY" ) );
	m_nAvatarFriendsY = vgui::scheme()->GetProportionalScaledValue( m_nAvatarFriendsY );

	m_iAvatarFrameTexture = CBaseModPanel::GetSingleton().GetImageId( "vgui/steam_avatar_border_ingame" );

	const char *pAvatarTextFont = pScheme->GetResourceString( "FooterPanel.AvatarTextFont" );
	m_hAvatarTextFont = pScheme->GetFont( pAvatarTextFont, true );
#endif

	m_bInitialized = true;
}

void CBaseModFooterPanel::FixLayout()
{
	if ( !m_bInitialized )
		return;

	char uilanguage[64];
	engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
	bool bIsEnglish = ( uilanguage[0] == 0 ) || !V_stricmp( uilanguage, "english" );

	FooterData_t *pFooterData = &m_FooterData[m_nFooterType];

	int x = pFooterData->m_nX;

	int nButtonWide, nButtonTall;

	DetermineFooterFont();

	// Determine if we're an alternate tile type
	CBaseModFrame *pFrame = BASEMODPANEL_SINGLETON.GetWindow( BASEMODPANEL_SINGLETON.GetActiveWindowType() );
	m_bUsesAlternateTiles = ( pFrame && pFrame->UsesAlternateTiles() );	

	if ( !IsGameConsole() )
	{
		Button_t buttonOrder[MAX_FOOTER_BUTTONS] = { FB_NONE };
		GetButtonOrder( pFooterData->m_Format, buttonOrder );

		for ( int i = 0; i < ARRAYSIZE( buttonOrder ); i++ )
		{
			CFooterBitmapButton *pButton = m_pButtons[i];
			if ( !pButton )
				continue;

			Button_t nButton = buttonOrder[i];
			pButton->SetVisible( ( nButton & pFooterData->m_Buttons ) != 0 );
			pButton->SetUsesAlternateTiles( m_bUsesAlternateTiles );

			if ( !( nButton & pFooterData->m_Buttons ) )
				continue;

			if ( nButton & FB_ABUTTON )
			{
				pButton->SetText( pFooterData->m_AButtonText.Get() );
			}
			else if ( nButton & FB_BBUTTON )
			{
				pButton->SetText( pFooterData->m_BButtonText.Get() );
			}
			else if ( nButton & FB_XBUTTON )
			{
				pButton->SetText( pFooterData->m_XButtonText.Get() );
			}
			else if ( nButton & FB_YBUTTON )
			{
				pButton->SetText( pFooterData->m_YButtonText.Get() );
			}
			else if ( nButton & FB_DPAD )
			{
				pButton->SetText( pFooterData->m_DPadButtonText.Get() );
			}
			else if ( nButton & FB_LSHOULDER )
			{
				pButton->SetText( pFooterData->m_LShoulderButtonText.Get() );
			}

			pButton->GetContentSize( nButtonWide, nButtonTall );
			pButton->SetBounds( x, pFooterData->m_nY, nButtonWide + m_nButtonPaddingX, nButtonTall );

			pButton->SetContentAlignment( vgui::Label::a_center );
			if ( bIsEnglish )
			{
				pButton->SetAllCaps( true );
			}

			// Setup our text color (and all of our other items)
			Color textColor = ( m_bUsesAlternateTiles ) ? m_TextColorAlt : m_TextColor;
			pButton->SetDefaultColor( textColor, Color(0,0,0,0) );
			pButton->SetDepressedColor( textColor, Color(0,0,0,0) );
			pButton->SetArmedColor( textColor, Color(0,0,0,0) );

			x += nButtonWide + m_nButtonPaddingX + m_nButtonGapX;
		}
	}
}

void CBaseModFooterPanel::DetermineFooterFont()
{
	if ( !IsGameConsole() )
	{
		return;
	}
	
	if ( !m_bInitialized )
	{
		return;
	}

	if ( m_hButtonFont == vgui::INVALID_FONT || m_hButtonTextLargeFont == vgui::INVALID_FONT || m_hButtonTextSmallFont == vgui::INVALID_FONT )
	{
		// Fonts aren't initialized yet!
		DevWarning( "UI Footer: Button fonts not initialized!\n" );
		return;
	}

	FooterData_t *pFooterData = &m_FooterData[m_nFooterType];

	if ( !HasContent() )
	{
		return;
	}

	if ( cl_footer_no_auto_shrink.GetBool() )
	{
		m_hButtonTextFont = m_hButtonTextLargeFont;
		m_nButtonGapX = m_nFullButtonGapX;
		return;
	}
	
	Button_t buttonOrder[MAX_FOOTER_BUTTONS] = { FB_NONE };
	GetButtonOrder( pFooterData->m_Format, buttonOrder );

	int nTotalFooterWidth = pFooterData->m_nX;

	for ( int i = 0; i < ARRAYSIZE( buttonOrder ); i++ )
	{
		Button_t nButton = buttonOrder[i];
		if ( !( nButton & pFooterData->m_Buttons ) )
			continue;

		if ( nButton & FB_ABUTTON )
		{
			nTotalFooterWidth += CalculateButtonWidth( "#GameUI_Icons_A_3DButton", pFooterData->m_AButtonText.Get() );
		}
		else if ( nButton & FB_BBUTTON )
		{
			nTotalFooterWidth += CalculateButtonWidth( "#GameUI_Icons_B_3DButton", pFooterData->m_BButtonText.Get() );
		}
		else if ( nButton & FB_XBUTTON )
		{
			nTotalFooterWidth += CalculateButtonWidth( "#GameUI_Icons_X_3DButton", pFooterData->m_XButtonText.Get() );
		}
		else if ( nButton & FB_YBUTTON )
		{
			nTotalFooterWidth += CalculateButtonWidth( "#GameUI_Icons_Y_3DButton", pFooterData->m_YButtonText.Get() );
		}
		else if ( nButton & FB_DPAD )
		{
			nTotalFooterWidth += CalculateButtonWidth( "#GameUI_Icons_CENTER_DPAD", pFooterData->m_DPadButtonText.Get() );
		}
	}

	nTotalFooterWidth -= m_nFullButtonGapX; // Subtract off final gap at the end

	int screenWide, screenTall;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	int nTitlesafeInset = screenWide * 0.075f;

	if ( nTotalFooterWidth > (screenWide - nTitlesafeInset) )
	{
		// use the smaller font
		m_hButtonTextFont = m_hButtonTextSmallFont;
		m_nButtonGapX = m_nFullButtonGapX * 0.5f;
	}
	else // use the standard (larger) font
	{
		m_hButtonTextFont = m_hButtonTextLargeFont;
		m_nButtonGapX = m_nFullButtonGapX;
	}
}

int CBaseModFooterPanel::CalculateButtonWidth( const char *pButton, const char *pText )
{
	int buttonLen = 0;
	wchar_t szButtonConverted[128] = {0};
	wchar_t *pButtonString = NULL;
	if ( pButton )
	{
		pButtonString = g_pVGuiLocalize->Find( pButton );
		if ( !pButtonString )
		{
			buttonLen = g_pVGuiLocalize->ConvertANSIToUnicode( pButton, szButtonConverted, sizeof( szButtonConverted ) );
			pButtonString = szButtonConverted;
		}
		else
		{
			buttonLen = V_wcslen( pButtonString );
		}
	}

	int buttonWide = 0, buttonTall = 0;
	if ( pButtonString )
	{
		vgui::surface()->GetTextSize( m_hButtonFont, pButtonString, buttonWide, buttonTall );
	}

	int nTotalWidth = buttonWide + m_nTextOffsetX;

	int textWide = 0;
	int textTall = 0;
	int labelLen = 0;
	wchar_t szLabelConverted[256];
	wchar_t const *pLabelString = NULL;
	if ( pText && pText[0] )
	{
		pLabelString = g_pVGuiLocalize->Find( pText );
		if ( !pLabelString )
		{
			labelLen = g_pVGuiLocalize->ConvertANSIToUnicode( pText, szLabelConverted, sizeof( szLabelConverted ) );
			pLabelString = szLabelConverted;
		}
		labelLen = V_wcslen( pLabelString );

		vgui::surface()->GetTextSize( m_hButtonTextLargeFont, pLabelString, textWide, textTall );

		nTotalWidth += textWide + m_nFullButtonGapX;
	}

	return nTotalWidth;
}

void CBaseModFooterPanel::DrawButtonAndText( int &x, int &y, const char *pButton, const char *pText, bool bNeedsUniqueLine, DrawButtonParams_t *pParams /* = NULL */ )
{
	if ( !m_bInitialized )
	{
		return;
	}

	FooterData_t *pFooterData = &m_FooterData[m_nFooterType];

	vgui::HFont hTextFont = m_hButtonTextFont;

#if defined( _PS3 )
	if ( pParams && pParams->hTextFont != vgui::INVALID_FONT )
	{
		hTextFont = pParams->hTextFont;
	}
#endif

	int buttonLen = 0;
	wchar_t szButtonConverted[128] = {0};
	wchar_t *pButtonString = NULL;
	if ( pButton )
	{
		pButtonString = g_pVGuiLocalize->Find( pButton );
		if ( !pButtonString )
		{
			buttonLen = g_pVGuiLocalize->ConvertANSIToUnicode( pButton, szButtonConverted, sizeof( szButtonConverted ) );
			pButtonString = szButtonConverted;
		}
		else
		{
			buttonLen = V_wcslen( pButtonString );
		}
	}

	int screenWide, screenTall;
	vgui::surface()->GetScreenSize( screenWide, screenTall );

	int buttonWide = 0, buttonTall = 0;
	if ( pButtonString )
	{
		vgui::surface()->GetTextSize( m_hButtonFont, pButtonString, buttonWide, buttonTall );
	}

	int nTotalWidth = buttonWide + m_nTextOffsetX;

	bool bAddGap = false;
	int textWide = 0;
	int textTall = 0;
	int labelLen = 0;
	wchar_t szLabelConverted[256];
	wchar_t const *pLabelString = NULL;
	if ( pText && pText[0] )
	{
		pLabelString = ( pParams && pParams->pwszText ) ? pParams->pwszText : g_pVGuiLocalize->Find( pText );
		if ( !pLabelString )
		{
			labelLen = g_pVGuiLocalize->ConvertANSIToUnicode( pText, szLabelConverted, sizeof( szLabelConverted ) );
			pLabelString = szLabelConverted;
		}
		labelLen = V_wcslen( pLabelString );

		vgui::surface()->GetTextSize( hTextFont, pLabelString, textWide, textTall );

		nTotalWidth += textWide;
		bAddGap = true;
	}

	int nTitlesafeInset = screenWide * 0.075f;

	if ( !cl_footer_no_auto_wrap.GetBool() )
	{
		if ( x != pFooterData->m_nX && ( bNeedsUniqueLine || x + nTotalWidth > ( screenWide - nTitlesafeInset ) ) && ( !pParams || !pParams->bRightAlign ) )
		{
			// not enough room, drop to next line
			x = pFooterData->m_nX;
			y += m_nButtonGapY;
		}
	}

	if ( bAddGap )
	{
		nTotalWidth += m_nButtonGapX;
	}

	// draw button
	if ( pButtonString )
	{
		vgui::surface()->DrawSetTextFont( m_hButtonFont );
		vgui::surface()->DrawSetTextPos( x, y - ( buttonTall / 2 ) );
		vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
		vgui::surface()->DrawPrintText( pButtonString, buttonLen );
	}

	if ( pLabelString )
	{
		int xText = x + buttonWide + m_nTextOffsetX;
		if ( pParams )
		{
			if ( pParams->bRightAlign )
			{
				xText = x - textWide;
				x -= nTotalWidth;
			}

			pParams->x = xText;
			pParams->y = y;
			if ( pParams->bVerticalAlign )
			{
				pParams->y += -( textTall / 2 ) + m_nTextOffsetY;
			}

			pParams->w = textWide;
			pParams->h = textTall;
		}

		vgui::surface()->DrawSetTextFont( hTextFont );

		int yText = y;
		if ( !pParams || pParams->bVerticalAlign )
		{
			yText += -( textTall / 2 ) + m_nTextOffsetY;
		}

		Color textColor = ( m_bUsesAlternateTiles ) ? m_TextColorAlt : m_TextColor;
		Color inGameTextColor = ( m_bUsesAlternateTiles ) ? m_InGameTextColorAlt : m_InGameTextColor;

		vgui::surface()->DrawSetTextPos( xText, yText );
		vgui::surface()->DrawSetTextColor( GameUI().IsInLevel() ? inGameTextColor : textColor );
		vgui::surface()->DrawPrintText( pLabelString, labelLen );
	}

	x += nTotalWidth;
}

bool CBaseModFooterPanel::HasContent( void )
{
	FooterData_t *pFooterData = &m_FooterData[m_nFooterType];

	if ( pFooterData->m_Buttons )
		return true;

	if ( IsPC() )
	{
		vgui::Panel *pCloudLabel = FindChildByName( "UsesCloudLabel" );
		if ( pCloudLabel && pCloudLabel->IsVisible() )
		{
			return true;
		}
	}

	return false;
}

void CBaseModFooterPanel::PaintBackground()
{
	FooterData_t *pFooterData = &m_FooterData[m_nFooterType];

	if ( !HasContent() )
		return;

	int x = pFooterData->m_nX;
	int y = pFooterData->m_nY;

	if ( !x || !y )
	{
		// no buttons can be at edge of screen
		// skip drawing until position established
		return;
	}

	if ( IsPC() )
	{
		WINDOW_TYPE wt = CBaseModPanel::GetSingleton().GetActiveWindowType();
		CBaseModFrame *pWindow = CBaseModPanel::GetSingleton().GetWindow( wt );
		if ( pWindow )
		{
			Button_t buttonOrder[MAX_FOOTER_BUTTONS] = { FB_NONE };
			GetButtonOrder( pFooterData->m_Format, buttonOrder );

			Vector mins, maxs;
			ClearBounds( mins, maxs );
			for ( int i = 0; i < ARRAYSIZE( buttonOrder ); i++ )
			{
				CFooterBitmapButton *pButton = m_pButtons[i];
				if ( !pButton || !pButton->IsVisible() )
					continue;

				Button_t nButton = buttonOrder[i];
				if ( !( nButton & pFooterData->m_Buttons ) )
					continue;

				int nButtonX, nButtonY, nButtonWide, nButtonTall;
				pButton->GetBounds( nButtonX, nButtonY, nButtonWide, nButtonTall );
				AddPointToBounds( Vector( nButtonX, nButtonY, 0 ), mins, maxs );
				AddPointToBounds( Vector( nButtonX + nButtonWide, nButtonY + nButtonTall, 0 ), mins, maxs );
			}

			if ( AreBoundsValid( mins, maxs ) )
			{
				Vector2D buttonBounds;
				buttonBounds.x = maxs.x - mins.x;
				buttonBounds.y = maxs.y - mins.y;
				bool bForce = ( buttonBounds != m_LastDrawnButtonBounds );
				m_LastDrawnButtonBounds = buttonBounds;

				CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->MarkTilesInRect( mins.x, mins.y, buttonBounds.x, buttonBounds.y, wt, bForce );
			}
			else
			{
				m_LastDrawnButtonBounds.Init();
			}
		}
	}

	if ( IsGameConsole() )
	{
		Button_t buttonOrder[MAX_FOOTER_BUTTONS] = { FB_NONE };
		GetButtonOrder( pFooterData->m_Format, buttonOrder );

		for ( int i = 0; i < ARRAYSIZE( buttonOrder ); i++ )
		{
			Button_t nButton = buttonOrder[i];
			if ( !( nButton & pFooterData->m_Buttons ) )
				continue;

			if ( nButton & FB_ABUTTON )
			{
				DrawButtonAndText( x, y, "#GameUI_Icons_A_3DButton", pFooterData->m_AButtonText.Get(), ( pFooterData->m_nNeedsUniqueLine & FB_ABUTTON ) != 0 );
			}
			else if ( nButton & FB_BBUTTON )
			{
				DrawButtonAndText( x, y, "#GameUI_Icons_B_3DButton", pFooterData->m_BButtonText.Get(), ( pFooterData->m_nNeedsUniqueLine & FB_BBUTTON ) != 0 );
			}
			else if ( nButton & FB_XBUTTON )
			{
				DrawButtonAndText( x, y, "#GameUI_Icons_X_3DButton", pFooterData->m_XButtonText.Get(), ( pFooterData->m_nNeedsUniqueLine & FB_XBUTTON ) != 0 );
			}
			else if ( nButton & FB_YBUTTON )
			{
				DrawButtonAndText( x, y, "#GameUI_Icons_Y_3DButton", pFooterData->m_YButtonText.Get(), ( pFooterData->m_nNeedsUniqueLine & FB_YBUTTON ) != 0 );
			}
			else if ( nButton & FB_DPAD )
			{
				DrawButtonAndText( x, y, "#GameUI_Icons_CENTER_DPAD", pFooterData->m_DPadButtonText.Get(), ( pFooterData->m_nNeedsUniqueLine & FB_DPAD ) != 0 );
			}
		}

#if defined(_PS3) && !defined(NO_STEAM) 
		if ( ( pFooterData->m_Buttons & FB_STEAM_SELECT ) || !( pFooterData->m_Buttons & FB_STEAM_NOPROFILE ) )
		{
			if ( pFooterData->m_Buttons & FB_STEAM_SELECT )
				DrawButtonAndText( x, y, NULL, "#L4D360UI_SteamFooterInstr", false );

			// if we are logged on, then we have a Steam user ID
			if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUser()->BLoggedOn() &&
				!( pFooterData->m_Buttons & FB_STEAM_NOPROFILE ) )
			{
				CSteamID sID = steamapicontext->SteamUser()->GetSteamID();
				if ( sID.IsValid() )
				{
					if ( m_pAvatarImage && ( sID.ConvertToUint64() != m_xuidAvatarImage ) )
					{
						CUIGameData::Get()->AccessAvatarImage( m_xuidAvatarImage, CUIGameData::kAvatarImageRelease );
						m_xuidAvatarImage = 0ull;
						m_pAvatarImage = NULL;
					}

					if ( !m_pAvatarImage )
					{
						m_pAvatarImage = CUIGameData::Get()->AccessAvatarImage( sID.ConvertToUint64(), CUIGameData::kAvatarImageRequest );
						if ( m_pAvatarImage )
						{
							m_xuidAvatarImage = sID.ConvertToUint64();
						}
					}

					if ( m_pAvatarImage )
					{
						int screenWide, screenTall;
						vgui::surface()->GetScreenSize( screenWide, screenTall );

						// anchor to top RHS of TCR critical title safe 85% 7.5% each side
						x = 0.925f * (float)screenWide;
						y = m_nAvatarOffsetY;
		
						int x0 = x - m_nAvatarBorderSize;
						int y0 = y;

						// avatar image, center within border
						int nAvatarX = x0 + ( m_nAvatarBorderSize - m_nAvatarSize ) / 2;
						int nAvatarY = y0 + ( m_nAvatarBorderSize - m_nAvatarSize ) / 2;
						m_pAvatarImage->SetPos( nAvatarX, nAvatarY );
						m_pAvatarImage->SetSize( m_nAvatarSize, m_nAvatarSize );
						m_pAvatarImage->Paint();

						if ( m_iAvatarFrameTexture != -1 )
						{
							// avatar border frame
							vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
							vgui::surface()->DrawSetTexture( m_iAvatarFrameTexture );
							vgui::surface()->DrawTexturedRect( x0, y0, x0 + m_nAvatarBorderSize, y0 + m_nAvatarBorderSize );
						}

						// text about friends online
						DrawButtonParams_t dbp;
						dbp.bRightAlign = true;
						dbp.hTextFont = m_hAvatarTextFont;

						wchar_t wszText[128];
						int numOnlineFriendsSteam = CUIGameData::Get()->GetNumOnlineFriends();
						Q_snwprintf( wszText, ARRAYSIZE( wszText ), L"%u ", numOnlineFriendsSteam );
						if ( wchar_t *pSteamFooterFriends = g_pVGuiLocalize->Find( (numOnlineFriendsSteam == 1) ? "#L4D360UI_SteamFooter_Friend1" : "#L4D360UI_SteamFooter_Friends" ) )
						{
							Q_wcsncpy( wszText + Q_wcslen( wszText ), pSteamFooterFriends, ARRAYSIZE( wszText ) );
						}
						dbp.pwszText = wszText;
						int nAvatarFriendsX = x0 - ( m_nAvatarBorderSize - m_nAvatarSize ) / 2;
						int nAvatarFriendsY = m_nAvatarFriendsY;
						DrawButtonAndText( nAvatarFriendsX, nAvatarFriendsY, NULL, " ", false, &dbp );
					
						dbp.pwszText = NULL;
						const char *pName = steamapicontext->SteamFriends()->GetFriendPersonaName( sID );
						int nAvatarNameX = x0 - ( m_nAvatarBorderSize - m_nAvatarSize ) / 2;
						int nAvatarNameY = m_nAvatarNameY;
						DrawButtonAndText( nAvatarNameX, nAvatarNameY, NULL, pName, false, &dbp );
					}
				}
			}
		}
#endif
	}
}

FooterButtons_t CBaseModFooterPanel::GetButtons( FooterType_t footerType )
{
	FooterData_t *pFooterData = &m_FooterData[footerType];

	return pFooterData->m_Buttons;
}

FooterFormat_t CBaseModFooterPanel::GetFormat( FooterType_t footerType )
{
	FooterData_t *pFooterData = &m_FooterData[footerType];

	return pFooterData->m_Format;
}

void CBaseModFooterPanel::GetPosition( int &x, int &y, FooterType_t footerType )
{
	FooterData_t *pFooterData = &m_FooterData[footerType];

	x = pFooterData->m_nX;
	y = pFooterData->m_nY;
}

void CBaseModFooterPanel::SetPosition( int x, int y, FooterType_t footerType )
{
	FooterData_t *pFooterData = &m_FooterData[footerType];

	pFooterData->m_nX = x;
	pFooterData->m_nY = y;

	FixLayout();
}

void CBaseModFooterPanel::SetButtons( FooterButtons_t flags, FooterFormat_t format, FooterType_t footerType )
{
	FooterData_t *pFooterData = &m_FooterData[footerType];

	// this state must stay locked to the button layout as we lack a stack
	// the stupid ui code already constantly slams the button state to maintain it's view of the global footer
	pFooterData->m_Format = format;
	pFooterData->m_Buttons = flags;

	FixLayout();
}

void CBaseModFooterPanel::SetShowCloud( bool bShow )
{
	// currently disabled
	return;

	if ( IsPC() )
	{
		SetControlVisible( "ImageCloud", bShow );
		SetControlVisible( "UsesCloudLabel", bShow );
	}
}

void CBaseModFooterPanel::SetButtonText( Button_t button, const char *pText, bool bNeedsUniqueLine, FooterType_t footerType )
{
	FooterData_t *pFooterData = &m_FooterData[footerType];

	switch ( button )
	{
	default:
	case FB_NONE:
		return;
	case FB_ABUTTON:
		pFooterData->m_AButtonText = pText;
		break;
	case FB_BBUTTON:
		pFooterData->m_BButtonText = pText;
		break;
	case FB_XBUTTON:
		pFooterData->m_XButtonText = pText;
		break;
	case FB_YBUTTON:
		pFooterData->m_YButtonText = pText;
		break;
	case FB_DPAD:
		pFooterData->m_DPadButtonText = pText;
		break;
	case FB_LSHOULDER:
		pFooterData->m_LShoulderButtonText = pText;
		break;
	}

	if ( bNeedsUniqueLine )
	{
		pFooterData->m_nNeedsUniqueLine |= button;
	}
	else
	{
		pFooterData->m_nNeedsUniqueLine &= ~button;
	}

	FixLayout();
}

void CBaseModFooterPanel::OnCommand( const char *pCommand )
{
	if ( IsGameConsole() )
		return;

	CBaseModFrame *pWindow = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	if ( pWindow )
	{
		FooterData_t *pFooterData = &m_FooterData[m_nFooterType];

		Button_t buttonOrder[MAX_FOOTER_BUTTONS] = { FB_NONE };
		GetButtonOrder( pFooterData->m_Format, buttonOrder );

		if ( !V_strnicmp( pCommand, "Btn", 3 ) )
		{
			int nWhich = atoi( pCommand + 3 );
			Button_t nButton = buttonOrder[nWhich];

			ButtonCode_t keyCode = KEY_NONE;
			switch ( nButton )
			{
			case FB_ABUTTON:
				keyCode = KEY_XBUTTON_A;
				break;
			case FB_BBUTTON:
				keyCode = KEY_XBUTTON_B;
				break;
			case FB_XBUTTON:
				keyCode = KEY_XBUTTON_X;
				break;
			case FB_YBUTTON:
				keyCode = KEY_XBUTTON_Y;
				break;
			case FB_LSHOULDER:
				keyCode = KEY_XBUTTON_LEFT_SHOULDER;
				break;
			default:
				return;
			}

			pWindow->OnKeyCodePressed( ButtonCodeToJoystickButtonCode( keyCode, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
	}
}

#ifndef _GAMECONSOLE
void CBaseModFooterPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	// keypresses in the footer really belong to the active window
	CBaseModFrame *pWindow = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	if ( pWindow )
	{
		pWindow->OnKeyCodeTyped( code );
	}
}
#endif

void CBaseModFooterPanel::SetFooterType( FooterType_t nFooterType )
{
	if ( nFooterType == m_nFooterType )
	{
		// no change
		return;
	}

	m_nFooterType = nFooterType;
	FixLayout();
}

FooterType_t CBaseModFooterPanel::GetFooterType()
{
	return m_nFooterType;
}

void CBaseModFooterPanel::GetButtonOrder( FooterFormat_t format, Button_t buttonOrder[MAX_FOOTER_BUTTONS] )
{
	// format specifies order
	switch ( format )
	{
	case FF_ABYXDL_ORDER:
		buttonOrder[0] = FB_ABUTTON;
		buttonOrder[1] = FB_BBUTTON;
		buttonOrder[2] = FB_YBUTTON;
		buttonOrder[3] = FB_XBUTTON;
		buttonOrder[4] = FB_DPAD;
		buttonOrder[5] = FB_LSHOULDER;
		break;

	default:
		// FF_ABXYDL_ORDER
		buttonOrder[0] = FB_ABUTTON;
		buttonOrder[1] = FB_BBUTTON;
		buttonOrder[2] = FB_XBUTTON;
		buttonOrder[3] = FB_YBUTTON;
		buttonOrder[4] = FB_DPAD;
		buttonOrder[5] = FB_LSHOULDER;
		break;
	}
}

#if !defined( _GAMECONSOLE )
void CBaseModFooterPanel::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	CBaseModFrame *pWindow = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	if ( pWindow )
	{
		pWindow->RestoreFocusToActiveControl();
	}
}
#endif

//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "tier1/keyvalues.h"
#include "vgui_basepanel.h"
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "../common/xbox/xboxstubs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Watermark panel visible for pre-release builds
//-----------------------------------------------------------------------------
class CWatermarkPanel : public CBasePanel
{
	typedef CBasePanel BaseClass;

public:
	CWatermarkPanel( vgui::Panel *parent );
	virtual			~CWatermarkPanel( void );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void	Paint();
	virtual bool	ShouldDraw( void ) { return true; };

private:
	void ComputeSize( void );

	vgui::HFont		m_hFont;
	bool			m_bLastDraw;

	int				m_nLinesNeeded;
};

#define WATERMARK_PANEL_WIDTH 400

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CWatermarkPanel::CWatermarkPanel( vgui::Panel *parent ) : BaseClass( NULL, "CWatermarkPanel" )
{
	SetParent( parent );
	SetVisible( true );
	SetCursor( 0 );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );
					    
	m_hFont = 0;
	m_nLinesNeeded = 5;		  

	ComputeSize();

	vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
	m_bLastDraw = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWatermarkPanel::~CWatermarkPanel( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Computes panel's desired size and position
//-----------------------------------------------------------------------------
void CWatermarkPanel::ComputeSize( void )
{
	int wide, tall;
	vgui::ipanel()->GetSize(GetVParent(), wide, tall );

	int x = 0;;
	int y = 0;
	if ( IsX360() )
	{
		x += XBOX_MINBORDERSAFE * wide;
		y += XBOX_MINBORDERSAFE * tall;
	}
	SetPos( x, y );
	SetSize( WATERMARK_PANEL_WIDTH, ( m_nLinesNeeded + 2 ) * vgui::surface()->GetFontTall( m_hFont ) + 4 );
}

void CWatermarkPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "DefaultFixedOutline" );
	Assert( m_hFont );

	ComputeSize();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CWatermarkPanel::Paint() 
{
	wchar_t unicode[ 200 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( "PRE-RELEASE BUILD - DO NOT REDISTRIBUTE", unicode, sizeof( unicode ) );

	DrawColoredText( m_hFont, 100, 42, 0, 255,	0, 255, unicode );
}


static CWatermarkPanel *watermarkPanel = NULL;

void CreateWatermarkPanel( vgui::Panel *parent )
{
	watermarkPanel = new CWatermarkPanel( parent );
	if (watermarkPanel)
	{
		watermarkPanel->SetVisible(true);
	}
}

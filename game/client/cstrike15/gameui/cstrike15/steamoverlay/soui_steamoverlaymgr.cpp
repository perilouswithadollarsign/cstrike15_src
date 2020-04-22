//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "isteamoverlaymgr.h"
#include <vgui_controls/Panel.h>
#include "view.h"
#include <vgui/IVGui.h>
#include "vguimatsurface/imatsystemsurface.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IPanel.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "filesystem.h"
#include "../common/xbox/xboxstubs.h"

#include "steam/steam_api.h"
#include "cdll_int.h"
#include "eiface.h"
#include "matchmaking/imatchframework.h"
#include "inputsystem/iinputsystem.h"
#include "steam/steam_platform_ps3/isteamps3overlayrenderer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ISteamPS3OverlayRenderHost *g_SteamOverlayPanel_RenderHostImplementation;

vgui::IScheme *g_SteamOverlayPanel_Scheme;
IMatSystemSurface *g_SteamOverlayPanel_Surface;


#ifndef NO_STEAM
static bool g_bRealEnhancedOverlayInputModeSetting = false;
static bool BCellPadDataHook( CellPadData &data )
{
	if ( !SteamPS3OverlayRender() )
		return false;

	return SteamPS3OverlayRender()->BHandleCellPadData( data )
		|| g_bRealEnhancedOverlayInputModeSetting;
}

static bool BCellPadNoDataHook()
{
	if ( !SteamPS3OverlayRender() )
		return false;

	return SteamPS3OverlayRender()->BResetInputState();
}
#endif // NO_STEAM

class CSteamOverlayPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CSteamOverlayPanel, vgui::Panel );

public:
	explicit		CSteamOverlayPanel( vgui::VPANEL parent );
	virtual			~CSteamOverlayPanel( void );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void	Paint() {}
	virtual void	PaintBackground();

protected:
	bool m_bInitializedRenderInterface;
};

CSteamOverlayPanel::CSteamOverlayPanel( vgui::VPANEL parent ) : BaseClass( NULL, "SteamOverlayPanel" )
{
	SetParent( parent );
	SetVisible( true );
	SetCursor( null );

	int w, h;
	materials->GetBackBufferDimensions( w, h );
	
	SetPos( 0, 0 );
	SetSize( w, h );
	DevMsg( "CSteamOverlayPanel created (%d x %d)\n", w, h );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( true );

	m_bInitializedRenderInterface = false;
}

CSteamOverlayPanel::~CSteamOverlayPanel()
{
	m_bInitializedRenderInterface = false;
}

void CSteamOverlayPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	g_SteamOverlayPanel_Scheme = pScheme;
	BaseClass::ApplySchemeSettings(pScheme);
}

static bool g_bGameBootReadyForSteamOverlay = true;
static bool g_bEnhancedOverlayInputMode = false;
class CSteamOverlayMgr : public ISteamOverlayManager
{
public:
	CSteamOverlayPanel *m_pPanel;
public:
	CSteamOverlayMgr( void )
	{
		m_pPanel = NULL;
	}
	void Create( vgui::VPANEL parent )
	{
		m_pPanel = new CSteamOverlayPanel( parent );
	}

	void GameBootReady()
	{
		g_bGameBootReadyForSteamOverlay = true;
	}

	void SetEnhancedOverlayInput( bool bEnable )
	{
		g_bEnhancedOverlayInputMode = bEnable;
	}

	void Destroy( void )
	{
		if ( m_pPanel )
		{
			m_pPanel->SetParent( (vgui::Panel *)NULL );
			delete m_pPanel;
			m_pPanel = NULL;
		}
	}
};
static CSteamOverlayMgr g_SteamOverlayMgr;
ISteamOverlayManager *g_pISteamOverlayMgr = &g_SteamOverlayMgr;

void CSteamOverlayPanel::PaintBackground()
{
	g_pMatSystemSurface->SetClipRect( 0, 0, 10000, 10000 );
	g_SteamOverlayPanel_Surface = g_pMatSystemSurface;

	if ( !m_bInitializedRenderInterface && g_bGameBootReadyForSteamOverlay )
	{
		m_bInitializedRenderInterface = true;

		int wide, tall;
		materials->GetBackBufferDimensions( wide, tall );

#ifndef NO_STEAM
		// Setup overlay render interface for PS3 Steam overlay
#if defined(NO_STEAM_PS3_OVERLAY) 
		SteamPS3OverlayRender()->BHostInitialize( wide, tall, 60, NULL, materials->PS3GetFontLibPtr() );
#else
		SteamPS3OverlayRender()->BHostInitialize( wide, tall, 60, g_SteamOverlayPanel_RenderHostImplementation, materials->PS3GetFontLibPtr() );
#endif
		g_pInputSystem->SetPS3CellPadDataHook( &BCellPadDataHook );
		g_pInputSystem->SetPS3CellPadNoDataHook( &BCellPadNoDataHook );
#endif
	}

	if ( m_bInitializedRenderInterface )
	{
#ifndef NO_STEAM
		if ( g_bRealEnhancedOverlayInputModeSetting != g_bEnhancedOverlayInputMode )
		{
			g_pInputSystem->ResetInputState();
			g_bRealEnhancedOverlayInputModeSetting = g_bEnhancedOverlayInputMode;
		}
		if ( g_bRealEnhancedOverlayInputModeSetting )
		{
			g_pInputSystem->PollInputState();
		}
		SteamPS3OverlayRender()->Render();
#endif
	}
}

class CSteamOverlayRenderHost : public ISteamPS3OverlayRenderHost
{

private:

	CUtlMap< int32, int > m_mapSteamToVGUITextureIDs;

	static void ConvertColorFromSRGBToGamma( Color &color )
	{
		color.SetColor( 
			clamp( static_cast< int >( .5f + 255.0f * LinearToGammaFullRange( SrgbGammaToLinear( color.r() * 1.0f/255.0f ) ) ), 0, 255 ), 
			clamp( static_cast< int >( .5f + 255.0f * LinearToGammaFullRange( SrgbGammaToLinear( color.g() * 1.0f/255.0f ) ) ), 0, 255 ),
			clamp( static_cast< int >( .5f + 255.0f * LinearToGammaFullRange( SrgbGammaToLinear( color.b() * 1.0f/255.0f ) ) ), 0, 255 ),
			color.a() );
	}

public:

	CSteamOverlayRenderHost()
	{

		m_mapSteamToVGUITextureIDs.SetLessFunc( DefLessFunc( int32 ) );
	}
	
	virtual void DrawTexturedRect( int x0, int y0, int x1, int y1, float u0, float v0, float u1, float v1, int32 iTextureID, DWORD colorStart, DWORD colorEnd, EOverlayGradientDirection eDirection ) 
	{
		unsigned short iMap = m_mapSteamToVGUITextureIDs.Find( iTextureID );
		if ( iMap != m_mapSteamToVGUITextureIDs.InvalidIndex() )
		{
			Color colStart( STEAM_COLOR_RED( colorStart ), STEAM_COLOR_GREEN( colorStart ), STEAM_COLOR_BLUE( colorStart ), STEAM_COLOR_ALPHA( colorStart ) );
			Color colEnd( STEAM_COLOR_RED( colorEnd ), STEAM_COLOR_GREEN( colorEnd ), STEAM_COLOR_BLUE( colorEnd ), STEAM_COLOR_ALPHA( colorEnd ) );

			// The input colors are in sRGB space, but IMatSystemSurface's draw helpers are expecting the colors in gamma space (because they ultimately 
			// use the vertexlit_and_unlit_generic vertex shader (NOT unlit_generic, which is actually no longer used), which converts the vertex colors from gamma to linear).
			ConvertColorFromSRGBToGamma( colStart );
			ConvertColorFromSRGBToGamma( colEnd );

			g_SteamOverlayPanel_Surface->DrawSetTexture( m_mapSteamToVGUITextureIDs[iMap] );
			g_SteamOverlayPanel_Surface->DrawTexturedSubRectGradient( x0, y0, x1, y1, u0, v0, u1, v1, colStart, colEnd, eDirection == k_EOverlayGradientHorizontal ? true : false );
		}
	}

	virtual void LoadOrUpdateTexture( int32 iTextureID, bool bIsFullTexture, int x0, int y0, uint32 uWidth, uint32 uHeight, int32 iBytes, char *pData ) 
	{
		if ( !bIsFullTexture )
		{
			unsigned short iMap = m_mapSteamToVGUITextureIDs.Find( iTextureID );
			if ( iMap != m_mapSteamToVGUITextureIDs.InvalidIndex() )
			{
				g_SteamOverlayPanel_Surface->DrawSetSubTextureRGBA( m_mapSteamToVGUITextureIDs[iMap], x0, y0, (unsigned char*)pData, uWidth, uHeight );
			}
		}
		else
		{
			int iVGUITexture = g_SteamOverlayPanel_Surface->CreateNewTextureID( true );
			g_SteamOverlayPanel_Surface->DrawSetTextureRGBALinear( iVGUITexture, (unsigned char *)pData, uWidth, uHeight );
			m_mapSteamToVGUITextureIDs.InsertOrReplace( iTextureID, iVGUITexture );
		}
	}


	virtual void DeleteTexture( int32 iTextureID ) 
	{
		unsigned short iMap = m_mapSteamToVGUITextureIDs.Find( iTextureID );
		if ( iMap != m_mapSteamToVGUITextureIDs.InvalidIndex() )
		{
			g_SteamOverlayPanel_Surface->DestroyTextureID( m_mapSteamToVGUITextureIDs[iMap] );
			m_mapSteamToVGUITextureIDs.RemoveAt( iMap );
		}
	}

	virtual void DeleteAllTextures()
	{
		FOR_EACH_MAP( m_mapSteamToVGUITextureIDs, iMap )
		{
			g_SteamOverlayPanel_Surface->DestroyTextureID( m_mapSteamToVGUITextureIDs[iMap] );
		}
		m_mapSteamToVGUITextureIDs.Purge();
	}
};
static CSteamOverlayRenderHost g_SteamOverlayPanel_RenderHostInstance;
ISteamPS3OverlayRenderHost *g_SteamOverlayPanel_RenderHostImplementation = &g_SteamOverlayPanel_RenderHostInstance;

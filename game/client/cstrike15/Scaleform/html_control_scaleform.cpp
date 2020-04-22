//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "html_control_scaleform.h"

#include "vgui/IInput.h"

#include "input/mousecursors.h"
#include "../vgui2/src/vgui_key_translation.h"
#include "materialsystem/materialsystem_config.h"

#include "OfflineMode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define SCALE_MSGS

// This isn't preferable, would likely work better to have only one system receiving the callbacks and dispatching them properly
#define CHECK_BROWSER_HANDLE() do { if ( pCmd->unBrowserHandle != m_unBrowserHandle ) return; } while ( false )

uint64 CHtmlControlScaleform::m_nextProceduralTextureId = 1;

const int NO_HTML_ZOOM = 99.0f;

ConVar csgo_html_zoom( "csgo_html_zoom", "1", FCVAR_NONE, "Scaling method for HTML displays (0=Bilinear Texture Scaling, 1=CEF Pixel Accurate Zoom)" );
ConVar csgo_html_zoom_override( "csgo_html_zoom_override", "99.0" );

extern bool gScaleformBrowserActive;


CHtmlControlScaleform::CHtmlControlScaleform()
:
m_NeedsPaint( this, &CHtmlControlScaleform::BrowserNeedsPaint ),
m_StartRequest( this, &CHtmlControlScaleform::BrowserStartRequest ),
m_URLChanged( this, &CHtmlControlScaleform::BrowserURLChanged ),
m_FinishedRequest( this, &CHtmlControlScaleform::BrowserFinishedRequest ),
m_NewWindow( this, &CHtmlControlScaleform::BrowserPopupHTMLWindow ),
m_HorizScroll( this, &CHtmlControlScaleform::BrowserHorizontalScrollBarSizeResponse ),
m_VertScroll( this, &CHtmlControlScaleform::BrowserVerticalScrollBarSizeResponse ),
m_JSAlert( this, &CHtmlControlScaleform::BrowserJSAlert ),
m_JSConfirm( this, &CHtmlControlScaleform::BrowserJSConfirm ),
m_CanGoBackForward( this, &CHtmlControlScaleform::BrowserCanGoBackandForward ),
m_SetCursor( this, &CHtmlControlScaleform::BrowserSetCursor ),
m_StatusText( this, &CHtmlControlScaleform::BrowserStatusText ),
m_ShowToolTip( this, &CHtmlControlScaleform::BrowserShowToolTip ),
m_UpdateToolTip( this, &CHtmlControlScaleform::BrowserUpdateToolTip ),
m_HideToolTip( this, &CHtmlControlScaleform::BrowserHideToolTip ),
m_SearchResults( this, &CHtmlControlScaleform::BrowserSearchResults ),
m_OpenLinkInNewTab( this, &CHtmlControlScaleform::BrowserOpenNewTab ),
m_FileLoadDialog( this, &CHtmlControlScaleform::BrowserFileLoadDialog ),
m_Close( this, &CHtmlControlScaleform::BrowserClose ),
m_LinkAtPosition( this, &CHtmlControlScaleform::BrowserLinkAtPositionResponse )
{
	m_pParent = NULL;
	m_proceduralTextureId = 0;
	m_bCachedResize = false;
	m_bCachedZoom = false;
	m_flUIScale = 1.f;
	m_iMouseX = 0;
	m_iMouseY = 0;
	m_iVertScrollStep = 75;
	m_bLoadingPage = false;
	m_bStopped = false;
	m_bFullRepaint = false;
	m_bNewWindowsOnly = false;
	m_bCanGoBack = false;
	m_bCanGoForward = false;
	m_flZoom = 1.f;
	m_flDesiredZoom = NO_HTML_ZOOM;
	m_flLastRequestedZoom = 1.f;

	m_unBrowserHandle = INVALID_HTMLBROWSER;

	gScaleformBrowserActive = true;
}

CHtmlControlScaleform::~CHtmlControlScaleform()
{
	g_pScaleformUI->ChromeHTMLImageRelease( m_proceduralTextureId );

	if ( m_SteamAPIContext.SteamHTMLSurface() )
	{
		m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser( m_unBrowserHandle );
	}

	gScaleformBrowserActive = false;
}

void CHtmlControlScaleform::Init( IHtmlParentScaleform* pParent )
{
#ifdef SCALE_MSGS
	Msg( "*** CHtmlControlScaleform::Init() ***\n" );
#endif

	m_pParent = pParent;

	// Initialize the HTML surface and create the browser instance
	m_SteamAPIContext.Init();
	if ( m_SteamAPIContext.SteamHTMLSurface() )
	{
		m_SteamAPIContext.SteamHTMLSurface()->Init();

		int iStageWidth, iStageHeight;
		engine->GetScreenSize( iStageWidth, iStageHeight );

		SteamAPICall_t hSteamAPICall = m_SteamAPIContext.SteamHTMLSurface()->CreateBrowser( "CSGO Client", "::-webkit-scrollbar { background-color: transparent; }" );
		m_SteamCallResultBrowserReady.Set( hSteamAPICall, this, &CHtmlControlScaleform::OnBrowserReady );
	}
	else
	{
		Warning( "Unable to access SteamHTMLSurface" );
	}

	m_proceduralTextureId = m_nextProceduralTextureId++;

	// Turn off bilinear filtering if csgo_html_zoom 1
	m_strProceduralTextureName.Format( "img%s://chrome_%llu", csgo_html_zoom.GetBool() ? "ps" : "", m_proceduralTextureId );
	g_pScaleformUI->ChromeHTMLImageAddRef( m_proceduralTextureId );

	if ( m_pParent )
	{
		m_pParent->InitChromeHTMLRenderTarget( m_strProceduralTextureName );
	}
}

void CHtmlControlScaleform::Update( void )
{
	if ( csgo_html_zoom_override.GetFloat() != NO_HTML_ZOOM )
	{
		m_flDesiredZoom = csgo_html_zoom_override.GetFloat();
	}

	if ( m_flDesiredZoom != NO_HTML_ZOOM && m_flDesiredZoom != m_flLastRequestedZoom )
	{
		SetZoomLevel( m_flDesiredZoom );
		m_flLastRequestedZoom = m_flDesiredZoom;
	}
}

void CHtmlControlScaleform::OnHTMLScrollBarMoved( int iPosition, bool bVert )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	if ( bVert )
	{
		if ( iPosition != m_scrollVertical.m_nScroll )
		{
			m_SteamAPIContext.SteamHTMLSurface()->SetVerticalScroll( m_unBrowserHandle, iPosition );
		}
	}
	else
	{
		if ( iPosition != m_scrollHorizontal.m_nScroll )
		{
			m_SteamAPIContext.SteamHTMLSurface()->SetHorizontalScroll( m_unBrowserHandle, iPosition );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: helper to convert UI mouse codes to CEF ones
//-----------------------------------------------------------------------------
ISteamHTMLSurface::EHTMLMouseButton ConvertMouseCodeToCEFCode( vgui::MouseCode code )
{
	switch ( code )
	{
	default:
	case MOUSE_LEFT:
		return ISteamHTMLSurface::eHTMLMouseButton_Left;

	case MOUSE_RIGHT:
		return ISteamHTMLSurface::eHTMLMouseButton_Right;

	case MOUSE_MIDDLE:
		return ISteamHTMLSurface::eHTMLMouseButton_Middle;
	}
}

//-----------------------------------------------------------------------------
// Purpose: a mouse click on the browser window
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnMouseDown( ButtonCode_t button, int x, int y )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->MouseDown( m_unBrowserHandle, ConvertMouseCodeToCEFCode( button ) );
}

//-----------------------------------------------------------------------------
// Purpose: a mouse click on the browser window
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnMouseUp( ButtonCode_t button, int x, int y )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->MouseUp( m_unBrowserHandle, ConvertMouseCodeToCEFCode( button ) );
}

//-----------------------------------------------------------------------------
// Purpose: keeps track of where the cursor is
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnMouseMoved( int x, int y )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	// Only do this when we are over the current panel
	m_iMouseX = x * m_flUIScale;
	m_iMouseY = y * m_flUIScale;

	m_SteamAPIContext.SteamHTMLSurface()->MouseMove( m_unBrowserHandle, m_iMouseX, m_iMouseY );

	/*
	else if ( !m_sDragURL.IsEmpty() )
	{
		if ( input()->GetMouseOver() == NULL )
		{
			// we're not over any vgui window, switch to the OS implementation of drag/drop
			// BR FIXME
			//			surface()->StartDragDropText( m_sDragURL );
			m_sDragURL = NULL;
		}
	}

	if ( !m_sDragURL.IsEmpty() && !input()->GetCursorOveride() )
	{
		// if we've dragged far enough (in global coordinates), set to use the drag cursor
		int gx, gy;
		input()->GetCursorPos( gx, gy );
		if ( abs(m_iDragStartX-gx) + abs(m_iDragStartY-gy) > 3 )
		{
			//			input()->SetCursorOveride( dc_alias );
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: scrolls the vertical scroll bar on a web page
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnMouseWheeled( int delta )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->MouseWheel( m_unBrowserHandle, delta * m_iVertScrollStep );
}


//-----------------------------------------------------------------------------
// Purpose: return the bitmask of any modifier keys that are currently down
//-----------------------------------------------------------------------------
int TranslateKeyModifiers()
{
	bool bControl = false;
	bool bAlt = false;
	bool bShift = false;

	if ( vgui::input()->IsKeyDown ( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL ) )
		bControl = true;

	if ( vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT ) )
		bAlt = true;

	if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
		bShift = true;

#ifdef OSX
	// for now pipe through the cmd-key to be like the control key so we get copy/paste
	if ( vgui::input()->IsKeyDown( KEY_LWIN ) || vgui::input()->IsKeyDown( KEY_RWIN ) )
		bControl = true;
#endif

	int nModifierCodes = 0;
	if ( bControl )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_CtrlDown;
	if ( bAlt )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_AltDown;
	if ( bShift )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_ShiftDown;

	return nModifierCodes;
}


//-----------------------------------------------------------------------------
// Purpose: Key down detection.
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnKeyDown( ButtonCode_t code )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->KeyDown( m_unBrowserHandle, KeyCode_VGUIToVirtualKey( code ), (ISteamHTMLSurface::EHTMLKeyModifiers)TranslateKeyModifiers() );
}

//-----------------------------------------------------------------------------
// Purpose: Key up detection.
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnKeyUp( ButtonCode_t code )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->KeyUp( m_unBrowserHandle, KeyCode_VGUIToVirtualKey( code ), (ISteamHTMLSurface::EHTMLKeyModifiers)TranslateKeyModifiers() );
}

//-----------------------------------------------------------------------------
// Purpose: passes key presses to the browser
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnKeyTyped( wchar_t unichar )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->KeyChar( m_unBrowserHandle, unichar, (ISteamHTMLSurface::EHTMLKeyModifiers)TranslateKeyModifiers() );
}

//-----------------------------------------------------------------------------
// Purpose: Zoom the current page in or out.
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::SetZoomLevel( float flZoom )
{
	// Make sure the browser is ready before sending the "Page Scale" message
	// Cache zoom factor otherwise, it will then get set in the OnBrowserReady callback
	m_flCachedZoom = flZoom;

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
	{
#ifdef SCALE_MSGS
		Msg( "SetZoomLevel, cache ( %f )\n", flZoom );
#endif
		m_bCachedZoom = true;
		return;
	}

	m_bCachedZoom = false;

#ifdef SCALE_MSGS
	Msg( "SteamHTMLSurface()->SetPageScaleFactor( %f )\n", flZoom );
#endif
	m_SteamAPIContext.SteamHTMLSurface()->SetPageScaleFactor( m_unBrowserHandle, flZoom, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Execute Javascript in current page
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::ExecuteJavascript( const char *pchScript )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
	{
		Assert( 0 );
	}

#ifdef HTML_CONTROL_JAVASCRIPT_DEBUG
	Msg( "SteamHTMLSurface()->ExecuteJavascript( %s )\n", pchScript );
#endif
	m_SteamAPIContext.SteamHTMLSurface()->ExecuteJavascript( m_unBrowserHandle, pchScript );
}

void CHtmlControlScaleform::CallbackJavascriptDialogResponse( bool bResult )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
	{
		Assert( 0 );
	}

#ifdef HTML_CONTROL_JAVASCRIPT_DEBUG
	Msg( "SteamHTMLSurface()->CallbackJavascriptDialogResponse( %s )\n", bResult ? "ACCEPT" : "Dismiss" );
#endif
	m_SteamAPIContext.SteamHTMLSurface()->JSDialogResponse( m_unBrowserHandle, bResult );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the base size for the browser window.
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::SetBrowserBaseSize( int iWidth, int iHeight, int iStageWidth, int iStageHeight )
{
#ifdef SCALE_MSGS
	Msg( "SetBrowserBaseSize()\n" );
#endif

	// Reloading the image in flash so make sure we are creating a new texture !
	g_pScaleformUI->ChromeHTMLImageRelease( m_proceduralTextureId );
	g_pScaleformUI->ChromeHTMLImageAddRef( m_proceduralTextureId );
	
	// default
	m_flDesiredZoom = 1.0f;

	// iWidth and iHeight are the size of the widget that will contain the browser. This size is unscaled!
	// iStageWidth and iStageHeight are the size of our screen resolution.
	// It's possible for the widget's size to be larger than our resolution (it will be scaled).
	
	if ( csgo_html_zoom.GetBool() )
	{
		// scale up the browser size to match actual screen pixels, giving a sharper render
		// Apply chrome zoom to make it fit the same size proportionally
		float flScale = ( iStageHeight / 720.0f );

		iWidth = (float)iWidth * flScale;
		iHeight = (float)iHeight * flScale;

		// these 'hacked' zoom values required updating since the removal of libcef and change in range of zoom values for isteamhtmlsurface (see .h).
		// they were determined by allowing the blog to sit at the desired vertical resolution (iStageheight) without a horizontal scrollbar.
		// dota abandoned this notion after the removal of libcef, and doesn't scale the browser page with resolution change.
		// scaling above 1.0 also problematic (hence the clamp) since no matter where we scale around, the page ends up with a horiz scroll bar.
		if ( iStageHeight <= 480 )			{ m_flDesiredZoom = 0.412f; }
		else if ( iStageHeight <= 576 )		{ m_flDesiredZoom = 0.496f; }
		else if ( iStageHeight <= 600 )		{ m_flDesiredZoom = 0.5195f; }
		else if ( iStageHeight <= 720 )		{ m_flDesiredZoom = 0.625f; }
		else if ( iStageHeight <= 768 )		{ m_flDesiredZoom = 0.66f; }
		else if ( iStageHeight <= 800 )		{ m_flDesiredZoom = 0.695f; }
		else if ( iStageHeight <= 864 )		{ m_flDesiredZoom = 0.75f; }
		else if ( iStageHeight <= 900 )		{ m_flDesiredZoom = 0.7815f; }
		else if ( iStageHeight <= 960 )		{ m_flDesiredZoom = 0.835f; }
		else if ( iStageHeight <= 1024 )	{ m_flDesiredZoom = 0.89f; }
		else if ( iStageHeight <= 1050 )	{ m_flDesiredZoom = 0.91f; }
		else if ( iStageHeight <= 1080 )	{ m_flDesiredZoom = 0.94f; }
		else if ( iStageHeight <= 1200 )	{ m_flDesiredZoom = 1.0f; }
		else if ( iStageHeight <= 1440 )	{ m_flDesiredZoom = 1.0f; }
		else if ( iStageHeight <= 1600 )	{ m_flDesiredZoom = 1.0f; }
		else								{ m_flDesiredZoom = 1.0f; }

		m_flUIScale = flScale;
		BrowserResize( iWidth, iHeight, iWidth, iHeight );

		// force scale to get cached now, attempting to catch case where the page is appearing scaled with a garbage float 
		SetZoomLevel( m_flDesiredZoom );
		m_flLastRequestedZoom = m_flDesiredZoom;
	}
	else
	{
		m_flUIScale = 1.5f;
		BrowserResize( iWidth, iHeight, iWidth*m_flUIScale, iHeight*m_flUIScale );
	}

}

//-----------------------------------------------------------------------------
// Purpose: shared code for sizing the HTML surface window
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserResize( int iWideRT, int iTallRT, int iWideBrowser, int iTallBrowser )
{
	m_iCachedWideRT = iWideRT;
	m_iCachedTallRT = iTallRT;
	m_iCachedWideBrowser = iWideBrowser;
	m_iCachedTallBrowser = iTallBrowser;

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
	{
		m_bCachedResize = true;
		return;
	}

	m_bCachedResize = false;

	// Message the resize to chrome.
	m_SteamAPIContext.SteamHTMLSurface()->SetSize( m_unBrowserHandle, iWideBrowser, iTallBrowser );
}

//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OpenURL(const char *URL, const char *postData )
{
	PostURL( URL, postData );
}

//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::PostURL(const char *URL, const char *pchPostData )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
	{
		m_sPendingURLLoad = URL;
		m_sPendingPostData = pchPostData;
		return;
	}

	m_sLoadingURL = URL;

	if ( pchPostData && Q_strlen( pchPostData ) > 0)
		m_SteamAPIContext.SteamHTMLSurface()->LoadURL( m_unBrowserHandle, URL, pchPostData );
	else
		m_SteamAPIContext.SteamHTMLSurface()->LoadURL( m_unBrowserHandle, URL, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
bool CHtmlControlScaleform::StopLoading()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return false;

	m_SteamAPIContext.SteamHTMLSurface()->StopLoad( m_unBrowserHandle );
	m_bStopped = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: refreshes the current page
//-----------------------------------------------------------------------------
bool CHtmlControlScaleform::Refresh()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return false;

	m_SteamAPIContext.SteamHTMLSurface()->Reload( m_unBrowserHandle );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Tells the browser control to go back
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::GoBack()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->GoBack( m_unBrowserHandle );
}


//-----------------------------------------------------------------------------
// Purpose: Tells the browser control to go forward
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::GoForward()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->GoForward( m_unBrowserHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Checks if the browser can go back further
//-----------------------------------------------------------------------------
bool CHtmlControlScaleform::BCanGoBack()
{
	return m_bCanGoBack;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the browser can go forward further
//-----------------------------------------------------------------------------
bool CHtmlControlScaleform::BCanGoFoward()
{
	return m_bCanGoForward;
}

void CHtmlControlScaleform::OnBrowserReady( HTML_BrowserReady_t *pBrowserReady, bool bIOFailure )
{
#ifdef SCALE_MSGS
	Msg( "OnBrowserReady()\n" );
#endif

	m_unBrowserHandle = pBrowserReady->unBrowserHandle;

	if ( m_bCachedZoom )
	{
		SetZoomLevel( m_flCachedZoom );
	}
	if ( m_bCachedResize )
	{
		BrowserResize( m_iCachedWideRT, m_iCachedTallRT, m_iCachedWideBrowser, m_iCachedTallBrowser );
	}

	// Only post the pending URL if we have set our OAuth token
	if ( !m_sPendingURLLoad.IsEmpty() )
	{
		PostURL( m_sPendingURLLoad, m_sPendingPostData );
		m_sPendingURLLoad.Clear();
	}

	// Notify the parent that the browser's been initialized
	if ( m_pParent )
	{
		m_pParent->BrowserReady();
	}
}

//-----------------------------------------------------------------------------
// Purpose: we have a new texture to update
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserNeedsPaint( HTML_NeedsPaint_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	// If Steam gave us a NULL pointer, something bad happened
	if ( pCmd->pBGRA == NULL )
		return;
	
#ifdef SCALE_MSGS
	Msg( "BrowserNeedsPaint(), pCmd->flPageScale = %f\n", pCmd->flPageScale );
#endif	
		
	// Update texture
	g_pScaleformUI->ChromeHTMLImageUpdate( m_proceduralTextureId, (const byte *)pCmd->pBGRA, pCmd->unWide, pCmd->unTall, IMAGE_FORMAT_BGRA8888 );

	if ( ( m_scrollVertical.m_bVisible && pCmd->unScrollY > 0 && abs( (int)pCmd->unScrollY - m_scrollVertical.m_nScroll) > 5 ) || ( m_scrollHorizontal.m_bVisible && pCmd->unScrollX > 0 && abs( (int)pCmd->unScrollX - m_scrollHorizontal.m_nScroll ) > 5 ) )
	{
		m_scrollVertical.m_nScroll = pCmd->unScrollY;
		m_scrollHorizontal.m_nScroll = pCmd->unScrollX;

		m_pParent->UpdateHTMLScrollbar( m_scrollHorizontal.m_nScroll, m_scrollHorizontal.m_nWide, m_scrollHorizontal.m_nMax, m_scrollHorizontal.m_bVisible, false );
		m_pParent->UpdateHTMLScrollbar( m_scrollVertical.m_nScroll, m_scrollVertical.m_nTall, m_scrollVertical.m_nMax, m_scrollVertical.m_bVisible, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: browser wants to start loading this url, do we let it?
//-----------------------------------------------------------------------------
bool CHtmlControlScaleform::OnStartRequest( const char *url, const char *target, const char *pchPostData, bool bIsRedirect )
{
	if ( !url || !Q_stricmp( url, "about:blank") )
		return true ; // this is just webkit loading a new frames contents inside an existing page

	HideFindDialog();
	// see if we have a custom handler for this
	bool bURLHandled = false;
	FOR_EACH_VEC( m_URLHandlerDelegates, i )
	{
		if ( m_URLHandlerDelegates[i]( url ) )
		{
			bURLHandled = true;
			break;
		}
	}

	if (bURLHandled)
	{
		return false;
	}

	if ( m_bNewWindowsOnly && bIsRedirect )
	{
		if ( target && ( !Q_stricmp( target, "_blank" ) || !Q_stricmp( target, "_new" ) )  ) // only allow NEW windows (_blank ones)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: callback from cef thread, load a url please
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserStartRequest( HTML_StartRequest_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	bool bRes = OnStartRequest( pCmd->pchURL, pCmd->pchTarget, pCmd->pchPostData, pCmd->bIsRedirect );

	m_SteamAPIContext.SteamHTMLSurface()->AllowStartRequest( m_unBrowserHandle, bRes );

	m_bLoadingPage = bRes;
	m_bStopped = false;
}

//-----------------------------------------------------------------------------
// Purpose: browser went to a new url
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserURLChanged( HTML_URLChanged_t *pCmd )
{
#ifdef SCALE_MSGS
	Msg( "BrowserURLChanged\n" );
#endif

	CHECK_BROWSER_HANDLE();

	m_sCurrentURL = pCmd->pchURL;

	OnURLChanged( m_sCurrentURL, pCmd->pchPostData, pCmd->bIsRedirect );

	if ( m_flDesiredZoom != NO_HTML_ZOOM )
	{
		SetZoomLevel( m_flDesiredZoom );
		m_flLastRequestedZoom = m_flDesiredZoom;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::OnFinishRequest( const char *url, const char *pageTitle )
{
	//m_bPostRequestPaint = true;
	m_bLoadingPage = false;

	m_pParent->OnFinishRequest( url, pageTitle );
	
	// TODO
	/*if ( m_pParent->FlashAPIIsValid() )
	{
		PARENT_WITH_SLOT_LOCKED();
		g_pScaleformUI->Value_InvokeWithoutReturn( m_pParent->GetFlashAPI(), "tweenChromeBrowser", NULL, 0 );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: finished loading this page
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserFinishedRequest( HTML_FinishedRequest_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	OnFinishRequest( pCmd->pchURL, pCmd->pchPageTitle );
}

//-----------------------------------------------------------------------------
// Purpose: browser telling us the state of back and forward buttons
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserCanGoBackandForward( HTML_CanGoBackAndForward_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	m_bCanGoBack = pCmd->bCanGoBack;
	m_bCanGoForward = pCmd->bCanGoForward;
}

//-----------------------------------------------------------------------------
// Purpose: update the value of the cached variables we keep
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::UpdateCachedHTMLValues()
{
}

//-----------------------------------------------------------------------------
// Purpose: browser telling us the size of the horizontal scrollbars
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserHorizontalScrollBarSizeResponse( HTML_HorizontalScroll_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	ScrollData_t scrollHorizontal;
	// Assumes scrollbar is not visible (by ignoring message) if requested scale and scale from the message do not match
	if ( pCmd->flPageScale == m_flLastRequestedZoom )
	{
		//	Same scale - update scrollbar
		scrollHorizontal.m_nScroll = pCmd->unScrollCurrent;
		scrollHorizontal.m_nMax = pCmd->unScrollMax;
		scrollHorizontal.m_bVisible = pCmd->bVisible;
		scrollHorizontal.m_flZoom = pCmd->flPageScale;
	}

	if ( scrollHorizontal != m_scrollHorizontal )
	{
		m_scrollHorizontal = scrollHorizontal;
		m_pParent->UpdateHTMLScrollbar( m_scrollHorizontal.m_nScroll, m_scrollHorizontal.m_nWide, m_scrollHorizontal.m_nMax, m_scrollHorizontal.m_bVisible, false );
	}
	else
	{
		m_scrollHorizontal = scrollHorizontal;
	}
}

//-----------------------------------------------------------------------------
// Purpose: browser telling us the size of the vertical scrollbars
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserVerticalScrollBarSizeResponse( HTML_VerticalScroll_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	ScrollData_t scrollVertical;
	scrollVertical.m_nScroll = pCmd->unScrollCurrent;
	scrollVertical.m_nMax = pCmd->unScrollMax;
	scrollVertical.m_bVisible = pCmd->bVisible;
	scrollVertical.m_flZoom = pCmd->flPageScale;

	if ( scrollVertical != m_scrollVertical )
	{
		m_scrollVertical = scrollVertical;
		m_pParent->UpdateHTMLScrollbar( m_scrollVertical.m_nScroll, m_scrollVertical.m_nTall, m_scrollVertical.m_nMax, m_scrollVertical.m_bVisible, true );
	}
	else
	{
		m_scrollVertical = scrollVertical;
	}
}

//-----------------------------------------------------------------------------
// Purpose: browser telling us to change our mouse cursor
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserSetCursor( HTML_SetCursor_t *pCmd )
{
	EMouseCursor cursor = (EMouseCursor)pCmd->eMouseCursor;
 	vgui::input()->SetCursorOveride( cursor );
}

//-----------------------------------------------------------------------------
// Purpose: status bar details
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserStatusText( HTML_StatusText_t *pCmd )
{
	FOR_EACH_VEC( m_BrowserStatusTextDelegates, i )
	{
		m_BrowserStatusTextDelegates[i]( pCmd->pchMsg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: display a new html window 
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::BrowserPopupHTMLWindow( HTML_NewWindow_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	// Remove intermediate browser created by cef / steam_api
	if ( m_SteamAPIContext.SteamHTMLSurface() )
	{
		Assert( pCmd->unNewWindow_BrowserHandle != m_unBrowserHandle );
		m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser( pCmd->unNewWindow_BrowserHandle );
	}
}

//-----------------------------------------------------------------------------
// AS OF YET UNIMPLEMENTED BROWSER FEATURES
//-----------------------------------------------------------------------------

// TODO: Tabs
void CHtmlControlScaleform::BrowserOpenNewTab( HTML_OpenLinkInNewTab_t *pCmd ) { Assert(0); }

// TODO: File Access
void CHtmlControlScaleform::BrowserFileLoadDialog( HTML_FileOpenDialog_t *pCmd ) { Assert(0); }

// TODO: Tool Tips
void CHtmlControlScaleform::BrowserShowToolTip( HTML_ShowToolTip_t *pCmd ) { Assert(0); }
void CHtmlControlScaleform::BrowserUpdateToolTip( HTML_UpdateToolTip_t *pCmd ) { Assert(0); }
void CHtmlControlScaleform::BrowserHideToolTip( HTML_HideToolTip_t *pCmd ) { Assert(0); }

// TODO: Page Search
void CHtmlControlScaleform::BrowserSearchResults( HTML_SearchResults_t *pCmd ) { Assert(0); }

// TODO: Javascript
void CHtmlControlScaleform::BrowserJSAlert( HTML_JSAlert_t *pCmd )
{
	if ( pCmd->unBrowserHandle != m_unBrowserHandle )
		return;

	m_pParent->HandleJsAlert( pCmd->pchMessage );
}
void CHtmlControlScaleform::BrowserJSConfirm( HTML_JSConfirm_t *pCmd ) { Assert(0); }

// TODO: ???
void CHtmlControlScaleform::BrowserClose( HTML_CloseBrowser_t *pCmd ) { Assert(0); }
void CHtmlControlScaleform::BrowserLinkAtPositionResponse( HTML_LinkAtPosition_t *pCmd ) { Assert(0); }

//-----------------------------------------------------------------------------
// Purpose: install callbacks that listen can respond to URL changes
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::AddURLHandlerDelegate( CUtlDelegate< bool ( const char * ) > pfnURLHandler )
{
	m_URLHandlerDelegates.AddToTail( pfnURLHandler );
}

//-----------------------------------------------------------------------------
// Purpose: install callbacks that listen for responses to link at position queries
//-----------------------------------------------------------------------------
void CHtmlControlScaleform::AddBrowserStatusTextDelegate( CUtlDelegate< void ( const char * ) > pfnStatusHandler )
{
	m_BrowserStatusTextDelegates.AddToTail( pfnStatusHandler );
}

#endif // INCLUDE_SCALEFORM

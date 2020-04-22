//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Control to display html content (Scaleform version)
//
// $NoKeywords: $
//=============================================================================//
#if defined( INCLUDE_SCALEFORM )

#ifndef HTML_CONTROL_SCALEFORM_H
#define HTML_CONTROL_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#ifndef VERSION_SAFE_STEAM_API_INTERFACES
#define VERSION_SAFE_STEAM_API_INTERFACES
#endif
#include "steam/steam_api.h"

#include "tier1/utlstring.h"

class IHtmlParentScaleform
{
public:

	virtual void InitChromeHTMLRenderTarget( const char* pszTextureName ) = 0;
	virtual void BrowserReady() = 0;
	virtual void HandleJsAlert( char const *pchAlert ) = 0;
	virtual void OnFinishRequest( const char *url, const char *pageTitle ) = 0;
	virtual void UpdateHTMLScrollbar( int iScroll, int iTall, int iMax, bool bVisible, bool bVert ) = 0;
};

class CHtmlControlScaleform 
{
public:

	CHtmlControlScaleform();
	virtual ~CHtmlControlScaleform();

	virtual void Init( IHtmlParentScaleform* pParent );

	// HTML Interface Functions
	virtual void OpenURL( const char *URL, const char *pchPostData );
	virtual void BrowserResize( int iWideRT, int iTallRT, int iWideBrowser, int iTallBrowser );
	virtual bool StopLoading();
	virtual bool Refresh();
	virtual void GoBack();
	virtual void GoForward();
	virtual bool BCanGoBack();
	virtual bool BCanGoFoward();

	bool IsLoadingPage() { return m_bLoadingPage && !m_bStopped; }
	const char* GetCurrentURL() { return m_sCurrentURL.Get(); }

	void SetBrowserBaseSize( int iWidth, int iHeight, int iStageWidth, int iStageHeight );
	void Update( void );

	// Browser Control
	void Find( const char *pchSubStr ) {}
	void StopFind() {}
	void FindNext() {}
	void FindPrevious() {}
	void ShowFindDialog() {}
	void HideFindDialog() {}
	bool FindDialogVisible() { return false; }
	void SetZoomLevel( float flZoom );

	void ExecuteJavascript( const char *pchScript );
	void CallbackJavascriptDialogResponse( bool bResult );

	// Input Passthrough
	void OnHTMLScrollBarMoved( int iPosition, bool bVert );
	virtual void OnMouseMoved( int x, int y );
	virtual void OnMouseDown( ButtonCode_t button, int x, int y );
	virtual void OnMouseUp( ButtonCode_t button, int x, int y );
	virtual void OnMouseWheeled( int delta );
	virtual void OnKeyDown( ButtonCode_t code );
	virtual void OnKeyUp( ButtonCode_t code );
	virtual void OnKeyTyped( wchar_t unichar );

	void AddURLHandlerDelegate( CUtlDelegate< bool ( const char * ) > pfnURLHandler );
	void AddBrowserStatusTextDelegate( CUtlDelegate< void ( const char * ) > pfnURLHandler );

protected:

	void PostURL( const char *URL, const char *pchPostData );

	virtual bool OnStartRequest( const char *url, const char *target, const char *pchPostData, bool bIsRedirect );
	virtual void OnFinishRequest( const char *url, const char *pageTitle );
	virtual void OnSetHTMLTitle( const char *pchTitle ) {}
	virtual void OnURLChanged( const char *url, const char *pchPostData, bool bIsRedirect ) {}

	void UpdateCachedHTMLValues();

private:

	/************************************************************
	 *  IHTMLResponses callbacks
	 */
	ISteamHTMLSurface *SteamHTMLSurface() { return m_SteamAPIContext.SteamHTMLSurface(); }

	STEAM_CALLBACK(CHtmlControlScaleform, BrowserNeedsPaint, HTML_NeedsPaint_t, m_NeedsPaint);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserStartRequest, HTML_StartRequest_t, m_StartRequest);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserURLChanged, HTML_URLChanged_t, m_URLChanged);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserFinishedRequest, HTML_FinishedRequest_t, m_FinishedRequest);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserPopupHTMLWindow, HTML_NewWindow_t, m_NewWindow);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserHorizontalScrollBarSizeResponse, HTML_HorizontalScroll_t, m_HorizScroll);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserVerticalScrollBarSizeResponse, HTML_VerticalScroll_t, m_VertScroll);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserJSAlert, HTML_JSAlert_t, m_JSAlert);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserJSConfirm, HTML_JSConfirm_t, m_JSConfirm);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserCanGoBackandForward, HTML_CanGoBackAndForward_t, m_CanGoBackForward);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserSetCursor, HTML_SetCursor_t, m_SetCursor);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserStatusText, HTML_StatusText_t, m_StatusText );
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserShowToolTip, HTML_ShowToolTip_t, m_ShowToolTip);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserUpdateToolTip, HTML_UpdateToolTip_t, m_UpdateToolTip);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserHideToolTip, HTML_HideToolTip_t, m_HideToolTip);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserSearchResults, HTML_SearchResults_t, m_SearchResults);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserOpenNewTab, HTML_OpenLinkInNewTab_t, m_OpenLinkInNewTab);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserFileLoadDialog, HTML_FileOpenDialog_t, m_FileLoadDialog);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserClose, HTML_CloseBrowser_t, m_Close);
	STEAM_CALLBACK(CHtmlControlScaleform, BrowserLinkAtPositionResponse, HTML_LinkAtPosition_t, m_LinkAtPosition);

	void OnBrowserReady(HTML_BrowserReady_t *pBrowserReady, bool bIOFailure);

	static uint64 m_nextProceduralTextureId;

	IHtmlParentScaleform* m_pParent;

	CUtlString m_strProceduralTextureName;
	// id used to identify chrome image when calling ScaleformUI::ChromeHTMLImageAddRef ... functions
	uint64 m_proceduralTextureId;

	int m_iBrowser; // our browser handle
	CUtlString m_sCurrentURL; // the url of our current page

	CUtlString m_sLoadingURL;
	CUtlString m_sPendingURLLoad; // cache of url to load if we get a PostURL before the cef object is mage
	CUtlString m_sPendingPostData; // cache of the post data for above

	// For resizes sent before the browser is initialized.
	bool m_bCachedResize;
	int m_iCachedTallRT;
	int m_iCachedWideRT;
	int m_iCachedTallBrowser;
	int m_iCachedWideBrowser;
	float m_flUIScale;

	// For zoom messages sent before the browser is initialized.
	bool m_bCachedZoom;
	float m_flCachedZoom;

	int m_iMouseX;
	int m_iMouseY;
	int m_iVertScrollStep;

	bool m_bLoadingPage;
	bool m_bStopped;
	bool m_bFullRepaint;
	bool m_bNewWindowsOnly;

	// cache of forward and back state
	bool m_bCanGoBack; 
	bool m_bCanGoForward;

	// Scroll Bars
	struct ScrollData_t
	{
		ScrollData_t() 
		{
			m_bVisible = false;
			m_nX = m_nY = m_nWide = m_nTall = m_nMax = 0;
			m_nScroll = -1;
		}

		bool operator==( ScrollData_t const &src ) const
		{
			return m_bVisible == src.m_bVisible && 
				m_nX == src.m_nX &&
				m_nY == src.m_nY &&
				m_nWide == src.m_nWide &&
				m_nTall == src.m_nTall &&
				m_nMax == src.m_nMax &&
				m_nScroll == src.m_nScroll;
		}

		bool operator!=( ScrollData_t const &src ) const
		{	
			return !operator==(src);
		}


		bool m_bVisible; // is the scroll bar visible
		int m_nX; /// where cef put the scroll bar
		int m_nY;
		int m_nWide;
		int m_nTall;  // how many pixels of scroll in the current scroll knob
		int m_nMax; // most amount of pixels we can scroll
		int m_nScroll; // currently scrolled amount of pixels
		float m_flZoom; // zoom level this scroll bar is for
	};

	ScrollData_t m_scrollHorizontal; // details of horizontal scroll bar
	ScrollData_t m_scrollVertical; // details of vertical scroll bar

	CUtlVector< CUtlDelegate< bool ( const char * ) > > m_URLHandlerDelegates;
	CUtlVector< CUtlDelegate< void ( const char * ) > > m_BrowserStatusTextDelegates;

	float m_flZoom; // current page zoom level
	float m_flDesiredZoom;
	float m_flLastRequestedZoom;

	CSteamAPIContext m_SteamAPIContext;
	HHTMLBrowser m_unBrowserHandle;
	CCallResult< CHtmlControlScaleform, HTML_BrowserReady_t > m_SteamCallResultBrowserReady;
};

#endif // HTML_CONTROL_SCALEFORM_H
#endif // include scaleform

//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for every scaleform screen that need to display html
//		    (using Chrome to display html in a texture)
//
// $NoKeywords: $
//=============================================================================//
#if defined( INCLUDE_SCALEFORM )

#ifndef HTML_BASE_SCALEFORM_H
#define HTML_BASE_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"
#include "html_control_scaleform.h"

class CHtmlBaseScaleform : public ScaleformFlashInterface, public IHtmlParentScaleform
{
public:

	CHtmlBaseScaleform();
	virtual ~CHtmlBaseScaleform();

	void InitChromeHTML( const char* pszBaseURL, const char *pszPostData );

	virtual void Update();

	// Override ScaleformFlashInterface methods
	virtual void PostUnloadFlash( void );

	// Implements IHtmlParentScaleform interface
	// (C++ calling ActionScript functions)
	virtual void InitChromeHTMLRenderTarget( const char* pszTextureName );
	virtual void BrowserReady() { }
	virtual void OnFinishRequest( const char *url, const char *pageTitle ) { }
	virtual void HandleJsAlert( char const *pchAlert ) { Assert( 0 ); }
	virtual void UpdateHTMLScrollbar( int iScroll, int iTall, int iMax, bool bVisible, bool bVert );

	// HTML Window Input Handlers
	// (ActionScript calling C++ functions)
	void	OnHTMLMouseDown( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLMouseUp( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLMouseMove( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLMouseWheel( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLKeyDown( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLKeyUp( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLKeyTyped( SCALEFORM_CALLBACK_ARGS_DECL );
	void	SetHTMLBrowserSize( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLScrollBarChanged( SCALEFORM_CALLBACK_ARGS_DECL );

	// HTML Window Controls
	// (ActionScript calling C++ functions)
	void	OnHTMLBackButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLForwardButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLRefreshButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLStopButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL );
	void	OnHTMLExternalBrowserButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL );
	
protected:
	virtual bool BShouldPreventInputRouting();

	CHtmlControlScaleform*	m_pChromeHTML;

	bool					m_bLastCanGoBack;
	bool					m_bLastCanGoForward;
	bool					m_bLastIsLoadingState;
};

// ActionScript calling C++ via GameApi
#define IMPLEMENT_HTML_SFUI_METHODS \
	SFUI_DECL_METHOD( OnHTMLMouseDown ), \
	SFUI_DECL_METHOD( OnHTMLMouseUp ), \
	SFUI_DECL_METHOD( OnHTMLMouseMove ), \
	SFUI_DECL_METHOD( OnHTMLMouseWheel ), \
	SFUI_DECL_METHOD( OnHTMLKeyDown ), \
	SFUI_DECL_METHOD( OnHTMLKeyUp ), \
	SFUI_DECL_METHOD( OnHTMLKeyTyped ), \
	SFUI_DECL_METHOD( SetHTMLBrowserSize ), \
	SFUI_DECL_METHOD( OnHTMLScrollBarChanged ), \
	SFUI_DECL_METHOD( OnHTMLBackButtonClicked ), \
	SFUI_DECL_METHOD( OnHTMLForwardButtonClicked ), \
	SFUI_DECL_METHOD( OnHTMLRefreshButtonClicked ), \
	SFUI_DECL_METHOD( OnHTMLStopButtonClicked ), \
	SFUI_DECL_METHOD( OnHTMLExternalBrowserButtonClicked )

// ActionScript calling C++ via Component api
#define SCALEFORM_COMPONENT_HTML_FUNCTIONS_API_DEF( componentclass ) \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnLoadFinished, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnUnload, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnReady, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLMouseDown, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLMouseUp, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLMouseMove, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLMouseWheel, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLKeyDown, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLKeyUp, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLKeyTyped, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, SetHTMLBrowserSize, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLScrollBarChanged, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLBackButtonClicked, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLForwardButtonClicked, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLRefreshButtonClicked, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLStopButtonClicked, componentclass ), \
	SCALEFORM_COMPONENT_FUNCTION_API_DEF_NOPREFIX( void, OnHTMLExternalBrowserButtonClicked, componentclass ),

#endif // HTML_BASE_SCALEFORM_H
#endif // include scaleform
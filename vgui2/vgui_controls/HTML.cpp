//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
// This class is a message box that has two buttons, ok and cancel instead of
// just the ok button of a message box. We use a message box class for the ok button
// and implement another button here.
//
// $NoKeywords: $ƒ
//=============================================================================//

#include "vgui_controls/pch_vgui_controls.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MessageBox.h>

#include <tier0/memdbgoff.h>
#include <tier0/memdbgon.h>

#include "filesystem.h"
#include "../vgui2/src/vgui_key_translation.h"
#include "../public/input/mousecursors.h"
#undef schema
#undef PostMessage
#undef MessageBox

#include "tier0/vprof.h"
#include "OfflineMode.h"

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file
#include "tier0/memdbgon.h"

using namespace vgui;

// This isn't preferable, would likely work better to have only one system receiving the callbacks and dispatching them properly
#define CHECK_BROWSER_HANDLE() do { if ( pCmd->unBrowserHandle != m_unBrowserHandle ) return; } while ( false )

const int k_nMaxCustomCursors = 2; // the max number of custom cursors we keep cached PER html control

bool gScaleformBrowserActive = false;

// Workaround to only allow one popup window to be visible
// Try to avoid malicious server from opening multiple window on the client side
static bool gMOTDPopupWindowActive = false;

//-----------------------------------------------------------------------------
// Purpose: A simple passthrough panel to render the border onto the HTML widget
//-----------------------------------------------------------------------------
class HTMLInterior : public Panel
{
	DECLARE_CLASS_SIMPLE( HTMLInterior, Panel );
public:
	HTMLInterior( HTML *parent ) : BaseClass( parent, "HTMLInterior" ) 
	{ 	
		m_pHTML = parent; 
		SetPaintBackgroundEnabled( false );
		// TODO::STYLE
		//SetPaintAppearanceEnabled( true );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( false );
	}

	// TODO::STYLE
	/*
	virtual int GetStyleFlags()
	{
		int nStyleFlags = BaseClass::GetStyleFlags();

		if ( m_pHTML->IsScrollbarVisible() )
			nStyleFlags |= k_EStyleFlagScrollbarVisible;

		return nStyleFlags;
	}
	*/

	/*
	virtual void Paint()
	{
		// super-hacky paint
		// TODO::STYLE
		PaintAppearanceBackground();
		PaintAppearance();
	}*/

private:
	HTML *m_pHTML;
};


//-----------------------------------------------------------------------------
// Purpose: container class for any external popup windows the browser requests
//-----------------------------------------------------------------------------
class HTMLPopup : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( HTMLPopup, vgui::Frame );
	class PopupHTML : public vgui::HTML
	{
		DECLARE_CLASS_SIMPLE( PopupHTML, vgui::HTML );
	public:
		PopupHTML( Frame *parent, const char *pchName, bool allowJavaScript , bool bPopupWindow, HHTMLBrowser unBrowserHandleToDelete  ) : HTML( parent, pchName, allowJavaScript, bPopupWindow, unBrowserHandleToDelete ) { m_pParent = parent; }

		virtual void OnSetHTMLTitle( const char *pchTitle )
		{
			BaseClass::OnSetHTMLTitle( pchTitle );
			m_pParent->SetTitle( pchTitle, true );
		}
	private:
		Frame *m_pParent;
	};
public:
	HTMLPopup( Panel *parent, HHTMLBrowser unBrowserHandleToDelete, const char *pchURL, const char *pchTitle ) : Frame( NULL, "HtmlPopup", true )
	{
		m_pHTML = new PopupHTML( this, "htmlpopupchild", true, true, unBrowserHandleToDelete );
		m_pHTML->OpenURL( pchURL, NULL );
		SetTitle( pchTitle, true );

		// Workaround to only allow one popup window to be visible
		// Try to avoid malicious server from opening multiple window on the client side
		gMOTDPopupWindowActive = true;
	}

	~HTMLPopup()
	{
		gMOTDPopupWindowActive = false;
	}

	enum
	{
		vert_inset = 40,
		horiz_inset = 6
	};

	void PerformLayout()
	{
		BaseClass::PerformLayout();
		int wide, tall;
		GetSize( wide, tall );
		m_pHTML->SetPos( horiz_inset, vert_inset );
		m_pHTML->SetSize( wide - horiz_inset*2, tall - vert_inset*2 );
	}

	void SetBounds( int x, int y, int wide, int tall )
	{
		BaseClass::SetBounds( x, y, wide + horiz_inset*2, tall + vert_inset*2 );
	}

	MESSAGE_FUNC( OnCloseWindow, "OnCloseWindow" )
	{
		Close();
	}
private:
	PopupHTML *m_pHTML;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
HTML::HTML( Panel *parent, const char *name, bool allowJavaScript /*= false*/, bool bPopupWindow /*= false*/, HHTMLBrowser unBrowserHandleToDelete /*= INVALID_HTMLBROWSER*/ ) 
:
Panel( parent, name ),
m_unBrowserHandle( INVALID_HTMLBROWSER ),
m_unBrowserHandleToDelete( unBrowserHandleToDelete ),
m_bPopupWindow( bPopupWindow ),
m_NeedsPaint( this, &HTML::BrowserNeedsPaint ),
m_StartRequest( this, &HTML::BrowserStartRequest ),
m_URLChanged( this, &HTML::BrowserURLChanged ),
m_FinishedRequest( this, &HTML::BrowserFinishedRequest ),
m_HorizScroll( this, &HTML::BrowserHorizontalScrollBarSizeResponse ),
m_VertScroll( this, &HTML::BrowserVerticalScrollBarSizeResponse ),
m_CanGoBackForward( this, &HTML::BrowserCanGoBackandForward ),
m_SetCursor( this, &HTML::BrowserSetCursor ),
m_Close( this, &HTML::BrowserClose ),
m_FileOpenDialog( this, &HTML::BrowserFileOpenDialog ),
m_ShowToolTip( this, &HTML::BrowserShowToolTip ),
m_UpdateToolTip( this, &HTML::BrowserUpdateToolTip ),
m_HideToolTip( this, &HTML::BrowserHideToolTip ),
m_SearchResults( this, &HTML::BrowserSearchResults ),
m_LinkAtPositionResponse( this, &HTML::BrowserLinkAtPositionResponse ),
m_JSAlert( this, &HTML::BrowserJSAlert ),
m_JSConfirm( this, &HTML::BrowserJSConfirm ),
m_NewWindow( this, &HTML::BrowserPopupHTMLWindow )
{
	m_iHTMLTextureID = 0;
	m_iComboBoxTextureID = 0;
	m_bCanGoBack = false;
	m_bCanGoForward = false;
	m_bInFind = false;
	m_bRequestingDragURL = false;
	m_bRequestingCopyLink = false;
	m_flZoom = 100.0f;
	m_iBrowser = -1;

	m_pInteriorPanel = new HTMLInterior( this );
	SetPostChildPaintEnabled( true );

	// Initialize the HTML surface and create the browser instance
	m_SteamAPIContext.Init();
	if ( m_SteamAPIContext.SteamHTMLSurface() )
	{
		m_SteamAPIContext.SteamHTMLSurface()->Init();

		SteamAPICall_t hSteamAPICall = m_SteamAPIContext.SteamHTMLSurface()->CreateBrowser( surface()->GetWebkitHTMLUserAgentString(), "::-webkit-scrollbar { background-color: #000000; } ::-webkit-scrollbar-track { border: 1px solid #7f7f7f; background: #000000; } ::-webkit-scrollbar-thumb{ border: 1px solid #7f7f7f; background: #000000; } ::-webkit-scrollbar-button{ border: 1px solid #7f7f7f; background: #000000; }" );
		m_SteamCallResultBrowserReady.Set( hSteamAPICall, this, &HTML::OnBrowserReady );
	}
	else
	{
		Warning( "Unable to access SteamHTMLSurface" );
	}

	m_iScrollBorderX=m_iScrollBorderY=0;

	// webkit handling the scrollbars
	m_bScrollBarEnabled = false; 

	m_bContextMenuEnabled = true; 
	m_bNewWindowsOnly = false;
	m_iMouseX = m_iMouseY = 0;
	m_iDragStartX = m_iDragStartY = 0;
	m_nViewSourceAllowedIndex = -1;
	m_iWideLastHTMLSize = m_iTalLastHTMLSize = 0;

	_hbar = new ScrollBar(this, "HorizScrollBar", false);
	_hbar->SetVisible(false);
	_hbar->AddActionSignalTarget(this);

	_vbar = new ScrollBar(this, "VertScrollBar", true);
	_vbar->SetVisible(false);
	_vbar->AddActionSignalTarget(this);

	m_pFindBar = new HTML::CHTMLFindBar( this );
	m_pFindBar->SetZPos( 2 );
	m_pFindBar->SetVisible( false );
	// TODO::STYLE
	//m_pFindBar->SetStyle( "html-findbar" );

	m_pContextMenu = new Menu( this, "contextmenu" );
	m_pContextMenu->AddMenuItem( "#vgui_HTMLBack", new KeyValues( "Command", "command", "back" ), this );
	m_pContextMenu->AddMenuItem( "#vgui_HTMLForward", new KeyValues( "Command", "command", "forward" ), this );
	m_pContextMenu->AddMenuItem( "#vgui_HTMLReload", new KeyValues( "Command", "command", "reload" ), this );
	m_pContextMenu->AddMenuItem( "#vgui_HTMLStop", new KeyValues( "Command", "command", "stop" ), this );
	m_pContextMenu->AddSeparator();
	m_pContextMenu->AddMenuItem( "#vgui_HTMLCopyUrl", new KeyValues( "Command", "command", "copyurl" ), this );
	m_iCopyLinkMenuItemID = m_pContextMenu->AddMenuItem( "#vgui_HTMLCopyLink", new KeyValues( "Command", "command", "copylink" ), this );
	m_pContextMenu->AddMenuItem( "#TextEntry_Copy", new KeyValues( "Command", "command", "copy" ), this );
	m_pContextMenu->AddMenuItem( "#TextEntry_Paste", new KeyValues( "Command", "command", "paste" ), this );
	m_pContextMenu->AddSeparator();
	m_nViewSourceAllowedIndex = m_pContextMenu->AddMenuItem( "#vgui_HTMLViewSource", new KeyValues( "Command", "command", "viewsource" ), this );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
HTML::~HTML()
{
	m_pContextMenu->MarkForDeletion();

	if ( m_SteamAPIContext.SteamHTMLSurface() )
	{
		m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser ( m_unBrowserHandle );
	}

	FOR_EACH_VEC( m_vecHCursor, i )
	{
		// BR FIXME!
//		surface()->DeleteCursor( m_vecHCursor[i].m_Cursor );
	}
	m_vecHCursor.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Handle message to change our cursor
//-----------------------------------------------------------------------------
void HTML::OnSetCursorVGUI( int cursor )
{
	SetCursor( (HCursor)cursor );
}

//-----------------------------------------------------------------------------
// Purpose: sets up colors/fonts/borders
//-----------------------------------------------------------------------------
void HTML::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
	BrowserResize();
}


//-----------------------------------------------------------------------------
// Purpose: overrides panel class, paints a texture of the HTML window as a background
//-----------------------------------------------------------------------------
void HTML::Paint()
{
	//VPROF_BUDGET( "HTML::Paint()", VPROF_BUDGETGROUP_OTHER_VGUI );
	BaseClass::Paint();

	if ( m_iHTMLTextureID != 0 )
	{
		surface()->DrawSetTexture( m_iHTMLTextureID );
		int tw = 0, tt = 0;
		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );

		int vbarInset = _vbar->IsVisible() ? _vbar->GetWide() : 0;
		GetSize( tw, tt );
		surface()->DrawTexturedRect( 0, 0, tw-vbarInset, tt );
	}

	// If we have scrollbars, we need to draw the bg color under them, since the browser
	// bitmap is a checkerboard under them, and they are transparent in the in-game client
	if ( m_iScrollBorderX > 0 || m_iScrollBorderY > 0 )
	{
		int w, h;
		GetSize( w, h );
		IBorder *border = GetBorder();
		int left = 0, top = 0, right = 0, bottom = 0;
		if ( border )
		{
			border->GetInset( left, top, right, bottom );
		}
		surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
		if ( m_iScrollBorderX )
		{
			surface()->DrawFilledRect( w - _vbar->GetWide(), top, w, h - bottom );
		}
		if ( m_iScrollBorderY )
		{
			surface()->DrawFilledRect( left, h-m_iScrollBorderY - bottom, w-m_iScrollBorderX - right, h );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: paint the combo box texture if we have one
//-----------------------------------------------------------------------------
void HTML::PaintComboBox()
{
	BaseClass::Paint();
	if ( m_iComboBoxTextureID != 0 )
	{
		surface()->DrawSetTexture( m_iComboBoxTextureID );
		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		int tw = m_allocedComboBoxWidth;
		int tt = m_allocedComboBoxHeight;
		surface()->DrawTexturedRect( 0, 0, tw, tt );
	}

}


//-----------------------------------------------------------------------------
// Purpose: causes a repaint when the layout changes
//-----------------------------------------------------------------------------
void HTML::PerformLayout()
{
	BaseClass::PerformLayout();
	Repaint();
	int vbarInset = _vbar->IsVisible() ? _vbar->GetWide() : 0;
	int maxw = GetWide() - vbarInset;
	m_pInteriorPanel->SetBounds( 0, 0, maxw-32, GetTall() );

	IScheme *pClientScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );

	int iSearchInsetY = 5;
	int iSearchInsetX = 5;
	int iSearchTall = 24;
	int iSearchWide = 150;
	const char *resourceString = pClientScheme->GetResourceString( "HTML.SearchInsetY");
	if ( resourceString )
	{
		iSearchInsetY = atoi(resourceString);
	}
	resourceString = pClientScheme->GetResourceString( "HTML.SearchInsetX");
	if ( resourceString )
	{
		iSearchInsetX = atoi(resourceString);
	}
	resourceString = pClientScheme->GetResourceString( "HTML.SearchTall");
	if ( resourceString )
	{
		iSearchTall = atoi(resourceString);
	}
	resourceString = pClientScheme->GetResourceString( "HTML.SearchWide");
	if ( resourceString )
	{
		iSearchWide = atoi(resourceString);
	}

	m_pFindBar->SetBounds( GetWide() - iSearchWide - iSearchInsetX - vbarInset, m_pFindBar->BIsHidden() ? -1*iSearchTall-5: iSearchInsetY, iSearchWide, iSearchTall );
}


//-----------------------------------------------------------------------------
// Purpose: updates the underlying HTML surface widgets position
//-----------------------------------------------------------------------------
void HTML::OnMove()
{
	BaseClass::OnMove();
}


//-----------------------------------------------------------------------------
// Purpose: calculates the need for and position of both horizontal and vertical scroll bars
//-----------------------------------------------------------------------------
void HTML::CalcScrollBars(int w, int h)
{
	bool bScrollbarVisible = _vbar->IsVisible();

	if ( m_bScrollBarEnabled )
	{
		for ( int i = 0; i < 2; i++ )
		{
			int scrollx, scrolly, scrollwide, scrolltall;
			bool bVisible = false;
			if ( i==0 )
			{
				scrollx = m_scrollHorizontal.m_nX;
				scrolly = m_scrollHorizontal.m_nY;
				scrollwide = m_scrollHorizontal.m_nWide;
				scrolltall = m_scrollHorizontal.m_nTall;
				bVisible = m_scrollHorizontal.m_bVisible;

				// scrollbar positioning tweaks - should be moved into a resource file
				scrollwide += 14;
				scrolltall += 5;
			}
			else
			{
				scrollx = m_scrollVertical.m_nX;
				scrolly = m_scrollVertical.m_nY;
				scrollwide = m_scrollVertical.m_nWide;
				scrolltall = m_scrollVertical.m_nTall;
				bVisible = m_scrollVertical.m_bVisible;

				// scrollbar positioning tweaks - should be moved into a resource file
				//scrollx -= 3;
				if ( m_scrollHorizontal.m_bVisible )
					scrolltall += 16;
				else
					scrolltall -= 2;

				scrollwide += 5;
			}
			
			if ( bVisible && scrollwide && scrolltall )
			{
				int panelWide, panelTall;
				GetSize( panelWide, panelTall );

				ScrollBar *bar = _vbar; 
				if ( i == 0 )
					bar = _hbar;
				
				if (!bar->IsVisible())
				{
					bar->SetVisible(true);
					// displayable area has changed, need to force an update
					PostMessage(this, new KeyValues("OnSliderMoved"), 0.02f);
				}

				int rangeWindow = panelTall - scrollwide;
				if ( i==0 )
					rangeWindow = panelWide - scrolltall;
				int range = m_scrollVertical.m_nMax + m_scrollVertical.m_nTall;
				if ( i == 0 )
					range = m_scrollHorizontal.m_nMax + m_scrollVertical.m_nWide;
				int curValue = m_scrollVertical.m_nScroll;
				if ( i == 0 )
					curValue = m_scrollHorizontal.m_nScroll;

				bar->SetEnabled(false);
				bar->SetRangeWindow( rangeWindow );
				bar->SetRange( 0, range ); // we want the range [0.. (img_h - h)], but the scrollbar actually returns [0..(range-rangeWindow)] so make sure -h gets deducted from the max range value	
				bar->SetButtonPressedScrollValue( 5 );
				if ( curValue > ( bar->GetValue() + 5 ) || curValue < (bar->GetValue() - 5 ) )
					bar->SetValue( curValue );

				if ( i == 0 )
				{
					bar->SetPos( 0, h - scrolltall - 1 );
					bar->SetWide( scrollwide );
					bar->SetTall( scrolltall );
				}
				else
				{
					bar->SetPos( w - scrollwide, 0 );
					bar->SetTall( scrolltall );
					bar->SetWide( scrollwide );
				}

				if ( i == 0 )
					m_iScrollBorderY=scrolltall;
				else
					m_iScrollBorderX=scrollwide;
			}
			else
			{
				if ( i == 0 )
				{
					m_iScrollBorderY=0;
					_hbar->SetVisible( false );
				}
				else
				{
					m_iScrollBorderX=0;
					_vbar->SetVisible( false );

				}
			}
		}
	}
	else
	{
		m_iScrollBorderX = 0;
		m_iScrollBorderY=0;
		_vbar->SetVisible(false);
		_hbar->SetVisible(false);
	}

	if ( bScrollbarVisible != _vbar->IsVisible() )
		InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
void HTML::OpenURL(const char *URL, const char *postData )
{
	PostURL( URL, postData );
}

//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
void HTML::PostURL(const char *URL, const char *pchPostData )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
	{
		m_sPendingURLLoad = URL;
		m_sPendingPostData = pchPostData;
		return;
	}

	if ( pchPostData && Q_strlen( pchPostData ) > 0 )
		m_SteamAPIContext.SteamHTMLSurface()->LoadURL( m_unBrowserHandle, URL, pchPostData );
	else
		m_SteamAPIContext.SteamHTMLSurface()->LoadURL( m_unBrowserHandle, URL, NULL );
}


//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
bool HTML::StopLoading()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return false;

	m_SteamAPIContext.SteamHTMLSurface()->StopLoad( m_unBrowserHandle );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: refreshes the current page
//-----------------------------------------------------------------------------
bool HTML::Refresh()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return false;

	m_SteamAPIContext.SteamHTMLSurface()->Reload( m_unBrowserHandle );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Tells the browser control to go back
//-----------------------------------------------------------------------------
void HTML::GoBack()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->GoBack( m_unBrowserHandle );
}


//-----------------------------------------------------------------------------
// Purpose: Tells the browser control to go forward
//-----------------------------------------------------------------------------
void HTML::GoForward()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->GoForward( m_unBrowserHandle );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the browser can go back further
//-----------------------------------------------------------------------------
bool HTML::BCanGoBack()
{
	return m_bCanGoBack;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the browser can go forward further
//-----------------------------------------------------------------------------
bool HTML::BCanGoFoward()
{
	return m_bCanGoForward;
}


//-----------------------------------------------------------------------------
// Purpose: handle resizing
//-----------------------------------------------------------------------------
void HTML::OnSizeChanged( int wide,int tall )
{
	BaseClass::OnSizeChanged(wide,tall);
	UpdateSizeAndScrollBars();

	BrowserResize();
}


//-----------------------------------------------------------------------------
// Purpose: Run javascript in the page
//-----------------------------------------------------------------------------
void HTML::RunJavascript( const char *pchScript )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->ExecuteJavascript( m_unBrowserHandle, pchScript );
}


//-----------------------------------------------------------------------------
// Purpose: helper to convert UI mouse codes to CEF ones
//-----------------------------------------------------------------------------
int HTML::ConvertMouseCodeToCEFCode( MouseCode code )
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
// Purpose: passes mouse clicks to the control
//-----------------------------------------------------------------------------
void HTML::OnMousePressed( MouseCode code )
{
	m_sDragURL = NULL;

	// mouse4 = back button
	if ( code == MOUSE_4 )
	{
        PostActionSignal( new KeyValues( "HTMLBackRequested" ) );
		return;
	}
	if ( code == MOUSE_5 )
	{
        PostActionSignal( new KeyValues( "HTMLForwardRequested" ) );
		return;
	}


	if ( code == MOUSE_RIGHT && m_bContextMenuEnabled )
	{
// Disable right click menu until it looks better/works!
// 		GetLinkAtPosition( m_iMouseX, m_iMouseY );
// 		Menu::PlaceContextMenu( this, m_pContextMenu );
// 		return;
	}

	// ask for the focus to come to this window
	RequestFocus();

	// now tell the browser about the click
	// ignore right clicks if context menu has been disabled
	if ( code != MOUSE_RIGHT )
	{
		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		m_SteamAPIContext.SteamHTMLSurface()->MouseDown( m_unBrowserHandle, (ISteamHTMLSurface::EHTMLMouseButton)ConvertMouseCodeToCEFCode( code ) );
	}

	if ( code == MOUSE_LEFT )
	{
		input()->GetCursorPos( m_iDragStartX, m_iDragStartY );
		int htmlx, htmly;
		ipanel()->GetAbsPos( GetVPanel(), htmlx, htmly );

		GetLinkAtPosition( m_iDragStartX - htmlx, m_iDragStartY - htmly );

		m_bRequestingDragURL = true;
		// make sure we get notified when the mouse gets released
		if ( !m_sDragURL.IsEmpty() )
		{
			input()->SetMouseCapture( GetVPanel() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: passes mouse up events
//-----------------------------------------------------------------------------
void HTML::OnMouseReleased( MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		input()->SetMouseCapture( NULL );
		input()->SetCursorOveride( 0 );

		if ( !m_sDragURL.IsEmpty() && input()->GetMouseOver() != GetVPanel() && input()->GetMouseOver() != NULL )
		{
			// post the text as a drag drop to the target panel
			KeyValuesAD kv( "DragDrop" );
			if ( ipanel()->RequestInfo( input()->GetMouseOver(), kv )
				&& kv->GetPtr( "AcceptPanel" ) != NULL )
			{
				VPANEL vpanel = (VPANEL)kv->GetPtr( "AcceptPanel" );
				ivgui()->PostMessage( vpanel, new KeyValues( "DragDrop", "text", m_sDragURL.Get() ), GetVPanel() );
			}
		}
		m_sDragURL = NULL;
	}

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->MouseUp( m_unBrowserHandle, (ISteamHTMLSurface::EHTMLMouseButton)ConvertMouseCodeToCEFCode( code ) );
}


//-----------------------------------------------------------------------------
// Purpose: keeps track of where the cursor is
//-----------------------------------------------------------------------------
void HTML::OnCursorMoved(int x,int y)
{
	// Only do this when we are over the current panel
	if ( vgui::input()->GetMouseOver() == GetVPanel() )
	{
		m_iMouseX = x;
		m_iMouseY = y;

		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		m_SteamAPIContext.SteamHTMLSurface()->MouseMove( m_unBrowserHandle, m_iMouseX, m_iMouseY );
	}
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
}


//-----------------------------------------------------------------------------
// Purpose: passes double click events to the browser
//-----------------------------------------------------------------------------
void HTML::OnMouseDoublePressed( MouseCode code )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->MouseDoubleClick( m_unBrowserHandle, (ISteamHTMLSurface::EHTMLMouseButton)ConvertMouseCodeToCEFCode( code ) );
}

//-----------------------------------------------------------------------------
// Purpose: return the bitmask of any modifier keys that are currently down
//-----------------------------------------------------------------------------
int HTML::TranslateKeyModifiers()
{
	bool bControl = false;
	bool bAlt = false;
	bool bShift = false;

	if ( vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL ) )
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

	return (ISteamHTMLSurface::EHTMLKeyModifiers)nModifierCodes;
}

//-----------------------------------------------------------------------------
// Purpose: passes key presses to the browser (we don't current do this)
//-----------------------------------------------------------------------------
void HTML::OnKeyTyped(wchar_t unichar)
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->KeyChar( m_unBrowserHandle, unichar, (ISteamHTMLSurface::EHTMLKeyModifiers)TranslateKeyModifiers() );
}


//-----------------------------------------------------------------------------
// Purpose: pop up the find dialog
//-----------------------------------------------------------------------------
void HTML::ShowFindDialog()
{
	IScheme *pClientScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );
	if ( !pClientScheme )
		return;

	m_pFindBar->SetVisible( true );
	m_pFindBar->RequestFocus();
	m_pFindBar->SetText( "" );
	m_pFindBar->HideCountLabel();
	m_pFindBar->SetHidden( false );
	int x = 0, y = 0, h = 0, w = 0;
	m_pFindBar->GetBounds( x, y, w, h );
	m_pFindBar->SetPos( x, -1*h );
	int iSearchInsetY = 0;
	const char *resourceString = pClientScheme->GetResourceString( "HTML.SearchInsetY");
	if ( resourceString )
	{
		iSearchInsetY = atoi(resourceString);
	}
	float flAnimationTime = 0.0f;
	resourceString = pClientScheme->GetResourceString( "HTML.SearchAnimationTime");
	if ( resourceString )
	{
		flAnimationTime = atof(resourceString);
	}

	GetAnimationController()->RunAnimationCommand( m_pFindBar, "ypos", iSearchInsetY, 0.0f, flAnimationTime, AnimationController::INTERPOLATOR_LINEAR );
}


//-----------------------------------------------------------------------------
// Purpose: hide the find dialog
//-----------------------------------------------------------------------------
void HTML::HideFindDialog()
{
	IScheme *pClientScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );
	if ( !pClientScheme )
		return;

	int x = 0, y = 0, h = 0, w = 0;
	m_pFindBar->GetBounds( x, y, w, h );
	float flAnimationTime = 0.0f;
	const char *resourceString = pClientScheme->GetResourceString( "HTML.SearchAnimationTime");
	if ( resourceString )
	{
		flAnimationTime = atof(resourceString);
	}

	GetAnimationController()->RunAnimationCommand( m_pFindBar, "ypos", -1*h-5, 0.0f, flAnimationTime, AnimationController::INTERPOLATOR_LINEAR );
	m_pFindBar->SetHidden( true );
	StopFind();
}


//-----------------------------------------------------------------------------
// Purpose: is the find dialog visible?
//-----------------------------------------------------------------------------
bool HTML::FindDialogVisible()
{
	return m_pFindBar->IsVisible() && !m_pFindBar->BIsHidden();
}


//-----------------------------------------------------------------------------
// Purpose: return the bitmask of any modifier keys that are currently down
//-----------------------------------------------------------------------------
int GetKeyModifiers()
{
	// Any time a key is pressed reset modifier list as well
	int nModifierCodes = 0;
	if( vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL ) )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_CtrlDown;

	if( vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT ) )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_AltDown;

	if( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_ShiftDown;

#ifdef OSX
	// for now pipe through the cmd-key to be like the control key so we get copy/paste
	if( vgui::input()->IsKeyDown( KEY_LWIN ) || vgui::input()->IsKeyDown( KEY_RWIN ) )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_CtrlDown;
#endif

	return nModifierCodes;
}


//-----------------------------------------------------------------------------
// Purpose: passes key presses to the browser 
//-----------------------------------------------------------------------------
void HTML::OnKeyCodeTyped(KeyCode code)
{
	switch( code )
	{
	case KEY_PAGEDOWN:
		{
		int val = _vbar->GetValue();
		val += 200;
		_vbar->SetValue(val);
		break;
		}
	case KEY_PAGEUP:
		{
		int val = _vbar->GetValue();
		val -= 200;
		_vbar->SetValue(val);
		break;	
		}
	case KEY_F5:
		{
		Refresh();
		break;
		}
	case KEY_F:
		{
			if ( (input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL) )
				|| ( IsOSX() && ( input()->IsKeyDown(KEY_LWIN) || input()->IsKeyDown(KEY_RWIN) ) ) )
			{
				if ( !FindDialogVisible() )
				{
					ShowFindDialog();
				}
				else
				{
					HideFindDialog();
				}
				break;
			}
		}
	case KEY_ESCAPE:
		{
			if ( FindDialogVisible() )
			{
				HideFindDialog();
				break;
			}
		}
	case KEY_TAB:
		{
			if ( input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL) )
			{
				// pass control-tab to parent (through baseclass)
				BaseClass::OnKeyTyped( code );
				return;
			}
			break;
		}
	}

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->KeyDown( m_unBrowserHandle, KeyCode_VGUIToVirtualKey( code ), (ISteamHTMLSurface::EHTMLKeyModifiers)GetKeyModifiers() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void HTML::OnKeyCodeReleased(KeyCode code)
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->KeyUp( m_unBrowserHandle, KeyCode_VGUIToVirtualKey( code ), (ISteamHTMLSurface::EHTMLKeyModifiers)GetKeyModifiers() );
}


//-----------------------------------------------------------------------------
// Purpose: scrolls the vertical scroll bar on a web page
//-----------------------------------------------------------------------------
void HTML::OnMouseWheeled(int delta)
{	
	if ( _vbar )
	{
		int val = _vbar->GetValue();
		val -= (delta * 100.0/3.0 ); // 100 for every 3 lines matches chromes code
		_vbar->SetValue(val);
	}

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->MouseWheel( m_unBrowserHandle, delta );
}


//-----------------------------------------------------------------------------
// Purpose: Inserts a custom URL handler
//-----------------------------------------------------------------------------
void HTML::AddCustomURLHandler(const char *customProtocolName, vgui::Panel *target)
{
	int index = m_CustomURLHandlers.AddToTail();
	m_CustomURLHandlers[index].hPanel = target;
	Q_strncpy(m_CustomURLHandlers[index].url, customProtocolName, sizeof(m_CustomURLHandlers[index].url));
}


//-----------------------------------------------------------------------------
// Purpose: shared code for sizing the HTML surface window
//-----------------------------------------------------------------------------
void HTML::BrowserResize()
{
	int w,h;
	GetSize( w, h );
	int right = 0, bottom = 0;
	// TODO::STYLE
	/*
	IAppearance *pAppearance = GetAppearance();
	int left = 0, top = 0;
	if ( pAppearance )
	{
		pAppearance->GetInset( left, top, right, bottom );
	}
	*/

	if ( ( m_unBrowserHandle != INVALID_HTMLBROWSER ) && ( m_iWideLastHTMLSize != ( w - m_iScrollBorderX - right ) || m_iTalLastHTMLSize != ( h - m_iScrollBorderY - bottom ) ) )
	{
		m_iWideLastHTMLSize = w - m_iScrollBorderX - right;
		m_iTalLastHTMLSize = h - m_iScrollBorderY - bottom;
		if ( m_iTalLastHTMLSize <= 0 )
		{
			SetTall( 64 );
			m_iTalLastHTMLSize = 64 - bottom;
		}

		m_SteamAPIContext.SteamHTMLSurface()->SetSize( m_unBrowserHandle, m_iWideLastHTMLSize, m_iTalLastHTMLSize );

		// webkit forgets the scroll offset when you resize (it saves the scroll in a DC and a resize throws away the DC)
		// so just tell it after the resize
		int scrollV = _vbar->GetValue();
		int scrollH = _hbar->GetValue();

		m_SteamAPIContext.SteamHTMLSurface()->SetHorizontalScroll( m_unBrowserHandle, scrollH );
		m_SteamAPIContext.SteamHTMLSurface()->SetVerticalScroll( m_unBrowserHandle, scrollV );
	}

}


//-----------------------------------------------------------------------------
// Purpose: when a slider moves causes the IE images to re-render itself
//-----------------------------------------------------------------------------
void HTML::OnSliderMoved()
{
	if(_hbar->IsVisible())
	{
		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		int scrollX =_hbar->GetValue();
		m_SteamAPIContext.SteamHTMLSurface()->SetHorizontalScroll( m_unBrowserHandle, scrollX );
	}

	if(_vbar->IsVisible())
	{
		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		int scrollY=_vbar->GetValue();
		m_SteamAPIContext.SteamHTMLSurface()->SetVerticalScroll( m_unBrowserHandle, scrollY );
	}
	
	// post a message that the slider has moved
	PostActionSignal( new KeyValues( "HTMLSliderMoved" ) );
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool HTML::IsScrolledToBottom()
{
	if ( !_vbar->IsVisible() )
		return true;

	return m_scrollVertical.m_nScroll >= m_scrollVertical.m_nMax;
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool HTML::IsScrollbarVisible()
{
	return _vbar->IsVisible();
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void HTML::SetScrollbarsEnabled(bool state)
{
	m_bScrollBarEnabled = state;
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void HTML::SetContextMenuEnabled(bool state)
{
	m_bContextMenuEnabled = state;
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void HTML::SetViewSourceEnabled(bool state)
{
	m_pContextMenu->SetItemVisible( m_nViewSourceAllowedIndex, state );
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void HTML::NewWindowsOnly( bool state )
{
	m_bNewWindowsOnly = state;
}


//-----------------------------------------------------------------------------
// Purpose: called when our children have finished painting
//-----------------------------------------------------------------------------
void HTML::PostChildPaint()
{
	BaseClass::PostChildPaint();
	// TODO::STYLE
	//m_pInteriorPanel->SetPaintAppearanceEnabled( true ); // turn painting back on so the IE hwnd can render this border
}


//-----------------------------------------------------------------------------
// Purpose: Adds a custom header to all requests
//-----------------------------------------------------------------------------
void HTML::AddHeader( const char *pchHeader, const char *pchValue )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->AddHeader( m_unBrowserHandle, pchHeader, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void HTML::OnSetFocus()
{
	BaseClass::OnSetFocus();

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->SetKeyFocus( m_unBrowserHandle, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void HTML::OnKillFocus()
{
	BaseClass::OnKillFocus();

	// Don't clear the actual html focus if a context menu is what took focus
	if ( m_pContextMenu->HasFocus() )
		return;

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->SetKeyFocus( m_unBrowserHandle, false );
}


//-----------------------------------------------------------------------------
// Purpose: webkit is telling us to use this cursor type
//-----------------------------------------------------------------------------
void HTML::OnCommand( const char *pchCommand )
{
	if ( !Q_stricmp( pchCommand, "back" ) )
	{
		PostActionSignal( new KeyValues( "HTMLBackRequested" ) );
	}
	else if ( !Q_stricmp( pchCommand, "forward" ) )
	{
		PostActionSignal( new KeyValues( "HTMLForwardRequested" ) );
	}
	else if ( !Q_stricmp( pchCommand, "reload" ) )
	{
		Refresh();
	}
	else if ( !Q_stricmp( pchCommand, "stop" ) )
	{
		StopLoading();
	}
	else if ( !Q_stricmp( pchCommand, "viewsource" ) )
	{
		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		m_SteamAPIContext.SteamHTMLSurface()->ViewSource( m_unBrowserHandle );
	}
	else if ( !Q_stricmp( pchCommand, "copy" ) )
	{
		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		m_SteamAPIContext.SteamHTMLSurface()->CopyToClipboard( m_unBrowserHandle );
	}
	else if ( !Q_stricmp( pchCommand, "paste" ) )
	{
		if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
			return;

		m_SteamAPIContext.SteamHTMLSurface()->PasteFromClipboard( m_unBrowserHandle );
	}
	else if ( !Q_stricmp( pchCommand, "copyurl" ) )
	{
		system()->SetClipboardText( m_sCurrentURL, m_sCurrentURL.Length() );
	}
	else if ( !Q_stricmp( pchCommand, "copylink" ) )
	{
		int x, y;
		m_pContextMenu->GetPos( x, y );
		int htmlx, htmly;
		ipanel()->GetAbsPos( GetVPanel(), htmlx, htmly );

		m_bRequestingCopyLink = true;
		GetLinkAtPosition( x - htmlx, y - htmly );
	}
	else
		BaseClass::OnCommand( pchCommand );

}


//-----------------------------------------------------------------------------
// Purpose: the control wants us to ask the user what file to load
//-----------------------------------------------------------------------------
void HTML::OnFileSelected( const char *pchSelectedFile )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->FileLoadDialogResponse( m_unBrowserHandle, &pchSelectedFile );
	m_hFileOpenDialog->Close();
}

//-----------------------------------------------------------------------------
// Purpose: called when the user dismissed the file dialog with no selection
//-----------------------------------------------------------------------------
void HTML::OnFileSelectionCancelled()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->FileLoadDialogResponse( m_unBrowserHandle, NULL );
	m_hFileOpenDialog->Close();
}

//-----------------------------------------------------------------------------
// Purpose: find any text on the html page with this sub string
//-----------------------------------------------------------------------------
void HTML::Find( const char *pchSubStr )
{
	m_bInFind = false;
	if ( m_sLastSearchString == pchSubStr ) // same string as last time, lets fine next
		m_bInFind = true;

	m_sLastSearchString = pchSubStr;

	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->Find( m_unBrowserHandle, pchSubStr, m_bInFind, false );
}


//-----------------------------------------------------------------------------
// Purpose: find any text on the html page with this sub string
//-----------------------------------------------------------------------------
void HTML::FindPrevious()
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->Find( m_unBrowserHandle, m_sLastSearchString, m_bInFind, true );
}


//-----------------------------------------------------------------------------
// Purpose: find any text on the html page with this sub string
//-----------------------------------------------------------------------------
void HTML::FindNext()
{
	Find( m_sLastSearchString );
}


//-----------------------------------------------------------------------------
// Purpose: stop an outstanding find request
//-----------------------------------------------------------------------------
void HTML::StopFind( )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->StopFind( m_unBrowserHandle );
	m_bInFind = false;
}


//-----------------------------------------------------------------------------
// Purpose: input handler
//-----------------------------------------------------------------------------
void HTML::OnEditNewLine( Panel *pPanel )
{
	OnTextChanged( pPanel );
}


//-----------------------h------------------------------------------------------
// Purpose: input handler
//-----------------------------------------------------------------------------
void HTML::OnTextChanged( Panel *pPanel )
{
	char rgchText[2048];
	m_pFindBar->GetText( rgchText, sizeof( rgchText ) );
	Find( rgchText );
}


//-----------------------------------------------------------------------------
// Purpose: helper class for the find bar
//-----------------------------------------------------------------------------
HTML::CHTMLFindBar::CHTMLFindBar( HTML *parent ) : EditablePanel( parent, "FindBar" )
{
	m_pParent = parent;
	m_bHidden = false;
	m_pFindBar = new TextEntry( this, "FindEntry" );
	m_pFindBar->AddActionSignalTarget( parent );
	m_pFindBar->SendNewLine( true );
	m_pFindCountLabel = new Label( this, "FindCount", "" );
	m_pFindCountLabel->SetVisible( false );
	LoadControlSettings( "resource/layout/htmlfindbar.layout" );
}


//-----------------------------------------------------------------------------
// Purpose: button input into the find bar
//-----------------------------------------------------------------------------
void HTML::CHTMLFindBar::OnCommand( const char *pchCmd )
{
	if ( !Q_stricmp( pchCmd, "close" ) )
	{
		m_pParent->HideFindDialog();
	}
	else if ( !Q_stricmp( pchCmd, "previous" ) )
	{
		m_pParent->FindPrevious();
	}
	else if ( !Q_stricmp( pchCmd, "next" ) )
	{
		m_pParent->FindNext();
	}
	else
		BaseClass::OnCommand( pchCmd );

}

//-----------------------------------------------------------------------------
// Purpose: browser is fully created and ready to use
//-----------------------------------------------------------------------------
void HTML::OnBrowserReady( HTML_BrowserReady_t *pBrowserReady, bool bIOFailure )
{
	m_unBrowserHandle = pBrowserReady->unBrowserHandle;

	BrowserResize();

	// Only post the pending URL if we have set our OAuth token
	if ( !m_sPendingURLLoad.IsEmpty() )
	{
		PostURL( m_sPendingURLLoad, m_sPendingPostData );
		m_sPendingURLLoad.Clear();
	}

	// Remove browser corresponding to BrowserPopupHTMLWindow (cf BrowserPopupHTMLWindow)
	if ( m_SteamAPIContext.SteamHTMLSurface() && ( m_unBrowserHandleToDelete != INVALID_HTMLBROWSER ) )
	{
		Assert( m_unBrowserHandleToDelete != m_unBrowserHandle );
		m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser( m_unBrowserHandleToDelete );
		m_unBrowserHandleToDelete = INVALID_HTMLBROWSER;
	}
}

//-----------------------------------------------------------------------------
// Purpose: we have a new texture to update
//-----------------------------------------------------------------------------
void HTML::BrowserNeedsPaint( HTML_NeedsPaint_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	if ( !pCmd->pBGRA ) 
		return;

	int tw = 0, tt = 0;
	if ( m_iHTMLTextureID != 0 )
	{
		tw = m_allocedTextureWidth;
		tt = m_allocedTextureHeight;
	}

	// update the vgui texture
	if ( m_iHTMLTextureID == 0  || tw != (int)pCmd->unWide || tt != (int)pCmd->unTall )
	{
		if ( m_iHTMLTextureID != 0 )
			surface()->DeleteTextureByID( m_iHTMLTextureID );

		// if the dimensions changed we also need to re-create the texture ID to support the overlay properly (it won't resize a texture on the fly, this is the only control that needs
		//   to so lets have a tiny bit more code here to support that)
		m_iHTMLTextureID = surface()->CreateNewTextureID( true );
		surface()->DrawSetTextureRGBAEx( m_iHTMLTextureID, (const unsigned char *)pCmd->pBGRA, pCmd->unWide, pCmd->unTall, IMAGE_FORMAT_BGRA8888 );// BR FIXME - this call seems to shift by some number of pixels?
		m_allocedTextureWidth = pCmd->unWide;
		m_allocedTextureHeight = pCmd->unTall;
	}
	else if ( (int)pCmd->unUpdateWide > 0 && (int)pCmd->unUpdateTall > 0 )
	{
		// same size texture, just bits changing in it, lets twiddle
		surface()->DrawUpdateRegionTextureRGBA( m_iHTMLTextureID, pCmd->unUpdateX, pCmd->unUpdateY, (const unsigned char *)pCmd->pBGRA, pCmd->unUpdateWide, pCmd->unUpdateTall, IMAGE_FORMAT_BGRA8888 );
	}
	else
	{
		surface()->DrawSetTextureRGBAEx( m_iHTMLTextureID, (const unsigned char *)pCmd->pBGRA, pCmd->unWide, pCmd->unTall, IMAGE_FORMAT_BGRA8888 );
	}

	// need a paint next time
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: browser wants to start loading this url, do we let it?
//-----------------------------------------------------------------------------
bool HTML::OnStartRequest( const char *url, const char *target, const char *pchPostData, bool bIsRedirect )
{
	if ( !url || !Q_stricmp( url, "about:blank") )
		return true ; // this is just webkit loading a new frames contents inside an existing page

	HideFindDialog();
	// see if we have a custom handler for this
	bool bURLHandled = false;
	for (int i = 0; i < m_CustomURLHandlers.Count(); i++)
	{
		if (!Q_strnicmp(m_CustomURLHandlers[i].url,url, Q_strlen(m_CustomURLHandlers[i].url)))
		{
			// we have a custom handler
			Panel *targetPanel = m_CustomURLHandlers[i].hPanel;
			if (targetPanel)
			{
				PostMessage(targetPanel, new KeyValues("CustomURL", "url", m_CustomURLHandlers[i].url ) );
			}

			bURLHandled = true;
		}
	}

	if (bURLHandled)
		return false;

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

	if ( target && !Q_strlen( target ) )
	{
		m_sCurrentURL = url;

		KeyValues *pMessage = new KeyValues( "OnURLChanged" );
		pMessage->SetString( "url", url );
		pMessage->SetString( "postdata", pchPostData );
		pMessage->SetInt( "isredirect", bIsRedirect ? 1 : 0 );

		PostActionSignal( pMessage );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: callback from cef thread, load a url please
//-----------------------------------------------------------------------------
void HTML::BrowserStartRequest( HTML_StartRequest_t *pCmd )
{
	// In the case of a popup, CEF will create a new browser handle that we will manually remove
	// (cf HTML::BrowserPopupHTMLWindow). However we must still reply to the HTML_StartRequest_t callback
	// in order not to hang the browser.
	if ( m_unBrowserHandleToDelete && ( pCmd->unBrowserHandle == m_unBrowserHandleToDelete ) )
	{
		m_SteamAPIContext.SteamHTMLSurface()->AllowStartRequest( pCmd->unBrowserHandle, false );
	}
	else if ( pCmd->unBrowserHandle == m_unBrowserHandle )
	{
		bool bRes = OnStartRequest( pCmd->pchURL, pCmd->pchTarget, pCmd->pchPostData, pCmd->bIsRedirect );

		m_SteamAPIContext.SteamHTMLSurface()->AllowStartRequest( m_unBrowserHandle, bRes );
	}
}


//-----------------------------------------------------------------------------
// Purpose: browser went to a new url
//-----------------------------------------------------------------------------
void HTML::BrowserURLChanged( HTML_URLChanged_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

 	m_sCurrentURL = pCmd->pchURL;
 
	KeyValues *pMessage = new KeyValues( "OnURLChanged" );
	pMessage->SetString( "url", pCmd->pchURL );
	pMessage->SetString( "postdata", pCmd->pchPostData );
	pMessage->SetInt( "isredirect", pCmd->bIsRedirect ? 1 : 0 );

	PostActionSignal( pMessage );

	OnURLChanged( m_sCurrentURL, pCmd->pchPostData, pCmd->bIsRedirect );
}


//-----------------------------------------------------------------------------
// Purpose: finished loading this page
//-----------------------------------------------------------------------------
void HTML::BrowserFinishedRequest( HTML_FinishedRequest_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	PostActionSignal( new KeyValues( "OnFinishRequest", "url", pCmd->pchURL ) );
	if ( strlen( pCmd->pchPageTitle ) )
		PostActionSignal( new KeyValues( "PageTitleChange", "title", pCmd->pchPageTitle ) );
	KeyValues *pKVSecure = new KeyValues( "SecurityStatus" );
	pKVSecure->SetString( "url", pCmd->pchURL );

 	OnFinishRequest( pCmd->pchURL, pCmd->pchPageTitle );
}


//-----------------------------------------------------------------------------
// Purpose: display a new html window 
//-----------------------------------------------------------------------------
void HTML::BrowserPopupHTMLWindow( HTML_NewWindow_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	// Do not open a popup from a popup window
	if (m_bPopupWindow)
	{
		if (m_SteamAPIContext.SteamHTMLSurface())
		{
			Assert( pCmd->unNewWindow_BrowserHandle != m_unBrowserHandle );
			m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser( pCmd->unNewWindow_BrowserHandle );
		}

		return;
	} 

	// Allow more than one popup to be active for now. For some reason, we are not getting HTML_NewWindow_t
	// messages after trying to have more than one popup active. (Suspect that the browser is not getting destroyed on the Steam side,
	// even though we request it via m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser( pCmd->unNewWindow_BrowserHandle )
	/*
	if ( gMOTDPopupWindowActive )
	{
		// Only allow one popup to be active at any time
		// Destroy the browser created by steamapi/cef

		Msg( "Only one popup active at any time. Dropping request to open a new window for %s\n", pCmd->pchURL );

		if ( m_SteamAPIContext.SteamHTMLSurface() )
		{
			m_SteamAPIContext.SteamHTMLSurface()->RemoveBrowser( pCmd->unNewWindow_BrowserHandle );
		}
		return;
	}
	*/

	// HACK - Do not pass the browser handle created by CEF
	// For some reason, we are not getting the paint messages anymore (might be related
	// to some changes to htmlsurface.cpp (steam code)
	// As a workaround, we are killing the supplied popup browser and create a new one
	// Defer killing the supllied popup browser until the new one is fully created (HTML_BrowserReady_t
	// message received) to work around a timing issue 
	// Note that we must also send a response to the HTML_StartRequest_t callback (with the browser handle 
	// created by CEF). If we do not reply to the callback, the browser may appear to hang (for 20 seconds)
	// instead of navigating to the new page in the popup
	
	// Create popup
	// pCmd->unNewWindow_BrowserHandle getting removed once HTML_BrowserReady_t message received

	HTMLPopup *p = new HTMLPopup( this, pCmd->unNewWindow_BrowserHandle, pCmd->pchURL, "" );
	int wide = pCmd->unWide;
	int tall = pCmd->unTall;
	if ( wide == 0 || tall == 0 )
	{
		wide = MAX( 640, GetWide() );
		tall = MAX( 480, GetTall() );
	}

	p->SetBounds( pCmd->unX, pCmd->unY, wide, tall );
	p->SetDeleteSelfOnClose( true );
	if ( pCmd->unX == 0 || pCmd->unY == 0 )
		p->MoveToCenterOfScreen();
	p->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: browser telling us to use this cursor
//-----------------------------------------------------------------------------
void HTML::BrowserSetCursor( HTML_SetCursor_t *pCmd )
{
	// GS - Mouse cursor value in CMsgSetCursor is set to one of EMouseCursor,
	// by CChromePainter::OnSetCursor in html_chrome.cpp
	// Code below relies on start of EMouseCursor being exactly same as vgui::CursorCode  
	
	vgui::CursorCode cursor;
	uint32 msgCursor = pCmd->eMouseCursor;

	if ( msgCursor >= (uint32)(dc_last) )
	{
		cursor = dc_arrow;
	}
	else
	{
		cursor = (CursorCode)msgCursor;
	}
	
	SetCursor( cursor );
}


//-----------------------------------------------------------------------------
// Purpose: browser telling to show the file loading dialog
//-----------------------------------------------------------------------------
void HTML::BrowserFileOpenDialog( HTML_FileOpenDialog_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	if ( m_hFileOpenDialog.Get() )
	{
		delete m_hFileOpenDialog.Get();
		m_hFileOpenDialog = NULL;
	}
	m_hFileOpenDialog = new FileOpenDialog( this, pCmd->pchTitle, true );
	m_hFileOpenDialog->SetStartDirectory( pCmd->pchInitialFile );
	m_hFileOpenDialog->AddActionSignalTarget( this );
	m_hFileOpenDialog->SetAutoDelete( true );
	m_hFileOpenDialog->DoModal(false);
}


//-----------------------------------------------------------------------------
// Purpose: browser asking to show a tooltip
//-----------------------------------------------------------------------------
void HTML::BrowserShowToolTip( HTML_ShowToolTip_t *pCmd )
{
	/*
	BR FIXME
	Tooltip *tip = GetTooltip();
	tip->SetText( pCmd->text().c_str() );
	tip->SetTooltipFormatToMultiLine();
	tip->SetTooltipDelayMS( 250 );
	tip->SetMaxToolTipWidth( MAX( 200, GetWide()/2 ) );
	tip->ShowTooltip( this );
	*/
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us to update tool tip text
//-----------------------------------------------------------------------------
void HTML::BrowserUpdateToolTip( HTML_UpdateToolTip_t *pCmd )
{
//	GetTooltip()->SetText( pCmd->text().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: browser telling that it is done with the tip
//-----------------------------------------------------------------------------
void HTML::BrowserHideToolTip( HTML_HideToolTip_t *pCmd )
{
//	GetTooltip()->HideTooltip();
//	DeleteToolTip();
}


//-----------------------------------------------------------------------------
// Purpose: callback when performing a search
//-----------------------------------------------------------------------------
void HTML::BrowserSearchResults( HTML_SearchResults_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	if ( pCmd->unResults == 0 )
		m_pFindBar->HideCountLabel();
	else
		m_pFindBar->ShowCountLabel();

	if ( pCmd->unResults > 0 )
		m_pFindBar->SetDialogVariable( "findcount", (int)pCmd->unResults );
	if ( pCmd->unCurrentMatch > 0 )
		m_pFindBar->SetDialogVariable( "findactive", (int)pCmd->unCurrentMatch );
	m_pFindBar->InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us it had a close requested
//-----------------------------------------------------------------------------
void HTML::BrowserClose( HTML_CloseBrowser_t *pCmd )
{
 	CHECK_BROWSER_HANDLE();

	PostActionSignal( new KeyValues( "OnCloseWindow" ) );
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us the size of the horizontal scrollbars
//-----------------------------------------------------------------------------
void HTML::BrowserHorizontalScrollBarSizeResponse( HTML_HorizontalScroll_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	ScrollData_t scrollHorizontal;
	scrollHorizontal.m_nScroll = pCmd->unScrollCurrent;
	scrollHorizontal.m_nMax = pCmd->unScrollMax;
	scrollHorizontal.m_bVisible = pCmd->bVisible;
	scrollHorizontal.m_flZoom = pCmd->flPageScale;

	/* Add this block if webkit scrollbars removed
	int w, h;
	GetSize( w, h );

	scrollHorizontal.m_nY = h - (h / 100);
	scrollHorizontal.m_nX = pCmd->unScrollCurrent;

	scrollHorizontal.m_nWide = w;
	scrollHorizontal.m_nTall = h / 100;
	*/

	if ( scrollHorizontal != m_scrollHorizontal )
	{
		m_scrollHorizontal = scrollHorizontal;
		UpdateSizeAndScrollBars();
	}
	else
	{
		m_scrollHorizontal = scrollHorizontal;
	}
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us the size of the vertical scrollbars
//-----------------------------------------------------------------------------
void HTML::BrowserVerticalScrollBarSizeResponse( HTML_VerticalScroll_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	ScrollData_t scrollVertical;
	scrollVertical.m_nScroll = pCmd->unScrollCurrent;
	scrollVertical.m_nMax = pCmd->unScrollMax;
	scrollVertical.m_bVisible = pCmd->bVisible;
	scrollVertical.m_flZoom = pCmd->flPageScale;

	/* Add this block if webkit scrollbars removed
	int w, h;
	GetSize( w, h );

	scrollVertical.m_nY = pCmd->unScrollCurrent;
	scrollVertical.m_nX = w - (w / 100);

	scrollVertical.m_nWide = w / 100;
	scrollVertical.m_nTall = h;
	*/

	if ( scrollVertical != m_scrollVertical )
	{
		m_scrollVertical = scrollVertical;
		UpdateSizeAndScrollBars();
	}
	else
	{
		m_scrollVertical = scrollVertical;
	}
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us what is at this location on the page
//-----------------------------------------------------------------------------
void HTML::BrowserLinkAtPositionResponse( HTML_LinkAtPosition_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	m_LinkAtPos.m_sURL = pCmd->pchURL;
	m_LinkAtPos.m_nX = pCmd->x;
	m_LinkAtPos.m_nY = pCmd->y;

	m_pContextMenu->SetItemVisible( m_iCopyLinkMenuItemID, !m_LinkAtPos.m_sURL.IsEmpty() ? true : false );
	if ( m_bRequestingDragURL )
	{
		m_bRequestingDragURL = false;
		m_sDragURL = m_LinkAtPos.m_sURL;
		// make sure we get notified when the mouse gets released
		if ( !m_sDragURL.IsEmpty() )
		{
			input()->SetMouseCapture( GetVPanel() );
		}
	}

	if ( m_bRequestingCopyLink )
	{
		m_bRequestingCopyLink = false;
		if ( !m_LinkAtPos.m_sURL.IsEmpty() )
			system()->SetClipboardText( m_LinkAtPos.m_sURL, m_LinkAtPos.m_sURL.Length() );
		else
			system()->SetClipboardText( "", 1 );
	}

	OnLinkAtPosition( m_LinkAtPos.m_sURL );
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us to pop a javascript alert dialog
//-----------------------------------------------------------------------------
void HTML::BrowserJSAlert( HTML_JSAlert_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	MessageBox *pDlg = new MessageBox( m_sCurrentURL, (const char *)pCmd->pchMessage, this );
	pDlg->AddActionSignalTarget( this );
	pDlg->SetCommand( new KeyValues( "DismissJSDialog", "result", false ) );
	pDlg->DoModal();
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us to pop a js confirm dialog
//-----------------------------------------------------------------------------
void HTML::BrowserJSConfirm( HTML_JSConfirm_t *pCmd )
{
	CHECK_BROWSER_HANDLE();
	
	QueryBox *pDlg = new QueryBox( m_sCurrentURL, (const char *)pCmd->pchMessage, this );
	pDlg->AddActionSignalTarget( this );
	pDlg->SetOKCommand( new KeyValues( "DismissJSDialog", "result", true ) );
	pDlg->SetCancelCommand( new KeyValues( "DismissJSDialog", "result", false ) );
	pDlg->DoModal();
}


//-----------------------------------------------------------------------------
// Purpose: browser telling us the state of back and forward buttons
//-----------------------------------------------------------------------------
void HTML::BrowserCanGoBackandForward( HTML_CanGoBackAndForward_t *pCmd )
{
	CHECK_BROWSER_HANDLE();

	m_bCanGoBack = pCmd->bCanGoBack;
	m_bCanGoForward = pCmd->bCanGoForward;
}


//-----------------------------------------------------------------------------
// Purpose: ask the browser for what is at this x,y
//-----------------------------------------------------------------------------
void HTML::GetLinkAtPosition( int x, int y )
{
	if ( m_unBrowserHandle == INVALID_HTMLBROWSER )
		return;

	m_SteamAPIContext.SteamHTMLSurface()->GetLinkAtPosition( m_unBrowserHandle, x, y );
}


//-----------------------------------------------------------------------------
// Purpose: update the size of the browser itself and scrollbars it shows
//-----------------------------------------------------------------------------
void HTML::UpdateSizeAndScrollBars()
{
	// Tell IE
	BrowserResize();

	// Do this after we tell IE!
	int w,h;
	GetSize( w, h );
	CalcScrollBars(w,h);

	InvalidateLayout();
}

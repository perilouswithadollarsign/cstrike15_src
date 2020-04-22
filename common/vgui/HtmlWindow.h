//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef _HTMLWINDOW_H_
#define _HTMLWINDOW_H_
#ifdef _WIN32
#pragma once
#endif

#ifdef WIN32

#include <exdispid.h>
#include <olectl.h>
#include <exdisp.h>

#include <vgui/vgui.h>
#include <vgui/IHTML.h>
#include <vgui/IImage.h>

#if defined ( GAMEUI_EXPORTS )
#include <vgui/IPanel.h>
#include <vgui_controls/Controls.h>
#else
#include "VPanel.h"
#endif

#if defined ( GAMEUI_EXPORTS )
#include "../vgui2/src/Memorybitmap.h"
#else
#include "Memorybitmap.h"
#endif


struct IHTMLElement;

class HtmlWindow : public vgui::IHTML,  vgui::IHTMLEvents
{
public:
	HtmlWindow(vgui::IHTMLEvents *events, vgui::VPANEL c, HWND parent, bool AllowJavaScript, bool DirectToHWND);
	virtual ~HtmlWindow();

	virtual void OpenURL(const char *);
	virtual bool StopLoading();
	virtual bool Refresh();
	virtual void SetVisible(bool state);
	virtual bool Show( bool state );

	void CreateBrowser( bool AllowJavaScript );

	HWND GetHWND() { return m_parent; }
	HWND GetIEHWND() { return m_ieHWND;}
	HWND GetObjectHWND() { return m_oleObjectHWND;}
	vgui::IHTMLEvents *GetEvents() {return m_events;}

	//HDC OnPaint(HDC hDC,HBITMAP *bits);
	virtual void OnPaint(HDC hDC);
	virtual vgui::IImage *GetBitmap() { return m_Bitmap; }

	virtual void Obsolete_OnSize(int x,int y, int w,int h);
	virtual void ScrollHTML(int x,int y);

	virtual void OnMouse(vgui::MouseCode code,MOUSE_STATE s,int x,int y);
//	virtual void OnMouse2(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
	virtual void OnChar(wchar_t unichar);
	virtual void OnKeyDown(vgui::KeyCode code);
	virtual void AddText(const char *text);
	virtual void Clear();

	virtual bool OnMouseOver();

	void GetSize(int &wide,int &tall) {wide=w ; tall=h;}
	virtual void GetHTMLSize(int &wide,int &tall) {wide=html_w ; tall=html_h;}

	// events
	virtual bool OnStartURL(const char *url, const char *target, bool first);
	virtual void OnFinishURL(const char *url);
	virtual void OnProgressURL(long current, long maximum);
	virtual void OnSetStatusText(const char *text);
	virtual void OnUpdate();
	virtual void OnLink();
	virtual void OffLink();


	const char *GetOpenedPage() { return m_currentUrl; }
	RECT SetBounds();

	int textureID; // used by the engine code

	void NewWindow(IDispatch **pIDispatch);
	void CalculateHTMLSize( void *pVoid );

	HHOOK GetHook() { return m_hHook; }
	void *GetIEWndProc() { return (void *)m_hIEWndProc; }
	UINT GetMousePos() { return m_iMousePos;}
	UINT GetMouseMessage() { return mouse_msg;}
	bool IsVisible() { return m_bVisible; }

	virtual void Obsolete_OnMouse(vgui::MouseCode code,MOUSE_STATE s,int x,int y) {}
	virtual void Obsolete_OnChar(wchar_t unichar) {}
	virtual void Obsolete_OnKeyDown(vgui::KeyCode code) {}

private:

	// support functions
	bool CheckIsLink(IHTMLElement *el, char *type);


	char m_currentUrl[512]; // the currently opened URL
	bool m_specificallyOpened;

	long w,h; // viewport size
	long window_x, window_y;
	long html_w,html_h; // html page size
	long html_x,html_y; // html page offset

	// active X container objects
	IConnectionPoint * m_connectionPoint;
	IWebBrowser2 * m_webBrowser;
	IOleObject * m_oleObject;
	IOleInPlaceObject * m_oleInPlaceObject;
	IViewObject * m_viewObject;

	HWND m_parent;  // the HWND of the vgui parent
	HWND m_oleObjectHWND; // the oleObjects window (which is inside the main window)
	HWND m_ieHWND; // the handle to the IE browser itself, which is inside oleObject
	vgui::VPANEL m_vcontext; // the vpanel of our frame, used to find out the abs pos of the panel

	
	vgui::IHTMLEvents *m_events; // place to send events to

	// state vars for OnPaint()
	HDC hdcMem;
	HDC lasthDC;
	HBITMAP hBitmap;


	vgui::MemoryBitmap *m_Bitmap; // the vgui image that the page is rendered into

	bool m_cleared,m_newpage; // 
	bool m_bDirectToHWND;

	void *m_fs; // a pointer to the framesite object for this control

	DWORD m_adviseCookie; // cookies (magic numbers) used to hook into ActiveX events
	DWORD m_HtmlEventsAdviseCookie;


	HHOOK m_hHook; // hook for window events
	bool m_bHooked; // whether a hook has been installed yet
	LPARAM m_iMousePos; // the lParam value for the last mouse movement on the object, used in GetMsgProc()
	UINT mouse_msg; // used to enumerate mouse functions to child windows, see EnunChildProc()
	bool m_bVisible;
	long m_hIEWndProc;
};

#endif // _WIN32
#endif // _HTMLWINDOW_H_ 

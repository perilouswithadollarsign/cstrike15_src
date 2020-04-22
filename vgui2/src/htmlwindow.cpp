//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides an ActiveX control hosting environment for an Internet Explorer control.
//
//			Abandon all hope ye who enter here, thar be dragons... (in other words, hacks below)
//
// $NoKeywords: $
//=============================================================================//

#pragma warning( disable: 4310 ) // cast truncates constant value

#define _WIN32_WINNT 0x0502
#include <oleidl.h>
#include <winerror.h>
#include <comdef.h>
#include <assert.h>
#include <vgui/HtmlWindow.h> // in common/vgui/
#include <tier0/dbg.h>

#include <mshtml.h>  // for HtmlDocument2 defines
#include <mshtmdid.h> 
#include <mshtmhst.h>	// for IDocHostUIHandler

#include <vgui/ISurface.h> // for surface()
#include "vgui/iinputinternal.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"


#include <tchar.h> // _T() define

#include <stdio.h>
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef UNICODE
#define GetClassName  GetClassNameW
#else
#define GetClassName  GetClassNameA
#endif // !UNICODE

#ifdef _DEBUG
#ifdef DEBUG
#undef DEBUG
#endif
//#define DEBUG( x ) OutputDebugString( #x "\n" )
#define DEBUG( x )
#else
#define DEBUG( x )
#endif

#define ASSERT assert


//-----------------------------------------------------------------------------
// Purpose: 
//		Class definitions for the various OLE (ActiveX) containers needed.
//
//
//
//-----------------------------------------------------------------------------
class HtmlWindow;
class FS_IOleInPlaceFrame;
class FS_IOleInPlaceSiteWindowless;
class FS_IOleClientSite;
class FS_IOleControlSite;
class FS_IOleCommandTarget;
class FS_IOleItemContainer;
class FS_IDispatch;
class FS_DWebBrowserEvents2;
class FS_DHTMLDocumentEvents2;
class FS_IAdviseSink2;
class FS_IAdviseSinkEx;
class FS_IDocHostUIHandler;

class FrameSite : public IUnknown
{
	friend class HtmlWindow;
	friend class FS_IOleInPlaceFrame;
	friend class FS_IOleInPlaceSiteWindowless;
	friend class FS_IOleClientSite;
	friend class FS_IOleControlSite;
	friend class FS_IOleCommandTarget;
	friend class FS_IOleItemContainer;
	friend class FS_IDispatch;
	friend class FS_DWebBrowserEvents2;
	friend class FS_DHTMLDocumentEvents2;
	friend class FS_IAdviseSink2;
	friend class FS_IAdviseSinkEx;
	friend class FS_IDocHostUIHandler;

public:
	FrameSite(HtmlWindow * win, bool AllowJavaScript);
	~FrameSite();

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	FS_IAdviseSinkEx * m_IAdviseSinkEx;
	
protected:
	int m_cRef;

	FS_IOleInPlaceFrame * m_IOleInPlaceFrame;
	FS_IOleInPlaceSiteWindowless * m_IOleInPlaceSiteWindowless;
	FS_IOleClientSite * m_IOleClientSite;
	FS_IOleControlSite * m_IOleControlSite;
	FS_IOleCommandTarget * m_IOleCommandTarget;
	FS_IOleItemContainer * m_IOleItemContainer;
	FS_IDispatch * m_IDispatch;
	FS_DWebBrowserEvents2 * m_DWebBrowserEvents2;
	FS_DHTMLDocumentEvents2 * m_DHTMLDocumentEvents2;
	FS_IAdviseSink2 * m_IAdviseSink2;
	FS_IDocHostUIHandler *m_IDocHostUIHandler;
	HtmlWindow * m_window;

	HDC m_hDCBuffer;
	HWND m_hWndParent;

	bool m_bSupportsWindowlessActivation;
	bool m_bInPlaceLocked;
	bool m_bInPlaceActive;
	bool m_bUIActive;
	bool m_bWindowless;

	LCID m_nAmbientLocale;
	COLORREF m_clrAmbientForeColor;
	COLORREF m_clrAmbientBackColor;
	bool m_bAmbientShowHatching;
	bool m_bAmbientShowGrabHandles;
	bool m_bAmbientUserMode;
	bool m_bAmbientAppearance;
};

class FS_IOleInPlaceFrame : public IOleInPlaceFrame
{
public:
	FS_IOleInPlaceFrame(FrameSite* fs) { m_fs = fs; }
	~FS_IOleInPlaceFrame() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IOleWindow
	STDMETHODIMP GetWindow(HWND*);
	STDMETHODIMP ContextSensitiveHelp(BOOL);
	//IOleInPlaceUIWindow
	STDMETHODIMP GetBorder(LPRECT);
	STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS);
	STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS);
	STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR);
	//IOleInPlaceFrame
	STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS);
	STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND);
	STDMETHODIMP RemoveMenus(HMENU);
	STDMETHODIMP SetStatusText(LPCOLESTR);
	STDMETHODIMP EnableModeless(BOOL);
	STDMETHODIMP TranslateAccelerator(LPMSG, WORD);
protected:
	FrameSite * m_fs;
};


class FS_IOleInPlaceSiteWindowless : public IOleInPlaceSiteWindowless
{
public:
	FS_IOleInPlaceSiteWindowless(FrameSite* fs) { m_fs = fs; }
	~FS_IOleInPlaceSiteWindowless() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IOleWindow
	STDMETHODIMP GetWindow(HWND* h)
	{ return m_fs->m_IOleInPlaceFrame->GetWindow(h); }
	STDMETHODIMP ContextSensitiveHelp(BOOL b)
	{ return m_fs->m_IOleInPlaceFrame->ContextSensitiveHelp(b); }
	//IOleInPlaceSite
	STDMETHODIMP CanInPlaceActivate();
	STDMETHODIMP OnInPlaceActivate();
	STDMETHODIMP OnUIActivate();
	STDMETHODIMP GetWindowContext(IOleInPlaceFrame**, IOleInPlaceUIWindow**, 
		LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO);
	STDMETHODIMP Scroll(SIZE);
	STDMETHODIMP OnUIDeactivate(BOOL);
	STDMETHODIMP OnInPlaceDeactivate();
	STDMETHODIMP DiscardUndoState();
	STDMETHODIMP DeactivateAndUndo();
	STDMETHODIMP OnPosRectChange(LPCRECT);
	//IOleInPlaceSiteEx
	STDMETHODIMP OnInPlaceActivateEx(BOOL*, DWORD);
	STDMETHODIMP OnInPlaceDeactivateEx(BOOL);
	STDMETHODIMP RequestUIActivate();
	//IOleInPlaceSiteWindowless
	STDMETHODIMP CanWindowlessActivate();
	STDMETHODIMP GetCapture();
	STDMETHODIMP SetCapture(BOOL);
	STDMETHODIMP GetFocus();
	STDMETHODIMP SetFocus(BOOL);
	STDMETHODIMP GetDC(LPCRECT, DWORD, HDC*);
	STDMETHODIMP ReleaseDC(HDC);
	STDMETHODIMP InvalidateRect(LPCRECT, BOOL);
	STDMETHODIMP InvalidateRgn(HRGN, BOOL);
	STDMETHODIMP ScrollRect(INT, INT, LPCRECT, LPCRECT);
	STDMETHODIMP AdjustRect(LPRECT);
	STDMETHODIMP OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*);

protected:
	FrameSite * m_fs;
};

class FS_IOleClientSite : public IOleClientSite
{
public:
	FS_IOleClientSite(FrameSite* fs) { m_fs = fs; }
	~FS_IOleClientSite() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IOleClientSite
	STDMETHODIMP SaveObject();
	STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**);
	STDMETHODIMP GetContainer(LPOLECONTAINER FAR*);
	STDMETHODIMP ShowObject();
	STDMETHODIMP OnShowWindow(BOOL);
	STDMETHODIMP RequestNewObjectLayout();
protected:
	FrameSite * m_fs;
};

class FS_IOleControlSite : public IOleControlSite
{
public:
	FS_IOleControlSite(FrameSite* fs) { m_fs = fs; }
	~FS_IOleControlSite() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IOleControlSite
	STDMETHODIMP OnControlInfoChanged();
	STDMETHODIMP LockInPlaceActive(BOOL);
	STDMETHODIMP GetExtendedControl(IDispatch**);
	STDMETHODIMP TransformCoords(POINTL*, POINTF*, DWORD);
	STDMETHODIMP ShowContextMenu(DWORD dwID,POINT *ppt, IUnknown *pcmdtReserved, IDispatch *pdispReserved);
	STDMETHODIMP TranslateAccelerator(LPMSG, DWORD);
	STDMETHODIMP OnFocus(BOOL);
	STDMETHODIMP ShowPropertyFrame();

protected:
	FrameSite * m_fs;
};

class FS_IOleCommandTarget : public IOleCommandTarget
{
public:
	FS_IOleCommandTarget(FrameSite* fs) { m_fs = fs; }
	~FS_IOleCommandTarget() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IOleCommandTarget
	STDMETHODIMP QueryStatus(const GUID*, ULONG, OLECMD[], OLECMDTEXT*);
	STDMETHODIMP Exec(const GUID*, DWORD, DWORD, VARIANTARG*, VARIANTARG*);
protected:
	FrameSite * m_fs;
};

class FS_IOleItemContainer : public IOleItemContainer
{
public:
	FS_IOleItemContainer(FrameSite* fs) { m_fs = fs; }
	~FS_IOleItemContainer() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IParseDisplayName
	STDMETHODIMP ParseDisplayName(IBindCtx*, LPOLESTR, ULONG*, IMoniker**);
	//IOleContainer
	STDMETHODIMP EnumObjects(DWORD, IEnumUnknown**);
	STDMETHODIMP LockContainer(BOOL);
	//IOleItemContainer
	STDMETHODIMP GetObject(LPOLESTR, DWORD, IBindCtx*, REFIID, void**);
	STDMETHODIMP GetObjectStorage(LPOLESTR, IBindCtx*, REFIID, void**);
	STDMETHODIMP IsRunning(LPOLESTR);
protected:
	FrameSite * m_fs;
};

class FS_IDispatch : public IDispatch
{
public:
	FS_IDispatch(FrameSite* fs) { m_fs = fs; m_bNewURL=0; m_bOnLink=false; m_bAllowJavaScript = true;}
	~FS_IDispatch() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID, OLECHAR**, unsigned int, LCID, DISPID*);
	STDMETHODIMP GetTypeInfo(unsigned int, LCID, ITypeInfo**);
	STDMETHODIMP GetTypeInfoCount(unsigned int*);
	STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);

	void SetAllowJavaScript( bool state ) { m_bAllowJavaScript = state; }
protected:
	FrameSite * m_fs;
	unsigned int m_bNewURL;
	bool m_bAllowJavaScript;
	bool m_bOnLink;

};

class FS_DWebBrowserEvents2 : public DWebBrowserEvents2
{
public:
	FS_DWebBrowserEvents2(FrameSite* fs) { m_fs = fs; }
	~FS_DWebBrowserEvents2() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID r, OLECHAR** o, unsigned int i, LCID l, DISPID* d)
	{ return m_fs->m_IDispatch->GetIDsOfNames(r, o, i, l, d); }
	STDMETHODIMP GetTypeInfo(unsigned int i, LCID l, ITypeInfo** t)
	{ return m_fs->m_IDispatch->GetTypeInfo(i, l, t); }
	STDMETHODIMP GetTypeInfoCount(unsigned int* i)
	{ return m_fs->m_IDispatch->GetTypeInfoCount(i); }
	STDMETHODIMP Invoke(DISPID d, REFIID r, LCID l, WORD w, DISPPARAMS* dp, 
		VARIANT* v, EXCEPINFO* e, UINT* u)
	{ return m_fs->m_IDispatch->Invoke(d, r, l, w, dp, v, e, u); }
protected:
	FrameSite * m_fs;
};

class FS_DHTMLDocumentEvents2 : public HTMLDocumentEvents
{
public:
	FS_DHTMLDocumentEvents2(FrameSite* fs) { m_fs = fs; }
	~FS_DHTMLDocumentEvents2() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID r, OLECHAR** o, unsigned int i, LCID l, DISPID* d)
	{ return m_fs->m_IDispatch->GetIDsOfNames(r, o, i, l, d); }
	STDMETHODIMP GetTypeInfo(unsigned int i, LCID l, ITypeInfo** t)
	{ return m_fs->m_IDispatch->GetTypeInfo(i, l, t); }
	STDMETHODIMP GetTypeInfoCount(unsigned int* i)
	{ return m_fs->m_IDispatch->GetTypeInfoCount(i); }
	STDMETHODIMP Invoke(DISPID d, REFIID r, LCID l, WORD w, DISPPARAMS* dp, 
		VARIANT* v, EXCEPINFO* e, UINT* u)
	{ return m_fs->m_IDispatch->Invoke(d, r, l, w, dp, v, e, u); }
protected:
	FrameSite * m_fs;
};


class FS_IAdviseSink2 : public IAdviseSink2
{
public:
	FS_IAdviseSink2(FrameSite* fs) { m_fs = fs; }
	~FS_IAdviseSink2() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IAdviseSink
	void STDMETHODCALLTYPE OnDataChange(FORMATETC*, STGMEDIUM*);
	void STDMETHODCALLTYPE OnViewChange(DWORD, LONG);
	void STDMETHODCALLTYPE OnRename(IMoniker*);
	void STDMETHODCALLTYPE OnSave();
	void STDMETHODCALLTYPE OnClose();
	//IAdviseSink2
	void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker*);
protected:
	FrameSite * m_fs;
};

class FS_IAdviseSinkEx : public IAdviseSinkEx
{
public:
	FS_IAdviseSinkEx(FrameSite* fs) { m_fs = fs; }
	~FS_IAdviseSinkEx() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	//IAdviseSink
	void STDMETHODCALLTYPE OnDataChange(FORMATETC* f, STGMEDIUM* s)
	{ m_fs->m_IAdviseSink2->OnDataChange(f, s); }
	void STDMETHODCALLTYPE OnViewChange(DWORD d, LONG l)
	{ m_fs->m_IAdviseSink2->OnViewChange(d, l); }
	void STDMETHODCALLTYPE OnRename(IMoniker* i)
	{ m_fs->m_IAdviseSink2->OnRename(i); }
	void STDMETHODCALLTYPE OnSave()
	{ m_fs->m_IAdviseSink2->OnSave(); }
	void STDMETHODCALLTYPE OnClose()
	{ m_fs->m_IAdviseSink2->OnClose(); }
	//IAdviseSinkEx
	void STDMETHODCALLTYPE OnViewStatusChange(DWORD);
protected:
	FrameSite * m_fs;
};

class FS_IDocHostUIHandler : public IDocHostUIHandler
{
public:
	FS_IDocHostUIHandler(FrameSite* fs) 	{ m_fs = fs; }
	~FS_IDocHostUIHandler() {}

	//IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	
	STDMETHODIMP GetHostInfo( 
		/* [out][in] */ DOCHOSTUIINFO __RPC_FAR *pInfo);

	STDMETHODIMP ShowContextMenu(
		/* [in] */ DWORD dwID,
		/* [in] */ POINT __RPC_FAR *ppt,
		/* [in] */ IUnknown __RPC_FAR *pcmdtReserved,
		/* [in] */ IDispatch __RPC_FAR *pdispReserved)		{ return S_FALSE; }
	
	STDMETHODIMP ShowUI( 
		/* [in] */ DWORD dwID,
		/* [in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject,
		/* [in] */ IOleCommandTarget __RPC_FAR *pCommandTarget,
		/* [in] */ IOleInPlaceFrame __RPC_FAR *pFrame,
		/* [in] */ IOleInPlaceUIWindow __RPC_FAR *pDoc)		{ return S_FALSE; }
	
    STDMETHODIMP HideUI(void)								{ return S_OK; }
	
	STDMETHODIMP UpdateUI(void)								{ return S_OK; }
	
	STDMETHODIMP EnableModeless(BOOL fEnable) 				{ return S_OK; }
	
	STDMETHODIMP OnDocWindowActivate(BOOL fActivate) 		{ return S_OK; }
	
	STDMETHODIMP OnFrameWindowActivate(BOOL fActivate)		{ return S_OK; }
	
	STDMETHODIMP ResizeBorder( 
		/* [in] */ LPCRECT prcBorder,
		/* [in] */ IOleInPlaceUIWindow __RPC_FAR *pUIWindow,
		/* [in] */ BOOL fRameWindow) 						{ return S_OK; }
	
	STDMETHODIMP TranslateAccelerator( 
		/* [in] */ LPMSG lpMsg,
		/* [in] */ const GUID __RPC_FAR *pguidCmdGroup,
		/* [in] */ DWORD nCmdID) 							{ return S_FALSE; }
	
	STDMETHODIMP GetOptionKeyPath( 
		/* [out] */ LPOLESTR __RPC_FAR *pchKey,
		/* [in] */ DWORD dw)								{ return S_FALSE; }
	
	STDMETHODIMP GetDropTarget( 
		/* [in] */ IDropTarget __RPC_FAR *pDropTarget,
		/* [out] */ IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget) { return E_FAIL; }
	
	STDMETHODIMP GetExternal( 
		/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDispatch) 	{ ppDispatch = NULL; return S_FALSE; }
	
	STDMETHODIMP TranslateUrl( 
		/* [in] */ DWORD dwTranslate,
		/* [in] */ OLECHAR __RPC_FAR *pchURLIn,
		/* [out] */ OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut) 		{ ppchURLOut = NULL; return S_FALSE; }
	
	STDMETHODIMP FilterDataObject( 
		/* [in] */ IDataObject __RPC_FAR *pDO,
		/* [out] */ IDataObject __RPC_FAR *__RPC_FAR *ppDORet) 		{ ppDORet = NULL; return S_FALSE; }

protected:
	FrameSite * m_fs;
};

HRESULT FS_IDocHostUIHandler::GetHostInfo(DOCHOSTUIINFO __RPC_FAR *pInfo)
{
	pInfo->cbSize = sizeof(DOCHOSTUIINFO);
    pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_SCROLL_NO;
	return S_OK;
}


CUtlVector<HtmlWindow *> html_windows;

//-----------------------------------------------------------------------------
// Purpose: forcefully destruct any HTML windows that are left around, so we can unhook from IE
//-----------------------------------------------------------------------------

LRESULT CALLBACK WindowProc(
  HWND hwnd,      // handle to window
  UINT uMsg,      // message identifier
  WPARAM wParam,  // first message parameter
  LPARAM lParam   // second message parameter
)
{
	// find the winwodw that triggered the hook
	HtmlWindow *win = NULL;
	for(int i=0;i<html_windows.Count();i++)
	{
		if(html_windows[i] && html_windows[i]->GetIEHWND()== hwnd)
		{
			win=html_windows[i];
			break;
		}
	}

	if ( win )
	{
		switch(uMsg)
		{
			case WM_KILLFOCUS:
				return 0;
				break;
			case WM_SETFOCUS:
				// yes, this is tricksy! 
				// if the IE hwnd gets focus then throw it back to the main VGUI hwnd
				// send a message (rather than SetFocus() ) so that the IE hwnd has focus for
				// a couple frames and can process the incoming mouse click/kb event
				::PostMessage( win->GetHWND(), WM_APP, 0, 0);
				return 0;
				break;	

			default:
				return ::CallWindowProc( (WNDPROC)win->GetIEWndProc(), hwnd, uMsg, wParam, lParam );
				break;
		}
	}
	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: callback used to hook window events (like WM_TIMER)
//-----------------------------------------------------------------------------
LRESULT CALLBACK GetMsgProc( int code, WPARAM wParam, LPARAM lParam )
{
	//CWPSTRUCT *msg = reinterpret_cast<CWPSTRUCT *>(lParam);
	MSG *msg = reinterpret_cast<MSG *>(lParam);
	HtmlWindow *win=NULL;

	// find the winwodw that triggered the hook
	for(int i=0;i<html_windows.Count();i++)
	{
		if(html_windows[i] && html_windows[i]->GetIEHWND()==msg->hwnd)
		{
			win=html_windows[i];
			break;
		}
	}

	if(msg && win)
	{		
		switch(msg->message)
		{
		case WM_TIMER:
	
			win->OnUpdate();
			break;
	
		default:
		/*	{
				char dbtxt[200];
				Q_snprintf(dbtxt,sizeof(dbtxt),"$$$$$$$$$$$$$$$ Got msg to window! %x (%p,%p)\n",msg->message,msg->hwnd,win);
				OutputDebugString(dbtxt);
			}*/
			break;
		}
		return CallNextHookEx(win->GetHook() ,code,wParam,lParam);
	}
	
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: callback used to enumerate through child windows
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildProc( HWND hwnd,  LPARAM lParam )
{
	POINT pt;
	pt.y = lParam >> 16;
	pt.x = lParam & 0xffff;

	HtmlWindow *win=NULL;
	HWND parent = ::GetParent(hwnd);

	for(int i=0;i<html_windows.Count();i++)
	{
		if(html_windows[i] && html_windows[i]->GetIEHWND()==parent)
		{
			win=html_windows[i];
			break;
		}
	}

	if(win)
	{
		RECT win_rect;
		::GetWindowRect(hwnd,&win_rect);
		RECT ie_rect;
		::GetWindowRect(win->GetIEHWND(),&ie_rect);

		// work out the coords for this event in the CHILD WINDOWS space (not the parents)
		pt.x-=(win_rect.left-ie_rect.left);
		pt.y-=(win_rect.top-ie_rect.top);

		// setup lparam with the new value
		lParam = ((int)pt.y) << 16;
		lParam |= (((int)pt.x)&0xffff);

		// re-center the clients window area from 0,0
		win_rect.bottom=(win_rect.bottom-win_rect.top);
		win_rect.top=0;
		win_rect.right=(win_rect.right-win_rect.left);
		win_rect.left=0;

	
		if( ::PtInRect(&win_rect,pt)) // if point is within client control
		{
				// send the mouse event
			if (::PostMessage(hwnd, win->GetMouseMessage(), 1, lParam)==0) 
			{ 
				//DWORD err= GetLastError();
				DEBUG("msg2 not delivered");
				return true; 
			}
		
			// also tell it that it got focus
			if ( ::PostMessage(hwnd, WM_SETFOCUS, reinterpret_cast<uint>(win->GetIEHWND()), lParam)==0) 
			{ 
				//DWORD err= GetLastError();
				DEBUG("msg2 not delivered");
				return true; 
			}

		}
	}

	return true;

}




//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
HtmlWindow::HtmlWindow(vgui::IHTMLEvents *events, vgui::VPANEL c,HWND parent, bool AllowJavaScript, bool DirectToHWND)
{
	m_oleObject = NULL;
	m_oleInPlaceObject = NULL;
	m_webBrowser = NULL;

	m_events = events;
	m_ieHWND=0;

	w=0;
	h=0;
	window_x = 0;
	window_y = 0;
	textureID=0;
	m_HtmlEventsAdviseCookie=0;
	m_Bitmap=NULL;
	m_bVisible = false;

	hdcMem=NULL;
	lasthDC=NULL;
	hBitmap=NULL;
	m_hHook=NULL;

	m_cleared=false;
	m_newpage=false;
	m_bHooked=false;
	m_bDirectToHWND = DirectToHWND;
	m_hIEWndProc=NULL;

	strcpy(m_currentUrl, "");
	m_specificallyOpened = false;
	m_parent=parent;
	m_vcontext=c;

	if ( !DirectToHWND )
	{

		// the render-to-bitmap version has a seperate window 
		// that is not connected to the main games hwnd so key focus
		// can be managed properly
		char tmp[50];
		if(::GetClassName(parent,tmp,50)==0)
		{
			WNDCLASS		wc;
			memset( &wc, 0, sizeof( wc ) );

			wc.style         = CS_OWNDC;
			wc.lpfnWndProc   = WindowProc;
			wc.hInstance     = GetModuleHandle(NULL);
			wc.lpszClassName = "VGUI_HTML";

			// Oops, we didn't clean up the class registration from last cycle which
			//  might mean that the wndproc pointer is bogus
			UnregisterClass( "VGUI_HTML", GetModuleHandle(NULL) );

			// Register it again
			RegisterClass( &wc );
			strcpy(tmp,"VGUI_HTML");
		}

		// create a temp window to contain this object, make it hidden and disabled
		m_parent = CreateWindowEx(0,tmp,"", WS_CHILD| WS_DISABLED,
								   0,0,1,1,parent,NULL,GetModuleHandle(NULL),NULL);
	}


	CreateBrowser(AllowJavaScript);
}

static const CLSID CLSID_MozillaBrowser =
{ 0x1339B54C, 0x3453, 0x11D2,
  { 0x93, 0xB9, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00 } };

//-----------------------------------------------------------------------------
// Purpose: Create the webbrowser object via COM calls and initialises it
//-----------------------------------------------------------------------------
void HtmlWindow::CreateBrowser( bool AllowJavaScript )
{
	HRESULT hret;

	IUnknown *p;			
	// Get IUnknown Interface
	hret = CoCreateInstance(CLSID_WebBrowser, NULL, 
			CLSCTX_ALL, IID_IUnknown, (void**)(&p));

	// this is the mozilla browser rather than the IE one, doesn't work too well tho...
	//hret = CoCreateInstance(CLSID_MozillaBrowser, NULL, 
	//		CLSCTX_ALL, IID_IUnknown, (void**)(&p));
	ASSERT(SUCCEEDED(hret));

	// Get IOleObject interface
	hret = p->QueryInterface(IID_IViewObject, (void**)(&m_viewObject));
	ASSERT(SUCCEEDED(hret));
	hret = p->QueryInterface(IID_IOleObject, (void**)(&m_oleObject));
	ASSERT(SUCCEEDED(hret));

	// make our container class for the object
	FrameSite *c = new FrameSite(this, AllowJavaScript);
	c->AddRef();

	m_fs=c;

	m_viewObject->SetAdvise(DVASPECT_CONTENT,ADVF_PRIMEFIRST,c->m_IAdviseSinkEx);

	DWORD dwMiscStatus;
	// query the state of the object
	m_oleObject->GetMiscStatus(DVASPECT_CONTENT, &dwMiscStatus);
	bool m_bSetClientSiteFirst = false;
	if (dwMiscStatus & OLEMISC_SETCLIENTSITEFIRST)
	{
		m_bSetClientSiteFirst = true;
	}
	bool m_bVisibleAtRuntime = true;
	if (dwMiscStatus & OLEMISC_INVISIBLEATRUNTIME)
	{
		m_bVisibleAtRuntime = false;
	}

	// tell the object where its container is
	if (m_bSetClientSiteFirst) m_oleObject->SetClientSite(c->m_IOleClientSite);

	// setup its persistant storage class
	IPersistStreamInit * psInit = NULL;
	hret = p->QueryInterface(IID_IPersistStreamInit, (void**)(&psInit));
	if (SUCCEEDED(hret) && psInit != NULL) {
		hret = psInit->InitNew();
		ASSERT(SUCCEEDED(hret));
	}
	psInit->Release();

	// Get IOleInPlaceObject interface
	hret = p->QueryInterface(IID_IOleInPlaceObject, (void**)(&m_oleInPlaceObject));
	assert(SUCCEEDED(hret));


	RECT posRect;
	posRect.left = 0;
	posRect.top = 0;
	posRect.right = w;
	posRect.bottom = h;

	m_oleInPlaceObject->SetObjectRects(&posRect, &posRect);


   MSG msg;

	if (m_bVisibleAtRuntime) {
		hret = m_oleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, &msg, 
			c->m_IOleClientSite, 0, m_parent, &posRect);
		assert(SUCCEEDED(hret));
	}
	
	
	// get a NEW handle, cause we hide this window
	hret = m_oleInPlaceObject->GetWindow(&m_oleObjectHWND);
	ASSERT(SUCCEEDED(hret));

	if (!m_bSetClientSiteFirst) m_oleObject->SetClientSite(c->m_IOleClientSite);

	// Get IWebBrowser2 Interface
	hret = p->QueryInterface(IID_IWebBrowser2, (void**)(&m_webBrowser));
	assert(SUCCEEDED(hret));

	IConnectionPointContainer * cpContainer;
	hret = p->QueryInterface(IID_IConnectionPointContainer, (void**)(&cpContainer));
	assert(SUCCEEDED(hret));
	hret = cpContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &m_connectionPoint);
	assert(SUCCEEDED(hret));
	// connect the object to our sink
	m_connectionPoint->Advise(c->m_DWebBrowserEvents2, &m_adviseCookie);
	cpContainer->Release();

	p->Release();

	// setup the webbrowser object
	m_webBrowser->put_MenuBar(VARIANT_FALSE);
	m_webBrowser->put_AddressBar(VARIANT_FALSE);
	m_webBrowser->put_StatusBar(VARIANT_FALSE);
	m_webBrowser->put_ToolBar(VARIANT_FALSE);

	m_webBrowser->put_RegisterAsBrowser(VARIANT_TRUE);
	m_webBrowser->put_RegisterAsDropTarget(VARIANT_TRUE);
	m_webBrowser->put_Silent(VARIANT_TRUE);

	if ( m_bDirectToHWND )
	{
		::ShowWindow( m_oleObjectHWND, SW_HIDE );
	}

	html_windows.AddToTail( this );
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
HtmlWindow::~HtmlWindow()
{
	html_windows.FindAndRemove( this );

	// we are being destroyed, reset the IE hwnd's window proc (or else we crash)
	/*	if ( m_hIEWndProc )

	if ( m_IEHWndPlat )
	{
		if ( m_IEHWndPlat->hdc )
		{
			::SetWindowLong( m_ieHWND, GWL_WNDPROC, (long)m_hIEWndProc );
			m_hIEWndProc = NULL;
		}
	*/
	if (m_oleInPlaceObject) {
		m_oleInPlaceObject->InPlaceDeactivate();
		m_oleInPlaceObject->UIDeactivate();
		m_oleInPlaceObject->Release();
	}
	if (m_connectionPoint) {
		m_connectionPoint->Unadvise(m_adviseCookie);
		m_connectionPoint->Unadvise(m_HtmlEventsAdviseCookie);
		m_connectionPoint->Release();
	}
	if (m_oleObject) {
		m_oleObject->Close(OLECLOSE_NOSAVE);
		m_oleObject->SetClientSite(NULL);
		m_oleObject->Release();
	}
	if (m_viewObject) {
		m_viewObject->Release();
	}
	if (m_webBrowser) {
		m_webBrowser->Release();
	}

	if ( m_fs )
	{
		delete (FrameSite *)m_fs;
	}
	if ( lasthDC )
	{
		::DeleteObject(hBitmap);
		::DeleteDC(hdcMem);
	}

	if ( m_hHook )
	{
		::UnhookWindowsHookEx( m_hHook );
		m_hHook = NULL;
	}

	delete m_Bitmap;
}

//-----------------------------------------------------------------------------
// Purpose: called when the VGUI panel changes size, updates the IE component and the
//			hidden window overlay.
//-----------------------------------------------------------------------------
void HtmlWindow::Obsolete_OnSize(int x,int y,int w_in,int h_in)
{	
	w=w_in;
	h=h_in;

	html_x=x;
	html_y=y;

	int panel_x,panel_y;
	// find out where the panel is, in screen space
#if defined ( GAMEUI_EXPORTS ) || defined( GAMEOVERLAYUI_EXPORTS )
	vgui::ipanel()->GetAbsPos(m_vcontext,panel_x, panel_y);

	// move the hidden IE container window to the top corner of the real panel
	::SetWindowPos(m_parent,HWND_TOP,panel_x,panel_y,w,h,SWP_HIDEWINDOW|SWP_NOACTIVATE);

#else
	if ( m_bDirectToHWND )
	{
		panel_x = 0; 
		panel_y = 0;

		int x_off, y_off;
		vgui::VPANEL panel = m_vcontext;
		while ( ! ((vgui::VPanel *)panel)->IsPopup() )
		{
			((vgui::VPanel *)panel)->GetPos(x_off, y_off);
			panel_x += x_off;
			panel_y += y_off;
			panel = (vgui::VPANEL) (((vgui::VPanel *)panel)->GetParent());
		}
	}
	else
	{
		((vgui::VPanel *)m_vcontext)->GetAbsPos(panel_x, panel_y);
	}
#endif

	window_x = panel_x + 1;
	window_y = panel_y + 1;
	// now reset our painting area
	if(lasthDC!=0)
	{
		::DeleteObject(hBitmap);
		::DeleteDC(hdcMem);
	}

	SetBounds();

	lasthDC=0;
}


//-----------------------------------------------------------------------------
// Purpose: determines whether the control is visible or not, DEPRECATED - Use SetVisible()
//-----------------------------------------------------------------------------
bool HtmlWindow::Show(bool shown)
{
	if (m_webBrowser) 
	{
		m_webBrowser->put_Visible(shown);
	}	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: determines whether the control is visible or not
//-----------------------------------------------------------------------------
void HtmlWindow::SetVisible(bool state)
{
	if (m_webBrowser) 
	{
		m_webBrowser->put_Visible(state);
	}
	m_bVisible = state;

	if ( m_newpage || !state )
	{
		if ( m_bDirectToHWND )
		{
			::ShowWindow( m_oleObjectHWND, state? SW_SHOWNA : SW_HIDE );
		}
		else
		{
			::ShowWindow( m_oleObjectHWND, SW_HIDE );
		}
	}
}	

//-----------------------------------------------------------------------------
// Purpose: causes the IE component to stop loading the current page
//-----------------------------------------------------------------------------
bool HtmlWindow::StopLoading()
{
	HRESULT hret=m_webBrowser->Stop();

	if(SUCCEEDED(hret))
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: triggers the IE component to refresh
//-----------------------------------------------------------------------------
bool HtmlWindow::Refresh()
{
	DEBUG("onrefresh");

	OpenURL(m_currentUrl);
	return true;

	// it would be nice to use Refresh() from IWebbrowser but it doesn't
	// fire any events so we can't for the hiding of the scroll bars...
	/*VARIANTARG vType;

	vType.vt=VT_INT;
	vType.intVal = REFRESH_NORMAL ;

	m_specificallyOpened = true;

	HRESULT hret=m_webBrowser->Refresh2(&vType);

	if(SUCCEEDED(hret))
	{
		OnFinishURL(m_currentUrl);
		return true;
	}
	return false;*/

}

//-----------------------------------------------------------------------------
// Purpose: generic function used to inform IE of its size and return the current size
//-----------------------------------------------------------------------------
RECT HtmlWindow::SetBounds()
{
	RECT posRect;

	if ( m_bDirectToHWND )
	{
		posRect.right = w + window_x;
		posRect.bottom = h + window_y;
		posRect.left = window_x;
		posRect.top = window_y;
	}
	else
	{
		posRect.right = w;
		posRect.bottom = h;
		posRect.left = 0;
		posRect.top = 0;
	}

	if (m_oleInPlaceObject) {
		int hret=	m_oleInPlaceObject->SetObjectRects(&posRect, &posRect);
		hret;
		ASSERT(SUCCEEDED(hret));
	}

	if ( m_webBrowser )
	{
		m_webBrowser->put_Height(h);
		m_webBrowser->put_Width(w);
		ScrollHTML(html_x,html_y);
	}
	
	if ( !m_bDirectToHWND )
	{
		::RedrawWindow( m_ieHWND, NULL, NULL, RDW_ERASE | RDW_INVALIDATE );
	}

	return posRect;
}

//-----------------------------------------------------------------------------
// Purpose: renders the current view of the IE window into a bitmap
//-----------------------------------------------------------------------------
void HtmlWindow::OnPaint(HDC hDC)
{
	DEBUG("HtmlWindow::OnPaint repainting html win");

	// have loaded a page yet?
	if ( w==0 || h==0 )
	{
		return; 
	}

	if ( m_bDirectToHWND )
	{
		return;
	}

	// Draw only when control is has a view object
	if ( !m_viewObject )
	{
		DEBUG("HtmlWindow::OnPaint Not Drawing!");
		return;
	}

	RECT posRect = SetBounds();
	HBITMAP oldbmp = NULL;

	// Create memory DC if we just started or its a new handle
	if ( lasthDC==0 || 
		lasthDC != hDC )
	{
		if ( lasthDC != 0 )
		{
			::DeleteObject(hBitmap);
			::DeleteDC(hdcMem);
		}
		lasthDC = hDC;

		hdcMem = CreateCompatibleDC( hDC );
		// Create compatible bitmap
		hBitmap = CreateCompatibleBitmap( hDC , w, h);
	}

	size_t datalen = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
	LPBITMAPINFO lpbi = (LPBITMAPINFO)(new char[datalen]);
	if ( !lpbi )
		return;

	Assert( hBitmap!=NULL );

	// Select bitmap into DC
	oldbmp =( HBITMAP )SelectObject( hdcMem, hBitmap );

	Assert( oldbmp != NULL );

	m_webBrowser->put_Height( h );
	m_webBrowser->put_Width( w );

	ScrollHTML( html_x, html_y );
	
	RECTL *prc = (RECTL *)&posRect;
	m_viewObject->Draw
	(
		DVASPECT_CONTENT, 
		-1, 
		NULL, 
		NULL, 
		hDC, 
		hdcMem, 
		prc, 
		NULL, 
		NULL, 
		0
	);

	Q_memset( lpbi, 0x0, datalen );
	lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
	
	// Query the bitmap for its size
	int chk = GetDIBits
	(
		hdcMem, 
		hBitmap, 
		0L, 
		(DWORD)h,
		(LPBYTE)NULL, 
		(LPBITMAPINFO)lpbi, 
		(DWORD)DIB_RGB_COLORS
	);

	if ( chk != 0 )
	{
		// tell windows what format we want the result in
		lpbi->bmiHeader.biBitCount		= 32;
		lpbi->bmiHeader.biCompression	= BI_BITFIELDS;
		lpbi->bmiHeader.biSizeImage		= lpbi->bmiHeader.biWidth*lpbi->bmiHeader.biHeight*(lpbi->bmiHeader.biBitCount/8);
		// invert the height to make it draw the right way up...
		lpbi->bmiHeader.biHeight *= -1;

		// Now try and read the actual bits from the bitmap
		byte *lpvBits = new byte[ lpbi->bmiHeader.biSizeImage ]; 
		if ( lpvBits )
		{
			// now copy the data into the allocated memory
			chk = GetDIBits
				(
					hdcMem, 
					hBitmap, 
					0L, 
					(DWORD)h, 
					(LPBYTE)lpvBits, 
					(LPBITMAPINFO)lpbi, 
					(DWORD)DIB_RGB_COLORS
				); 

			if ( chk != 0 )
			{	
				byte *bmpBits = lpvBits;

#if defined ( GAMEUI_EXPORTS ) || defined( GAMEOVERLAYUI_EXPORTS )
				// windows stores DIB's as BGR not RGB ... (why oh why?)
				// we could get windows to do it via CreateDIBitmap() but this is quicker :)
				// (and win95 can have problems with RGB format...)
				for( unsigned int i=0; i < lpbi->bmiHeader.biSizeImage; i+=4 )
				{
					register unsigned char tmp;
					tmp=bmpBits[i];
					bmpBits[i]=bmpBits[i+2];
					bmpBits[i+2]=tmp;
					bmpBits[i+3]=255; // make it fully opaque
				}
#endif

				if ( !m_Bitmap )
				{
					m_Bitmap = new vgui::MemoryBitmap( bmpBits, w , h );
				}
				else
				{
					m_Bitmap->ForceUpload( bmpBits, w, h ); // upload the new image
				}
			}

			delete[] lpvBits;
		}
	}

	SelectObject( hdcMem, oldbmp );

	delete[] lpbi;
}


//-----------------------------------------------------------------------------
// Purpose: handler for mouse click events, passes the message through to the IE component
//-----------------------------------------------------------------------------
void HtmlWindow::OnMouse(vgui::MouseCode code,MOUSE_STATE s,int x,int y)
 {
	DEBUG("mouse event");
	UINT msg = 0;
	WPARAM wParam = 1;
	LPARAM lParam = 0;
	
	lParam = y << 16;
	lParam |= x;
	m_iMousePos=lParam;

#ifndef _DEBUG
	/// ONLY handle left clicks!
	if(code!=MOUSE_LEFT)
		return;
#endif

	if(s==IHTML::UP)
	{
		if(code==MOUSE_LEFT) 
		{
			msg=WM_LBUTTONUP;
		}
		else if (code==MOUSE_RIGHT) 
		{
			msg=WM_RBUTTONUP;
		}
	}
	else if (s==IHTML::DOWN)
	{
		if(code==MOUSE_LEFT) 
		{
			msg=WM_LBUTTONDOWN;
		}
		else if (code==MOUSE_RIGHT) 
		{
			msg=WM_RBUTTONDOWN;	
		}
	}
	else if (s==IHTML::MOVE)
	{
		msg=WM_MOUSEMOVE;
	}
	else
	{
		return;
	}

	if (m_oleInPlaceObject == NULL) { DEBUG("no oleInPlaceObject"); return ; }
	
	// send the mouse event
	if (m_ieHWND && ::PostMessage(m_ieHWND, msg, wParam, lParam)==0) 
	{ 
		//DWORD err= GetLastError();
		DEBUG("msg2 not delivered");
		return; 
	}

	// also tell it that it got focus
	if (m_ieHWND && ::PostMessage(m_ieHWND, WM_SETFOCUS, reinterpret_cast<uint>(m_oleObjectHWND), lParam)==0) 
	{ 
		//DWORD err= GetLastError();
		DEBUG("msg2 not delivered");
		return; 
	}

	EnumChildWindows(m_ieHWND,EnumChildProc,lParam);

	DEBUG("msg sent");
}


//-----------------------------------------------------------------------------
// Purpose: empties the current HTML container of any HTML text (used in conjunction with AddText)
//-----------------------------------------------------------------------------
void HtmlWindow::Clear()
{
	IDispatch* pDisp ;
	m_webBrowser->get_Document(&pDisp);

	if (pDisp != NULL )
	{
		IHTMLDocument2* pHTMLDocument2;
		HRESULT hr;
		hr = pDisp->QueryInterface( IID_IHTMLDocument2, (void**)&pHTMLDocument2 );
		if (hr == S_OK)
		{
			pHTMLDocument2->close();
			pHTMLDocument2->Release();
		}
		pDisp->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds the string "text" to the end of the current HTML page.
//-----------------------------------------------------------------------------
void HtmlWindow::AddText(const char *text)
{
	IDispatch* pDisp ;
	m_webBrowser->get_Document(&pDisp);

	IHTMLDocument2* pHTMLDocument2;
	HRESULT hret;
	hret = pDisp->QueryInterface( IID_IHTMLDocument2, (void**)&pHTMLDocument2 );
	if (hret == S_OK)
	{
		wchar_t *tmp= new wchar_t[strlen(text)+1];
		// Creates a new one-dimensional array
		if(tmp)
		{
			IHTMLElement *pElement;
			::MultiByteToWideChar(CP_ACP,0,text,strlen(text)+1,tmp,strlen(text)+1);
			SysAllocString(tmp);
			
			HRESULT hret = pHTMLDocument2->get_body(&pElement);
			if( hret == S_OK && pElement)
			{
				BSTR where = L"beforeEnd";
				pElement->insertAdjacentHTML(where,tmp);
				pElement->Release();
			}
		}
		pHTMLDocument2->Release();
	}
	pDisp->Release();
}

//-----------------------------------------------------------------------------
// Purpose: passes a character though to the browser
//-----------------------------------------------------------------------------
void HtmlWindow::OnChar(wchar_t unichar)
{
	UINT msg = 0;
	WPARAM wParam = 1;
	LPARAM lParam = 0;
	//LRESULT lResult = 0;

	wParam = unichar;

	// pass the character to the window
	msg=WM_CHAR;

	if (m_oleInPlaceObject == NULL) { DEBUG("no oleInPlaceObject"); return ; }

	if (m_ieHWND && ::PostMessage(m_ieHWND, msg, wParam, lParam)==0) { DEBUG("msg not delivered"); return; }
	DEBUG("msg sent");
}

//-----------------------------------------------------------------------------
// Purpose: passes a keydown event to the engine, used mainly for left and right arrows in text displays
//-----------------------------------------------------------------------------
void HtmlWindow::OnKeyDown(vgui::KeyCode code)
{
	UINT msg = 0;
	WPARAM wParam = 1;
	LPARAM lParam = 0;
	//LRESULT lResult = 0;

	wParam = KeyCode_VGUIToVirtualKey( code );

	// pass the character to the window
	msg=WM_KEYDOWN;

	if (m_oleInPlaceObject == NULL) { DEBUG("no oleInPlaceObject"); return ; }

	if (m_ieHWND && ::PostMessage(m_ieHWND, msg, wParam, lParam)==0) { DEBUG("msg not delivered"); return; }
	DEBUG("msg sent");
}


//-----------------------------------------------------------------------------
// Purpose: called when a new URL is requested, can stop the engine processing it.
//			This calls the m_events->OnStartURL callback if available.
//-----------------------------------------------------------------------------
bool HtmlWindow::OnStartURL(const char * url, const char *target, bool first)
{
	DEBUG("loading url:");
	DEBUG(url);

	if (m_HtmlEventsAdviseCookie != 0 ) // this is reset for each new page load
	{
		m_connectionPoint->Unadvise(m_HtmlEventsAdviseCookie);
	}

	Q_strncpy(m_currentUrl,url,512);
	if (m_specificallyOpened) {
		m_specificallyOpened = false;

	}
	m_newpage=false;

	if (m_events)
	{
		if ( m_events->OnStartRequestInternal(url, target, NULL, false) )
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
// Purpose: called as the URL download progresses
//-----------------------------------------------------------------------------
void HtmlWindow::OnProgressURL(long current, long maximum)
{
	DEBUG("progress url");
	if(m_events)
	{
		m_events->OnProgressRequest(current,maximum);
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback to provide useful status info from the IE component
//-----------------------------------------------------------------------------
void HtmlWindow::OnSetStatusText(const char *text)
{
	DEBUG("set status text");
	if(m_events)
	{
		m_events->OnSetStatusText(text);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a repaint is needed
//-----------------------------------------------------------------------------
void HtmlWindow::OnUpdate()
{
	DEBUG("onupdate");
	// You need to query the IHTMLElement interface here to get it later, I have no idea why...
	IDispatch* pDisp ;
	m_webBrowser->get_Document(&pDisp);

	if ( pDisp )
	{
		IHTMLDocument2* pHTMLDocument2;
		HRESULT hret;
		hret = pDisp->QueryInterface( IID_IHTMLDocument2, (void**)&pHTMLDocument2 );
		if (hret == S_OK)
		{	
			FrameSite *c = reinterpret_cast<FrameSite *>(m_fs);
			IConnectionPointContainer * cpContainer;
			hret = pDisp->QueryInterface(IID_IConnectionPointContainer, (void**)(&cpContainer));
			assert(SUCCEEDED(hret));
			hret = cpContainer->FindConnectionPoint(DIID_HTMLDocumentEvents, &m_connectionPoint);
			assert(SUCCEEDED(hret));
			// connect the object to our sink
			m_connectionPoint->Advise(c->m_DHTMLDocumentEvents2, &m_HtmlEventsAdviseCookie);
			cpContainer->Release();

			CalculateHTMLSize( pHTMLDocument2 );
			pHTMLDocument2->Release();
		}
		pDisp->Release();
	}

	if(m_events)
	{
		m_events->OnUpdate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the cursor goes over a link
//-----------------------------------------------------------------------------
void HtmlWindow::OnLink()
{
	DEBUG("onlink");
	if(m_events)
	{
		m_events->OnLink();
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the cursor leaves a link
//-----------------------------------------------------------------------------
void HtmlWindow::OffLink()
{
	DEBUG("offlink");
	if(m_events)
	{
		m_events->OffLink();
	}
}



//-----------------------------------------------------------------------------
// Purpose: returns whether the HTML element is a <a href></a> element
//-----------------------------------------------------------------------------
bool HtmlWindow::CheckIsLink(IHTMLElement *el, char *type)
{
	BSTR bstr;
	bool IsLink=false;
	el->get_tagName(&bstr);
	_bstr_t p  = _bstr_t(bstr);
	if (bstr)
	{	
		const char *tag = static_cast<char *>(p);
		if( !Q_stricmp(tag,type))
		{
			// its a link
			IsLink=true;
		}
		SysFreeString(bstr);
	}
	return IsLink;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void HtmlWindow::CalculateHTMLSize( void *pVoid )
{
	IHTMLDocument2 *pHTMLDocument2 = (IHTMLDocument2 *)pVoid;
	IHTMLBodyElement *piBody = NULL;
	IHTMLTextContainer *piCont = NULL;
	IHTMLElement *piElem = NULL;
	pHTMLDocument2->get_body( &piElem );
	if (!piElem )
		return;

	piElem->QueryInterface(IID_IHTMLBodyElement,(void **)&piBody);
	if (!piBody)
		return;

	piBody->put_scroll(_bstr_t("no"));

	HRESULT hret =	piBody->QueryInterface(IID_IHTMLTextContainer,(void **)&piCont);
	if ( hret == S_OK && piCont )
	{
		piCont->get_scrollWidth(&html_w);
		piCont->get_scrollHeight(&html_h);

		piCont->Release();
	}
	piBody->Release();
	piElem->Release();
}

//-----------------------------------------------------------------------------
// Purpose: scrolls the html element by x pixels down and y pixels across
//-----------------------------------------------------------------------------
void HtmlWindow::ScrollHTML(int x,int y)
{
	IDispatch* pDisp ;
	m_webBrowser->get_Document(&pDisp);
	bool bIECSSCompatMode = false; // yes, IE really does suck

	if (!pDisp )
		return;

	IHTMLDocument5* pHTMLDocument5 = NULL;
	HRESULT hret;
	hret = pDisp->QueryInterface( IID_IHTMLDocument5, (void**)&pHTMLDocument5 );
	if ( hret == S_OK && pHTMLDocument5 )
	{
		BSTR bStr;
		pHTMLDocument5->get_compatMode( &bStr );
		if ( !wcscmp( bStr, L"CSS1Compat" ) )
		{
			bIECSSCompatMode = true;
		}
	}

	if( bIECSSCompatMode )
	{
		IHTMLDocument3* pHTMLDocument2;
		HRESULT hret;
		hret = pDisp->QueryInterface( IID_IHTMLDocument3, (void**)&pHTMLDocument2 );
		if (hret == S_OK && pHTMLDocument2)
		{	
			IHTMLElement *pElement;
			hret = pHTMLDocument2->get_documentElement(&pElement);

			if(hret == S_OK && pElement)
			{
				IHTMLElement2 *piElem2 = NULL;
				hret = pElement->QueryInterface(IID_IHTMLElement2,(void **)&piElem2);	
				if ( hret == S_OK && piElem2 )
				{
					piElem2->put_scrollLeft( x );
					piElem2->put_scrollTop( y );
					piElem2->Release();
				}

				pElement->Release();
			}
			pHTMLDocument2->Release();
			
		}
		pDisp->Release();
	}
	else 
	{
		IHTMLDocument2* pHTMLDocument2;
		HRESULT hret;
		hret = pDisp->QueryInterface( IID_IHTMLDocument2, (void**)&pHTMLDocument2 );
		if (hret == S_OK && pHTMLDocument2)
		{	
			IHTMLElement *pElement = NULL;
			hret = pHTMLDocument2->get_body(&pElement);

			if(hret == S_OK && pElement)
			{
				IHTMLTextContainer *piCont = NULL;

				hret = pElement->QueryInterface(IID_IHTMLTextContainer,(void **)&piCont);
				if ( hret == S_OK && piCont )
				{
					piCont->put_scrollLeft(x);
					piCont->put_scrollTop(y);

					piCont->Release();
				}

				pElement->Release();
			}
			pHTMLDocument2->Release();

		}
		pDisp->Release();
	}


}

//-----------------------------------------------------------------------------
// Purpose: called when the cursor enters a new HTML Element (i.e link, image,etc)
//-----------------------------------------------------------------------------
bool HtmlWindow::OnMouseOver()
{
	bool IsLink=false;
	IDispatch* pDisp ;
	m_webBrowser->get_Document(&pDisp);

	IHTMLDocument2* pHTMLDocument2;
	HRESULT hret;
	hret = pDisp->QueryInterface( IID_IHTMLDocument2, (void**)&pHTMLDocument2 );
	if (hret == S_OK)
	{	
		IHTMLWindow2* pParentWindow;
	
		HRESULT hr = pHTMLDocument2->get_parentWindow(&pParentWindow);

			if (SUCCEEDED(hr))
			{
				IHTMLEventObj* pEvtObj;

				hr = pParentWindow->get_event(&pEvtObj);
				pParentWindow->Release();
	
				if (SUCCEEDED(hr) && pEvtObj)  // sometimes it can succeed BUT pEvtObj is NULL, nice eh?
				{
					IHTMLElement *el;
					pEvtObj->get_srcElement(&el);

					if(CheckIsLink(el,"A"))
					{
						IsLink=true;
					}
					else
					{
						IHTMLElement *pel=el,*oldpel=NULL;
						while(IsLink==false && SUCCEEDED(pel->get_parentElement(&pel)))
						{
							if(oldpel!=NULL)
							{
								oldpel->Release();
							}
							if(pel==NULL || pel==oldpel) 
								// this goes up the nested elements until it hits the <HTML> tag 
								// and then it just keeps returing that (silly thing...)
							{
								break;
							}

							if(CheckIsLink(pel,"A"))
							{
								IsLink=true;
							}

							oldpel=pel;	
						} 
						if(pel!=NULL)
						{
							pel->Release();
						}
					} //CheckIsLink
					el->Release();
				} // if got event
			} // if got window
		pHTMLDocument2->Release();
	} // if got document
	pDisp->Release();

	return IsLink;
}

void HtmlWindow::NewWindow(IDispatch **pIDispatch)
{
   m_webBrowser->get_Application(pIDispatch);
}


//-----------------------------------------------------------------------------
// Purpose: called when a URL is finished loading
//-----------------------------------------------------------------------------
void HtmlWindow::OnFinishURL(const char * url)
{
	DEBUG("loaded url:");

	static bool hooked=false; // this is only done ONCE for all HTML windows :)

	m_currentUrl[0] = 0;
	if ( url )
	{	
		Q_strncpy(m_currentUrl,url,512);
	}

	m_newpage=true;
	m_cleared=false;

	// You need to query the IHTMLElement interface here to get it later, I have no idea why...
	IDispatch* pDisp ;
	m_webBrowser->get_Document(&pDisp);

	if ( pDisp )
	{
		IHTMLDocument2* pHTMLDocument2;
		HRESULT hret;
		hret = pDisp->QueryInterface( IID_IHTMLDocument2, (void**)&pHTMLDocument2 );
		if (hret == S_OK)
		{	
			FrameSite *c = reinterpret_cast<FrameSite *>(m_fs);
			IConnectionPointContainer * cpContainer;
			hret = pDisp->QueryInterface(IID_IConnectionPointContainer, (void**)(&cpContainer));
			assert(SUCCEEDED(hret));
			hret = cpContainer->FindConnectionPoint(DIID_HTMLDocumentEvents, &m_connectionPoint);
			assert(SUCCEEDED(hret));
			// connect the object to our sink
			m_connectionPoint->Advise(c->m_DHTMLDocumentEvents2, &m_HtmlEventsAdviseCookie);
			cpContainer->Release();

			CalculateHTMLSize(pHTMLDocument2);
			pHTMLDocument2->Release();
		}
		pDisp->Release();
	}


	// tell the parent to repaint itself, we have a new page
	if(m_events)
	{
		m_events->OnFinishRequest(url, NULL);
	}

	HWND tst=m_oleObjectHWND;
	char name[100];
	memset(name,0x0,100);

	while(strcmp(name,"Internet Explorer_Server") && tst)
	{
		tst=::GetWindow(tst,GW_CHILD);
		if(tst) ::GetClassName(tst,name,100/sizeof(TCHAR));
	}

	if(tst)
	{
		m_ieHWND=tst;
	}

	if ( ! hooked )
	{
		DWORD tid = ::GetWindowThreadProcessId(m_oleObjectHWND, NULL); 
		m_hHook=::SetWindowsHookEx(WH_GETMESSAGE,GetMsgProc,NULL,tid);


		if(m_hHook==NULL)
		{
			DWORD err=::GetLastError();
			err++;
		}
		hooked = true;
	}

	if ( m_bVisible )
	{
		if ( m_bDirectToHWND )
		{
			::ShowWindow( m_oleObjectHWND, m_bVisible? SW_SHOWNA : SW_HIDE );
		}
		else
		{
			::ShowWindow( m_oleObjectHWND, SW_HIDE );
		}
	}
}


void HtmlWindow::OpenURL(const char *url)
{
	if (!url || !*url)
		return;

	VARIANTARG navFlag, targetFrame, postData, headers,vUrl;
	navFlag.vt = VT_EMPTY; 
	targetFrame.vt = VT_EMPTY;
	postData.vt = VT_EMPTY;
	headers.vt = VT_EMPTY;
	vUrl.vt=VT_BSTR;
	_bstr_t bstr =_T(url);
	vUrl.bstrVal=bstr;

	m_specificallyOpened = true;

	HRESULT hret;
	hret = m_webBrowser->Navigate2(&vUrl, 
		&navFlag, &targetFrame, &postData, &headers);

}

//-----------------------------------------------------------------------------
// Purpose: 
//		Implemenetation of the various OLE (ActiveX) containers for the IE widget.
//
//
//
//
//-----------------------------------------------------------------------------



FrameSite::FrameSite(HtmlWindow * win, bool AllowJavaScript)
{
	m_cRef = 0;

	m_window = win;
	m_bSupportsWindowlessActivation = true;
	m_bInPlaceLocked = false;
	m_bUIActive = false;
	m_bInPlaceActive = false;
	m_bWindowless = false;

	m_nAmbientLocale = 0;
	m_clrAmbientForeColor = ::GetSysColor(COLOR_WINDOWTEXT);
	m_clrAmbientBackColor = ::GetSysColor(COLOR_WINDOW);
	m_bAmbientUserMode = true;
	m_bAmbientShowHatching = true;
	m_bAmbientShowGrabHandles = true;
	m_bAmbientAppearance = true;
 
	m_hDCBuffer = NULL;
	m_hWndParent = (HWND)m_window->GetHWND();

	m_IOleInPlaceFrame = new FS_IOleInPlaceFrame(this);
	m_IOleInPlaceSiteWindowless = new FS_IOleInPlaceSiteWindowless(this);
	m_IOleClientSite = new FS_IOleClientSite(this);
	m_IOleControlSite = new FS_IOleControlSite(this);
	m_IOleCommandTarget = new FS_IOleCommandTarget(this);
	m_IOleItemContainer = new FS_IOleItemContainer(this);
	m_IDispatch = new FS_IDispatch(this);
	m_IDispatch->SetAllowJavaScript(AllowJavaScript);
	m_DWebBrowserEvents2 = new FS_DWebBrowserEvents2(this);
	m_DHTMLDocumentEvents2 = new FS_DHTMLDocumentEvents2(this);
	m_IAdviseSink2 = new FS_IAdviseSink2(this);
	m_IAdviseSinkEx = new FS_IAdviseSinkEx(this);
	m_IDocHostUIHandler = new FS_IDocHostUIHandler(this);
}

FrameSite::~FrameSite()
{
	delete m_IAdviseSinkEx;
	delete m_IAdviseSink2;
	delete m_DWebBrowserEvents2;
	delete m_DHTMLDocumentEvents2;
	delete m_IDispatch;
	delete m_IOleItemContainer;
	delete m_IOleCommandTarget;
	delete m_IOleControlSite;
	delete m_IOleClientSite;
	delete m_IOleInPlaceSiteWindowless;
	delete m_IOleInPlaceFrame;
	delete m_IDocHostUIHandler;
}

//IUnknown

STDMETHODIMP FrameSite::QueryInterface(REFIID riid, void **ppv)             
{
	if (ppv == NULL) return E_INVALIDARG;
	*ppv = NULL;
	if (riid == IID_IUnknown) 
		*ppv = this;
	else if (riid == IID_IOleWindow ||
		riid == IID_IOleInPlaceUIWindow ||
		riid == IID_IOleInPlaceFrame) 
		*ppv = m_IOleInPlaceFrame;
	else if (riid == IID_IOleInPlaceSite ||
		riid == IID_IOleInPlaceSiteEx ||
		riid == IID_IOleInPlaceSiteWindowless) 
		*ppv = m_IOleInPlaceSiteWindowless;
	else if (riid == IID_IOleClientSite) 
		*ppv = m_IOleClientSite;
	else if (riid == IID_IOleControlSite) 
		*ppv = m_IOleControlSite;
	else if (riid == IID_IOleCommandTarget) 
		*ppv = m_IOleCommandTarget;
	else if (riid == IID_IOleItemContainer ||
		riid == IID_IOleContainer ||
		riid == IID_IParseDisplayName) 
		*ppv = m_IOleItemContainer;
	else if (riid == IID_IDispatch) 
		*ppv = m_IDispatch;
	else if (riid == DIID_DWebBrowserEvents2) 
		*ppv = m_DWebBrowserEvents2;
	else if (riid == DIID_HTMLDocumentEvents) 
		*ppv = m_DHTMLDocumentEvents2;
	else if (riid == IID_IAdviseSink || 
		riid == IID_IAdviseSink2) 
		*ppv = m_IAdviseSink2;
	else if (riid == IID_IAdviseSinkEx) 
		*ppv = m_IAdviseSinkEx;
	else if (riid == IID_IDocHostUIHandler)
		*ppv = m_IDocHostUIHandler;
	
	if (*ppv == NULL) return (HRESULT) E_NOINTERFACE;                                         
	AddRef();
	return S_OK;
}                                                                           
                                                                              
STDMETHODIMP_(ULONG) FrameSite::AddRef()                                    
{                                                                           
	return ++m_cRef;                                                          
}                                                                           
                                                                              
STDMETHODIMP_(ULONG) FrameSite::Release()                                   
{                                                                           
	if ( --m_cRef == 0 ) {                                                    
		delete this;                                                            
		return 0;                                                               
	}                                                                         
	else return m_cRef;                                                          
}

//IDispatch

HRESULT FS_IDispatch::GetIDsOfNames(REFIID riid, OLECHAR ** rgszNames, unsigned int cNames,
								 LCID lcid, DISPID * rgDispId)
{
	DEBUG("IDispatch::GetIDsOfNames");
	return E_NOTIMPL;
}

HRESULT FS_IDispatch::GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo ** ppTInfo)
{
	DEBUG("IDispatch::GetTypeInfo");
	return E_NOTIMPL;
}

HRESULT FS_IDispatch::GetTypeInfoCount(unsigned int * pcTInfo)
{
	DEBUG("IDispatch::GetTypeInfoCount");
	return E_NOTIMPL;
}

HRESULT FS_IDispatch::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
						  WORD wFlags, DISPPARAMS * pDispParams,
						  VARIANT * pVarResult, EXCEPINFO * pExcepInfo,
						  unsigned int * puArgErr)
{
	DEBUG("IDispatch::Invoke");
	if (wFlags & DISPATCH_PROPERTYGET)
	{
		if (pVarResult == NULL) return E_INVALIDARG;
		switch (dispIdMember)
		{
			case DISPID_AMBIENT_APPEARANCE:
				pVarResult->vt = VT_BOOL;
				pVarResult->boolVal = m_fs->m_bAmbientAppearance;
				break;

			case DISPID_AMBIENT_FORECOLOR:
				pVarResult->vt = VT_I4;
				pVarResult->lVal = (long) m_fs->m_clrAmbientForeColor;
				break;

			case DISPID_AMBIENT_BACKCOLOR:
				pVarResult->vt = VT_I4;
				pVarResult->lVal = (long) m_fs->m_clrAmbientBackColor;
				break;

			case DISPID_AMBIENT_LOCALEID:
				pVarResult->vt = VT_I4;
				pVarResult->lVal = (long) m_fs->m_nAmbientLocale;
				break;

			case DISPID_AMBIENT_USERMODE:
				pVarResult->vt = VT_BOOL;
				pVarResult->boolVal = m_fs->m_bAmbientUserMode;
				break;

			case DISPID_AMBIENT_SHOWGRABHANDLES:
				pVarResult->vt = VT_BOOL;
				pVarResult->boolVal = m_fs->m_bAmbientShowGrabHandles;
				break;

			case DISPID_AMBIENT_SHOWHATCHING:
				pVarResult->vt = VT_BOOL;
				pVarResult->boolVal = m_fs->m_bAmbientShowHatching;
				break;

			case DISPID_AMBIENT_DLCONTROL: // turn off javascript
				pVarResult->vt = VT_I4;
				pVarResult->lVal = (long)(DLCTL_DLIMAGES|DLCTL_VIDEOS|
		                      DLCTL_BGSOUNDS);
				if ( !m_bAllowJavaScript )
				{
					pVarResult->lVal |= (long)(DLCTL_NO_SCRIPTS);
				}
			
				break;



			default:
				return DISP_E_MEMBERNOTFOUND;
		}
		return S_OK;
	}

//	char dbtxt[200];
//	Q_snprintf(dbtxt,200,"$$$$$$$$$$$$$$$ Got ID %i %i\n",dispIdMember, DISPID_BEFORENAVIGATE2);
//	OutputDebugString(dbtxt);
	

	switch (dispIdMember)
	{
		case DISPID_BEFORENAVIGATE2:
		{
			VARIANT * vurl = pDispParams->rgvarg[5].pvarVal;
			VARIANT * targetFrame = pDispParams->rgvarg[3].pvarVal;

			if (m_fs->m_window->OnStartURL((char*)_bstr_t(vurl->bstrVal),(char*)_bstr_t(targetFrame->bstrVal),!m_bNewURL))
			{
				*pDispParams->rgvarg->pboolVal = VARIANT_FALSE;
			}
			else
			{ // this cancels the navigation
				*pDispParams->rgvarg->pboolVal = VARIANT_TRUE;
			}
			
			break;
		}
		case DISPID_PROGRESSCHANGE:
		{
			long current = pDispParams->rgvarg[1].lVal;
			long maximum = pDispParams->rgvarg[0].lVal;
			m_fs->m_window->OnProgressURL(current, maximum);
			break;
		}
		case DISPID_DOWNLOADCOMPLETE:
		{
			// for each DOWNLOADBEGIN there is a DOWNLOADCOMPLETE
			if(m_bNewURL>0) 
			{
				m_bNewURL--;
			}
			break;
		}

		case DISPID_DOCUMENTCOMPLETE:
		{

			// DON'T do this in DISPID_NAVIGATECOMPLETE2, the DHTML stuff is incomplete in that event

			// tell the main window to update
			if(m_bNewURL==0) // only allow the first url to get through :)
			{
				VARIANT * vurl = pDispParams->rgvarg[0].pvarVal;
				m_fs->m_window->OnFinishURL((char*)_bstr_t(vurl->bstrVal));
			}
			else
			{
				m_fs->m_window->OnUpdate(); // not finished the page, but a major "chunk" is done
			}
		}
		case DISPID_NAVIGATECOMPLETE2:
		{
			m_fs->m_window->OnUpdate();
		
			break;
		}
		case DISPID_DOWNLOADBEGIN:
		{
			// for each DOWNLOADBEGIN there is a DOWNLOADCOMPLETE
			m_bNewURL++;
			//Msg("Begining download\n");
			break;
		}

		case DISPID_NEWWINDOW2:
		{ 
			// stop new browser windows being opened if we are in the engine
#if defined( VGUIMATSURFACE_DLL_EXPORT ) || defined( VGUIRENDERSURFACE_DLL_EXPORT )
			if (!vgui::surface()->SupportsFeature(vgui::ISurface::OPENING_NEW_HTML_WINDOWS))
#else
			if (!vgui::g_pSurface->SupportsFeature(vgui::ISurface::OPENING_NEW_HTML_WINDOWS))
#endif
			{
				m_bNewURL++;
				IDispatch *pIDispatch;
				 m_fs->m_window->NewWindow(&pIDispatch);
			   *(pDispParams->rgvarg[1].ppdispVal) = pIDispatch;
				break;
			}
		}
	
		// IHTMLDocuments2 events
		case DISPID_HTMLDOCUMENTEVENTS_ONMOUSEOVER:
	     {
			if(m_fs->m_window->OnMouseOver())
			{
				m_bOnLink=true;
				m_fs->m_window->OnLink();
			}
			else if(m_bOnLink)
			{
				m_bOnLink=false;
				m_fs->m_window->OffLink();
			}
			break;
		}

	}
	return S_OK;
}

//IOleWindow

HRESULT FS_IOleInPlaceFrame::GetWindow(HWND * phwnd)
{
	DEBUG("IOleWindow::GetWindow");
	if (phwnd == NULL) return E_INVALIDARG;
	(*phwnd) = m_fs->m_hWndParent;
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::ContextSensitiveHelp(BOOL fEnterMode)
{
	DEBUG("IOleWindow::ContextSensitiveHelp");
	return S_OK;
}

//IOleInPlaceUIWindow

HRESULT FS_IOleInPlaceFrame::GetBorder(LPRECT lprectBorder)
{
	DEBUG("IOleInPlaceUIWindow::GetBorder");
	if (lprectBorder == NULL) return E_INVALIDARG;
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT FS_IOleInPlaceFrame::RequestBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
	DEBUG("IOleInPlaceUIWindow::RequestBorderSpace");
	if (pborderwidths == NULL) return E_INVALIDARG;
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT FS_IOleInPlaceFrame::SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
	DEBUG("IOleInPlaceUIWindow::SetBorderSpace");
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::SetActiveObject(IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName)
{
	DEBUG("IOleInPlaceUIWindow::SetActiveObject");
	return S_OK;
}

//IOleInPlaceFrame

HRESULT FS_IOleInPlaceFrame::InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths)
{
	DEBUG("IOleInPlaceFrame::InsertMenus");
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject)
{
	DEBUG("IOleInPlaceFrame::SetMenu");
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::RemoveMenus(HMENU hmenuShared)
{
	DEBUG("IOleInPlaceFrame::RemoveMenus");
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::SetStatusText(LPCOLESTR pszStatusText)
{
	DEBUG("IOleInPlaceFrame::SetStatusText");
//FIXME
	if(m_fs->m_window->GetEvents())
	{
		char tmp[512];
		WideCharToMultiByte(CP_ACP, 0, pszStatusText, -1, tmp,512, NULL, NULL);
		m_fs->m_window->GetEvents()->OnSetStatusText(tmp);
	}
	return S_OK;
}
 
HRESULT FS_IOleInPlaceFrame::EnableModeless(BOOL fEnable)
{
	DEBUG("IOleInPlaceFrame::EnableModeless");
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::TranslateAccelerator(LPMSG lpmsg, WORD wID)
{
	DEBUG("IOleInPlaceFrame::TranslateAccelerator");
	return E_NOTIMPL;
}

//IOleInPlaceSite

HRESULT FS_IOleInPlaceSiteWindowless::CanInPlaceActivate()
{
	DEBUG("IOleInPlaceSite::CanInPlaceActivate");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceActivate()
{
	DEBUG("**************IOleInPlaceSite::OnInPlaceActivate");
	m_fs->m_bInPlaceActive = true;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnUIActivate()
{
	DEBUG("*****IOleInPlaceSite::OnUIActivate");
	m_fs->m_bUIActive = true;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetWindowContext(IOleInPlaceFrame **ppFrame,
									IOleInPlaceUIWindow **ppDoc,
									LPRECT lprcPosRect,
									LPRECT lprcClipRect,
									LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	DEBUG("IOleInPlaceSite::GetWindowContext");
	if (ppFrame == NULL || ppDoc == NULL || lprcPosRect == NULL ||
		lprcClipRect == NULL || lpFrameInfo == NULL)
	{
		if (ppFrame != NULL) (*ppFrame) = NULL;
		if (ppDoc != NULL) (*ppDoc) = NULL;
		return E_INVALIDARG;
	}

	(*ppDoc) = (*ppFrame) = m_fs->m_IOleInPlaceFrame;
	(*ppDoc)->AddRef();
	(*ppFrame)->AddRef();

	int w,h;
	m_fs->m_window->GetSize(w, h);

	lprcPosRect->left =0; 
	lprcPosRect->top = 0;
	lprcPosRect->right = w;
	lprcPosRect->bottom = h;
	lprcClipRect->left = 0;
	lprcClipRect->top = 0;
	lprcClipRect->right = w;
	lprcClipRect->bottom = h;


	lpFrameInfo->fMDIApp = FALSE;
	lpFrameInfo->hwndFrame = m_fs->m_hWndParent;
	lpFrameInfo->haccel = NULL;
	lpFrameInfo->cAccelEntries = 0;

	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::Scroll(SIZE scrollExtent)
{
	DEBUG("IOleInPlaceSite::Scroll");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnUIDeactivate(BOOL fUndoable)
{
	DEBUG("IOleInPlaceSite::OnUIDeactivate");
	m_fs->m_bUIActive = false;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceDeactivate()
{
	DEBUG("IOleInPlaceSite::OnInPlaceDeactivate");
	m_fs->m_bInPlaceActive = false;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::DiscardUndoState()
{
	DEBUG("IOleInPlaceSite::DiscardUndoState");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::DeactivateAndUndo()
{
	DEBUG("IOleInPlaceSite::DeactivateAndUndo");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnPosRectChange(LPCRECT lprcPosRect)
{
	DEBUG("IOleInPlaceSite::OnPosRectChange");
	return S_OK;
}

//IOleInPlaceSiteEx

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL * pfNoRedraw, DWORD dwFlags)
{
	DEBUG("IOleInPlaceSiteEx::OnInPlaceActivateEx");
	if (pfNoRedraw) (*pfNoRedraw) = FALSE;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceDeactivateEx(BOOL fNoRedraw)
{
	DEBUG("************IOleInPlaceSiteEx::OnInPlaceDeactivateEx");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::RequestUIActivate()
{
	DEBUG("************IOleInPlaceSiteEx::RequestUIActivate");
	return S_OK;
}

//IOleInPlaceSiteWindowless

HRESULT FS_IOleInPlaceSiteWindowless::CanWindowlessActivate()
{
	DEBUG("************IOleInPlaceSiteWindowless::CanWindowlessActivate");
	return (m_fs->m_bSupportsWindowlessActivation) ? S_OK : S_FALSE;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetCapture()
{
	DEBUG("************IOleInPlaceSiteWindowless::GetCapture");
	return S_FALSE;
}

HRESULT FS_IOleInPlaceSiteWindowless::SetCapture(BOOL fCapture)
{
	DEBUG("************IOleInPlaceSiteWindowless::SetCapture");
	return S_FALSE;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetFocus()
{
	DEBUG("************IOleInPlaceSiteWindowless::GetFocus");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::SetFocus(BOOL fFocus)
{
	DEBUG("************IOleInPlaceSiteWindowless::SetFocus");
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetDC(LPCRECT pRect, DWORD grfFlags, HDC* phDC)
{
	DEBUG("************IOleInPlaceSiteWindowless::GetDC");
	if (phDC == NULL) return E_INVALIDARG;

	if (grfFlags & OLEDC_NODRAW) 
	{
		(*phDC) = m_fs->m_hDCBuffer;
		return S_OK;
	}

	if (m_fs->m_hDCBuffer != NULL) return E_UNEXPECTED;

	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::ReleaseDC(HDC hDC)
{
	DEBUG("************IOleInPlaceSiteWindowless::ReleaseDC");
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::InvalidateRect(LPCRECT pRect, BOOL fErase)
{
	DEBUG("************IOleInPlaceSiteWindowless::InvalidateRect");
	// Clip the rectangle against the object's size and invalidate it
	RECT rcI = { 0, 0, 0, 0 };
	RECT posRect=m_fs->m_window->SetBounds();
	if (pRect == NULL)
	{
		rcI = posRect;
	}
	else
	{
		IntersectRect(&rcI, &posRect, pRect);
	}
	::InvalidateRect(m_fs->m_hWndParent, &rcI, fErase);
 
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::InvalidateRgn(HRGN, BOOL)
{
	DEBUG("************IOleInPlaceSiteWindowless::InvalidateRgn");
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::ScrollRect(INT, INT, LPCRECT, LPCRECT)
{
	DEBUG("************IOleInPlaceSiteWindowless::ScrollRect");
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::AdjustRect(LPRECT)
{
	DEBUG("************IOleInPlaceSiteWindowless::AdjustRect");
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*)
{
	DEBUG("************IOleInPlaceSiteWindowless::OnDefWindowMessage");
	return E_NOTIMPL;
}

//IOleClientSite

HRESULT FS_IOleClientSite::SaveObject()
{
	DEBUG("IOleClientSite::SaveObject");
	return S_OK;
}

HRESULT FS_IOleClientSite::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker,
							  IMoniker ** ppmk)
{
	DEBUG("IOleClientSite::GetMoniker");
	return E_NOTIMPL;
}

HRESULT FS_IOleClientSite::GetContainer(LPOLECONTAINER * ppContainer)
{
	DEBUG("IOleClientSite::GetContainer");
	if (ppContainer == NULL) return E_INVALIDARG;
	this->QueryInterface(IID_IOleContainer, (void**)(ppContainer));
	return S_OK;
}

HRESULT FS_IOleClientSite::ShowObject()
{
	DEBUG("IOleClientSite::ShowObject");
	return S_OK;
}

HRESULT FS_IOleClientSite::OnShowWindow(BOOL fShow)
{
	DEBUG("IOleClientSite::OnShowWindow");
	return S_OK;
}

HRESULT FS_IOleClientSite::RequestNewObjectLayout()
{
	DEBUG("IOleClientSite::RequestNewObjectLayout");
	return E_NOTIMPL;
}

//IParseDisplayName

HRESULT FS_IOleItemContainer::ParseDisplayName(IBindCtx *pbc, LPOLESTR pszDisplayName,
									ULONG *pchEaten, IMoniker **ppmkOut)
{
	DEBUG("IParseDisplayName::ParseDisplayName");
	return E_NOTIMPL;
}

//IOleContainer

HRESULT FS_IOleItemContainer::EnumObjects(DWORD grfFlags, IEnumUnknown **ppenum)
{
	DEBUG("IOleContainer::EnumObjects");
	return E_NOTIMPL;
}

HRESULT FS_IOleItemContainer::LockContainer(BOOL fLock)
{
	DEBUG("IOleContainer::LockContainer");
	return S_OK;
}

//IOleItemContainer

HRESULT FS_IOleItemContainer::GetObject(LPOLESTR pszItem, DWORD dwSpeedNeeded, 
							 IBindCtx * pbc, REFIID riid, void ** ppvObject)
{
	DEBUG("IOleItemContainer::GetObject");
	if (pszItem == NULL) return E_INVALIDARG;
	if (ppvObject == NULL) return E_INVALIDARG;

	*ppvObject = NULL;
	return MK_E_NOOBJECT;
}

HRESULT FS_IOleItemContainer::GetObjectStorage(LPOLESTR pszItem, IBindCtx * pbc, 
									REFIID riid, void ** ppvStorage)
{
	DEBUG("IOleItemContainer::GetObjectStorage");
	if (pszItem == NULL) return E_INVALIDARG;
	if (ppvStorage == NULL) return E_INVALIDARG;

	*ppvStorage = NULL;
	return MK_E_NOOBJECT;
}

HRESULT FS_IOleItemContainer::IsRunning(LPOLESTR pszItem)
{
	DEBUG("IOleItemContainer::IsRunning");
	if (pszItem == NULL) return E_INVALIDARG;

	return MK_E_NOOBJECT;
}

//IOleControlSite

HRESULT FS_IOleControlSite::OnControlInfoChanged()
{
	DEBUG("IOleControlSite::OnControlInfoChanged");
	return S_OK;
}

HRESULT FS_IOleControlSite::LockInPlaceActive(BOOL fLock)
{
	DEBUG("IOleControlSite::LockInPlaceActive");
	m_fs->m_bInPlaceLocked = (fLock) ? true : false;
	return S_OK;
}

HRESULT FS_IOleControlSite::GetExtendedControl(IDispatch ** ppDisp)
{
	DEBUG("IOleControlSite::GetExtendedControl");
	return E_NOTIMPL;
}

HRESULT FS_IOleControlSite::TransformCoords(POINTL * pPtlHimetric, POINTF * pPtfContainer, DWORD dwFlags)
{
	DEBUG("IOleControlSite::TransformCoords");
	HRESULT hr = S_OK;

	if (pPtlHimetric == NULL)
	{
		return E_INVALIDARG;
	}
	if (pPtfContainer == NULL)
	{
		return E_INVALIDARG;
	}

	HDC hdc = ::GetDC(m_fs->m_hWndParent);
	::SetMapMode(hdc, MM_HIMETRIC);

	POINT rgptConvert[2];
	rgptConvert[0].x = 0;
	rgptConvert[0].y = 0;

	if (dwFlags & XFORMCOORDS_HIMETRICTOCONTAINER)
	{
		rgptConvert[1].x = pPtlHimetric->x;
		rgptConvert[1].y = pPtlHimetric->y;
		::LPtoDP(hdc, rgptConvert, 2);
		if (dwFlags & XFORMCOORDS_SIZE)
		{
			pPtfContainer->x = (float)(rgptConvert[1].x - rgptConvert[0].x);
			pPtfContainer->y = (float)(rgptConvert[0].y - rgptConvert[1].y);
		}
		else if (dwFlags & XFORMCOORDS_POSITION)
		{
			pPtfContainer->x = (float)rgptConvert[1].x;
			pPtfContainer->y = (float)rgptConvert[1].y;
		}
		else
		{
			hr = E_INVALIDARG;
		}
	}
	else if (dwFlags & XFORMCOORDS_CONTAINERTOHIMETRIC)
	{
		rgptConvert[1].x = (int)(pPtfContainer->x);
		rgptConvert[1].y = (int)(pPtfContainer->y);
		::DPtoLP(hdc, rgptConvert, 2);
		if (dwFlags & XFORMCOORDS_SIZE)
		{
			pPtlHimetric->x = rgptConvert[1].x - rgptConvert[0].x;
			pPtlHimetric->y = rgptConvert[0].y - rgptConvert[1].y;
		}
		else if (dwFlags & XFORMCOORDS_POSITION)
		{
			pPtlHimetric->x = rgptConvert[1].x;
			pPtlHimetric->y = rgptConvert[1].y;
		}
		else
		{
			hr = E_INVALIDARG;
		}
	}
	else
	{
		hr = E_INVALIDARG;
	}

	::ReleaseDC(m_fs->m_hWndParent, hdc); 
	return hr;
}

HRESULT FS_IOleControlSite::TranslateAccelerator(LPMSG pMsg, DWORD grfModifiers)
{
	DEBUG("IOleControlSite::TranslateAccelerator");
	return E_NOTIMPL;
}

HRESULT FS_IOleControlSite::ShowContextMenu( DWORD dwID,POINT *ppt, IUnknown *pcmdtReserved, IDispatch *pdispReserved)
{
	DEBUG("IOleControlSite::ShowContextMenu");
	return S_OK;
}


HRESULT FS_IOleControlSite::OnFocus(BOOL fGotFocus)
{
	DEBUG("IOleControlSite::OnFocus");
	return S_OK;
}

HRESULT FS_IOleControlSite::ShowPropertyFrame()
{
	DEBUG("IOleControlSite::ShowPropertyFrame");
	return E_NOTIMPL;
}

//IOleCommandTarget

HRESULT FS_IOleCommandTarget::QueryStatus(const GUID * pguidCmdGroup, ULONG cCmds, 
							   OLECMD * prgCmds, OLECMDTEXT * pCmdTet)
{
	DEBUG("IOleCommandTarget::QueryStatus");
	if (prgCmds == NULL) return E_INVALIDARG;
	bool bCmdGroupFound = false;

	for (ULONG nCmd = 0; nCmd < cCmds; nCmd++)
	{
		// unsupported by default
		prgCmds[nCmd].cmdf = 0;

		// TODO
	}

	if (!bCmdGroupFound) { return OLECMDERR_E_UNKNOWNGROUP; }
	return S_OK;
}

HRESULT FS_IOleCommandTarget::Exec(const GUID * pguidCmdGroup, DWORD nCmdID, 
						DWORD nCmdExecOpt, VARIANTARG * pVaIn, 
						VARIANTARG * pVaOut)
{
	DEBUG("IOleCommandTarget::Exec");
	bool bCmdGroupFound = false;

	if (!bCmdGroupFound) { return OLECMDERR_E_UNKNOWNGROUP; }
	return OLECMDERR_E_NOTSUPPORTED;
}

//IAdviseSink

void STDMETHODCALLTYPE FS_IAdviseSink2::OnDataChange(FORMATETC * pFormatEtc, STGMEDIUM * pgStgMed)
{
	DEBUG("IAdviseSink::OnDataChange");
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnViewChange(DWORD dwAspect, LONG lIndex)
{
	DEBUG("IAdviseSink::OnViewChange");
	// redraw the control
	m_fs->m_IOleInPlaceSiteWindowless->InvalidateRect(NULL, FALSE);
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnRename(IMoniker * pmk)
{
	DEBUG("IAdviseSink::OnRename");
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnSave()
{
	DEBUG("IAdviseSink::OnSave");
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnClose()
{
	DEBUG("IAdviseSink::OnClose");
}

//IAdviseSink2

void STDMETHODCALLTYPE FS_IAdviseSink2::OnLinkSrcChange(IMoniker * pmk)
{
	DEBUG("IAdviseSink2::OnLinkSrcChange");
}

//IAdviseSinkEx

void STDMETHODCALLTYPE FS_IAdviseSinkEx::OnViewStatusChange(DWORD dwViewStatus)
{
	DEBUG("IAdviseSinkEx::OnViewStatusChange");
}


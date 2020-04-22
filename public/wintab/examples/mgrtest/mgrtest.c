/* ------------------------------- mgrtest.c -------------------------------- */

#include <windows.h>
//#include <winuser.h>
#include <string.h>
#include <wintab.h>

#include "mgrdlg.h"
#include "resource.h"
#include "msgpack.h"
#include "mgrtest.h"

HANDLE hInst;

/* application globals */
HMGR hMgr = NULL;
UINT ObtCat;
UINT ObtSize;
BOOL *ObtBuf;
HMENU hObtMenu = NULL;

UINT NDevices;

HMODULE hWintab = NULL;


/* If the exe imports WTMgrDefContextEx(), then we won't be able to run with
    older Wintab.dll/Wintab32.dll's which are don't support Wintab Spec 1.1.
    Instead, we'll try to GetProcAddress it ourselves. On failure, just disable
    features that depend on it. */
HCTX (API * pWTMgrDefContextEx)(HMGR, UINT, BOOL);

extern BOOL FAR PASCAL ButtonDlgProc(HWND, UINT, WPARAM, LPARAM);
void set_default_BtnMask( HWND hWnd, HMGR hMgr, int fSys ); /* BtnMask.c */
void set_default_CsrMask( HWND hWnd, HMGR hMgr, int fSys ); /* Csrmask.c */
void set_xBtnMap( HWND hWnd, HMGR hMgr ); /* btnMap.c */

/*------------------------------------------------------------------------------
encapsulate non-portable items:
	wintab string name
	LoadLibrary behavior
	Unicode/ANSI function name suffixes
------------------------------------------------------------------------------*/
#ifdef WIN32
	/* no Unicode support yet. */
	#define CHARSET	"A"
	char szWintab[] = "Wintab32";
	#define LoadLibraryWorked(h)	(h)
#else
	#define CHARSET
	char szWintab[] = "Wintab";
	#define LoadLibraryWorked(h)	(h >= HINSTANCE_ERROR)
#endif
/* -------------------------------------------------------------------------- */
/* portable wrappers for non-portable functions. */
/* -------------------------------------------------------------------------- */
BOOL ConfigReplace(HMGR h, BOOL f, LPSTR m, LPSTR p)
{
	typedef BOOL (API *CRX)(HMGR, int, LPSTR, LPSTR);
	typedef BOOL (API *CR)(HMGR, int, WTCONFIGPROC);
	static CR cr = NULL;
	static CRX crx = NULL;

	/* if not got wintab handle... */
	if (!hWintab)
		/* get wintab handle. */
		hWintab = LoadLibrary(szWintab);
	/* failsafe. */
	if (!LoadLibraryWorked(hWintab))
		return FALSE;

	/* if not got a proc... */
	if (!crx && !cr) {
		/* try for portable version. */
		crx = (CRX)GetProcAddress(hWintab, "WTMgrConfigReplaceEx" CHARSET);
		/* if no portable version... */
		if (!crx)
			/* try for non-portable version. */
			cr = (CR)GetProcAddress(hWintab, "WTMgrConfigReplace");
	}
	/* failsafe. */
	if (!crx && !cr)
		return FALSE;

	/* if portable version... */
	if (crx) {
		/* call it. */
		return crx(h, f, m, p);
	}
	else {
		/* convert arguments to call non-portable version. */
		static HMODULE curh = NULL;

		/* if args  and state legal for installing... */
		if (f && m && p && !curh) {
			/* try to get the library. */
			curh = LoadLibrary(m);
			/* if got library... */
			if (LoadLibraryWorked(curh)) {
				WTCONFIGPROC fp;

				/* try to get the proc. */
				fp = (WTCONFIGPROC)GetProcAddress(curh, p);
				/* if got the proc... */
				if (fp) {
					/* call the non-portable function to install. */
					f = cr(h, f, fp);
					/* if install failed... */
					if (!f) {
						/* free library and reset our state. */
						FreeLibrary(curh);
						curh = NULL;
					}
					return f;
				}
				else {
					/* no proc in the library -- free it and fail. */
					FreeLibrary(curh);
					return FALSE;
				}
			}
			else {
				/* couldn't load library -- fail. */
				return FALSE;
			}
		}
		else if (!f && curh) {
			/* args and state legal for removing -- try remove. */
			f = cr(h, f, NULL);
			/* if removal succeeded... */
			if (f) {
				/* free library and reset our state. */
				FreeLibrary(curh);
				curh = NULL;
			}
			return f;
		}
		else {
			/* args or state illegal. */
			return FALSE;
		}
	}
}
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
UINT ScanExts(UINT wTag)
{
	UINT i;
	UINT wScanTag;

	/* scan for wTag's info category. */
	for (i = 0; WTInfo(WTI_EXTENSIONS + i, EXT_TAG, &wScanTag); i++) {
		 if (wTag == wScanTag) {
			/* return category offset from WTI_EXTENSIONS. */
			return i;
		}
	}
	/* return error code. */
	return 0xFFFF;
}
/* -------------------------------------------------------------------------- */
BOOL ObtInit(void)
{
	ObtCat = ScanExts(WTX_OBT);
	if (ObtCat == 0xFFFF)
		return FALSE;

	ObtSize = WTInfo(WTI_EXTENSIONS + ObtCat, EXT_DEFAULT, NULL);
	if (ObtBuf = (BOOL *)LocalAlloc(LPTR, ObtSize)) {
		return TRUE;
	}
	return FALSE;
}
/* -------------------------------------------------------------------------- */
BOOL ObtGet(UINT wDev)
{
	WTInfo(WTI_EXTENSIONS + ObtCat, EXT_DEFAULT, ObtBuf);
	return ObtBuf[wDev];
}
/* -------------------------------------------------------------------------- */
BOOL ObtSet(UINT wDev, BOOL fOn)
{
	ObtBuf[wDev] = fOn;
	return WTMgrExt(hMgr, WTX_OBT, ObtBuf);
}
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
int PASCAL WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow)
HANDLE hInstance;
HANDLE hPrevInstance;
LPSTR lpCmdLine;
int nCmdShow;
{
    MSG msg;

    if (!hPrevInstance)
	if (!InitApplication(hInstance))
	    return (FALSE);

    /* Perform initializations that apply to a specific instance */

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

    /* Acquire and dispatch messages until a WM_QUIT message is received. */

    while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
    }
    return (msg.wParam);
}
/* -------------------------------------------------------------------------- */
BOOL InitApplication(hInstance)
HANDLE hInstance;
{
    WNDCLASS  wc;

    /* Fill in window class structure with parameters that describe the       */
    /* main window.                                                           */

    wc.style = 0;
    wc.lpfnWndProc = MainWndProc;

    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(1 + COLOR_APPWORKSPACE);
    wc.lpszMenuName =  "MgrTestMenu";
    wc.lpszClassName = "MgrTestWClass";

    /* Register the window class and return success/failure code. */

    return (RegisterClass(&wc));

}
/* -------------------------------------------------------------------------- */
BOOL InitInstance(hInstance, nCmdShow)
    HANDLE          hInstance;
    int             nCmdShow;
{
    HWND            hWnd;
	HMENU			hMenu, hCsrMenu;
	char *p;
	UINT size;
	int i;

    /* Save the instance handle in static variable, which will be used in  */
    /* many subsequence calls from this application to Windows.            */

    hInst = hInstance;

	/* check if WinTab available. */
	if (!WTInfo(0, 0, NULL)) {
		MessageBox(NULL, "WinTab Services Not Available.", "WinTab",
					MB_OK | MB_ICONHAND);
		return FALSE;
	}

    /* Create a main window for this application instance.  */

    hWnd = CreateWindow(
        "MgrTestWClass",
        "MgrTest Sample Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    /* If window could not be created, return "failure" */

    if (!hWnd) {
		if (!hMgr)
			MessageBox(NULL, "Can't get Manager Handle.", "MgrTest",
						MB_ICONHAND | MB_OK);
        return (FALSE);
	}

	/* get device count. */
	WTInfo(WTI_INTERFACE, IFC_NDEVICES, &NDevices);

	/* Tack on more menu items. */
	hMenu		= GetSubMenu(GetMenu(hWnd), IDM_EDIT);

	hCsrMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP, (UINT)hCsrMenu, "&Active Cursors");
	for (i = 0; size = WTInfo(WTI_CURSORS + i, CSR_NAME, NULL); i++) {
		if (p = (char *)LocalAlloc(LPTR, 1 + size)) {
			BOOL fActive;

			p[0] = '&';
			WTInfo(WTI_CURSORS + i, CSR_NAME, p + 1);

			AppendMenu(hCsrMenu, 0, IDM_CURSORS + i, p);
			LocalFree((HLOCAL)p);
			WTInfo(WTI_CURSORS + i, CSR_ACTIVE, &fActive);
			CheckMenuItem(hCsrMenu, IDM_CURSORS + i,
								(fActive ? MF_CHECKED : MF_UNCHECKED));
		}
	}

	hObtMenu = NULL;
	if (ObtInit()) {
		if (NDevices > 1) {
			hObtMenu = CreatePopupMenu();
			ModifyMenu(hMenu, IDM_OBT, MF_POPUP, (UINT)hObtMenu,
						"&Out of Bounds Tracking");
		}
		else {
			CheckMenuItem(hMenu, IDM_OBT,
				(ObtGet(0) ? MF_CHECKED : MF_UNCHECKED));
		}
	}
	else {
		EnableMenuItem(hMenu, IDM_OBT, MF_GRAYED);
	}

	AppendMenu(hMenu, MF_SEPARATOR, 0,				NULL);
	for (i = 0; size = WTInfo(WTI_DEVICES + i, DVC_NAME, NULL); i++) {
		static char suffix[] = " Settings...";

		if (p = (char *)LocalAlloc(LPTR, 1 + size + sizeof(suffix))) {

			p[0] = '&';
			WTInfo(WTI_DEVICES + i, DVC_NAME, p + 1);
			strtok(p, ";");

			if (hObtMenu)
				AppendMenu(hObtMenu, (ObtGet(i) ? MF_CHECKED : MF_UNCHECKED),
							IDM_OBTDEVS + i, p);

			strcat(p, suffix);

			AppendMenu(hMenu, 0, IDM_DEVICES + i, p);
			LocalFree((HLOCAL)p);
		}
	}




    /* Make the window visible; update its client area; and return "success" */

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return (TRUE);

}
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* statics for context list painting. */
static int nLine = 0;
static char buf[200];
static SIZE szTextExtent = {0};
static LOGCONTEXT lc;
static char ownertext[40];
static HCTX hCtxOrder[50];
static int nCtxs = 0;
/* -------------------------------------------------------------------------- */
BOOL FAR PASCAL Do1Context(HCTX hCtx, LPARAM lParam)
{
	HDC hDC = (HDC)lParam;
	char *p = buf;
	HWND hOwner;
	char status[30] = "";
	unsigned len;

	hCtxOrder[nLine] = hCtx;
	WTGet(hCtx, &lc);

	/* Decode status */
	if( lc.lcStatus & CXS_DISABLED )
		strcpy( status, "Disabled," );
	if( lc.lcStatus & CXS_OBSCURED )
		strcat( status, "Obscured," );
	if( lc.lcStatus & CXS_ONTOP )
		strcat( status, "On Top," );
	len = strlen( status );
	if( len ) /* Get rid of the last comma */
		status[len-1] = 0; 

	hOwner = WTMgrContextOwner(hMgr, hCtx);
	GetWindowText(hOwner, ownertext, 40);
	TextOut(hDC, 0, nLine * szTextExtent.cy, status, len - 1); /* Display status information */
	_itoa( lc.lcDevice, status, 10 );
	TextOut(hDC, 17*szTextExtent.cx, nLine*szTextExtent.cy, status, strlen(status) );
	wsprintf(p, "%s:%s", (LPSTR)lc.lcName, (LPSTR)ownertext);
	TextOut(hDC, 19 * szTextExtent.cx, nLine++ * szTextExtent.cy, buf, strlen(buf)); /* Display context name */

	return TRUE;
}
/* -------------------------------------------------------------------------- */
BOOL ListContexts(HDC hDC, PAINTSTRUCT *ps)
{
	static char info[] = "To edit a context, click on it in the above list.";
	BOOL fResult;
	FARPROC fp;

	if (!szTextExtent.cx)
		GetTextExtentPoint(hDC, "M", 1, &szTextExtent);
	nLine = 0;

	SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
	SetBkColor(hDC, GetSysColor(COLOR_APPWORKSPACE));

	fp = MakeProcInstance((FARPROC)Do1Context, hInst);
	fResult = WTMgrContextEnum(hMgr, (WTENUMPROC)fp, (LPARAM)hDC);
	FreeProcInstance(fp);

	nCtxs = nLine;


	TextOut(hDC, 0, (1 + nLine) * szTextExtent.cy, info, strlen(info));
	return fResult;
}
/* -------------------------------------------------------------------------- */
HCTX ListPoint(int y)
{
	int n = y / szTextExtent.cy;
	return ( n < nCtxs ? hCtxOrder[n] : NULL);
}
/* -------------------------------------------------------------------------- */
BOOL QueryKillCtx(HWND hWnd, HCTX hCtx)
{
	static char msg[] =
		"Closing this context may cause the owning application %s to crash."
		"Do you want to close it anyway?";
	HWND hOwner;

	hOwner = WTMgrContextOwner(hMgr, hCtx);
	if (IsWindow(hOwner)) {
		GetWindowText(hOwner, ownertext, 40);
		wsprintf(buf, msg, (LPSTR)ownertext);
		return (MessageBox(hWnd, buf, "MgrTest", MB_ICONSTOP|MB_OKCANCEL)==IDOK);
	}
	else
		return TRUE;
}
void set_default_device( HWND hWnd, int fSys )
{
	int id;
	HCTX hCtx;
	FARPROC lpProcDlg;

	lpProcDlg = MakeProcInstance(CursInfoDlgProc, hInst);
	id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
		hInst, lpProcDlg, WTI_DEVICES);
	FreeProcInstance(lpProcDlg);

	if( id >= 0 ) {
		LOGCONTEXT log;

		/* Change the default device 
		   (the device that is used by WTI_DEFCONTEXT, WTI_DEFSYSCTX and WTMgrDefContext) */
		hCtx = WTMgrDefContextEx(hMgr, id, fSys);
		if( !hCtx ) {
			MessageBox(hWnd, "WTMgrDefContextEx failed.", "MgrTest", MB_ICONHAND | MB_OK);
			return;
		}
		if( !WTGet( hCtx, &log ) ) {
			MessageBox(hWnd, "WTGet failed.", "MgrTest", MB_ICONHAND | MB_OK);
			return;
		}
		hCtx = WTMgrDefContext(hMgr,fSys);
		if( !WTSet( hCtx, &log ) ) {
			MessageBox(hWnd, "WTSet failed.", "MgrTest", MB_ICONHAND | MB_OK);
			return;
		}

		/* Test that an innocent WTSet won't inadvertently change defalt_device */
		if( id > 0 ) {
			log.lcDevice = 0;
			hCtx = WTOpen(hWnd, &log, 0);
			if( !hCtx ) {
				MessageBox(hWnd, "WTOpen failed.", "MgrTest", MB_ICONHAND | MB_OK);
				return;
			}
			if( !WTSet(hCtx, &log) )
				MessageBox(hWnd, "WTSet failed.", "MgrTest", MB_ICONHAND | MB_OK);
			if( !WTClose(hCtx) )
				MessageBox(hWnd, "WTClose failed.", "MgrTest", MB_ICONHAND | MB_OK);
		}

		/* Test that the change was actually made */
		hCtx = WTMgrDefContext(hMgr, fSys);
		if( !WTGet( hCtx, &log ) ) {
			MessageBox(hWnd, "WTGet failed.", "MgrTest", MB_ICONHAND | MB_OK);
			return;
		}
		if( (int)log.lcDevice == id )
			MessageBox(hWnd, "Default device changed.", "MgrTest", MB_ICONINFORMATION | MB_OK);
		else
			MessageBox(hWnd, "Default device not changed properly.", "MgrTest", MB_ICONHAND | MB_OK);

	}
}
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
BOOL 
CALLBACK ctx_edit_DlgProc( HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam )
{
	BOOL fResult;

	switch( Msg ) {
	case WM_COMMAND:
		EndDialog(hDlg, wParam);
		fResult = TRUE;
		break;
	default:
		fResult = FALSE;
	}
	return fResult;
}
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* If we can't link WTMgrDefContextEx(), use this instead */
HCTX API autofail(HMGR a, UINT b, BOOL c)
{
	return 0;
}
/* -------------------------------------------------------------------------- */
LRESULT FAR PASCAL MainWndProc(hWnd, message, wParam, lParam)
HWND hWnd;
unsigned message;
WPARAM wParam;
LPARAM lParam;
{
	HCTX hCtx;
    FARPROC lpProcDlg;
	LRESULT lResult = 0;
	static BOOL fCBRTest = FALSE;
	BOOL fEnable;
	HMENU hMenu;
	int i;
	WORD id,specver;
	HMODULE hModule;

    switch (message) {
		case WM_CREATE:
			hMgr = WTMgrOpen(hWnd, WT_DEFBASE);
			if( !hMgr )
				MessageBox(hWnd, "WTMgrOpen failed.", "MgrTest", MB_ICONHAND | MB_OK);
			lResult = !!hMgr - 1;
			
			
			/* Try to link WTMgrDefContextEx() */
			hModule = GetModuleHandle(
#ifdef _WIN32
										"wintab32.dll"
#else
										"wintab.dll"
#endif		
										);
			(FARPROC)pWTMgrDefContextEx = GetProcAddress( hModule, "WTMgrDefContextEx" );
			if( !pWTMgrDefContextEx ) {
				/* Disable features which depend on WTMgrDefContextEx */
				pWTMgrDefContextEx = autofail;
				EnableWindow( GetDlgItem(hWnd, IDM_DEFDEV_DIG), FALSE );
				EnableWindow( GetDlgItem(hWnd, IDM_DEFDEV_SYS), FALSE );
			}
			break;

		case WT_CTXOPEN:
		case WT_CTXCLOSE:
		case WT_CTXUPDATE:
		case WT_CTXOVERLAP:
		case WT_PROXIMITY:
			InvalidateRect(hWnd, NULL, TRUE);
			UpdateWindow(hWnd);
			break;

		case WT_INFOCHANGE:
			FlashWindow(hWnd, TRUE);
			hMenu = GetSubMenu(GetSubMenu(GetMenu(hWnd),IDM_EDIT), IDM_CSRMENU);
			if (hMenu) {
				for (i = 0; WTInfo(WTI_CURSORS+i, CSR_ACTIVE, &fEnable); i++) {
					CheckMenuItem(hMenu, IDM_CURSORS + i,
									(fEnable ? MF_CHECKED : MF_UNCHECKED));
				}
			}
			FlashWindow(hWnd, FALSE);
			InvalidateRect(hWnd, NULL, TRUE);
			UpdateWindow(hWnd);

			break;

		case WM_LBUTTONDOWN:
			hCtx = ListPoint(HIWORD(lParam));

			if( hCtx ) {
				LOGCONTEXT lc;

				lpProcDlg = MakeProcInstance(ctx_edit_DlgProc, hInst);
				id = DialogBox(hInst, MAKEINTRESOURCE(IDD_CTXEDIT), hWnd, lpProcDlg);
				FreeProcInstance(lpProcDlg);
				
				switch( id ) {
				case IDC_WTCONFIG:
					WTConfig(hCtx, hWnd);
					break;

				case IDC_BUTTONS:
					WTGet(hCtx, &lc);
					set_ctx_BtnMask(hWnd, hCtx, &lc);
					break;

				case IDC_MOVEMASK:
					set_ctx_MoveMask(hWnd, hMgr, hCtx);
					break;
				}
			}
			break;

		case WM_RBUTTONDOWN:
			hCtx = ListPoint(HIWORD(lParam));
			if (QueryKillCtx(hWnd, hCtx)) {
				WTClose(hCtx);
			}
			break;


		case WM_PAINT:
			if (hMgr) {
			 	HDC hDC;
				PAINTSTRUCT ps;

				hDC = BeginPaint(hWnd, &ps);

				ListContexts(hDC, &ps);

				EndPaint(hWnd, &ps);
			}
			break;

		case WM_COMMAND:
			id = GET_WM_COMMAND_ID(wParam, lParam);

			if (id >= IDM_DEVICES && 
				WTInfo(WTI_DEVICES + id - IDM_DEVICES, DVC_NAME, NULL))
			{
				WTMgrDeviceConfig(hMgr, id - IDM_DEVICES, hWnd);
			}

			if (id >= IDM_CURSORS && 
				WTInfo(WTI_CURSORS+id-IDM_CURSORS, CSR_ACTIVE, &fEnable))
			{
				fEnable ^= WTMgrCsrEnable(hMgr, id - IDM_CURSORS, !fEnable);
				hMenu = GetSubMenu(GetSubMenu(GetMenu(hWnd), IDM_EDIT),
									IDM_CSRMENU);
				CheckMenuItem(hMenu, id,
						 (fEnable ? MF_CHECKED : MF_UNCHECKED));
			}

			if (id >= IDM_OBTDEVS &&
				id < (WORD)(IDM_OBTDEVS + NDevices)) {

				/* one of multiple devices. */
				ObtSet(id - IDM_OBTDEVS, !ObtGet(id - IDM_OBTDEVS));
				CheckMenuItem(hObtMenu, id,
						(ObtGet(0) ? MF_CHECKED : MF_UNCHECKED));
			}

			switch (id)
			{
				case IDM_ABOUT:
					lpProcDlg = MakeProcInstance(About, hInst);
					DialogBox(hInst, "AboutBox", hWnd, lpProcDlg);
					FreeProcInstance(lpProcDlg);
					break;

				case IDM_BUTTMAPS:
					lpProcDlg = MakeProcInstance(ButtonDlgProc, hInst);
					DialogBox(hInst, MAKEINTRESOURCE(IDD_BUTTONS),
						hWnd, lpProcDlg);
					FreeProcInstance(lpProcDlg);
					break;

				case IDM_XBUTTMAPS:
					set_xBtnMap( hWnd, hMgr );
					break;

				case IDM_OBT:
					/* only one device present. */
					ObtSet(0, !ObtGet(0));
					hMenu = GetSubMenu(GetMenu(hWnd), IDM_EDIT);
					CheckMenuItem(hMenu, id,
						 	(ObtGet(0) ? MF_CHECKED : MF_UNCHECKED));
					break;

				case IDM_DEFDIG:

					/* Open a dialog to choose which device to use, if nessicary */
					lpProcDlg = MakeProcInstance(CursInfoDlgProc, hInst);
					id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
								hWnd, lpProcDlg, WTI_DDCTXS);
					FreeProcInstance(lpProcDlg);

					/* Open the Wintab context config dialog */
					if (id >= 0) {
						int numDevices;

						WTInfo(WTI_INTERFACE, IFC_NDEVICES, &numDevices);

						if( id < numDevices ) {
							hCtx = WTMgrDefContextEx(hMgr, id, 0);
							if( !hCtx )
								hCtx = WTMgrDefContext(hMgr, 0);
						} else
							hCtx = WTMgrDefContext(hMgr, 0);

						if( hCtx )
								WTConfig(hCtx, hWnd);
						else
							MessageBox(hWnd, "WTMgrDefContext failed.", "MgrTest",
										MB_ICONHAND | MB_OK);
					}

					break;

					
				case IDM_DEFSYS:
	
					/* Open a dialog to choose which device to use, if nessicary */
					lpProcDlg = MakeProcInstance(CursInfoDlgProc, hInst);
					id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
								hInst, lpProcDlg, WTI_DSCTXS);
					FreeProcInstance(lpProcDlg);

					/* Open the Wintab context dialog */
					if (id >= 0) {
						int numDevices;

						WTInfo(WTI_INTERFACE, IFC_NDEVICES, &numDevices);

						if( id < numDevices ) {
							hCtx = WTMgrDefContextEx(hMgr, id, 1);
							if( !hCtx )
								hCtx = WTMgrDefContext(hMgr, 1);
						} else
							hCtx = WTMgrDefContext(hMgr, 1);
						/* 'Default Device' was the last choice in the dialog */

						if( hCtx )
								WTConfig(hCtx, hWnd);
						else
							MessageBox(hWnd, "WTMgrDefContext failed.", "MgrTest",
										MB_ICONHAND | MB_OK);
					}
					break;


					
				case IDM_RESET_DEFDIG:
		    		//Check for version 1.1
					WTInfo(WTI_INTERFACE,IFC_SPECVERSION,&specver);
					if( ( HIBYTE(specver)>=1) && ( LOBYTE(specver)>=1)){
						lpProcDlg = MakeProcInstance(CursInfoDlgProc, hInst);
						id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
								hInst, lpProcDlg, WTI_DDCTXS);
						FreeProcInstance(lpProcDlg);
						if (id >= 0) {
							int numDevices;
							WTInfo(WTI_INTERFACE, IFC_NDEVICES, &numDevices);
						
							if( id < numDevices ) {
								hCtx = WTMgrDefContextEx(hMgr, id, 0);
								if( !hCtx )
									hCtx = WTMgrDefContext(hMgr, 0);
							} else
								hCtx = WTMgrDefContext(hMgr, 0);
			
							if( hCtx ) {
								if( WTSet(hCtx, 0) )
									MessageBox(hWnd, "Error! WTSet(hCtx, 0)"
											   "returned success.", "MgrTest",
											   MB_ICONHAND | MB_OK);
								if( !WTSet(hCtx, WTP_LPDEFAULT) )
									MessageBox(hWnd, "WTSet failed.", "MgrTest",
											   MB_ICONHAND | MB_OK);
								else
									MessageBox(hWnd, "WTSet succeeded.", "MgrTest",
											   MB_OK | MB_ICONINFORMATION);
							}else
								MessageBox(hWnd, "WTMgrDefContext failed.", "MgrTest",
								MB_ICONHAND | MB_OK);
						}
					}else
						MessageBox(hWnd, "This feature is only supported in "
								   "devices using Wintab specification 1.1 and later.", 
								   "MgrTest", MB_ICONHAND | MB_OK);
					break;


				case IDM_RESET_DEFSYS:
					//Check for version 1.1
					WTInfo(WTI_INTERFACE,IFC_SPECVERSION,&specver);
					if( ( HIBYTE(specver)>=1) && ( LOBYTE(specver)>=1)){
						lpProcDlg = MakeProcInstance(CursInfoDlgProc, hInst);
						id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
											hInst, lpProcDlg, WTI_DSCTXS);
						FreeProcInstance(lpProcDlg);

						if (id >= 0) {
							int numDevices;
							WTInfo(WTI_INTERFACE, IFC_NDEVICES, &numDevices);

							if( id < numDevices ) {
								hCtx = WTMgrDefContextEx(hMgr, id, 1);
								if( !hCtx )
									hCtx = WTMgrDefContext(hMgr, 1);
							} else
								hCtx = WTMgrDefContext(hMgr, 1);
							if( hCtx ) {
								if( WTSet(hCtx, 0) )
									MessageBox(hWnd, "Error! WTSet(hCtx, 0) "
											   "returned success.", "MgrTest", MB_ICONHAND | MB_OK);
								if( !WTSet(hCtx, WTP_LPDEFAULT) )
									MessageBox(hWnd, "WTSet failed.", "MgrTest",
											   MB_ICONHAND | MB_OK);
								else
									MessageBox(hWnd, "WTSet succeeded.", "MgrTest",
											   MB_OK | MB_ICONINFORMATION);
							} else
								MessageBox(hWnd, "WTMgrDefContext failed.", "MgrTest",
										   MB_ICONHAND | MB_OK);
						}
					}else
						MessageBox(hWnd, "This feature is only supported in "
								   "devices using Wintab specification 1.1 and later.", 
								   "MgrTest", MB_ICONHAND | MB_OK);

					break;


				case IDM_DEFDEV_DIG:
					//Check for version 1.1
						WTInfo(WTI_INTERFACE,IFC_SPECVERSION,&specver);
						if( ( HIBYTE(specver)>=1) && ( LOBYTE(specver)>=1)){
							set_default_device( hWnd, 0 );
						}else{
							MessageBox(hWnd, "This feature is only supported in "
									   "devices using Wintab specification 1.1 and later.", 
									   "MgrTest", MB_ICONHAND | MB_OK);
						}
					break;
				case IDM_DEFDEV_SYS:
					//Check for version 1.1
						WTInfo(WTI_INTERFACE,IFC_SPECVERSION,&specver);
						if( ( HIBYTE(specver)>=1) && ( LOBYTE(specver)>=1)){
							set_default_device( hWnd, 1 );
						}else{
							MessageBox(hWnd, "This feature is only supported in "
									   "devices using Wintab specification 1.1 and later.", 
									   "MgrTest", MB_ICONHAND | MB_OK);
						}
					break;

				case IDM_CSRMASK_DIG:
					set_default_CsrMask( hWnd, hMgr, 0 );
					break;

				case IDM_CSRMASK_SYS:
					set_default_CsrMask( hWnd, hMgr, 1 );
					break;

				case IDM_XBTN_DIG:
					set_default_BtnMask( hWnd, hMgr, 0 );
					break;
				case IDM_XBTN_SYS:
					set_default_BtnMask( hWnd, hMgr, 1 );
					break;

				case IDM_CBRTEST:
					if (ConfigReplace(hMgr, !fCBRTest,
											"MgrDLL.DLL", "CBRTestProc")) {
						fCBRTest = !fCBRTest;
					}
					CheckMenuItem(GetSubMenu(GetMenu(hWnd), IDM_TEST),
						IDM_CBRTEST, (fCBRTest ? MF_CHECKED : MF_UNCHECKED));
					break;

				case IDM_BMSTEST:
					BMSTest(hWnd);
					break;

				case IDM_PMSTEST:
					PMSTest(hWnd);
					break;

				case IDM_PRSTEST:
					PRSTest(hWnd);
					break;

				case IDM_HMGRTEST:
					HMGRTest(hWnd);
					break;

				default:
					return (DefWindowProc(hWnd, message, wParam, lParam));
			}
			break;

		case WM_DESTROY:
			if (fCBRTest) {
				ConfigReplace(hMgr, FALSE, NULL, NULL);
			}
			if (hMgr)
				WTMgrClose(hMgr);
			if (LoadLibraryWorked(hWintab))
				FreeLibrary(hWintab);
	    	PostQuitMessage(0);
	    	break;

		default:
	    	return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return lResult;
}
/* -------------------------------------------------------------------------- */
BOOL FAR PASCAL About(hDlg, message, wParam, lParam)
HWND hDlg;
unsigned message;
WPARAM wParam;
LPARAM lParam;
{
    switch (message) {
		case WM_INITDIALOG:
	    	return (TRUE);

		case WM_COMMAND:
	    if (GET_WM_COMMAND_ID(wParam, lParam) == IDOK                
                || GET_WM_COMMAND_ID(wParam, lParam) == IDCANCEL) {      
				EndDialog(hDlg, TRUE);
				return (TRUE);
	    	}
	    	break;
    }
    return (FALSE);
}
/* -------------------------------------------------------------------------- */


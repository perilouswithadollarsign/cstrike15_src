/*------------------------------------------------------------------------------
SysPress - using pressure input with a system context.
RICO 6/7/93
------------------------------------------------------------------------------*/

#include <windows.h>
#include "msgpack.h"
#include <wintab.h>
#define PACKETDATA	(PK_CURSOR | PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE)
#define PACKETMODE	0
#include <pktdef.h>
#include "syspress.h"
#ifdef USE_X_LIB
#include <wintabx.h>
#endif

HANDLE hInst;

#define NPACKETQSIZE	32	/* set as needed for your app. */
PACKET localPacketBuf[NPACKETQSIZE];

static HCTX hTab = NULL;

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

	while (GetMessage(&msg,
		NULL,
		0,
		0))
	{
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
	wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
	wc.lpszMenuName =  "SysPressMenu";
	wc.lpszClassName = "SysPressWClass";

	/* Register the window class and return success/failure code. */

	return (RegisterClass(&wc));

}
/* -------------------------------------------------------------------------- */
BOOL InitInstance(hInstance, nCmdShow)
	HANDLE          hInstance;
	int             nCmdShow;
{
	HWND            hWnd;
	char buf[50];

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

	wsprintf(buf, "SysPress:%x", hInst);
	hWnd = CreateWindow(
		"SysPressWClass",
		buf,
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

	if (!hWnd)
		return (FALSE);

	/* Make the window visible; update its client area; and return "success" */

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return (TRUE);

}
/* -------------------------------------------------------------------------- */
HCTX static NEAR TabletInit(HWND hWnd)
{
	LOGCONTEXT lcMine;
	HCTX hResult;

	/* get default system context. */
	WTInfo(WTI_DEFSYSCTX, 0, &lcMine);

	/* modify the digitizing context */
	wsprintf(lcMine.lcName, "SysPress Digitizing %x", hInst);

	/* same define constants from top of file used in context definition. */
	lcMine.lcPktData = PACKETDATA;
	lcMine.lcPktMode = PACKETMODE;
	lcMine.lcMoveMask = PACKETDATA;

	/* inherit default button down mask. */
	/* allows tablet control panel to reserve some buttons for special */
	/* functions. */
	/* make sure we get button up AND down reports for all buttons we watch. */
	lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;

	/* x & y match mouse points. */
	lcMine.lcOutOrgX = 0;
	lcMine.lcOutExtX = GetSystemMetrics(SM_CXSCREEN);
	lcMine.lcOutOrgY = 0;
	/* the negative sign here reverses the sense of the Y axis. */
	/* WinTab uses a lower-left origin; MM_TEXT uses upper left. */
	lcMine.lcOutExtY = -GetSystemMetrics(SM_CYSCREEN);

	/* open the context */
	hResult = WTOpen(hWnd, &lcMine, TRUE);

	/* Q size set needs error-checking, but skipped here for */
	/* simplicity's sake. */
	WTQueueSizeSet(hResult, NPACKETQSIZE);

	return hResult;
}
/*------------------------------------------------------------------------------
The functions PrsInit and PrsAdjust make sure that our pressure out can always
reach the full 0-255 range we desire, regardless of the button pressed or the
"pressure button marks" settings.
------------------------------------------------------------------------------*/
/* pressure adjuster local state. */
/* need wOldCsr = -1, so PrsAdjust will call PrsInit first time */
static UINT wActiveCsr = 0,  wOldCsr = (UINT)-1;
static BYTE wPrsBtn;
static UINT prsYesBtnOrg, prsYesBtnExt, prsNoBtnOrg, prsNoBtnExt;
/* -------------------------------------------------------------------------- */
void PrsInit(void)
{
	/* browse WinTab's many info items to discover pressure handling. */
	AXIS np;
	LOGCONTEXT lc;
	BYTE logBtns[32];
	UINT btnMarks[2];
	UINT size;

	/* discover the LOGICAL button generated by the pressure channel. */
	/* get the PHYSICAL button from the cursor category and run it */
	/* through that cursor's button map (usually the identity map). */
	wPrsBtn = (BYTE)-1;
	WTInfo(WTI_CURSORS + wActiveCsr, CSR_NPBUTTON, &wPrsBtn);
	size = WTInfo(WTI_CURSORS + wActiveCsr, CSR_BUTTONMAP, &logBtns);
	if ((UINT)wPrsBtn < size)
		wPrsBtn = logBtns[wPrsBtn];

	/* get the current context for its device variable. */
	WTGet(hTab, &lc);

	/* get the size of the pressure axis. */
	WTInfo(WTI_DEVICES + lc.lcDevice, DVC_NPRESSURE, &np);
	prsNoBtnOrg = (UINT)np.axMin;
	prsNoBtnExt = (UINT)np.axMax - (UINT)np.axMin;

	/* get the button marks (up & down generation thresholds) */
	/* and calculate what pressure range we get when pressure-button is down. */
	btnMarks[1] = 0; /* default if info item not present. */
	WTInfo(WTI_CURSORS + wActiveCsr, CSR_NPBTNMARKS, btnMarks);
	prsYesBtnOrg = btnMarks[1];
	prsYesBtnExt = (UINT)np.axMax - btnMarks[1];
}
/* -------------------------------------------------------------------------- */
UINT PrsAdjust(PACKET p)
{
	UINT wResult;

	wActiveCsr = p.pkCursor;
	if (wActiveCsr != wOldCsr) {

		/* re-init on cursor change. */
		PrsInit();
		wOldCsr = wActiveCsr;
	}

	/* scaling output range is 0-255 */

	if (p.pkButtons & (1 << wPrsBtn)) {
		/* if pressure-activated button is down, */
		/* scale pressure output to compensate btn marks */
		wResult = p.pkNormalPressure - prsYesBtnOrg;
		wResult = MulDiv(wResult, 255, prsYesBtnExt);
	}
	else {
		/* if pressure-activated button is not down, */
		/* scale pressure output directly */
		wResult = p.pkNormalPressure - prsNoBtnOrg;
		wResult = MulDiv(wResult, 255, prsNoBtnExt);
	}

	return wResult;
}
/* -------------------------------------------------------------------------- */
LRESULT FAR PASCAL MainWndProc(hWnd, message, wParam, lParam)
HWND hWnd;
unsigned message;
WPARAM wParam;
LPARAM lParam;
{
	FARPROC lpProcAbout;
	static POINT ptOrg;
	static POINT ptOld, ptNew;
	static UINT prsOld, prsNew;
	static DWORD btnOld, btnNew;
	HDC hDC;
	int nPackets;
	BOOL fHandled = TRUE;
	LRESULT lResult = 0L;


	switch (message) {

		case WM_CREATE:
			hTab = TabletInit(hWnd);
			if (!hTab) {
				MessageBox(NULL, " Could Not Open Tablet Context.", "WinTab",
							MB_OK | MB_ICONHAND);

				SendMessage(hWnd, WM_DESTROY, 0, 0L);
			}
			break;

		case WM_SIZE:
		case WM_MOVE:
			ptOrg.x = ptOrg.y = 0;
			ClientToScreen(hWnd, &ptOrg);
			InvalidateRect(hWnd, NULL, TRUE);
			break;

		case WM_ACTIVATE:
			/* This call puts your context (active area) on top the */
			/* context overlap order. */
			if (hTab && GET_WM_ACTIVATE_STATE(wParam, lParam))
				WTOverlap(hTab, TRUE);
			break;

		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
			/* ANY mouse msg */

			if (IsIconic(hWnd))
				break;

			/* empty the queue to a local buffer. */
			if (nPackets = WTPacketsGet(hTab, NPACKETQSIZE, &localPacketBuf)) {
				int i;

				if (hDC = GetDC(hWnd)) {
					HBRUSH hBrOld = NULL;
					HPEN hPenOld;

					hPenOld = SelectObject(hDC, GetStockObject(NULL_PEN));

					for (i = 0; i < nPackets; i++) {
						DWORD btnChange;

						btnOld = btnNew;
						btnNew = localPacketBuf[i].pkButtons;
						btnChange = btnOld ^ btnNew;

						if (btnNew & btnChange) {
							/* a button went down. */
							MessageBeep(0);
						}

						if (btnNew) {

							/* save last packet's data. */
							ptOld = ptNew;
							prsOld = prsNew;

							/* get new point. */
							ptNew.x = (UINT)localPacketBuf[i].pkX;
							ptNew.y = (UINT)localPacketBuf[i].pkY;

							/* translate coordinates to client area. */
							ptNew.x -= ptOrg.x;
							ptNew.y -= ptOrg.y;

							prsNew = PrsAdjust(localPacketBuf[i]);
							if (hBrOld == NULL) {
							 	hBrOld = SelectObject(hDC,
							 		CreateSolidBrush(
							 			RGB(prsNew, prsNew, prsNew)));
							}
							else if (prsNew != prsOld) {
								DeleteObject(SelectObject(hDC, hBrOld));
							 	hBrOld = SelectObject(hDC,
							 		CreateSolidBrush(
							 			RGB(prsNew, prsNew, prsNew)));
							}
							if (ptNew.x != ptOld.x ||
								ptNew.y != ptOld.y ||
								prsNew != prsOld) {
									Ellipse(hDC, ptNew.x - 3, ptNew.y - 3,
											ptNew.x + 3, ptNew.y + 3);
							}
						}
					}
					SelectObject(hDC, hPenOld);
					if (hBrOld)
						DeleteObject(SelectObject(hDC, hBrOld));
					ReleaseDC(hWnd, hDC);
				}
			}

			break;

		case WM_COMMAND:
			if (GET_WM_COMMAND_ID(wParam, lParam) == IDM_ABOUT) {
				lpProcAbout = MakeProcInstance(About, hInst);

				DialogBox(hInst,
					"AboutBox",
					hWnd,
					lpProcAbout);

				FreeProcInstance(lpProcAbout);
			}
			else
				fHandled = FALSE;
			break;

		case WT_INFOCHANGE:		/* IF some info item changed */
			PrsInit();		/* update the pressure mapping state vars. */
			break;

		case WM_DESTROY:
			if (hTab)
				WTClose(hTab);
#ifdef USE_X_LIB
			_UnlinkWintab();
#endif
			PostQuitMessage(0);
			break;

		default:
			fHandled = FALSE;
			break;
	}
	if (fHandled)
		return (lResult);
	else
		return (DefWindowProc(hWnd, message, wParam, lParam));
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

/*------------------------------------------------------------------------------
PrsTest - using pressure input.
RICO 4/1/92
------------------------------------------------------------------------------*/

#include <windows.h>
#include "msgpack.h"
#include <wintab.h>
#define PACKETDATA	(PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE)
#define PACKETMODE	PK_BUTTONS
#include <pktdef.h>
#include "prstest.h"
#ifdef USE_X_LIB
#include <wintabx.h>
#endif

HANDLE hInst;


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

#ifdef USE_X_LIB
	_UnlinkWintab();
#endif

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
	wc.lpszMenuName =  "PrsTestMenu";
	wc.lpszClassName = "PrsTestWClass";

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

	wsprintf(buf, "PrsTest:%x", hInst);
	hWnd = CreateWindow(
		"PrsTestWClass",
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

	/* get default region */
	WTInfo(WTI_DEFCONTEXT, 0, &lcMine);

	/* modify the digitizing region */
	wsprintf(lcMine.lcName, "PrsTest Digitizing %x", hInst);
	lcMine.lcOptions |= CXO_MESSAGES;
	lcMine.lcPktData = PACKETDATA;
	lcMine.lcPktMode = PACKETMODE;
	lcMine.lcMoveMask = PACKETDATA;
	lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;

	/* output in 10000 x 10000 grid */
	lcMine.lcOutOrgX = lcMine.lcOutOrgY = 0;
	lcMine.lcOutExtX = 10000;
	lcMine.lcOutExtY = 10000;

	/* open the region */
	return WTOpen(hWnd, &lcMine, TRUE);

}
/* -------------------------------------------------------------------------- */
LRESULT FAR PASCAL MainWndProc(hWnd, message, wParam, lParam)
HWND hWnd;
unsigned message;
WPARAM wParam;
LPARAM lParam;
{
	FARPROC lpProcAbout;
	static HCTX hTab = NULL;
	static POINT ptOld, ptNew;
	static UINT prsOld, prsNew;
	static RECT rcClient;
	PAINTSTRUCT psPaint;
	HDC hDC;
	PACKET pkt;
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
			GetClientRect(hWnd, &rcClient);
			InvalidateRect(hWnd, NULL, TRUE);
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

		case WT_PACKET:
			if (WTPacket((HCTX)lParam, wParam, &pkt)) {
				if (HIWORD(pkt.pkButtons)==TBN_DOWN) {
					MessageBeep(0);
				}
				ptOld = ptNew;
				prsOld = prsNew;
				ptNew.x = MulDiv((UINT)pkt.pkX, rcClient.right, 10000);
				ptNew.y = MulDiv((UINT)pkt.pkY, rcClient.bottom, 10000);
				prsNew = pkt.pkNormalPressure;
				if (ptNew.x != ptOld.x ||
					ptNew.y != ptOld.y ||
					prsNew != prsOld) {
					InvalidateRect(hWnd, NULL, TRUE);
				}
			}
			break;

		case WM_ACTIVATE:
			if (GET_WM_ACTIVATE_STATE(wParam, lParam))
				InvalidateRect(hWnd, NULL, TRUE);

			/* if switching in the middle, disable the region */
			if (hTab) {
				WTEnable(hTab, GET_WM_ACTIVATE_STATE(wParam, lParam));
				if (hTab && GET_WM_ACTIVATE_STATE(wParam, lParam))
					WTOverlap(hTab, TRUE);
			}
			break;

		case WM_DESTROY:
			if (hTab)
				WTClose(hTab);
			PostQuitMessage(0);
			break;

		case WM_PAINT:
			if (hDC = BeginPaint(hWnd, &psPaint)) {
				POINT ptHere;

				ptHere.x = ptNew.x;
				ptHere.y = rcClient.bottom - ptNew.y;

				/* redo horz */
				Ellipse(hDC, ptHere.x - prsNew, ptHere.y - prsNew,
						ptHere.x + prsNew, ptHere.y + prsNew);
				PatBlt(hDC, rcClient.left, ptHere.y,
						rcClient.right, 1, DSTINVERT);
				/* redo vert */
				PatBlt(hDC, ptHere.x, rcClient.top,
						1, rcClient.bottom, DSTINVERT);

				EndPaint(hWnd, &psPaint);
			}
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


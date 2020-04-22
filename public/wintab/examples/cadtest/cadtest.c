/*------------------------------------------------------------------------------
CadTest - example of how a cad program might use WinTab.
RICO 4/1/92
------------------------------------------------------------------------------*/

#include <string.h>
#include <windows.h>
#include "msgpack.h"
#include <commdlg.h>
#include <wintab.h>
#define PACKETDATA	(PK_X | PK_Y | PK_BUTTONS)
#define PACKETMODE	PK_BUTTONS
#include <pktdef.h>
#include "cadtest.h"
#ifdef USE_X_LIB
#include <wintabx.h>
#endif

HANDLE hInst;

#ifdef WIN32
	#define GetID()	GetCurrentProcessId()
#else
	#define GetID()	hInst
#endif


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
	wc.lpszMenuName =  "CadTestMenu";
	wc.lpszClassName = "CadTestWClass";

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

	wsprintf(buf, "CadTest:%x", GetID());
	hWnd = CreateWindow(
		"CadTestWClass",
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
/* open and save file data. */
char szFile[256] = "cadtest.ctx";
/* -------------------------------------------------------------------------- */
/* enforces context rules even if we didn't save the context. */
void static NEAR TabletSetup(PLOGCONTEXT pLc)
{
	/* modify the digitizing region */
	wsprintf(pLc->lcName, "CadTest Digitizing %x", GetID());
	pLc->lcOptions |= CXO_MESSAGES;
	pLc->lcMsgBase = WT_DEFBASE;
	pLc->lcPktData = PACKETDATA;
	pLc->lcPktMode = PACKETMODE;
	pLc->lcMoveMask = PACKETDATA;
	pLc->lcBtnUpMask = pLc->lcBtnDnMask;

	/* output in 10000 x 10000 grid */
	pLc->lcOutOrgX = pLc->lcOutOrgY = 0;
	pLc->lcOutExtX = 10000;
	pLc->lcOutExtY = 10000;
}
/* -------------------------------------------------------------------------- */
HCTX static TabletRestore(HCTX hCtx, HWND hWnd)
{
	void *save_buf;
	UINT save_size;
	HFILE hFile;
	OFSTRUCT ofs;
	LOGCONTEXT lc;

	/* alloc a save buffer. */
	WTInfo(WTI_INTERFACE, IFC_CTXSAVESIZE, &save_size);
	if (save_buf = (void *)LocalAlloc(LPTR, save_size))
	{
		OPENFILENAME ofn;

		/* do open file box. */
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFilter = "Context Files\0*.ctx\0";
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.Flags = OFN_FILEMUSTEXIST |OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = "ctx";

		if (GetOpenFileName(&ofn)) {

			/* open the file. */
			if ((hFile = OpenFile(szFile, &ofs, OF_READ)) != HFILE_ERROR)
			{
				/* read in the data. */
				_lread(hFile, save_buf, save_size);

				/* close the file. */
				_lclose(hFile);
			}

			/* restore the context, disabled. */
			if (hCtx)
				WTClose(hCtx);
			hCtx = WTRestore(hWnd, save_buf, FALSE);
			/* re-init the context. */
			if (hCtx) {
				WTGet(hCtx, &lc);
				TabletSetup(&lc);
				WTSet(hCtx, &lc);

				/* open for real. */
				WTEnable(hCtx, TRUE);
			}
		}
		/* bag the save buffer. */
		LocalFree((HLOCAL)save_buf);
	}
	return hCtx;
}
/* -------------------------------------------------------------------------- */
void TabletSave(HCTX hCtx, HWND hWnd, BOOL fAs)
{

	void *save_buf;
	UINT save_size;
	HFILE hFile;
	OFSTRUCT ofs;

	/* alloc a save buffer. */
	WTInfo(WTI_INTERFACE, IFC_CTXSAVESIZE, &save_size);
	if (save_buf = (void *)LocalAlloc(LPTR, save_size))
	{
		/* save the data. */
		if (WTSave(hCtx, save_buf)) {

			/* if setting file name... */
			if (fAs) {
				OPENFILENAME ofn;

				/* do save file box. */
				memset(&ofn, 0, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFilter = "Context Files\0*.ctx\0";
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof(szFile);
				ofn.Flags = OFN_PATHMUSTEXIST;
				ofn.lpstrDefExt = "ctx";

				fAs = GetSaveFileName(&ofn);
			}
			else
				/* assume file name good. */
				fAs = TRUE;

			/* if good file name... */
			if (fAs) {
				/* open the file. */
				if ((hFile = OpenFile(szFile, &ofs, OF_WRITE | OF_CREATE)) !=
						HFILE_ERROR)
				{
					/* read in the data. */
					_lwrite(hFile, save_buf, save_size);

					/* close the file. */
					_lclose(hFile);
				}
			}
		}
		/* bag the save buffer. */
		LocalFree((HLOCAL)save_buf);
	}
}
/* -------------------------------------------------------------------------- */
HCTX static NEAR TabletInit(HWND hWnd)
{
	LOGCONTEXT lcMine;

	/* get default region */
	WTInfo(WTI_DEFCONTEXT, 0, &lcMine);

	/* init the data structure */
	TabletSetup(&lcMine);

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
	static RECT rcClient;
	PAINTSTRUCT psPaint;
	HDC hDC;
	PACKET pkt;
	BOOL fHandled = TRUE;
	LRESULT lResult = 0L;
	static int count;
	static BOOL fPersist;


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
			switch (GET_WM_COMMAND_ID(wParam, lParam)) {
			case IDM_ABOUT:
				lpProcAbout = MakeProcInstance(About, hInst);
				DialogBox(hInst, "AboutBox", hWnd, lpProcAbout);
				FreeProcInstance(lpProcAbout);
				break;

			case IDM_OPEN:
				hTab = TabletRestore(hTab, hWnd);
				break;

			case IDM_SAVE:
				TabletSave(hTab, hWnd, FALSE);
				break;

			case IDM_SAVE_AS:
				TabletSave(hTab, hWnd, TRUE);
				break;

			case IDM_CONFIG:
				WTConfig(hTab, hWnd);
				break;

			case IDM_PERSIST:
				fPersist = !fPersist;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), IDM_EDIT),
					IDM_PERSIST, (fPersist ? MF_CHECKED : MF_UNCHECKED));
				break;

			default:
				fHandled = FALSE;
				break;
			}
			break;

		case WT_PACKET:
			if (WTPacket((HCTX)lParam, wParam, &pkt)) {
				if (HIWORD(pkt.pkButtons)==TBN_DOWN) {
					MessageBeep(0);
				}
				ptOld = ptNew;
				ptNew.x = MulDiv((UINT)pkt.pkX, rcClient.right, 10000);
				ptNew.y = MulDiv((UINT)pkt.pkY, rcClient.bottom, 10000);
				if (ptNew.x != ptOld.x || ptNew.y != ptOld.y) {
					InvalidateRect(hWnd, NULL, TRUE);
					if (count++ == 4) {
						count = 0;
						UpdateWindow(hWnd);
					}
				}
			}
			break;

		case WM_ACTIVATE:
			if (GET_WM_ACTIVATE_STATE(wParam, lParam))
				InvalidateRect(hWnd, NULL, TRUE);

			/* if switching in the middle, disable the region */
			if (hTab) {
				if (!fPersist)
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
			count = 0;
			hDC = BeginPaint(hWnd, &psPaint);

			/* redo horz */
			PatBlt(hDC, rcClient.left, rcClient.bottom - ptNew.y,
					rcClient.right, 1, BLACKNESS);
			/* redo vert */
			PatBlt(hDC, ptNew.x, rcClient.top,
					1, rcClient.bottom, BLACKNESS);

			EndPaint(hWnd, &psPaint);
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
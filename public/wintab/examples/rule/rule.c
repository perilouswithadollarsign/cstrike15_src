/*------------------------------------------------------------------------------
Rule - a simple WinTab program -- polling version.
RICO 8/20/91
------------------------------------------------------------------------------*/

#include <string.h>
#include <windows.h>
#include <stdlib.h>
#include <wintab.h>

#ifdef USE_X_LIB
#include <wintabx.h>
#endif

#define PACKETDATA	(PK_X | PK_Y | PK_BUTTONS)
#define PACKETMODE	0
#include <pktdef.h>
#include "rule.h"

/* -------------------------------------------------------------------------- */
#define Inch2Cm	CASTFIX32(2.54)
#define Cm2Inch	CASTFIX32(1.0/2.54)
/* -------------------------------------------------------------------------- */

char _szAppName[] = "Rule";
HANDLE __hInstance = NULL;  /* Our instance handle */

LRESULT FAR PASCAL RuleAppWndProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
BOOL NEAR PASCAL RegisterAppWndClass (HANDLE hInstance);

/* -------------------------------------------------------------------------- */
int PASCAL WinMain (HANDLE hInstance, HANDLE hPrevInstance,
					LPSTR lpszCmdLine, int nCmdShow)
{
	MSG msg;
	HWND hWnd;

	__hInstance = hInstance;

	if (hPrevInstance == NULL)
		if (!RegisterAppWndClass(hInstance))
			return(0);

	hWnd = CreateDialog( hInstance, _szAppName, NULL, NULL);

	if (hWnd == NULL)
		return(0);

	ShowWindow(hWnd, nCmdShow);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifdef USE_X_LIB
	_UnlinkWintab();
#endif

	return(0);
}
/* -------------------------------------------------------------------------- */
BOOL NEAR PASCAL RegisterAppWndClass (HANDLE hInstance)
{
	WNDCLASS WndClass;

	WndClass.style			= 0;
	WndClass.lpfnWndProc	= RuleAppWndProc;
	WndClass.cbClsExtra	 	= 0;
	WndClass.cbWndExtra	 	= DLGWINDOWEXTRA;
	WndClass.hInstance	  	= hInstance;
	WndClass.hIcon			= LoadIcon(hInstance, _szAppName);
	WndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground 	= (HBRUSH)(COLOR_WINDOW + 1);
	WndClass.lpszMenuName  	= NULL;
	WndClass.lpszClassName 	= _szAppName;
	return(RegisterClass(&WndClass));
}
/* -------------------------------------------------------------------------- */
HCTX static NEAR TabletInit(HWND hWnd, FIX32 scale[])
{
	LOGCONTEXT lcMine;

	/* get default region */
	WTInfo(WTI_DEFCONTEXT, 0, &lcMine);

	/* modify the digitizing region */
	strcpy(lcMine.lcName, "Rule Digitizing");
	lcMine.lcPktData = PACKETDATA;
	lcMine.lcPktMode = PACKETMODE;
	lcMine.lcMoveMask = 0;
	lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;

	/* output in 1000ths of cm */
	lcMine.lcOutOrgX = lcMine.lcOutOrgY = 0;
	lcMine.lcOutExtX = INT(scale[0] * lcMine.lcInExtX);
	lcMine.lcOutExtY = INT(scale[1] * lcMine.lcInExtY);

	/* open the region */
	return WTOpen(hWnd, &lcMine, TRUE);

}
/* -------------------------------------------------------------------------- */
/* return scaling factors in thousandths of cm per axis unit */
static void TabletScaling(FIX32 scale[])
{
	AXIS aXY[2];
	int i;
	UINT wDevice;

	/* get the data */
	WTInfo(WTI_DEFCONTEXT, CTX_DEVICE, &wDevice);
	WTInfo(WTI_DEVICES+wDevice, DVC_X, &aXY[0]);
	WTInfo(WTI_DEVICES+wDevice, DVC_Y, &aXY[1]);

	/* calculate the scaling factors */
	for (i = 0; i < 2; i++) {
		FIX_DIV(scale[i], CASTFIX32(1000), aXY[i].axResolution);
		if (aXY[i].axUnits == TU_INCHES) {
			FIX_MUL(scale[i], scale[i], Inch2Cm);
		}
	}
}
/* -------------------------------------------------------------------------- */
DWORD nsqrt(DWORD x)
{
	/* integer square root via Newton's method. */
	DWORD guess, oguess;

	if (x <= 1)
		return x;

	guess = 1;
	do
	{
		oguess = guess;
		guess = (guess + x/guess)/2;
	}
	while (labs(guess - oguess) > 1);

	if (guess == oguess)
		guess++;

	if (labs((guess * guess) - x) > labs((oguess * oguess) - x))
		guess = oguess;

	return guess;
}
/* -------------------------------------------------------------------------- */
LRESULT FAR PASCAL RuleAppWndProc (HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static int inMode = ID_CLICK;
	static LONG x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	static HCTX hTab = NULL;

	static FIX32 scale[2];

	PAINTSTRUCT psPaint;
	HDC hDC;

	switch (wMsg) {

		case WM_CREATE:
			TabletScaling(scale);
			break;

		case WM_LBUTTONDOWN:
			if ((hTab = TabletInit(hWnd, scale)) != NULL) {
				PACKET pkt;

				inMode = ID_PRESS;
				InvalidateRect(hWnd, NULL, TRUE);
				UpdateWindow(hWnd);

				while (inMode != ID_CLICK) {

					/* poll */
					if (!WTPacketsGet(hTab, 1, &pkt))
						continue;

					/* handle it */
					if (inMode == ID_PRESS && pkt.pkButtons) {
						x1 = pkt.pkX;
						y1 = pkt.pkY;
						inMode = ID_RELEASE;
						InvalidateRect(hWnd, NULL, TRUE);
						UpdateWindow(hWnd);
					}
					if (inMode == ID_RELEASE && pkt.pkButtons == 0) {
						x2 = pkt.pkX;
						y2 = pkt.pkY;
						inMode = ID_CLICK;
						InvalidateRect(hWnd, NULL, TRUE);
						UpdateWindow(hWnd);
					}
				}
				WTClose(hTab);
			}
			break;

		case WM_PAINT:
			hDC = BeginPaint(hWnd, &psPaint);
			ShowWindow(GetDlgItem(hWnd, ID_CLICK), inMode == ID_CLICK);
			ShowWindow(GetDlgItem(hWnd, ID_PRESS), inMode == ID_PRESS);
			ShowWindow(GetDlgItem(hWnd, ID_RELEASE), inMode == ID_RELEASE);
			if (inMode == ID_CLICK) {
				LONG delta[3];	/* horz/vert/diag */
				int i;

				delta[0] = labs(x2 - x1);
				delta[1] = labs(y2 - y1);
				delta[2] = nsqrt(delta[0] * delta[0] + delta[1] * delta[1]);

				for (i = 0; i < 3; i++) {	 	/* direction */
					char buf[20];

					/* print result in cm */
					wsprintf(buf, "%d.%3.3d", (UINT)delta[i]/1000,
						(UINT)delta[i]%1000);
					SetWindowText(GetDlgItem(hWnd, ID_HC + i), buf);

					/* convert to inches */
					delta[i] = INT(delta[i] * Cm2Inch);

					/* print result in inches */
					wsprintf(buf, "%d.%3.3d", (UINT)delta[i]/1000,
						(UINT)delta[i]%1000);
					SetWindowText(GetDlgItem(hWnd, ID_HI + i), buf);
				}
			}
			EndPaint(hWnd, &psPaint);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, wMsg, wParam, lParam);
	}
	return (LRESULT)0;
}
/* -------------------------------------------------------------------------- */


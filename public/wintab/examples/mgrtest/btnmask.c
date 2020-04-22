/* -------------------------------------------------------------------------- */
/* Set the Button Mask and XBTNMASK extension                                 */
/* -------------------------------------------------------------------------- */

#include <windows.h>

#include "wintab.h"

#include "Mgrdlg.h"
#include "Mgrtest.h"
#include "resource.h"


typedef struct {
	LOGCONTEXT * lc;
	BOOL xBtnMask_avail;
	XBTNMASK xBtnMask;
} btn_info;

static const unsigned nbitboxes = 18;
static const int bitbox_id[] = {
	XBU_DISPLAY1, XBU_DISPLAY2, XBU_DISPLAY3, XBU_DISPLAY4, XBU_DISPLAY5, XBU_DISPLAY6, XBU_DISPLAY7, XBU_DISPLAY8,
	XBD_DISPLAY1, XBD_DISPLAY2, XBD_DISPLAY3, XBD_DISPLAY4, XBD_DISPLAY5, XBD_DISPLAY6, XBD_DISPLAY7, XBD_DISPLAY8,
	BNU_DISPLAY, BND_DISPLAY
};


extern HANDLE hInst;

extern BOOL FAR PASCAL ButtonDlgProc(HWND, UINT, WPARAM, LPARAM);


void
static DisplayBits( HWND hDlg, const btn_info * btn )
{
	static char buf[33];
	int i;

	buf[32] = 0;

	/* Display the XBTNMASK bits */
	for( i = 0; i < 8; i++ ) {
		unsigned j;
		for( j = 0; j < 32; j++ )
			buf[j] = '0' + ((btn->xBtnMask.xBtnUpMask[4*i + j/8] >> (j%8)) & 0x01);
		SetWindowText(GetDlgItem(hDlg, bitbox_id[i]), buf);
	}

	for( i = 8; i < 16; i++ ) {
		unsigned j;
		for( j = 0; j < 32; j++ )
			buf[j] = '0' + ((btn->xBtnMask.xBtnDnMask[4*(i-8) + j/8] >> (j%8)) & 0x01);
		SetWindowText(GetDlgItem(hDlg, bitbox_id[i]), buf);
	}

	/* Display the regular button mask bits */
	for( i = 0; i < 32; i++ )
		buf[i] = (char)('0' + ((btn->lc->lcBtnUpMask >> i) & 0x01));
	SetWindowText(GetDlgItem(hDlg, BNU_DISPLAY), buf);

	for( i = 0; i < 32; i++ )
		buf[i] = (char)('0' + ((btn->lc->lcBtnDnMask >> i) & 0x01));
	SetWindowText(GetDlgItem(hDlg, BND_DISPLAY), buf);
}
/* -------------------------------------------------------------------------- */
BOOL 
CALLBACK BtnMaskDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam)
{
	static btn_info * btn;
	BOOL fResult;
	int bit;

	switch (Msg) {
		case WM_INITDIALOG:
			btn = (btn_info *)lParam;

			if( !btn->xBtnMask_avail ) {
				EnableWindow( GetDlgItem(hDlg, XBD_BOX), FALSE );
				EnableWindow( GetDlgItem(hDlg, XBU_BOX), FALSE );

				for( bit = 0; bit < 16; bit++ )
					EnableWindow( GetDlgItem(hDlg, bitbox_id[bit]), FALSE );
			}
				
			DisplayBits(hDlg, btn);
			break;

		case WM_LBUTTONDOWN: /* Change the xBtnMask bits */
			/* If clicked on a digit in ???_DISPLAY?, then change it's corrisponding bit */
			/* Luckily 1 and 0 have the same width in the default font */
			bit = test_bitboxes( hDlg, lParam, 32, nbitboxes, bitbox_id );

			if( bit > -1 ) {
				/* Flip the bit. The first 32 bits of XBTNMASK should match the regular
					32 bit button mask */
				if( bit/32 == 16 ) { /* Regular button mask */
					btn->lc->lcBtnUpMask ^= 1 << (bit%32);
					if( btn->xBtnMask_avail )
						btn->xBtnMask.xBtnUpMask[(bit%32)/8] ^= 1 << (bit%32);
				} else
					if( bit/32 == 17 ) {
						btn->lc->lcBtnDnMask ^= 1 << (bit%32);
						if( btn->xBtnMask_avail )
							btn->xBtnMask.xBtnDnMask[(bit%32)/8] ^= 1 << (bit%32);
					} else /* xBtnMask */
						if( btn->xBtnMask_avail )
							if( bit >= 256 ) {
								btn->xBtnMask.xBtnDnMask[(bit-256)/8] ^= 1 << (bit%8);
								if( bit-256 < 32 )
									btn->lc->lcBtnDnMask ^= 1 << (bit%32);
							} else {
								btn->xBtnMask.xBtnUpMask[bit / 8] ^= 1 << (bit%8);
								if( bit < 32 )
									btn->lc->lcBtnUpMask ^= 1 << (bit%32);
							}
				DisplayBits(hDlg, btn);
			}
			fResult = TRUE;
			break;

		case WM_COMMAND:
			if (wParam == IDOK) {
				EndDialog(hDlg, wParam);
				fResult = TRUE;
			}
			else if (wParam == IDCANCEL) {
				EndDialog(hDlg, wParam);
				fResult = TRUE;
			}
			break;

		default:
			fResult = FALSE;
	}
	return fResult;
}	/* BtnMaskDlgProc */



void
set_default_BtnMask( HWND hWnd, HMGR hMgr, int fSys )
{
	int wDev;
	FARPROC lpProcDlg;

	/* Get a device # */
	lpProcDlg = MakeProcInstance(CursInfoDlgProc, hWnd);
	wDev = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
		hWnd, lpProcDlg, WTI_DDCTXS);
	FreeProcInstance(lpProcDlg);

	if( wDev >= 0 ) {
		FARPROC fpProc;
		btn_info btn;
		HCTX hCtx;
		int id;
		unsigned numDevices;
		LOGCONTEXT lc;

		WTInfo(WTI_INTERFACE, IFC_NDEVICES, &numDevices);

		if( wDev < (int)numDevices ) {
			hCtx = WTMgrDefContextEx(hMgr, wDev, fSys);
			if( !hCtx )
				hCtx = WTMgrDefContext(hMgr, fSys);
		} else
			hCtx = WTMgrDefContext(hMgr, fSys); /* 'Default Device' was the last choice in the dialog */

		btn.lc = &lc;

		/* Read the button masks */
		if( !hCtx ) {
			MessageBox( hWnd, "Failed to open default context.", "MgrTest", MB_ICONHAND | MB_OK );
			return;
		}
		if( !WTGet( hCtx, btn.lc ) ) {
			MessageBox( hWnd, "WTGet failed.", "MgrTest", MB_ICONHAND | MB_OK );
			return;
		}
		if( WTExtGet( hCtx, WTX_XBTNMASK, &btn.xBtnMask ) )
			btn.xBtnMask_avail = TRUE;
		else {
			btn.xBtnMask_avail = FALSE;
			memset( btn.xBtnMask.xBtnDnMask, 0, sizeof(XBTNMASK) );
		}

		/* Do the button bit dialog */
		fpProc = MakeProcInstance((FARPROC)BtnMaskDlgProc, hWnd); 
		id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_BTNMASKS), hWnd, fpProc, (long)&btn);
		FreeProcInstance(fpProc);

		/* Set the new button masks */
		if( id == IDOK ) {
			if( !WTSet( hCtx, btn.lc ) )
				MessageBox( hWnd, "WTSet failed.", "MgrTest", MB_ICONHAND | MB_OK );
			if( btn.xBtnMask_avail && !WTExtSet( hCtx, WTX_XBTNMASK, &btn.xBtnMask ) )
				MessageBox( hWnd, "WTExtSet failed.", "MgrTest", MB_ICONHAND | MB_OK );
		}
	}
}

void
set_ctx_BtnMask( HWND hWnd, HCTX hCtx, LOGCONTEXT * lc )
{
	FARPROC fpProc;
	btn_info btn;
	int id;

	btn.lc = lc;
	if( WTExtGet( hCtx, WTX_XBTNMASK, &btn.xBtnMask ) )
		btn.xBtnMask_avail = TRUE;
	else {
		btn.xBtnMask_avail = FALSE;
		memset( btn.xBtnMask.xBtnDnMask, 0, sizeof(XBTNMASK) );
	}

	fpProc = MakeProcInstance((FARPROC)BtnMaskDlgProc, hWnd);
	id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_BTNMASKS), hWnd, fpProc, (long)&btn);
	FreeProcInstance(fpProc);

	if( id == IDOK ) {
		/* Set the new button masks */
		if( !WTSet( hCtx, btn.lc ) )
			MessageBox( hWnd, "WTSet failed.", "MgrTest", MB_ICONHAND | MB_OK );
		if( btn.xBtnMask_avail && !WTExtSet( hCtx, WTX_XBTNMASK, &btn.xBtnMask ) )
			MessageBox( hWnd, "WTExtSet failed.", "MgrTest", MB_ICONHAND | MB_OK );
	}
}
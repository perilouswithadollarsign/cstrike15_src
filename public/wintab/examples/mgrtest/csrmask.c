#include <windows.h>

#include "wintab.h"

#include "Mgrtest.h"
#include "resource.h"


static const unsigned textfield[4] = { IDC_TEXT1, IDC_TEXT2, IDC_TEXT3, IDC_TEXT4 };

extern HANDLE hInst;


static void CsrDisplayBits( HWND hDlg, const BYTE FAR csrmask[16] )
{
	char buf[33];
	unsigned i, j;

	buf[32] = 0;

	for( i = 0; i < 4; i++ ) {
		for( j = 0; j < 32; j++ )
			buf[j] = '0' + ((csrmask[4*i + j/8] >> (j%8)) & 0x01);

		SetWindowText(GetDlgItem(hDlg, textfield[i]), buf);
	}
}

BOOL 
CALLBACK CsrmaskDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam)
{
	static BYTE FAR * csrmask;
	
	BOOL fResult;

	int i;
	LRESULT val;

	switch (Msg) {
	case WM_INITDIALOG:
		/* Display the Csrmask bitfield bits */
		csrmask = (BYTE FAR *)lParam;

		CsrDisplayBits(hDlg, csrmask);

		fResult = TRUE;


		/* List the available cursors, and highlight them if their Csrmask bits are set */
		i = 0;
		while( WTInfo( WTI_CURSORS + i, 0, 0 ) ) {
			char name[1024];

			WTInfo( WTI_CURSORS + i, CSR_NAME, name );
			SendDlgItemMessage( hDlg, IDC_CSRLST, LB_ADDSTRING, 0, (LPARAM)((char FAR *)name) );
			i++;
		}

		/* for Unknown Reason, sending LB_SETSEL right after LB_ADDSTRING doesn't work reliably */
		i = 0;
		while( WTInfo( WTI_CURSORS + i, 0, 0 ) ) {
			/* Highlight the name in the selection box */
			SendDlgItemMessage( hDlg, IDC_CSRLST, LB_SETSEL, (csrmask[i/8] >> (i%8)) & 0x01, i );
			i++;
		}

		break;


	case WM_LBUTTONDOWN:
		/* If clicked on a number in IDC_TEXT?, then change it's corrisponding bit */
		i = test_bitboxes( hDlg, lParam, 32, 4, textfield );

		if( i > -1 ) {
			/* Flip the bit */
			csrmask[i / 8] ^= 1 << (i % 8);
			CsrDisplayBits(hDlg, csrmask);

			/* Highlight the corrisponding cursor in the listbox appropriately */
			SendDlgItemMessage( hDlg, IDC_CSRLST, LB_SETSEL, (csrmask[i/8] >> (i%8)) & 0x01, i );
		}
		fResult = TRUE;

		break;


	case WM_COMMAND:
		switch( wParam ) {
		case IDOK:
			EndDialog(hDlg, wParam);
			fResult = TRUE;
			break;

		case IDCANCEL:
			EndDialog(hDlg, wParam);
			fResult = TRUE;
			break;

		default:
			if( HIWORD(wParam) == LBN_SELCHANGE || wParam == IDC_CSRLST ) {
				/* Set all of the bits according to the selection box selections until error */
				i = 0;
				fResult = FALSE;
				while( !fResult ) {
					val = SendMessage( (HWND)lParam, LB_GETSEL, i, 0 );
					if( val > 0 )
						csrmask[i/8] |= 1 << (i%8);
					else
						if( val == 0 )
							csrmask[i/8] &= ~(1 << (i%8));
						else 
							fResult = TRUE;

					i++;
				}

				/* Redisplay the bits */
				CsrDisplayBits(hDlg, csrmask);
			}
			break;
		}
		break;

	default:
		fResult = FALSE;
		break;
	}

	return fResult;
}



void
set_default_CsrMask( HWND hWnd, HMGR hMgr, int fSys )
{
	int wDev;
	FARPROC lpProcDlg;

	/* Get a device # */
	lpProcDlg = MakeProcInstance( (FARPROC)CursInfoDlgProc, hInst);
	wDev = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INFOLIST),
		hWnd, lpProcDlg, WTI_DDCTXS);
	FreeProcInstance(lpProcDlg);

	if( wDev >= 0 ) {
		FARPROC fpProc;
		HCTX hCtx;
		int id;
		BYTE CsrMask[16];
		unsigned numDevices;

		WTInfo(WTI_INTERFACE, IFC_NDEVICES, &numDevices);

		if( wDev < (int)numDevices ) {
			hCtx = WTMgrDefContextEx(hMgr, wDev, fSys);
			if( !hCtx )
				hCtx = WTMgrDefContext(hMgr, fSys);
		} else
			hCtx = WTMgrDefContext(hMgr, fSys); /* 'Default Device' was the last choice in the dialog */

		/* Read the button masks */
		if( !hCtx ) {
			MessageBox( hWnd, "Failed to open default context.", "MgrTest", MB_ICONHAND | MB_OK );
			return;
		}
		if( !WTExtGet( hCtx, WTX_CSRMASK, CsrMask ) ) {
			MessageBox( hWnd, "Cursor mask not supported on this device.", "MgrTest", MB_ICONHAND | MB_OK );
			return;
		}

		/* Do the button bit dialog */
		fpProc = MakeProcInstance((FARPROC)CsrmaskDlgProc, hInst ); 
		id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CSRMASKS), hWnd, fpProc, (long)((BYTE FAR *)CsrMask));
		FreeProcInstance(fpProc);

		/* Set the new button masks */
		if( id == IDOK ) {
			if( !WTExtSet( hCtx, WTX_CSRMASK, CsrMask ) )
				MessageBox( hWnd, "WTExtSet failed.", "MgrTest", MB_ICONHAND | MB_OK );
		}
	}
}

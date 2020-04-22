/****************************/
/* Set extended button maps */
/****************************/

#include <stdio.h>

#include <windows.h>

#include "wintab.h"

#include "mgrtest.h"
#include "resource.h"


typedef struct {
	UINT wCsr;
	BYTE sysBtns[256];
	BYTE logBtns[256];
} xBtn_info;


static const unsigned nbitboxes = 32;
static const int bitbox_id[] = {
	IDC_SYS_1, IDC_SYS_2, IDC_SYS_3, IDC_SYS_4, IDC_SYS_5, IDC_SYS_6, IDC_SYS_7, IDC_SYS_8, IDC_SYS_9, IDC_SYS_10, IDC_SYS_11, IDC_SYS_12, IDC_SYS_13, IDC_SYS_14, IDC_SYS_15, IDC_SYS_16,
	IDC_LOG_1, IDC_LOG_2, IDC_LOG_3, IDC_LOG_4, IDC_LOG_5, IDC_LOG_6, IDC_LOG_7, IDC_LOG_8, IDC_LOG_9, IDC_LOG_10, IDC_LOG_11, IDC_LOG_12, IDC_LOG_13, IDC_LOG_14, IDC_LOG_15, IDC_LOG_16
};


extern HANDLE hInst;


void
display_xButton_info( HWND hDlg, const xBtn_info * btn )
{
	unsigned i;

	for( i = 0; i < nbitboxes; i++ ) {
		unsigned j;
		char buf[100] = "";

		for( j = 0; j < 16; j++ ) {
			char tmp[5];

			sprintf( tmp, "%2x ", (int)*(btn->sysBtns + i*16 + j) );
			strcat( buf, tmp );
		}
		
		SetWindowText(GetDlgItem(hDlg, bitbox_id[i]), buf);
	}
}


BOOL 
CALLBACK valueProc( HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam )
{
	BOOL fResult;
	int retval;

	switch( Msg ) {
	case WM_COMMAND:

		switch( wParam ) {
		case IDOK:
			retval = GetDlgItemInt( hDlg, IDC_EDIT, 0, TRUE );
			EndDialog(hDlg, retval);
			fResult = TRUE;
			break;
		case IDCANCEL:
			EndDialog(hDlg, -1);
			fResult = TRUE;
			break;
		default:
			fResult = FALSE;
		}
		break;

	default:
		fResult = FALSE;
	}

	return fResult;
}


BOOL 
CALLBACK xButtonDlgProc( HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam )
{
	static xBtn_info * btn;
	int i;
	BOOL fResult;

	switch( Msg ) {
	case WM_INITDIALOG:
		btn = (xBtn_info *)lParam;
		display_xButton_info( hDlg, btn );
		fResult = TRUE;
		break;

	case WM_LBUTTONDOWN: /* Change the xBtn map */
		i = test_bitboxes( hDlg, lParam, 16, nbitboxes, bitbox_id );
			
		if( i > -1 ) {
			FARPROC lpProcDlg;
			int val;

			/* Open 'Enter Value' dialog */
			lpProcDlg = MakeProcInstance( valueProc, hInst );
			val = DialogBox( hInst, MAKEINTRESOURCE(IDD_VALUE), hDlg, lpProcDlg );
			FreeProcInstance( lpProcDlg );

			if( val >= 0 && val <= 0xff ) {
				*(btn->sysBtns + i) = val;
				display_xButton_info( hDlg, btn );
			} else
				if( val != -1 )
					MessageBox( hDlg, "Invalid value.", "MgrTest", MB_OK | MB_ICONHAND );
		}
		fResult = TRUE;
		break;

	case WM_COMMAND:
		if (wParam == IDOK || wParam == IDCANCEL) {
			EndDialog(hDlg, wParam);
			fResult = TRUE;
		} else 
			fResult = FALSE;
		break;

	default:
		fResult = FALSE;
	}
	return fResult;
}


void
set_xBtnMap( HWND hWnd, HMGR hMgr )
{
    FARPROC lpProcDlg;
	xBtn_info info;
	unsigned i;
	int tag;

	/* Open a dialog to choose which cursor to use. */
	lpProcDlg = MakeProcInstance( CursInfoDlgProc, hInst );
	info.wCsr = DialogBoxParam( hInst, MAKEINTRESOURCE(IDD_INFOLIST),
		hWnd, lpProcDlg, WTI_CURSORS );
	FreeProcInstance( lpProcDlg );

	if( info.wCsr != 0xffffffff ) {
		/* Find xBtnMask info */
		i = 0;
		while( WTInfo( WTI_EXTENSIONS + i, EXT_TAG, &tag ) && tag != WTX_XBTNMASK )
			i++;

		if( tag != WTX_XBTNMASK )
			MessageBox( hWnd, "XBTNMASK extension not supported.", "MgrTest", MB_ICONHAND | MB_OK );

		/* Read the xBtn map info */
		if( !WTInfo( WTI_EXTENSIONS + i, EXT_CURSORS + info.wCsr, info.sysBtns ) )
			MessageBox( hWnd, "This cursor does not support XBTNMASK.", "MgrTest", MB_ICONHAND | MB_OK );
		else {
			int id;

			/* Start the XBUTTONS dialog */
			lpProcDlg = MakeProcInstance( xButtonDlgProc, hInst );
			id = DialogBoxParam( hInst, MAKEINTRESOURCE(IDD_XBUTTONS),
				hWnd, lpProcDlg, (long)&info );
			FreeProcInstance( lpProcDlg );

			if( id == IDOK )
				if( !WTMgrCsrExt( hMgr, info.wCsr, WTX_XBTNMASK, info.sysBtns ) )
					MessageBox( hWnd, "WTMgrCsrExt failed.", "MgrTest", MB_ICONHAND | MB_OK );
		}
	}
}

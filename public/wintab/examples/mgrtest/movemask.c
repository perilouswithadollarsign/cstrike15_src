#include <windows.h>

#include "wintab.h"

#include "Mgrtest.h"
#include "resource.h"


static const unsigned textfield[1] = { IDC_MMTEXT };
static const char * pk_tags[] = {
	"PK_CONTEXT", "PK_STATUS", "PK_TIME", "PK_CHANGED", "PK_SERIAL_NUMBER", "PK_CURSOR", "PK_BUTTONS",
	"PK_X", "PK_Y", "PK_Z", "PK_NORMAL_PRESSURE", "PK_TANGENT_PRESSURE", "PK_ORIENTATION", "PK_ROTATION", 0 };
extern HANDLE hInst;


static void MoveMask_DisplayBits( HWND hDlg, DWORD MoveMask )
{
	char buf[33];
	unsigned j;

	buf[32] = 0;

	for( j = 0; j < 32; j++ )
		buf[j] = '0' + (char)((MoveMask >> j) & 0x01);

	SetWindowText(GetDlgItem(hDlg, IDC_MMTEXT), buf);
}

BOOL 
CALLBACK MoveMask_DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam)
{
	static DWORD * MoveMask;
	
	BOOL fResult;

	int i;
	LRESULT val;

	switch (Msg) {
	case WM_INITDIALOG:
		MoveMask = (DWORD *)lParam;

		/* List the available cursors, and highlight them if their Csrmask bits are set */
		i = 0;
		while( pk_tags[i] != NULL ) {
			SendDlgItemMessage( hDlg, IDC_LST, LB_ADDSTRING, 0, (LPARAM)((char far *)pk_tags[i]) );
			i++;
		}

		/* for Unknown Reason, sending LB_SETSEL right after LB_ADDSTRING doesn't work reliably */
		i = 0;
		while( pk_tags[i] != NULL ) {
			/* Highlight the name in the selection box */
			SendDlgItemMessage( hDlg, IDC_LST, LB_SETSEL, (WPARAM)((*MoveMask >> i) & 0x01), i );
			i++;
		}

		/* Display the Csrmask bitfield bits */
		MoveMask_DisplayBits( hDlg, *MoveMask );

		fResult = TRUE;

		break;


	case WM_LBUTTONDOWN:
		/* If clicked on a number in IDC_TEXT?, then change it's corrisponding bit */
		i = test_bitboxes( hDlg, lParam, 32, 1, textfield );

		if( i > -1 ) {
			/* Flip the bit */
			*MoveMask ^= 1 << i;
			MoveMask_DisplayBits( hDlg, *MoveMask );

			/* Highlight the corrisponding cursor in the listbox appropriately */
			SendDlgItemMessage( hDlg, IDC_LST, LB_SETSEL, (WPARAM)((*MoveMask >> i) & 0x01), i );
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
			if( HIWORD(wParam) == LBN_SELCHANGE || wParam == IDC_LST ) {
				/* Set all of the bits according to the selection box selections until error */
				i = 0;
				fResult = FALSE;
				while( !fResult ) {
					val = SendMessage( (HWND)lParam, LB_GETSEL, i, 0 );
					if( val > 0 )
						*MoveMask |= 1 << i;
					else
						if( val == 0 )
							*MoveMask &= ~(1 << i);
						else 
							fResult = TRUE;
					if( i == 31 )
						fResult = TRUE;
					i++;
				}

				/* Redisplay the bits */
				MoveMask_DisplayBits( hDlg, *MoveMask );
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
set_ctx_MoveMask( HWND hWnd, HMGR hMgr, HCTX hCtx )
{
	FARPROC fpProc;
	int id;
	LOGCONTEXT lc;

	/* Read the button masks */
	if( !WTGet( hCtx, &lc ) ) {
		MessageBox( hWnd, "WTGet failed.", "MgrTest", MB_ICONHAND | MB_OK );
		return;
	}

	/* Do the button bit dialog */
	fpProc = MakeProcInstance( (FARPROC)MoveMask_DlgProc, hInst );
	id = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_MOVEMASK), hWnd, fpProc, (LPARAM)((DWORD FAR *)&lc.lcMoveMask));
	FreeProcInstance(fpProc);

	/* Set the new button masks */
	if( id == IDOK ) {
		if( !WTSet( hCtx, &lc ) )
			MessageBox( hWnd, "WTSet failed.", "MgrTest", MB_ICONHAND | MB_OK );
	}
}

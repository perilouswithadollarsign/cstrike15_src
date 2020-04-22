/* ------------------------------- infodlg.c -------------------------------- */

/* 11-25-91 DMH    -- Dialog boxes to mediate cursor/extension info-boxes, */
  
/*   6/1/98 Napoli -- Added WTI_DDCTXS, WTI_DSCTXS    */
/*                 -- Copied from wttest32 to mgrtest */

#include <string.h>
#include <windows.h>
#include <tchar.h>

#include "wintab.h"
#include "mgrtest.h"

#define Abort()		EndDialog(hDlg, -2)

extern HANDLE hInst;

#ifndef WIN32
#define TCHAR char
#define wcscpy(a,b) strcpy((a),(b))
#define TEXT(a) (a)
#endif

BOOL CALLBACK CursInfoDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam)
								
{
	int cBaseCat;
						/* Base category -- cursors, devices, extensions? */
	int cNumCats=0;
	int nNameCat=1;		/* For all three the *_NAME Index is 1. &&&MAGIC&&&*/
	int cSize;
	long nCat;
	TCHAR *szCatName;
	
	TCHAR PhraseBuf[40];
	TCHAR TextBuf[40];
	TCHAR szNameBuf[128];




	lParam = lParam;		/* Nuke warning */



	switch (Msg) {
		default:
			return FALSE;

		case WM_INITDIALOG:
			
			cBaseCat = LOWORD(lParam);
			switch (cBaseCat) {
				default:
					EndDialog(hDlg, -2);
					break;

				case WTI_DDCTXS:
				case WTI_DSCTXS:
				case WTI_DEVICES:
					szCatName = TEXT("Device");
					WTInfo(WTI_INTERFACE, IFC_NDEVICES, &cNumCats);

					if( cBaseCat == WTI_DDCTXS || cBaseCat == WTI_DSCTXS ) {
						WORD ver;
						if( !WTInfo(WTI_INTERFACE,IFC_SPECVERSION, &ver) ||
								((ver >> 8) <= 1 && (ver & 0xff) <= 10) ) {
							/* Apparently, this version of Wintab doesn't support WTI_DDCTXS
								or WTI_DSCTXS; return cNumCats, to indicate "default device"
								since no other categories actually exist. */
							EndDialog(hDlg, cNumCats);
							return TRUE;
						}
					}
					break;

				case WTI_CURSORS:
					szCatName = TEXT("Cursor");
					WTInfo(WTI_INTERFACE, IFC_NCURSORS, &cNumCats);
					break;

				case WTI_EXTENSIONS:
					szCatName = TEXT("Extension");
					WTInfo(WTI_INTERFACE, IFC_NEXTENSIONS, &cNumCats);
					break;

				/* Failure to support IFC_N* is handled later. */
			}	/* Switch on lParam */

			if (cNumCats == 1)
				EndDialog(hDlg, 0);

/*&&& Send message to secret field, indicating this window's
			cBaseCat.  Also, set window's title field and caption.  */

		 	GetWindowText(hDlg, TextBuf, sizeof(TextBuf));
			wsprintf(PhraseBuf, TextBuf, (LPSTR)szCatName);
		 	SetWindowText(hDlg, PhraseBuf);
			
		 	GetDlgItemText(hDlg, LBC_TITLE, TextBuf, sizeof(TextBuf));
			wsprintf(PhraseBuf, TextBuf, (LPSTR)szCatName);
		 	SetDlgItemText(hDlg, LBC_TITLE, PhraseBuf);
			
			SetDlgItemInt(hDlg, LBC_BASECAT, cBaseCat, FALSE);



			/* How many items are there?  Catch un-reported items.*/

			while (WTInfo(cBaseCat + cNumCats, 0, NULL))
				cNumCats++;

			while (cNumCats > 0 && !WTInfo(cBaseCat + cNumCats-1, 0, NULL))
				cNumCats--;

			if (cNumCats == 0) {
				wsprintf(PhraseBuf, TEXT("No %ss defined!"), (LPSTR)szCatName);
				MessageBox(hInst, PhraseBuf, TEXT("Info Boxes"),
					MB_ICONINFORMATION | MB_OK);
				EndDialog(hDlg, -2);
				return TRUE;
			}


			/* Fill list Box */
			for (nCat = 0; nCat < cNumCats; nCat++) {

				cSize = WTInfo(cBaseCat + (int)nCat, CSR_NAME,
							(LPVOID) szNameBuf);

				if (cSize == 0 || szNameBuf[0] == '\0')
					wsprintf(szNameBuf, TEXT("%s #%d"), (LPSTR)szCatName,(int)nCat);


				cSize = (int)SendDlgItemMessage(hDlg, LBC_LISTBOX,
					LB_INSERTSTRING, (WPARAM)-1, (DWORD)(LPSTR)szNameBuf);

				if (cSize == LB_ERR || cSize == LB_ERRSPACE) {

					MessageBox(hInst, TEXT("Couldn't set an item name!"), TEXT("Info Boxes"),
						MB_ICONINFORMATION | MB_OK);
				
/*					EndDialog(hDlg, -2);
					return TRUE;			/* Abort on failure */

				}	/* Abort on failure */
				else
					;

			}	/* for each cursor */
			
			if( cBaseCat == WTI_DDCTXS || cBaseCat == WTI_DSCTXS ) {
				/* Give a 'Default Device' choice, to fall back to WTI_DEFCONTEXT, WTI_DEFSYSCTX */
				wcscpy((wchar_t *)szNameBuf, (wchar_t *)TEXT("Default Device"));
				SendDlgItemMessage(hDlg, LBC_LISTBOX, LB_INSERTSTRING, (WPARAM)-1, (DWORD)(LPSTR)szNameBuf);
			}

/* Default selection was current cursor.  Now, skipping the whole
	thing, because there can be multiple active cursors! */

			nCat = 0;		/* Index of 0, First item. */
			if (LB_ERR == (int)SendDlgItemMessage(hDlg, LBC_LISTBOX,
									LB_SETCURSEL, (int)nCat, (long)NULL))
				MessageBox(hInst, TEXT("Couldn't set current selection!"),
					TEXT("Info Boxes"),
					MB_ICONINFORMATION | MB_OK);


			return TRUE;


		case WM_COMMAND:
			switch (wParam) {
				default:
					return FALSE;

				case LBC_LISTBOX:
					if (HIWORD(lParam)!=LBN_DBLCLK)
						return FALSE;
					// fall through on double click!
				case IDOK:

					cBaseCat = GetDlgItemInt(hDlg, LBC_BASECAT, NULL,
						FALSE);

					nCat = (int)SendDlgItemMessage(hDlg, LBC_LISTBOX,
													LB_GETCURSEL, 0,
													(long)NULL);
					EndDialog(hDlg, (int)nCat);

					return TRUE;

				case IDCANCEL:
					EndDialog(hDlg, -1);
					return TRUE;

			}	/* Switch wParam on WM_COMMAND */

	}	/* switch Msg number */

	return FALSE;
}	/* CursInfoDlgProc */

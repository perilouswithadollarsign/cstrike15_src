#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include "msgpack.h"
#include <wintab.h>

#include "mgrdlg.h"

extern HMGR hMgr;



/* Fake a notification msg to our own dialog. */
#define FakeNotify(Ctl, Msg) FORWARD_WM_COMMAND(hDlg, Ctl, \
								GetDlgItem(hDlg, Ctl), Msg, SendMessage)


char Logical[]="Button #\0\0\0\0\0";
char *LogNum= Logical+8;

WORD wCsr=0xFFFF;					/* Current Cursor */
unsigned char bLogBtns[32]={0}, bSysBtns[32]={0};

char *MseActs[]={
	"No Mouse Action",
	"Left Click",
	"Left Double-Click",
	"Left Drag",

	"Right Click",
	"Right Double-Click",
	"Right Drag",

	"Middle Click",
	"Middle Double-Click",
	"Middle Drag",
	NULL
};

char *PenActs[]={
	"No Pen Action",

	"Tip Click",
	"Tip Double-Click",
	"Tip Drag",

	"Inverted Click",
	"Inverted Double-Click",
	"Inverted Drag",

	"Barrel 1 Click",
	"Barrel 1 Double-Click",
	"Barrel 1 Drag",

	"Barrel 2 Click",
	"Barrel 2 Double-Click",
	"Barrel 2 Drag",

	"Barrel 3 Click",
	"Barrel 3 Double-Click",
	"Barrel 3 Drag",

	NULL
};


/* Primitive -- load into globals */
void static GetButtonMaps(HWND hDlg)
{
	char NameBuf[500], *Name;

	WTInfo(WTI_CURSORS+wCsr, CSR_BTNNAMES, NameBuf);
	WTInfo(WTI_CURSORS+wCsr, CSR_BUTTONMAP, bLogBtns);
	WTInfo(WTI_CURSORS+wCsr, CSR_SYSBTNMAP, bSysBtns);
	
	/* Clear out old strings */
	while (SendDlgItemMessage(hDlg, IDC_NAMES,
		CB_DELETESTRING, 0, 0L))
		;


	for (Name = NameBuf; *Name != '\0';
					Name += lstrlen(Name)+1)
		SendDlgItemMessage(hDlg, IDC_NAMES, CB_ADDSTRING,
			0, (LPARAM)(LPSTR)Name);
}


/* Primitive -- Set values from globals */
void static SetButtonMaps(HWND hDlg)
{
	if (wCsr == 0xFFFF)
		return;


	WTMgrCsrButtonMap(hMgr, wCsr, bLogBtns, bSysBtns);

}


/* Primitive -- Set default values */
void static ResetButtonMaps(HWND hDlg)
{
	if (wCsr == 0xFFFF)
		return;


	WTMgrCsrButtonMap(hMgr, wCsr, WTP_LPDEFAULT, WTP_LPDEFAULT);

}




BOOL CALLBACK ButtonDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int wTmp, i, wTmpCsr;
	static char Name[80];
	WORD id, cmd;
	HWND hWnd;

	switch (msg) {
		default:
			return FALSE;

		case WM_INITDIALOG:
			/* Cursor names */
			WTInfo(WTI_INTERFACE, IFC_NCURSORS, &wTmp);
			for (i = 0; i < wTmp; i++) {
				BOOL fActive;

				/* remember first active cursor. */
				WTInfo(WTI_CURSORS+i, CSR_ACTIVE, &fActive);
				if (fActive) {
					wTmpCsr = i;					
				}
				WTInfo(WTI_CURSORS+i, CSR_NAME, Name);
				SendDlgItemMessage(hDlg, IDC_CURSORS, CB_ADDSTRING, 0,
					(LPARAM)(LPSTR)Name);
			}

			/* Logical button numbers */
			for (i = 0; i < 32; i++) {
				sprintf( LogNum, "%i", i );
				/* itoa(i, LogNum, 10); */
				SendDlgItemMessage(hDlg, IDC_LOGICAL, CB_ADDSTRING, 0,
					(LPARAM)(LPSTR)Logical);
			}

			/* Mouse Actions */
			for (i = 0; MseActs[i] != NULL; i++) {
				SendDlgItemMessage(hDlg, IDC_MOUSE, CB_ADDSTRING, 0,
					(LPARAM)(LPSTR)(MseActs[i]));
			}

			/* Pen Actions */
			for (i = 0; PenActs[i] != NULL; i++) {
				SendDlgItemMessage(hDlg, IDC_PEN, CB_ADDSTRING, 0,
					(LPARAM)(LPSTR)(PenActs[i]));
			}


			SendDlgItemMessage(hDlg, IDC_NAMES, CB_ADDSTRING,
					0, (LPARAM)(LPSTR)"FOOBAR");

			/* start with current cursor selected. */
			SendDlgItemMessage(hDlg, IDC_CURSORS,
								CB_SETCURSEL, wTmpCsr, 0L);

			FakeNotify(IDC_CURSORS, CBN_SELCHANGE);

			return TRUE;

		case WM_COMMAND:
			id = GET_WM_COMMAND_ID(wParam, lParam);
			cmd = GET_WM_COMMAND_CMD(wParam, lParam);
			hWnd = GET_WM_COMMAND_HWND(wParam, lParam);
			switch (id) {
				case IDC_CURSORS:
					if (cmd == CBN_SELCHANGE) {

						/* Set old values */
						SetButtonMaps(hDlg);

						/* Set Button names and cascade selections. */

						wCsr = (WORD)SendDlgItemMessage(hDlg, IDC_CURSORS,
							CB_GETCURSEL, 0, 0L);

						GetButtonMaps(hDlg);

						/* Fake selection to continue cascade */
						SendDlgItemMessage(hDlg, IDC_NAMES,
							CB_SETCURSEL, 0, 0L);

						FakeNotify(IDC_NAMES, CBN_SELCHANGE);
					}	/* selchange */
					break;

				case IDC_NAMES:
					if (cmd == CBN_SELCHANGE) {

						wTmp = (WORD)SendDlgItemMessage(hDlg, IDC_NAMES,
							CB_GETCURSEL, 0, 0L);

						/* Fake selection to continue cascade */
						SendDlgItemMessage(hDlg, IDC_LOGICAL,
							CB_SETCURSEL, bLogBtns[wTmp], 0L);

						FakeNotify(IDC_LOGICAL, CBN_SELCHANGE);
					}
					break;

				case IDC_LOGICAL:
					if (cmd == CBN_SELCHANGE) {
						WORD wButton=0;

						wButton = (WORD)SendDlgItemMessage(hDlg, IDC_NAMES,
							CB_GETCURSEL, 0, 0L);

						wTmp = (WORD)SendDlgItemMessage(hDlg, IDC_LOGICAL,
							CB_GETCURSEL, 0, 0L);

						bLogBtns[wButton] = (BYTE)wTmp;

						/* Fake selection to continue cascade */
						SendDlgItemMessage(hDlg, IDC_MOUSE,
							CB_SETCURSEL, bSysBtns[wTmp] & 0xF, 0L);
						SendDlgItemMessage(hDlg, IDC_PEN,
							CB_SETCURSEL, bSysBtns[wTmp] >> 4, 0L);

					}
					break;


				case IDC_MOUSE:
					if (cmd == CBN_SELCHANGE) {
						WORD wButton=0;

						wButton = (WORD)SendDlgItemMessage(hDlg, IDC_LOGICAL,
							CB_GETCURSEL, 0, 0L);

						wTmp = (WORD)SendDlgItemMessage(hDlg, IDC_MOUSE,
							CB_GETCURSEL, 0, 0L);

						bSysBtns[wButton] &= 0xF0;
						bSysBtns[wButton] |= wTmp & 0x0F;
					}							 
					break;


				case IDC_PEN:
					if (cmd == CBN_SELCHANGE) {
						WORD wButton=0;

						wButton = (WORD)SendDlgItemMessage(hDlg, IDC_LOGICAL,
							CB_GETCURSEL, 0, 0L);

						wTmp = (WORD)SendDlgItemMessage(hDlg, IDC_PEN,
							CB_GETCURSEL, 0, 0L);

						bSysBtns[wButton] &= 0x0F;
						bSysBtns[wButton] |= (wTmp & 0x0F) << 4;
					}							 
					break;


				case IDC_DEFAULT:
					/* Set maps back to defaults */
					ResetButtonMaps(hDlg);
					wCsr = 0xFFFF;
					FakeNotify(IDC_CURSORS, CBN_SELCHANGE);
					break;

				case IDOK:
					SetButtonMaps(hDlg);
					EndDialog(hDlg, id);
					break;

				case IDCANCEL:
					EndDialog(hDlg, id);
					break;

						
				
						

			}	/* Switch id */

	}	/* Switch msg */

	return TRUE;
}

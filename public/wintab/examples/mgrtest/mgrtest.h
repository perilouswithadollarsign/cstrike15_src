#include <wintab.h>

#define IDM_TEST	0
#define IDM_EDIT	1
#define IDM_DEFCTX	2
#define IDM_DEFSCTX	3
#define IDM_HELP	4

#define IDM_CSRMENU	3

#define IDM_ABOUT			10
#define IDM_DEFDIG			11
#define IDM_DEFSYS			12
#define IDM_CBRTEST			13
#define IDM_BMSTEST			14
#define IDM_PMSTEST			15
#define IDM_PRSTEST			16
#define IDM_BUTTMAPS		17
#define IDM_OBT				18
#define IDM_DEVICES			20
#define IDM_CURSORS			30
#define IDM_OBTDEVS			40
#define IDM_RESET_DEFDIG    40005
#define IDM_RESET_DEFSYS    40006
#define IDM_HMGRTEST        40007
#define IDM_DEFDEV_DIG      40009
#define IDM_DEFDEV_SYS      40010
#define IDM_XBTN_DIG        40011
#define IDM_XBTN_SYS        40012
#define IDM_CSRMASK_DIG     40013
#define IDM_CSRMASK_SYS     40014

/* For IDD_INFOLIST */
#define LBC_TITLE	3
#define LBC_LISTBOX	4
#define LBC_BASECAT	5

int PASCAL WinMain(HANDLE, HANDLE, LPSTR, int);
BOOL InitApplication(HANDLE);
BOOL InitInstance(HANDLE, int);
LRESULT FAR PASCAL MainWndProc(HWND, unsigned, WPARAM, LPARAM);
BOOL FAR PASCAL About(HWND, unsigned, WPARAM, LPARAM);

/* If the exe imports WTMgrDefContextEx(), then we won't be able to run with
    older Wintab.dll/Wintab32.dll's which are don't support Wintab Spec 1.1.
    Instead, we'll try to GetProcAddress it ourselves. On failure, just disable
    features that depend on it. */
extern HCTX (API * pWTMgrDefContextEx)(HMGR, UINT, BOOL);
#define WTMgrDefContextEx( a, b, c )	pWTMgrDefContextEx( (a), (b), (c) )


BOOL CALLBACK CursInfoDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LONG lParam);
void set_ctx_BtnMask( HWND hWnd, HCTX hCtx, LOGCONTEXT * lc );
void set_ctx_MoveMask( HWND hWnd, HMGR hMgr, HCTX hCtx );

/* tests.c */
void BMSTest(HWND hWnd);
void PMSTest(HWND hWnd);
void PRSTest(HWND hWnd);
void HMGRTest(HWND hWnd);

/* test_bitboxes() - use a static text box for selecting/changing a list of bits, hex bytes,
	or other evenly spaced things. */
/* LOWORD(pos) = x coord */
/* HIWORD(pos) = y coord */
/* box_id = an array of dialog ID's, one for each box */
/* ndiv = number of divisions per box */
/* nboxes = number of boxes */
/* return value = selection number or -1 if point is outside of all boxes */
int test_bitboxes( HWND hDlg, unsigned long pos, unsigned ndiv, int nboxes, const int *box_id );

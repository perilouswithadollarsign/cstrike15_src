/* Various tests to check if Wintab is acting as we would expect */

#include <windows.h>

#include "wintab.h"


extern HMGR hMgr;
extern HMODULE hWintab;
extern char szWintab[];


#ifdef WIN32
	#define LoadLibraryWorked(h)	(h)
#else
	#define LoadLibraryWorked(h)	(h >= HINSTANCE_ERROR)
#endif



/* -------------------------------------------------------------------------- */
BOOL BtnMarks(HMGR h, UINT c, UINT FAR *n, UINT FAR *t)
{
	typedef BOOL (API *BM)(HMGR, UINT, DWORD, DWORD);
	typedef BOOL (API *BMX)(HMGR, UINT, UINT FAR *, UINT FAR *);
	static BM bm = NULL;
	static BMX bmx = NULL;

	/* if not got wintab handle... */
	if (!hWintab)
		/* get wintab handle. */
		hWintab = LoadLibrary(szWintab);
	/* failsafe. */
	if (!LoadLibraryWorked(hWintab))
		return FALSE;

	/* if not got a proc... */
	if (!bmx && !bm) {
		/* try for portable version. */
		bmx = (BMX)GetProcAddress(hWintab, "WTMgrCsrPressureBtnMarksEx");
		/* if no portable version... */
		if (!bmx)
			/* try for non-portable version. */
			bm = (BM)GetProcAddress(hWintab, "WTMgrCsrPressureBtnMarks");
	}
	/* failsafe. */
	if (!bmx && !bm)
		return FALSE;

	/* if portable version... */
	if (bmx) {
		/* call it. */
		return bmx(h, c, n, t);
	}
	else {
		/* convert arguments and call non-portable version. */
		DWORD dwN, dwT;

		if (!n)
			dwN = 0;
		else if (n == WTP_LPDEFAULT)
			dwN = WTP_DWDEFAULT;
		else
			dwN = *(DWORD FAR *)n;

		if (!t)
			dwT = 0;
		else if (t == WTP_LPDEFAULT)
			dwT = WTP_DWDEFAULT;
		else
			dwT = *(DWORD FAR *)t;
		
		return bm(h, c, dwN, dwT);
	}
}

/* -------------------------------------------------------------------------- */
void BMSTest(HWND hWnd)
{
	typedef BYTE MAP[32];
	static MAP logSave, sysSave, logTest, sysTest, logPatt, sysPatt;
	int i;
	BOOL fResult;
	char buf[200];

	/* set up funky test patterns. */
	for (i = 0; i < sizeof(MAP); i++) {
		logPatt[i] = 31;
		sysPatt[i] = SBN_LDRAG;
	}
	/* loop over cursors... */
	for (i = 0; WTInfo(WTI_CURSORS + i, CSR_NAME, NULL); i++) {

		/* save the current maps. */
		WTInfo(WTI_CURSORS + i, CSR_BUTTONMAP, logSave);
		WTInfo(WTI_CURSORS + i, CSR_SYSBTNMAP, sysSave);

		/* if the function thinks it succeeded... */
		if (fResult = WTMgrCsrButtonMap(hMgr, i, logPatt, sysPatt)) {

			/* get the resulting maps. */
			WTInfo(WTI_CURSORS + i, CSR_BUTTONMAP, logTest);
			WTInfo(WTI_CURSORS + i, CSR_SYSBTNMAP, sysTest);

			/* put back the originals. */
			WTMgrCsrButtonMap(hMgr, i, logSave, sysSave);

			/* compare what we sent with what we got. */
			fResult = (!memcmp(logTest, logPatt, sizeof(MAP)) &&
						!memcmp(sysTest, sysPatt, sizeof(MAP)));
		}
		/* report the results. */
		wsprintf(buf, "WTMgrCsrButtonMap() Test %s for Cursor %d.",
				(LPSTR)(fResult ? "Succeeded" : "Failed"), i);
		MessageBox(hWnd, buf, "MgrTest", MB_ICONINFORMATION | MB_OK);
	}
}
/* -------------------------------------------------------------------------- */
void PMSTest(HWND hWnd)
{
	UINT nSave[2], tSave[2], nTest[2], tTest[2], nPatt[2], tPatt[2];
	int i;
	BOOL fResult;
	BOOL fN, fT;
	char buf[200];

	/* set up funky test patterns. */
	nPatt[0] = tPatt[0] = 1;
	nPatt[1] = tPatt[1] = 2;

	/* loop over cursors... */
	for (i = 0; WTInfo(WTI_CURSORS + i, CSR_NAME, NULL); i++) {

		/* check which channels to test. */
		fN = !!WTInfo(WTI_CURSORS + i, CSR_NPBTNMARKS, NULL);
		fT = !!WTInfo(WTI_CURSORS + i, CSR_TPBTNMARKS, NULL);

		if (!fN && !fT) {
			fResult = !BtnMarks(hMgr, i, NULL, NULL);
			fResult &= !BtnMarks(hMgr, i, nPatt, NULL);
			fResult &= !BtnMarks(hMgr, i, NULL, tPatt);
			fResult &= !BtnMarks(hMgr, i, nPatt, tPatt);
		}
		else
		{
			/* save the current maps. */
			if (fN)
				WTInfo(WTI_CURSORS + i, CSR_NPBTNMARKS, nSave);
			if (fT)
				WTInfo(WTI_CURSORS + i, CSR_TPBTNMARKS, tSave);
	
			/* if the function thinks it succeeded... */
			if (BtnMarks(hMgr, i, (fN ? (LPVOID)nPatt : NULL),
												 (fT ? (LPVOID)tPatt : NULL))) {
	
				/* get the resulting maps. */
				if (fN)
					WTInfo(WTI_CURSORS + i, CSR_NPBTNMARKS, nTest);
				if (fT)
					WTInfo(WTI_CURSORS + i, CSR_TPBTNMARKS, tTest);
	
				/* put back the originals. */
				BtnMarks(hMgr, i, (fN ? (LPVOID)nSave : NULL),
												(fT ? (LPVOID)tSave : NULL));
	
				/* compare what we sent with what we got. */
				fResult = TRUE;
				if (fN)
					fResult &= !memcmp(nTest, nPatt, sizeof(nTest));
				if (fT)
					fResult &= !memcmp(tTest, tPatt, sizeof(tTest));
			}
			else
				fResult = FALSE;
		}
		/* report the results. */
		wsprintf(buf, "WTMgrCsrPressureBtnMarks() Test %s for Cursor %d.",
				(LPSTR)(fResult ? "Succeeded" : "Failed"), i);
		MessageBox(hWnd, buf, "MgrTest", MB_ICONINFORMATION | MB_OK);
	}
}
/* -------------------------------------------------------------------------- */
void PRSTest(HWND hWnd)
{
	typedef UINT CURVE[256];
	static CURVE nSave, tSave, nTest, tTest, nPatt, tPatt;
	int nSize, tSize;
	int i, j;
	BOOL fResult;
	BOOL fN, fT;
	AXIS p;
	char buf[200];

	/* loop over devices... */
	for (j = 0; WTInfo(WTI_DEVICES + j, DVC_FIRSTCSR, &i); j++) {
		int k;

		/* set up funky test patterns. */
		if (WTInfo(WTI_DEVICES + j, DVC_NPRESSURE, &p)) {
			nSize = (int)(p.axMax - p.axMin);
			for (k = 0; k < nSize; k++)
				nPatt[k] = (k ? (UINT)p.axMax : (UINT)p.axMin);
		}

		if (WTInfo(WTI_DEVICES + j, DVC_TPRESSURE, &p)) {
			tSize = (int)(p.axMax - p.axMin);
			for (k = 0; k < tSize; k++)
				tPatt[k] = (k ? (UINT)p.axMax : (UINT)p.axMin);
		}

		/* loop over cursors... */
		for (; WTInfo(WTI_CURSORS + i, CSR_NAME, NULL); i++) {

			/* check which channels to test. */
			fN = !!WTInfo(WTI_CURSORS + i, CSR_NPRESPONSE, NULL);
			fT = !!WTInfo(WTI_CURSORS + i, CSR_TPRESPONSE, NULL);
	
			if (!fN && !fT) {
				fResult = !WTMgrCsrPressureResponse(hMgr, i, NULL, NULL);
				fResult &= !WTMgrCsrPressureResponse(hMgr, i, nPatt, NULL);
				fResult &= !WTMgrCsrPressureResponse(hMgr, i, NULL, tPatt);
				fResult &= !WTMgrCsrPressureResponse(hMgr, i, nPatt, tPatt);
			}
			else
			{
				/* save the current maps. */
				if (fN)
					WTInfo(WTI_CURSORS + i, CSR_NPRESPONSE, nSave);
				if (fT)
					WTInfo(WTI_CURSORS + i, CSR_TPRESPONSE, tSave);
	
				/* if the function thinks it succeeded... */
				if (WTMgrCsrPressureResponse(hMgr, i,
											 (fN ? (LPVOID)nPatt : NULL),
											 (fT ? (LPVOID)tPatt : NULL))) {
		
					/* get the resulting maps. */
					if (fN)
						WTInfo(WTI_CURSORS + i, CSR_NPRESPONSE, &nTest);
					if (fT)
						WTInfo(WTI_CURSORS + i, CSR_TPRESPONSE, &tTest);
		
					/* put back the originals. */
					WTMgrCsrPressureResponse(hMgr, i,
											 (fN ? (LPVOID)nSave : NULL),
											 (fT ? (LPVOID)tSave : NULL));
		
					/* compare what we sent with what we got. */
					fResult = TRUE;
					if (fN)
						fResult &= !memcmp(&nTest, &nPatt, sizeof(nTest));
					if (fT)
						fResult &= !memcmp(&tTest, &tPatt, sizeof(nTest));
				}
				else
					fResult = FALSE;
			}
			/* report the results. */
			wsprintf(buf, "WTMgrCsrPressureResponse() Test %s for Cursor %d.",
					(LPSTR)(fResult ? "Succeeded" : "Failed"), i);
			MessageBox(hWnd, buf, "MgrTest", MB_ICONINFORMATION | MB_OK);
		}
	}
}
/* -------------------------------------------------------------------------- */
void HMGRTest(HWND hWnd)
{
	BOOL success = 1;
	HMGR old_hMgr = hMgr;
	HMGR new_hMgr;

	new_hMgr = WTMgrOpen(hWnd, WT_DEFBASE);
	if( !new_hMgr ) {
		MessageBox(hWnd, "WTMgrOpen failed.", "MgrTest", MB_ICONHAND | MB_OK);
		success = 0;
	} else {
		HCTX old_hCtx = WTMgrDefContext(old_hMgr, 0);
		HCTX new_hCtx = WTMgrDefContext(new_hMgr, 0);
		LOGCONTEXT ctx;

		if( !old_hCtx || !new_hCtx ) {
			MessageBox(hWnd, "WTMgrDefContext failed.", "MgrTest", MB_ICONHAND | MB_OK);
			success = 0;
		}
		if( !WTGet(old_hCtx, &ctx) || !WTGet(old_hCtx, &ctx) ) {
			MessageBox(hWnd, "WTGet failed before WTMgrClose called.", "MgrTest", MB_ICONHAND | MB_OK);
			success = 0;
		}
		if( !WTMgrClose(old_hMgr) ) {
			MessageBox(hWnd, "WTMgrClose failed.", "MgrTest", MB_ICONHAND | MB_OK);
			success = 0;
		}
		if( WTGet(old_hCtx, &ctx) ) {
			MessageBox(hWnd, "Error! WTGet returned success on context handle from a closed manager.", "MgrTest", MB_ICONHAND | MB_OK);
			success = 0;
		}
		if( !WTGet(new_hCtx, &ctx) ) {
			MessageBox(hWnd, "WTGet failed for new_hCtx after WTMgrClose(old_hMgr).", "MgrTest", MB_ICONHAND | MB_OK);
			success = 0;
		}
		hMgr = new_hMgr;
	}
	if( success )
		MessageBox(hWnd, "Test Passed.", "MgrTest", MB_ICONINFORMATION | MB_OK);
}

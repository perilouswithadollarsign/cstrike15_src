/* -------------------------------- mgrdll.c -------------------------------- */
#include <windows.h>
#include <wintab.h>

/* -------------------------------------------------------------------------- */
/* Win32 dll entry/exit point. */
/* -------------------------------------------------------------------------- */
BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved)
{
	return TRUE;
}
/* -------------------------------------------------------------------------- */
/* Win16 dll entry point. */
/* -------------------------------------------------------------------------- */
BOOL WINAPI LibMain(HANDLE hModule, WORD wDataSeg, WORD cbHeapSize,
						LPSTR lpCmdLine)
{
	return TRUE;
}
/* -------------------------------------------------------------------------- */
/* Win16 dll exit point. */
/* -------------------------------------------------------------------------- */
int WINAPI WEP(int nSystemExit)
{
	return TRUE;
}
/* -------------------------------------------------------------------------- */
BOOL CALLBACK CBRTestProc(HCTX hCtx, HWND hWnd)
{
	MessageBox(hWnd, "WTMgrConfigReplace() test succeeded!", "MgrTest",
				MB_ICONINFORMATION | MB_OK);
	return(FALSE);
}
/* -------------------------------------------------------------------------- */


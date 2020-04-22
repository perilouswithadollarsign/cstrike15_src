//wthook.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <wintab.h>

BOOL (WINAPI * Record)(BOOL,HMGR);
BOOL (WINAPI * Playback)(BOOL,HMGR);
long (WINAPI * get_num_pkts_recorded)(void);
long (WINAPI * get_num_pkts_played)(void);
void (WINAPI * display_record)(void);
void (WINAPI * reset)(void);


LRESULT WINAPI WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
	return DefWindowProc(h, msg, w, l);
}



//WinMain
int WINAPI WinMain(
    HINSTANCE hInstance,	// handle to current instance
    HINSTANCE hPrevInstance,	// handle to previous instance
    LPSTR lpCmdLine,	// pointer to command line
    int nCmdShow 	// show state of window
   )
{
	WNDCLASS wc =  {0};
	HINSTANCE hModule;
	HMGR hMgr;
	LPCSTR szClass = "WTHookClass";
	HWND hWnd;

	/* Load the functions from our dll */
	hModule = LoadLibrary( "wthkdll.dll" );
	if( !hModule ) {
		MessageBox( 0, "LoadLibrary on 'wthkdll' failed.", "wthook", MB_OK );
		return -1;
	}

	(FARPROC)Record = GetProcAddress( hModule, "Record" );
	(FARPROC)Playback = GetProcAddress( hModule, "Playback" );
	(FARPROC)get_num_pkts_recorded = GetProcAddress( hModule, "get_num_pkts_recorded" );
	(FARPROC)get_num_pkts_played = GetProcAddress( hModule, "get_num_pkts_played" );
	(FARPROC)display_record = GetProcAddress( hModule, "display_record" );
	(FARPROC)reset = GetProcAddress( hModule, "reset" );

	if( !Record || !Playback || !get_num_pkts_recorded || !get_num_pkts_played || !display_record || !reset ) {
		MessageBox( 0, "GetProcAddress on 'wthkdll' failed.", "wthook", MB_OK );
		return -1;
	}

	/* Open a window and get a manager handle */
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = szClass;

	if (RegisterClass(&wc)) {
		hWnd = CreateWindow(szClass, "WTHookWnd", 0,
							 0, 0, 0, 0,
							 0, 0, hInstance, NULL);
	}
	hMgr = WTMgrOpen(hWnd, WT_DEFBASE);

	reset(); /* Reset wthkdll */


	/* Record some packets */
	if (Record(TRUE,hMgr)) {
		long recsize;
		char buf[128];

		MessageBox(0, "Hook installed. Recording Packets. Hit ok to end hook.", "WTHook", MB_OK);
		recsize = get_num_pkts_recorded();
		Record(FALSE,hMgr);

		sprintf( buf, "Recorded %li packets.", recsize );
		MessageBox( 0, buf, "WTHook", MB_OK );
	}


	/* Display the packet data */
	display_record();


	/* Clean up */
	WTMgrClose(hMgr);
	DestroyWindow(hWnd);
	UnregisterClass(szClass, hInstance);
	reset(); /* Reset wthkdll */
	FreeLibrary( hModule );

	return 0;
}
 

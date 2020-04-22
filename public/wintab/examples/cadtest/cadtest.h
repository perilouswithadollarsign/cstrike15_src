#define IDM_FILE	0
#define IDM_EDIT	1
#define IDM_HELP	2

#define IDM_ABOUT	100
#define IDM_OPEN	101
#define IDM_SAVE	102
#define IDM_SAVE_AS	103
#define IDM_CONFIG	104
#define IDM_PERSIST	105

int PASCAL WinMain(HANDLE, HANDLE, LPSTR, int);
BOOL InitApplication(HANDLE);
BOOL InitInstance(HANDLE, int);
LRESULT FAR PASCAL MainWndProc(HWND, unsigned, WPARAM, LPARAM);
BOOL FAR PASCAL About(HWND, unsigned, WPARAM, LPARAM);

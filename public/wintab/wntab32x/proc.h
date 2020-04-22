/* --------------------------------- proc.h --------------------------------- */
#include <windows.h>
#include <wintab.h>


#define T_WTInfoA	UINT
#define K_WTInfoA	API
#define P_WTInfoA	(UINT a, UINT b, LPVOID c)
#define A_WTInfoA	(a,b,c)
#define T_WTInfoW	UINT
#define K_WTInfoW	API
#define P_WTInfoW	(UINT a, UINT b, LPVOID c)
#define A_WTInfoW	(a,b,c)
#define T_WTOpenA	HCTX
#define K_WTOpenA	API
#define P_WTOpenA	(HWND a, LPLOGCONTEXTA b, BOOL c)
#define A_WTOpenA	(a,b,c)
#define T_WTOpenW	HCTX
#define K_WTOpenW	API
#define P_WTOpenW	(HWND a, LPLOGCONTEXTW b, BOOL c)
#define A_WTOpenW	(a,b,c)
#define T_WTClose	BOOL
#define K_WTClose	API
#define P_WTClose	(HCTX a)
#define A_WTClose	(a)
#define T_WTPacketsGet	int
#define K_WTPacketsGet	API
#define P_WTPacketsGet	(HCTX a, int b, LPVOID c)
#define A_WTPacketsGet	(a,b,c)
#define T_WTPacket	BOOL
#define K_WTPacket	API
#define P_WTPacket	(HCTX a, UINT b, LPVOID c)
#define A_WTPacket	(a,b,c)
#define T_WTEnable	BOOL
#define K_WTEnable	API
#define P_WTEnable	(HCTX a, BOOL b)
#define A_WTEnable	(a,b)
#define T_WTOverlap	BOOL
#define K_WTOverlap	API
#define P_WTOverlap	(HCTX a, BOOL b)
#define A_WTOverlap	(a,b)
#define T_WTConfig	BOOL
#define K_WTConfig	API
#define P_WTConfig	(HCTX a, HWND b)
#define A_WTConfig	(a,b)
#define T_WTGetA	BOOL
#define K_WTGetA	API
#define P_WTGetA	(HCTX a, LPLOGCONTEXTA b)
#define A_WTGetA	(a,b)
#define T_WTGetW	BOOL
#define K_WTGetW	API
#define P_WTGetW	(HCTX a, LPLOGCONTEXTW b)
#define A_WTGetW	(a,b)
#define T_WTSetA	BOOL
#define K_WTSetA	API
#define P_WTSetA	(HCTX a, LPLOGCONTEXTA b)
#define A_WTSetA	(a,b)
#define T_WTSetW	BOOL
#define K_WTSetW	API
#define P_WTSetW	(HCTX a, LPLOGCONTEXTW b)
#define A_WTSetW	(a,b)
#define T_WTExtGet	BOOL
#define K_WTExtGet	API
#define P_WTExtGet	(HCTX a, UINT b, LPVOID c)
#define A_WTExtGet	(a,b,c)
#define T_WTExtSet	BOOL
#define K_WTExtSet	API
#define P_WTExtSet	(HCTX a, UINT b, LPVOID c)
#define A_WTExtSet	(a,b,c)
#define T_WTSave	BOOL
#define K_WTSave	API
#define P_WTSave	(HCTX a, LPVOID b)
#define A_WTSave	(a,b)
#define T_WTRestore	HCTX
#define K_WTRestore	API
#define P_WTRestore	(HWND a, LPVOID b, BOOL c)
#define A_WTRestore	(a,b,c)
#define T_WTPacketsPeek	int
#define K_WTPacketsPeek	API
#define P_WTPacketsPeek	(HCTX a, int b, LPVOID c)
#define A_WTPacketsPeek	(a,b,c)
#define T_WTDataGet	int
#define K_WTDataGet	API
#define P_WTDataGet	(HCTX a, UINT b, UINT c, int d, LPVOID e, LPINT f)
#define A_WTDataGet	(a,b,c,d,e,f)
#define T_WTDataPeek	int
#define K_WTDataPeek	API
#define P_WTDataPeek	(HCTX a, UINT b, UINT c, int d, LPVOID e, LPINT f)
#define A_WTDataPeek	(a,b,c,d,e,f)
#define T_WTQueuePacketsEx	BOOL
#define K_WTQueuePacketsEx	API
#define P_WTQueuePacketsEx	(HCTX a, UINT FAR * b, UINT FAR * c)
#define A_WTQueuePacketsEx	(a,b,c)
#define T_WTQueueSizeGet	int
#define K_WTQueueSizeGet	API
#define P_WTQueueSizeGet	(HCTX a)
#define A_WTQueueSizeGet	(a)
#define T_WTQueueSizeSet	int
#define K_WTQueueSizeSet	API
#define P_WTQueueSizeSet	(HCTX a, int b)
#define A_WTQueueSizeSet	(a,b)
#define T_WTMgrOpen	HMGR
#define K_WTMgrOpen	API
#define P_WTMgrOpen	(HWND a, UINT b)
#define A_WTMgrOpen	(a,b)
#define T_WTMgrClose	BOOL
#define K_WTMgrClose	API
#define P_WTMgrClose	(HMGR a)
#define A_WTMgrClose	(a)
#define T_WTMgrContextEnum	BOOL
#define K_WTMgrContextEnum	API
#define P_WTMgrContextEnum	(HMGR a, WTENUMPROC b, LPARAM c)
#define A_WTMgrContextEnum	(a,b,c)
#define T_WTMgrContextOwner	HWND
#define K_WTMgrContextOwner	API
#define P_WTMgrContextOwner	(HMGR a, HCTX b)
#define A_WTMgrContextOwner	(a,b)
#define T_WTMgrDefContext	HCTX
#define K_WTMgrDefContext	API
#define P_WTMgrDefContext	(HMGR a, BOOL b)
#define A_WTMgrDefContext	(a,b)
#define T_WTMgrDefContextEx	HCTX
#define K_WTMgrDefContextEx	API
#define P_WTMgrDefContextEx	(HMGR a, UINT b, BOOL c)
#define A_WTMgrDefContextEx	(a,b,c)
#define T_WTMgrDeviceConfig	UINT
#define K_WTMgrDeviceConfig	API
#define P_WTMgrDeviceConfig	(HMGR a, UINT b, HWND c)
#define A_WTMgrDeviceConfig	(a,b,c)
#define T_WTMgrConfigReplaceExA	BOOL
#define K_WTMgrConfigReplaceExA	API
#define P_WTMgrConfigReplaceExA	(HMGR a, int b, LPSTR c, LPSTR d)
#define A_WTMgrConfigReplaceExA	(a,b,c,d)
#define T_WTMgrConfigReplaceExW	BOOL
#define K_WTMgrConfigReplaceExW	API
#define P_WTMgrConfigReplaceExW	(HMGR a, int b, LPWSTR c, LPSTR d)
#define A_WTMgrConfigReplaceExW	(a,b,c,d)
#define T_WTMgrPacketHookExA	HWTHOOK
#define K_WTMgrPacketHookExA	API
#define P_WTMgrPacketHookExA	(HMGR a, int b, LPSTR c, LPSTR d)
#define A_WTMgrPacketHookExA	(a,b,c,d)
#define T_WTMgrPacketHookExW	HWTHOOK
#define K_WTMgrPacketHookExW	API
#define P_WTMgrPacketHookExW	(HMGR a, int b, LPWSTR c, LPSTR d)
#define A_WTMgrPacketHookExW	(a,b,c,d)
#define T_WTMgrPacketUnhook	BOOL
#define K_WTMgrPacketUnhook	API
#define P_WTMgrPacketUnhook	(HWTHOOK a)
#define A_WTMgrPacketUnhook	(a)
#define T_WTMgrPacketHookNext	LRESULT
#define K_WTMgrPacketHookNext	API
#define P_WTMgrPacketHookNext	(HWTHOOK a, int b, WPARAM c, LPARAM d)
#define A_WTMgrPacketHookNext	(a,b,c,d)
#define T_WTMgrExt	BOOL
#define K_WTMgrExt	API
#define P_WTMgrExt	(HMGR a, UINT b, LPVOID c)
#define A_WTMgrExt	(a,b,c)
#define T_WTMgrCsrEnable	BOOL
#define K_WTMgrCsrEnable	API
#define P_WTMgrCsrEnable	(HMGR a, UINT b, BOOL c)
#define A_WTMgrCsrEnable	(a,b,c)
#define T_WTMgrCsrButtonMap	BOOL
#define K_WTMgrCsrButtonMap	API
#define P_WTMgrCsrButtonMap	(HMGR a, UINT b, LPBYTE c, LPBYTE d)
#define A_WTMgrCsrButtonMap	(a,b,c,d)
#define T_WTMgrCsrPressureBtnMarks	BOOL
#define K_WTMgrCsrPressureBtnMarks	API
#define P_WTMgrCsrPressureBtnMarks	(HMGR a, UINT b, DWORD c, DWORD d)
#define A_WTMgrCsrPressureBtnMarks	(a,b,c,d)
#define T_WTMgrCsrPressureBtnMarksEx	BOOL
#define K_WTMgrCsrPressureBtnMarksEx	API
#define P_WTMgrCsrPressureBtnMarksEx	(HMGR a, UINT b, UINT FAR * c, UINT FAR * d)
#define A_WTMgrCsrPressureBtnMarksEx	(a,b,c,d)
#define T_WTMgrCsrPressureResponse	BOOL
#define K_WTMgrCsrPressureResponse	API
#define P_WTMgrCsrPressureResponse	(HMGR a, UINT b, UINT FAR * c, UINT FAR * d)
#define A_WTMgrCsrPressureResponse	(a,b,c,d)
#define T_WTMgrCsrExt	BOOL
#define K_WTMgrCsrExt	API
#define P_WTMgrCsrExt	(HMGR a, UINT b, UINT c, LPVOID d)
#define A_WTMgrCsrExt	(a,b,c,d)

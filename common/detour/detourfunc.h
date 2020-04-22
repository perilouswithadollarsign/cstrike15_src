//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Function Detouring code used by the overlay
//
// $NoKeywords: $
//=============================================================================
#ifndef DETOURFUNC_H
#define DETOURFUNC_H
#ifdef _WIN32
#pragma once
#endif

void * HookFunc( BYTE *pRealFunctionAddr, const BYTE *pHookFunctionAddr, int nJumpsToFollowBeforeHooking = 0 );

bool HookFuncSafe( BYTE *pRealFunctionAddr, const BYTE *pHookFunctionAddr, void ** ppRelocFunctionAddr, int nJumpsToFollowBeforeHooking = 0 );
bool bIsFuncHooked( BYTE *pRealFunctionAddr, void *pHookFunc = NULL );
void UnhookFunc( BYTE *pRealFunctionAddr, BYTE *pOriginalFunctionAddr_DEPRECATED );
void UnhookFunc( BYTE *pRealFunctionAddr, bool bLogFailures = true );
void UnhookFuncByRelocAddr( BYTE *pRelocFunctionAddr, bool bLogFailures = true );

void RegregisterTrampolines();

void DetectUnloadedHooks();

#if defined( _WIN32 ) && DEBUG_ENABLE_DETOUR_RECORDING
template <typename T, int k_nCountElements > 
class CCallRecordSet
{
public:
	typedef T ElemType_t;

	CCallRecordSet()
	{
		m_cElements = 0;
		m_cElementPostWrite = 0;
		m_cElementMax = k_nCountElements;
		m_cubElements = sizeof(m_rgElements);
		memset( m_rgElements, 0, sizeof(m_rgElements) );
	}

	// if return value is >= 0, then we matched an existing record
	int AddFunctionCallRecord( const ElemType_t &fcr )
	{
		// if we are full, dont bother searching any more
		// this reduces our perf impact to near zero if these functions are 
		// called a lot more than we expect
		int cElements = m_cElements;
		if ( cElements >= k_nCountElements )
		{
			return -2;
		}

		// search backwards through the log
		for( int i = cElements-1; i >= 0; i-- )
		{
			if ( m_rgElements[i] == fcr )
				return i;
		}

		cElements = ++m_cElements;
		if ( cElements <= k_nCountElements )
		{
			m_rgElements[cElements-1] = fcr;
		}
		// if an external reader sees m_cElements != m_cElementPostWrite
		// they know the last item(s) may not be complete
		m_cElementPostWrite++;
		return -1;
	}

	CInterlockedIntT< int > m_cElements;
	CInterlockedIntT< int > m_cElementPostWrite;
	int m_cElementMax;
	int m_cubElements;
	ElemType_t m_rgElements[k_nCountElements];
};


class CRecordDetouredCalls
{
public:
	CRecordDetouredCalls();

	void SetMasterSwitchOn() { m_bMasterSwitch = true; }
	bool BIsMasterSwitchOn() { return m_bMasterSwitch; }

	bool BShouldRecordProtectFlags( DWORD flProtect );

	void RecordGetAsyncKeyState( DWORD vKey, 
			PVOID lpCallersAddress, PVOID lpCallersCallerAddress
			);

	void RecordVirtualAlloc( LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
			LPVOID lpvResult, DWORD dwGetLastError, 
			PVOID lpCallersAddress, PVOID lpCallersCallerAddress
			);

	void RecordVirtualProtect( LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
			BOOL bResult, DWORD dwGetLastError, 
			PVOID lpCallersAddress, PVOID lpCallersCallerAddress
			);

	void RecordVirtualAllocEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
			LPVOID lpvResult, DWORD dwGetLastError, 
			PVOID lpCallersAddress, PVOID lpCallersCallerAddress
			);

	void RecordVirtualProtectEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
			BOOL bResult, DWORD dwGetLastError, 
			PVOID lpCallersAddress, PVOID lpCallersCallerAddress
			);

	void RecordLoadLibraryW( 
			LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
			HMODULE hModule, DWORD dwGetLastError, 
			PVOID lpCallersAddress, PVOID lpCallersCallerAddress
			);

	void RecordLoadLibraryA( 
		LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
		HMODULE hModule, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		);

private:

	struct FunctionCallRecordBase_t
	{
		void SharedInit(
				DWORD dwResult, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		DWORD m_dwResult;
		DWORD m_dwGetLastError;
		LPVOID m_lpFirstCallersAddress;
		LPVOID m_lpLastCallerAddress;
	};

	// for GetAsyncKeyState the only thing we care about is the call site
	// dont care about results or params
	struct GetAsyncKeyStateCallRecord_t : public FunctionCallRecordBase_t
	{
		GetAsyncKeyStateCallRecord_t()
		{}

		void InitGetAsyncKeyState( DWORD vKey, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		bool operator==( const FunctionCallRecordBase_t &rhs ) const
		{
			// compare callers only, dont care about results or params
			return 
				m_lpFirstCallersAddress	== rhs.m_lpFirstCallersAddress && 
				m_lpLastCallerAddress	== rhs.m_lpLastCallerAddress;
		}

	};

	struct VirtualAllocCallRecord_t : public FunctionCallRecordBase_t
	{
		VirtualAllocCallRecord_t()
		{}

		// VirtualAlloc
		void InitVirtualAlloc( LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
				LPVOID lpvResult, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		// VirtualAllocEx
		void InitVirtualAllocEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
				LPVOID lpvResult, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		// VirtualProtect
		void InitVirtualProtect( LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
				BOOL bResult, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		// VirtualProtectEx
		void InitVirtualProtectEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
				BOOL bResult, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		bool operator==( const VirtualAllocCallRecord_t &rhs ) const
		{
			// compare everything
			return
				m_dwResult				== rhs.m_dwResult && 
				m_dwGetLastError		== rhs.m_dwGetLastError && 
				m_dwProcessId			== rhs.m_dwProcessId && 
				m_lpAddress				== rhs.m_lpAddress && 
				m_dwSize				== rhs.m_dwSize && 
				m_flProtect				== rhs.m_flProtect && 
				m_dw2					== rhs.m_dw2 && 
				m_lpFirstCallersAddress	== rhs.m_lpFirstCallersAddress && 
				m_lpLastCallerAddress	== rhs.m_lpLastCallerAddress;
		}

		DWORD m_dwProcessId;
		LPVOID m_lpAddress;
		SIZE_T m_dwSize;
		DWORD m_flProtect;
		DWORD m_dw2;
	};

	// for LoadLibrary just log everything, params and call sites
	struct LoadLibraryCallRecord_t : public FunctionCallRecordBase_t
	{
		LoadLibraryCallRecord_t() {}

		// LoadLibraryExW or LoadLibraryW
		void InitLoadLibraryW( 
				LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
				HMODULE hModule, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		void InitLoadLibraryA( 
				LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
				HMODULE hModule, DWORD dwGetLastError, 
				PVOID lpCallersAddress, PVOID lpCallersCallerAddress
				);

		bool operator==( const LoadLibraryCallRecord_t &rhs ) const
		{
			// compare the result ( hModule ) but not the callers
			// we arent going to have a perfect history of every caller
			if ( m_dwResult != rhs.m_dwResult )
			{
				return false;
			}
			// and then what we have of the actual filename
			return ( memcmp( m_rgubFileName, &rhs.m_rgubFileName, sizeof(m_rgubFileName) ) == 0 );
		}

		uint8 m_rgubFileName[128];
		HANDLE m_hFile;
		DWORD m_dwFlags;
	};

	// These GUIDs are constants, and it is how we find this structure when looking through the data section
	// when we are trying to read this data with an external process
	GUID m_guidMarkerBegin;

	// some helpers for parsing the structure externally
	int m_nVersionNumber;
	int m_cubRecordDetouredCalls;
	int m_cubGetAsyncKeyStateCallRecord;
	int m_cubVirtualAllocCallRecord;
	int m_cubVirtualProtectCallRecord;
	int m_cubLoadLibraryCallRecord;

	// these numbers were chosen by profiling CS:GO a bunch
	CCallRecordSet< GetAsyncKeyStateCallRecord_t, 50 >		m_GetAsyncKeyStateCallRecord;
	CCallRecordSet< VirtualAllocCallRecord_t, 300 >			m_VirtualAllocCallRecord;
	CCallRecordSet< VirtualAllocCallRecord_t, 500 >			m_VirtualProtectCallRecord;
	CCallRecordSet< LoadLibraryCallRecord_t, 200 >			m_LoadLibraryCallRecord;

	bool m_bMasterSwitch;
	// These GUIDs are constants, and it is how we find this structure when looking through the data section
	GUID m_guidMarkerEnd;

};

extern CRecordDetouredCalls g_RecordDetouredCalls;

typedef PVOID (WINAPI *RtlGetCallersAddress_t)( PVOID *CallersAddress, PVOID *CallersCaller );
extern RtlGetCallersAddress_t g_pRtlGetCallersAddress;
#endif // _WIN32

#endif // DETOURFUNC_H
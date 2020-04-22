//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Actual code for our d3d main interface wrapper
//
// $NoKeywords: $
//============================================================================= 

#include "tier0/memdbgoff.h"

#include "winlite.h"
typedef __int16 int16;
typedef unsigned __int16 uint16;
typedef __int32 int32;
typedef unsigned __int32 uint32;
typedef __int64 int64;
typedef unsigned __int64 uint64;
typedef char tchar;

#define DEBUG_ENABLE_ERROR_STREAM 0
#define DEBUG_ENABLE_DETOUR_RECORDING 0

// Suppress having to include of tier0 Assert functions
// #define DBG_H
// #define Assert( ... ) ( (void) 0 )
// #define Msg( ... ) ( (void) 0 )
// #define AssertMsg( ... ) ( (void) 0 )
// #define AssertMsg3( ... ) ( (void) 0 )

#include "tier0/basetypes.h"
// #include "tier0/threadtools.h"
#include "detourfunc.h"
#include "disassembler.h"
#include <map>
#include <vector>
#include <set>


// Define this to do verbose logging of detoured calls
//#define DEBUG_LOG_DETOURED_CALLS

#if DEBUG_ENABLE_ERROR_STREAM

// We dump error messages that we want the steam client to be able to read here
#pragma pack( push, 1 )
struct ErrorStreamMsg_t
{
	uint32 unStrLen;
	char rgchError[1024];
};
#pragma pack( pop )
CSharedMemStream *g_pDetourErrorStream = NULL;
static inline void Log( char const *, ... ) {}

#else

#define Log( ... ) ( (void) 0 )

#endif

#pragma pack( push, 1 ) // very important as we use structs to pack asm instructions together
// Structure that we pack ASM jump code into for hooking function calls
typedef struct
{
	BYTE	m_JmpOpCode[2];			// 0xFF 0x25		= jmp ptr qword
	DWORD	m_JumpPtrOffset;		// offset to jump to the qword ptr (0)
	uint64	m_QWORDTarget;			// address to jump to
} JumpCodeDirectX64_t;

// This relative jump is valid in x64 and x86
typedef struct
{
	BYTE	m_JmpOpCode;			// 0xE9		= near jmp( dword )
	int32	m_JumpOffset;			// offset to jump to
} JumpCodeRelative_t;
#pragma pack( pop )

// Structure to save information about hooked functions that we may need later (ie, for unhooking)
#define MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH 48
typedef struct 
{
	BYTE *m_pFuncHookedAddr;
	BYTE *m_pTrampolineRealFunc;
	BYTE *m_pTrampolineEntryPoint;
	int32 m_nOriginalPreambleLength;
	BYTE m_rgOriginalPreambleCode[ MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH ];
} HookData_t;

class CDetourLock
{
public:
	CDetourLock()
	{
		InitializeCriticalSection( &m_cs );
	}
	~CDetourLock()
	{
		DeleteCriticalSection( &m_cs );
	}

	void Lock()
	{
		EnterCriticalSection( &m_cs );
	}
	void Unlock()
	{
		LeaveCriticalSection( &m_cs );
	}

private:

	CRITICAL_SECTION m_cs;

	// Private and unimplemented to prevent copying
	CDetourLock( const CDetourLock& );
	CDetourLock& operator=( const CDetourLock& );
};

class GetLock
{
public:
	GetLock( CDetourLock& lock )
		: m_lock( lock )
	{
		m_lock.Lock();
	}
	~GetLock()
	{
		m_lock.Unlock();
	}

private:
	GetLock( const GetLock& );
	GetLock& operator=( const GetLock& );

	CDetourLock& m_lock;
};

CDetourLock g_mapLock;

// todo: add marker here so we can find this from VAC
// Set to keep track of all the functions we have hooked
std::map<void *, HookData_t> g_mapHookedFunctions;

#if DEBUG_ENABLE_ERROR_STREAM
// Set to keep track of functions we already reported failures hooking
std::set<void * > g_mapAlreadyReportedDetourFailures;
#endif


// We need at most this many bytes in our allocated trampoline regions, see comments below on HookFunc:
//  - 14 (5 on x86) for jump to real detour address
//  - 32 for copied code (really should be less than this, 5-12?, but leave some space)
//  - 14 (5 on x86) for jump back into body of real function after copied code
#define BYTES_FOR_TRAMPOLINE_ALLOCATION 64

// todo: add some way to find and interpret these from VAC
// Tracking for allocated trampoline memory ready to be used by future hooks
std::vector< void *> g_vecTrampolineRegionsReady;

std::vector< void *> g_vecTrampolinesAllocated;

std::set< const void * > g_setBlacklistedTrampolineSearchAddresses;

class CTrampolineRegionMutex
{
public:
	CTrampolineRegionMutex()
	{
		m_hMutex = ::CreateMutexA( NULL, FALSE, NULL );
	}

	bool BLock( DWORD dwTimeout )
	{
		if( WaitForSingleObject( m_hMutex, dwTimeout ) != WAIT_OBJECT_0 )
		{
			return false;
		}
		return true;
	}

	void Release()
	{
		ReleaseMutex( m_hMutex );
	}

private:
	HANDLE m_hMutex;

	// Private and unimplemented to prevent copying
	CTrampolineRegionMutex( const CTrampolineRegionMutex& );
	CTrampolineRegionMutex& operator=( const CTrampolineRegionMutex& );
};
CTrampolineRegionMutex g_TrampolineRegionMutex;


static inline DWORD GetSystemPageSize()
{
	static DWORD dwSystemPageSize = 0;
	if ( !dwSystemPageSize )
	{
		SYSTEM_INFO sysInfo;
		::GetSystemInfo( &sysInfo );
		dwSystemPageSize = sysInfo.dwPageSize;
		Log( "System page size: %u\n", dwSystemPageSize );
	}
	return dwSystemPageSize;
}


//-----------------------------------------------------------------------------
// Purpose: Function to find an existing trampoline region we've allocated near
// the area we need it.
//-----------------------------------------------------------------------------
BYTE * GetTrampolineRegionNearAddress( const void *pAddressToFindNear )
{
	if ( !g_TrampolineRegionMutex.BLock( 1000 ) )
		Log( "Couldn't get trampoline region lock, will continue possibly unsafely.\n" );

	BYTE *pTrampolineAddress = NULL;

	// First, see if we can find a trampoline address to use in range in our already allocated set
	std::vector<void *>::iterator iter;
	for( iter = g_vecTrampolineRegionsReady.begin(); iter != g_vecTrampolineRegionsReady.end(); ++iter )
	{
		int64 qwAddress = (int64)(*iter);
		int64 qwOffset = qwAddress - (int64)pAddressToFindNear;
		if ( qwOffset < 0 && qwOffset > LONG_MIN || qwOffset > 0 && qwOffset+BYTES_FOR_TRAMPOLINE_ALLOCATION < LONG_MAX )
		{
			pTrampolineAddress = (BYTE*)qwAddress;
			//Log( "Using already allocated trampoline block at %I64d, distance is %I64d\n", qwAddress, qwOffset );
			g_vecTrampolineRegionsReady.erase( iter );
			break;
		}
	}

	g_TrampolineRegionMutex.Release();

	return pTrampolineAddress;
}


//-----------------------------------------------------------------------------
// Purpose: Return trampoline address for use, maybe we failed detours and didn't end up using
//-----------------------------------------------------------------------------
void ReturnTrampolineAddress( BYTE *pTrampolineAddress )
{
	if ( !g_TrampolineRegionMutex.BLock( 1000 ) )
		Log( "Couldn't get trampoline region lock, will continue possibly unsafely.\n" );

	g_vecTrampolineRegionsReady.push_back( pTrampolineAddress );

	g_TrampolineRegionMutex.Release();
}


//-----------------------------------------------------------------------------
// Purpose: Function to allocate new trampoline regions near a target address, call
// only if GetTrampolineRegionNearAddress doesn't return you any existing region to use.
//-----------------------------------------------------------------------------
void AllocateNewTrampolineRegionsNearAddress( const void *pAddressToAllocNear )
{
	if ( !g_TrampolineRegionMutex.BLock( 1000 ) )
		Log( "Couldn't get trampoline region lock, will continue possibly unsafely.\n" );

	// Check we didn't blacklist trying to allocate regions near this address because no memory could be found already,
	// otherwise we can keep trying and trying and perf is awful
	if ( g_setBlacklistedTrampolineSearchAddresses.find( pAddressToAllocNear ) != g_setBlacklistedTrampolineSearchAddresses.end() )
	{
		g_TrampolineRegionMutex.Release();
		return;
	}

	// Get handle to process
	HANDLE hProc = GetCurrentProcess();

	// First, need to know system page size, determine now if we haven't before
	DWORD dwSystemPageSize = GetSystemPageSize();

	BYTE * pTrampolineAddress = NULL;
	if ( pAddressToAllocNear == NULL )
	{
		//Log( "Allocating trampoline page at random location\n" );
		pTrampolineAddress = (BYTE *)VirtualAllocEx( hProc, NULL, dwSystemPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
		if ( !pTrampolineAddress )
		{
			Log ( "Failed allocating memory during hooking: %d\n", GetLastError() );
		}
		else
		{
			g_vecTrampolinesAllocated.push_back( pTrampolineAddress );
		}
	}
	else
	{
		//Log( "Allocating trampoline page at targeted location\n" );

		// Ok, we'll search for the closest page that is free and within +/- 2 gigs from our code.
		int64 qwPageToOffsetFrom = (int64)pAddressToAllocNear - ( (int64)pAddressToAllocNear % dwSystemPageSize );

		int64 qwPageToTryNegative = qwPageToOffsetFrom - dwSystemPageSize;
		int64 qwPageToTryPositive = qwPageToOffsetFrom + dwSystemPageSize;

		bool bLoggedFailures = false;

		while ( !pTrampolineAddress )
		{
			int64 *pqwPageToTry;
			bool bDirectionPositive = false;
			if ( qwPageToOffsetFrom - qwPageToTryNegative < qwPageToTryPositive - qwPageToOffsetFrom )
			{
				pqwPageToTry = &qwPageToTryNegative;
			}
			else
			{
				pqwPageToTry = &qwPageToTryPositive;
				bDirectionPositive = true;
			}

			//Log( "Real func at: %I64d, checking %I64d\n", (int64)pFuncToHook, (*pqwPageToTry) );
			MEMORY_BASIC_INFORMATION memInfo;
			if ( !VirtualQuery( (void *)(*pqwPageToTry), &memInfo, sizeof( memInfo ) ) )
			{
				if ( !bLoggedFailures )
				{
					Log( "VirtualQuery failures\n" );
					bLoggedFailures = true;
				}
			}
			else
			{
				if ( memInfo.State == MEM_FREE )
				{
					pTrampolineAddress = (BYTE *)VirtualAllocEx( hProc, (void*)(*pqwPageToTry), dwSystemPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
					if ( !pTrampolineAddress )
					{
						// Skip this page, another thread may have alloced it while we tried or something, just find the next usuable one
						if ( bDirectionPositive )
							qwPageToTryPositive += dwSystemPageSize;
						else
							qwPageToTryNegative -= dwSystemPageSize;
						continue;
					}
					g_vecTrampolinesAllocated.push_back( pTrampolineAddress );

					break;
				}
			}

			// Increment page and try again, we can skip ahead RegionSize bytes because
			// we know all pages in that region have identical info.
			if ( bDirectionPositive )
				qwPageToTryPositive += memInfo.RegionSize;
			else
				qwPageToTryNegative -= memInfo.RegionSize;

			if ( qwPageToTryPositive + dwSystemPageSize >= (int64)pAddressToAllocNear + LONG_MAX 
				&& qwPageToTryNegative <= (int64)pAddressToAllocNear - LONG_MIN )
			{
				Log ( "Could not find page for trampoline in +/- 2GB range of function to hook\n" );
				g_setBlacklistedTrampolineSearchAddresses.insert( pAddressToAllocNear );
				break;
			}
		}
	}

	// If we succeeded allocating a trampoline page, then track the extra pages for later use
	if ( pTrampolineAddress )
	{
		// Track the extra space in the page for future use
		BYTE *pNextTrampolineAddress = pTrampolineAddress;
		while ( pNextTrampolineAddress <= pTrampolineAddress+dwSystemPageSize-BYTES_FOR_TRAMPOLINE_ALLOCATION )
		{
			g_vecTrampolineRegionsReady.push_back( pNextTrampolineAddress );
			pNextTrampolineAddress += BYTES_FOR_TRAMPOLINE_ALLOCATION;
		}
	}

	g_TrampolineRegionMutex.Release();
	return;
}


//-----------------------------------------------------------------------------
// Purpose: RegregisterTrampolines
// when we first allocated these trampolines, our VirtualAlloc/Protect
// monitoring wasnt set up, just re-protect them and that will get them
// recorded so we know they are ours
// could use this code to remove write permission from them
// except that we will redo a bunch of hooking on library load ( PerformHooking )
//-----------------------------------------------------------------------------
void RegregisterTrampolines()
{
	if ( !g_TrampolineRegionMutex.BLock( 1000 ) )
		Log( "Couldn't get trampoline region lock, will continue possibly unsafely.\n" );

	// First, need to know system page size, determine now if we haven't before
	DWORD dwSystemPageSize = GetSystemPageSize();

	std::vector<void *>::iterator iter;
	for( iter = g_vecTrampolinesAllocated.begin(); iter != g_vecTrampolinesAllocated.end(); ++iter )
	{
		DWORD flOldProtect;
		VirtualProtect( *iter, dwSystemPageSize, PAGE_EXECUTE_READWRITE, &flOldProtect );
	}

	g_TrampolineRegionMutex.Release();
}


//-----------------------------------------------------------------------------
// Purpose: Check if a given address range is fully covered by executable pages
//-----------------------------------------------------------------------------
static bool BIsAddressRangeExecutable( const void *pAddress, size_t length )
{
	MEMORY_BASIC_INFORMATION memInfo;
	if ( !VirtualQuery( (const void *)pAddress, &memInfo, sizeof( memInfo ) ) )
		return false;

	if ( memInfo.State != MEM_COMMIT )
		return false;

	if ( memInfo.Protect != PAGE_EXECUTE && memInfo.Protect != PAGE_EXECUTE_READ &&
		memInfo.Protect != PAGE_EXECUTE_READWRITE && memInfo.Protect != PAGE_EXECUTE_WRITECOPY )
	{
		return false;
	}

	uintp lastAddress = (uintp)pAddress + length - 1;
	uintp lastInRegion = (uintp)memInfo.BaseAddress + memInfo.RegionSize - 1;
	if ( lastAddress <= lastInRegion )
		return true;

	// Start of this address range is executable. But what about subsequent regions?
	return BIsAddressRangeExecutable( (const void*)(lastInRegion + 1), lastAddress - lastInRegion );
}


//-----------------------------------------------------------------------------
// Purpose: Hook a function (at pRealFunctionAddr) causing calls to it to instead call
// our own function at pHookFunctionAddr.  We'll return a pointer to code that can be
// called as the original function by our hook code and will have the original unhooked
// behavior.
//
// The nJumpsToFolowBeforeHooking parameter determines what we will do if we find an E9 
// or FF 25 jmp instruction at the beginning of the code to hook.  This probably means the 
// function is  already hooked.  We support both hooking the original address and chaining 
// to the old hook  then, or alternatively following the jump and hooking it's target.  Sometimes 
// this follow  then hook is preferable because other hook code may not chain nicely and may 
// overwrite our hook if we try to put it first (ie, FRAPS & ATI Tray Tools from Guru3d)
//-----------------------------------------------------------------------------
#pragma warning( push )
#pragma warning( disable : 4127 ) // conditional expression is constant, from sizeof( intp ) checks

static bool HookFuncInternal( BYTE *pRealFunctionAddr, const BYTE *pHookFunctionAddr, void ** ppRealFunctionAdr, BYTE **ppTrampolineAddressToReturn, int nJumpsToFollowBeforeHooking );

void * HookFunc( BYTE *pRealFunctionAddr, const BYTE *pHookFunctionAddr, int nJumpsToFollowBeforeHooking /* = 0 */ )
{
	void *pTrampolineAddr = NULL;
	if ( !HookFuncSafe( pRealFunctionAddr, pHookFunctionAddr, (void **)&pTrampolineAddr, nJumpsToFollowBeforeHooking ) )
		return NULL;

	return pTrampolineAddr;
}

bool HookFuncSafe( BYTE *pRealFunctionAddr, const BYTE *pHookFunctionAddr, void ** ppRealFunctionAdr, int nJumpsToFollowBeforeHooking /* = 0 */ )
{
	// If hook setting fails, then the trampoline is not being used, and can be returned to our pool
	BYTE *pTrampolineAddressToReturn = NULL;
	bool bRet = HookFuncInternal( pRealFunctionAddr, pHookFunctionAddr, ppRealFunctionAdr, &pTrampolineAddressToReturn, nJumpsToFollowBeforeHooking );

	if ( pTrampolineAddressToReturn )
	{
		ReturnTrampolineAddress( pTrampolineAddressToReturn );
	}

	return bRet;
}

// We detour with the following setup:
//
// 1) Allocate some memory within 2G range from the function we are detouring (we search with VirtualQuery to find where to alloc)
// 2) Place a relative jump E9 opcode (only 5 bytes) at the beginning of the original function to jump to our allocated memory
// 3) At the start of our allocated memory we place an absolute jump (FF 25, 6 bytes on x86, 14 on x64 because instead of being 
//    an absolute dword ptr, it has a relative offset to a qword ptr).  This jump goes to our hook function we are detouring to,
//    the E9 at the start of the original function jumps to this, then this goes to the real function which may be more than 2G away.
// 4) We copy the original 5 bytes + slop for opcode boundaries into the remaining space in our allocated region, after that we place a FF 25 jump
//    jump back to the original function 6 bytes in (or a little more if the opcodes didn't have a boundary at 5 bytes).
// 5) We return a ptr to the original 5 bytes we copied's new address and that is the "real function ptr" that our hook function can call
//    to call the original implementation.
//
// This method is good because it works with just 5 bytes overwritten in the original function on both x86 and x64, the only tricky part
// is that we have to search for a page we can allocate within 2 gigabytes of the function address and if we can't find one we can fail 
// (which would only happen on x64, and doesn't really happen in practice).  If it did start to happen more we could fallback to trying to 
// put the full 14 byte FF 25 x64 jmp at the start of the function, but many functions are too short or make calls that can't be easily relocated,
// or have other code jumping into them at less than 14 bytes, so thats not very safe.
static bool HookFuncInternal( BYTE *pRealFunctionAddr, const BYTE *pHookFunctionAddr, void ** ppRealFunctionAdr, BYTE **ppTrampolineAddressToReturn, int nJumpsToFollowBeforeHooking )
{
	if ( !pRealFunctionAddr )
	{
		Log( "Aborting HookFunc because pRealFunctionAddr is null\n" );
		return false;
	}

	if ( !pHookFunctionAddr )
	{
		Log( "Aborting HookFunc because pHookFunctionAddr is null\n" );
		return false;
	}

	// Make sure we aren't double-hooking a function, in case someone else installed a hook
	// after ours which made us think that our hook was removed when it was really just relocated.
	// UnhookFunc will short-circuit the trampoline and bypass our old hook even if it can't
	// fully undo the jump into our trampoline code.
	UnhookFunc( pRealFunctionAddr, false /*bLogFailures*/ );

	HANDLE hProc = GetCurrentProcess();
	BYTE *pFuncToHook = pRealFunctionAddr;

	// See if there is already a hook in place on this code and we have been instructed to follow it and hook 
	// the target instead.
	while( nJumpsToFollowBeforeHooking > 0 )
	{
		if ( pFuncToHook[0] == 0xEB )
		{
			int8 * pOffset = (int8 *)(pFuncToHook + 1);
			pFuncToHook = (BYTE*)((intp)pFuncToHook + 2 + *pOffset);
		}
		else if ( pFuncToHook[0] == 0xE9 )
		{
			// Make sure the hook isn't pointing at the same place we are going to detour to (which would mean we've already hooked)
			int32 * pOffset = (int32 *)(pFuncToHook+1);
			pFuncToHook = (BYTE*)((intp)pFuncToHook + 5 + *pOffset);
		}
#ifdef _WIN64
		else if ( pFuncToHook[0] == 0xFF && pFuncToHook[1] == 0x25 )
		{

			// On x64 we have a signed 32-bit relative offset to an absolute qword ptr
			int32 * pOffset = (int32 *)(pFuncToHook+2);
			intp *pTarget = (intp*)(pFuncToHook + 6 + *pOffset);
			pFuncToHook = (BYTE*)*pTarget;
		}
#endif
		else
		{
			// Done, no more chained jumps
			break;
		}

		--nJumpsToFollowBeforeHooking;
	}


	// If the function pointer isn't marked as executable code, or there isn't enough room for our jump, warn
	if ( !BIsAddressRangeExecutable( pFuncToHook, sizeof( JumpCodeRelative_t ) ) )
	{
		Log( "Warning: hook target starting at %#p covers a non-executable page\n", (void*)pFuncToHook );
		// non-fatal, as system may not be enforcing Data Execution Prevention / hardware NX-bit.
	}

	// Special blacklist: if the function begins with an unconditional 2-byte jump, it is unhookable!
	// If this becomes necessary, we could follow the jump to see where it goes, and hook there instead.
	if ( (BYTE) pFuncToHook[0] == 0xEB )
	{
		Log( "Warning: hook target starting at %#p begins with uncoditional 2-byte jump, skipping\n", (void*)pFuncToHook );
		return false;
	}

	// This struct will get reused a bunch to compose jumps
	JumpCodeRelative_t sRelativeJumpCode;
	sRelativeJumpCode.m_JmpOpCode = 0xE9;
	//sRelativeJumpCode.m_JumpOffset = ...;
	
	// On X64, we use this struct for jumps > +/-2GB
	JumpCodeDirectX64_t sDirectX64JumpCode;
	sDirectX64JumpCode.m_JmpOpCode[0] = 0xFF;
	sDirectX64JumpCode.m_JmpOpCode[1] = 0x25;
	sDirectX64JumpCode.m_JumpPtrOffset = 0;
	//sDirectX64JumpCode.m_QWORDTarget = ...;

	// We need to figure out if we recognize the preamble for the
	// current function so we can match it up with a good hook code length
	int32 nHookCodeLength = 0;
	BYTE *pOpcode = pFuncToHook;
	bool bParsedRETOpcode = false;

	BYTE rgCopiedCode[ MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH ];

	// we just need a minimum of 5 bytes for our hook code
	while ( nHookCodeLength < sizeof( JumpCodeRelative_t ) )
	{
		int nLength;
		EOpCodeOffsetType eOffsetType;
		bool bKnown = ParseOpcode( pOpcode, nLength, eOffsetType );

		if ( bKnown )
		{
			// Make sure that if we hook a RET, it is the last byte, or followed by only INT 3 or NOP
			// inter-function padding. If this causes hooks to fail, then we need to be smarter
			// about examining relative jumps to determine the boundaries of the function, so
			// that we know if the RET is an early-out and the function continues onward or not.
			// We are trying hard to avoid overwriting the start of another function, in case
			// the target function is very small and there is no padding afterwards.
			if ( bParsedRETOpcode && *pOpcode != 0xCC && *pOpcode != 0x90 )
			{
				Log( "Warning: hook target starting at %#p contains early RET\n", (void*)pFuncToHook );
				// fall through to expanded error reporting below by setting bKnown to false
				bKnown = false;
			}
		
			if ( *pOpcode == 0xC3 || *pOpcode == 0xC2 )
			{
				bParsedRETOpcode = true;
			}
		}

		if ( !bKnown || 
		     ( eOffsetType != k_ENoRelativeOffsets && eOffsetType != k_EDWORDOffsetAtByteTwo && eOffsetType != k_EDWORDOffsetAtByteThree 
			   && eOffsetType != k_EBYTEOffsetAtByteTwo && eOffsetType != k_EDWORDOffsetAtByteFour ) )
		{
#if DEBUG_ENABLE_ERROR_STREAM
			bool bAlreadyReported = true;
			{
				GetLock getLock( g_mapLock );
				if ( g_mapAlreadyReportedDetourFailures.find( pFuncToHook ) == g_mapAlreadyReportedDetourFailures.end() )
				{
					bAlreadyReported = false;
					g_mapAlreadyReportedDetourFailures.insert( pFuncToHook );
				}
			}

			ErrorStreamMsg_t msg;
			_snprintf( msg.rgchError, sizeof( msg.rgchError ), "Unknown opcodes for %s at %d bytes for func %#p: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
#ifdef _WIN64
				"AMD64",
#else
				"X86",
#endif
				nHookCodeLength,
				pFuncToHook,
				pFuncToHook[0], pFuncToHook[1], pFuncToHook[2], pFuncToHook[3],
				pFuncToHook[4], pFuncToHook[5], pFuncToHook[6], pFuncToHook[7],
				pFuncToHook[8], pFuncToHook[9], pFuncToHook[10], pFuncToHook[11],
				pFuncToHook[12], pFuncToHook[13], pFuncToHook[14], pFuncToHook[15]
			);

			Log( msg.rgchError );

			if ( !bAlreadyReported )
			{

				msg.unStrLen = (uint32)strlen( msg.rgchError );
				if ( !g_pDetourErrorStream )
					g_pDetourErrorStream = new CSharedMemStream( "GameOverlayRender_DetourErrorStream", SHMEMSTREAM_SIZE_ONE_KBYTE*32, 50 );

				g_pDetourErrorStream->Put( &msg, sizeof( msg.unStrLen ) + msg.unStrLen );
			}
#endif

			return false;
		}

		// make sure we have enough room, we should always have enough unless an opcode is huge!
		if ( sizeof( rgCopiedCode ) - nHookCodeLength - nLength < 0 )
		{
			Log( "Not enough room to copy function preamble\n" );
			return false;
		}

		// Copy the bytes into our local buffer
		memcpy( &rgCopiedCode[ nHookCodeLength ], pOpcode, nLength );

		pOpcode += nLength;
		nHookCodeLength += nLength;
	}

	// We only account for a max of 32 bytes that needs relocating in our allocated trampoline area
	// if we are over that complain and fail, should never hit this
	if ( nHookCodeLength > MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH )
	{
		Log( "Copied more than MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH bytes to make room for E9 jmp of 5 bytes?  Bad opcode parsing?\n" );
		return false;
	}


	// We need to find/allocate a region for our trampoline that is within +/-2GB of the function we are hooking.
	BYTE *pTrampolineAddress = GetTrampolineRegionNearAddress( pFuncToHook );
	if ( !pTrampolineAddress )
	{
		AllocateNewTrampolineRegionsNearAddress( pFuncToHook );
		pTrampolineAddress = GetTrampolineRegionNearAddress( pFuncToHook );
	}
	// Total failure at this point, couldn't allocate memory close enough.
	if ( !pTrampolineAddress )
	{
		Log( "Error allocating trampoline memory (no memory within +/-2gb? prior failures?)\n" );
		return false;
	}
	// Store the trampoline address to output parameter so caller can clean up on failure
	*ppTrampolineAddressToReturn = pTrampolineAddress;


	// Save the original function preamble so we can restore it later
	HookData_t SavedData;
	memcpy( SavedData.m_rgOriginalPreambleCode, rgCopiedCode, MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH );
	SavedData.m_nOriginalPreambleLength = nHookCodeLength;
	SavedData.m_pFuncHookedAddr = pFuncToHook;
	SavedData.m_pTrampolineRealFunc = NULL;
	SavedData.m_pTrampolineEntryPoint = NULL;

	// Now fixup any relative offsets in our copied code to account for the new relative base pointer,
	// since the copied code will be executing from the trampoline area instead of its original location
	int nFixupPosition = 0;
	while( nFixupPosition < nHookCodeLength )
	{
		int nLength;
		EOpCodeOffsetType eOffsetType;
		bool bKnown = ParseOpcode( &rgCopiedCode[nFixupPosition], nLength, eOffsetType );

		if ( !bKnown || 
			 ( eOffsetType != k_ENoRelativeOffsets && eOffsetType != k_EDWORDOffsetAtByteTwo  && eOffsetType != k_EDWORDOffsetAtByteThree 
			   && eOffsetType != k_EBYTEOffsetAtByteTwo && eOffsetType != k_EDWORDOffsetAtByteFour ) )
		{
			Log( "Failed parsing copied bytes during detour -- shouldn't happen as this is a second pass: position %d\n"
				"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", nFixupPosition,
				rgCopiedCode[0], rgCopiedCode[1], rgCopiedCode[2], rgCopiedCode[3],
				rgCopiedCode[4], rgCopiedCode[5], rgCopiedCode[6], rgCopiedCode[7],
				rgCopiedCode[8], rgCopiedCode[9], rgCopiedCode[10], rgCopiedCode[11],
				rgCopiedCode[12], rgCopiedCode[13], rgCopiedCode[14], rgCopiedCode[15],
				rgCopiedCode[16], rgCopiedCode[17], rgCopiedCode[18], rgCopiedCode[19] );

			return false;
		}

		// If there is a relative offset, we need to fix it up according to how far we moved the code
		int iPositionOfDWORDFixup = -1;
		switch ( eOffsetType )
		{
		case k_ENoRelativeOffsets:
			break;

		case k_EDWORDOffsetAtByteTwo:
			iPositionOfDWORDFixup = 1;
			break;

		case k_EDWORDOffsetAtByteThree:
			iPositionOfDWORDFixup = 2;
			break;

		case k_EDWORDOffsetAtByteFour:
			iPositionOfDWORDFixup = 3;
			break;
		
		case k_EBYTEOffsetAtByteTwo:
			// We need explicit knowledge of the opcode here so that we can convert it to DWORD-offset form
			if ( (BYTE)rgCopiedCode[nFixupPosition] == 0xEB && nLength == 2 )
			{
				if ( nHookCodeLength + 3 > MAX_HOOKED_FUNCTION_PREAMBLE_LENGTH )
				{
					Log( "Can't fixup EB jmp because there isn't enough room to expand to E9 jmp\n" );
					return false;
				}
				rgCopiedCode[nFixupPosition] = 0xE9;
				memmove( &rgCopiedCode[nFixupPosition + 5], &rgCopiedCode[nFixupPosition + 2], nHookCodeLength - nFixupPosition - 2 );

				// Expand from 8-bit signed offset to 32-bit signed offset, and remember it for address fixup below
				// (subtract 3 from offset to account for additional length of the replacement JMP instruction)
				int32 iOffset = (int8) rgCopiedCode[nFixupPosition + 1] - 3;
				memcpy( &rgCopiedCode[nFixupPosition + 1], &iOffset, 4 );
				iPositionOfDWORDFixup = 1;

				// This opcode and the total amount of copied data grew by 3 bytes
				nLength += 3;
				nHookCodeLength += 3;
			}
			else
			{
				Log( "Opcode %x of type k_EBYTEOffsetAtByteTwo can't be converted to larger relative address\n", rgCopiedCode[nFixupPosition] );
				return false;
			}
			break;
		default:
			Log( "Unknown opcode relative-offset enum value %d\n", (int)eOffsetType );
			return false;
		}

		if ( iPositionOfDWORDFixup != -1 )
		{
			int32 iOffset;
			memcpy( &iOffset, &rgCopiedCode[ nFixupPosition + iPositionOfDWORDFixup ], 4 );

			intp iNewOffset = iOffset + (intp)pFuncToHook - (intp)pTrampolineAddress;
			iOffset = (int32)iNewOffset;
			// On 32-bit platforms, 32-bit relative mode can reach any valid address.
			// On 64-bit platforms, 32-bit relative mode can only reach addresses +/- 2GB.
			if ( sizeof(intp) > sizeof(int32) && (intp)iOffset != iNewOffset )
			{
				Log( "Can't relocate and adjust offset because offset is too big after relocation.\n" );
				return false;
			}
			memcpy( &rgCopiedCode[ nFixupPosition + iPositionOfDWORDFixup ], &iOffset, 4 );
		}

		nFixupPosition += nLength;
	}

	// Copy out the original code to our allocated memory to save it, keep track of original trampoline beginning
	BYTE *pBeginTrampoline = pTrampolineAddress; 
	SavedData.m_pTrampolineRealFunc = pTrampolineAddress;
	
	memcpy( pTrampolineAddress, rgCopiedCode, nHookCodeLength );
	pTrampolineAddress += nHookCodeLength; // move pointer forward past copied code

	// Place a jump at the end of the copied code to jump back to the rest of the post-hook function body
	intp lJumpTarget = (intp)pFuncToHook + nHookCodeLength;
	intp lJumpInstruction = (intp)pTrampolineAddress;
	intp lJumpOffset = lJumpTarget - lJumpInstruction - sizeof( JumpCodeRelative_t );
	sRelativeJumpCode.m_JumpOffset = (int32)lJumpOffset;

	// On 64-bit platforms, 32-bit relative addressing can only reach addresses +/- 2GB.
	if ( sizeof(intp) > sizeof(int32) && (intp)sRelativeJumpCode.m_JumpOffset != lJumpOffset )
	{
		// Use a direct 64-bit jump instead
		sDirectX64JumpCode.m_QWORDTarget = lJumpTarget;
		memcpy( pTrampolineAddress, &sDirectX64JumpCode, sizeof( JumpCodeDirectX64_t ) );
		pTrampolineAddress += sizeof( JumpCodeDirectX64_t );
	}
	else
	{
		memcpy( pTrampolineAddress, &sRelativeJumpCode, sizeof( JumpCodeRelative_t ) );
		pTrampolineAddress += sizeof( JumpCodeRelative_t );
	}


	// Ok, now write the other half of the trampoline, which is the entry point that we will make the
	// hooked function jump to. This will in turn jump into our hook function, which may then call the
	// original function bytes that we relocated into the start of the trampoline.
	SavedData.m_pTrampolineEntryPoint = pTrampolineAddress;
	BYTE *pIntermediateJumpLocation = pTrampolineAddress;

	lJumpTarget = (intp)pHookFunctionAddr;
	lJumpInstruction = (intp)pIntermediateJumpLocation;
	lJumpOffset = lJumpTarget - lJumpInstruction - sizeof( JumpCodeRelative_t );
	sRelativeJumpCode.m_JumpOffset = (int32)lJumpOffset;

	if ( sizeof(intp) > sizeof(int32) && (intp)sRelativeJumpCode.m_JumpOffset != lJumpOffset )
	{
		sDirectX64JumpCode.m_QWORDTarget = lJumpTarget;
		memcpy( pTrampolineAddress, &sDirectX64JumpCode, sizeof( JumpCodeDirectX64_t ) );
		pTrampolineAddress += sizeof( JumpCodeDirectX64_t );
	}
	else
	{
		memcpy( pTrampolineAddress, &sRelativeJumpCode, sizeof( JumpCodeRelative_t ) );
		pTrampolineAddress += sizeof( JumpCodeRelative_t );
	}

	// Now flush instruction cache to ensure the processor detects the changed memory.
	FlushInstructionCache( hProc, pBeginTrampoline, pTrampolineAddress - pBeginTrampoline );


	// Trampoline is done; write jump-into-trampoline over the original function body
	lJumpTarget = (intp)pIntermediateJumpLocation;
	lJumpInstruction = (intp)pFuncToHook;
	lJumpOffset = lJumpTarget - lJumpInstruction - sizeof( JumpCodeRelative_t );
	sRelativeJumpCode.m_JumpOffset = (int32)lJumpOffset;

	if ( sizeof(intp) > sizeof(int32) && (intp)sRelativeJumpCode.m_JumpOffset != lJumpOffset )
	{
		// Shouldn't ever hit this, since we explicitly found an address to place the intermediate
		// trampoline which was close enough.
		Log( "Warning: Jump from function to intermediate trampoline is too far! Shouldn't happen." );
		return false;
	}

	// Jump is prepared for writing, now adjust virtual protection and overwrite the function start

	DWORD dwSystemPageSize = GetSystemPageSize();

	void *pLastHookByte = pFuncToHook + sizeof( JumpCodeRelative_t ) - 1;
	bool bHookSpansTwoPages = ( (uintp)pFuncToHook / dwSystemPageSize != (uintp)pLastHookByte / dwSystemPageSize );

	DWORD dwOldProtectionLevel = 0;
	DWORD dwOldProtectionLevel2 = 0;
	DWORD dwIgnore;

	// Fix up the protection on the memory where the functions current asm code is
	// so that we will be able read/write it.
	if( !VirtualProtect( pFuncToHook, 1, PAGE_EXECUTE_READWRITE, &dwOldProtectionLevel ) )
	{
		Log( "Warning: VirtualProtect call failed during hook attempt\n" );
		return false;
	}

	// In case the hook spans a page boundary, also adjust protections on the last byte,
	// and track the memory protection for the second page in a separate variable since
	// it could theoretically be different (although that would be very odd).
	if ( bHookSpansTwoPages && !VirtualProtect( pLastHookByte, 1, PAGE_EXECUTE_READWRITE, &dwOldProtectionLevel2 ) )
	{
		// Restore original protection on first page.
		VirtualProtect( pFuncToHook, 1, dwOldProtectionLevel, &dwIgnore );
		Log( "Warning: VirtualProtect (2) call failed during hook attempt\n" );
		return false;
	}

	bool bSuccess = false;

	// We must store the relocated function address to the output variable after the trampoline
	// is written, but BEFORE the hook is written, because once the hook is written it could be
	// executed by anybody on any thread, and it needs to know the real function address.
	*ppRealFunctionAdr = pBeginTrampoline;

	// Write new function body which jumps to trampoline which runs our hook and then relocated function bits
	SIZE_T cBytesWritten;
	if( !WriteProcessMemory( hProc, (void *)pFuncToHook, &sRelativeJumpCode, sizeof( JumpCodeRelative_t ), &cBytesWritten ) )
	{
		Log( "WriteProcessMemory() call failed trying to overwrite first 5 bytes of function body during hook\n" );
	}
	else
	{
		// From this point on, we must return success because we wrote a live jump into the trampoline
		*ppTrampolineAddressToReturn = NULL;
		bSuccess = true;

		if ( !FlushInstructionCache( hProc, (void*)pFuncToHook, sizeof( JumpCodeRelative_t ) ) )
		{
			// if flush instruction cache fails what should we do?
			Log( "FlushInstructionCache() call failed trying to overwrite first 5 bytes of function body during hook\n" );
		}
	}

	// Restore the original protection flags regardless of success, unless they already matched, then don't bother
	if ( bHookSpansTwoPages && dwOldProtectionLevel2 != PAGE_EXECUTE_READWRITE && dwOldProtectionLevel2 != PAGE_EXECUTE_WRITECOPY )
	{
		if ( !VirtualProtect( pLastHookByte, 1, dwOldProtectionLevel2, &dwIgnore ) )
		{
			Log( "Warning: VirtualProtect (2) call failed to restore protection flags during hook attempt\n" );
		}
	}
	if ( dwOldProtectionLevel != PAGE_EXECUTE_READWRITE && dwOldProtectionLevel != PAGE_EXECUTE_WRITECOPY )
	{
		if( !VirtualProtect( pFuncToHook, 1, dwOldProtectionLevel, &dwIgnore ) )
		{
			Log( "Warning: VirtualProtect call failed to restore protection flags during hook attempt\n" );
		}
	}

	// Track that we have hooked the function at this address
	if ( bSuccess )
	{
		GetLock getLock( g_mapLock );
		g_mapHookedFunctions[ (void *)pRealFunctionAddr ] = SavedData;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Check if windows says a given address is committed.  Used to make
// sure we don't follow jumps into unloaded modules due to other apps bad detours
// code.  
//-----------------------------------------------------------------------------
static bool BIsAddressCommited( const void *pAddress )
{
	MEMORY_BASIC_INFORMATION memInfo;
	if ( !VirtualQuery( pAddress, &memInfo, sizeof( memInfo ) ) )
	{
		return false;
	}

	if ( memInfo.State == MEM_COMMIT )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Check if we have already hooked a function at a given address.
// Params: pRealFunctionAddr -- the address of the function to detour.
//         pHookFunc -- optional, and if given is the function we want to detour to. 
//         Providing it will allow additional detection to make sure a detour to 
//         the target isn't already set via an E9 or chain of E9 calls at the start
//         of the function.
//-----------------------------------------------------------------------------
bool bIsFuncHooked( BYTE *pRealFunctionAddr, void *pHookFunc /* = NULL */ )
{
	if ( !pRealFunctionAddr )
		return false;

	{
		GetLock getLock( g_mapLock );
		if ( g_mapHookedFunctions.find( (void*)pRealFunctionAddr ) != g_mapHookedFunctions.end() )
		{
			if ( *pRealFunctionAddr != 0xE9 
#ifdef _WIN64
				&& ( *pRealFunctionAddr != 0xFF || *(pRealFunctionAddr+1) != 0x25 ) 
#endif
			)
			{
				Log( "Warning: Function we had previously hooked now appears unhooked.\n" );
			}
			return true;
		}
	}



	// If we were told what the hook func address is we can do more checking to avoid infinite recursion
	BYTE *pFuncToHook = pRealFunctionAddr;

	int nJumpsToCheckForExistingHook = 5;
	BYTE * pCurrentDetour = pFuncToHook;
	while( nJumpsToCheckForExistingHook )
	{
		// We defensively check all the pointers we find following the detour chain
		// to make sure they are at least in commited pages to try to avoid following
		// bad jmps.  We can end up in bad jmps due to badly behaving third-party detour 
		// code. 
		if ( !BIsAddressCommited( pCurrentDetour ) )
			return false;

		if ( pCurrentDetour[0] == 0xE9 )
		{
			// Make sure the hook isn't pointing at the same place we are going to detour to (which would mean we've already hooked)
			int32 * pOffset = (int32 *)(pCurrentDetour+1);
			if ( !BIsAddressCommited( pOffset ) )
				return false;

			pCurrentDetour = (BYTE*)((int64)pCurrentDetour + 5 + *pOffset);
			if ( pCurrentDetour == pHookFunc )
			{
				Log ( "Trying to hook when already detoured to target addr (by E9)\n" );
				return true;
			}
		}
#ifdef _WIN64
		else if ( pCurrentDetour[0] == 0xFF && pCurrentDetour[1] == 0x25 )
		{
			// On x64 we have a relative offset to an absolute qword ptr
			DWORD * pOffset = (DWORD *)(pCurrentDetour+2);
			if ( !BIsAddressCommited( pOffset ) )
				return false;

			int64 *pTarget = (int64*)(pCurrentDetour + 6 + *pOffset);
			if ( !BIsAddressCommited( pTarget ) )
				return false;

			pCurrentDetour = (BYTE*)*pTarget;
			if ( (void *)pCurrentDetour == pHookFunc )
			{
				Log ( "Trying to hook when already detoured to target addr (by FF 25)\n" );
				return true;
			}
		}
#endif
		else
		{
			// Done, no more chained jumps
			break;
		}
		--nJumpsToCheckForExistingHook;
	}
	

	return false;

}



//-----------------------------------------------------------------------------
// Purpose: Check if any of the functions in our map of already hooked ones appears
// to no longer exist in a valid module, if that has happened its likely the following 
// sequence of events has occurred:
//
// hMod = LoadLibrary( "target.dll" );
// ...
// DetourFunc called on method in target.dll
// ...
// FreeLibrary( hMod ); // ref count to 0 for dll in process
//
// If that has happened, we want to remove the address from the list of hooked code as 
// if the DLL is reloaded the address will likely be the same but the code will be restored
// and no longer hooked.
//-----------------------------------------------------------------------------
void DetectUnloadedHooks()
{
	void **pTestAddresses = NULL;
	uint32 nTestAddresses = 0;

	// Build an array of function addresses to test, naturally sorted ascending due to std::map.
	// Don't hold the lock while we call GetModuleHandleEx or there will be potential to deadlock!
	{
		GetLock getLock( g_mapLock );
		nTestAddresses = (uint32)g_mapHookedFunctions.size();
		pTestAddresses = (void**) malloc( sizeof(void*) * nTestAddresses );
		uint32 i = 0;
		for ( const auto &entry : g_mapHookedFunctions )
		{
			pTestAddresses[i++] = entry.first;
			if ( nTestAddresses == i )
				break;
		}
	}

	// Iterate from high addresses to low, can eliminate some GetModuleHandleExA calls since
	// the HMODULE returned is the module's base address, defining a known-valid module range.
	BYTE *pLoadedModuleBase = NULL;
	for ( uint32 i = nTestAddresses; i--; )
	{
		if ( !pLoadedModuleBase || pLoadedModuleBase > (BYTE*)pTestAddresses[i] )
		{
			HMODULE hMod = NULL;
			if ( !GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)pTestAddresses[i], &hMod ) || !hMod )
			{
				// leave entry alone so that it is erased from map below.
				Log( "Found a hooked function in now unloaded module, removing from map.\n" );
				pLoadedModuleBase = NULL;
				continue;
			}
			pLoadedModuleBase = (BYTE*)hMod;
		}

		// Either we shortcut the test because we already know this module is loaded, or
		// we looked up the function's module and found it to be valid (and remembered it).
		// Swap from back and shorten array.
		pTestAddresses[i] = pTestAddresses[--nTestAddresses];
	}

	// Lock again and delete the entries that we found to be pointing at unloaded modules
	if ( nTestAddresses )
	{
		GetLock getLock( g_mapLock );
		for ( uint32 i = 0; i < nTestAddresses; ++i )
		{
			g_mapHookedFunctions.erase( pTestAddresses[i] );
		}
	}

	free( pTestAddresses );
}


//-----------------------------------------------------------------------------
// Purpose: Unhook a function, this doesn't remove the jump code, it just makes
// it jump back to the original code directly
//-----------------------------------------------------------------------------

void UnhookFunc(  BYTE *pRealFunctionAddr, BYTE *pOriginalFunctionAddr_DEPRECATED )
{
	(void)pOriginalFunctionAddr_DEPRECATED;
	UnhookFunc( pRealFunctionAddr, true );
}

void UnhookFunc( BYTE *pRealFunctionAddr, bool bLogFailures )
{
	if ( !pRealFunctionAddr )
	{
		if ( bLogFailures )
			Log( "Aborting UnhookFunc because pRealFunctionAddr is null\n" );
		return;
	}

	HookData_t hookData;
	{
		GetLock getLock( g_mapLock );
		std::map<void *, HookData_t>::iterator iter;
		iter = g_mapHookedFunctions.find( (void*)pRealFunctionAddr );
		if ( iter == g_mapHookedFunctions.end() )
		{
			if ( bLogFailures )
				Log( "Aborting UnhookFunc because pRealFunctionAddr is not hooked\n" );
			return;
		}
		else
		{
			hookData = iter->second;
			g_mapHookedFunctions.erase( iter );
		}
	}

	DWORD dwSystemPageSize = GetSystemPageSize();
	HANDLE hProc = GetCurrentProcess();

	BYTE *pFuncToUnhook = hookData.m_pFuncHookedAddr;
	void *pLastHookByte = pFuncToUnhook + hookData.m_nOriginalPreambleLength - 1;
	bool bHookSpansTwoPages = ( (uintp)pFuncToUnhook / dwSystemPageSize != (uintp)pLastHookByte / dwSystemPageSize );

	// Write a 2-byte 0xEB jump into the trampoline at the entry point (the jump to our hook function)
	// to cause it to jump again to the start of the saved function bytes instead of calling our hook.
	COMPILE_TIME_ASSERT( BYTES_FOR_TRAMPOLINE_ALLOCATION < 128 );
	union {
		struct {
			uint8 opcode;
			int8 offset;
		} s;
		uint16 u16;
	} smalljump;
	smalljump.s.opcode = 0xEB; // tiny jump to 8-bit immediate offset from next instruction
	smalljump.s.offset = (int8)( hookData.m_pTrampolineRealFunc - ( hookData.m_pTrampolineEntryPoint + 2 ) );

	*(UNALIGNED uint16*)hookData.m_pTrampolineEntryPoint = smalljump.u16;
	FlushInstructionCache( hProc, hookData.m_pTrampolineEntryPoint, 2 );

	if ( !BIsAddressCommited( pFuncToUnhook ) )
	{
		if ( bLogFailures )
			Log( "UnhookFunc not restoring original bytes - function is unmapped\n" );
		return;
	}

	// Check that the function still starts with our 0xE9 jump before slamming it back to original code
	if ( *pFuncToUnhook != 0xE9 )
	{
		if ( bLogFailures )
			Log( "UnhookFunc not restoring original bytes - jump instruction not found\n" );
		return;
	}
	
	BYTE *pJumpTarget = pFuncToUnhook + 5 + *(UNALIGNED int32*)(pFuncToUnhook + 1);
	if ( pJumpTarget != hookData.m_pTrampolineEntryPoint )
	{
		if ( bLogFailures )
			Log( "UnhookFunc not restoring original bytes - jump target has changed\n" );
		return;
	}

	DWORD dwOldProtectionLevel = 0;
	DWORD dwOldProtectionLevel2 = 0;
	DWORD dwIgnore;

	// Fix up the protection on the memory where the functions current asm code is
	// so that we will be able read/write it
	if( !VirtualProtect( pFuncToUnhook, hookData.m_nOriginalPreambleLength, PAGE_EXECUTE_READWRITE, &dwOldProtectionLevel ) )
	{
		if ( bLogFailures )
			Log( "Warning: VirtualProtect call failed during unhook\n" );
		return;
	}

	// In case the hook spans a page boundary, also adjust protections on the last byte,
	// and track the memory protection for the second page in a separate variable since
	// it could theoretically be different (although that would be very odd).
	if ( bHookSpansTwoPages && !VirtualProtect( pLastHookByte, 1, PAGE_EXECUTE_READWRITE, &dwOldProtectionLevel2 ) )
	{
		// Restore original protection on first page.
		VirtualProtect( pFuncToUnhook, 1, dwOldProtectionLevel, &dwIgnore );
		if ( bLogFailures )
			Log( "Warning: VirtualProtect (2) call failed during unhook\n" );
		return;
	}

	memcpy( pFuncToUnhook, hookData.m_rgOriginalPreambleCode, hookData.m_nOriginalPreambleLength );

	// Must flush instruction cache to ensure the processor detects the changed memory
	FlushInstructionCache( hProc, pFuncToUnhook, hookData.m_nOriginalPreambleLength );

	// Restore the original protection flags regardless of success, unless they already matched, then don't bother
	if ( bHookSpansTwoPages && dwOldProtectionLevel2 != PAGE_EXECUTE_READWRITE && dwOldProtectionLevel2 != PAGE_EXECUTE_WRITECOPY )
	{
		if ( !VirtualProtect( pLastHookByte, 1, dwOldProtectionLevel2, &dwIgnore ) )
		{
			if ( bLogFailures )
				Log( "Warning: VirtualProtect (2) call failed to restore protection flags during unhook\n" );
		}
	}
	if ( dwOldProtectionLevel != PAGE_EXECUTE_READWRITE && dwOldProtectionLevel != PAGE_EXECUTE_WRITECOPY )
	{
		if ( !VirtualProtect( pFuncToUnhook, 1, dwOldProtectionLevel, &dwIgnore ) )
		{
			if ( bLogFailures )
				Log( "Warning: VirtualProtect call failed to restore protection flags during unhook\n" );
		}
	}
}

void UnhookFuncByRelocAddr( BYTE *pRelocFunctionAddr, bool bLogFailures )
{
	if ( !pRelocFunctionAddr )
	{
		if ( bLogFailures )
			Log( "Aborting UnhookFunc because pRelocFunctionAddr is null\n" );
		return;
	}

	BYTE *pOrigFunc = NULL;
	{
		GetLock getLock( g_mapLock );
		for ( const auto &entry : g_mapHookedFunctions )
		{
			if ( entry.second.m_pTrampolineRealFunc == pRelocFunctionAddr )
			{
				pOrigFunc = (BYTE*)entry.first;
				break;
			}
		}
	}
	
	if ( !pOrigFunc )
	{
		if ( bLogFailures )
			Log( "Aborting UnhookFuncByRelocAddr because no matching function is hooked\n" );
		return;
	}

	UnhookFunc( pOrigFunc, bLogFailures );
}

#if DEBUG_ENABLE_DETOUR_RECORDING

//-----------------------------------------------------------------------------
// CRecordDetouredCalls
//-----------------------------------------------------------------------------
CRecordDetouredCalls::CRecordDetouredCalls()
{
	m_guidMarkerBegin = { 0xb6a8cedf, 0x3296, 0x43d2, { 0xae, 0xc1, 0xa5, 0x96, 0xea, 0xb7, 0x6c, 0xc2 } };
	m_nVersionNumber = 1;
	m_cubRecordDetouredCalls = sizeof(CRecordDetouredCalls);
	m_cubGetAsyncKeyStateCallRecord = sizeof( m_GetAsyncKeyStateCallRecord );
	m_cubVirtualAllocCallRecord		= sizeof( m_VirtualAllocCallRecord );
	m_cubVirtualProtectCallRecord	= sizeof( m_VirtualProtectCallRecord );
	m_cubLoadLibraryCallRecord		= sizeof( m_LoadLibraryCallRecord );
	m_bMasterSwitch = false;
	m_guidMarkerEnd = { 0xff84867e, 0x86e0, 0x4c0f, { 0x81, 0xf5, 0x8f, 0xe5, 0x48, 0x72, 0xa7, 0xe5 } };
}


//-----------------------------------------------------------------------------
// BShouldRecordProtectFlags
// only want to track PAGE_EXECUTE_READWRITE for now
//-----------------------------------------------------------------------------
bool CRecordDetouredCalls::BShouldRecordProtectFlags( DWORD flProtect )
{
	return flProtect == PAGE_EXECUTE_READWRITE;
}


//-----------------------------------------------------------------------------
// RecordGetAsyncKeyState
// only care about callers address, not params or results
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordGetAsyncKeyState( DWORD vKey, 
	PVOID lpCallersAddress, PVOID lpCallersCallerAddress
	)
{
	GetAsyncKeyStateCallRecord_t fcr;
	fcr.InitGetAsyncKeyState( vKey, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_GetAsyncKeyStateCallRecord.AddFunctionCallRecord( fcr );
#ifdef DEBUG_LOG_DETOURED_CALLS
	Log( "GetAsyncKeyState called %d from %p %p\n", iCall, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}


//-----------------------------------------------------------------------------
// RecordVirtualAlloc
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordVirtualAlloc( LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
		LPVOID lpvResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	VirtualAllocCallRecord_t fcr;
	fcr.InitVirtualAlloc( lpAddress, dwSize, flAllocationType, flProtect, lpvResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_VirtualAllocCallRecord.AddFunctionCallRecord( fcr );
#ifdef DEBUG_LOG_DETOURED_CALLS
	Log( "VirtualAlloc called %d : %p %llx %x %x result %p from %p %p\n", iCall, lpAddress, (uint64)dwSize, flAllocationType, flProtect, lpvResult, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}


//-----------------------------------------------------------------------------
// RecordVirtualProtect
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordVirtualProtect( LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
		BOOL bResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	VirtualAllocCallRecord_t fcr;
	fcr.InitVirtualProtect( lpAddress, dwSize, flNewProtect, flOldProtect, bResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_VirtualProtectCallRecord.AddFunctionCallRecord( fcr );
#ifdef DEBUG_LOG_DETOURED_CALLS
	Log( "VirtualProtect called %d : %p %llx %x %x result %x from %p %p\n", iCall, lpAddress, (uint64)dwSize, flNewProtect, flOldProtect, bResult, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}


//-----------------------------------------------------------------------------
// RecordVirtualAllocEx
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordVirtualAllocEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
		LPVOID lpvResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	VirtualAllocCallRecord_t fcr;
	fcr.InitVirtualAllocEx( hProcess, lpAddress, dwSize, flAllocationType, flProtect, lpvResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_VirtualAllocCallRecord.AddFunctionCallRecord( fcr );
#ifdef DEBUG_LOG_DETOURED_CALLS
	Log( "VirtualAllocEx called %d : %p %llx %x %x result %p from %p %p\n", iCall, lpAddress, (uint64)dwSize, flAllocationType, flProtect, lpvResult, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}


//-----------------------------------------------------------------------------
// RecordVirtualProtectEx
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordVirtualProtectEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
		BOOL bResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	VirtualAllocCallRecord_t fcr;
	fcr.InitVirtualProtectEx( hProcess, lpAddress, dwSize, flNewProtect, flOldProtect, bResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_VirtualProtectCallRecord.AddFunctionCallRecord( fcr );
#ifdef DEBUG_LOG_DETOURED_CALLS
	Log( "VirtualProtectEx called %d : %p %llx %x %x result %x from %p %p\n", iCall, lpAddress, (uint64)dwSize, flNewProtect, flOldProtect, bResult, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}


//-----------------------------------------------------------------------------
// RecordLoadLibraryW
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordLoadLibraryW( 
		LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
		HMODULE hModule, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	LoadLibraryCallRecord_t fcr;
	fcr.InitLoadLibraryW( lpLibFileName, hFile, dwFlags, hModule, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_LoadLibraryCallRecord.AddFunctionCallRecord( fcr );
	if ( iCall >= 0 )
	{
		// keep updating the last callers address, so we will have the first and last caller, but lose any in between
		m_LoadLibraryCallRecord.m_rgElements[iCall].m_lpLastCallerAddress = lpCallersAddress;
	}
#ifdef DEBUG_LOG_DETOURED_CALLS
	char rgchCopy[500];
	wcstombs( rgchCopy, lpLibFileName, 500 );
	Log( "LoadLibraryW called %d : %s result %p from %p %p\n", iCall, rgchCopy, hModule, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}


//-----------------------------------------------------------------------------
// RecordLoadLibraryA
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::RecordLoadLibraryA( 
		LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
		HMODULE hModule, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	LoadLibraryCallRecord_t fcr;
	fcr.InitLoadLibraryA( lpLibFileName, hFile, dwFlags, hModule, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	int iCall = m_LoadLibraryCallRecord.AddFunctionCallRecord( fcr );
	if ( iCall >= 0 )
	{
		// keep updating the last callers address, so we will have the first and last caller, but lose any in between
		m_LoadLibraryCallRecord.m_rgElements[iCall].m_lpLastCallerAddress = lpCallersAddress;
	}
#ifdef DEBUG_LOG_DETOURED_CALLS
	Log( "LoadLibraryA called %d : %s result %p from %p %p\n", iCall, lpLibFileName, hModule, lpCallersAddress, lpCallersCallerAddress );
#else
	iCall;
#endif
}

//-----------------------------------------------------------------------------
// SharedInit
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::FunctionCallRecordBase_t::SharedInit(
	DWORD dwResult, DWORD dwGetLastError, 
	PVOID lpCallersAddress, PVOID lpCallersCallerAddress
	)
{
	m_dwResult = dwResult;
	m_dwGetLastError = dwGetLastError;
	m_lpFirstCallersAddress = lpCallersAddress;
	m_lpLastCallerAddress = NULL;
	lpCallersCallerAddress;
}


//-----------------------------------------------------------------------------
// CRecordDetouredCalls private implementations
//-----------------------------------------------------------------------------
void CRecordDetouredCalls::GetAsyncKeyStateCallRecord_t::InitGetAsyncKeyState( DWORD vKey, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	vKey;
	SharedInit( 0, 0, lpCallersAddress, lpCallersCallerAddress );
}

void CRecordDetouredCalls::VirtualAllocCallRecord_t::InitVirtualAlloc( LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
		LPVOID lpvResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	SharedInit( (DWORD)lpvResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	m_dwProcessId = 0;
	m_lpAddress = lpAddress;
	m_dwSize = dwSize;
	m_flProtect = flProtect;
	m_dw2 = flAllocationType;
}

// VirtualAllocEx
void CRecordDetouredCalls::VirtualAllocCallRecord_t::InitVirtualAllocEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, 
		LPVOID lpvResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	SharedInit( (DWORD)lpvResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	m_dwProcessId = GetProcessId( hProcess );
	m_lpAddress = lpAddress;
	m_dwSize = dwSize;
	m_flProtect = flProtect;
	m_dw2 = flAllocationType;
}

// VirtualProtect
void CRecordDetouredCalls::VirtualAllocCallRecord_t::InitVirtualProtect( LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
		BOOL bResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	SharedInit( (DWORD)bResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );

	m_dwProcessId = 0;
	m_lpAddress = lpAddress;
	m_dwSize = dwSize;
	m_flProtect = flNewProtect;
	m_dw2 = flOldProtect;
}

// VirtualProtectEx
void CRecordDetouredCalls::VirtualAllocCallRecord_t::InitVirtualProtectEx( HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD flOldProtect, 
		BOOL bResult, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	SharedInit( (DWORD)bResult, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );

	m_dwProcessId = GetProcessId( hProcess );
	m_lpAddress = lpAddress;
	m_dwSize = dwSize;
	m_flProtect = flNewProtect;
	m_dw2 = flOldProtect;
}

// LoadLibraryExW
void CRecordDetouredCalls::LoadLibraryCallRecord_t::InitLoadLibraryW( 
		LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
		HMODULE hModule, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	SharedInit( (DWORD)hModule, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	m_hFile = hFile;
	m_dwFlags = dwFlags;

	memset( m_rgubFileName, 0, sizeof(m_rgubFileName) );
	if ( hModule != NULL && lpLibFileName != NULL )
	{
		// record as many of the tail bytes as will fit in m_rgubFileName
		size_t cubLibFileName = wcslen( lpLibFileName )* sizeof(WCHAR);
		size_t cubToCopy = cubLibFileName;
		size_t nOffset = 0;
		if ( cubToCopy > sizeof(m_rgubFileName) )
		{
			nOffset = cubToCopy - sizeof(m_rgubFileName);
			cubToCopy = sizeof(m_rgubFileName);
		}
		memcpy( m_rgubFileName, ((uint8 *)lpLibFileName) + nOffset, cubToCopy );
	}
}

// LoadLibraryExA
void CRecordDetouredCalls::LoadLibraryCallRecord_t::InitLoadLibraryA( 
		LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
		HMODULE hModule, DWORD dwGetLastError, 
		PVOID lpCallersAddress, PVOID lpCallersCallerAddress
		)
{
	SharedInit( (DWORD)hModule, dwGetLastError, lpCallersAddress, lpCallersCallerAddress );
	m_hFile = hFile;
	m_dwFlags = dwFlags;

	memset( m_rgubFileName, 0, sizeof(m_rgubFileName) );
	if ( hModule != NULL && lpLibFileName != NULL )
	{
		// record as many of the tail bytes as will fit in m_rgubFileName
		size_t cubLibFileName = strlen( lpLibFileName );
		size_t cubToCopy = cubLibFileName;
		size_t nOffset = 0;
		if ( cubToCopy > sizeof(m_rgubFileName) )
		{
			nOffset = cubToCopy - sizeof(m_rgubFileName);
			cubToCopy = sizeof(m_rgubFileName);
		}
		memcpy( m_rgubFileName, ((uint8 *)lpLibFileName) + nOffset, cubToCopy );
	}
}

#endif // DEBUG_ENABLE_DETOUR_RECORDING

#pragma warning( pop )

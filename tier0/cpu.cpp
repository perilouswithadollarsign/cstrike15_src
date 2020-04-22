//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "pch_tier0.h"

#if defined(_WIN32) && !defined(_X360)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include "cputopology.h"
#elif defined( PLATFORM_OSX )
#include <sys/sysctl.h>
#endif

#ifndef _PS3
#include "tier0_strtools.h"
#endif

//#include "tier1/strtools.h" // this is included for the definition of V_isspace()
#ifdef PLATFORM_WINDOWS_PC
#include <intrin.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

const tchar* GetProcessorVendorId();
const tchar* GetProcessorBrand();

struct CpuIdResult_t
{
	unsigned long eax;
	unsigned long ebx;
	unsigned long ecx;
	unsigned long edx;

	void Reset()
	{
		eax = ebx = ecx = edx = 0;
	}
};


static bool cpuid( unsigned long function, CpuIdResult_t &out )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#elif defined(GNUC)
	unsigned long out_eax,out_ebx,out_ecx,out_edx;
#ifdef PLATFORM_64BITS
	asm("mov %%rbx, %%rsi\n\t"
		"cpuid\n\t"
		"xchg %%rsi, %%rbx"
		: "=a" (out_eax),
		  "=S" (out_ebx),
		  "=c" (out_ecx),
		  "=d" (out_edx)
		: "a" (function) 
	);
#else
	asm("mov %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchg %%esi, %%ebx"
		: "=a" (out_eax),
		  "=S" (out_ebx),
		  "=c" (out_ecx),
		  "=d" (out_edx)
		: "a" (function) 
	);
#endif
	out.eax = out_eax;
	out.ebx = out_ebx;
	out.ecx = out_ecx;
	out.edx = out_edx;	

	return true;
#elif defined(_WIN64)
	int pCPUInfo[4];
	__cpuid( pCPUInfo, (int)function );
	out.eax = pCPUInfo[0];
	out.ebx = pCPUInfo[1];
	out.ecx = pCPUInfo[2];
	out.edx = pCPUInfo[3];
	return true;
#else
	bool retval = true;
	unsigned long out_eax = 0, out_ebx = 0, out_ecx = 0, out_edx = 0;
	_asm pushad;

	__try
	{
        _asm
		{
			xor edx, edx		// Clue the compiler that EDX & others is about to be used. 
			xor ecx, ecx
			xor ebx, ebx        // <Sergiy> Note: if I don't zero these out, cpuid sometimes won't work, I didn't find out why yet
            mov eax, function   // set up CPUID to return processor version and features
								//      0 = vendor string, 1 = version info, 2 = cache info
            cpuid				// code bytes = 0fh,  0a2h
            mov out_eax, eax	// features returned in eax
            mov out_ebx, ebx	// features returned in ebx
            mov out_ecx, ecx	// features returned in ecx
            mov out_edx, edx	// features returned in edx
		}
    } 
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		retval = false; 
	}

	out.eax = out_eax;
	out.ebx = out_ebx;
	out.ecx = out_ecx;
	out.edx = out_edx;

	_asm popad

	return retval;
#endif
}


static bool cpuidex( unsigned long function, unsigned long subfunction, CpuIdResult_t &out )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#elif defined(GNUC)
	unsigned long out_eax, out_ebx, out_ecx, out_edx;

	asm( "mov %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchg %%esi, %%ebx"
		: "=a" ( out_eax ),
		"=S" ( out_ebx ),
		"=c" ( out_ecx ),
		"=d" ( out_edx )
		: "a" ( function ),
		  "c" ( subfunction )
		);

	out.eax = out_eax;
	out.ebx = out_ebx;
	out.ecx = out_ecx;
	out.edx = out_edx;

	return true;
#elif defined(_WIN64)
	int pCPUInfo[ 4 ];
	__cpuidex( pCPUInfo, ( int )function, ( int )subfunction );
	out.eax = pCPUInfo[ 0 ];
	out.ebx = pCPUInfo[ 1 ];
	out.ecx = pCPUInfo[ 2 ];
	out.edx = pCPUInfo[ 3 ];
	return false;
#else
	bool retval = true;
	unsigned long out_eax = 0, out_ebx = 0, out_ecx = 0, out_edx = 0;
	_asm pushad;

	__try
	{
		_asm
		{
			xor edx, edx		// Clue the compiler that EDX & others is about to be used. 
			mov ecx, subfunction
			xor ebx, ebx        // <Sergiy> Note: if I don't zero these out, cpuid sometimes won't work, I didn't find out why yet
			mov eax, function   // set up CPUID to return processor version and features
			//      0 = vendor string, 1 = version info, 2 = cache info
			cpuid				// code bytes = 0fh,  0a2h
			mov out_eax, eax	// features returned in eax
			mov out_ebx, ebx	// features returned in ebx
			mov out_ecx, ecx	// features returned in ecx
			mov out_edx, edx	// features returned in edx
		}
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		retval = false;
	}

	out.eax = out_eax;
	out.ebx = out_ebx;
	out.ecx = out_ecx;
	out.edx = out_edx;

	_asm popad

	return retval;
#endif
}


static CpuIdResult_t cpuid( unsigned long function )
{
	CpuIdResult_t out;
	if ( !cpuid( function, out ) )
	{
		out.Reset();
	}
	return out;
}

static CpuIdResult_t cpuidex( unsigned long function, unsigned long subfunction )
{
	CpuIdResult_t out;
	if ( !cpuidex( function, subfunction, out ) )
	{
		out.Reset();
	}
	return out;
}

//-----------------------------------------------------------------------------
// Purpose: This is a bit of a hack because it appears 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static bool IsWin98OrOlder()
{
#if defined( _X360 ) || defined( _PS3 ) || defined( POSIX )
	return false;
#else
	bool retval = false;

	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	
	BOOL bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi);
	if( !bOsVersionInfoEx )
	{
		// If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
		
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if ( !GetVersionEx ( (OSVERSIONINFO *) &osvi) )
		{
			Error( _T("IsWin98OrOlder:  Unable to get OS version information") );
		}
	}

	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		// NT, XP, Win2K, etc. all OK for SSE
		break;
	case VER_PLATFORM_WIN32_WINDOWS:
		// Win95, 98, Me can't do SSE
		retval = true;
		break;
	case VER_PLATFORM_WIN32s:
		// Can't really run this way I don't think...
		retval = true;
		break;
	default:
		break;
	}

	return retval;
#endif
}


static bool CheckSSETechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return true;
#else
	if ( IsWin98OrOlder() )
	{
		return false;
	}

    return ( cpuid( 1 ).edx & 0x2000000L ) != 0;
#endif
}

static bool CheckSSE2Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
    return ( cpuid( 1 ).edx & 0x04000000 ) != 0;
#endif
}

bool CheckSSE3Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( cpuid( 1 ).ecx & 0x00000001 ) != 0;	// bit 1 of ECX
#endif
}

bool CheckSSSE3Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSSE 3 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( cpuid( 1 ).ecx & ( 1 << 9 ) ) != 0;	// bit 9 of ECX
#endif
}

bool CheckSSE41Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4.1 is implemented by both Intel and AMD
	// detection is done the same way for both vendors

	return ( cpuid( 1 ).ecx & ( 1 << 19 ) ) != 0;	// bit 19 of ECX
#endif
}

bool CheckSSE42Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE4.2 is an Intel-only feature

	const char *pchVendor = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( pchVendor, "GenuineIntel" ) )
		return false;

	return ( cpuid( 1 ).ecx & ( 1 << 20 ) ) != 0;	// bit 20 of ECX
#endif
}


bool CheckSSE4aTechnology( void )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4a is an AMD-only feature

	const char *pchVendor = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( pchVendor, "AuthenticAMD" ) )
		return false;

	return ( cpuid( 1 ).ecx & ( 1 << 6 ) ) != 0;	// bit 6 of ECX
#endif
}


static bool Check3DNowTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	if ( cpuid( 0x80000000 ).eax > 0x80000000L )
    {
		return ( cpuid( 0x80000001 ).eax & ( 1 << 31 ) ) != 0;
    }
    return false;
#endif
}

static bool CheckCMOVTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( cpuid( 1 ).edx & ( 1 << 15 ) ) != 0;
#endif
}

static bool CheckFCMOVTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( cpuid( 1 ).edx & ( 1 << 16 ) ) != 0;
#endif
}

static bool CheckRDTSCTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( cpuid( 1 ).edx & 0x10 ) != 0;
#endif
}


static tchar s_CpuVendorID[ 13 ] = "unknown";

bool s_bCpuVendorIdInitialized = false;

union CpuBrand_t
{
	CpuIdResult_t cpuid[ 3 ];
	char name[ 49 ];
};
CpuBrand_t s_CpuBrand;

bool s_bCpuBrandInitialized = false;

// Return the Processor's vendor identification string, or "Generic_x86" if it doesn't exist on this CPU
const tchar* GetProcessorVendorId()
{
#if defined( _X360 ) || defined( _PS3 )
	return "PPC";
#else
	if ( s_bCpuVendorIdInitialized )
	{
		return s_CpuVendorID;
	}

	s_bCpuVendorIdInitialized = true;

	CpuIdResult_t cpuid0 = cpuid( 0 );
	
	memset( s_CpuVendorID, 0, sizeof(s_CpuVendorID) );

	if ( !cpuid0.eax )
	{
		// weird...
		if ( IsPC() )
		{
			_tcscpy( s_CpuVendorID, _T( "Generic_x86" ) ); 
		}
		else if ( IsX360() )
		{
			_tcscpy( s_CpuVendorID, _T( "PowerPC" ) ); 
		}
	}
	else
	{
		memcpy( s_CpuVendorID + 0, &( cpuid0.ebx ), sizeof( cpuid0.ebx ) );
		memcpy( s_CpuVendorID + 4, &( cpuid0.edx ), sizeof( cpuid0.edx ) );
		memcpy( s_CpuVendorID + 8, &( cpuid0.ecx ), sizeof( cpuid0.ecx ) );
	}

	return s_CpuVendorID;
#endif
}

const tchar* GetProcessorBrand()
{
#if defined( _X360 )
	return "Xenon";
#elif defined( _PS3 )
	return "Cell Broadband Engine";
#else
	if ( s_bCpuBrandInitialized )
	{
		return s_CpuBrand.name;
	}
	s_bCpuBrandInitialized = true;

	memset( &s_CpuBrand, 0, sizeof( s_CpuBrand ) );

	const char *pchVendor = GetProcessorVendorId();
	if ( 0 == V_tier0_stricmp( pchVendor, "GenuineIntel" ) )
	{
		// Intel brand string
		if ( cpuid( 0x80000000 ).eax >= 0x80000004 )
		{
			s_CpuBrand.cpuid[ 0 ] = cpuid( 0x80000002 );
			s_CpuBrand.cpuid[ 1 ] = cpuid( 0x80000003 );
			s_CpuBrand.cpuid[ 2 ] = cpuid( 0x80000004 );
		}
	}
	return s_CpuBrand.name;

#endif
}

// Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.
// If it's supported, it does not mean that it's been enabled. So we test another flag to see if it's enabled
// See Intel Processor Identification and the CPUID instruction Application Note 485
// http://www.intel.com/Assets/PDF/appnote/241618.pdf
static bool HTSupported(void)
{
#if ( defined( _X360 ) || defined( _PS3 ) )
	// not entirtely sure about the semantic of HT support, it being an intel name
	// are we asking about HW threads or HT?
	return true;
#else
	enum {
		HT_BIT		 = 0x10000000,  // EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
		FAMILY_ID     = 0x0f00,      // EAX[11:8] - Bit 11 thru 8 contains family processor id
		EXT_FAMILY_ID = 0x0f00000,	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
		FAMILY_ID_386 = 0x0300,
		FAMILY_ID_486 = 0x0400,     // EAX[8:12]  -  486, 487 and overdrive
		FAMILY_ID_PENTIUM = 0x0500, //               Pentium, Pentium OverDrive  60 - 200
		FAMILY_ID_PENTIUM_PRO = 0x0600,//            P Pro, P II, P III, P M, Celeron M, Core Duo, Core Solo, Core2 Duo, Core2 Extreme, P D, Xeon model F,
		                               //            also 45-nm : Intel Atom, Core i7, Xeon MP ; see Intel Processor Identification and the CPUID instruction pg 20,21
		                               
		FAMILY_ID_EXTENDED = 0x0F00 //               P IV, Xeon, Celeron D, P D, 
	};

	// this works on both newer AMD and Intel CPUs
	CpuIdResult_t cpuid1 = cpuid( 1 );

	// <Sergiy> Previously, we detected P4 specifically; now, we detect GenuineIntel with HT enabled in general
	// if (((cpuid1.eax & FAMILY_ID) ==  FAMILY_ID_EXTENDED) || (cpuid1.eax & EXT_FAMILY_ID))
	
	//  Check to see if this is an Intel Processor with HT or CMT capability , and if HT/CMT is enabled
	// ddk: This codef is actually correct: see example code at software.intel.com/en-us/articles/multi-core-detect/
	return ( cpuid1.edx & HT_BIT ) != 0 && // Genuine Intel Processor with Hyper-Threading Technology implemented
		( ( cpuid1.ebx >> 16 ) & 0xFF ) > 1; // Hyper-Threading OR Core Multi-Processing has been enabled
#endif
}

// Returns the number of logical processors per physical processors.
static uint8 LogicalProcessorsPerPackage(void)
{
#if defined( _X360 )
	return 2;
#else
	// EBX[23:16] indicate number of logical processors per package
	const unsigned NUM_LOGICAL_BITS = 0x00FF0000;

	if ( !HTSupported() ) 
		return 1; 

	return ( uint8 )( ( cpuid( 1 ).ebx & NUM_LOGICAL_BITS ) >> 16 );
#endif
}

#if defined(POSIX)
// Move this declaration out of the CalculateClockSpeed() function because
// otherwise clang warns that it is non-obvious whether it is a variable
// or a function declaration: [-Wvexing-parse]
uint64 CalculateCPUFreq(); // from cpu_linux.cpp
#endif

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
#if defined( _X360 ) || defined(_PS3)
	// Xbox360 and PS3 have the same clock speed and share a lot of characteristics on PPU
	return 3200000000LL;
#else	
#if defined( _WIN32 )
	LARGE_INTEGER waitTime, startCount, curCount;
	CCycleCount start, end;

	// Take 1/32 of a second for the measurement.
	QueryPerformanceFrequency( &waitTime );
	int scale = 5;
	waitTime.QuadPart >>= scale;

	QueryPerformanceCounter( &startCount );
	start.Sample();
	do
	{
		QueryPerformanceCounter( &curCount );
	}
	while ( curCount.QuadPart - startCount.QuadPart < waitTime.QuadPart );
	end.Sample();

	return (end.m_Int64 - start.m_Int64) << scale;
#elif defined(POSIX)
	int64 freq =(int64)CalculateCPUFreq();
	if ( freq == 0 ) // couldn't calculate clock speed
	{
		Error( "Unable to determine CPU Frequency\n" );
	}
	return freq;
#else
	#error "Please implement Clock Speed function for this platform"
#endif
#endif
}

static CPUInformation s_cpuInformation;

struct IntelCacheDesc_t
{
	uint8 nDesc;
	uint16 nCacheSize;
};

static IntelCacheDesc_t s_IntelL1DataCacheDesc[] = {
	{ 0xA, 8 },
	{ 0xC, 16 },
	{ 0xD, 16 },
	{ 0x2C, 32 },
	{ 0x30, 32 },
	{ 0x60, 16 },
	{ 0x66, 8 },
	{ 0x67, 16 },
	{ 0x68, 32 }
};


static IntelCacheDesc_t s_IntelL2DataCacheDesc[] =
{
	{ 0x21, 256 },
	{ 0x39, 128 },
	{ 0x3a, 192 },
	{ 0x3b, 128 },
	{ 0x3c, 256 },
	{ 0x3D, 384 },
	{ 0x3E, 512 },
	{ 0x41, 128 },
	{ 0x42, 256 },
	{ 0x43, 512 },
	{ 0x44, 1024 },
	{ 0x45, 2048 },
	{ 0x48, 3 * 1024 },
	{ 0x4e, 6 * 1024 },
	{ 0x78, 1024 },
	{ 0x79, 128 },
	{ 0x7a, 256 },
	{ 0x7b, 512 },
	{ 0x7c, 1024 },
	{ 0x7d, 2048 },
	{ 0x7f, 512 },
	{ 0x82, 256 },
	{ 0x83, 512 },
	{ 0x84, 1024 },
	{ 0x85, 2048 },
	{ 0x86, 512 },
	{ 0x87, 1024 }
};


static IntelCacheDesc_t s_IntelL3DataCacheDesc[] = {
	{ 0x22, 512 },
	{ 0x23, 1024 },
	{ 0x25, 2 * 1024 },
	{ 0x29, 4 * 1024 },
	{ 0x46, 4 * 1024 },
	{ 0x47, 8 * 1024 },
	// { 49, 
	{ 0x4a, 6 * 1024 },
	{ 0x4b, 8 * 1024 },
	{ 0x4c, 12 * 1024 },
	{ 0x4d, 16 * 1014 },
	{ 0xD0, 512 },
	{ 0xD1, 1024 },
	{ 0xD2, 2048 },
	{ 0xD6, 1024 },
	{ 0xD7, 2048 },
	{ 0xD8, 4096 },
	{ 0xDC, 1536 },
	{ 0xDD, 3 * 1024 },
	{ 0xDE, 6 * 1024 },
	{ 0xE2, 2048 },
	{ 0xE3, 4096 },
	{ 0xE4, 8 * 1024 },
	{ 0xEA, 12 * 1024 },
	{ 0xEB, 18 * 1024 },
	{ 0xEC, 24 * 1024 }
};

static void FindIntelCacheDesc( uint8 nDesc, const IntelCacheDesc_t *pDesc, int nDescCount, uint32 &nCache, uint32 &nCacheDesc )
{
	for ( int i = 0; i < nDescCount; ++i )
	{
		if ( pDesc->nDesc == nDesc )
		{
			nCache = pDesc->nCacheSize;
			nCacheDesc = nDesc;
			break;
		}
	}
}

// see "Output of the CPUID instruction" from Intel, page 26
static void InterpretIntelCacheDescriptors( uint32 nPackedDesc )
{
	if ( nPackedDesc & 0x80000000 )
	{
		return; // this is a wrong descriptor
	}
	for ( int i = 0; i < 4; ++i )
	{
		FindIntelCacheDesc( nPackedDesc & 0xFF, s_IntelL1DataCacheDesc, ARRAYSIZE( s_IntelL1DataCacheDesc ), s_cpuInformation.m_nL1CacheSizeKb, s_cpuInformation.m_nL1CacheDesc );
		FindIntelCacheDesc( nPackedDesc & 0xFF, s_IntelL2DataCacheDesc, ARRAYSIZE( s_IntelL2DataCacheDesc ), s_cpuInformation.m_nL2CacheSizeKb, s_cpuInformation.m_nL2CacheDesc );
		FindIntelCacheDesc( nPackedDesc & 0xFF, s_IntelL3DataCacheDesc, ARRAYSIZE( s_IntelL3DataCacheDesc ), s_cpuInformation.m_nL3CacheSizeKb, s_cpuInformation.m_nL3CacheDesc );
		nPackedDesc >>= 8;
	}
}


const CPUInformation& GetCPUInformation()
{
	CPUInformation &pi = s_cpuInformation;
	// Has the structure already been initialized and filled out?
	if ( pi.m_Size == sizeof(pi) )
		return pi;

	// Redundant, but just in case the user somehow messes with the size.
	memset(&pi, 0x0, sizeof(pi));

	// Fill out the structure, and return it: 
	pi.m_Size = sizeof(pi);

	// Grab the processor frequency:
	pi.m_Speed = CalculateClockSpeed();
	
	// Get the logical and physical processor counts:
	pi.m_nLogicalProcessors = LogicalProcessorsPerPackage();

	bool bAuthenticAMD = ( 0 == V_tier0_stricmp( GetProcessorVendorId(), "AuthenticAMD" ) );
	bool bGenuineIntel = !bAuthenticAMD && ( 0 == V_tier0_stricmp( GetProcessorVendorId(), "GenuineIntel" ) );

#if defined( _X360 )
	pi.m_nPhysicalProcessors = 3;
	pi.m_nLogicalProcessors  = 6;
#elif defined( _PS3 )
	pi.m_nPhysicalProcessors = 1;
	pi.m_nLogicalProcessors  = 2;
#elif defined(_WIN32) && !defined( _X360 )
	SYSTEM_INFO si;
	ZeroMemory( &si, sizeof(si) );

	GetSystemInfo( &si );

	// Sergiy: fixing: si.dwNumberOfProcessors is the number of logical processors according to experiments on i7, P4 and a DirectX sample (Aug'09)
	//         this is contrary to MSDN documentation on GetSystemInfo()
	// 
	pi.m_nLogicalProcessors = si.dwNumberOfProcessors;

	if ( bAuthenticAMD )
	{
		// quick fix for AMD Phenom: it reports 3 logical cores and 4 physical cores;
		// no AMD CPUs by the end of 2009 have HT, so we'll override HT detection here
		pi.m_nPhysicalProcessors = pi.m_nLogicalProcessors;
	}
	else
	{
		CpuTopology topo;
		pi.m_nPhysicalProcessors = topo.NumberOfSystemCores();
	}

	// Make sure I always report at least one, when running WinXP with the /ONECPU switch, 
	// it likes to report 0 processors for some reason.
	if ( pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0 )
	{
		Assert( !"Sergiy: apparently I didn't fix some CPU detection code completely. Let me know and I'll do my best to fix it soon." );
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
	}
#elif defined(LINUX)
	pi.m_nLogicalProcessors = 0;
	pi.m_nPhysicalProcessors = 0;
	const int k_cMaxProcessors = 256;
	bool rgbProcessors[k_cMaxProcessors];
	memset( rgbProcessors, 0, sizeof( rgbProcessors ) );
	int cMaxCoreId = 0;

	FILE *fpCpuInfo = fopen( "/proc/cpuinfo", "r" );
	if ( fpCpuInfo )
	{
		char rgchLine[256];
		while ( fgets( rgchLine, sizeof( rgchLine ), fpCpuInfo ) )
		{
			if ( !strncasecmp( rgchLine, "processor", strlen( "processor" ) ) )
			{
				pi.m_nLogicalProcessors++;
			}
			if ( !strncasecmp( rgchLine, "core id", strlen( "core id" ) ) )
			{
				char *pchValue = strchr( rgchLine, ':' );
				cMaxCoreId = MAX( cMaxCoreId, atoi( pchValue + 1 ) );
			}
			if ( !strncasecmp( rgchLine, "physical id", strlen( "physical id" ) ) )
			{
				// it seems (based on survey data) that we can see
				// processor N (N > 0) when it's the only processor in
				// the system.  so keep track of each processor
				char *pchValue = strchr( rgchLine, ':' );
				int cPhysicalId = atoi( pchValue + 1 );
				if ( cPhysicalId < k_cMaxProcessors )
					rgbProcessors[cPhysicalId] = true;
			}
			/* this code will tell us how many physical chips are in the machine, but we want
			   core count, so for the moment, each processor counts as both logical and physical.
			if ( !strncasecmp( rgchLine, "physical id ", strlen( "physical id " ) ) )
			{
				char *pchValue = strchr( rgchLine, ':' );
				pi.m_nPhysicalProcessors = MAX( pi.m_nPhysicalProcessors, atol( pchValue ) );
			}
			*/
		}
		fclose( fpCpuInfo );
		for ( int i = 0; i < k_cMaxProcessors; i++ )
			if ( rgbProcessors[i] )
				pi.m_nPhysicalProcessors++;
		pi.m_nPhysicalProcessors *= ( cMaxCoreId + 1 );
	}
	else
	{
		pi.m_nLogicalProcessors = 1;
		pi.m_nPhysicalProcessors = 1;
		Assert( !"couldn't read cpu information from /proc/cpuinfo" );
	}

#elif defined(OSX)

	int num_phys_cpu = 1, num_log_cpu = 1;
	size_t len = sizeof(num_phys_cpu);
	sysctlbyname( "hw.physicalcpu", &num_phys_cpu, &len, NULL, 0 );
	sysctlbyname( "hw.logicalcpu", &num_log_cpu, &len, NULL, 0 );
	pi.m_nPhysicalProcessors = num_phys_cpu;
	pi.m_nLogicalProcessors  = num_log_cpu;

#endif

	CpuIdResult_t cpuid0 = cpuid( 0 );
	if ( cpuid0.eax >= 1 )
	{
		CpuIdResult_t cpuid1 = cpuid( 1 );
		uint bFPU = cpuid1.edx & 1; // this should always be on on anything we support
		// Determine Processor Features:
		pi.m_bRDTSC = ( cpuid1.edx >> 4 ) & 1;
		pi.m_bCMOV = ( cpuid1.edx >> 15 ) & 1;
		pi.m_bFCMOV = ( pi.m_bCMOV && bFPU ) ? 1 : 0;
		pi.m_bMMX = ( cpuid1.edx >> 23 ) & 1;
		pi.m_bSSE = ( cpuid1.edx >> 25 ) & 1;
		pi.m_bSSE2 = ( cpuid1.edx >> 26 ) & 1;
		pi.m_bSSE3 = cpuid1.ecx & 1;
		pi.m_bSSSE3 = ( cpuid1.ecx >> 9 ) & 1;;
		pi.m_bSSE4a = CheckSSE4aTechnology();
		pi.m_bSSE41 = ( cpuid1.ecx >> 19 ) & 1;
		pi.m_bSSE42 = ( cpuid1.ecx >> 20 ) & 1;
		pi.m_b3DNow = Check3DNowTechnology();
		pi.m_bAVX	= ( cpuid1.ecx >> 28 ) & 1;
		pi.m_szProcessorID = ( tchar* )GetProcessorVendorId();
		pi.m_szProcessorBrand = ( tchar* )GetProcessorBrand();
		pi.m_bHT = ( pi.m_nPhysicalProcessors < pi.m_nLogicalProcessors ); //HTSupported();

		pi.m_nModel			= cpuid1.eax; // full CPU model info
		pi.m_nFeatures[ 0 ] = cpuid1.edx; // x87+ features
		pi.m_nFeatures[ 1 ] = cpuid1.ecx; // sse3+ features
		pi.m_nFeatures[ 2 ] = cpuid1.ebx; // some additional features

		if ( bGenuineIntel )
		{
			if ( cpuid0.eax >= 4 )
			{
				// we have CPUID.4, use it to find all the cache parameters
				const uint nCachesToQuery = 4; // leve 0 is not used
				uint nCacheSizeKiB[ nCachesToQuery ];
				for ( uint i = 0; i < nCachesToQuery; ++i )
				{
					nCacheSizeKiB[ i ] = 0;
				}
				for ( unsigned long nSub = 0; nSub < 1024 ; ++nSub )
				{
					CpuIdResult_t cpuid4 = cpuidex( 4, nSub );
					uint nCacheType = cpuid4.eax & 0x1F;
					if ( nCacheType == 0 )
					{
						// no more caches
						break; 
					}
					if ( nCacheType & 1 )
					{
						// this cache includes data cache: it's either data or unified. Instuction cache type is 2
						uint nCacheLevel = ( cpuid4.eax >> 5 ) & 7;
						if ( nCacheLevel < nCachesToQuery )
						{
							uint nCacheWays = 1 + ( ( cpuid4.ebx >> 22 ) & 0x3F );
							uint nCachePartitions = 1 + ( ( cpuid4.ebx >> 12 ) & 0x3F );
							uint nCacheLineSize = 1 + ( cpuid4.ebx & 0xFF );
							uint nCacheSets = 1 + cpuid4.ecx;
							uint nCacheSizeBytes = nCacheWays * nCachePartitions * nCacheLineSize * nCacheSets;
							nCacheSizeKiB[ nCacheLevel ] = nCacheSizeBytes >> 10;
						}
					}
				}

				pi.m_nL1CacheSizeKb = nCacheSizeKiB[ 1 ];
				pi.m_nL2CacheSizeKb = nCacheSizeKiB[ 2 ];
				pi.m_nL3CacheSizeKb = nCacheSizeKiB[ 3 ];
			}
			else if ( cpuid0.eax >= 2 )
			{
				// get the cache
				CpuIdResult_t cpuid2 = cpuid( 2 );
				for ( int i = ( cpuid2.eax & 0xFF ); i-- > 0; )
				{
					InterpretIntelCacheDescriptors( cpuid2.eax & ~0xFF );
					InterpretIntelCacheDescriptors( cpuid2.ebx );
					InterpretIntelCacheDescriptors( cpuid2.ecx );
					InterpretIntelCacheDescriptors( cpuid2.edx );
					cpuid2 = cpuid( 2 ); // read the next
				}
			}
		}
	}

	CpuIdResult_t cpuid0ex = cpuid( 0x80000000 );
	if ( bAuthenticAMD )
	{
		if ( cpuid0ex.eax >= 0x80000005 )
		{
			CpuIdResult_t cpuid5ex = cpuid( 0x80000005 );
			pi.m_nL1CacheSizeKb = cpuid5ex.ecx >> 24;
			pi.m_nL1CacheDesc = cpuid5ex.ecx & 0xFFFFFF;
		}
		if ( cpuid0ex.eax >= 0x80000006 )
		{
			CpuIdResult_t cpuid6ex = cpuid( 0x80000006 );
			pi.m_nL2CacheSizeKb = cpuid6ex.ecx >> 16;
			pi.m_nL2CacheDesc = cpuid6ex.ecx & 0xFFFF;
			pi.m_nL3CacheSizeKb = ( cpuid6ex.edx >> 18 ) * 512;
			pi.m_nL3CacheDesc = cpuid6ex.edx & 0xFFFF;
		}
	}
	else if ( bGenuineIntel )
	{
		if ( cpuid0ex.eax >= 0x80000006 )
		{
			// make sure we got the L2 cache info right
			pi.m_nL2CacheSizeKb = ( cpuid( 0x80000006 ).ecx >> 16 );
		}
	}
	return pi;
}


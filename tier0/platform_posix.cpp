//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/platform.h"
#include "tier0/memalloc.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#ifdef OSX
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <CoreServices/CoreServices.h>
#endif


static bool g_bBenchmarkMode = false;
static double g_FakeBenchmarkTime = 0;
static double g_FakeBenchmarkTimeInc = 1.0 / 66.0;


bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}



#ifdef OSX

static uint64 start_time = 0;
static mach_timebase_info_data_t    sTimebaseInfo;
static double conversion = 0.0;
static double conversionMS = 0.0;
static double conversionUS = 0.0;

void InitTime()
{
	start_time = mach_absolute_time();
	mach_timebase_info(&sTimebaseInfo);
	conversion = 1e-9 * (double) sTimebaseInfo.numer / (double) sTimebaseInfo.denom;
	conversionMS = 1e-6 * (double) sTimebaseInfo.numer / (double) sTimebaseInfo.denom;
	conversionUS = 1e-3 * (double) sTimebaseInfo.numer / (double) sTimebaseInfo.denom;
}

uint64 Plat_GetClockStart()
{
	if ( !start_time )
	{
		InitTime();
	}
	
	return start_time;
}

double Plat_FloatTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
	if ( !start_time )
	{
		InitTime();
	}
	
	uint64 now = mach_absolute_time();
	
	return ( now - start_time ) * conversion;
}
uint32 Plat_MSTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
	if ( !start_time )
	{
		InitTime();
	}
	
	uint64 now = mach_absolute_time();
	
	return uint32( ( now - start_time ) * conversionMS );
}

uint64 Plat_USTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
	if ( !start_time )
	{
		InitTime();
	}
	
	uint64 now = mach_absolute_time();
	
	return uint64( ( now - start_time ) * conversionUS );
}

#else

static int      secbase = 0;

void InitTime( struct timeval &tp )
{
	secbase = tp.tv_sec;
}

uint64 Plat_GetClockStart()
{
	if ( !secbase )
	{
		struct timeval  tp;
		
		gettimeofday( &tp, NULL );
		InitTime( tp );
	}
	
	return secbase;
}


double Plat_FloatTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
	struct timeval  tp;
	
	gettimeofday( &tp, NULL );
	
	if ( !secbase )
	{
		InitTime( tp );
	}
	
	return (( tp.tv_sec - secbase ) + tp.tv_usec / 1000000.0 );
}

uint32 Plat_MSTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return uint32( g_FakeBenchmarkTime * 1000.0 );
	}
	
	struct timeval  tp;
	
	gettimeofday( &tp, NULL );
	
	if ( !secbase )
	{
		InitTime( tp );
	}
	
	return (( tp.tv_sec - secbase )*1000 + tp.tv_usec / 1000 );
}

uint64 Plat_USTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return uint32( g_FakeBenchmarkTime * 1e6 );
	}
	
	struct timeval  tp;
	
	gettimeofday( &tp, NULL );
	
	if ( !secbase )
	{
		InitTime( tp );
	}
	
	return ( uint64( tp.tv_sec - secbase )*1000000ull + tp.tv_usec );
}

#endif


// Wraps the thread-safe versions of asctime. buf must be at least 26 bytes 
char *Plat_asctime( const struct tm *tm, char *buf, size_t bufsize )
{
	return asctime_r( tm, buf );
}

// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
	return ctime_r( timep, buf );
}

// Wraps the thread-safe versions of gmtime
struct tm *Platform_gmtime( const time_t *timep, struct tm *result )
{
	return gmtime_r( timep, result );
}

time_t Plat_timegm( struct tm *timeptr )
{
	return timegm( timeptr );
}

// Wraps the thread-safe versions of localtime
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
	return localtime_r( timep, result );
}

uint64 Plat_GetTime()
{
	// We just provide a wrapper on this function so we can protect access to time() everywhere.
	time_t ltime;
	time( &ltime );
	return ltime;
}

bool vtune( bool resume )
{
	return 0;
}


// -------------------------------------------------------------------------------------------------- //
// Memory stuff.
// -------------------------------------------------------------------------------------------------- //

PLATFORM_INTERFACE void Plat_DefaultAllocErrorFn( unsigned long size )
{
}

typedef void (*Plat_AllocErrorFn)( unsigned long size );
Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;

PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size )
{
	void *pRet = g_pMemAlloc->Alloc( size );
	if ( pRet )
	{
		return pRet;
	}
	else
	{
		g_AllocError( size );
		return 0;
	}
}


PLATFORM_INTERFACE void* Plat_Realloc( void *ptr, unsigned long size )
{
	void *pRet = g_pMemAlloc->Realloc( ptr, size );
	if ( pRet )
	{
		return pRet;
	}
	else
	{
		g_AllocError( size );
		return 0;
	}
}


PLATFORM_INTERFACE void Plat_Free( void *ptr )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_pMemAlloc->Free( ptr );
#else
	free( ptr );
#endif   
}


PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}

static char g_CmdLine[ 2048 ];
PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine )
{
	strncpy( g_CmdLine, cmdLine, sizeof(g_CmdLine) );
	g_CmdLine[ sizeof(g_CmdLine) -1 ] = 0;
}

PLATFORM_INTERFACE void Plat_SetCommandLineArgs( char **argv, int argc )
{
	g_CmdLine[0] = 0;
	for ( int i = 0; i < argc; i++ )
	{
		strncat( g_CmdLine, argv[i], sizeof(g_CmdLine) - strlen(g_CmdLine) );
	}
	
	g_CmdLine[ sizeof(g_CmdLine) -1 ] = 0;
}


PLATFORM_INTERFACE const tchar *Plat_GetCommandLine()
{
	return g_CmdLine;
}

PLATFORM_INTERFACE bool Is64BitOS()
{
#if defined OSX
	return true;
#elif defined LINUX
	FILE *pp = popen( "uname -m", "r" );
	if ( pp != NULL )
	{
		char rgchArchString[256];
		fgets( rgchArchString, sizeof( rgchArchString ), pp );
		pclose( pp );
		if ( !strncasecmp( rgchArchString, "x86_64", strlen( "x86_64" ) ) )
			return true;
	}
#else
	Assert( !"implement Is64BitOS" );
#endif
	return false;
}


bool Plat_IsInDebugSession()
{
#if defined(OSX)
	int mib[4];
	struct kinfo_proc info;
	size_t size;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = getpid();
	size = sizeof(info);
	info.kp_proc.p_flag = 0;
	sysctl(mib,4,&info,&size,NULL,0);
	bool result = ((info.kp_proc.p_flag & P_TRACED) == P_TRACED);
	return result;
#elif defined(LINUX)
	char s[256];
	snprintf(s, 256, "/proc/%d/cmdline", getppid());
	FILE * fp = fopen(s, "r");
	if (fp != NULL) 
	{
		fread(s, 256, 1, fp);
		fclose(fp);
		return (0 == strncmp(s, "gdb", 3));
	}
	return false;
#endif
}



void Plat_ExitProcess( int nCode )
{
	fflush( stdout );
	if ( nCode != 0 )
	{
		// Right now we want a non-zero exit code to cause a hard crash so
		// that we trigger minidump.
		int* x = NULL;
		*x = 1;
	}
	_exit( nCode );
}

static int s_nWatchDogTimerTimeScale = 0;
static bool s_bInittedWD = false;

static void WatchdogCoreDumpSigHandler( int nSignal )
{
	signal( SIGALRM, SIG_IGN );
	printf( "**** WARNING: Watchdog timer exceeded, aborting!\n" );
	abort();
}

static void InitWatchDogTimer( void )
{
	if( !strstr( g_CmdLine, "-nowatchdog" ) )
	{
#ifdef _DEBUG
		s_nWatchDogTimerTimeScale = 10;						// debug is slow
#else
		s_nWatchDogTimerTimeScale = 1;
#endif
		
		if( !strstr( g_CmdLine, "-nonabortingwatchdog" ) )
		{
			signal( SIGALRM, WatchdogCoreDumpSigHandler );
		}
	}
	
}

// watchdog timer support
void BeginWatchdogTimer( int nSecs )
{
	if (! s_bInittedWD )
	{
		s_bInittedWD = true;
		InitWatchDogTimer();
	}
	nSecs *= s_nWatchDogTimerTimeScale;
	nSecs = MIN( nSecs, 5 * 60 );							// no more than 5 minutes no matter what
	if ( nSecs )
		alarm( nSecs );
}

void EndWatchdogTimer( void )
{
	alarm( 0 );
}

static CThreadMutex g_LocalTimeMutex;


void Plat_GetLocalTime( struct tm *pNow )
{
	// We just provide a wrapper on this function so we can protect access to time() everywhere.
	time_t ltime;
	time( &ltime );
	
	Plat_ConvertToLocalTime( ltime, pNow );
}

void Plat_ConvertToLocalTime( uint64 nTime, struct tm *pNow )
{
	// Since localtime() returns a global, we need to protect against multiple threads stomping it.
	g_LocalTimeMutex.Lock();
	
	time_t ltime = (time_t)nTime;
	tm *pTime = localtime( &ltime );
	if ( pTime )
		*pNow = *pTime;
	else
		memset( pNow, 0, sizeof( *pNow ) );
	
	g_LocalTimeMutex.Unlock();
}

void Plat_GetTimeString( struct tm *pTime, char *pOut, int nMaxBytes )
{
	g_LocalTimeMutex.Lock();
	
	char *pStr = asctime( pTime );
	strncpy( pOut, pStr, nMaxBytes );
	pOut[nMaxBytes-1] = 0;
	
	g_LocalTimeMutex.Unlock();
}

// timezone
int32 Plat_timezone( void )
{
	return timezone;
}

// daylight savings
int32 Plat_daylight( void )
{
	return daylight;
}

void Platform_gmtime( uint64 nTime, struct tm *pTime )
{
	time_t tmtTime = nTime;
	struct tm * tmp = gmtime( &tmtTime );
	* pTime = * tmp;
}

#ifdef LINUX
/*
From http://man7.org/linux/man-pages/man5/proc.5.html:
       /proc/[pid]/statm
              Provides information about memory usage, measured in pages.
              The columns are:

                  size       (1) total program size
                             (same as VmSize in /proc/[pid]/status)
                  resident   (2) resident set size
                             (same as VmRSS in /proc/[pid]/status)
                  share      (3) shared pages (i.e., backed by a file)
                  text       (4) text (code)
                  lib        (5) library (unused in Linux 2.6)
                  data       (6) data + stack
                  dt         (7) dirty pages (unused in Linux 2.6)
*/

// This returns the resident memory size (RES column in 'top') in bytes.
size_t ApproximateProcessMemoryUsage( void )
{
	size_t nRet = 0;
	FILE *pFile = fopen( "/proc/self/statm", "r" );
	if ( pFile )
	{
		size_t nSize, nResident, nShare, nText, nLib_Unused, nDataPlusStack, nDt_Unused;
		if ( fscanf( pFile, "%zu %zu %zu %zu %zu %zu %zu", &nSize, &nResident, &nShare, &nText, &nLib_Unused, &nDataPlusStack, &nDt_Unused ) >= 2 )
		{
			nRet = 4096 * nResident;
		}
		fclose( pFile );
	}
	return nRet;
}
#else

size_t ApproximateProcessMemoryUsage( void )
{
	return 0;
}

#endif

char const * Plat_GetEnv(char const *pEnvVarName)
{
	return getenv(pEnvVarName);
}

PLATFORM_INTERFACE bool Plat_GetExecutablePath( char *pBuff, size_t nBuff )
{
#if defined OSX
	uint32_t _nBuff = nBuff; 
	bool bSuccess = _NSGetExecutablePath(pBuff, &_nBuff) == 0;
	pBuff[nBuff-1] = '\0';
	return bSuccess;
#elif defined LINUX
	ssize_t nRead = readlink("/proc/self/exe", pBuff, nBuff-1 );
	if ( nRead != -1 )
	{
		pBuff[ nRead ] = 0;
		return true;
	 }

	pBuff[0] = 0;
	return false;
#else
	AssertMsg( false, "Implement Plat_GetExecutablePath" );
	return false;
#endif
}

//========= Copyright (c) Valve Corporation, All rights reserved. ============
//
// this is even smaller profiler than miniprofiler: there is no linking or bookkeeping
// of any kind, it's just to throw in quick profilers that don't mess with code speed
// even in CERT builds
//
#ifndef TIER0MICROPROFILERHDR
#define TIER0MICROPROFILERHDR

#include "platform.h"


// define this to 1 to enable microprofiler; 2 to enable miniprofiler
#ifndef ENABLE_MICRO_PROFILER
#define ENABLE_MICRO_PROFILER 1
#endif


class CMicroProfiler;

PLATFORM_INTERFACE void MicroProfilerAddTS( CMicroProfiler *pProfiler, uint64 numTimeBaseTicks );
PLATFORM_INTERFACE int64 GetHardwareClockReliably();


#ifdef IS_WINDOWS_PC
#include <intrin.h>	// get __rdtsc
#endif


#if defined(_LINUX) || defined( OSX )
inline unsigned long long GetTimebaseRegister( void )
{
#ifdef PLATFORM_64BITS
    unsigned long long Low, High;
    __asm__ __volatile__ ( "rdtsc" : "=a" (Low), "=d" (High) );
    return ( High << 32 ) | ( Low & 0xffffffff );
#else
    unsigned long long Val;
    __asm__ __volatile__ ( "rdtsc" : "=A" (Val) );
    return Val;
#endif
}

#else
// Warning: THere's a hardware bug with 64-bit MFTB on PS3 (not sure about X360): sometimes it returns incorrect results (when low word overflows, the high word doesn't increment for some time)
inline int64 GetTimebaseRegister()
{
#if defined( _X360 )
	return __mftb32(); // X360: ~64 CPU ticks resolution
#elif defined( _PS3 )
	// The timebase frequency on PS/3 is 79.8 MHz, see sys_time_get_timebase_frequency()
	// this works out to 40.10025 clock ticks per timebase tick
	return __mftb();
#elif defined( OSX )
	return GetTimebaseRegister();
#else
	return __rdtsc();
#endif
}
#endif



#if ENABLE_MICRO_PROFILER > 0

class CMicroProfilerSample
{
	int64 m_nTimeBaseBegin;       // time base is kept here instead of using -= and += for better reliability and to avoid a cache miss at the beginning of the profiled section

public:
	CMicroProfilerSample()
	{
		m_nTimeBaseBegin = GetTimebaseRegister();
	}

	int64 GetElapsed()const
	{
		return GetTimebaseRegister() - m_nTimeBaseBegin;
	}
};


#ifdef IS_WINDOWS_PC
class CMicroProfilerQpcSample
{
	int64 m_nTimeBaseBegin;       // time base is kept here instead of using -= and += for better reliability and to avoid a cache miss at the beginning of the profiled section

public:
	CMicroProfilerQpcSample()
	{
		m_nTimeBaseBegin = GetHardwareClockReliably();
	}

	int64 GetElapsed()const
	{
		return GetHardwareClockReliably() - m_nTimeBaseBegin;
	}
};
#else
typedef CMicroProfilerSample CMicroProfilerQpcSample;
#endif


class CMicroProfiler
{
public:
	uint64 m_numTimeBaseTicks;	 // this will be totally screwed between Begin() and End()
	uint64 m_numCalls;
public:
	CMicroProfiler()
	{
		Reset();
	}

	CMicroProfiler( const CMicroProfiler &other )
	{
		m_numTimeBaseTicks = other.m_numTimeBaseTicks;
		m_numCalls = other.m_numCalls;
	}

	CMicroProfiler &operator=( const CMicroProfiler &other )
	{
		m_numTimeBaseTicks = other.m_numTimeBaseTicks;
		m_numCalls = other.m_numCalls;
		return *this;
	}

	void Begin()
	{
		m_numTimeBaseTicks -= GetTimebaseRegister();
	}

	void End()
	{
		m_numTimeBaseTicks += GetTimebaseRegister();
		m_numCalls ++;
	}
	void End( uint64 nCalls )
	{
		m_numTimeBaseTicks += GetTimebaseRegister();
		m_numCalls += nCalls;
	}


	void Add( uint64 numTimeBaseTicks, uint64 numCalls = 1)
	{
		m_numTimeBaseTicks += numTimeBaseTicks;
		m_numCalls += numCalls;
	}

	void AddTS( uint64 numTimeBaseTicks )
	{
		MicroProfilerAddTS( this, numTimeBaseTicks );
	}

	void Add(const CMicroProfilerSample &sample)
	{
		Add( sample.GetElapsed() );
	}

	void Add( const CMicroProfiler& profiler)
	{
		Add( profiler.m_numTimeBaseTicks, profiler.m_numCalls );
	}

	void Reset()
	{
		m_numTimeBaseTicks = 0;
		m_numCalls = 0;
	}

	void Damp( int shift = 1 )
	{
		m_numTimeBaseTicks >>= shift;
		m_numCalls >>= shift;
	}

	uint64 GetNumCalls() const
	{
		return m_numCalls;
	}

	uint64 GetNumTimeBaseTicks() const
	{
		return m_numTimeBaseTicks;
	}

	float GetAverageMilliseconds() const
	{
		return m_numCalls ? GetTotalMilliseconds() / m_numCalls : 0;
	}

	float GetTotalMilliseconds() const
	{
		return TimeBaseTicksToMilliseconds( m_numTimeBaseTicks );
	}

	static float TimeBaseTicksToMilliseconds( uint64 numTimeBaseTicks )
	{
#if defined( _X360 ) || defined( _PS3 )
		return numTimeBaseTicks / 79800.0 ;
#else
		return numTimeBaseTicks / ( GetCPUInformation().m_Speed * 0.001f );
#endif
	}
	float GetAverageTicks() const
	{
#if defined( _X360 ) || defined( _PS3 )
		return m_numTimeBaseTicks * 40.1f / m_numCalls; // timebase register is 79.8 MHz on these platforms
#else
		return float( m_numTimeBaseTicks / m_numCalls );
#endif
	}
	friend const CMicroProfiler operator + ( const CMicroProfiler &left, const CMicroProfiler &right );
	
	void Accumulate( const CMicroProfiler &other )
	{
		m_numCalls += other.m_numCalls;
		m_numTimeBaseTicks += other.m_numTimeBaseTicks;
	}
};


inline const CMicroProfiler operator + ( const CMicroProfiler &left, const CMicroProfiler &right )
{
	CMicroProfiler result;
	result.m_numCalls = left.m_numCalls + right.m_numCalls;
	result.m_numTimeBaseTicks = left.m_numTimeBaseTicks + right.m_numTimeBaseTicks;
	return result;
}


class CMicroProfilerGuard: public CMicroProfilerSample
{
	CMicroProfiler *m_pProfiler;
public:
	CMicroProfilerGuard( CMicroProfiler *pProfiler )
	{
		m_pProfiler = pProfiler;
	}
	~CMicroProfilerGuard()
	{
		m_pProfiler->Add( GetElapsed() );
	}
};


class CMicroProfilerGuardWithCount: public CMicroProfilerSample
{
	CMicroProfiler *m_pProfiler;
	uint m_nCount;
public:
	CMicroProfilerGuardWithCount( CMicroProfiler *pProfiler, uint nCount )
	{
		m_nCount = nCount;
		m_pProfiler = pProfiler;
	}
	void OverrideCount( uint nCount ) { m_nCount = nCount; }
	~CMicroProfilerGuardWithCount( )
	{
		m_pProfiler->Add( GetElapsed( ), m_nCount );
	}
};


// thread-safe variant of the same profiler
class CMicroProfilerGuardTS: public CMicroProfilerSample
{
	CMicroProfiler *m_pProfiler;
public:
	CMicroProfilerGuardTS( CMicroProfiler *pProfiler )
	{
		m_pProfiler = pProfiler;
	}
	~CMicroProfilerGuardTS()
	{
		m_pProfiler->AddTS( GetElapsed() );
	}
};


#define MICRO_PROFILER_NAME_0(LINE) localAutoMpg##LINE
#define MICRO_PROFILER_NAME(LINE) MICRO_PROFILER_NAME_0(LINE)
#define MICRO_PROFILE( MP ) CMicroProfilerGuard MICRO_PROFILER_NAME(__LINE__)( &( MP ) )
#define MICRO_PROFILE_TS( MP ) CMicroProfilerGuardTS MICRO_PROFILER_NAME(__LINE__)( &( MP ) )
 


#else


class CMicroProfilerSample
{public:
	CMicroProfilerSample(){}
	int GetElapsed()const{ return 0;}
};

typedef CMicroProfilerSample CMicroProfilerQpcSample;



class CMicroProfiler
{ public:
	CMicroProfiler(){}
	CMicroProfiler &operator=( const CMicroProfiler &other ) { return *this; }
	void Begin(){}
	void End(){}
	void End( uint32 ){}
	void Add( uint64 numTimeBaseTicks, int numCalls = 1){}
	void AddTS( uint64 numTimeBaseTicks ) {}
	void Add( const CMicroProfilerSample &sample){}
	void Add( const CMicroProfiler& profiler) {}
	void Reset(){}
	void Damp(int shift = 1){}
	uint64 GetNumCalls() const { return 0; }
	uint64 GetNumTimeBaseTicks() const { return 0; }
	int64 GetNumTimeBaseTicksExclusive() const { return 0; }
	float GetAverageMilliseconds()const { return 0; }
	float GetTotalMilliseconds()const { return 0; }
	static float TimeBaseTicksToMilliseconds( uint64 numTimeBaseTicks ) { return 0; }
	float GetAverageTicks() const { return 0; }
	friend const CMicroProfiler operator + ( const CMicroProfiler &left, const CMicroProfiler &right );
};

inline const CMicroProfiler operator + ( const CMicroProfiler &left, const CMicroProfiler &right ) { return left; }

class CMicroProfilerGuard: public CMicroProfilerSample
{
public:
	CMicroProfilerGuard( CMicroProfiler *pProfiler ){}
	~CMicroProfilerGuard(){}
};

class CMicroProfilerGuardTS: public CMicroProfilerSample
{
public:
	CMicroProfilerGuardTS( CMicroProfiler *pProfiler ){}
	~CMicroProfilerGuardTS(){}
};

#define MICRO_PROFILE( MP ) 
#endif


#endif

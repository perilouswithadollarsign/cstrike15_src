//========= Copyright c Valve Corporation, All rights reserved. ============//
#ifndef TIER0_HARDWARE_TIMER
#define TIER0_HARDWARE_TIMER

#include "tier0/platform.h"

#ifdef GNUC
inline int GetHardwareClockFast( void )
{
	unsigned long long int nRet;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (nRet)); // rdtsc
	return ( int ) nRet;
}

#else

#ifdef _X360
inline /*__declspec(naked)*/ int GetHardwareClockFast()
{
	/*__asm
	{
		lis		r3,08FFFh
		ld		r3,011E0h(r3)
		rldicl	r3,r3,32,32
		blr
	}  */
	return __mftb32() << 6;
}
#elif defined( _PS3 )
inline int GetHardwareClockFast()
{
	// The timebase frequency on PS/3 is 79.8 MHz, see sys_time_get_timebase_frequency()
	// this works out to 40.10025 clock ticks per timebase tick
	return __mftb() * 40;
}
#else

#include <intrin.h>


inline int GetHardwareClockFast()
{
	return __rdtsc();
}
#endif

#endif

#endif
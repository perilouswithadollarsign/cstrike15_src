
#include "tier0/platform.h"

#ifndef _PS3
#error "Error: _PS3 not defined in PS3-specific file"
#endif // _PS3


#ifdef _DEBUG
#define tier0_ps3 tier0_dbg
#else
#define tier0_ps3 tier0_rel
#endif


#ifdef _DEBUG
#include "Debug_PS3/prxexport.inl"
#else
#include "Release_PS3/prxexport.inl"
#endif

extern void _tier0_ps3_prx_required_for_linking();
void _tier0_ps3_prx_required_for_linking_prx()
{
	_tier0_ps3_prx_required_for_linking();
}


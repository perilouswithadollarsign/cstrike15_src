
#include "tier0/platform.h"

#ifndef _PS3
#error "Error: _PS3 not defined in PS3-specific file"
#endif // _PS3


#ifdef _DEBUG
#define vstdlib_ps3 vstdlib_dbg
#else
#define vstdlib_ps3 vstdlib_rel
#endif


#ifdef _DEBUG
#include "Debug_PS3/prxexport.inl"
#else
#include "Release_PS3/prxexport.inl"
#endif


extern void _vstdlib_ps3_prx_required_for_linking();
void _vstdlib_ps3_prx_required_for_linking_prx()
{
	_vstdlib_ps3_prx_required_for_linking();
}


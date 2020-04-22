#include <cellstatus.h>
#include <sys/prx.h>
#include <sys/exportcplusplus.h>
#include <stdio.h>

#include "tier0/threadtools.h"
#include "vstdlib/random.h"
#include "ps3/ps3_helpers.h"

#ifdef _DEBUG
#define vstdlib_ps3 vstdlib_dbg
#else
#define vstdlib_ps3 vstdlib_rel
#endif


// macro to redirect name substitution
PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER( vstdlib );
SYS_MODULE_START( _vstdlib_ps3_prx_entry );

SYS_LIB_DECLARE( vstdlib_ps3, SYS_LIB_AUTO_EXPORT | SYS_LIB_WEAK_IMPORT );

SYS_LIB_EXPORT( RandomSeed, vstdlib_ps3 );
SYS_LIB_EXPORT( RandomFloat, vstdlib_ps3 );
SYS_LIB_EXPORT( RandomFloatExp, vstdlib_ps3 );
SYS_LIB_EXPORT( RandomInt, vstdlib_ps3 );
SYS_LIB_EXPORT( RandomGaussianFloat, vstdlib_ps3 );

SYS_LIB_EXPORT( InstallUniformRandomStream, vstdlib_ps3 );

SYS_LIB_EXPORT( KeyValuesSystem, vstdlib_ps3 );
SYS_LIB_EXPORT( VStdLib_GetICVarFactory, vstdlib_ps3 );

SYS_LIB_EXPORT( RunThreadPoolTests, vstdlib_ps3 );
SYS_LIB_EXPORT( CreateNewThreadPool, vstdlib_ps3 );
SYS_LIB_EXPORT( DestroyThreadPool, vstdlib_ps3 );

SYS_LIB_EXPORT_VAR( g_pThreadPool, vstdlib_ps3 );

SYS_LIB_EXPORT( V_UTF8ToUnicode, vstdlib_ps3 );
SYS_LIB_EXPORT( V_UnicodeToUTF8, vstdlib_ps3 );
SYS_LIB_EXPORT( V_UCS2ToUnicode, vstdlib_ps3 );
SYS_LIB_EXPORT( V_UCS2ToUTF8, vstdlib_ps3 );
SYS_LIB_EXPORT( V_UnicodeToUCS2, vstdlib_ps3 );
SYS_LIB_EXPORT( V_UTF8ToUCS2, vstdlib_ps3 );
SYS_LIB_EXPORT( V_UTF8_strncpy, vstdlib_ps3 );
SYS_LIB_EXPORT( Coroutine_Create, vstdlib_ps3 );
SYS_LIB_EXPORT( Coroutine_Continue, vstdlib_ps3 );
SYS_LIB_EXPORT( Coroutine_YieldToMain, vstdlib_ps3 );
SYS_LIB_EXPORT( Coroutine_DebugAssert, vstdlib_ps3 );
SYS_LIB_EXPORT( Coroutine_DebugBreak, vstdlib_ps3 );


// BEGIN PICKUP
// You must run build_prxexport_snc.bat AFTER building vstdlib to regenerate prxexport.c, and THEN rebuild vstdlib again for the changes to take effect.
// This is required to get C++ class exports working
SYS_LIB_EXPORTPICKUP_CLASS( "CUniformRandomStream@", vstdlib_ps3 );
SYS_LIB_EXPORTPICKUP_CLASS( "CGaussianRandomStream@", vstdlib_ps3 );
// END PICKUP


extern "C" int _vstdlib_ps3_prx_entry( unsigned int args, void *pArg )
{
    return SYS_PRX_RESIDENT;
}

void _vstdlib_ps3_prx_required_for_linking()
{
}



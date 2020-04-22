//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "mathlib/mathlib.h"
#include "convar.h" // ConVar define
#include "view.h"
#include "gl_cvars.h" // mat_overbright
#include "cmd.h" // Cmd_*
#include "console.h"  // ConMsg

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bool s_bAllow3DNow = true;
static bool s_bAllowSSE2 = true;


static void OnEnableFastMathChanged( IConVar *var = NULL, const char *pOldValue = NULL, float flOldValue = 0.0f);

ConVar enable_fast_math( "enable_fast_math", "1" , FCVAR_RELEASE, "Turns Denormals-Are-Zeroes and Flush-to-Zero on or off", OnEnableFastMathChanged );

// <sergiy> We should use consistent SSE flags on Linux and Windows to avoid different math/physics/casting results. 
//          Also, we should try to use FZ/DAZ when we can because without it, 
//          It would be preferable to use the verbose macros to set/reset FZ and DAZ modes on SSE, but 
//          in Linux, we are using an outdated version of GCC that doesn't have those defined in standard headers.
//          So I'll just throw literals here until such time as we rev the version of GCC.

static void OnEnableFastMathChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( enable_fast_math.GetBool() )
	{
		_mm_setcsr( _mm_getcsr() | 0x8040 );

		//_MM_SET_FLUSH_ZERO_MODE( _MM_FLUSH_ZERO_ON );
		//_MM_SET_DENORMALS_ZERO_MODE( _MM_DENORMALS_ZERO_ON );
	}
	else
	{
		_mm_setcsr( _mm_getcsr() & ~0x8040 );

		//_MM_SET_FLUSH_ZERO_MODE( _MM_FLUSH_ZERO_OFF );
		//_MM_SET_DENORMALS_ZERO_MODE( _MM_DENORMALS_ZERO_OFF );
	}
}

void InitMathlib( void )
{
	MathLib_Init( 2.2f, // v_gamma.GetFloat()
		2.2f, // v_texgamma.GetFloat()
		0.0f /*v_brightness.GetFloat() */, 
		2.0f /*mat_overbright.GetInt() */, s_bAllow3DNow, true, s_bAllowSSE2, true );
	OnEnableFastMathChanged();
}


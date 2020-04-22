#ifndef _CEGCLIENTWRAPPER_H_
#define _CEGCLIENTWRAPPER_H_

#pragma once

#include "xbox/xboxstubs.h" // defines DWORD, DWORD_PTR, etc.

// required by cegclient.h, but no need to pull in all of windef.h
#if defined( PLATFORM_64BITS )
	typedef int64 INT_PTR;
#else
	typedef	int32	INT_PTR;
#endif

typedef unsigned char BYTE;
typedef BYTE *LPBYTE;
typedef	int32	INT32;
typedef	uint32	DWORD32;
typedef	uint64	DWORD64;


#define CEG_GET_CONSTANT_VALUE_AVOID_CEG( fn_name )			fn_name()	// makes it easy for us to turn off CEG checks if one of the CEG'ed functions has a bigger impact on perf than expected

// To disable CEG in your build for one or more modules, add "/NO_CEG" to your VPC parameters
#if (!defined( USE_CEG ) || ( !defined( CLIENT_DLL ) && !defined( GAME_DLL ) ) )

#define CEG_NOINLINE

// Stub out functions if CEG is disabled
#define STEAMWORKS_REGISTERTHREAD() (true)
#define STEAMWORKS_UNREGISTERTHREAD() (true)
#define STEAMWORKS_INITCEGLIBRARY() (true)
#define STEAMWORKS_TERMCEGLIBRARY() (true)
#define STEAMWORKS_TESTSECRET()
#define STEAMWORKS_TESTSECRETALWAYS()
#define STEAMWORKS_SELFCHECK()
#define STEAMWORKS_TESTSECRET_AMORTIZE( period )
#define STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( period )
#define STEAMWORKS_SELFCHECK_AMORTIZE( period )
#define RANDOM_CEG_TEST_SECRET()
#define RANDOM_CEG_TEST_SECRET_PERIOD( testPeriod, checkPeriod )
#define RANDOM_CEG_TEST_SECRET_LINE_PERIOD( testPeriod, testLinePeriod, checkPeriod, checkLinePeriod )
#define CEG_PROTECT_FUNCTION( unquotedSymbolHelper )
#define	CEG_ENCRYPT_FUNCTION( unquotedSymbolHelper )
#define CEG_PROTECT_MEMBER_FUNCTION( unquotedSymbolHelper )
#define CEG_PROTECT_VIRTUAL_FUNCTION( unquotedSymbolHelper )
#define CEG_PROTECT_STATIC_MEMBER_FUNCTION( unquotedSymbolHelper, fn_name )
#define CEG_DECLARE_CONSTANT_FUNCTION( fn_name )			extern DWORD	__cdecl	fn_name();
#define CEG_DECLARE_CONSTANT_FLOAT_FUNCTION( fn_name )		extern float	__cdecl	fn_name();
#define CEG_DEFINE_CONSTANT_FUNCTION( fn_name, val )		DWORD __cdecl fn_name() { return val; }
#define CEG_DEFINE_CONSTANT_FLOAT_FUNCTION( fn_name, val )	float __cdecl fn_name() { return val; }
#define CEG_GET_CONSTANT_VALUE( fn_name )					fn_name()
#define CEG_GCV_PRE()
#define CEG_GCV_POST()

#else // CEG is enabled

#if defined( _GAMECONSOLE ) || defined( POSIX ) || defined( NO_STEAM )
#error
#endif

#include "cegclient.h"

// use this on any functions where we use CEG_PROTECT to ensure they don't get inlined in the CEG build
#define CEG_NOINLINE __declspec(noinline)

// wrapping CEG calls in macros allows us to hide interface changes and centrally add checks and perf instrumentation
#define STEAMWORKS_REGISTERTHREAD()							Steamworks_RegisterThread()
#define STEAMWORKS_UNREGISTERTHREAD()						Steamworks_UnRegisterThread()
#define STEAMWORKS_INITCEGLIBRARY()							Steamworks_InitCEGLibrary()
#define STEAMWORKS_TERMCEGLIBRARY()							Steamworks_TermCEGLibrary()
#define CEG_DECLARE_CONSTANT_FUNCTION( fn_name )			CEG_Declare_Constant_Function( fn_name )
#define CEG_DECLARE_CONSTANT_FLOAT_FUNCTION( fn_name )		CEG_Declare_ConstantFloat_Function( fn_name )
#define CEG_DEFINE_CONSTANT_FUNCTION( fn_name, val )		CEG_Define_Constant_Function( fn_name, val )
#define CEG_DEFINE_CONSTANT_FLOAT_FUNCTION( fn_name, val )	CEG_Define_ConstantFloat_Function2( fn_name, val )
#define CEG_GET_CONSTANT_VALUE( fn_name )					CEG_GetConstantValue( fn_name )

// seeded with large primes at frequencies that lead to approximately one check every 30 seconds
#define RANDOM_CEG_TEST_SECRET() \
	RANDOM_CEG_TEST_SECRET_LINE_PERIOD( 128449, 19, 1102441, 397 )

#define RANDOM_CEG_TEST_SECRET_PERIOD( testPeriod, checkPeriod ) \
	RANDOM_CEG_TEST_SECRET_LINE_PERIOD( testPeriod, 0, checkPeriod, 0 )

// uncomment here to enable dev messages per CEG call, including CEG performance numbers
//#define PROFILE_CEG
#if !defined( PROFILE_CEG )

#define CEG_PROFILE_DECL
#define CEG_PROFILE_BLOCK( fn_name, fn_type, identifier ) fn_name;
#define CEG_GCV_PRE()
#define CEG_GCV_POST()

#else // defined( PROFILE_CEG )

#include "tier0\fasttimer.h"

extern	CAverageCycleCounter	allCEG;
extern	CAverageCycleCounter	allTestSecret;
extern	CAverageCycleCounter	allSelfCheck;
extern	CAverageCycleCounter	allProtectMember;
extern	CAverageCycleCounter	allProtectVirtual;

#if !defined( MEMOVERRIDE_MODULE )
#define MEMOVERRIDE_MODULE UNKNOWN_MODULE
#endif

#define _MKSTRING(arg) #arg
#define MKSTRING(arg) _MKSTRING(arg)

#define CEG_PROFILE_BLOCK( fn_name, fn_type, identifier ) \
	static unsigned long s_##fn_type##Hits = 0; \
	s_##fn_type##Hits++; \
	static	CAverageCycleCounter	this##fn_type; \
	{	\
		CAverageTimeMarker	this##fn_type##Marker( &this##fn_type );	\
		CAverageTimeMarker	all##fn_type##Marker( &all##fn_type );	\
		CAverageTimeMarker	allCEGMarker( &allCEG );	\
		fn_name;  \
	}	\
	DevMsg( "%s: %s%s %s:%d - %.2fms avg (%.2fms total, %.2fms peak, %d iters), %lu/%lu = %.5f%% hit rate.\n"	\
			"Avg CEG %s Cost: %.2fms   Avg CEG Cost: %.2fms\n", \
			MKSTRING(MEMOVERRIDE_MODULE), identifier, #fn_type, __FILE__, __LINE__, \
			this##fn_type.GetAverageMilliseconds(), this##fn_type.GetTotalMilliseconds(), this##fn_type.GetPeakMilliseconds(), this##fn_type.GetIters(), \
			s_##fn_type##Hits, s_tests, \
			float( s_##fn_type##Hits * 100 ) / float( s_tests ), \
			#fn_type, \
			all##fn_type.GetAverageMilliseconds(), \
			allCEG.GetAverageMilliseconds() );

#define CEG_PROFILE_DECL \
	static unsigned long int s_tests = 0; \
	s_tests++;

#define CEG_GCV_PRE() \
	CFastTimer __FUNCTION__##Timer; \
	__FUNCTION__##Timer.Start();

#define CEG_GCV_POST() \
	__FUNCTION__##Timer.End(); \
	if( __FUNCTION__##Timer.GetDuration().GetMillisecondsF() > 0.5f ) \
{ \
	DevMsg( "%s: GetConstantValue in %s : %.2fms\n",	\
	MKSTRING(MEMOVERRIDE_MODULE), __FUNCTION__, __FUNCTION__##Timer.GetDuration().GetMillisecondsF() ); \
}

#endif // PROFILE_CEG off/on

// the below macros allow us to turn profiling on or off with a single #ifdef, using the same underlying implementation in each case
#define RANDOM_CEG_TEST_SECRET_LINE_PERIOD( testPeriod, testLinePeriod, checkPeriod, checkLinePeriod ) \
	do { \
		const unsigned long tp = testPeriod + testLinePeriod * __LINE__; \
		const unsigned long cp = checkPeriod + checkLinePeriod * __LINE__; \
		static unsigned long s_nCegCounter = __LINE__ ^ __COUNTER__ ^ reinterpret_cast<int>( &s_nCegCounter ); \
		++s_nCegCounter; \
		CEG_PROFILE_DECL; \
		if ( !( ( s_nCegCounter ) % ( tp ) ) ) \
		{ \
			CEG_PROFILE_BLOCK( Steamworks_TestSecretAlways(), TestSecret, "Random " ); \
		} \
		else if ( !( ( s_nCegCounter ) % ( cp ) ) ) \
		{ \
			CEG_PROFILE_BLOCK( Steamworks_SelfCheck(), SelfCheck, "Random " ); \
		} \
	} \
	while( 0 );

//
//	Can't grab metrics - although this is placed within the scope a function - 
//	The effect is to change calls to the function to first execute some CEG code, 
//	and this CEG code is not available within this scope.
//	The CEG code computes the address of the containing function, and jumps to it.
//
#define CEG_PROTECT_FUNCTION( unquotedSymbolHelper ) \
	CEG_ProtectFunction( );

//
//	Can't grab metrics - although this is placed within the scope a function - 
//	The effect is to change calls to the function to first execute some CEG code, 
//	and this CEG code is not available within this scope.
//
//	The body of the function that contains this macro is encrypted, and the CEG code
//	decrypts the opcodes and places them in executable memory, and then executes it.
//	Upon return, the executable memory is released.
//
#define	CEG_ENCRYPT_FUNCTION( unquotedSymbolHelper ) \
	CEG_EncryptFunction( );

#define CEG_PROTECT_MEMBER_FUNCTION( unquotedSymbolHelper ) \
	do { \
		CEG_PROFILE_DECL; \
		CEG_PROFILE_BLOCK( CEG_ProtectFunction( ), ProtectMember, "" ); \
	} \
	while( 0 );

#define CEG_PROTECT_VIRTUAL_FUNCTION( unquotedSymbolHelper ) \
	do { \
		CEG_PROFILE_DECL; \
		CEG_PROFILE_BLOCK( CEG_ProtectFunction( ), ProtectVirtual, "" ); \
	}	while( 0 );

// defined outside the scope of a function, so can't trivially grab metrics
#define CEG_PROTECT_STATIC_MEMBER_FUNCTION( unquotedSymbolHelper, fn_name ) \
	CEG_Protect_StaticMemberFunction( unquotedSymbolHelper, fn_name );

#define STEAMWORKS_TESTSECRET() \
	do { \
		CEG_PROFILE_DECL; \
		CEG_PROFILE_BLOCK( Steamworks_TestSecret(), TestSecret, "" ); \
	}	while( 0 );

#define STEAMWORKS_TESTSECRETALWAYS() \
	do { \
		CEG_PROFILE_DECL; \
		CEG_PROFILE_BLOCK( Steamworks_TestSecretAlways(), TestSecret, "" ); \
	}	while( 0 );

#define STEAMWORKS_SELFCHECK() \
	do { \
		CEG_PROFILE_DECL; \
		CEG_PROFILE_BLOCK( Steamworks_SelfCheck(), SelfCheck, "" ); \
	}	while( 0 );

// AMORTIZED

#define STEAMWORKS_TESTSECRET_AMORTIZE( period ) \
	do { \
		const unsigned long tp = period; \
		static unsigned long s_nCegCounter = __LINE__ ^ __COUNTER__ ^ reinterpret_cast<int>( &s_nCegCounter ); \
		++s_nCegCounter; \
		CEG_PROFILE_DECL; \
		if ( !( ( s_nCegCounter ) % ( tp ) ) ) \
		{ \
			CEG_PROFILE_BLOCK( Steamworks_TestSecret(), TestSecret, "Amortized " ); \
		} \
	}	while( 0 );

#define STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( period ) \
	do { \
		const unsigned long tp = period; \
		static unsigned long s_nCegCounter = __LINE__ ^ __COUNTER__ ^ reinterpret_cast<int>( &s_nCegCounter ); \
		++s_nCegCounter; \
		CEG_PROFILE_DECL; \
		if ( !( ( s_nCegCounter ) % ( tp ) ) ) \
		{ \
			CEG_PROFILE_BLOCK( Steamworks_TestSecretAlways(), TestSecret, "Amortized " ); \
		} \
	}	while( 0 );

#define STEAMWORKS_SELFCHECK_AMORTIZE( period ) \
	do { \
		const unsigned long tp = period; \
		static unsigned long s_nCegCounter = __LINE__ ^ __COUNTER__ ^ reinterpret_cast<int>( &s_nCegCounter ); \
		++s_nCegCounter; \
		CEG_PROFILE_DECL; \
		if ( !( ( s_nCegCounter ) % ( tp ) ) ) \
		{ \
			CEG_PROFILE_BLOCK( Steamworks_SelfCheck(), TestSecret, "Amortized " ); \
		} \
	}	while( 0 );


#endif // CEG disabled/enabled

#if defined( CLIENT_DLL )							// client-only

void Init_GCVs();

CEG_DECLARE_CONSTANT_FUNCTION( HudAllowTextChatFlag );
CEG_DECLARE_CONSTANT_FUNCTION( HudAllowBuyMenuFlag );
CEG_DECLARE_CONSTANT_FUNCTION( UiAllowProperTintFlag );

#elif defined( GAME_DLL )							// server-only

void Init_GCVs();

#endif // defined( GAME_DLL )

// Worst-case, if you need to debug CEG calls or CEG perf, and CEG profiling isn't doing the trick (it doesn't cover everything), you can
// do a binary search on one or all of the below calls by restoring part of the #if 0 block below. You'll get a duplicate definition warning,
// but anything redefined below will be a no-op in the build, allowing you to rule it out as a potential perf issue.
// Remember that CEG builds neeed to be rebuilds (CEG'ed dlls only), but the /MP flag should make it about 60 seconds per rebuild.
#if 0
#define STEAMWORKS_TESTSECRET()
#define STEAMWORKS_TESTSECRETALWAYS()
#define STEAMWORKS_SELFCHECK()
#define RANDOM_CEG_TEST_SECRET()
#define RANDOM_CEG_TEST_SECRET_PERIOD( testPeriod, checkPeriod )
#define RANDOM_CEG_TEST_SECRET_LINE_PERIOD( testPeriod, testLinePeriod, checkPeriod, checkLinePeriod )
#define CEG_PROTECT_FUNCTION( unquotedSymbolHelper )
#define CEG_PROTECT_MEMBER_FUNCTION( unquotedSymbolHelper )
#define CEG_PROTECT_VIRTUAL_FUNCTION( unquotedSymbolHelper )
#define CEG_PROTECT_STATIC_MEMBER_FUNCTION( unquotedSymbolHelper, fn_name )
#define CEG_DECLARE_CONSTANT_FUNCTION( fn_name )			extern DWORD	__cdecl	fn_name();
#define CEG_DECLARE_CONSTANT_FLOAT_FUNCTION( fn_name )		extern float	__cdecl	fn_name();
#define CEG_DEFINE_CONSTANT_FUNCTION( fn_name, val )		DWORD __cdecl fn_name() { return val; }
#define CEG_DEFINE_CONSTANT_FLOAT_FUNCTION( fn_name, val )	float __cdecl fn_name() { return val; }
#define CEG_GET_CONSTANT_VALUE( fn_name )					fn_name()
#endif

#endif //_CEGCLIENTWRAPPER_H_
//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: - defines SIMD "structure of arrays" classes and functions.
//
//===========================================================================//
#ifndef SSEMATH_H
#define SSEMATH_H

#if defined( _X360 )
#include <xboxmath.h>
#elif defined ( _PS3 )
#include <vectormath/c/vectormath_aos.h>
#include <vectormath/c/vectormath_aos_v.h>
#else
#include <xmmintrin.h>
#ifndef _LINUX
#include <emmintrin.h>
#endif
#endif

#ifndef SPU
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#else
#include "mathlib/math_pfns.h"
#endif

#include "mathlib/fltx4.h"

// The FLTX4 type is a fltx4 used as a parameter to a function.
// On the 360, the best way to do this is pass-by-copy on the registers.
// On the PC, the best way is to pass by const reference. 
// The compiler will sometimes, but not always, replace a pass-by-const-ref
// with a pass-in-reg on the 360; to avoid this confusion, you can
// explicitly use a FLTX4 as the parameter type.
#ifdef _X360
typedef __vector4 FLTX4;
#elif defined( _PS3 )
typedef vec_float4 FLTX4;
#else
typedef const fltx4 & FLTX4;
#endif

// A 16-byte aligned int32 datastructure
// (for use when writing out fltx4's as SIGNED
// ints).
struct ALIGN16 intx4
{
	int32 m_i32[4];

	inline int & operator[](int which) 
	{
		return m_i32[which];
	}

	inline const int & operator[](int which) const
	{
		return m_i32[which];
	}

	inline int32 *Base() {
		return m_i32;
	}

	inline const int32 *Base() const
	{
		return m_i32;
	}

	inline bool operator==(const intx4 &other) const
	{
		return m_i32[0] == other.m_i32[0] &&
			m_i32[1] == other.m_i32[1] &&
			m_i32[2] == other.m_i32[2] &&
			m_i32[3] == other.m_i32[3] 	;
	}
} ALIGN16_POST;


#if defined( _DEBUG ) && defined( _X360 )
FORCEINLINE void TestVPUFlags()
{
	// Check that the VPU is in the appropriate (Java-compliant) mode (see 3.2.1 in altivec_pem.pdf on xds.xbox.com)
	__vector4 a;
	__asm
	{
		mfvscr	a;
	}
	unsigned int * flags		= (unsigned int *)&a;
	unsigned int   controlWord	= flags[3];
	Assert(controlWord == 0);
}
#else  // _DEBUG
FORCEINLINE void TestVPUFlags() {}
#endif // _DEBUG


// useful constants in SIMD packed float format:
// (note: some of these aren't stored on the 360, 
// but are manufactured directly in one or two 
// instructions, saving a load and possible L2
// miss.)

#ifdef _X360
// Shouldn't the PS3 have something similar?
#define			   Four_Zeros			XMVectorZero()			// 0 0 0 0
#define			   Four_Ones			XMVectorSplatOne()		// 1 1 1 1
extern const fltx4 Four_Twos;									// 2 2 2 2
extern const fltx4 Four_Threes;									// 3 3 3 3
extern const fltx4 Four_Fours;									// guess.
extern const fltx4 Four_Point225s;								// .225 .225 .225 .225
extern const fltx4 Four_PointFives;								// .5 .5 .5 .5
extern const fltx4 Four_Thirds;									// 1/3
extern const fltx4 Four_TwoThirds;								// 2/3
extern const fltx4 Four_NegativeOnes;							// -1 -1 -1 -1 
extern const fltx4 Four_DegToRad;								// (float)(M_PI_F / 180.f) times four
#elif defined(SPU)
#define			   Four_Zeros			spu_splats( 0.0f )		// 0 0 0 0
#define			   Four_Ones			spu_splats( 1.0f )		// 1 1 1 1
#define			   Four_Twos			spu_splats( 2.0f )		// 2 2 2 2
#define			   Four_Threes			spu_splats( 3.0f )		// 3 3 3 3
#define			   Four_Fours			spu_splats( 4.0f )		// guess.
#define			   Four_Point225s		spu_splats( 0.225f )		// .225 .225 .225 .225
#define			   Four_PointFives		spu_splats( 0.5f )		// .5 .5 .5 .5
#define			   Four_Thirds			spu_splats( 0.33333333 );	// 1/3
#define			   Four_TwoThirds		spu_splats( 0.66666666 );	// 2/3
#define			   Four_NegativeOnes	spu_splats( -1.0f )		// -1 -1 -1 -1 
#define			   Four_DegToRad		spu_splats((float)(M_PI_F / 180.f))
#else
extern const fltx4 Four_Zeros;									// 0 0 0 0
extern const fltx4 Four_Ones;									// 1 1 1 1
extern const fltx4 Four_Twos;									// 2 2 2 2
extern const fltx4 Four_Threes;									// 3 3 3 3
extern const fltx4 Four_Fours;									// guess.
extern const fltx4 Four_Point225s;								// .225 .225 .225 .225
extern const fltx4 Four_PointFives;								// .5 .5 .5 .5
extern const fltx4 Four_Thirds;									// 1/3
extern const fltx4 Four_TwoThirds;								// 2/3
extern const fltx4 Four_NegativeOnes;							// -1 -1 -1 -1 
extern const fltx4 Four_DegToRad;								// (float)(M_PI_F / 180.f) times four
#endif
extern const fltx4 Four_Epsilons;								// FLT_EPSILON FLT_EPSILON FLT_EPSILON FLT_EPSILON
extern const fltx4 Four_2ToThe21s;								// (1<<21)..
extern const fltx4 Four_2ToThe22s;								// (1<<22)..
extern const fltx4 Four_2ToThe23s;								// (1<<23)..
extern const fltx4 Four_2ToThe24s;								// (1<<24)..
extern const fltx4 Four_Origin;									// 0 0 0 1 (origin point, like vr0 on the PS2)
extern const fltx4 Four_FLT_MAX;								// FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX
extern const fltx4 Four_Negative_FLT_MAX;						// -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX
extern const fltx4 g_SIMD_0123;									// 0 1 2 3 as float


// coefficients for polynomial approximation of srgb conversions

// 4th order polynomial for x^(1/2.2), x in 0..1
extern const fltx4 Four_LinearToGammaCoefficients_A;		// *x^4
extern const fltx4 Four_LinearToGammaCoefficients_B;		// *x^3
extern const fltx4 Four_LinearToGammaCoefficients_C;		// *x^2
extern const fltx4 Four_LinearToGammaCoefficients_D;		// *x^1
extern const fltx4 Four_LinearToGammaCoefficients_E;		// *x^0

// 3rd order polynomial for x^2.2 x in 0..1
extern const fltx4 Four_GammaToLinearCoefficients_A;		// *x^3
extern const fltx4 Four_GammaToLinearCoefficients_B;		// *x^2
extern const fltx4 Four_GammaToLinearCoefficients_C;		// *x^1
extern const fltx4 Four_GammaToLinearCoefficients_D;		// *x^0


// external aligned integer constants
#ifndef ALIGN16_POST
#define ALIGN16_POST
#endif
extern const ALIGN16 int32 g_SIMD_clear_signmask[] ALIGN16_POST;			// 0x7fffffff x 4
extern const ALIGN16 int32 g_SIMD_signmask[] ALIGN16_POST;				// 0x80000000 x 4
extern const ALIGN16 int32 g_SIMD_lsbmask[] ALIGN16_POST;				// 0xfffffffe x 4
extern const ALIGN16 int32 g_SIMD_clear_wmask[] ALIGN16_POST;			// -1 -1 -1 0
extern const ALIGN16 int32 g_SIMD_ComponentMask[4][4] ALIGN16_POST;		// [0xFFFFFFFF 0 0 0], [0 0xFFFFFFFF 0 0], [0 0 0xFFFFFFFF 0], [0 0 0 0xFFFFFFFF]
extern const ALIGN16 int32 g_SIMD_AllOnesMask[] ALIGN16_POST;			// ~0,~0,~0,~0
extern const fltx4 g_SIMD_Identity[4];									// [1 0 0 0], [0 1 0 0], [0 0 1 0], [0 0 0 1]
extern const ALIGN16 int32 g_SIMD_Low16BitsMask[] ALIGN16_POST;			// 0xffff x 4

// this mask is used for skipping the tail of things. If you have N elements in an array, and wish
// to mask out the tail, g_SIMD_SkipTailMask[N & 3] what you want to use for the last iteration.
extern const int32 ALIGN16 g_SIMD_SkipTailMask[4][4] ALIGN16_POST;

extern const int32 ALIGN16 g_SIMD_EveryOtherMask[];				// 0, ~0, 0, ~0
// Define prefetch macros.
// The characteristics of cache and prefetch are completely 
// different between the different platforms, so you DO NOT
// want to just define one macro that maps to every platform
// intrinsic under the hood -- you need to prefetch at different
// intervals between x86 and PPC, for example, and that is
// a higher level code change. 
// On the other hand, I'm tired of typing #ifdef _X360
// all over the place, so this is just a nop on Intel, PS3.
#ifdef PLATFORM_PPC
#if defined(_X360)
#define PREFETCH360(address, offset) __dcbt(offset,address)
#elif defined(_PS3)
#define PREFETCH360(address, offset) __dcbt( reinterpret_cast< const char * >(address) + offset )
#else
#error Prefetch not defined for this platform!
#endif
#else
#define PREFETCH360(x,y) // nothing
#endif

// Here's a handy function to align a pointer to the next
// sixteen byte boundary -- it'll round it up to the nearest
// multiple of 16. This is useful if you're subdividing
// big swaths of allocated memory, but in that case, remember
// to leave yourself the necessary slack in the allocation.
template<class T>
inline T *AlignPointer(void * ptr)
{
#if defined( __clang__ )
	uintp temp = (uintp)ptr;
#else
	unsigned temp = ptr;
#endif
	temp = ALIGN_VALUE(temp, sizeof(T));
	return (T *)temp;
}

#ifdef _PS3

// Note that similar defines exist in math_pfns.h
// Maybe we should consolidate in one place for all platforms.

#define _VEC_CLEAR_SIGNMASK (__vector unsigned int)		{0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff}
#define _VEC_SIGNMASK		(__vector unsigned int)		{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 }
#define _VEC_LSBMASK		(__vector unsigned int)		{ 0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe }
#define _VEC_CLEAR_WMASK	(__vector unsigned int)		{0xffffffff, 0xffffffff, 0xffffffff, 0}
#define _VEC_COMPONENT_MASK_0 (__vector unsigned int)	{0xffffffff, 0, 0, 0}
#define _VEC_COMPONENT_MASK_1 (__vector unsigned int)	{0, 0xffffffff, 0, 0}
#define _VEC_COMPONENT_MASK_2 (__vector unsigned int)	{0, 0, 0xffffffff, 0}
#define _VEC_COMPONENT_MASK_3 (__vector unsigned int)	{0, 0, 0, 0xffffffff}

#define _VEC_SWIZZLE_WZYX (__vector unsigned char)		{ 0x0c,0x0d,0x0e,0x0f, 0x08,0x09,0x0a,0x0b, 0x04,0x05,0x06,0x07, 0x00,0x01,0x02,0x03 }
#define _VEC_SWIZZLE_ZWXY (__vector unsigned char)		{ 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f, 0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07 }
#define _VEC_SWIZZLE_YXWZ (__vector unsigned char)		{ 0x04,0x05,0x06,0x07, 0x00,0x01,0x02,0x03, 0x0c,0x0d,0x0e,0x0f, 0x08,0x09,0x0a,0x0b }

#define _VEC_ZERO           (__vector unsigned int)		{0,0,0,0}

#define _VEC_FLTMAX			(__vector float)			{FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX}
#define _VEC_FLTMIN			(__vector float)			{FLT_MIN,FLT_MIN,FLT_MIN,FLT_MIN}

#define _VEC_ORIGIN			(__vector unsigned int)		{ 0x00000000, 0x00000000, 0x00000000, 0xffffffff }

#endif

#if USE_STDC_FOR_SIMD

//---------------------------------------------------------------------
// Standard C (fallback/Linux) implementation (only there for compat - slow)
//---------------------------------------------------------------------

FORCEINLINE float SubFloat( const fltx4 & a, int idx )
{
	return a.m128_f32[ idx ];
}

FORCEINLINE float & SubFloat( fltx4 & a, int idx )
{
	return a.m128_f32[idx];
}

FORCEINLINE uint32 SubInt( const fltx4 & a, int idx )
{
	return a.m128_u32[idx];
}

FORCEINLINE uint32 & SubInt( fltx4 & a, int idx )
{
	return a.m128_u32[idx];
}

// Return one in the fastest way -- on the x360, faster even than loading.
FORCEINLINE fltx4 LoadZeroSIMD( void )
{
	return Four_Zeros;
}

// Return one in the fastest way -- on the x360, faster even than loading.
FORCEINLINE fltx4 LoadOneSIMD( void )
{
	return Four_Ones;
}

FORCEINLINE fltx4 SplatXSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = SubFloat( a, 0 );
	SubFloat( retVal, 2 ) = SubFloat( a, 0 );
	SubFloat( retVal, 3 ) = SubFloat( a, 0 );
	return retVal;
}

FORCEINLINE fltx4 SplatYSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 1 );
	SubFloat( retVal, 1 ) = SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = SubFloat( a, 1 );
	SubFloat( retVal, 3 ) = SubFloat( a, 1 );
	return retVal;
}

FORCEINLINE fltx4 SplatZSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 2 );
	SubFloat( retVal, 1 ) = SubFloat( a, 2 );
	SubFloat( retVal, 2 ) = SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = SubFloat( a, 2 );
	return retVal;
}

FORCEINLINE fltx4 SplatWSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 3 );
	SubFloat( retVal, 1 ) = SubFloat( a, 3 );
	SubFloat( retVal, 2 ) = SubFloat( a, 3 );
	SubFloat( retVal, 3 ) = SubFloat( a, 3 );
	return retVal;
}

FORCEINLINE fltx4 SetXSIMD( const fltx4& a, const fltx4& x )
{
	fltx4 result = a;
	SubFloat( result, 0 ) = SubFloat( x, 0 );
	return result;
}

FORCEINLINE fltx4 SetYSIMD( const fltx4& a, const fltx4& y )
{
	fltx4 result = a;
	SubFloat( result, 1 ) = SubFloat( y, 1 );
	return result;
}

FORCEINLINE fltx4 SetZSIMD( const fltx4& a, const fltx4& z )
{
	fltx4 result = a;
	SubFloat( result, 2 ) = SubFloat( z, 2 );
	return result;
}

FORCEINLINE fltx4 SetWSIMD( const fltx4& a, const fltx4& w )
{
	fltx4 result = a;
	SubFloat( result, 3 ) = SubFloat( w, 3 );
	return result;
}

FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, int nComponent, float flValue )
{
	fltx4 result = a;
	SubFloat( result, nComponent ) = flValue;
	return result;
}


// a b c d -> b c d a
FORCEINLINE fltx4 RotateLeft( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 1 );
	SubFloat( retVal, 1 ) = SubFloat( a, 2 );
	SubFloat( retVal, 2 ) = SubFloat( a, 3 );
	SubFloat( retVal, 3 ) = SubFloat( a, 0 );
	return retVal;
}

// a b c d -> c d a b
FORCEINLINE fltx4 RotateLeft2( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 2 );
	SubFloat( retVal, 1 ) = SubFloat( a, 3 );
	SubFloat( retVal, 2 ) = SubFloat( a, 0 );
	SubFloat( retVal, 3 ) = SubFloat( a, 1 );
	return retVal;
}

#define BINOP(op) 														\
	fltx4 retVal;                                          				\
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) op SubFloat( b, 0 ) );	\
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) op SubFloat( b, 1 ) );	\
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) op SubFloat( b, 2 ) );	\
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) op SubFloat( b, 3 ) );	\
    return retVal;

#define IBINOP(op) 														\
	fltx4 retVal;														\
	SubInt( retVal, 0 ) = ( SubInt( a, 0 ) op SubInt ( b, 0 ) );		\
	SubInt( retVal, 1 ) = ( SubInt( a, 1 ) op SubInt ( b, 1 ) );		\
	SubInt( retVal, 2 ) = ( SubInt( a, 2 ) op SubInt ( b, 2 ) );		\
	SubInt( retVal, 3 ) = ( SubInt( a, 3 ) op SubInt ( b, 3 ) );		\
    return retVal;

FORCEINLINE fltx4 AddSIMD( const fltx4 & a, const fltx4 & b )
{
	BINOP(+);
}

FORCEINLINE fltx4 SubSIMD( const fltx4 & a, const fltx4 & b )				// a-b
{
	BINOP(-);
};

FORCEINLINE fltx4 MulSIMD( const fltx4 & a, const fltx4 & b )				// a*b
{
	BINOP(*);
}

FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )				// a/b
{
	BINOP(/);
}

FORCEINLINE fltx4 DivEstSIMD( const fltx4 & a, const fltx4 & b )			// a/b
{
	BINOP(/);
}

FORCEINLINE fltx4 MaddSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// a*b + c
{
	return AddSIMD( MulSIMD(a,b), c );
}

FORCEINLINE fltx4 MsubSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// c - a*b
{
	return SubSIMD( c, MulSIMD(a,b) );
};


FORCEINLINE fltx4 SinSIMD( const fltx4 &radians )
{
	fltx4 result;
	SubFloat( result, 0 ) = sin( SubFloat( radians, 0 ) );
	SubFloat( result, 1 ) = sin( SubFloat( radians, 1 ) );
	SubFloat( result, 2 ) = sin( SubFloat( radians, 2 ) );
	SubFloat( result, 3 ) = sin( SubFloat( radians, 3 ) );
	return result;
}

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
}

FORCEINLINE void SinCosSIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
	SinCos( SubFloat( radians, 3 ), &SubFloat( sine, 3 ), &SubFloat( cosine, 3 ) );
}

FORCEINLINE fltx4 ArcSinSIMD( const fltx4 &sine )
{
	fltx4 result;
	SubFloat( result, 0 ) = asin( SubFloat( sine, 0 ) );
	SubFloat( result, 1 ) = asin( SubFloat( sine, 1 ) );
	SubFloat( result, 2 ) = asin( SubFloat( sine, 2 ) );
	SubFloat( result, 3 ) = asin( SubFloat( sine, 3 ) );
	return result;
}

FORCEINLINE fltx4 ArcCosSIMD( const fltx4 &cs )
{
	fltx4 result;
	SubFloat( result, 0 ) = acos( SubFloat( cs, 0 ) );
	SubFloat( result, 1 ) = acos( SubFloat( cs, 1 ) );
	SubFloat( result, 2 ) = acos( SubFloat( cs, 2 ) );
	SubFloat( result, 3 ) = acos( SubFloat( cs, 3 ) );
	return result;
}

// tan^1(a/b) .. ie, pass sin in as a and cos in as b
FORCEINLINE fltx4 ArcTan2SIMD( const fltx4 &a, const fltx4 &b )
{
	fltx4 result;
	SubFloat( result, 0 ) = atan2( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( result, 1 ) = atan2( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( result, 2 ) = atan2( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( result, 3 ) = atan2( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return result;
}

FORCEINLINE fltx4 MaxSIMD( const fltx4 & a, const fltx4 & b )				// max(a,b)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = max( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( retVal, 1 ) = max( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( retVal, 2 ) = max( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( retVal, 3 ) = max( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 MinSIMD( const fltx4 & a, const fltx4 & b )				// min(a,b)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = min( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( retVal, 1 ) = min( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( retVal, 2 ) = min( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( retVal, 3 ) = min( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const fltx4 & b )				// a & b
{
	IBINOP(&);
}

FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// ~a & b
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ~SubInt( a, 0 ) & SubInt( b, 0 );
	SubInt( retVal, 1 ) = ~SubInt( a, 1 ) & SubInt( b, 1 );
	SubInt( retVal, 2 ) = ~SubInt( a, 2 ) & SubInt( b, 2 );
	SubInt( retVal, 3 ) = ~SubInt( a, 3 ) & SubInt( b, 3 );
	return retVal;
}

FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
	IBINOP(^);
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
	IBINOP(|);
}

FORCEINLINE fltx4 NegSIMD(const fltx4 &a) // negate: -a
{
	fltx4 retval;
	SubFloat( retval, 0 ) = -SubFloat( a, 0 );
	SubFloat( retval, 1 ) = -SubFloat( a, 1 );
	SubFloat( retval, 2 ) = -SubFloat( a, 2 );
	SubFloat( retval, 3 ) = -SubFloat( a, 3 );

	return retval;
}

FORCEINLINE bool IsAllZeros( const fltx4 & a )								// all floats of a zero?
{
	return	( SubFloat( a, 0 ) == 0.0 ) &&
		( SubFloat( a, 1 ) == 0.0 ) &&
		( SubFloat( a, 2 ) == 0.0 ) &&
		( SubFloat( a, 3 ) == 0.0 ) ;
}


// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	return	SubFloat(a,0) > SubFloat(b,0) &&
		SubFloat(a,1) > SubFloat(b,1) &&
		SubFloat(a,2) > SubFloat(b,2) &&
		SubFloat(a,3) > SubFloat(b,3);
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	return	SubFloat(a,0) >= SubFloat(b,0) &&
		SubFloat(a,1) >= SubFloat(b,1) &&
		SubFloat(a,2) >= SubFloat(b,2) &&
		SubFloat(a,3) >= SubFloat(b,3);
}

// For branching if all a.xyzw == b.xyzw
FORCEINLINE bool IsAllEqual( const fltx4 & a, const fltx4 & b )
{
	return	SubFloat(a,0) == SubFloat(b,0) &&
		SubFloat(a,1) == SubFloat(b,1) &&
		SubFloat(a,2) == SubFloat(b,2) &&
		SubFloat(a,3) == SubFloat(b,3);
}

// For branching if a.x == b.x || a.y == b.y || a.z == b.z || a.w == b.w
FORCEINLINE bool IsAnyEqual( const fltx4 & a, const fltx4 & b )
{
	return	SubFloat(a,0) == SubFloat(b,0) ||
			SubFloat(a,1) == SubFloat(b,1) ||
			SubFloat(a,2) == SubFloat(b,2) ||
			SubFloat(a,3) == SubFloat(b,3);
}

FORCEINLINE int TestSignSIMD( const fltx4 & a )								// mask of which floats have the high bit set
{
	int nRet = 0;

	nRet |= ( SubInt( a, 0 ) & 0x80000000 ) >> 31; // sign(x) -> bit 0
	nRet |= ( SubInt( a, 1 ) & 0x80000000 ) >> 30; // sign(y) -> bit 1
	nRet |= ( SubInt( a, 2 ) & 0x80000000 ) >> 29; // sign(z) -> bit 2
	nRet |= ( SubInt( a, 3 ) & 0x80000000 ) >> 28; // sign(w) -> bit 3

	return nRet;
}

FORCEINLINE bool IsAnyNegative( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

FORCEINLINE bool IsAnyTrue( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

FORCEINLINE fltx4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) == SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) == SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) == SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) == SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpGtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) > SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) > SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) > SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) > SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpGeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) >= SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) >= SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) >= SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) >= SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpLtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) < SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) < SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) < SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) < SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpLeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? ~0 : 0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 ) && SubFloat( a, 0 ) >= -SubFloat( b, 0 ) ) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 ) && SubFloat( a, 1 ) >= -SubFloat( b, 1 ) ) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 ) && SubFloat( a, 2 ) >= -SubFloat( b, 2 ) ) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 ) && SubFloat( a, 3 ) >= -SubFloat( b, 3 ) ) ? ~0 : 0;
	return retVal;
}


FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
	return OrSIMD(
		AndSIMD( ReplacementMask, NewValue ),
		AndNotSIMD( ReplacementMask, OldValue ) );
}

FORCEINLINE fltx4 ReplicateX4( float flValue )					//  a,a,a,a
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = flValue;
	SubFloat( retVal, 1 ) = flValue;
	SubFloat( retVal, 2 ) = flValue;
	SubFloat( retVal, 3 ) = flValue;
	return retVal;
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE fltx4 ReplicateIX4( int nValue )
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = nValue;
	SubInt( retVal, 1 ) = nValue;
	SubInt( retVal, 2 ) = nValue;
	SubInt( retVal, 3 ) = nValue;
	return retVal;

}

// Round towards positive infinity
FORCEINLINE fltx4 CeilSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ceil( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = ceil( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = ceil( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = ceil( SubFloat( a, 3 ) );
	return retVal;

}

// Round towards negative infinity
FORCEINLINE fltx4 FloorSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = floor( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = floor( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = floor( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = floor( SubFloat( a, 3 ) );
	return retVal;

}

FORCEINLINE fltx4 SqrtEstSIMD( const fltx4 & a )				// sqrt(a), more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 SqrtSIMD( const fltx4 & a )					// sqrt(a)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtEstSIMD( const fltx4 & a )		// 1/sqrt(a), more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) != 0.0f ? SubFloat( a, 0 ) : FLT_EPSILON );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) != 0.0f ? SubFloat( a, 1 ) : FLT_EPSILON );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) != 0.0f ? SubFloat( a, 2 ) : FLT_EPSILON );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) != 0.0f ? SubFloat( a, 3 ) : FLT_EPSILON );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtSIMD( const fltx4 & a )			// 1/sqrt(a)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalEstSIMD( const fltx4 & a )			// 1/a, more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = 1.0 / SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = 1.0 / SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = 1.0 / SubFloat( a, 3 );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )				// 1/a
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = 1.0 / SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = 1.0 / SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = 1.0 / SubFloat( a, 3 );
	return retVal;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / (SubFloat( a, 0 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 0 ));
	SubFloat( retVal, 1 ) = 1.0 / (SubFloat( a, 1 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 1 ));
	SubFloat( retVal, 2 ) = 1.0 / (SubFloat( a, 2 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 2 ));
	SubFloat( retVal, 3 ) = 1.0 / (SubFloat( a, 3 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 3 ));
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / (SubFloat( a, 0 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 0 ));
	SubFloat( retVal, 1 ) = 1.0 / (SubFloat( a, 1 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 1 ));
	SubFloat( retVal, 2 ) = 1.0 / (SubFloat( a, 2 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 2 ));
	SubFloat( retVal, 3 ) = 1.0 / (SubFloat( a, 3 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 3 ));
	return retVal;
}

// 2^x for all values (the antilog)
FORCEINLINE fltx4 ExpSIMD( const fltx4 &toPower )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = powf( 2, SubFloat(toPower, 0) );
	SubFloat( retVal, 1 ) = powf( 2, SubFloat(toPower, 1) );
	SubFloat( retVal, 2 ) = powf( 2, SubFloat(toPower, 2) );
	SubFloat( retVal, 3 ) = powf( 2, SubFloat(toPower, 3) );

	return retVal;
}

FORCEINLINE fltx4 Dot3SIMD( const fltx4 &a, const fltx4 &b )
{
	float flDot = SubFloat( a, 0 ) * SubFloat( b, 0 ) +
		SubFloat( a, 1 ) * SubFloat( b, 1 ) + 
		SubFloat( a, 2 ) * SubFloat( b, 2 );
	return ReplicateX4( flDot );
}

FORCEINLINE fltx4 Dot4SIMD( const fltx4 &a, const fltx4 &b )
{
	float flDot = SubFloat( a, 0 ) * SubFloat( b, 0 ) +
		SubFloat( a, 1 ) * SubFloat( b, 1 ) + 
		SubFloat( a, 2 ) * SubFloat( b, 2 ) +
		SubFloat( a, 3 ) * SubFloat( b, 3 );
	return ReplicateX4( flDot );
}

// Clamps the components of a vector to a specified minimum and maximum range.
FORCEINLINE fltx4 ClampVectorSIMD( FLTX4 in, FLTX4 min, FLTX4 max)
{
	return MaxSIMD( min, MinSIMD( max, in ) );
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	fltx4 retval;
	retval = a;
	SubFloat( retval, 0 ) = 0;
	return retval;
}

FORCEINLINE fltx4 LoadUnalignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

FORCEINLINE fltx4 LoadUnaligned3SIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

// load a single unaligned float into the x component of a SIMD word
FORCEINLINE fltx4 LoadUnalignedFloatSIMD( const float *pFlt )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = *pFlt;
	return retval;
}

FORCEINLINE fltx4 LoadAlignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned & pSIMD )
{
	fltx4 retval = LoadAlignedSIMD(pSIMD.Base());
	// squelch w
	SubInt( retval, 3 ) = 0;
	return retval;
}


// construct a fltx4 from four different scalars, which are assumed to be neither aligned nor contiguous
FORCEINLINE fltx4 LoadGatherSIMD( const float &x, const float &y, const float &z, const float &w )
{
	fltx4 retval = { x, y, z, w };
	return retval;
}

FORCEINLINE void StoreAlignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnalignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnalignedFloat( float *pSingleFloat, const fltx4 & a )
{
	*pSingleFloat  = SubFloat( a, 0 );
}

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	*pSIMD     = SubFloat(a, 0);
	*(pSIMD+1) = SubFloat(a, 1);
	*(pSIMD+2) = SubFloat(a, 2);
}


// strongly typed -- syntactic castor oil used for typechecking as we transition to SIMD
FORCEINLINE void StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, const fltx4 & a )
{
	StoreAlignedSIMD(pSIMD->Base(),a);
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination[0],  pDestination[1],  pDestination[2],  pDestination[3]
// The Vectors are assumed to be unaligned.
FORCEINLINE void StoreFourUnalignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											   Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination ,  pDestination + 1,  pDestination + 2,  pDestination + 3
// The Vectors are assumed to start on an ALIGNED address, that is, 
// pDestination is 16-byte aligned (thhough obviously pDestination+1 is not).
FORCEINLINE void StoreFourAlignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											 Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}


FORCEINLINE void TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w )
{
#define SWAP_FLOATS( _a_, _ia_, _b_, _ib_ ) { float tmp = SubFloat( _a_, _ia_ ); SubFloat( _a_, _ia_ ) = SubFloat( _b_, _ib_ ); SubFloat( _b_, _ib_ ) = tmp; }
	SWAP_FLOATS( x, 1, y, 0 );
	SWAP_FLOATS( x, 2, z, 0 );
	SWAP_FLOATS( x, 3, w, 0 );
	SWAP_FLOATS( y, 2, z, 1 );
	SWAP_FLOATS( y, 3, w, 1 );
	SWAP_FLOATS( z, 3, w, 2 );
}

// find the lowest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
FORCEINLINE fltx4 FindLowestSIMD3( const fltx4 & a )
{
	float lowest = min( min( SubFloat(a, 0), SubFloat(a, 1) ), SubFloat(a, 2));
	return ReplicateX4(lowest);
}

// find the highest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
FORCEINLINE fltx4 FindHighestSIMD3( const fltx4 & a )
{
	float highest = max( max( SubFloat(a, 0), SubFloat(a, 1) ), SubFloat(a, 2));
	return ReplicateX4(highest);
}

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing 
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way 
// to guarantee an immediate as a parameter to function
// like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
	(*pDest)[0] = SubFloat(vSrc, 0);
	(*pDest)[1] = SubFloat(vSrc, 1);
	(*pDest)[2] = SubFloat(vSrc, 2);
	(*pDest)[3] = SubFloat(vSrc, 3);
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------
// splat all components of a vector to a signed immediate int number.
FORCEINLINE fltx4 IntSetImmediateSIMD( int nValue )
{
	fltx4 retval;
	SubInt( retval, 0 ) = SubInt( retval, 1 ) = SubInt( retval, 2 ) = SubInt( retval, 3) = nValue;
	return retval;
}

// Load 4 aligned words into a SIMD register
FORCEINLINE i32x4 LoadAlignedIntSIMD(const void * RESTRICT pSIMD)
{
	return *( reinterpret_cast< const i32x4 *> ( pSIMD ) );
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD( const void * RESTRICT pSIMD)
{
	return *( reinterpret_cast< const i32x4 *> ( pSIMD ) );
}

// save into four words, 16-byte aligned
FORCEINLINE void StoreAlignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreAlignedIntSIMD( intx4 &pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD.Base() ) ) = a;
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

// Load four consecutive uint16's, and turn them into floating point numbers.
// This function isn't especially fast and could be made faster if anyone is
// using it heavily.
FORCEINLINE fltx4 LoadAndConvertUint16SIMD( const uint16 *pInts )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = pInts[0];
	SubFloat( retval, 1 ) = pInts[1];
	SubFloat( retval, 2 ) = pInts[2];
	SubFloat( retval, 3 ) = pInts[3];
}


// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. No
// fixed point conversion is done.
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const u32x4 &vSrcA )
{
	Assert(0);			/* pc has no such operation */
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) SubInt( vSrcA, 0 ) );
	SubFloat( retval, 1 ) = ( (float) SubInt( vSrcA, 1 ) );
	SubFloat( retval, 2 ) = ( (float) SubInt( vSrcA, 2 ) );
	SubFloat( retval, 3 ) = ( (float) SubInt( vSrcA, 3 ) );
	return retval;
}


#if 0				/* pc has no such op */
// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[0])) );
	SubFloat( retval, 1 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[1])) );
	SubFloat( retval, 2 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[2])) );
	SubFloat( retval, 3 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[3])) );
	return retval;
}


/*
  works on fltx4's as if they are four uints.
  the first parameter contains the words to be shifted,
  the second contains the amount to shift by AS INTS

  for i = 0 to 3
  shift = vSrcB_i*32:(i*32)+4
  vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
FORCEINLINE i32x4 IntShiftLeftWordSIMD(const i32x4 &vSrcA, const i32x4 &vSrcB)
{
	i32x4 retval;
	SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
	SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
	SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
	SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);


	return retval;
}

#endif

#elif ( defined( _PS3 ) )
#define SN_IMPROVED_INTRINSICS ( (( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 1 ) && ( __GNUC_PATCHLEVEL__ == 1 )) ||\
							     (defined(__SN_VER__) && (__SN_VER__ > 25002)) )

//---------------------------------------------------------------------
// PS3 implementation
//---------------------------------------------------------------------

FORCEINLINE float FloatSIMD( fltx4 & a, int idx )
{
#if SN_IMPROVED_INTRINSICS
	return vec_extract(a,idx);
#else
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxf);
	return a_union.m128_f32[idx];
#endif
}

FORCEINLINE unsigned int UIntSIMD( u32x4 & a, int idx )
{
#if SN_IMPROVED_INTRINSICS
	return vec_extract(a,idx);
#else
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxui);
	return a_union.m128_u32[idx];
#endif
}

FORCEINLINE fltx4 AddSIMD( const fltx4 & a, const fltx4 & b )
{
	return vec_add( a, b );
}

FORCEINLINE fltx4 SubSIMD( const fltx4 & a, const fltx4 & b )				// a-b
{	
	return vec_sub( a, b );
}

FORCEINLINE fltx4 MulSIMD( const fltx4 & a, const fltx4 & b )				// a*b
{
	return vec_madd( a, b, _VEC_ZEROF );
}

FORCEINLINE fltx4 MaddSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// a*b + c
{
	return vec_madd( a, b, c );
}

FORCEINLINE fltx4 MsubSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// c - a*b
{
	return vec_nmsub( a, b, c );
};

FORCEINLINE fltx4 Dot3SIMD( const fltx4& a, const fltx4& b)
{
	// oliviern: it seems that this code could be optimized
	//  (or maybe the latency will slow down if there is nothing to put in between)
	//	Something like that (to verify on PS3 and SPU):
	//		result2 = vec_madd(a, b, _VEC_ZEROF);						// a0 * b0, a1 * b1, a2 * b2, a3 * b3
	//		result = vec_add(vec_sld(result2, result2, 4), result2);	// (a0 * b0) + (a1 * b1), (a1 * b1) + (a2 * b2), (a2 * b2) + (a3 * b3), (a3 * b3) + (a0 * b0)
	//		result = vec_add(vec_sld(result2, result2, 8), result);		// (a0 * b0) + (a1 * b1) + (a2 * b2), (a1 * b1) + (a2 * b2) + (a3 * b3), (a2 * b2) + (a3 * b3) + (a0 * b0), (a3 * b3) + (a0 * b0) + ...
	//		result = vec_splat(result, 0);								// DotProduct3...
	//		6 SIMD instructions instead of 8 (but again with potentially one more latency - it depends if other stuff can be interleaved in between).
	//		It may still be a bit faster in the worst case.

	fltx4 result;

	result = vec_madd( a, b, _VEC_ZEROF );
	result = vec_madd( vec_sld(a,a,4), vec_sld(b,b,4), result );
	result = vec_madd( vec_sld(a,a,8), vec_sld(b,b,8), result );

	// replicate across all
	result = vec_splat(result,0);

	return result;
}

FORCEINLINE fltx4 Dot4SIMD( const fltx4& a, const fltx4& b)
{
	// See comment in Dot3SIMD, we could reduce to 6 SIMD instructions instead of 7 (but again with potentially one more latency).
	//		result = vec_madd(a, b, _VEC_ZEROF);						// a0 * b0, a1 * b1, a2 * b2, a3 * b3
	//		result = vec_add(vec_sld(result, result, 4), result);		// (a0 * b0) + (a1 * b1), (a1 * b1) + (a2 * b2), (a2 * b2) + (a3 * b3), (a3 * b3) + (a0 * b0)
	//		result = vec_add(vec_sld(result, result, 8), result);		// (a0 * b0) + (a1 * b1) + (a2 * b2) + (a3 * b3), ...
	//		result = vec_splat(result, 0);								// DotProduct3...
	//		6 SIMD instructions instead of 7 (but again with potentially one more latency - it depends if other stuff can be interleaved in between).
	//		It may be a wash in the worst case.

	fltx4 result;

	result = vec_madd( a, b, _VEC_ZEROF );
	result = vec_madd( vec_sld(a,a,4), vec_sld(b,b,4), result );
	result = vec_add( vec_sld(result,result,8), result );

	// replicate across all
	result = vec_splat(result,0);

	return result;
}

FORCEINLINE fltx4 SinSIMD( const fltx4 &radians )
{
	return sinf4( radians );
}

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	sincosf4( radians, &sine, &cosine ); 	
}

FORCEINLINE void SinCosSIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )				// a*b + c
{
	sincosf4( radians, &sine, &cosine ); 	
}

FORCEINLINE fltx4 ArcCosSIMD( const fltx4 &cs )
{
	return acosf4( cs );
}

FORCEINLINE fltx4 ArcTan2SIMD( const fltx4 &a, const fltx4 &b )
{
	return atan2f4( a, b );
}

FORCEINLINE fltx4 ArcSinSIMD( const fltx4 &sine )
{
	return asinf4( sine );
}

// DivSIMD defined further down, since it uses ReciprocalSIMD

FORCEINLINE fltx4 MaxSIMD( const fltx4 & a, const fltx4 & b )				// max(a,b)
{
	return vec_max( a, b );
}
FORCEINLINE fltx4 MinSIMD( const fltx4 & a, const fltx4 & b )				// min(a,b)
{
	return vec_min( a, b );
}

FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const fltx4 & b )				// a & b
{
	return vec_and( a, b );
}
FORCEINLINE fltx4 AndSIMD( const bi32x4 & a, const fltx4 & b )				// a & b
{
	return vec_and( (fltx4)a, b );
}
FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const bi32x4 & b )				// a & b
{
	return vec_and( a, (fltx4)b );
}
FORCEINLINE bi32x4 AndSIMD( const bi32x4 & a, const bi32x4 & b )				// a & b
{
	return vec_and( a, b );
}

#if 0
FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// ~a & b
{
	// NOTE: a and b are swapped in the call: SSE complements the first argument, VMX the second
	return vec_andc( b, a);
}
FORCEINLINE fltx4 AndNotSIMD( const bi32x4 & a, const fltx4 & b )			// ~a & b
{
	// NOTE: a and b are swapped in the call: SSE complements the first argument, VMX the second
	return vec_andc( b, (fltx4)a);
}
FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const bi32x4 & b )			// ~a & b
{
	// NOTE: a and b are swapped in the call: SSE complements the first argument, VMX the second
	return (fltx4)vec_andc( b, (bi32x4)a);
}
FORCEINLINE bi32x4 AndNotSIMD( const bi32x4 & a, const bi32x4 & b )			// ~a & b
{
	// NOTE: a and b are swapped in the call: SSE complements the first argument, VMX the second
	return vec_andc( b, a);
}
#else
template< typename T, typename U >
FORCEINLINE T AndNotSIMD( const T &a, const U &b ) // ~a & b
{
	return vec_andc( b, (T)a );
}

// specialize for the case of bi, flt
FORCEINLINE fltx4 AndNotSIMD( const bi32x4 &a, const fltx4 &b ) // ~a & b
{
	return vec_andc( b, (fltx4)a );
}
#endif

FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
	return vec_xor( a, b );
}
FORCEINLINE fltx4 XorSIMD( const bi32x4 & a, const fltx4 & b )				// a ^ b
{
	return vec_xor( (fltx4)a, b );
}
FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const bi32x4 & b )				// a ^ b
{
	return vec_xor( a, (fltx4)b );
}
FORCEINLINE bi32x4 XorSIMD( const bi32x4 & a, const bi32x4 & b )				// a ^ b
{
	return vec_xor( a, b );
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
	return vec_or( a, b );
}
FORCEINLINE fltx4 OrSIMD( const bi32x4 & a, const fltx4 & b )				// a | b
{
	return vec_or( (fltx4)a, b );
}
FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const bi32x4 & b )				// a | b
{
	return vec_or( a, (fltx4)b );
}
FORCEINLINE i32x4 OrSIMD( const i32x4 & a, const i32x4 & b )				// a | b
{
	return vec_or( a, b );
}
FORCEINLINE u32x4 OrSIMD( const u32x4 & a, const u32x4 & b )				// a | b
{
	return vec_or( a, b );
}

#if !defined(__SPU__)	// bi32x4 typedef to same as u32x4 on SPU
FORCEINLINE bi32x4 OrSIMD( const bi32x4 & a, const bi32x4 & b )				// a | b
{
	return vec_or( a, b );
}
#endif

FORCEINLINE fltx4 NegSIMD(const fltx4 &a) // negate: -a
{
	return( SubSIMD( _VEC_ZEROF, a ) );

	// untested
	//	vec_float4 signMask;
	//	vec_float4 result;
	//	signMask = vec_splat_s32(-1);
	//	signMask = vec_sll(signMask, signMask);
	//	result = vec_xor(a, signMask);
	//	return result;
}

FORCEINLINE bool IsAnyZeros( const fltx4 & a )								// any floats are zero?
{
	return vec_any_eq( a, _VEC_ZEROF );
}

FORCEINLINE bool IsAnyZeros( const bi32x4 & a )								// any floats are zero?
{
	return vec_any_eq( (u32x4)a, _VEC_ZERO );
}

FORCEINLINE bool IsAllZeros( const bi32x4 & a )								// all floats of a zero?
{
	return vec_all_eq( (u32x4)a, _VEC_ZERO );
}

FORCEINLINE bool IsAnyXYZZero( const fltx4 &a )								// are any of x,y,z zero?
{
#if SN_IMPROVED_INTRINSICS

	// push 1.0 into w (NON-ZERO)
	fltx4 b = vec_insert(1.0f,a,3);

	return vec_any_eq( b, _VEC_ZEROF );
#else
	fltx4 b = vec_perm(a,_VEC_ONEF,_VEC_PERMUTE_XYZ0W1);
	return vec_any_eq( b, _VEC_ZEROF );
#endif
}

// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	return vec_all_gt( a, b );
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	return vec_all_ge(a,b);
}

FORCEINLINE bool IsAllEqual( const fltx4 &a, const fltx4 &b )
{
	return vec_all_eq(a,b);
}


FORCEINLINE int TestSignSIMD( const fltx4 & a )								// mask of which floats have the high bit set
{
	// NOTE: this maps to SSE way better than it does to VMX (most code uses IsAnyNegative(), though)
	int nRet = 0;

	fltx4_union a_union;
	vec_st(a,0,&a_union.vmxf);

	nRet |= ( a_union.m128_u32[0] & 0x80000000 ) >> 31; // sign(x) -> bit 0
	nRet |= ( a_union.m128_u32[1] & 0x80000000 ) >> 30; // sign(y) -> bit 1
	nRet |= ( a_union.m128_u32[2] & 0x80000000 ) >> 29; // sign(z) -> bit 2
	nRet |= ( a_union.m128_u32[3] & 0x80000000 ) >> 28; // sign(w) -> bit 3

	return nRet;
}
FORCEINLINE int TestSignSIMD( const bi32x4 & a )								// mask of which floats have the high bit set
{
	// NOTE: this maps to SSE way better than it does to VMX (most code uses IsAnyNegative(), though)
	int nRet = 0;

	fltx4_union a_union;
	vec_st(a,0,&a_union.vmxbi);

	nRet |= ( a_union.m128_u32[0] & 0x80000000 ) >> 31; // sign(x) -> bit 0
	nRet |= ( a_union.m128_u32[1] & 0x80000000 ) >> 30; // sign(y) -> bit 1
	nRet |= ( a_union.m128_u32[2] & 0x80000000 ) >> 29; // sign(z) -> bit 2
	nRet |= ( a_union.m128_u32[3] & 0x80000000 ) >> 28; // sign(w) -> bit 3

	return nRet;
}

FORCEINLINE bool IsAnyNegative( const bi32x4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	return (fltx4)vec_and( (u32x4)a, _VEC_CLEAR_WMASK );
}
FORCEINLINE bi32x4 SetWToZeroSIMD( const bi32x4 & a )
{
	return (bi32x4)vec_and( (u32x4)a, _VEC_CLEAR_WMASK );
}

FORCEINLINE bool IsAnyNegative( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	// NOTE: this tests the top bits of each vector element using integer math
	//       (so it ignores NaNs - it will return true for "-NaN")
	return vec_any_lt( a, _VEC_ZEROF );
}

FORCEINLINE bool IsAnyTrue( const fltx4 & a )
{
	return vec_any_ne( a, _VEC_ZEROF );
}

#ifdef DIFFERENT_NATIVE_VECTOR_TYPES

FORCEINLINE bool IsAnyTrue( const bi32x4 & a )
{
	return vec_any_ne( (vector unsigned int) a, _VEC_0L );
}

#endif

FORCEINLINE bi32x4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
	return (bi32x4)vec_cmpeq( a, b );
}
FORCEINLINE bi32x4 CmpEqSIMD( const i32x4 & a, const i32x4 & b )				// (a==b) ? ~0:0
{
	return (bi32x4)vec_cmpeq( a, b );
}
FORCEINLINE bi32x4 CmpEqSIMD( const u32x4 & a, const u32x4 & b )				// (a==b) ? ~0:0
{
	return (bi32x4)vec_cmpeq( a, b );
}

FORCEINLINE bi32x4 CmpGtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? ~0:0
{
	return (bi32x4)vec_cmpgt( a, b );
}
FORCEINLINE bi32x4 CmpGtSIMD( const i32x4 & a, const i32x4 & b )				// (a>b) ? ~0:0
{
	return (bi32x4)vec_cmpgt( a, b );
}
FORCEINLINE bi32x4 CmpGtSIMD( const u32x4 & a, const u32x4 & b )				// (a>b) ? ~0:0
{
	return (bi32x4)vec_cmpgt( a, b );
}

FORCEINLINE bi32x4 CmpGeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? ~0:0
{
	return (bi32x4)vec_cmpge( a, b );
}


FORCEINLINE bi32x4 CmpLtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? ~0:0
{
	return (bi32x4)vec_cmplt( a, b );
}

FORCEINLINE bi32x4 CmpLeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? ~0:0
{
	return (bi32x4)vec_cmple( a, b );
}



FORCEINLINE bi32x4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? ~0 : 0
{
	i32x4 control;
	control = vec_cmpb(a,b);
	return (bi32x4)vec_cmpeq( (u32x4)control, _VEC_ZERO );
}

FORCEINLINE int CmpAnyLeSIMD( const fltx4 & a, const fltx4 & b )				
{
	return vec_any_le( a, b );
}

FORCEINLINE int CmpAnyGeSIMD( const fltx4 & a, const fltx4 & b )				
{
	return vec_any_ge( a, b );
}

FORCEINLINE int CmpAnyLtSIMD( const fltx4 & a, const fltx4 & b )				
{
	return vec_any_lt( a, b );
}
FORCEINLINE int CmpAnyLtSIMD( const bi32x4 & a, const i32x4 & b )				
{
	return vec_any_lt( (i32x4)a, b );
}

FORCEINLINE int CmpAnyGtSIMD( const fltx4 & a, const fltx4 & b )				
{
	return vec_any_gt( a, b );
}

FORCEINLINE int CmpAnyNeSIMD( const fltx4 & a, const fltx4 & b )				
{
	return vec_any_ne( a, b );
}
FORCEINLINE int CmpAnyNeSIMD( const bi32x4 & a, const bi32x4 & b )				
{
	return vec_any_ne( a, b );
}
FORCEINLINE int CmpAnyNeSIMD( const bi32x4 & a, const i32x4 & b )				
{
	return vec_any_ne( a, (bi32x4)b );
}

FORCEINLINE int CmpAllLeSIMD( const fltx4 & a, const fltx4 & b )				
{
	return vec_all_le( a, b );
}

FORCEINLINE fltx4 MaskedAssign( const bi32x4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
	return vec_sel( OldValue, NewValue, ReplacementMask );
}

FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
	return vec_sel( OldValue, NewValue, (const bi32x4) ReplacementMask );
}

FORCEINLINE vector signed short MaskedAssign( const vector unsigned short & ReplacementMask, const vector signed short & NewValue, const vector signed short & OldValue )
{
	return vec_sel( OldValue, NewValue, ReplacementMask );
}

// AKA "Broadcast", "Splat"
FORCEINLINE fltx4 ReplicateX4( float flValue )					//  a,a,a,a
{
#if SN_IMPROVED_INTRINSICS
	return vec_splats(flValue);
#else
	// NOTE: if flValue comes from a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
	float * pValue = &flValue;
	Assert( pValue );
	Assert( ((unsigned int)pValue & 3) == 0);

	fltx4 result;

	result = vec_ld(0, pValue);
	result = vec_splat( vec_perm( result, result, vec_lvsl(0, pValue) ), 0 );

	return result;
#endif
}

FORCEINLINE fltx4 ReplicateX4( const float *pValue )					//  a,a,a,a
{
#if SN_IMPROVED_INTRINSICS
	return vec_splats(*pValue);
#else
	Assert( pValue );
	fltx4 result;

	result = vec_ld(0, pValue);
	result = vec_splat( vec_perm( result, result, vec_lvsl(0, pValue) ), 0 );

	return result;
#endif
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE i32x4 ReplicateIX4( int nValue )
{
#if SN_IMPROVED_INTRINSICS
	return vec_splats(nValue);
#else
	// NOTE: if nValue comes from a register, this causes a Load-Hit-Store stall (should not mix ints with fltx4s!)
	int * pValue = &nValue;
	Assert( pValue );
	Assert( ((unsigned int)pValue & 3) == 0);
	i32x4 result;

	result = vec_ld(0, pValue);
	result = vec_splat( vec_perm( result, result, vec_lvsl(0, pValue) ), 0 );

	return result;
#endif
}

FORCEINLINE fltx4 SqrtSIMD( const fltx4 & a )					// sqrt(a)
{
	return sqrtf4(a);
}

FORCEINLINE fltx4 SqrtEstSIMD( const fltx4 & a )				// sqrt(a), more or less
{
#if defined( _PS3 ) && !defined( SPU )
	// This is exactly what the Xbox 360 does in XMVectorSqrtEst
	fltx4 vRecipSquareRoot = vec_rsqrte( a );
	i32x4 vOne = vec_splat_s32( 1 );
	i32x4 vAllOnes = vec_splat_s32( -1 );
	i32x4 vShiftLeft24 = vec_splat_s32( -8 ); // -8 is the same bit pattern as 24 with a 5-bit mask
	fltx4 vZero = (fltx4)vec_splat_s32( 0 );
	u32x4 vInputShifted = vec_sl( (u32x4)a, (u32x4)vOne );
	u32x4 vInfinityShifted = vec_sl( (u32x4)vAllOnes, (u32x4)vShiftLeft24 );
	bi32x4 vEqualsZero = vec_vcmpeqfp( a, vZero );
	bi32x4 vEqualsInfinity = vec_vcmpequw( vInputShifted, vInfinityShifted );
	fltx4 vSquareRoot = vec_madd( a, vRecipSquareRoot, _VEC_ZEROF );
	bi32x4 vResultMask = vec_vcmpequw( (u32x4)vEqualsInfinity, (u32x4)vEqualsZero ); // mask has 1s wherever the square root is valid
	fltx4 vCorrectedSquareRoot = vec_sel( a, vSquareRoot, vResultMask );

	return vCorrectedSquareRoot; 
#else
	return SqrtSIMD( a );
#endif
}

FORCEINLINE fltx4 ReciprocalSqrtEstSIMD( const fltx4 & a )		// 1/sqrt(a), more or less
{
	return vec_rsqrte( a );
}

FORCEINLINE fltx4 ReciprocalSqrtSIMD( const fltx4 & a )			// 1/sqrt(a)
{
	// This matches standard library function rsqrtf4
	fltx4 result;
	vmathV4RsqrtPerElem( (VmathVector4 *)&result, (const VmathVector4 *)&a );

	return result;
}

FORCEINLINE fltx4 ReciprocalEstSIMD( const fltx4 & a )			// 1/a, more or less
{
	return vec_re( a );
}

/// 1/x for all 4 values, more or less
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalEstSaturateSIMD( const fltx4 & a )
{
	bi32x4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalEstSIMD( ret );
	return ret;
}
 

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton iteration.
/// No error checking!
FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )				// 1/a
{
	// This matches standard library function recipf4
	fltx4 result;
	vmathV4RecipPerElem	( (VmathVector4 *)&result, (const VmathVector4 *)&a );

	return result;
}

FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )	// a/b
{
	return MulSIMD( ReciprocalSIMD( b ), a );
}

FORCEINLINE fltx4 DivEstSIMD( const fltx4 & a, const fltx4 & b )	// Est(a/b)
{
	return MulSIMD( ReciprocalEstSIMD( b ), a );
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalSaturateSIMD( const fltx4 & a )
{
	// Convert zeros to epsilons
	bi32x4 zero_mask = CmpEqSIMD( a, _VEC_ZEROF );
	fltx4 a_safe = OrSIMD( a, AndSIMD( _VEC_EPSILONF, zero_mask ) );
	return ReciprocalSIMD( a_safe );

	// FIXME: This could be faster (BUT: it doesn't preserve the sign of -0.0, whereas the above does)
	// fltx4 zeroMask = CmpEqSIMD( gFour_Zeros, a );
	// fltx4 a_safe = XMVectorSelect( a, gFour_Epsilons, zeroMask );
	// return ReciprocalSIMD( a_safe );
}


// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
FORCEINLINE fltx4 ExpSIMD( const fltx4 &toPower )
{
	return exp2f4(toPower);
}

// a unique Altivec concept, the "Vector 2 Raised to the Exponent Estimate Floating Point",
// which is accurate to four bits of mantissa.
FORCEINLINE fltx4 Exp2EstSIMD( const fltx4 &f )
{
	return exp2f4fast( f );
}


// Clamps the components of a vector to a specified minimum and maximum range.
FORCEINLINE fltx4 ClampVectorSIMD( FLTX4 in, FLTX4 min, FLTX4 max)
{
	fltx4 result = vec_max(min, in);
	return vec_min(max, result);
}


FORCEINLINE fltx4 LoadUnalignedSIMD( const void *pSIMD )
{
#if SN_IMPROVED_INTRINSICS

	fltx4 v0, v1;

	Assert( pSIMD );


	v0 = (fltx4)vec_lvlx( 0, (float*)pSIMD );
	v1 = (fltx4)vec_lvrx( 16, (float*)pSIMD );
	return vec_or(v0, v1);

#else

	fltx4 v0, v1;
	vector unsigned char permMask;

	Assert( pSIMD );

	v0	 = vec_ld( 0, pSIMD );			
	permMask = vec_lvsl( 0, pSIMD );	
	v1	 = vec_ld( 15, pSIMD );			

	return vec_perm(v0, v1, permMask);  

#endif
}

FORCEINLINE fltx4 LoadUnsignedByte4SIMD( unsigned char *pBytes )	// unpack contiguous 4 bytes into vec float 4
{

#if SN_IMPROVED_INTRINSICS

	__vector unsigned char  res_uc;
	__vector unsigned short res_us;

	__vector unsigned char vZero8 = (__vector unsigned char)vec_splat_u8(0);
	__vector unsigned short vZero16 = (__vector unsigned short)vec_splat_u16(0);

	res_uc = (__vector unsigned char)vec_lvlx(0, pBytes);
	res_uc = vec_mergeh( vZero8, res_uc );
	res_us = vec_mergeh( vZero16, (__vector unsigned short)res_uc );
	return vec_ctf( (__vector unsigned int)res_us, 0);

#else

	vector unsigned char v0, v1;
	vector bool char res_uc;
	vector unsigned char permMask;
	vector bool short res_us;

	vector bool char vZero8 = (vector bool char)vec_splat_u8(0);
	vector bool short vZero16 = (vector bool short)vec_splat_u16(0);

	v0 = vec_ld(0, pBytes);
	permMask = vec_lvsl(0, pBytes);
	v1 = vec_ld(3, pBytes);
	res_uc = (vector bool char)vec_perm(v0, v1, permMask);
	res_uc = vec_mergeh( vZero8, res_uc );
	res_us = vec_mergeh( vZero16, (vector bool short)res_uc );
	return vec_ctf( (vector unsigned int)res_us, 0);

#endif

}

FORCEINLINE fltx4 LoadSignedByte4SIMD( signed char *pBytes )	// unpack contiguous 4 bytes into vec float 4
{

#if SN_IMPROVED_INTRINSICS	

	vector signed char  res_uc;
	vector signed short res_us;
	vector signed int   res_ui;

	res_uc = (vector signed char)vec_lvlx(0, pBytes);
	res_us = vec_unpackh( res_uc );
	res_ui = vec_unpackh( res_us );
	return vec_ctf( res_ui, 0);

#else

	vector signed char v0, v1, res_uc;
	vector unsigned char permMask;
	vector signed short res_us;
	vector signed int   res_ui;

	v0 = vec_ld(0, pBytes);
	permMask = vec_lvsl(0, pBytes);
	v1 = vec_ld(3, pBytes);
	res_uc = vec_perm(v0, v1, permMask);
	res_us = vec_unpackh( res_uc );
	res_ui = vec_unpackh( res_us );
	return vec_ctf( res_ui, 0);

#endif

}


// load a 3-vector (as opposed to LoadUnalignedSIMD, which loads a 4-vec). 
FORCEINLINE fltx4 LoadUnaligned3SIMD( const void *pSIMD )
{
	Assert( pSIMD );

	fltx4 v0 = vec_ld( 0, ( float  * )( pSIMD ) );			
	vector unsigned char permMask = vec_lvsl( 0, ( float * ) ( pSIMD ) );	
	fltx4 v1 = vec_ld( 11, ( float * )( pSIMD ) );			

	return vec_perm( v0, v1, permMask );  
}


// load a single unaligned float into the x component of a SIMD word
FORCEINLINE fltx4 LoadUnalignedFloatSIMD( const float *pFlt )
{
	fltx4 v0 = vec_lde( 0, const_cast<float *>(pFlt) );
	vector unsigned char permMask = vec_lvsl( 0, const_cast<float *>(pFlt) );
	return vec_perm( v0, v0, permMask );
}


FORCEINLINE fltx4 LoadAlignedSIMD( const void *pSIMD )
{
	return vec_ld( 0, ( float * )pSIMD );
}

#ifndef SPU
// No reason to support VectorAligned on SPU.

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned &pSIMD )
{
	fltx4 out;
	out = vec_ld( 0, pSIMD.Base() );

	// squelch w
	return (fltx4)vec_and( (u32x4)out, _VEC_CLEAR_WMASK );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned * RESTRICT pSIMD )
{
	fltx4 out;
	out = vec_ld( 0, pSIMD->Base() );

	// squelch w
	return (fltx4)vec_and( (u32x4)out, _VEC_CLEAR_WMASK );
}


// strongly typed -- for typechecking as we transition to SIMD
FORCEINLINE void StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, const fltx4 & a )
{
	vec_st(a, 0, pSIMD->Base());
}
#endif

FORCEINLINE void StoreAlignedSIMD( float *pSIMD, const fltx4 & a )
{
	vec_st(a, 0, pSIMD);
}

FORCEINLINE void StoreUnalignedSIMD( float *pSIMD, const fltx4 & a )
{
#if ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 1 ) && ( __GNUC_PATCHLEVEL__ == 1 )
	vec_stvlx( a, 0, pSIMD);
	vec_stvrx( a, 16, pSIMD);
#else
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxf);
	pSIMD[0] = a_union.m128_f32[0];
	pSIMD[1] = a_union.m128_f32[1];
	pSIMD[2] = a_union.m128_f32[2];
	pSIMD[3] = a_union.m128_f32[3];
#endif

}

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxf);
	pSIMD[0] = a_union.m128_f32[0];
	pSIMD[1] = a_union.m128_f32[1];
	pSIMD[2] = a_union.m128_f32[2];
};



#ifndef SPU
// No reason to support unaligned Vectors on SPU


FORCEINLINE fltx4 Compress4SIMD( fltx4 const a, fltx4 const &b, fltx4 const &c, fltx4 const &d );
// construct a fltx4 from four different scalars, which are assumed to be neither aligned nor contiguous
FORCEINLINE fltx4 LoadGatherSIMD( const float &x, const float &y, const float &z, const float &w )
{
#if USING_POINTLESSLY_SLOW_SONY_CODE
	return vmathV4MakeFromElems_V( x,y,z,w ).vec128;
#else
	// load the float into the low word of each vector register (this exploits the unaligned load op)
	fltx4 vx = vec_lvlx( 0, &x );
	fltx4 vy = vec_lvlx( 0, &y );
	fltx4 vz = vec_lvlx( 0, &z );
	fltx4 vw = vec_lvlx( 0, &w );
	return Compress4SIMD( vx, vy, vz, vw );
#endif
}


// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination[0],  pDestination[1],  pDestination[2],  pDestination[3]
// The Vectors are assumed to be unaligned.
FORCEINLINE void StoreFourUnalignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											   Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination ,  pDestination + 1,  pDestination + 2,  pDestination + 3
// The Vectors are assumed to start on an ALIGNED address, that is, 
// pDestination is 16-byte aligned (thhough obviously pDestination+1 is not).
FORCEINLINE void StoreFourAlignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											 Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}
#endif

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing 
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way 
// to guarantee an immediate as a parameter to function
// like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
	i32x4 asInt = vec_cts( vSrc, 0 );
	vec_st(asInt, 0, pDest->Base());
}

FORCEINLINE void TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w )
{
	fltx4 p0, p1, p2, p3;

	p0 = vec_mergeh(x, z);
	p1 = vec_mergeh(y, w);
	p2 = vec_mergel(x, z);
	p3 = vec_mergel(y, w);

	x = vec_mergeh(p0, p1);
	y = vec_mergel(p0, p1);
	z = vec_mergeh(p2, p3);
	w = vec_mergel(p2, p3);
}

// Return one in the fastest way -- faster even than loading.
FORCEINLINE fltx4 LoadZeroSIMD( void )
{
	return _VEC_ZEROF;
}
FORCEINLINE i32x4 LoadZeroISIMD( void )
{
	return vec_splat_s32(0);
}


// Return one in the fastest way -- faster even than loading.
FORCEINLINE fltx4 LoadOneSIMD( void )
{
	return _VEC_ONEF;
}
FORCEINLINE i32x4 LoadOneISIMD( void )
{
	return vec_splat_s32(1);
}

FORCEINLINE fltx4 SplatXSIMD( fltx4 a )
{
	return vec_splat(a,0);
}
FORCEINLINE fltx4 SplatYSIMD( fltx4 a )
{
	return vec_splat(a,1);
}
FORCEINLINE fltx4 SplatZSIMD( fltx4 a )
{
	return vec_splat(a,2);
}
FORCEINLINE fltx4 SplatWSIMD( fltx4 a )
{
	return vec_splat(a,3);
}

FORCEINLINE bi32x4 SplatXSIMD( bi32x4 a )
{
	return vec_splat(a,0);
}
FORCEINLINE bi32x4 SplatYSIMD( bi32x4 a )
{
	return vec_splat(a,1);
}
FORCEINLINE bi32x4 SplatZSIMD( bi32x4 a )
{
	return vec_splat(a,2);
}
FORCEINLINE bi32x4 SplatWSIMD( bi32x4 a )
{
	return vec_splat(a,3);
}

FORCEINLINE fltx4 SetXSIMD( const fltx4& a, const fltx4& x )
{
	return vec_sel(a,x, _VEC_COMPONENT_MASK_0);
}

FORCEINLINE fltx4 SetYSIMD( const fltx4& a, const fltx4& y )
{
	return vec_sel(a,y, _VEC_COMPONENT_MASK_1);
}

FORCEINLINE fltx4 SetZSIMD( const fltx4& a, const fltx4& z )
{
	return vec_sel(a,z, _VEC_COMPONENT_MASK_2);
}

FORCEINLINE fltx4 SetWSIMD( const fltx4& a, const fltx4& w )
{
	return vec_sel(a,w, _VEC_COMPONENT_MASK_3);
}

FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, int nComponent, float flValue )
{
#if SN_IMPROVED_INTRINSICS
	return vec_insert( flValue, a, nComponent );
#else
	fltx4_union a_union;
	a_union.vmxf = vec_ld(0,&a);
	a_union.m128_f32[nComponent] = flValue;
	return a_union.vmxf;
#endif
}

FORCEINLINE float GetComponentSIMD( const fltx4& a, int nComponent )
{
#if SN_IMPROVED_INTRINSICS
	return vec_extract( a, nComponent );
#else
	fltx4_union a_union;
	a_union.vmxf = vec_ld(0,&a);
	return a_union.m128_f32[nComponent];
#endif
}


FORCEINLINE fltx4 RotateLeft( const fltx4 & a )
{
	return vec_sld(a,a,4);
}

FORCEINLINE fltx4 RotateLeft2( const fltx4 & a )
{
	return vec_sld(a,a,8);
}

FORCEINLINE fltx4 RotateRight( const fltx4 & a )
{
	return vec_sld(a,a,12);
}

FORCEINLINE fltx4 RotateRight2( const fltx4 & a )
{
	return vec_sld(a,a,8);
}

// rotate a vector left by an arbitrary number of 
// bits known at compile time. The bit parameter
// is template because it's actually used as an 
// immediate field in an instruction, eg it absolutely
// must be known at compile time. nBits>127 leads
// to doom. 
// zeroes are shifted in from the right
template < uint nBits, typename T > 
FORCEINLINE T ShiftLeftByBits(const T &a)
{
	// hopefully the compiler, seeing nBits as a const immediate, elides these ifs
	if ( nBits >= 128 ) // WTF are you doing?!
	{
		return (T) LoadZeroSIMD();
	}
	else if ( nBits == 0 )
	{
		return a;
	}
	else if ( (nBits >  7) ) // if we have to rotate by at least one byte, do the by-octet rotation first
	{
		T t = vec_sld( a, ((T)LoadZeroSIMD()), (nBits >> 3) ); // rotated left by octets
		return ShiftLeftByBits< (nBits & 0x7) >( t );
	}
	else // we need to rotate by <= 7 bits 
	{
		// on AltiVec there's no immediate shift left by bits; we need to splat the bits onto a vector and runtime shift.
		// the splat, however, does require an immediate. Go IBM!
		vector unsigned int shifter = (vector unsigned int) (vec_splat_s8( ((signed char)(nBits & 0x7)) ));
		return (T) vec_sll( (vector signed int) a, shifter );
	}
}

// as above, but shift right
template < uint nBits, typename T > 
FORCEINLINE T ShiftRightByBits(const T &a)
{
	// hopefully the compiler, seeing nBits as a const immediate, elides these ifs
	if ( nBits >= 128 ) // WTF are you doing?!
	{
		return (T) LoadZeroSIMD();
	}
	else if ( nBits == 0 )
	{
		return a;
	}
	else if ( (nBits >  7) ) // if we have to rotate by at least one byte, do the by-octet rotation first
	{
		T t = vec_sld( ((T)LoadZeroSIMD()), a, 16 - (nBits >> 3) ); // rotated right by octets -- a rotate right of one is like a rotate left of fifteen. 
		return ShiftRightByBits< (nBits & 0x7) >( t );
	}
	else // we need to rotate by <= 7 bits 
	{
		// on AltiVec there's no immediate shift left by bits; we need to splat the bits onto a vector and runtime shift.
		// the splat, however, does require an immediate. Go IBM!
		vector unsigned int shifter = (vector unsigned int) (vec_splat_s8( ((signed char)(nBits & 0x7)) ));
		return (T) vec_srl( (vector unsigned int) a, shifter );
	}
}


/**** an example of ShiftLeftByBits:
fltx4 ShiftByTwentyOne( fltx4 foo )
{
	return ShiftLeftByBits<21>(foo);
}

compiles to:

	ShiftByTwentyOne(float __vector):
	0x000059FC: 0x1060038C vspltisw v3,0                  PIPE
	0x00005A00: 0x1085030C vspltisb v4,5
	0x00005A04: 0x104218AC vsldoi   v2,v2,v3,2            02 (000059FC) REG PIPE
	0x00005A08: 0x104221C4 vsl      v2,v2,v4              03 (00005A04) REG
	0x00005A0C: 0x4E800020 blr   
*****/



// find the lowest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
// ignores a.w.
// Forcing this inline should hopefully help with scheduling.
FORCEINLINE fltx4 FindLowestSIMD3( const fltx4 & a )
{
	fltx4 result;
	fltx4 x = vec_splat( a, 0 );
	fltx4 y = vec_splat( a, 1 );
	fltx4 z = vec_splat( a, 2 );

	if ( vec_any_nan( a ) )
	{
		x = vec_all_nan( x ) ? _VEC_FLTMAX : x;
		y = vec_all_nan( y ) ? _VEC_FLTMAX : y;
		z = vec_all_nan( z ) ? _VEC_FLTMAX : z;
	}

	result = vec_min( y, x );
	result = vec_min( z, result );

	return result;

}

// find the highest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
// ignores a.w.
// Though this is only five instructions long,
// they are all dependent, making this stall city.
// Forcing this inline should hopefully help with scheduling.
FORCEINLINE fltx4 FindHighestSIMD3( const fltx4 & a )
{
	fltx4 result;
	fltx4 x = vec_splat( a, 0 );
	fltx4 y = vec_splat( a, 1 );
	fltx4 z = vec_splat( a, 2 );

	if ( vec_any_nan( a ) )
	{
		x = vec_all_nan( x ) ? _VEC_FLTMIN : x;
		y = vec_all_nan( y ) ? _VEC_FLTMIN : y;
		z = vec_all_nan( z ) ? _VEC_FLTMIN : z;
	}

	result = vec_max( y, x );
	result = vec_max( z, result );

	return result;
}


// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------

// Load 4 aligned words into a SIMD register
FORCEINLINE i32x4 LoadAlignedIntSIMD(const int32 * RESTRICT pSIMD)
{
	return vec_ld(0, const_cast<int32 *>(pSIMD));
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD(const int32 * RESTRICT pSIMD)
{
	i32x4 v0, v1;
	vector unsigned char permMask;

	Assert( pSIMD );

	v0	 = vec_ld( 0, const_cast<int32 *>(pSIMD) );			
	permMask = vec_lvsl( 0, const_cast<int32 *>(pSIMD) );	
	v1	 = vec_ld( 15, const_cast<int32 *>(pSIMD) );			

	return vec_perm(v0, v1, permMask);  

}

// save into four words, 16-byte aligned
FORCEINLINE void StoreAlignedIntSIMD( int32 *pSIMD, const i32x4 & a )
{
	vec_st(a,0,pSIMD);
}

FORCEINLINE void StoreAlignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	vec_st((i32x4)a,0,pSIMD);
}

FORCEINLINE void StoreAlignedIntSIMD( intx4 &pSIMD, const i32x4 & a )
{
	vec_st(a,0,pSIMD.Base());
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 *pSIMD, const i32x4 & a )
{
#if SN_IMPROVED_INTRINSICS

	// NOTE : NOT TESTED
	vec_stvlx(a,0,pSIMD);
	vec_stvrx(a,16,pSIMD);

#else

	fltx4_union tmp;
	vec_st(a,0,&tmp.vmxi);

	pSIMD[0] = tmp.m128_u32[0];
	pSIMD[1] = tmp.m128_u32[1];
	pSIMD[2] = tmp.m128_u32[2];
	pSIMD[3] = tmp.m128_u32[3];

#endif
}

// a={ a.x, a.z, b.x, b.z }
// combine two fltx4s by throwing away every other field.
FORCEINLINE fltx4 CompressSIMD( fltx4 const & a, fltx4 const &b )
{
	const int32 ALIGN16 n4shuffleACXZ[4] ALIGN16_POST = { 0x00010203, 0x08090A0B, 0x10111213, 0x18191A1B };
	return vec_perm( a, b, (vec_uchar16)LoadAlignedIntSIMD( n4shuffleACXZ ) );
}

// a={ a.x, b.x, c.x, d.x }
// combine 4 fltx4s by throwing away 3/4s of the fields
// TODO: make more efficient by doing this in a parallel way at the caller
//    Compress4SIMD(FourVectors.. )
FORCEINLINE fltx4 Compress4SIMD( fltx4 const a, fltx4 const &b, fltx4 const &c, fltx4 const &d )
{
	fltx4 ab = vec_mergeh( a, b );  // a.x, b.x, a.y, b.y
	fltx4 cd = vec_mergeh( c, d );  // c.x, d.x...
	static const int32 ALIGN16 shuffleABXY[4] ALIGN16_POST = { 0x00010203, 0x04050607, 0x10111213, 0x14151617 };

	return vec_perm( ab, cd, (vec_uchar16)LoadAlignedIntSIMD( shuffleABXY ) );
}


// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. No
// fixed point conversion is done.
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	return vec_ctf(vSrcA,0);
}


// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	return vec_ctf(vSrcA,0);
}

// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. Each uint
// will be divided by 2^immed after conversion
// (eg, this is fixed point math). 
/* as if:
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const i32x4 &vSrcA, unsigned int uImmed )
{
return vec_ctf(vSrcA,uImmed);
}
*/
#define UnsignedFixedIntConvertToFltSIMD(vSrcA, uImmed) (vec_ctf( (vSrcA), (uImmed) ))

// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. Each int
// will be divided by 2^immed (eg, this is fixed point
// math). 
/* as if:
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA, unsigned int uImmed )
{
return vec_ctf(vSrcA,uImmed);
}
*/
#define SignedFixedIntConvertToFltSIMD(vSrcA, uImmed) (vec_ctf( (vSrcA), (uImmed) ))

// set all components of a vector to a signed immediate int number.
/* as if:
FORCEINLINE fltx4 IntSetImmediateSIMD(int toImmediate)
{
return vec_splat_s32( toImmediate );
}
*/
#define IntSetImmediateSIMD(x) (vec_splat_s32(x))


/*
works on fltx4's as if they are four uints.
the first parameter contains the words to be shifted,
the second contains the amount to shift by AS INTS

for i = 0 to 3
shift = vSrcB_i*32:(i*32)+4
vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
FORCEINLINE u32x4 IntShiftLeftWordSIMD(u32x4 vSrcA, u32x4 vSrcB)
{
	return vec_sl(vSrcA, vSrcB);
}


FORCEINLINE float SubFloat( const fltx4 & a, int idx )
{
#if ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 1 ) && ( __GNUC_PATCHLEVEL__ == 1 )
	return( vec_extract( a, idx ) );
#else // GCC 4.1.1
	// NOTE: if the output goes into a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxf);
	return a_union.m128_f32[idx];
#endif // GCC 4.1.1
}

FORCEINLINE float & SubFloat( fltx4 & a, int idx )
{
	fltx4_union & a_union = (fltx4_union &)a;
	return a_union.m128_f32[idx];
}

FORCEINLINE uint32 SubInt( const u32x4 & a, int idx )
{
#if ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 1 ) && ( __GNUC_PATCHLEVEL__ == 1 )
	return( vec_extract( a, idx ) );
#else // GCC 4.1.1
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxui);
	return a_union.m128_u32[idx];
#endif // GCC 4.1.1
}

FORCEINLINE uint32 SubInt( const fltx4 & a, int idx )
{
#if ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 1 ) && ( __GNUC_PATCHLEVEL__ == 1 )
	return( vec_extract( (u32x4)a, idx ) );
#else
	fltx4_union a_union;
	vec_st(a, 0, &a_union.vmxf);
	return a_union.m128_u32[idx];
#endif
}

FORCEINLINE uint32 & SubInt( u32x4 & a, int idx )
{
	fltx4_union & a_union = (fltx4_union &)a;
	return a_union.m128_u32[idx];
}

FORCEINLINE uint32 SubFloatConvertToInt( const fltx4 & a, int idx )
{

#if ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 1 ) && ( __GNUC_PATCHLEVEL__ == 1 )
	return( vec_extract( vec_ctu( a, 0 ), idx ) );
#else
	u32x4 t = vec_ctu( a, 0 );
	return SubInt(t,idx);
#endif

}

// perform an Altivec permute op. There is no corresponding SSE op, so 
// this function is missing from that fork. This is deliberate, because
// permute-based algorithms simply need to be abandoned and rebuilt 
// differently way for SSE. 
// (see http://developer.apple.com/hardwaredrivers/ve/sse.html#Translation_Perm )
template< typename T, typename U >
FORCEINLINE T PermuteVMX( T a, T b, U swizzleMask )
{
	return vec_perm( a, b, (vec_uchar16) swizzleMask );
}


// __fsel(double fComparand, double fValGE, double fLT) == fComparand >= 0 ? fValGE : fLT
// this is much faster than if ( aFloat > 0 ) { x = .. }
#if !defined(__SPU__)
#define fsel __fsel
#endif

inline bool IsVector3LessThan(const fltx4 &v1, const fltx4 &v2 )
{
	return vec_any_lt( v1, v2 );
}

inline bool IsVector3GreaterOrEqual(const fltx4 &v1, const fltx4 &v2 )
{
	return !IsVector3LessThan( v1, v2 );
}

FORCEINLINE fltx4 ReciprocalSqrtEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) != 0.0f ? SubFloat( a, 0 ) : FLT_EPSILON );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) != 0.0f ? SubFloat( a, 1 ) : FLT_EPSILON );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) != 0.0f ? SubFloat( a, 2 ) : FLT_EPSILON );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) != 0.0f ? SubFloat( a, 3 ) : FLT_EPSILON );
	return retVal;
}

// Round towards negative infinity
FORCEINLINE fltx4 FloorSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = floor( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = floor( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = floor( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = floor( SubFloat( a, 3 ) );
	return retVal;
}

#elif ( defined( _X360 ) )

//---------------------------------------------------------------------
// X360 implementation
//---------------------------------------------------------------------

inline bool IsVector3LessThan(const fltx4 &v1, const fltx4 &v2 )
{
	return !XMVector3GreaterOrEqual( v1, v2 );
}

inline BOOL IsVector3GreaterOrEqual(const fltx4 &v1, const fltx4 &v2 )
{
	return XMVector3GreaterOrEqual( v1, v2 );
}


FORCEINLINE float & FloatSIMD( fltx4 & a, int idx )
{
	fltx4_union & a_union = (fltx4_union &)a;
	return a_union.m128_f32[idx];
}

FORCEINLINE unsigned int & UIntSIMD( fltx4 & a, int idx )
{
	fltx4_union & a_union = (fltx4_union &)a;
	return a_union.m128_u32[idx];
}

FORCEINLINE fltx4 AddSIMD( const fltx4 & a, const fltx4 & b )
{
	return __vaddfp( a, b );
}

FORCEINLINE fltx4 SubSIMD( const fltx4 & a, const fltx4 & b )				// a-b
{
	return __vsubfp( a, b );
}

FORCEINLINE fltx4 MulSIMD( const fltx4 & a, const fltx4 & b )				// a*b
{
	return __vmulfp( a, b );
}

FORCEINLINE fltx4 MaddSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// a*b + c
{
	return __vmaddfp( a, b, c );
}

FORCEINLINE fltx4 MsubSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// c - a*b
{
	return __vnmsubfp( a, b, c );
};

FORCEINLINE fltx4 Dot3SIMD( const fltx4 &a, const fltx4 &b )
{
	return __vmsum3fp( a, b );
}

FORCEINLINE fltx4 Dot4SIMD( const fltx4 &a, const fltx4 &b )
{
	return __vmsum4fp( a, b );
}

FORCEINLINE fltx4 SinSIMD( const fltx4 &radians )
{
	return XMVectorSin( radians );
}

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	XMVectorSinCos( &sine, &cosine, radians ); 	
}

FORCEINLINE void SinCosSIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )			
{
	XMVectorSinCos( &sine, &cosine, radians ); 	
}

FORCEINLINE void CosSIMD( fltx4 &cosine, const fltx4 &radians )				
{
	cosine = XMVectorCos( radians ); 	
}

FORCEINLINE fltx4 ArcSinSIMD( const fltx4 &sine )
{
	return XMVectorASin( sine );
}

FORCEINLINE fltx4 ArcCosSIMD( const fltx4 &cs )
{
	return XMVectorACos( cs );
}

// tan^1(a/b) .. ie, pass sin in as a and cos in as b
FORCEINLINE fltx4 ArcTan2SIMD( const fltx4 &a, const fltx4 &b )
{
	return XMVectorATan2( a, b );
}

// DivSIMD defined further down, since it uses ReciprocalSIMD

FORCEINLINE fltx4 MaxSIMD( const fltx4 & a, const fltx4 & b )				// max(a,b)
{
	return __vmaxfp( a, b );
}

FORCEINLINE fltx4 MinSIMD( const fltx4 & a, const fltx4 & b )				// min(a,b)
{
	return __vminfp( a, b );
}

FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const fltx4 & b )				// a & b
{
    return __vand( a, b );
}

FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// ~a & b
{
	// NOTE: a and b are swapped in the call: SSE complements the first argument, VMX the second
    return __vandc( b, a );
}

FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
    return __vxor( a, b );
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
    return __vor( a, b );
}

FORCEINLINE fltx4 NegSIMD(const fltx4 &a) // negate: -a
{
	return XMVectorNegate(a);
}

FORCEINLINE bool IsAllZeros( const fltx4 & a )								// all floats of a zero?
{
	unsigned int equalFlags = 0;
    __vcmpeqfpR( a, Four_Zeros, &equalFlags );
    return XMComparisonAllTrue( equalFlags );
}

FORCEINLINE bool IsAnyZeros( const fltx4 & a )								// any floats are zero?
{
	unsigned int conditionregister;
	XMVectorEqualR(&conditionregister, a, XMVectorZero());
	return XMComparisonAnyTrue(conditionregister);
}

FORCEINLINE bool IsAnyXYZZero( const fltx4 &a )								// are any of x,y,z zero?
{
	// copy a's x component into w, in case w was zero.
	fltx4 temp = __vrlimi(a, a, 1, 1);
	unsigned int conditionregister;
	XMVectorEqualR(&conditionregister, temp, XMVectorZero());
	return XMComparisonAnyTrue(conditionregister);
}

// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterR(&cr,a,b);
	return XMComparisonAllTrue(cr);
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterOrEqualR(&cr,a,b);
	return XMComparisonAllTrue(cr);
}

// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAnyGreaterThan( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterR(&cr,a,b);
	return XMComparisonAnyTrue(cr);
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAnyGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterOrEqualR(&cr,a,b);
	return XMComparisonAnyTrue(cr);
}

// For branching if all a.xyzw == b.xyzw
FORCEINLINE bool IsAllEqual( const fltx4 & a, const fltx4 & b )
{
	unsigned int cr;
	XMVectorEqualR(&cr,a,b);
	return XMComparisonAllTrue(cr);
}


FORCEINLINE int TestSignSIMD( const fltx4 & a )								// mask of which floats have the high bit set
{
	// NOTE: this maps to SSE way better than it does to VMX (most code uses IsAnyNegative(), though)
	int nRet = 0;

	const fltx4_union & a_union = (const fltx4_union &)a;
	nRet |= ( a_union.m128_u32[0] & 0x80000000 ) >> 31; // sign(x) -> bit 0
	nRet |= ( a_union.m128_u32[1] & 0x80000000 ) >> 30; // sign(y) -> bit 1
	nRet |= ( a_union.m128_u32[2] & 0x80000000 ) >> 29; // sign(z) -> bit 2
	nRet |= ( a_union.m128_u32[3] & 0x80000000 ) >> 28; // sign(w) -> bit 3

	return nRet;
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	return __vrlimi( a, __vzero(), 1, 0 );
}

FORCEINLINE bool IsAnyNegative( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	// NOTE: this tests the top bits of each vector element using integer math
	//       (so it ignores NaNs - it will return true for "-NaN")
	unsigned int equalFlags = 0;
    fltx4 signMask = __vspltisw( -1 );             // 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF (low order 5 bits of each element = 31)
    signMask       = __vslw( signMask, signMask ); // 0x80000000 0x80000000 0x80000000 0x80000000 
	__vcmpequwR( Four_Zeros, __vand( signMask, a ), &equalFlags );
	return !XMComparisonAllTrue( equalFlags );
}

FORCEINLINE bool IsAnyTrue( const fltx4 & a )
{
	unsigned int equalFlags = 0;
	__vcmpequwR( Four_Zeros, a, &equalFlags ); // compare to zero
	return XMComparisonAnyFalse( equalFlags ); // at least one element was not zero, eg was true
}

FORCEINLINE fltx4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
    return __vcmpeqfp( a, b );
}


FORCEINLINE fltx4 CmpGtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? ~0:0
{
    return __vcmpgtfp( a, b );
}

FORCEINLINE fltx4 CmpGeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? ~0:0
{
    return __vcmpgefp( a, b );
}

FORCEINLINE fltx4 CmpLtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? ~0:0
{
    return __vcmpgtfp( b, a );
}

FORCEINLINE fltx4 CmpLeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? ~0:0
{
    return __vcmpgefp( b, a );
}

FORCEINLINE fltx4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? ~0 : 0
{
	return XMVectorInBounds( a, b );
}

// returned[i] = ReplacementMask[i] == 0 ? OldValue : NewValue
FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
    return __vsel( OldValue, NewValue, ReplacementMask );
}


// perform an Altivec permute op. There is no corresponding SSE op, so 
// this function is missing from that fork. This is deliberate, because
// permute-based algorithms simply need to be abandoned and rebuilt 
// differently way for SSE. 
// (see http://developer.apple.com/hardwaredrivers/ve/sse.html#Translation_Perm )
template< typename T, typename U >
FORCEINLINE T PermuteVMX( T a, T b, U swizzleMask )
{
	return __vperm( a, b, swizzleMask );
}

// AKA "Broadcast", "Splat"
FORCEINLINE fltx4 ReplicateX4( float flValue )					//  a,a,a,a
{
	// NOTE: if flValue comes from a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
	float * pValue = &flValue;
	Assert( pValue );
    Assert( ((unsigned int)pValue & 3) == 0);
	return __vspltw( __lvlx( pValue, 0 ), 0 );
}

FORCEINLINE fltx4 ReplicateX4( const float *pValue )					//  a,a,a,a
{
	Assert( pValue );
	return __vspltw( __lvlx( pValue, 0 ), 0 );
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE fltx4 ReplicateIX4( int nValue )
{
	// NOTE: if nValue comes from a register, this causes a Load-Hit-Store stall (should not mix ints with fltx4s!)
	int * pValue = &nValue;
	Assert( pValue );
    Assert( ((unsigned int)pValue & 3) == 0);
	return __vspltw( __lvlx( pValue, 0 ), 0 );
}

// Round towards positive infinity
FORCEINLINE fltx4 CeilSIMD( const fltx4 &a )
{
	return __vrfip(a);
}

// Round towards nearest integer
FORCEINLINE fltx4 RoundSIMD( const fltx4 &a )
{
	return __vrfin(a);
}

// Round towards negative infinity
FORCEINLINE fltx4 FloorSIMD( const fltx4 &a )
{
	return __vrfim(a);
}

FORCEINLINE fltx4 SqrtEstSIMD( const fltx4 & a )				// sqrt(a), more or less
{
	// This is emulated from rsqrt
	return XMVectorSqrtEst( a );
}

FORCEINLINE fltx4 SqrtSIMD( const fltx4 & a )					// sqrt(a)
{
	// This is emulated from rsqrt
	return XMVectorSqrt( a );
}

FORCEINLINE fltx4 ReciprocalSqrtEstSIMD( const fltx4 & a )		// 1/sqrt(a), more or less
{
    return __vrsqrtefp( a );
}

FORCEINLINE fltx4 ReciprocalSqrtEstSaturateSIMD( const fltx4 & a )
{
	// Convert zeros to epsilons
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 a_safe = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	return ReciprocalSqrtEstSIMD( a_safe );
}

FORCEINLINE fltx4 ReciprocalSqrtSIMD( const fltx4 & a )			// 1/sqrt(a)
{
	// This uses Newton-Raphson to improve the HW result
 	return XMVectorReciprocalSqrt( a );
}

FORCEINLINE fltx4 ReciprocalEstSIMD( const fltx4 & a )			// 1/a, more or less
{
    return __vrefp( a );
}

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton iteration.
/// No error checking!
FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )				// 1/a
{
	// This uses Newton-Raphson to improve the HW result
	return XMVectorReciprocal( a );
}

FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )	// a/b
{
	return MulSIMD( ReciprocalSIMD( b ), a );
}

FORCEINLINE fltx4 DivEstSIMD( const fltx4 & a, const fltx4 & b )	// Est(a/b)
{
	return MulSIMD( ReciprocalEstSIMD( b ), a );
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalEstSaturateSIMD( const fltx4 & a )
{
	// Convert zeros to epsilons
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 a_safe = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	return ReciprocalEstSIMD( a_safe );
}

FORCEINLINE fltx4 ReciprocalSaturateSIMD( const fltx4 & a )
{
	// Convert zeros to epsilons
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 a_safe = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	return ReciprocalSIMD( a_safe );

	// FIXME: This could be faster (BUT: it doesn't preserve the sign of -0.0, whereas the above does)
	// fltx4 zeroMask = CmpEqSIMD( Four_Zeros, a );
	// fltx4 a_safe = XMVectorSelect( a, Four_Epsilons, zeroMask );
	// return ReciprocalSIMD( a_safe );
}

// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
FORCEINLINE fltx4 ExpSIMD( const fltx4 &toPower )
{
	return XMVectorExp(toPower);
}

// a unique Altivec concept, the "Vector 2 Raised to the Exponent Estimate Floating Point",
// which is accurate to four bits of mantissa.
FORCEINLINE fltx4 Exp2EstSIMD( const fltx4 &f )
{
	return XMVectorExpEst( f );
}

// Clamps the components of a vector to a specified minimum and maximum range.
FORCEINLINE fltx4 ClampVectorSIMD( FLTX4 in, FLTX4 min, FLTX4 max)
{
	return XMVectorClamp(in, min, max);
}

FORCEINLINE fltx4 LoadUnalignedSIMD( const void *pSIMD )
{
	return XMLoadVector4( pSIMD );
}

// load a 3-vector (as opposed to LoadUnalignedSIMD, which loads a 4-vec). 
FORCEINLINE fltx4 LoadUnaligned3SIMD( const void *pSIMD )
{
	return XMLoadVector3( pSIMD );
}

// load a single unaligned float into the x component of a SIMD word
FORCEINLINE fltx4 LoadUnalignedFloatSIMD( const float *pFlt )
{
	return __lvlx( pFlt, 0 );
}

FORCEINLINE fltx4 LoadAlignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned & pSIMD )
{
	fltx4 out = XMLoadVector3A(pSIMD.Base());
	// squelch w
	return __vrlimi( out, __vzero(), 1, 0 );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned * RESTRICT pSIMD )
{
	fltx4 out = XMLoadVector3A(pSIMD);
	// squelch w
	return __vrlimi( out, __vzero(), 1, 0 );
}

FORCEINLINE void StoreAlignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnalignedSIMD( float *pSIMD, const fltx4 & a )
{
	XMStoreVector4( pSIMD, a );
}

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	XMStoreVector3( pSIMD, a );
}

// strongly typed -- for typechecking as we transition to SIMD
FORCEINLINE void StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, const fltx4 & a )
{
	XMStoreVector3A(pSIMD->Base(),a);
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination[0],  pDestination[1],  pDestination[2],  pDestination[3]
// The Vectors are assumed to be unaligned.
FORCEINLINE void StoreFourUnalignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											   Vector * const pDestination )
{
	// since four Vec3s == 48 bytes, we can use full-vector stores here, so long as 
	// we arrange the data properly first.
	// The vrlimi ops trash the destination param which is why we require 
	// pass-by-copy. I'm counting on the compiler to schedule these properly.
	b = __vrlimi( b, b, 15, 1 );    // b = y1z1__x1
	c = __vrlimi( c, c, 15, 2 );    // c = z2__x2y2

	a = __vrlimi( a, b, 1, 0 );     // a = x0y0z0x1
	b = __vrlimi( b, c, 2|1, 0 );   // b = y1z1x2y2
	c = __vrlimi( c, d, 4|2|1, 3 ); // c = z2x3y3z3

	float *RESTRICT pOut = pDestination->Base();
	StoreUnalignedSIMD( pOut + 0, a );
	StoreUnalignedSIMD( pOut + 4, b );
	StoreUnalignedSIMD( pOut + 8, c );
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination ,  pDestination + 1,  pDestination + 2,  pDestination + 3
// The Vectors are assumed to start on an ALIGNED address, that is, 
// pDestination is 16-byte aligned (thhough obviously pDestination+1 is not).
FORCEINLINE void StoreFourAlignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											   Vector * const pDestination )
{
	// since four Vec3s == 48 bytes, we can use full-vector stores here, so long as 
	// we arrange the data properly first.
	// The vrlimi ops trash the destination param which is why we require 
	// pass-by-copy. I'm counting on the compiler to schedule these properly.
	b = __vrlimi( b, b, 15, 1 );    // b = y1z1__x1
	c = __vrlimi( c, c, 15, 2 );    // c = z2__x2y2

	a = __vrlimi( a, b, 1, 0 );     // a = x0y0z0x1
	b = __vrlimi( b, c, 2|1, 0 );   // b = y1z1x2y2
	c = __vrlimi( c, d, 4|2|1, 3 ); // c = z2x3y3z3

	float *RESTRICT pOut = pDestination->Base();
	StoreAlignedSIMD( pOut + 0, a );
	StoreAlignedSIMD( pOut + 4, b );
	StoreAlignedSIMD( pOut + 8, c );
}

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing 
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way 
// to guarantee an immediate as a parameter to function
// like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
	fltx4 asInt = __vctsxs( vSrc, 0 );
	XMStoreVector4A(pDest->Base(), asInt);
}

FORCEINLINE void TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w )
{
	XMMATRIX xyzwMatrix = _XMMATRIX( x, y, z, w );
	xyzwMatrix = XMMatrixTranspose( xyzwMatrix );
	x = xyzwMatrix.r[0];
	y = xyzwMatrix.r[1];
	z = xyzwMatrix.r[2];
	w = xyzwMatrix.r[3];
}

// Return one in the fastest way -- faster even than loading.
FORCEINLINE fltx4 LoadZeroSIMD( void )
{
	return XMVectorZero();
}

// Return one in the fastest way -- faster even than loading.
FORCEINLINE fltx4 LoadOneSIMD( void )
{
	return XMVectorSplatOne();
}

FORCEINLINE fltx4 SplatXSIMD( fltx4 a )
{
	return XMVectorSplatX( a );
}

FORCEINLINE fltx4 SplatYSIMD( fltx4 a )
{
	return XMVectorSplatY( a );
}

FORCEINLINE fltx4 SplatZSIMD( fltx4 a )
{
	return XMVectorSplatZ( a );
}

FORCEINLINE fltx4 SplatWSIMD( fltx4 a )
{
	return XMVectorSplatW( a );
}

FORCEINLINE fltx4 SetXSIMD( const fltx4& a, const fltx4& x )
{
	fltx4 result = __vrlimi(a, x, 8, 0);
	return result;
}

FORCEINLINE fltx4 SetYSIMD( const fltx4& a, const fltx4& y )
{
	fltx4 result = __vrlimi(a, y, 4, 0);
	return result;
}

FORCEINLINE fltx4 SetZSIMD( const fltx4& a, const fltx4& z )
{
	fltx4 result = __vrlimi(a, z, 2, 0);
	return result;
}

FORCEINLINE fltx4 SetWSIMD( const fltx4& a, const fltx4& w )
{
	fltx4 result = __vrlimi(a, w, 1, 0);
	return result;
}

FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, int nComponent, float flValue )
{
	static int s_nVrlimiMask[4] = { 8, 4, 2, 1 };
	fltx4 val = ReplicateX4( flValue );
	fltx4 result = __vrlimi(a, val, s_nVrlimiMask[nComponent], 0);
	return result;
}

FORCEINLINE fltx4 RotateLeft( const fltx4 & a )
{
	fltx4 compareOne = a;
	return __vrlimi( compareOne, a, 8 | 4 | 2 | 1, 1 );
}

FORCEINLINE fltx4 RotateLeft2( const fltx4 & a )
{
	fltx4 compareOne = a;
	return __vrlimi( compareOne, a, 8 | 4 | 2 | 1, 2 );
}

FORCEINLINE fltx4 RotateRight( const fltx4 & a )
{
	fltx4 compareOne = a;
	return __vrlimi( compareOne, a, 8 | 4 | 2 | 1, 3 );
}

FORCEINLINE fltx4 RotateRight2( const fltx4 & a )
{
	fltx4 compareOne = a;
	return __vrlimi( compareOne, a, 8 | 4 | 2 | 1, 2 );
}


// rotate a vector left by an arbitrary number of 
// bits known at compile time. The bit parameter
// is template because it's actually used as an 
// immediate field in an instruction, eg it absolutely
// must be known at compile time. nBits>127 leads
// to doom. 
// zeroes are shifted in from the right
template < uint nBits > 
FORCEINLINE fltx4 ShiftLeftByBits(const fltx4 &a)
{
	// hopefully the compiler, seeing nBits as a const immediate, elides these ifs
	if ( nBits >= 128 ) // WTF are you doing?!
	{
		return LoadZeroSIMD();
	}
	else if ( nBits == 0 )
	{
		return a;
	}
	else if ( (nBits >  7) ) // if we have to rotate by at least one byte, do the by-octet rotation first
	{
		fltx4 t = __vsldoi( a, (LoadZeroSIMD()), (nBits >> 3) ); // rotated left by octets
		return ShiftLeftByBits< (nBits & 0x7) >( t );
	}
	else // we need to rotate by <= 7 bits 
	{
		// on AltiVec there's no immediate shift left by bits; we need to splat the bits onto a vector and runtime shift.
		// the splat, however, does require an immediate. Go IBM!
		u32x4 shifter = u32x4 (__vspltisb( ((signed char)(nBits & 0x7)) ));
		return __vsl(  a, shifter );
	}
}

// as above, but shift right
template < uint nBits > 
FORCEINLINE fltx4 ShiftRightByBits(const fltx4 &a)
{
	// hopefully the compiler, seeing nBits as a const immediate, elides these ifs
	if ( nBits >= 128 ) // WTF are you doing?!
	{
		return LoadZeroSIMD();
	}
	else if ( nBits == 0 )
	{
		return a;
	}
	else if ( (nBits >  7) ) // if we have to rotate by at least one byte, do the by-octet rotation first
	{
		fltx4 t = __vsldoi( (LoadZeroSIMD()), a, 16 - (nBits >> 3) ); // rotated right by octets -- a rotate right of one is like a rotate left of fifteen. 
		return ShiftRightByBits< (nBits & 0x7) >( t );
	}
	else // we need to rotate by <= 7 bits 
	{
		// on AltiVec there's no immediate shift left by bits; we need to splat the bits onto a vector and runtime shift.
		// the splat, however, does require an immediate. Go IBM!
		u32x4 shifter = u32x4 (__vspltisb( ((signed char)(nBits & 0x7)) ));
		return __vsr( a, shifter );
	}
}

// find the lowest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
// ignores a.w.
// Though this is only five instructions long,
// they are all dependent, making this stall city.
// Forcing this inline should hopefully help with scheduling.
FORCEINLINE fltx4 FindLowestSIMD3( const fltx4 & a )
{
	// a is [x,y,z,G] (where G is garbage)
	// rotate left by one 
	fltx4 compareOne = a ;
	compareOne = __vrlimi( compareOne, a, 8 | 4 , 1 );
	// compareOne is [y,z,G,G]
	fltx4 retval = MinSIMD( a, compareOne );
	// retVal is [min(x,y), min(y,z), G, G]
	compareOne = __vrlimi( compareOne, a, 8 , 2);
	// compareOne is [z, G, G, G]
	retval = MinSIMD( retval, compareOne );
	// retVal = [ min(min(x,y),z), G, G, G ]
	
	// splat the x component out to the whole vector and return
	return SplatXSIMD( retval );
}

// find the highest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
// ignores a.w.
// Though this is only five instructions long,
// they are all dependent, making this stall city.
// Forcing this inline should hopefully help with scheduling.
FORCEINLINE fltx4 FindHighestSIMD3( const fltx4 & a )
{
	// a is [x,y,z,G] (where G is garbage)
	// rotate left by one 
	fltx4 compareOne = a ;
	compareOne = __vrlimi( compareOne, a, 8 | 4 , 1 );
	// compareOne is [y,z,G,G]
	fltx4 retval = MaxSIMD( a, compareOne );
	// retVal is [max(x,y), max(y,z), G, G]
	compareOne = __vrlimi( compareOne, a, 8 , 2);
	// compareOne is [z, G, G, G]
	retval = MaxSIMD( retval, compareOne );
	// retVal = [ max(max(x,y),z), G, G, G ]

	// splat the x component out to the whole vector and return
	return SplatXSIMD( retval );
}


// Transform many (horizontal) points in-place by a 3x4 matrix,
// here already loaded onto three fltx4 registers. 
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1. 
// To spare yourself the annoyance of loading the matrix yourself,
// use one of the overloads below.
void TransformManyPointsBy(VectorAligned * RESTRICT pVectors, unsigned int numVectors, FLTX4 mRow1, FLTX4 mRow2, FLTX4 mRow3);

// Transform many (horizontal) points in-place by a 3x4 matrix.
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1. 
// In this function, the matrix need not be aligned.
FORCEINLINE void TransformManyPointsBy(VectorAligned * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t &pMatrix)
{
	return TransformManyPointsBy(pVectors, numVectors, 
								 LoadUnalignedSIMD( pMatrix[0] ), LoadUnalignedSIMD( pMatrix[1] ), LoadUnalignedSIMD( pMatrix[2] ) );
}

// Transform many (horizontal) points in-place by a 3x4 matrix.
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1. 
// In this function, the matrix must itself be aligned on a 16-byte
// boundary.
FORCEINLINE void TransformManyPointsByA(VectorAligned * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t &pMatrix)
{
	return TransformManyPointsBy(pVectors, numVectors, 
								 LoadAlignedSIMD( pMatrix[0] ), LoadAlignedSIMD( pMatrix[1] ), LoadAlignedSIMD( pMatrix[2] ) );
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------

// Load 4 aligned words into a SIMD register
FORCEINLINE i32x4 LoadAlignedIntSIMD( const void * RESTRICT pSIMD)
{
	return XMLoadVector4A(pSIMD);
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD(const void * RESTRICT pSIMD)
{
	return XMLoadVector4( pSIMD );
}

// save into four words, 16-byte aligned
FORCEINLINE void StoreAlignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreAlignedIntSIMD( intx4 &pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD.Base() ) ) = a;
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	XMStoreVector4(pSIMD, a);
}

// Load four consecutive uint16's, and turn them into floating point numbers.
// This function isn't especially fast and could be made faster if anyone is
// using it heavily.
FORCEINLINE fltx4 LoadAndConvertUint16SIMD( const uint16 *pInts )
{
	return XMLoadUShort4(reinterpret_cast<const XMUSHORT4 *>(pInts));
}

// a={ a.x, a.z, b.x, b.z }
// combine two fltx4s by throwing away every other field.
FORCEINLINE fltx4 CompressSIMD( fltx4 const & a, fltx4 const &b )
{
	return XMVectorPermute( a, b, XMVectorPermuteControl( 0, 2, 4, 6  )  );
}

// a={ a.x, b.x, c.x, d.x }
// combine 4 fltx4s by throwing away 3/4s of the fields
// TODO: make more efficient by doing this in a parallel way at the caller
//    Compress4SIMD(FourVectors.. )
FORCEINLINE fltx4 Compress4SIMD( fltx4 const a, fltx4 const &b, fltx4 const &c, fltx4 const &d )
{
	fltx4 abcd = __vrlimi( a, b, 4, 3 );  // a.x, b.x, a.z, a.w
	abcd = __vrlimi( abcd, c, 2, 2 );  // ax, bx, cx, aw
	abcd = __vrlimi( abcd, d, 1, 1 );  // ax, bx, cx, dx

	return abcd;
}


// construct a fltx4 from four different scalars, which are assumed to be neither aligned nor contiguous
FORCEINLINE fltx4 LoadGatherSIMD( const float &x, const float &y, const float &z, const float &w )
{
	// load the float into the low word of each vector register (this exploits the unaligned load op)
	fltx4 vx = __lvlx( &x, 0 );
	fltx4 vy = __lvlx( &y, 0 );
	fltx4 vz = __lvlx( &z, 0 );
	fltx4 vw = __lvlx( &w, 0 );
	return Compress4SIMD( vx, vy, vz, vw );
}

// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. No
// fixed point conversion is done.
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	return __vcfux( vSrcA, 0 );
}

// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	return __vcfsx( vSrcA, 0 );
}

// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. Each uint
// will be divided by 2^immed after conversion
// (eg, this is fixed point math). 
/* as if:
   FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const i32x4 &vSrcA, unsigned int uImmed )
   {
   return __vcfux( vSrcA, uImmed );
   }
*/
#define UnsignedFixedIntConvertToFltSIMD(vSrcA, uImmed) (__vcfux( (vSrcA), (uImmed) ))

// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. Each int
// will be divided by 2^immed (eg, this is fixed point
// math). 
/* as if:
   FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA, unsigned int uImmed )
   {
   return __vcfsx( vSrcA, uImmed );
   }
*/
#define SignedFixedIntConvertToFltSIMD(vSrcA, uImmed) (__vcfsx( (vSrcA), (uImmed) ))

// set all components of a vector to a signed immediate int number.
/* as if:
   FORCEINLINE fltx4 IntSetImmediateSIMD(int toImmediate)
   {
   return __vspltisw( toImmediate );
   }
*/
#define IntSetImmediateSIMD(x) (__vspltisw(x))

/*
  works on fltx4's as if they are four uints.
  the first parameter contains the words to be shifted,
  the second contains the amount to shift by AS INTS

  for i = 0 to 3
  shift = vSrcB_i*32:(i*32)+4
  vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
FORCEINLINE fltx4 IntShiftLeftWordSIMD(fltx4 vSrcA, fltx4 vSrcB)
{
	return __vslw(vSrcA, vSrcB);
}

FORCEINLINE float SubFloat( const fltx4 & a, int idx )
{
	// NOTE: if the output goes into a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
	const fltx4_union & a_union = (const fltx4_union &)a;
	return a_union.m128_f32[ idx ];
}

FORCEINLINE float & SubFloat( fltx4 & a, int idx )
{
	fltx4_union & a_union = (fltx4_union &)a;
	return a_union.m128_f32[idx];
}

FORCEINLINE uint32 SubFloatConvertToInt( const fltx4 & a, int idx )
{
	fltx4 t = __vctuxs( a, 0 );
	const fltx4_union & a_union = (const fltx4_union &)t;
	return a_union.m128_u32[idx];
}


FORCEINLINE uint32 SubInt( const fltx4 & a, int idx )
{
	const fltx4_union & a_union = (const fltx4_union &)a;
	return a_union.m128_u32[idx];
}

FORCEINLINE uint32 & SubInt( fltx4 & a, int idx )
{
	fltx4_union & a_union = (fltx4_union &)a;
	return a_union.m128_u32[idx];
}

#else

//---------------------------------------------------------------------
// Intel/SSE implementation
//---------------------------------------------------------------------

FORCEINLINE void StoreAlignedSIMD( float * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_store_ps( pSIMD, a );
}

FORCEINLINE void StoreAlignedSIMD( short * RESTRICT pSIMD, const shortx8 & a )
{
	_mm_store_si128( (shortx8 *)pSIMD, a );
}
FORCEINLINE void StoreUnalignedSIMD( float * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_storeu_ps( pSIMD, a );
}

FORCEINLINE void StoreUnalignedSIMD(short* RESTRICT pSIMD, const shortx8& a)
{
	_mm_storeu_si128((shortx8*)pSIMD, a);
}

FORCEINLINE void StoreUnalignedFloat( float *pSingleFloat, const fltx4 & a )
{
	_mm_store_ss( pSingleFloat, a );
}


FORCEINLINE fltx4 RotateLeft( const fltx4 & a );
FORCEINLINE fltx4 RotateLeft2( const fltx4 & a );

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	_mm_store_ss(pSIMD, a);
	_mm_store_ss(pSIMD+1, RotateLeft(a));
	_mm_store_ss(pSIMD+2, RotateLeft2(a));
}


// strongly typed -- syntactic castor oil used for typechecking as we transition to SIMD
FORCEINLINE void StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, const fltx4 & a )
{
	StoreAlignedSIMD( pSIMD->Base(),a );
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination[0],  pDestination[1],  pDestination[2],  pDestination[3]
// The Vectors are assumed to be unaligned.
FORCEINLINE void StoreFourUnalignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											   Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination ,  pDestination + 1,  pDestination + 2,  pDestination + 3
// The Vectors are assumed to start on an ALIGNED address, that is, 
// pDestination is 16-byte aligned (thhough obviously pDestination+1 is not).
FORCEINLINE void StoreFourAlignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											 Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}

FORCEINLINE fltx4 LoadAlignedSIMD( const void *pSIMD )
{
	return _mm_load_ps( reinterpret_cast< const float *> ( pSIMD ) );
}

FORCEINLINE shortx8 LoadAlignedShortSIMD( const void *pSIMD )
{
	return _mm_load_si128( reinterpret_cast< const shortx8 *> ( pSIMD ) );
}

FORCEINLINE shortx8 LoadUnalignedShortSIMD( const void *pSIMD )
{
	return _mm_loadu_si128( reinterpret_cast< const shortx8 *> ( pSIMD ) );
}

FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const fltx4 & b )				// a & b
{
	return _mm_and_ps( a, b );
}

FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// a & ~b
{
	return _mm_andnot_ps( a, b );
}

FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
	return _mm_xor_ps( a, b );
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
	return _mm_or_ps( a, b );
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	return AndSIMD( a, LoadAlignedSIMD( g_SIMD_clear_wmask ) );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned & pSIMD )
{
	return SetWToZeroSIMD( LoadAlignedSIMD(pSIMD.Base()) );
}

FORCEINLINE fltx4 LoadUnalignedSIMD( const void *pSIMD )
{
	return _mm_loadu_ps( reinterpret_cast<const float *>( pSIMD ) );
}

FORCEINLINE fltx4 LoadUnaligned3SIMD( const void *pSIMD )
{
	return _mm_loadu_ps( reinterpret_cast<const float *>( pSIMD ) );
}

// load a single unaligned float into the x component of a SIMD word
FORCEINLINE fltx4 LoadUnalignedFloatSIMD( const float *pFlt )
{
	return _mm_load_ss(pFlt);
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE fltx4 ReplicateIX4( int i )
{
	fltx4 value = _mm_set_ss( * ( ( float *) &i ) );;
	return _mm_shuffle_ps( value, value, 0);
}


FORCEINLINE fltx4 ReplicateX4( float flValue )
{
	__m128 value = _mm_set_ss( flValue );
	return _mm_shuffle_ps( value, value, 0 );
}

FORCEINLINE fltx4 ReplicateX4( const float * flValue )
{
	__m128 value = _mm_set_ss( *flValue );
	return _mm_shuffle_ps( value, value, 0 );
}

FORCEINLINE float SubFloat( const fltx4 & a, int idx )
{
	// NOTE: if the output goes into a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
#ifndef POSIX
	return a.m128_f32[ idx ];
#else
	return (reinterpret_cast<float const *>(&a))[idx];
#endif
}

FORCEINLINE float & SubFloat( fltx4 & a, int idx )
{
#ifndef POSIX
	return a.m128_f32[ idx ];
#else
	return (reinterpret_cast<float *>(&a))[idx];
#endif
}

FORCEINLINE uint32 SubFloatConvertToInt( const fltx4 & a, int idx )
{
	return (uint32)SubFloat(a,idx);
}

FORCEINLINE uint32 SubInt( const fltx4 & a, int idx )
{
#ifndef POSIX
	return a.m128_u32[idx];
#else
	return (reinterpret_cast<uint32 const *>(&a))[idx];
#endif
}

FORCEINLINE uint32 & SubInt( fltx4 & a, int idx )
{
#ifndef POSIX
	return a.m128_u32[idx];
#else
	return (reinterpret_cast<uint32 *>(&a))[idx];
#endif
}

// Return one in the fastest way -- on the x360, faster even than loading.
FORCEINLINE fltx4 LoadZeroSIMD( void )
{
	return Four_Zeros;
}

// Return one in the fastest way -- on the x360, faster even than loading.
FORCEINLINE fltx4 LoadOneSIMD( void )
{
	return Four_Ones;
}

FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
	return OrSIMD(
		AndSIMD( ReplacementMask, NewValue ),
		AndNotSIMD( ReplacementMask, OldValue ) );
}

// remember, the SSE numbers its words 3 2 1 0
// The way we want to specify shuffles is backwards from the default
// MM_SHUFFLE_REV is in array index order (default is reversed)
#define MM_SHUFFLE_REV(a,b,c,d) _MM_SHUFFLE(d,c,b,a)

FORCEINLINE fltx4 SplatXSIMD( fltx4 const & a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 0, 0, 0, 0 ) );
}

FORCEINLINE fltx4 SplatYSIMD( fltx4 const &a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 1, 1, 1, 1 ) );
}

FORCEINLINE fltx4 SplatZSIMD( fltx4 const &a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 2, 2, 2, 2 ) );
}

FORCEINLINE fltx4 SplatWSIMD( fltx4 const &a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 3, 3, 3, 3 ) );
}

FORCEINLINE fltx4 ShuffleXXYY( const fltx4 &a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 0, 0, 1, 1 ) );
}

FORCEINLINE fltx4 ShuffleXYXY( const fltx4 &a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );
}

FORCEINLINE fltx4 ShuffleZZWW( const fltx4 &a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 2, 2, 3, 3 ) );
}




FORCEINLINE fltx4 SetXSIMD( const fltx4& a, const fltx4& x )
{
	fltx4 result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[0] ), x, a );
	return result;
}

FORCEINLINE fltx4 SetYSIMD( const fltx4& a, const fltx4& y )
{
	fltx4 result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[1] ), y, a );
	return result;
}

FORCEINLINE fltx4 SetZSIMD( const fltx4& a, const fltx4& z )
{
	fltx4 result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[2] ), z, a );
	return result;
}

FORCEINLINE fltx4 SetWSIMD( const fltx4& a, const fltx4& w )
{
	fltx4 result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[3] ), w, a );
	return result;
}

FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, int nComponent, float flValue )
{
	fltx4 val = ReplicateX4( flValue );
	fltx4 result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[nComponent] ), val, a );
	return result;
}

// a b c d -> b c d a
FORCEINLINE fltx4 RotateLeft( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 1, 2, 3, 0 ) );
}

// a b c d -> c d a b
FORCEINLINE fltx4 RotateLeft2( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 2, 3, 0, 1 ) );
}

// a b c d -> d a b c
FORCEINLINE fltx4 RotateRight( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 3, 0, 1, 2 ) );
}

// a b c d -> c d a b
FORCEINLINE fltx4 RotateRight2( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 2, 3, 0, 1 ) );
}

FORCEINLINE fltx4 AddSIMD( const fltx4 & a, const fltx4 & b )				// a+b
{
	return _mm_add_ps( a, b );
}

FORCEINLINE fltx4 SubSIMD( const fltx4 & a, const fltx4 & b )				// a-b
{
	return _mm_sub_ps( a, b );
};

FORCEINLINE fltx4 MulSIMD( const fltx4 & a, const fltx4 & b )				// a*b
{
	return _mm_mul_ps( a, b );
};

FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )				// a/b
{
	return _mm_div_ps( a, b );
};

fltx4 ReciprocalEstSIMD( const fltx4 & a );
FORCEINLINE fltx4 DivEstSIMD( const fltx4 & a, const fltx4 & b )			// Est(a/b)
{
	return MulSIMD( ReciprocalEstSIMD( b ), a );
};

FORCEINLINE fltx4 MaddSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// a*b + c
{
	return AddSIMD( MulSIMD(a,b), c );
}

FORCEINLINE fltx4 MsubSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// c - a*b
{
	return SubSIMD( c, MulSIMD(a,b) );
};

FORCEINLINE fltx4 Dot3SIMD( const fltx4 &a, const fltx4 &b )
{
	fltx4 m = MulSIMD( a, b );
	return AddSIMD( AddSIMD( SplatXSIMD(m), SplatYSIMD(m) ), SplatZSIMD(m) );
}

FORCEINLINE fltx4 Dot4SIMD( const fltx4 &a, const fltx4 &b )
{
	// 4 instructions, serial, order of addition varies so individual elements my differ in the LSB on some CPUs
	fltx4 fl4Product = MulSIMD( a, b );
	fltx4 fl4YXWZ = _mm_shuffle_ps( fl4Product, fl4Product, MM_SHUFFLE_REV(1,0,3,2) );
	fltx4 fl4UUVV = AddSIMD( fl4Product, fl4YXWZ ); // U = X+Y; V = Z+W
	fltx4 fl4VVUU = RotateLeft2( fl4UUVV );
	return AddSIMD( fl4UUVV, fl4VVUU );
}

//TODO: implement as four-way Taylor series (see xbox implementation)
FORCEINLINE fltx4 SinSIMD( const fltx4 &radians )
{
	fltx4 result;
	SubFloat( result, 0 ) = sin( SubFloat( radians, 0 ) );
	SubFloat( result, 1 ) = sin( SubFloat( radians, 1 ) );
	SubFloat( result, 2 ) = sin( SubFloat( radians, 2 ) );
	SubFloat( result, 3 ) = sin( SubFloat( radians, 3 ) );
	return result;
}

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	// FIXME: Make a fast SSE version
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
}

FORCEINLINE void SinCosSIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )				// a*b + c
{
	// FIXME: Make a fast SSE version
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
	SinCos( SubFloat( radians, 3 ), &SubFloat( sine, 3 ), &SubFloat( cosine, 3 ) );
}

//TODO: implement as four-way Taylor series (see xbox implementation)
FORCEINLINE fltx4 ArcSinSIMD( const fltx4 &sine )
{
	// FIXME: Make a fast SSE version
	fltx4 result;
	SubFloat( result, 0 ) = asin( SubFloat( sine, 0 ) );
	SubFloat( result, 1 ) = asin( SubFloat( sine, 1 ) );
	SubFloat( result, 2 ) = asin( SubFloat( sine, 2 ) );
	SubFloat( result, 3 ) = asin( SubFloat( sine, 3 ) );
	return result;
}

FORCEINLINE fltx4 ArcCosSIMD( const fltx4 &cs )
{
	fltx4 result;
	SubFloat( result, 0 ) = acos( SubFloat( cs, 0 ) );
	SubFloat( result, 1 ) = acos( SubFloat( cs, 1 ) );
	SubFloat( result, 2 ) = acos( SubFloat( cs, 2 ) );
	SubFloat( result, 3 ) = acos( SubFloat( cs, 3 ) );
	return result;
}

// tan^1(a/b) .. ie, pass sin in as a and cos in as b
FORCEINLINE fltx4 ArcTan2SIMD( const fltx4 &a, const fltx4 &b )
{
	fltx4 result;
	SubFloat( result, 0 ) = atan2( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( result, 1 ) = atan2( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( result, 2 ) = atan2( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( result, 3 ) = atan2( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return result;
}

FORCEINLINE fltx4 NegSIMD(const fltx4 &a) // negate: -a
{
	return SubSIMD(LoadZeroSIMD(),a);
}

FORCEINLINE int TestSignSIMD( const fltx4 & a )								// mask of which floats have the high bit set
{
	return _mm_movemask_ps( a );
}

FORCEINLINE bool IsAnyNegative( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

FORCEINLINE bool IsAnyTrue( const fltx4 & a )
{
	return (0 != TestSignSIMD( a ));
}

FORCEINLINE fltx4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
	return _mm_cmpeq_ps( a, b );
}

FORCEINLINE fltx4 CmpGtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? ~0:0
{
	return _mm_cmpgt_ps( a, b );
}

FORCEINLINE fltx4 CmpGeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? ~0:0
{
	return _mm_cmpge_ps( a, b );
}

FORCEINLINE fltx4 CmpLtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? ~0:0
{
	return _mm_cmplt_ps( a, b );
}

FORCEINLINE fltx4 CmpLeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? ~0:0
{
	return _mm_cmple_ps( a, b );
}

// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	return	TestSignSIMD( CmpLeSIMD( a, b ) ) == 0;
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	return	TestSignSIMD( CmpLtSIMD( a, b ) ) == 0;
}

// For branching if all a.xyzw == b.xyzw
FORCEINLINE bool IsAllEqual( const fltx4 & a, const fltx4 & b )
{
	return	TestSignSIMD( CmpEqSIMD( a, b ) ) == 0xf;
}

FORCEINLINE fltx4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? ~0 : 0
{
	return AndSIMD( CmpLeSIMD(a,b), CmpGeSIMD(a, NegSIMD(b)) );
}

FORCEINLINE fltx4 MinSIMD( const fltx4 & a, const fltx4 & b )				// min(a,b)
{
	return _mm_min_ps( a, b );
}

FORCEINLINE fltx4 MaxSIMD( const fltx4 & a, const fltx4 & b )				// max(a,b)
{
	return _mm_max_ps( a, b );
}



// SSE lacks rounding operations. 
// Really.
// You can emulate them by setting the rounding mode for the 
// whole processor and then converting to int, and then back again.
// But every time you set the rounding mode, you clear out the
// entire pipeline. So, I can't do them per operation. You
// have to do it once, before the loop that would call these.
// Round towards positive infinity
FORCEINLINE fltx4 CeilSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ceil( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = ceil( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = ceil( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = ceil( SubFloat( a, 3 ) );
	return retVal;

}

fltx4 AbsSIMD( const fltx4 & x );		// To make it more coherent with the whole API (the whole SIMD API is postfixed with SIMD except a couple of methods. Well...)
fltx4 fabs( const fltx4 & x );

// Round towards negative infinity
// This is the implementation that was here before; it assumes
// you are in round-to-floor mode, which I guess is usually the
// case for us vis-a-vis SSE. It's totally unnecessary on 
// VMX, which has a native floor op.
FORCEINLINE fltx4 FloorSIMD( const fltx4 &val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 ival = SubSIMD( AddSIMD( fl4Abs, Four_2ToThe23s ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, fl4Abs ), SubSIMD( ival, Four_Ones ), ival );
	return XorSIMD( ival, XorSIMD( val, fl4Abs ) );			// restore sign bits
}



FORCEINLINE bool IsAnyZeros( const fltx4 & a )								// any floats are zero?
{
	return TestSignSIMD( CmpEqSIMD( a, Four_Zeros ) ) != 0;
}

inline bool IsAllZeros( const fltx4 & var )
{
	return TestSignSIMD( CmpEqSIMD( var, Four_Zeros ) ) == 0xF;
}

FORCEINLINE fltx4 SqrtEstSIMD( const fltx4 & a )					// sqrt(a), more or less
{
	return _mm_sqrt_ps( a );
}

FORCEINLINE fltx4 SqrtSIMD( const fltx4 & a )						// sqrt(a)
{
	return _mm_sqrt_ps( a );
}

FORCEINLINE fltx4 ReciprocalSqrtEstSIMD( const fltx4 & a )			// 1/sqrt(a), more or less
{
	return _mm_rsqrt_ps( a );
}

FORCEINLINE fltx4 ReciprocalSqrtEstSaturateSIMD( const fltx4 & a )
{
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalSqrtEstSIMD( ret );
	return ret;
}

/// uses newton iteration for higher precision results than ReciprocalSqrtEstSIMD
FORCEINLINE fltx4 ReciprocalSqrtSIMD( const fltx4 & a )				// 1/sqrt(a)
{
	fltx4 guess = ReciprocalSqrtEstSIMD( a );
	// newton iteration for 1/sqrt(a) : y(n+1) = 1/2 (y(n)*(3-a*y(n)^2));
	guess = MulSIMD( guess, SubSIMD( Four_Threes, MulSIMD( a, MulSIMD( guess, guess ))));
	guess = MulSIMD( Four_PointFives, guess);
	return guess;
}

FORCEINLINE fltx4 ReciprocalEstSIMD( const fltx4 & a )				// 1/a, more or less
{
	return _mm_rcp_ps( a );
}

/// 1/x for all 4 values, more or less
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalEstSaturateSIMD( const fltx4 & a )
{
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalEstSIMD( ret );
	return ret;
}

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton iteration.
/// No error checking!
FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )					// 1/a
{
	fltx4 ret = ReciprocalEstSIMD( a );
	// newton iteration is: Y(n+1) = 2*Y(n)-a*Y(n)^2
	ret = SubSIMD( AddSIMD( ret, ret ), MulSIMD( a, MulSIMD( ret, ret ) ) );
	return ret;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalSaturateSIMD( const fltx4 & a )
{
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalSIMD( ret );
	return ret;
}

// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
FORCEINLINE fltx4 ExpSIMD( const fltx4 &toPower )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = powf( 2, SubFloat(toPower, 0) );
	SubFloat( retval, 1 ) = powf( 2, SubFloat(toPower, 1) );
	SubFloat( retval, 2 ) = powf( 2, SubFloat(toPower, 2) );
	SubFloat( retval, 3 ) = powf( 2, SubFloat(toPower, 3) );

	return retval;
}

// Clamps the components of a vector to a specified minimum and maximum range.
FORCEINLINE fltx4 ClampVectorSIMD( FLTX4 in, FLTX4 min, FLTX4 max)
{
	return MaxSIMD( min, MinSIMD( max, in ) );
}

FORCEINLINE void TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w)
{
	_MM_TRANSPOSE4_PS( x, y, z, w );
}

FORCEINLINE fltx4 FindLowestSIMD3( const fltx4 &a )
{
	// a is [x,y,z,G] (where G is garbage)
	// rotate left by one 
	fltx4 compareOne = RotateLeft( a );
	// compareOne is [y,z,G,x]
	fltx4 retval = MinSIMD( a, compareOne );
	// retVal is [min(x,y), ... ]
	compareOne = RotateLeft2( a );
	// compareOne is [z, G, x, y]
	retval = MinSIMD( retval, compareOne );
	// retVal = [ min(min(x,y),z)..]
	// splat the x component out to the whole vector and return
	return SplatXSIMD( retval );
	
}

FORCEINLINE fltx4 FindHighestSIMD3( const fltx4 &a )
{
	// a is [x,y,z,G] (where G is garbage)
	// rotate left by one 
	fltx4 compareOne = RotateLeft( a );
	// compareOne is [y,z,G,x]
	fltx4 retval = MaxSIMD( a, compareOne );
	// retVal is [max(x,y), ... ]
	compareOne = RotateLeft2( a );
	// compareOne is [z, G, x, y]
	retval = MaxSIMD( retval, compareOne );
	// retVal = [ max(max(x,y),z)..]
	// splat the x component out to the whole vector and return
	return SplatXSIMD( retval );
	
}


inline bool IsVector3LessThan(const fltx4 &v1, const fltx4 &v2 )
{
	bi32x4 isOut = CmpLtSIMD( v1, v2 );
	return IsAnyNegative( isOut );
}

inline bool IsVector4LessThan(const fltx4 &v1, const fltx4 &v2 )
{
	bi32x4 isOut = CmpLtSIMD( v1, v2 );
	return IsAnyNegative( isOut );
}


// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------


#if 0				/* pc does not have these ops */
// splat all components of a vector to a signed immediate int number.
FORCEINLINE fltx4 IntSetImmediateSIMD(int to)
{
	//CHRISG: SSE2 has this, but not SSE1. What to do?
	fltx4 retval;
	SubInt( retval, 0 ) = to;
	SubInt( retval, 1 ) = to;
	SubInt( retval, 2 ) = to;
	SubInt( retval, 3 ) = to;
	return retval;
}
#endif

// Load 4 aligned words into a SIMD register
FORCEINLINE i32x4 LoadAlignedIntSIMD( const void * RESTRICT pSIMD)
{
	return _mm_load_ps( reinterpret_cast<const float *>(pSIMD) );
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD( const void * RESTRICT pSIMD)
{
	return _mm_loadu_ps( reinterpret_cast<const float *>(pSIMD) );
}

// save into four words, 16-byte aligned
FORCEINLINE void StoreAlignedIntSIMD( int32 * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_store_ps( reinterpret_cast<float *>(pSIMD), a );
}

FORCEINLINE void StoreAlignedIntSIMD( intx4 &pSIMD, const fltx4 & a )
{
	_mm_store_ps( reinterpret_cast<float *>(pSIMD.Base()), a );
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_storeu_ps( reinterpret_cast<float *>(pSIMD), a );
}

// a={ a.x, a.z, b.x, b.z }
// combine two fltx4s by throwing away every other field.
FORCEINLINE fltx4 CompressSIMD( fltx4 const & a, fltx4 const &b )
{
	return _mm_shuffle_ps( a, b, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );
}

// Load four consecutive uint16's, and turn them into floating point numbers.
// This function isn't especially fast and could be made faster if anyone is
// using it heavily.
FORCEINLINE fltx4 LoadAndConvertUint16SIMD( const uint16 *pInts )
{
#ifdef POSIX
	fltx4 retval;
	SubFloat( retval, 0 ) = pInts[0];
	SubFloat( retval, 1 ) = pInts[1];
	SubFloat( retval, 2 ) = pInts[2];
	SubFloat( retval, 3 ) = pInts[3];
	return retval;
#else
	__m128i inA = _mm_loadl_epi64( (__m128i const*) pInts); // Load the lower 64 bits of the value pointed to by p into the lower 64 bits of the result, zeroing the upper 64 bits of the result.
	inA = _mm_unpacklo_epi16( inA, _mm_setzero_si128() ); // unpack unsigned 16's to signed 32's
	return _mm_cvtepi32_ps(inA);
#endif
}


// a={ a.x, b.x, c.x, d.x }
// combine 4 fltx4s by throwing away 3/4s of the fields
FORCEINLINE fltx4 Compress4SIMD( fltx4 const a, fltx4 const &b, fltx4 const &c, fltx4 const &d )
{
	fltx4 aacc = _mm_shuffle_ps( a, c, MM_SHUFFLE_REV( 0, 0, 0, 0 ) );
	fltx4 bbdd = _mm_shuffle_ps( b, d, MM_SHUFFLE_REV( 0, 0, 0, 0 ) );
	return MaskedAssign( LoadAlignedSIMD( g_SIMD_EveryOtherMask ), bbdd, aacc );
}

// outa={a.x, a.x, a.y, a.y}, outb = a.z, a.z, a.w, a.w }
FORCEINLINE void ExpandSIMD( fltx4 const &a, fltx4 &fl4OutA, fltx4 &fl4OutB )
{
	fl4OutA = _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 0, 0, 1, 1 ) );
	fl4OutB = _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 2, 2, 3, 3 ) );

}


// construct a fltx4 from four different scalars, which are assumed to be neither aligned nor contiguous
FORCEINLINE fltx4 LoadGatherSIMD( const float &x, const float &y, const float &z, const float &w )
{
	// load the float into the low word of each vector register (this exploits the unaligned load op)
	fltx4 vx = _mm_load_ss( &x );
	fltx4 vy = _mm_load_ss( &y );
	fltx4 vz = _mm_load_ss( &z );
	fltx4 vw = _mm_load_ss( &w );
	return Compress4SIMD( vx, vy, vz, vw );
}

// CHRISG: the conversion functions all seem to operate on m64's only...
// how do we make them work here?

// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. No
// fixed point conversion is done.
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const u32x4 &vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) SubInt( retval, 0 ) );
	SubFloat( retval, 1 ) = ( (float) SubInt( retval, 1 ) );
	SubFloat( retval, 2 ) = ( (float) SubInt( retval, 2 ) );
	SubFloat( retval, 3 ) = ( (float) SubInt( retval, 3 ) );
	return retval;
}

// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	return  _mm_cvtepi32_ps( (const __m128i &)vSrcA );
}

FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const shortx8 &vSrcA )
{
	return  _mm_cvtepi32_ps( vSrcA );
}

#if 0
// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[0]));
	SubFloat( retval, 1 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[1]));
	SubFloat( retval, 2 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[2]));
	SubFloat( retval, 3 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[3]));
	return retval;
}

#endif

/*
  works on fltx4's as if they are four uints.
  the first parameter contains the words to be shifted,
  the second contains the amount to shift by AS INTS

  for i = 0 to 3
  shift = vSrcB_i*32:(i*32)+4
  vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
FORCEINLINE i32x4 IntShiftLeftWordSIMD(const i32x4 &vSrcA, const i32x4 &vSrcB)
{
	i32x4 retval;
	SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
	SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
	SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
	SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);


	return retval;
}


// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing 
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way 
// to guarantee an immediate as a parameter to function
// like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
#if defined(_MSC_VER) && _MSC_VER >= 1900 && defined(COMPILER_MSVC64)
	(*pDest)[0] = (int)SubFloat(vSrc, 0);
	(*pDest)[1] = (int)SubFloat(vSrc, 1);
	(*pDest)[2] = (int)SubFloat(vSrc, 2);
	(*pDest)[3] = (int)SubFloat(vSrc, 3);
#else
	__m64 bottom = _mm_cvttps_pi32( vSrc );
	__m64 top    = _mm_cvttps_pi32( _mm_movehl_ps(vSrc,vSrc) );

	*reinterpret_cast<__m64 *>(&(*pDest)[0]) = bottom;
	*reinterpret_cast<__m64 *>(&(*pDest)[2]) = top;

	_mm_empty();
#endif
}



#endif

// a={a.y, a.z, a.w, b.x } b={b.y, b.z, b.w, b.x }
FORCEINLINE void RotateLeftDoubleSIMD( fltx4 &a, fltx4 &b )
{
	a = SetWSIMD( RotateLeft( a ), SplatXSIMD( b ) );
	b = RotateLeft( b );
}


// // Some convenience operator overloads, which are just aliasing the functions above.
// Unneccessary on 360, as you already have them from xboxmath.h (same for PS3 PPU and SPU)
#if !defined(PLATFORM_PPC) && !defined( POSIX ) && !defined(SPU)
#if 1  // TODO: verify generation of non-bad code. 
// Componentwise add
FORCEINLINE fltx4 operator+( FLTX4 a, FLTX4 b )
{
	return AddSIMD( a, b );
}

// Componentwise subtract
FORCEINLINE fltx4 operator-( FLTX4 a, FLTX4 b )
{
	return SubSIMD( a, b );
}

// Componentwise multiply
FORCEINLINE fltx4 operator*( FLTX4 a, FLTX4 b )
{
	return MulSIMD( a, b );
}

// No divide. You need to think carefully about whether you want a reciprocal
// or a reciprocal estimate.

// bitwise and
FORCEINLINE fltx4 operator&( FLTX4 a, FLTX4 b )
{
	return AndSIMD( a ,b );
}

// bitwise or
FORCEINLINE fltx4 operator|( FLTX4 a, FLTX4 b )
{
	return OrSIMD( a, b );
}

// bitwise xor
FORCEINLINE fltx4 operator^( FLTX4 a, FLTX4 b )
{
	return XorSIMD( a, b );
}

// unary negate
FORCEINLINE fltx4 operator-( FLTX4 a )
{
	return NegSIMD( a );
}
#endif // 0
#endif

#if defined(_X360) || defined(_PS3)
FORCEINLINE fltx4 VectorMergeHighSIMD( fltx4 fl4SrcA, fltx4 fl4SrcB )
{
#if defined( _X360 )
	return __vmrghw( fl4SrcA, fl4SrcB );
#else
	return vec_mergeh( fl4SrcA, fl4SrcB );
#endif
}

FORCEINLINE fltx4 VectorMergeLowSIMD( fltx4 fl4SrcA, fltx4 fl4SrcB )
{
#if defined( _X360 )
	return __vmrglw( fl4SrcA, fl4SrcB );
#else
	return vec_mergel( fl4SrcA, fl4SrcB );
#endif
}
#endif

#ifndef SPU
// fourplanes_t, Frustrum_t are not supported on SPU
// It would make sense to support FourVectors on SPU at some point.

struct ALIGN16 fourplanes_t
{
	fltx4		nX;
	fltx4		nY;
	fltx4		nZ;
	fltx4		dist;
	bi32x4		xSign;
	bi32x4		ySign;
	bi32x4		zSign;
	fltx4		nXAbs;
	fltx4		nYAbs;
	fltx4		nZAbs;

	void ComputeSignbits();

	// fast SIMD loads
	void Set4Planes( const VPlane *pPlanes );
	void Set2Planes( const VPlane *pPlanes );
	void Get4Planes( VPlane *pPlanesOut ) const;
	void Get2Planes( VPlane *pPlanesOut ) const;
	// not-SIMD, much slower
	void GetPlane( int index, Vector *pNormal, float *pDist ) const;
	void SetPlane( int index, const Vector &vecNormal, float planeDist );
};

class ALIGN16 Frustum_t
{
public:
	Frustum_t();
	void SetPlane( int i, const Vector &vecNormal, float dist );
	void GetPlane( int i, Vector *pNormalOut, float *pDistOut ) const;
	void SetPlanes( const VPlane *pPlanes );
	void GetPlanes( VPlane *pPlanesOut ) const;
	// returns false if the box is within the frustum, true if it is outside
	bool CullBox( const Vector &mins, const Vector &maxs ) const;
	bool CullBoxCenterExtents( const Vector &center, const Vector &extents ) const;

	bool CullBox( const fltx4 &fl4Mins, const fltx4 &fl4Maxs ) const;
	bool CullBoxCenterExtents( const fltx4 &fl4Center, const fltx4 &fl4Extents ) const;


	// Return true if frustum contains this bounding volume, false if any corner is outside
	bool Contains( const Vector &mins, const Vector &maxs ) const;

	// Return true if this frustum intersects the frustum, false if it is outside
	bool Intersects( Frustum_t &otherFrustum ) const;

	// Return true if this bounding volume intersects the frustum, false if it is outside
	bool Intersects( const Vector &mins, const Vector &maxs ) const;
	bool IntersectsCenterExtents( const Vector &center, const Vector &extents ) const;

	bool Intersects( const fltx4 &fl4Mins, const fltx4 &fl4Maxs ) const;
	bool IntersectsCenterExtents( const fltx4 &fl4Center, const fltx4 &fl4Extents ) const;

	
	void CreatePerspectiveFrustum( const Vector& origin, const Vector &forward, 
		const Vector &right, const Vector &up, float flZNear, float flZFar, 
		float flFovX, float flAspect );

	void CreatePerspectiveFrustumFLU( const Vector& vOrigin, const Vector &vForward, 
		const Vector &vLeft, const Vector &vUp, float flZNear, float flZFar, 
		float flFovX, float flAspect );

	// Version that accepts angles instead of vectors
	void CreatePerspectiveFrustum( const Vector& origin, const QAngle &angles, float flZNear, 
		float flZFar, float flFovX, float flAspectRatio );

	// Generate a frustum based on orthographic parameters
	void CreateOrthoFrustum( const Vector &origin, const Vector &forward, const Vector &right, const Vector &up, 
		float flLeft, float flRight, float flBottom, float flTop, float flZNear, float flZFar );

	void CreateOrthoFrustumFLU( const Vector &vOrigin, const Vector &vForward, const Vector &vLeft, const Vector &vUp, 
		float flLeft, float flRight, float flBottom, float flTop, float flZNear, float flZFar );

	// The points returned correspond to the corners of the frustum faces 
	// Points 0 to 3 correspond to the near face 
	// Points 4 to 7 correspond to the far face 
	// Returns points in a face in this order:
	//  2--3
	//	|  |
	//	0--1
	// Returns false if a corner couldn't be generated for some reason.
	bool GetCorners( Vector *pPoints ) const;
		
	fourplanes_t	planes[2];
};

#endif

class FourQuaternions;
/// class FourVectors stores 4 independent vectors for use in SIMD processing. These vectors are
/// stored in the format x x x x y y y y z z z z so that they can be efficiently SIMD-accelerated.
class ALIGN16 FourVectors
{
public:
	fltx4 x, y, z;

	FourVectors(void)
	{
	}

	FourVectors( FourVectors const &src )
	{
		x=src.x;
		y=src.y;
		z=src.z;
	}

	explicit FORCEINLINE FourVectors( float a )
	{
		fltx4 aReplicated = ReplicateX4( a );
		x = y = z = aReplicated;
	}

	FORCEINLINE void Init( void )
	{
		x = Four_Zeros;
		y = Four_Zeros;
		z = Four_Zeros;
	}

	FORCEINLINE void Init( float flX, float flY, float flZ )
	{
		x = ReplicateX4( flX );
		y = ReplicateX4( flY );
		z = ReplicateX4( flZ );
	}

	FORCEINLINE FourVectors( float flX, float flY, float flZ )
	{
		Init( flX, flY, flZ );
	}

	FORCEINLINE void Init( fltx4 const &fl4X, fltx4 const &fl4Y, fltx4 const &fl4Z )
	{
		x = fl4X;
		y = fl4Y;
		z = fl4Z;
	}

	FORCEINLINE FourVectors( fltx4 const &fl4X, fltx4 const &fl4Y, fltx4 const &fl4Z )
	{
		Init( fl4X, fl4Y, fl4Z );
	}



	/// construct a FourVectors from 4 separate Vectors
	FORCEINLINE FourVectors(Vector const &a, Vector const &b, Vector const &c, Vector const &d)
	{
		LoadAndSwizzle(a,b,c,d);
	}

	/// construct a FourVectors from 4 separate Vectors
	FORCEINLINE FourVectors(VectorAligned const &a, VectorAligned const &b, VectorAligned const &c, VectorAligned const &d)
	{
		LoadAndSwizzleAligned(a,b,c,d);
	}

	// construct from twelve floats; really only useful for static const constructors.
	// input arrays must be aligned, and in the fourvectors' native format
	// (eg in xxxx,yyyy,zzzz form) 
	// each pointer should be to an aligned array of four floats
	FORCEINLINE FourVectors( const float *xs , const float *ys, const float *zs ) :
		x( LoadAlignedSIMD(xs) ), y( LoadAlignedSIMD(ys) ), z( LoadAlignedSIMD(zs) ) 
	{};

	FORCEINLINE void DuplicateVector(Vector const &v)			//< set all 4 vectors to the same vector value
	{
		x=ReplicateX4(v.x);
		y=ReplicateX4(v.y);
		z=ReplicateX4(v.z);
	}

	FORCEINLINE fltx4 const & operator[](int idx) const
	{
		return *((&x)+idx);
	}

	FORCEINLINE fltx4 & operator[](int idx)
	{
		return *((&x)+idx);
	}

	FORCEINLINE void operator+=(FourVectors const &b)			//< add 4 vectors to another 4 vectors
	{
		x=AddSIMD(x,b.x);
		y=AddSIMD(y,b.y);
		z=AddSIMD(z,b.z);
	}

	FORCEINLINE void operator-=(FourVectors const &b)			//< subtract 4 vectors from another 4
	{
		x=SubSIMD(x,b.x);
		y=SubSIMD(y,b.y);
		z=SubSIMD(z,b.z);
	}

	FORCEINLINE void operator*=(FourVectors const &b)			//< scale all four vectors per component scale
	{
		x=MulSIMD(x,b.x);
		y=MulSIMD(y,b.y);
		z=MulSIMD(z,b.z);
	}

	FORCEINLINE void operator*=(const fltx4 & scale)			//< scale 
	{
		x=MulSIMD(x,scale);
		y=MulSIMD(y,scale);
		z=MulSIMD(z,scale);
	}

	FORCEINLINE void operator*=(float scale)					//< uniformly scale all 4 vectors
	{
		fltx4 scalepacked = ReplicateX4(scale);
		*this *= scalepacked;
	}

	FORCEINLINE fltx4 operator*(FourVectors const &b) const		//< 4 dot products
	{
		fltx4 dot=MulSIMD(x,b.x);
		dot=MaddSIMD(y,b.y,dot);
		dot=MaddSIMD(z,b.z,dot);
		return dot;
	}

	FORCEINLINE fltx4 operator*(Vector const &b) const			//< dot product all 4 vectors with 1 vector
	{
		fltx4 dot=MulSIMD(x,ReplicateX4(b.x));
		dot=MaddSIMD(y,ReplicateX4(b.y), dot);
		dot=MaddSIMD(z,ReplicateX4(b.z), dot);
		return dot;
	}

	FORCEINLINE FourVectors operator*(float b) const					//< scale
	{
		fltx4 scalepacked = ReplicateX4( b );
		FourVectors res;
		res.x = MulSIMD( x, scalepacked );
		res.y = MulSIMD( y, scalepacked );
		res.z = MulSIMD( z, scalepacked );
		return res;
	}

	FORCEINLINE FourVectors operator*( FLTX4 fl4Scale ) const					//< scale
	{
		FourVectors res;
		res.x = MulSIMD( x, fl4Scale );
		res.y = MulSIMD( y, fl4Scale );
		res.z = MulSIMD( z, fl4Scale );
		return res;
	}

	FORCEINLINE void VProduct( FourVectors const &b )				//< component by component mul
	{
		x=MulSIMD(x,b.x);
		y=MulSIMD(y,b.y);
		z=MulSIMD(z,b.z);
	}
	FORCEINLINE void MakeReciprocal(void)						//< (x,y,z)=(1/x,1/y,1/z)
	{
		x=ReciprocalSIMD(x);
		y=ReciprocalSIMD(y);
		z=ReciprocalSIMD(z);
	}

	FORCEINLINE void MakeReciprocalSaturate(void)				//< (x,y,z)=(1/x,1/y,1/z), 1/0=1.0e23
	{
		x=ReciprocalSaturateSIMD(x);
		y=ReciprocalSaturateSIMD(y);
		z=ReciprocalSaturateSIMD(z);
	}

	// Assume the given matrix is a rotation, and rotate these vectors by it.
	// If you have a long list of FourVectors structures that you all want 
	// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
	inline void RotateBy(const matrix3x4_t& matrix);
	/***** removed because one of the SWIG permutations doesn't include ssequaternion.h, causing a missing symbol on this function:
	// rotate these vectors ( in place ) by the corresponding quaternions:
	inline void RotateBy( const FourQuaternions &quats );
	******/

	/// You can use this to rotate a long array of FourVectors all by the same
	/// matrix. The first parameter is the head of the array. The second is the
	/// number of vectors to rotate. The third is the matrix.
	static void RotateManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix );

	static void RotateManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix, FourVectors * RESTRICT pOut );

	/// Assume the vectors are points, and transform them in place by the matrix.
	inline void TransformBy(const matrix3x4_t& matrix);

	/// You can use this to Transform a long array of FourVectors all by the same
	/// matrix. The first parameter is the head of the array. The second is the
	/// number of vectors to rotate. The third is the matrix. The fourth is the 
	/// output buffer, which must not overlap the pVectors buffer. This is not 
	/// an in-place transformation.
	static void TransformManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix, FourVectors * RESTRICT pOut );

	/// You can use this to Transform a long array of FourVectors all by the same
	/// matrix. The first parameter is the head of the array. The second is the
	/// number of vectors to rotate. The third is the matrix. The fourth is the 
	/// output buffer, which must not overlap the pVectors buffer. 
	/// This is an in-place transformation.
	static void TransformManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix );

	static void CalcClosestPointOnLineSIMD( const FourVectors &P, const FourVectors &vLineA, const FourVectors &vLineB, FourVectors &vClosest, fltx4 *outT = 0);
	static fltx4 CalcClosestPointToLineTSIMD( const FourVectors &P, const FourVectors &vLineA, const FourVectors &vLineB, FourVectors &vDir );

	// X(),Y(),Z() - get at the desired component of the i'th (0..3) vector.
	FORCEINLINE const float & X(int idx) const
	{
		// NOTE: if the output goes into a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
		return SubFloat( (fltx4 &)x, idx );
	}

	FORCEINLINE const float & Y(int idx) const
	{
		return SubFloat( (fltx4 &)y, idx );
	}

	FORCEINLINE const float & Z(int idx) const
	{
		return SubFloat( (fltx4 &)z, idx );
	}

	FORCEINLINE float & X(int idx)
	{
		return SubFloat( x, idx );
	}

	FORCEINLINE float & Y(int idx)
	{
		return SubFloat( y, idx );
	}

	FORCEINLINE float & Z(int idx)
	{
		return SubFloat( z, idx );
	}

	FORCEINLINE Vector Vec(int idx) const						//< unpack one of the vectors
	{
		return Vector( X(idx), Y(idx), Z(idx) );
	}
	
	FORCEINLINE void operator=( FourVectors const &src )
	{
		x=src.x;
		y=src.y;
		z=src.z;
	}

	/// LoadAndSwizzle - load 4 Vectors into a FourVectors, performing transpose op
	FORCEINLINE void LoadAndSwizzle(Vector const &a, Vector const &b, Vector const &c, Vector const &d)
	{
		// TransposeSIMD has large sub-expressions that the compiler can't eliminate on x360
		// use an unfolded implementation here
#if defined( _X360 ) || defined(_PS3)
		fltx4 tx = LoadUnalignedSIMD( &a.x );
		fltx4 ty = LoadUnalignedSIMD( &b.x );
		fltx4 tz = LoadUnalignedSIMD( &c.x );
		fltx4 tw = LoadUnalignedSIMD( &d.x );
		fltx4 r0 = VectorMergeHighSIMD(tx, tz);
		fltx4 r1 = VectorMergeHighSIMD(ty, tw);
		fltx4 r2 = VectorMergeLowSIMD(tx, tz);
		fltx4 r3 = VectorMergeLowSIMD(ty, tw);

		x = VectorMergeHighSIMD(r0, r1);
		y = VectorMergeLowSIMD(r0, r1);
		z = VectorMergeHighSIMD(r2, r3);
#else
		x		= LoadUnalignedSIMD( &( a.x ));
		y		= LoadUnalignedSIMD( &( b.x ));
		z		= LoadUnalignedSIMD( &( c.x ));
		fltx4 w = LoadUnalignedSIMD( &( d.x ));
		// now, matrix is:
		// x y z ?
		// x y z ?
		// x y z ?
		// x y z ?
		TransposeSIMD(x, y, z, w);
#endif
	}

	FORCEINLINE void LoadAndSwizzle(Vector const &a)
	{
		LoadAndSwizzle( a, a, a, a );
	}

	// Broadcasts a, b, c, and d into the four vectors
	// This is only performant if the floats are ALREADY IN MEMORY
	// and not on registers -- eg, 
	// .Load( &fltArrray[0], &fltArrray[1], &fltArrray[2], &fltArrray[3] ) is okay,
	// .Load( fltArrray[0] * 0.5f,  fltArrray[1] * 0.5f,  fltArrray[2] * 0.5f,  fltArrray[3] * 0.5f ) is not.
	FORCEINLINE void Load( const float &a, const float &b, const float &c, const float &d )
	{
#if defined( _X360 ) || defined( _PS3 )
		fltx4 temp[4];
		temp[0] = LoadUnalignedFloatSIMD( &a );
		temp[1] = LoadUnalignedFloatSIMD( &b ); 
		temp[2] = LoadUnalignedFloatSIMD( &c );
		temp[3] = LoadUnalignedFloatSIMD( &d );
		y = VectorMergeHighSIMD( temp[0], temp[2] ); // ac__
		z = VectorMergeHighSIMD( temp[1], temp[3] ); // bd__

		x = VectorMergeHighSIMD( y, z ); // abcd
		y = x;
		z = x;
#else
		ALIGN16 float temp[4];
		temp[0] = a; temp[1] = b; temp[2] = c; temp[3] = d;
		fltx4 v = LoadAlignedSIMD( temp );
		x = v;
		y = v;
		z = v;
#endif
	}

	// transform four horizontal vectors into the internal vertical ones
	FORCEINLINE void LoadAndSwizzle( FLTX4 a, FLTX4 b, FLTX4 c, FLTX4 d  )
	{
#if defined( _X360 ) || defined( _PS3 )
		fltx4 tx = a;
		fltx4 ty = b;
		fltx4 tz = c;
		fltx4 tw = d;
		fltx4 r0 = VectorMergeHighSIMD(tx, tz);
		fltx4 r1 = VectorMergeHighSIMD(ty, tw);
		fltx4 r2 = VectorMergeLowSIMD(tx, tz);
		fltx4 r3 = VectorMergeLowSIMD(ty, tw);

		x = VectorMergeHighSIMD(r0, r1);
		y = VectorMergeLowSIMD(r0, r1);
		z = VectorMergeHighSIMD(r2, r3);
#else
		x		= a;
		y		= b;
		z		= c;
		fltx4 w = d;
		// now, matrix is:
		// x y z ?
		// x y z ?
		// x y z ?
		// x y z ?
		TransposeSIMD(x, y, z, w);
#endif
	}

	/// LoadAndSwizzleAligned - load 4 Vectors into a FourVectors, performing transpose op.
	/// all 4 vectors must be 128 bit boundary
	FORCEINLINE void LoadAndSwizzleAligned(const float *RESTRICT a, const float *RESTRICT b, const float *RESTRICT c, const float *RESTRICT d)
	{
#if defined( _X360 ) || defined( _PS3 )
		fltx4 tx = LoadAlignedSIMD(a);
		fltx4 ty = LoadAlignedSIMD(b);
		fltx4 tz = LoadAlignedSIMD(c);
		fltx4 tw = LoadAlignedSIMD(d);
		fltx4 r0 = VectorMergeHighSIMD(tx, tz);
		fltx4 r1 = VectorMergeHighSIMD(ty, tw);
		fltx4 r2 = VectorMergeLowSIMD(tx, tz);
		fltx4 r3 = VectorMergeLowSIMD(ty, tw);

		x = VectorMergeHighSIMD(r0, r1);
		y = VectorMergeLowSIMD(r0, r1);
		z = VectorMergeHighSIMD(r2, r3);
#else
		x		= LoadAlignedSIMD( a );
		y		= LoadAlignedSIMD( b );
		z		= LoadAlignedSIMD( c );
		fltx4 w = LoadAlignedSIMD( d );
		// now, matrix is:
		// x y z ?
		// x y z ?
		// x y z ?
		// x y z ?
		TransposeSIMD( x, y, z, w );
#endif
	}

	FORCEINLINE void LoadAndSwizzleAligned(Vector const &a, Vector const &b, Vector const &c, Vector const &d)
	{
		LoadAndSwizzleAligned( &a.x, &b.x, &c.x, &d.x );
	}

	/// Unpack a FourVectors back into four horizontal fltx4s.
	/// Since the FourVectors doesn't store a w row, you can optionally
	/// specify your own; otherwise it will be 0.
	/// This function ABSOLUTELY MUST be inlined or the reference parameters will
	/// induce a severe load-hit-store.
	FORCEINLINE void TransposeOnto( fltx4 &out0,  fltx4 &out1,  fltx4 &out2,  fltx4 &out3, FLTX4 w = Four_Zeros ) const
	{
		// TransposeSIMD has large sub-expressions that the compiler can't eliminate on x360
		// use an unfolded implementation here
#if defined( _X360 ) || defined(_PS3)
		fltx4 r0 = VectorMergeHighSIMD(x, z);
		fltx4 r1 = VectorMergeHighSIMD(y, w);
		fltx4 r2 = VectorMergeLowSIMD(x, z);
		fltx4 r3 = VectorMergeLowSIMD(y, w);

		out0 = VectorMergeHighSIMD(r0, r1);
		out1 = VectorMergeLowSIMD(r0, r1);
		out2 = VectorMergeHighSIMD(r2, r3);
		out3 = VectorMergeLowSIMD(r2, r3);
#else
		out0 = x;
		out1 = y;
		out2 = z;
		out3 = w;

		TransposeSIMD(out0, out1, out2, out3);
#endif
	}

#if !defined(__SPU__)
	/// Store a FourVectors into four NON-CONTIGUOUS Vector*'s. 
	FORCEINLINE void StoreUnalignedVector3SIMD( Vector * RESTRICT out0, Vector * RESTRICT out1, Vector * RESTRICT out2, Vector * RESTRICT out3 ) const;
#endif

	/// Store a FourVectors into four NON-CONTIGUOUS VectorAligned s. 
	FORCEINLINE void StoreAlignedVectorSIMD( VectorAligned * RESTRICT out0, VectorAligned * RESTRICT out1, VectorAligned * RESTRICT out2, VectorAligned * RESTRICT out3 ) const;

#if !defined(__SPU__)
	/// Store a FourVectors into four CONSECUTIVE Vectors in memory,
	/// where the first vector IS NOT aligned on a 16-byte boundary. 
	FORCEINLINE void StoreUnalignedContigVector3SIMD( Vector * RESTRICT pDestination )
	{
		fltx4 a,b,c,d;
		TransposeOnto(a,b,c,d);
		StoreFourUnalignedVector3SIMD( a, b, c, d, pDestination );
	}
#endif

	/// Store a FourVectors into four CONSECUTIVE Vectors in memory,
	/// where the first vector IS aligned on a 16-byte boundary. 
	/// (since four Vector3s = 48 bytes, groups of four can be said
	///  to be 16-byte aligned though obviously the 2nd, 3d, and 4th
	///  vectors in the group individually are not)
#if !defined(__SPU__)
	FORCEINLINE void StoreAlignedContigVector3SIMD( Vector * RESTRICT pDestination )
	{
		fltx4 a,b,c,d;
		TransposeOnto(a,b,c,d);
		StoreFourAlignedVector3SIMD( a, b, c, d, pDestination );
	}

	/// Store a FourVectors into four CONSECUTIVE VectorAligneds in memory
	FORCEINLINE void StoreAlignedContigVectorASIMD( VectorAligned * RESTRICT pDestination )
	{
		StoreAlignedVectorSIMD( pDestination, pDestination + 1, pDestination + 2, pDestination + 3 );
	}
#endif

	/// return the squared length of all 4 vectors, the same name as used on Vector
	FORCEINLINE fltx4 LengthSqr( void ) const
	{
		const FourVectors &a = *this;
		return a * a;
	}

	/// return the squared length of all 4 vectors
	FORCEINLINE fltx4 length2(void) const
	{
		return (*this)*(*this);
	}

	/// return the approximate length of all 4 vectors. uses the sqrt approximation instruction
	FORCEINLINE fltx4 length(void) const
	{
		return SqrtEstSIMD(length2());
	}

	/// full precision square root. upper/lower case name is an artifact - the lower case one should be changed to refelct the lower accuracy. I added the mixed case one for compat with Vector
	FORCEINLINE fltx4 Length( void ) const
	{
		return SqrtSIMD( length2() );
	}


	/// normalize all 4 vectors in place. not mega-accurate (uses reciprocal approximation instruction)
	FORCEINLINE void VectorNormalizeFast(void)
	{
		fltx4 mag_sq=(*this)*(*this);						// length^2
		(*this) *= ReciprocalSqrtEstSIMD(mag_sq);			// *(1.0/sqrt(length^2))
	}

	/// normalize all 4 vectors in place.
	FORCEINLINE void VectorNormalize(void)
	{
		fltx4 mag_sq=(*this)*(*this);						// length^2
		(*this) *= ReciprocalSqrtSIMD(mag_sq);				// *(1.0/sqrt(length^2))
	}

	FORCEINLINE fltx4 DistToSqr( FourVectors const &pnt )
	{
		fltx4 fl4dX = SubSIMD( pnt.x, x );
		fltx4 fl4dY = SubSIMD( pnt.y, y );
		fltx4 fl4dZ = SubSIMD( pnt.z, z );
		return AddSIMD( MulSIMD( fl4dX, fl4dX), AddSIMD( MulSIMD( fl4dY, fl4dY ), MulSIMD( fl4dZ, fl4dZ ) ) );

	}
	
	FORCEINLINE fltx4 TValueOfClosestPointOnLine( FourVectors const &p0, FourVectors const &p1 ) const
	{
		FourVectors lineDelta = p1;
		lineDelta -= p0;
		fltx4 OOlineDirDotlineDir = ReciprocalSIMD( p1 * p1 );
		FourVectors v4OurPnt = *this;
		v4OurPnt -= p0;
		return MulSIMD( OOlineDirDotlineDir, v4OurPnt * lineDelta );
	}

	FORCEINLINE fltx4 DistSqrToLineSegment( FourVectors const &p0, FourVectors const &p1 ) const
	{
		FourVectors lineDelta = p1;
		FourVectors v4OurPnt = *this;
		v4OurPnt -= p0;
		lineDelta -= p0;

		fltx4 OOlineDirDotlineDir = ReciprocalSIMD( lineDelta * lineDelta );

		fltx4 fl4T = MulSIMD( OOlineDirDotlineDir, v4OurPnt * lineDelta );

		fl4T = MinSIMD( fl4T, Four_Ones );
		fl4T = MaxSIMD( fl4T, Four_Zeros );
		lineDelta *= fl4T;
		return v4OurPnt.DistToSqr( lineDelta );
	}
	FORCEINLINE FourVectors Normalized()const
	{
		fltx4 fl4LengthInv = ReciprocalSqrtSIMD( LengthSqr() );
		FourVectors out;
		out.x = x * fl4LengthInv;
		out.y = y * fl4LengthInv;
		out.z = z * fl4LengthInv;
		return out;
	}

	FORCEINLINE FourVectors NormalizedSafeX() const
	{
		fltx4 f4LenSqr = LengthSqr();
		fltx4 isBigEnough = CmpGeSIMD( f4LenSqr, Four_Epsilons );
		fltx4 fl4LengthInv = ReciprocalSqrtSIMD( f4LenSqr );
		FourVectors out;
		out.x = MaskedAssign( isBigEnough, x * fl4LengthInv, Four_Ones );
		out.y = AndSIMD( y * fl4LengthInv, isBigEnough );
		out.z = AndSIMD( z * fl4LengthInv, isBigEnough );
		return out;
	}
	FORCEINLINE FourVectors NormalizedSafeY() const
	{
		fltx4 f4LenSqr = LengthSqr();
		fltx4 isBigEnough = CmpGeSIMD( f4LenSqr, Four_Epsilons );
		fltx4 fl4LengthInv = ReciprocalSqrtSIMD( f4LenSqr );
		FourVectors out;
		out.x = AndSIMD( x * fl4LengthInv, isBigEnough );
		out.y = MaskedAssign( isBigEnough, y * fl4LengthInv, Four_Ones );
		out.z = AndSIMD( z * fl4LengthInv, isBigEnough );
		return out;
	}

	FORCEINLINE FourVectors NormalizedSafeZ() const
	{
		fltx4 f4LenSqr = LengthSqr();
		fltx4 isBigEnough = CmpGeSIMD( f4LenSqr, Four_Epsilons );
		fltx4 fl4LengthInv = ReciprocalSqrtSIMD( f4LenSqr );
		FourVectors out;
		out.x = AndSIMD( x * fl4LengthInv, isBigEnough );
		out.y = AndSIMD( y * fl4LengthInv, isBigEnough );
		out.z = MaskedAssign( isBigEnough, z * fl4LengthInv, Four_Ones );
		return out;
	}
};


inline FourVectors CrossProduct( const FourVectors& a, const FourVectors& b )
{
	return FourVectors( a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x );
}

inline fltx4 DotProduct( const FourVectors& a, const FourVectors& b )
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline FourVectors operator * ( fltx4 left, const FourVectors &right )
{
	return right * left;
}


//
inline FourVectors Mul( const FourVectors &a, const fltx4 &b )	
{
	FourVectors ret;
	ret.x = MulSIMD( a.x, b );
	ret.y = MulSIMD( a.y, b );
	ret.z = MulSIMD( a.z, b );
	return ret;
}

inline FourVectors Mul( const FourVectors &a, const FourVectors &b )	
{
	FourVectors ret;
	ret.x = MulSIMD( a.x, b.x );
	ret.y = MulSIMD( a.y, b.y );
	ret.z = MulSIMD( a.z, b.z );
	return ret;
}

inline FourVectors Madd( const FourVectors &a, const fltx4 &b, const FourVectors &c )	// a*b + c
{
	FourVectors ret;
	ret.x = MaddSIMD( a.x, b, c.x );
	ret.y = MaddSIMD( a.y, b, c.y );
	ret.z = MaddSIMD( a.z, b, c.z );
	return ret;
}

/// form 4 cross products
inline FourVectors operator ^(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=SubSIMD(MulSIMD(a.y,b.z),MulSIMD(a.z,b.y));
	ret.y=SubSIMD(MulSIMD(a.z,b.x),MulSIMD(a.x,b.z));
	ret.z=SubSIMD(MulSIMD(a.x,b.y),MulSIMD(a.y,b.x));
	return ret;
}

inline FourVectors operator-(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=SubSIMD(a.x,b.x);
	ret.y=SubSIMD(a.y,b.y);
	ret.z=SubSIMD(a.z,b.z);
	return ret;
}

inline FourVectors operator+( const FourVectors &a, const FourVectors &b )
{
	FourVectors ret;
	ret.x = AddSIMD( a.x, b.x );
	ret.y = AddSIMD( a.y, b.y );
	ret.z = AddSIMD( a.z, b.z );
	return ret;
}

/// component-by-componentwise MAX operator
inline FourVectors maximum(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=MaxSIMD(a.x,b.x);
	ret.y=MaxSIMD(a.y,b.y);
	ret.z=MaxSIMD(a.z,b.z);
	return ret;
}

/// component-by-componentwise MIN operator
inline FourVectors minimum(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=MinSIMD(a.x,b.x);
	ret.y=MinSIMD(a.y,b.y);
	ret.z=MinSIMD(a.z,b.z);
	return ret;
}

FORCEINLINE FourVectors RotateLeft( const FourVectors &src )
{
	FourVectors ret;
	ret.x = RotateLeft( src.x );
	ret.y = RotateLeft( src.y );
	ret.z = RotateLeft( src.z );
	return ret;
}

FORCEINLINE FourVectors RotateRight( const FourVectors &src )
{
	FourVectors ret;
	ret.x = RotateRight( src.x );
	ret.y = RotateRight( src.y );
	ret.z = RotateRight( src.z );
	return ret;
}
FORCEINLINE FourVectors MaskedAssign( const bi32x4 & ReplacementMask, const FourVectors & NewValue, const FourVectors & OldValue )
{
	FourVectors ret;
	ret.x = MaskedAssign( ReplacementMask, NewValue.x, OldValue.x );
	ret.y = MaskedAssign( ReplacementMask, NewValue.y, OldValue.y );
	ret.z = MaskedAssign( ReplacementMask, NewValue.z, OldValue.z );
	return ret;
}

/// calculate reflection vector. incident and normal dir assumed normalized
FORCEINLINE FourVectors VectorReflect( const FourVectors &incident, const FourVectors &normal )
{
	FourVectors ret = incident;
	fltx4 iDotNx2 = incident * normal;
	iDotNx2 = AddSIMD( iDotNx2, iDotNx2 );
	FourVectors nPart = normal;
	nPart *= iDotNx2;
	ret -= nPart;											// i-2(n*i)n
	return ret;
}

/// calculate slide vector. removes all components of a vector which are perpendicular to a normal vector.
FORCEINLINE FourVectors VectorSlide( const FourVectors &incident, const FourVectors &normal )
{
	FourVectors ret = incident;
	fltx4 iDotN = incident * normal;
	FourVectors nPart = normal;
	nPart *= iDotN;
	ret -= nPart;											// i-(n*i)n
	return ret;
}

/// normalize all 4 vectors in place. not mega-accurate (uses reciprocal approximation instruction)
FORCEINLINE FourVectors VectorNormalizeFast( const FourVectors &src )
{
	fltx4 mag_sq = ReciprocalSqrtEstSIMD( src * src );					// *(1.0/sqrt(length^2))
	FourVectors result;
	result.x = MulSIMD( src.x, mag_sq );			
	result.y = MulSIMD( src.y, mag_sq );			
	result.z = MulSIMD( src.z, mag_sq );			
	return result;
}

#if !defined(__SPU__)
/// Store a FourVectors into four NON-CONTIGUOUS Vector*'s. 
FORCEINLINE void FourVectors::StoreUnalignedVector3SIMD( Vector * RESTRICT out0, Vector * RESTRICT out1, Vector * RESTRICT out2, Vector * RESTRICT out3 ) const
{
#ifdef _X360
	fltx4 x0,x1,x2,x3, y0,y1,y2,y3, z0,z1,z2,z3;
	x0 = SplatXSIMD(x); // all x0x0x0x0
	x1 = SplatYSIMD(x);
	x2 = SplatZSIMD(x);
	x3 = SplatWSIMD(x);

	y0 = SplatXSIMD(y); 
	y1 = SplatYSIMD(y);
	y2 = SplatZSIMD(y);
	y3 = SplatWSIMD(y);

	z0 = SplatXSIMD(z); 
	z1 = SplatYSIMD(z);
	z2 = SplatZSIMD(z);
	z3 = SplatWSIMD(z);

	__stvewx( x0, out0->Base(), 0 );  // store X word
	__stvewx( y0, out0->Base(), 4 );  // store Y word
	__stvewx( z0, out0->Base(), 8 );  // store Z word

	__stvewx( x1, out1->Base(), 0 );  // store X word
	__stvewx( y1, out1->Base(), 4 );  // store Y word
	__stvewx( z1, out1->Base(), 8 );  // store Z word

	__stvewx( x2, out2->Base(), 0 );  // store X word
	__stvewx( y2, out2->Base(), 4 );  // store Y word
	__stvewx( z2, out2->Base(), 8 );  // store Z word

	__stvewx( x3, out3->Base(), 0 );  // store X word
	__stvewx( y3, out3->Base(), 4 );  // store Y word
	__stvewx( z3, out3->Base(), 8 );  // store Z word
#else
	fltx4 a,b,c,d;
	TransposeOnto(a,b,c,d);
	StoreUnaligned3SIMD( out0->Base(), a );
	StoreUnaligned3SIMD( out1->Base(), b );
	StoreUnaligned3SIMD( out2->Base(), c );
	StoreUnaligned3SIMD( out3->Base(), d );
#endif
}

/// Store a FourVectors into four NON-CONTIGUOUS VectorAligned s. 
FORCEINLINE void FourVectors::StoreAlignedVectorSIMD( VectorAligned * RESTRICT out0, VectorAligned * RESTRICT out1, VectorAligned * RESTRICT out2, VectorAligned * RESTRICT out3 ) const
{
	fltx4 a,b,c,d;
	TransposeOnto(a,b,c,d);
	StoreAligned3SIMD( out0, a );
	StoreAligned3SIMD( out1, b );
	StoreAligned3SIMD( out2, c );
	StoreAligned3SIMD( out3, d );

}
#endif

#if !defined(__SPU__)
// Assume the given matrix is a rotation, and rotate these vectors by it.
// If you have a long list of FourVectors structures that you all want 
// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
void FourVectors::RotateBy(const matrix3x4_t& matrix)
{
	// Splat out each of the entries in the matrix to a fltx4. Do this
	// in the order that we will need them, to hide latency. I'm
	// avoiding making an array of them, so that they'll remain in 
	// registers.
	fltx4 matSplat00, matSplat01, matSplat02,
		matSplat10, matSplat11, matSplat12,
		matSplat20, matSplat21, matSplat22;

	// Load the matrix into local vectors. Sadly, matrix3x4_ts are 
	// often unaligned. The w components will be the tranpose row of
	// the matrix, but we don't really care about that.
	fltx4 matCol0 = LoadUnalignedSIMD( matrix[0] );
	fltx4 matCol1 = LoadUnalignedSIMD( matrix[1] );
	fltx4 matCol2 = LoadUnalignedSIMD( matrix[2] );
	
	matSplat00 = SplatXSIMD( matCol0 );
	matSplat01 = SplatYSIMD( matCol0 );
	matSplat02 = SplatZSIMD( matCol0 );

	matSplat10 = SplatXSIMD( matCol1 );
	matSplat11 = SplatYSIMD( matCol1 );
	matSplat12 = SplatZSIMD( matCol1 );
	
	matSplat20 = SplatXSIMD( matCol2 );
	matSplat21 = SplatYSIMD( matCol2 );
	matSplat22 = SplatZSIMD( matCol2 );

	// Trust in the compiler to schedule these operations correctly:
	fltx4 outX, outY, outZ;
	outX = AddSIMD( AddSIMD( MulSIMD( x, matSplat00 ), MulSIMD( y, matSplat01 ) ), MulSIMD( z, matSplat02 ) );
	outY = AddSIMD( AddSIMD( MulSIMD( x, matSplat10 ), MulSIMD( y, matSplat11 ) ), MulSIMD( z, matSplat12 ) );
	outZ = AddSIMD( AddSIMD( MulSIMD( x, matSplat20 ), MulSIMD( y, matSplat21 ) ), MulSIMD( z, matSplat22 ) );
	
	x = outX;
	y = outY;
	z = outZ;
}


// Assume the given matrix is a rotation, and rotate these vectors by it.
// If you have a long list of FourVectors structures that you all want 
// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
void FourVectors::TransformBy(const matrix3x4_t& matrix)
{
	// Splat out each of the entries in the matrix to a fltx4. Do this
	// in the order that we will need them, to hide latency. I'm
	// avoiding making an array of them, so that they'll remain in 
	// registers.
	fltx4 matSplat00, matSplat01, matSplat02,
		matSplat10, matSplat11, matSplat12,
		matSplat20, matSplat21, matSplat22;

	// Load the matrix into local vectors. Sadly, matrix3x4_ts are 
	// often unaligned. The w components will be the tranpose row of
	// the matrix, but we don't really care about that.
	fltx4 matCol0 = LoadUnalignedSIMD( matrix[0] );
	fltx4 matCol1 = LoadUnalignedSIMD( matrix[1] );
	fltx4 matCol2 = LoadUnalignedSIMD( matrix[2] );
	
	matSplat00 = SplatXSIMD( matCol0 );
	matSplat01 = SplatYSIMD( matCol0 );
	matSplat02 = SplatZSIMD( matCol0 );
	
	matSplat10 = SplatXSIMD( matCol1 );
	matSplat11 = SplatYSIMD( matCol1 );
	matSplat12 = SplatZSIMD( matCol1 );
	
	matSplat20 = SplatXSIMD( matCol2 );
	matSplat21 = SplatYSIMD( matCol2 );
	matSplat22 = SplatZSIMD( matCol2 );
	
	// Trust in the compiler to schedule these operations correctly:
	fltx4 outX, outY, outZ;
	
	outX = MaddSIMD( z, matSplat02, AddSIMD( MulSIMD( x, matSplat00 ), MulSIMD( y, matSplat01 ) ) );
	outY = MaddSIMD( z, matSplat12, AddSIMD( MulSIMD( x, matSplat10 ), MulSIMD( y, matSplat11 ) ) );
	outZ = MaddSIMD( z, matSplat22, AddSIMD( MulSIMD( x, matSplat20 ), MulSIMD( y, matSplat21 ) ) );
	
	x = AddSIMD( outX, ReplicateX4( matrix[0][3] ));
	y = AddSIMD( outY, ReplicateX4( matrix[1][3] ));
	 z = AddSIMD( outZ, ReplicateX4( matrix[2][3] ));
}
#endif

fltx4 NoiseSIMD( FourVectors const &v );

// vector valued noise direction
FourVectors DNoiseSIMD( FourVectors const &v );

// vector value "curl" noise function. see http://hyperphysics.phy-astr.gsu.edu/hbase/curl.html
FourVectors CurlNoiseSIMD( FourVectors const &v );

//#endif // !defined SPU


/// quick, low quality perlin-style noise() function suitable for real time use.
/// return value is -1..1. Only reliable around +/- 1 million or so.
fltx4 NoiseSIMD( const fltx4 & x, const fltx4 & y, const fltx4 & z );


/// calculate the absolute value of a packed single
inline fltx4 fabs( const fltx4 & x )
{
	return AndSIMD( x, LoadAlignedSIMD( g_SIMD_clear_signmask ) );
}

// Convenience version
inline fltx4 AbsSIMD( const fltx4 & x )
{
	return fabs( x );
}

/// negate all four components of a SIMD packed single
inline fltx4 fnegate( const fltx4 & x )
{
	return XorSIMD( x, LoadAlignedSIMD( g_SIMD_signmask ) );
}

fltx4 Pow_FixedPoint_Exponent_SIMD( const fltx4 & x, int exponent);

// PowSIMD - raise a SIMD register to a power.  This is analogous to the C pow() function, with some
// restictions: fractional exponents are only handled with 2 bits of precision. Basically,
// fractions of 0,.25,.5, and .75 are handled. PowSIMD(x,.30) will be the same as PowSIMD(x,.25).
// negative and fractional powers are handled by the SIMD reciprocal and square root approximation
// instructions and so are not especially accurate ----Note that this routine does not raise
// numeric exceptions because it uses SIMD--- This routine is O(log2(exponent)).
inline fltx4 PowSIMD( const fltx4 & x, float exponent )
{
	return Pow_FixedPoint_Exponent_SIMD(x,(int) (4.0*exponent));
}

///  (x<1)?x^(1/2.2):1. Use a 4th order polynomial to approximate x^(1/2.2) over 0..1
inline fltx4 LinearToGammaSIMD( fltx4 x )
{
	// y = -3.7295x4 + 8.9635x3 - 7.7397x2 + 3.443x + 0.048
	x = MaxSIMD( MinSIMD( Four_Ones, x ), Four_Zeros );
	return AddSIMD( Four_LinearToGammaCoefficients_E,
					MulSIMD( x, AddSIMD( Four_LinearToGammaCoefficients_D, 
										 MulSIMD( x, AddSIMD( Four_LinearToGammaCoefficients_C,
															  MulSIMD( x, AddSIMD( Four_LinearToGammaCoefficients_B,
																				   MulSIMD( x, Four_LinearToGammaCoefficients_A ) ) ) ) ) ) ) );
}


inline fltx4 GammaToLinearSIMD( fltx4 x )
{
	x = MaxSIMD( x, Four_Zeros );
	x = AddSIMD( Four_GammaToLinearCoefficients_D,
					MulSIMD( x, AddSIMD( Four_GammaToLinearCoefficients_C, 
										 MulSIMD( x, AddSIMD( Four_GammaToLinearCoefficients_B,
															  MulSIMD( x, Four_GammaToLinearCoefficients_A ) ) ) ) ) );
	return MinSIMD( x, Four_Ones );
}

/// ( x > 1 ) ? x : x^2.2
inline fltx4 GammaToLinearExtendedSIMD( fltx4 x )
{
	x = MaxSIMD( x, Four_Zeros );
	fltx4 fl4Ret = AddSIMD( Four_GammaToLinearCoefficients_D,
					MulSIMD( x, AddSIMD( Four_GammaToLinearCoefficients_C, 
										 MulSIMD( x, AddSIMD( Four_GammaToLinearCoefficients_B,
															  MulSIMD( x, Four_GammaToLinearCoefficients_A ) ) ) ) ) );
	return MaskedAssign( CmpGeSIMD( x, Four_Ones ), x, fl4Ret );
}

// random number generation - generate 4 random numbers quickly.

void SeedRandSIMD(uint32 seed);								// seed the random # generator
fltx4 RandSIMD( int nContext = 0 );							// return 4 numbers in the 0..1 range

// for multithreaded, you need to use these and use the argument form of RandSIMD:
int GetSIMDRandContext( void );
void ReleaseSIMDRandContext( int nContext );

FORCEINLINE fltx4 RandSignedSIMD( void )					// -1..1
{
	return SubSIMD( MulSIMD( Four_Twos, RandSIMD() ), Four_Ones );
}


FORCEINLINE fltx4 LerpSIMD ( const fltx4 &percent, const fltx4 &a, const fltx4 &b)
{
	return AddSIMD( a, MulSIMD( SubSIMD( b, a ), percent ) );
}

FORCEINLINE fltx4 RemapValClampedSIMD(const fltx4 &val, const fltx4 &a, const fltx4 &b, const fltx4 &c, const fltx4 &d) // Remap val from clamped range between a and b to new range between c and d
{
	fltx4 range = MaskedAssign( CmpEqSIMD( a, b ), Four_Ones, SubSIMD( b, a ) ); //make sure range > 0
	fltx4 cVal = MaxSIMD( Four_Zeros, MinSIMD( Four_Ones, DivSIMD( SubSIMD( val, a ), range ) ) ); //saturate
	return LerpSIMD( cVal, c, d );
}

// SIMD versions of mathlib simplespline functions
// hermite basis function for smooth interpolation
// Similar to Gain() above, but very cheap to call
// value should be between 0 & 1 inclusive
inline fltx4 SimpleSpline( const fltx4 & value )
{
	// Arranged to avoid a data dependency between these two MULs:
	fltx4 valueDoubled = MulSIMD( value, Four_Twos );
	fltx4 valueSquared = MulSIMD( value, value );

	// Nice little ease-in, ease-out spline-like curve
	return SubSIMD(
		MulSIMD( Four_Threes,  valueSquared ),
		MulSIMD( valueDoubled, valueSquared ) );
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
inline fltx4 SimpleSplineRemapValWithDeltas( const fltx4 & val,
											 const fltx4 & A, const fltx4 & BMinusA,
											 const fltx4 & OneOverBMinusA, const fltx4 & C, 
											 const fltx4 & DMinusC )
{
// 	if ( A == B )
// 		return val >= B ? D : C;
	fltx4 cVal = MulSIMD( SubSIMD( val, A), OneOverBMinusA );
	return AddSIMD( C, MulSIMD( DMinusC, SimpleSpline( cVal ) ) );
}

inline fltx4 SimpleSplineRemapValWithDeltasClamped( const fltx4 & val,
													const fltx4 & A, const fltx4 & BMinusA,
													const fltx4 & OneOverBMinusA, const fltx4 & C, 
													const fltx4 & DMinusC )
{
// 	if ( A == B )
// 		return val >= B ? D : C;
	fltx4 cVal = MulSIMD( SubSIMD( val, A), OneOverBMinusA );
	cVal = MinSIMD( Four_Ones, MaxSIMD( Four_Zeros, cVal ) );
	return AddSIMD( C, MulSIMD( DMinusC, SimpleSpline( cVal ) ) );
}

FORCEINLINE fltx4 FracSIMD( const fltx4 &val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 ival = SubSIMD( AddSIMD( fl4Abs, Four_2ToThe23s ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, fl4Abs ), SubSIMD( ival, Four_Ones ), ival );
	return XorSIMD( SubSIMD( fl4Abs, ival ), XorSIMD( val, fl4Abs ) );			// restore sign bits
}

#ifndef SPU
// Disable on SPU for the moment as it generates a warning
// warning: dereferencing type-punned pointer will break strict-aliasing rules
// This is related to LoadAlignedSIMD( (float *) g_SIMD_lsbmask )
// LoadAlignedSIMD() under the hood is dereferencing the variable.
FORCEINLINE fltx4 Mod2SIMD( const fltx4 &val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 ival = SubSIMD( AndSIMD( LoadAlignedSIMD( (float *) g_SIMD_lsbmask ), AddSIMD( fl4Abs, Four_2ToThe23s ) ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, fl4Abs ), SubSIMD( ival, Four_Twos ), ival );
	return XorSIMD( SubSIMD( fl4Abs, ival ), XorSIMD( val, fl4Abs ) );			// restore sign bits
}
#endif

FORCEINLINE fltx4 Mod2SIMDPositiveInput( const fltx4 &val )
{
	fltx4 ival = SubSIMD( AndSIMD( LoadAlignedSIMD( g_SIMD_lsbmask ), AddSIMD( val, Four_2ToThe23s ) ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, val ), SubSIMD( ival, Four_Twos ), ival );
	return SubSIMD( val, ival );
}


// approximate sin of an angle, with -1..1 representing the whole sin wave period instead of -pi..pi.
// no range reduction is done - for values outside of 0..1 you won't like the results
FORCEINLINE fltx4 _SinEst01SIMD( const fltx4 &val )
{
	// really rough approximation - x*(4-x*4) - a parabola. s(0) = 0, s(.5) = 1, s(1)=0, smooth in-between.
	// sufficient for simple oscillation.
	return MulSIMD( val, SubSIMD( Four_Fours, MulSIMD( val, Four_Fours ) ) );
}

FORCEINLINE fltx4 _Sin01SIMD( const fltx4 &val )
{
	// not a bad approximation : parabola always over-estimates. Squared parabola always
	// underestimates. So lets blend between them:  goodsin = badsin + .225*( badsin^2-badsin)
	fltx4 fl4BadEst = MulSIMD( val, SubSIMD( Four_Fours, MulSIMD( val, Four_Fours ) ) );
	return AddSIMD( MulSIMD( Four_Point225s, SubSIMD( MulSIMD( fl4BadEst, fl4BadEst ), fl4BadEst ) ), fl4BadEst );
}

// full range useable implementations
FORCEINLINE fltx4 SinEst01SIMD( const fltx4 &val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 fl4Reduced2 = Mod2SIMDPositiveInput( fl4Abs );
	bi32x4 fl4OddMask = CmpGeSIMD( fl4Reduced2, Four_Ones );
	fltx4 fl4val = SubSIMD( fl4Reduced2, AndSIMD( Four_Ones, fl4OddMask ) );
	fltx4 fl4Sin = _SinEst01SIMD( fl4val );
	fl4Sin = XorSIMD( fl4Sin, AndSIMD( LoadAlignedSIMD( g_SIMD_signmask ), XorSIMD( val, fl4OddMask ) ) );
	return fl4Sin;

}

FORCEINLINE fltx4 Sin01SIMD( const fltx4 &val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 fl4Reduced2 = Mod2SIMDPositiveInput( fl4Abs );
	bi32x4 fl4OddMask = CmpGeSIMD( fl4Reduced2, Four_Ones );
	fltx4 fl4val = SubSIMD( fl4Reduced2, AndSIMD( Four_Ones, fl4OddMask ) );
	fltx4 fl4Sin = _Sin01SIMD( fl4val );
	fl4Sin = XorSIMD( fl4Sin, AndSIMD( LoadAlignedSIMD( g_SIMD_signmask ), XorSIMD( val, fl4OddMask ) ) );
	return fl4Sin;

}

FORCEINLINE fltx4 NatExpSIMD( const fltx4 &val )			// why is ExpSimd( x ) defined to be 2^x?
{
	// need to write this. just stub with normal float implementation for now
	fltx4 fl4Result;
	SubFloat( fl4Result, 0 ) = exp( SubFloat( val, 0 ) );
	SubFloat( fl4Result, 1 ) = exp( SubFloat( val, 1 ) );
	SubFloat( fl4Result, 2 ) = exp( SubFloat( val, 2 ) );
	SubFloat( fl4Result, 3 ) = exp( SubFloat( val, 3 ) );
	return fl4Result;
}

// Schlick style Bias approximation see graphics gems 4 : bias(t,a)= t/( (1/a-2)*(1-t)+1)
 
FORCEINLINE fltx4 PreCalcBiasParameter( const fltx4 &bias_parameter )
{
	// convert perlin-style-bias parameter to the value right for the approximation
	return SubSIMD( ReciprocalSIMD( bias_parameter ), Four_Twos );
}

FORCEINLINE fltx4 BiasSIMD( const fltx4 &val, const fltx4 &precalc_param )
{
	// similar to bias function except pass precalced bias value from calling PreCalcBiasParameter.

	//!!speed!! use reciprocal est?
	//!!speed!! could save one op by precalcing _2_ values
	return DivSIMD( val, AddSIMD( MulSIMD( precalc_param, SubSIMD( Four_Ones, val ) ), Four_Ones ) );
}

//-----------------------------------------------------------------------------
// Box/plane test 
// NOTE: The w component of emins + emaxs must be 1 for this to work
//-----------------------------------------------------------------------------

#ifndef SPU
// We don't need this on SPU right now

FORCEINLINE int BoxOnPlaneSideSIMD( const fltx4& emins, const fltx4& emaxs, const cplane_t *p, float tolerance = 0.f )
{
	fltx4 corners[2];
	fltx4 normal = LoadUnalignedSIMD( p->normal.Base() );
	fltx4 dist = ReplicateX4( -p->dist );
	normal = SetWSIMD( normal, dist );
	fltx4 t4 = ReplicateX4( tolerance );
	fltx4 negt4 = ReplicateX4( -tolerance );
	bi32x4 cmp = CmpGeSIMD( normal, Four_Zeros );
	corners[0] = MaskedAssign( cmp, emaxs, emins );
	corners[1] = MaskedAssign( cmp, emins, emaxs );
	fltx4 dot1 = Dot4SIMD( normal, corners[0] );
	fltx4 dot2 = Dot4SIMD( normal, corners[1] );
	cmp = CmpGeSIMD( dot1, t4 );
	bi32x4 cmp2 = CmpGtSIMD( negt4, dot2 );
	fltx4 result = MaskedAssign( cmp, Four_Ones, Four_Zeros );
	fltx4 result2 = MaskedAssign( cmp2, Four_Twos, Four_Zeros );
	result = AddSIMD( result, result2 );
	intx4 sides;
	ConvertStoreAsIntsSIMD( &sides, result );
	return sides[0];
}


// k-dop bounding volume. 26-dop bounds with 13 plane-pairs plus 3 other "arbitrary bounds". The arbitrary values could be used to hold type info, etc,
// which can compare against "for free"
class KDop32_t
{
public:
	fltx4 m_Mins[4];
	fltx4 m_Maxes[4];

	FORCEINLINE bool Intersects( KDop32_t const &other ) const;

	FORCEINLINE void operator|=( KDop32_t const & other );

	FORCEINLINE bool IsEmpty( void ) const;

	FORCEINLINE void Init( void )
	{
		for( int i = 0; i < ARRAYSIZE( m_Mins ); i++ )
		{
			m_Mins[i] = Four_FLT_MAX;
			m_Maxes[i] = Four_Negative_FLT_MAX;
		}
	}

	// given a set of points, expand the kdop to contain them
	void AddPointSet( Vector const *pPoints, int nPnts );

	void CreateFromPointSet( Vector const *pPoints, int nPnts );
};

FORCEINLINE void KDop32_t::operator|=( KDop32_t const & other )
{
	m_Mins[0] = MinSIMD( m_Mins[0], other.m_Mins[0] );
	m_Mins[1] = MinSIMD( m_Mins[1], other.m_Mins[1] );
	m_Mins[2] = MinSIMD( m_Mins[2], other.m_Mins[2] );
	m_Mins[3] = MinSIMD( m_Mins[3], other.m_Mins[3] );

	m_Maxes[0] = MaxSIMD( m_Maxes[0], other.m_Maxes[0] );
	m_Maxes[1] = MaxSIMD( m_Maxes[1], other.m_Maxes[1] );
	m_Maxes[2] = MaxSIMD( m_Maxes[2], other.m_Maxes[2] );
	m_Maxes[3] = MaxSIMD( m_Maxes[3], other.m_Maxes[3] );


}

FORCEINLINE bool KDop32_t::Intersects( KDop32_t const &other ) const
{
	bi32x4 c00 = CmpLeSIMD( m_Mins[0], other.m_Maxes[0] );
	bi32x4 c01 = CmpLeSIMD( m_Mins[1], other.m_Maxes[1] );
	bi32x4 c02 = CmpLeSIMD( m_Mins[2], other.m_Maxes[2] );
	bi32x4 c03 = CmpLeSIMD( m_Mins[3], other.m_Maxes[3] );

	bi32x4 c10 = CmpGeSIMD( m_Maxes[0], other.m_Mins[0] );
	bi32x4 c11 = CmpGeSIMD( m_Maxes[1], other.m_Mins[1] );
	bi32x4 c12 = CmpGeSIMD( m_Maxes[2], other.m_Mins[2] );
	bi32x4 c13 = CmpGeSIMD( m_Maxes[3], other.m_Mins[3] );
	
	bi32x4 a0 = AndSIMD( AndSIMD( c00, c01 ), AndSIMD( c02, c03 ) );
	bi32x4 a1 = AndSIMD( AndSIMD( c10, c11 ), AndSIMD( c12, c13 ) );
	
	return ! ( IsAnyZeros( AndSIMD( a1, a0 ) ) );
}


FORCEINLINE bool KDop32_t::IsEmpty( void ) const
{
	bi32x4 c00 = CmpLtSIMD( m_Maxes[0], m_Mins[0] );
	bi32x4 c01 = CmpLtSIMD( m_Maxes[1], m_Mins[1] );
	bi32x4 c02 = CmpLtSIMD( m_Maxes[2], m_Mins[2] );
	bi32x4 c03 = CmpLtSIMD( m_Maxes[3], m_Mins[3] );

	return IsAnyTrue( OrSIMD( OrSIMD( c00, c01 ), OrSIMD( c02, c03 ) ) );
}


extern const fltx4 g_KDop32XDirs[4];
extern const fltx4 g_KDop32YDirs[4];
extern const fltx4 g_KDop32ZDirs[4];
#endif

#if 0

// FIXME!!!  If we need a version of this that runs on 360, this is a work-in-progress version that hasn't been debugged.

#define _VEC_SWIZZLE_QUAT48_UNPACK (__vector unsigned char)		{ 16, 17, 0, 1, 16, 17, 2, 3, 16, 17, 4, 5, 16, 17, 6, 7 }
#define _VEC_SWIZZLE_QUAT48_UNPACK_SHIFT (__vector unsigned int )		{ 0, 0, 1, 0 }

// unpack a single Quaternion48 at the pointer into the x,y,z,w components of a fltx4
FORCEINLINE fltx4 UnpackQuaternion48SIMD( const Quaternion48 * RESTRICT pVec )
{
	// A quaternion 48 stores the x and y components as 0..65535 , which is almost mapped onto -1.0..1.0 via (x - 32768) / 32768.5 .
	// z is stored as 0..32767, which is almost mapped onto -1..1 via (z - 16384) / 16384.5 .
	// w is inferred from 1 - the dot product of the other tree components. the top bit of what would otherwise be the 16-bit z is
	// w's sign bit.
//	fltx4 q16s = XMLoadVector3((const void *)pVec);
	fltx4 q16s = LoadUnaligned3SIMD( (const float * )pVec);

//	fltx4 shift = *( fltx4 * )&g_SIMD_Quat48_Unpack_Shift; // load the aligned shift mask that we use to shuffle z.
//	fltx4 permute = *( fltx4 * )&g_SIMD_Quat48_Unpack_Permute0; // load the permute word that shuffles x,y,z into their own words
	bool wneg = pVec->wneg; // loading pVec into two different kinds of registers -- but not shuffling between (I hope!) so no LHS.

	//	q16s = __vperm( q16s, Four_Threes, permute ); // permute so that x, y, and z are now each in their own words. The top half is the floating point rep of 3.0f
	q16s = vec_perm( q16s, Four_Threes, _VEC_SWIZZLE_QUAT48_UNPACK ); // permute so that x, y, and z are now each in their own words. The top half is the floating point rep of 3.0f

	//	q16s = __vslh(q16s, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
//	q16s = vec_sl( *( u32x4 * )( void * )( &q16s ), _VEC_SWIZZLE_QUAT48_UNPACK_SHIFT ); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	u32x4 tmp = IntShiftLeftWordSIMD( *( u32x4 * )&q16s, _VEC_SWIZZLE_QUAT48_UNPACK_SHIFT );
	q16s = *( fltx4 * )&tmp;

	// each word of q16s contains 3.0 + n * 2^-22 -- convert this so that we get numbers on the range -1..1
	const fltx4 vUpkMul = SplatXSIMD(g_SIMD_Quat48_Unpack_Magic_Constants); // { UnpackMul16s, UnpackMul16s, UnpackMul16s, UnpackMul16s };
	const fltx4 vUpkAdd = SplatYSIMD(g_SIMD_Quat48_Unpack_Magic_Constants);

	/*
	fltx4 ret = __vcfux( q16s, 0 ); // convert from uint16 to floats.

	// scale from 0..65535 to -1..1 : tmp.x = ((int)x - 32768) * (1 / 32768.0);
	ret = __vmaddfp( ret, g_SIMD_Quat48_DivByU15, Four_NegativeOnes  );
	*/
//	fltx4 ret = __vmaddfp( q16s, vUpkMul, vUpkAdd );
	fltx4 ret = vec_madd( q16s, vUpkMul, vUpkAdd );

	// now, work out what w must be. 
	fltx4 dotxyz = Dot3SIMD( ret, ret ); // all components are dot product of ret w/ self.
	dotxyz = ClampVectorSIMD( dotxyz, Four_Zeros, Four_Ones );

	fltx4 ww = SubSIMD( Four_Ones, dotxyz ); // all components are 1 - dotxyz
	ww = SqrtSIMD(ww); // all components are sqrt(1-dotxyz)
	if ( wneg )
	{
		ret = SetWSIMD( ret, NegSIMD( ww ) );
//		ret = __vrlimi( ret, NegSIMD(ww), 1, 0 ); // insert one element from the ww vector into the w component of ret
	}
	else
	{
		ret = SetWSIMD( ret, ww );
//		ret = __vrlimi( ret, ww, 1, 0 ); // insert one element from the ww vector into the w component of ret
	}
	return ret;
}

#endif

// These are not optimized right now for some platforms. We should be able to shuffle the values in some platforms.
// As the methods are hard-coded we can actually avoid loading memory to do the transfer.
// We should be able to create all versions.
FORCEINLINE fltx4 SetWFromXSIMD( const fltx4 & a, const fltx4 & x )
{
	fltx4 value = SplatXSIMD( x );
	return SetWSIMD( a, value );
}

FORCEINLINE fltx4 SetWFromYSIMD( const fltx4 & a, const fltx4 & y )
{
	fltx4 value = SplatYSIMD( y );
	return SetWSIMD( a, value );
}

FORCEINLINE fltx4 SetWFromZSIMD( const fltx4 & a, const fltx4 & z )
{
	fltx4 value = SplatZSIMD( z );
	return SetWSIMD( a, value );
}

FORCEINLINE fltx4 CrossProductSIMD( const fltx4 &A, const fltx4 &B )
{
#if defined( _X360 )
	return XMVector3Cross( A, B );
#elif defined( _WIN32 )
	fltx4 A1 = _mm_shuffle_ps( A, A, MM_SHUFFLE_REV( 1, 2, 0, 3 ) );
	fltx4 B1 = _mm_shuffle_ps( B, B, MM_SHUFFLE_REV( 2, 0, 1, 3 ) );
	fltx4 Result1 = MulSIMD( A1, B1 );
	fltx4 A2 = _mm_shuffle_ps( A, A, MM_SHUFFLE_REV( 2, 0, 1, 3 ) );
	fltx4 B2 = _mm_shuffle_ps( B, B, MM_SHUFFLE_REV( 1, 2, 0, 3 ) );
	fltx4 Result2 = MulSIMD( A2, B2 );
	return SubSIMD( Result1, Result2 );

#elif defined(_PS3)
	/*
	fltx4 perm1 = (vector unsigned char){0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x00,0x01,0x02,0x03,0x0c,0x0d,0x0e,0x0f};
	fltx4 perm2 = (vector unsigned char){0x08,0x09,0x0a,0x0b,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x0c,0x0d,0x0e,0x0f};

	fltx4 A1 = __vpermwi( A, A, perm1 );
	fltx4 A2 = __vpermwi( B, B, perm2 );
	fltx4 Result1 = MulSIMD( A1, B1 );
	fltx4 A2 = __vpermwi( A, A, perm2 );
	fltx4 B2 = __vpermwi( B, B, perm1 );
	return MsubSIMD( A2, B2, Result1 );
	*/
	return _vmathVfCross( A, B );
#else
	fltx4 CrossVal;
	SubFloat( CrossVal, 0 ) = SubFloat( A, 1 )*SubFloat( B, 2 ) - SubFloat( A, 2 )*SubFloat( B, 1 );
	SubFloat( CrossVal, 1 ) = SubFloat( A, 2 )*SubFloat( B, 0 ) - SubFloat( A, 0 )*SubFloat( B, 2 );
	SubFloat( CrossVal, 2 ) = SubFloat( A, 0 )*SubFloat( B, 1 ) - SubFloat( A, 1 )*SubFloat( B, 0 );
	SubFloat( CrossVal, 3 ) = 0;
	return CrossVal;
#endif
}

inline const fltx4 Length3SIMD(const fltx4 vec)
{
	fltx4 scLengthSqr = Dot3SIMD(vec,vec);
	bi32x4 isSignificant = CmpGtSIMD(scLengthSqr, Four_Epsilons);
	fltx4 scLengthInv = ReciprocalSqrtSIMD(scLengthSqr);
	return AndSIMD(isSignificant, MulSIMD(scLengthInv, scLengthSqr));
}

inline const fltx4 Normalized3SIMD (const fltx4 vec)
{
	fltx4 scLengthSqr = Dot3SIMD(vec,vec);
	bi32x4 isSignificant = CmpGtSIMD(scLengthSqr, Four_Epsilons);
	fltx4 scLengthInv = ReciprocalSqrtSIMD(scLengthSqr);
	return AndSIMD(isSignificant, MulSIMD(vec, scLengthInv));
}


// Some convenience operator overloads, which are just aliasing the functions above.
// Unneccessary on 360, as you already have them from xboxmath.h
// Componentwise add
#ifndef COMPILER_GCC

FORCEINLINE fltx4 operator+=( fltx4 &a, FLTX4 b )
{
	a = AddSIMD( a, b );
	return a;
}

FORCEINLINE fltx4 operator-=( fltx4 &a, FLTX4 b )
{
	a = SubSIMD( a, b );
	return a;
}


FORCEINLINE fltx4 operator*=( fltx4 &a, FLTX4 b )
{
	a = MulSIMD( a, b );
	return a;
}

#endif
#endif // _ssemath_h

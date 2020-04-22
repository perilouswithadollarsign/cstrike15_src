//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of our SIMD functions for the x86 using SSE
//==============================================================//


#ifndef _MATH_PFNS_H_
#include "mathlib/math_pfns.h"
#endif

#if defined( PLATFORM_WINDOWS_PC )
#include <intrin.h>
#else
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
 

//---------------------------------------------------------------------
// Intel/SSE implementation
//---------------------------------------------------------------------

FORCEINLINE void StoreAlignedSIMD( float * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_store_ps( pSIMD, a );
}

FORCEINLINE void StoreUnalignedSIMD( float * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_storeu_ps( pSIMD, a );
}

FORCEINLINE void StoreUnalignedSIMD( int * RESTRICT pSIMD, const i32x4 &a )
{
	_mm_storeu_si128( ( __m128i * ) pSIMD, a );
}


FORCEINLINE fltx4 RotateLeft( const fltx4 & a );
FORCEINLINE fltx4 RotateLeft2( const fltx4 & a );

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	_mm_store_ss(pSIMD, a);
	_mm_store_ss(pSIMD+1, RotateLeft(a));
	_mm_store_ss(pSIMD+2, RotateLeft2(a));
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

FORCEINLINE i32x4 AndSIMD( const i32x4 &a, const i32x4 &b )
{
	return _mm_and_si128( a, b );
}

FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// ~a & b
{
	return _mm_andnot_ps( a, b );
}

FORCEINLINE i32x4 AndNotSIMD( const i32x4 & a, const i32x4 & b )			// ~a & b
{
	return _mm_andnot_si128( a, b );
}


FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
	return _mm_xor_ps( a, b );
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
	return _mm_or_ps( a, b );
}

FORCEINLINE i32x4 OrSIMD( const i32x4 &a, const i32x4 &b )
{
	return _mm_or_si128( a, b );
}


// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	return AndSIMD( a, LoadAlignedSIMD( g_SIMD_clear_wmask ) );
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

FORCEINLINE fltx4 CastToFltx4( i32x4 const & a )
{
	return _mm_castsi128_ps( a );
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE i32x4 ReplicateIX4( int i )
{
	return _mm_set1_epi32( i );
}


FORCEINLINE fltx4 ReplicateX4( float flValue )
{
	__m128 value = _mm_set_ss( flValue );
	return _mm_shuffle_ps( value, value, 0 );
}

// AltiVec compilers may have trouble inlining pass-by-value variant of ReplicateX4, whereas 
// they will have absolutely no problem inlining pass-by-pointer variant. So it's better to use
// the pass-by-pointer variant unless you're mixing scalar and vector code (which is bad for perf on AltiVec anyway)
FORCEINLINE fltx4 ReplicateX4( const float *pValue )
{
	return ReplicateX4( *pValue );
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

FORCEINLINE uint32 SubInt( i32x4 const & a, int idx )
{
#ifndef POSIX
	return a.m128i_u32[idx];
#else
	return (reinterpret_cast<const uint32 *>(&a))[idx];
#endif
}

FORCEINLINE uint32 & SubInt( i32x4 & a, int idx )
{
#ifndef POSIX
	return a.m128i_u32[idx];
#else
	return (reinterpret_cast<uint32 *>(&a))[idx];
#endif
}

// gather from array. Indices are in units of float size
FORCEINLINE fltx4 GatherFltX4SIMD( float const *pData, i32x4 n4Indices )
{
	fltx4 fl4Ret;
	SubFloat( fl4Ret, 0 ) = pData[SubInt(n4Indices,0)];
	SubFloat( fl4Ret, 1 ) = pData[SubInt(n4Indices,1)];
	SubFloat( fl4Ret, 2 ) = pData[SubInt(n4Indices,2)];
	SubFloat( fl4Ret, 3 ) = pData[SubInt(n4Indices,3)];
	return fl4Ret;

}

// gather from array. Indices are in units of float size
FORCEINLINE fltx4 GatherFltX4SIMD( fltx4 const *pData, i32x4 n4Indices )
{
	return GatherFltX4SIMD( ( float const * ) pData, n4Indices );

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

/// Set one component of a SIMD word with the given float value. 
/// This function is a template because the native implementation of
/// this on PPC platforms requires that the component be given as a 
/// compiler immediate -- not a function parameter, not a const function
/// parameter, not even a load from a const static array. It has to be
/// a real immediate. 
/// \param NCOMPONENT 0 is x, 1 is y, 2 is z, 3 is w.
/// \note This function is not particularly performant on any platform (because of 
///       the load from float), so prefer a masked assign from a fltx4 wherever
///       possible. 
template < unsigned int NCOMPONENT >
FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, float flValue )
{
	fltx4 val = ReplicateX4( flValue );
	fltx4 result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[NCOMPONENT] ), val, a );
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

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	// FIXME: Make a fast SSE version
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
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

/// [ a.x+a.y a.z+a.w b.x+b.y b.z+b.w ] from sse3
FORCEINLINE fltx4 PairwiseHorizontalAddSIMD( const fltx4 &a, const fltx4 &b )
{
	return _mm_hadd_ps( a, b );
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


FORCEINLINE fltx4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
	return _mm_cmpeq_ps( a, b );
}

FORCEINLINE fltx4 CmpEqSIMD( const i32x4 & a, const i32x4 & b )				// (a==b) ? ~0:0   for 32 bit ints.fltx4 result.
{
	return _mm_castsi128_ps( _mm_cmpeq_epi32( a, b ) );
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

FORCEINLINE fltx4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? 1.0 : 0
{
	return AndSIMD( CmpLeSIMD(a,b), CmpGeSIMD(a, NegSIMD(b)) ) ;
}

FORCEINLINE fltx4 Cmp01EqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? 1.0:0
{
	return AndSIMD( Four_Ones, _mm_cmpeq_ps( a, b ) );
}

FORCEINLINE fltx4 Cmp01GtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? 1.0:0
{
	return AndSIMD( Four_Ones, _mm_cmpgt_ps( a, b ) );
}

FORCEINLINE fltx4 Cmp01GeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? 1.0:0
{
	return AndSIMD( Four_Ones, _mm_cmpge_ps( a, b ) );
}

FORCEINLINE fltx4 Cmp01LtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? 1.0:0
{
	return AndSIMD( Four_Ones, _mm_cmplt_ps( a, b ) );
}

FORCEINLINE fltx4 Cmp01LeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? 1.0:0
{
	return AndSIMD( Four_Ones, _mm_cmple_ps( a, b ) );
}

FORCEINLINE fltx4 Cmp01InBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? 1.0 : 0
{
	return AndSIMD( Four_Ones, AndSIMD( CmpLeSIMD(a,b), CmpGeSIMD(a, NegSIMD(b)) ) );
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

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton iteration.
/// No error checking!
FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )					// 1/a
{
	fltx4 ret = ReciprocalEstSIMD( a );
	// newton iteration is: Y(n+1) = 2*Y(n)-a*Y(n)^2
	ret = SubSIMD( AddSIMD( ret, ret ), MulSIMD( a, MulSIMD( ret, ret ) ) );
	return ret;
}

// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
FORCEINLINE fltx4 PowerOfTwoSIMD( const fltx4 &toPower )
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
	return _mm_load_si128( reinterpret_cast<const __m128i *>(pSIMD) );
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD( const void * RESTRICT pSIMD)
{
	return _mm_loadu_si128( reinterpret_cast<const __m128i *>(pSIMD) );
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

FORCEINLINE void StoreAlignedSIMD( short * RESTRICT pSIMD, const shortx8 & a )
{
	_mm_store_si128( (shortx8 *)pSIMD, a );
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 * RESTRICT pSIMD, const fltx4 & a )
{
	_mm_storeu_ps( reinterpret_cast<float *>(pSIMD), a );
}

FORCEINLINE void StoreUnalignedSIMD( short * RESTRICT pSIMD, const shortx8 & a )
{
	_mm_storeu_si128( (shortx8 *)pSIMD, a );
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


// Convert the 4 32-bit integers to single precison floats.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	return  _mm_cvtepi32_ps( (const __m128i &)vSrcA );
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


// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing 
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way 
// to guarantee an immediate as a parameter to function
// like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
#if defined( COMPILER_MSVC64 )

	(*pDest)[0] = SubFloat(vSrc, 0);
	(*pDest)[1] = SubFloat(vSrc, 1);
	(*pDest)[2] = SubFloat(vSrc, 2);
	(*pDest)[3] = SubFloat(vSrc, 3);

#else

	__m64 bottom = _mm_cvttps_pi32( vSrc );
	__m64 top    = _mm_cvttps_pi32( _mm_movehl_ps(vSrc,vSrc) );

	*reinterpret_cast<__m64 *>(&(*pDest)[0]) = bottom;
	*reinterpret_cast<__m64 *>(&(*pDest)[2]) = top;

	_mm_empty();

#endif
}


// some sse2 packed integer intrinsic wrappers
#if _MSC_VER >= 1600 || defined(LINUX) || defined(OSX)
/// replicate an 16 bit integer value to all 8 16-bit positions in an fltx4
FORCEINLINE fltx4 ReplicateWordX8( uint16 nWord )
{
	return _mm_castsi128_ps( _mm_set_epi16( nWord, nWord, nWord, nWord, nWord, nWord, nWord, nWord ) );

}
/// Return a 16-bit mask consiting of the upper bit of each of the bytes in the input
FORCEINLINE int TestSignsOfBytesSIMD( fltx4 const &packedBytes )
{
	return _mm_movemask_epi8( _mm_castps_si128( packedBytes ) );
}

/// compare each 16-bit field of a word for equality
FORCEINLINE fltx4 CmpEqWordsSIMD( fltx4 const &flIn, fltx4 const &flValue )
{
	return _mm_castsi128_ps( _mm_cmpeq_epi16( _mm_castps_si128( flIn ), _mm_castps_si128( flValue ) ) );
}

/// grab 16 16-bit signed words from two fltx4s, and pack them into one register holding 16 bytes converted from them
FORCEINLINE fltx4 PackSignedWordsToBytesWithSaturateSIMD( fltx4 const &packedWorlds0, fltx4 const &packedWorlds1 )
{
	return _mm_castsi128_ps( _mm_packs_epi16( _mm_castps_si128( packedWorlds0 ), _mm_castps_si128( packedWorlds1 ) ) );
}


FORCEINLINE fltx4 CrossProduct3SIMD( const fltx4 &v1, const fltx4 &v2 )
{
	fltx4 v1_yzxx = _mm_shuffle_ps( v1, v1, MM_SHUFFLE_REV( 1,2,0,0 ) );
	fltx4 v2_zxyy = _mm_shuffle_ps( v2, v2, MM_SHUFFLE_REV( 2,0,1,0 ) );
	fltx4 v1_zxyy = _mm_shuffle_ps( v1, v1, MM_SHUFFLE_REV( 2,0,1,0 ) );
	fltx4 v2_yzxx = _mm_shuffle_ps( v2, v2, MM_SHUFFLE_REV( 1,2,0,0 ) );
	return SubSIMD( MulSIMD( v1_yzxx, v2_zxyy ), MulSIMD( v1_zxyy, v2_yzxx ) );
}




#endif

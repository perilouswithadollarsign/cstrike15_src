//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of our SIMD functions for the 360.
//==============================================================//


#ifndef DBG_H
#include "tier0/dbg.h"
#endif


//---------------------------------------------------------------------
// X360 implementation
//---------------------------------------------------------------------

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

/// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterR(&cr,a,b);
	return XMComparisonAllTrue(cr);
}

/// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterOrEqualR(&cr,a,b);
	return XMComparisonAllTrue(cr);
}

/// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAnyGreaterThan( const fltx4 &a, const fltx4 &b )
{
	unsigned int cr;
	XMVectorGreaterR(&cr,a,b);
	return XMComparisonAnyTrue(cr);
}

/// for branching when a.xyzw >= b.xyzw
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

FORCEINLINE fltx4 Cmp01EqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? 1.0:0
{
    return AndSIMD( Four_Ones, __vcmpeqfp( a, b ) );
}


FORCEINLINE fltx4 Cmp01GtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? 1.0:0
{
    return AndSIMD( Four_Ones, __vcmpgtfp( a, b ) );
}

FORCEINLINE fltx4 Cmp01GeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? 1.0:0
{
    return AndSIMD( Four_Ones, __vcmpgefp( a, b ) );
}

FORCEINLINE fltx4 Cmp01LtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? 1.0:0
{
    return AndSIMD( Four_Ones, __vcmpgtfp( b, a ) );
}

FORCEINLINE fltx4 Cmp01LeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? 1.0:0
{
    return AndSIMD( Four_Ones, __vcmpgefp( b, a ) );
}

FORCEINLINE fltx4 Cmp01InBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? 1.0 : 0
{
	return AndSIMD( Four_Ones, XMVectorInBounds( a, b ) );
}


// returned[i] = ReplacementMask[i] == 0 ? OldValue : NewValue
FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
    return __vsel( OldValue, NewValue, ReplacementMask );
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

// FIXME: on 360, this is very slow, since it uses ReciprocalSIMD (do we need DivEstSIMD?)
FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )	// a/b
{
	return MulSIMD( ReciprocalSIMD( b ), a );
}


// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
FORCEINLINE fltx4 PowerOfTwoSIMD( const fltx4 &toPower )
{
	return XMVectorExp(toPower);
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


FORCEINLINE shortx8 LoadAlignedShortSIMD( const void *pSIMD )
{
	return XMLoadVector4A( pSIMD );
}

FORCEINLINE shortx8 LoadUnalignedShortSIMD( const void *pSIMD )
{
	return XMLoadVector4( pSIMD );
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
	// vrlimi can only take an immediate opcode -- that is a constant
	// passed in from the compiler, not a function parameter, nor an 
	// element loaded from an array, not even a const static array.
#define SETCOMPONENTSIMD_MASK_IMMEDIATE ( NCOMPONENT == 0 ) ? 8 :\
										( NCOMPONENT == 1 ) ? 4 :\
										( NCOMPONENT == 2 ) ? 2 :\
										( NCOMPONENT == 3 ) ? 1 :\
										17 //< a meaningless immediate intended to make the compiler angry

	fltx4 val = ReplicateX4( flValue );
	fltx4 result = __vrlimi(a, val, SETCOMPONENTSIMD_MASK_IMMEDIATE, 0);
	return result;

#undef SETCOMPONENTSIMD_MASK_IMMEDIATE
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

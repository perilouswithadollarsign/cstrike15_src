//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: - defines SIMD "structure of arrays" classes and functions.
//
//===========================================================================//
#ifndef SSEQUATMATH_H
#define SSEQUATMATH_H

#ifdef _WIN32
#pragma once
#endif


#include "mathlib/ssemath.h"

// Use this #define to allow SSE versions of Quaternion math
// to exist on PC.
// On PC, certain horizontal vector operations are not supported.
// This causes the SSE implementation of quaternion math to mix the
// vector and scalar floating point units, which is extremely 
// performance negative if you don't compile to native SSE2 (which 
// we don't as of Sept 1, 2007). So, it's best not to allow these
// functions to exist at all. It's not good enough to simply replace
// the contents of the functions with scalar math, because each call
// to LoadAligned and StoreAligned will result in an unnecssary copy
// of the quaternion, and several moves to and from the XMM registers.
//
// Basically, the problem you run into is that for efficient SIMD code,
// you need to load the quaternions and vectors into SIMD registers and
// keep them there as long as possible while doing only SIMD math,
// whereas for efficient scalar code, each time you copy onto or ever
// use a fltx4, it hoses your pipeline. So the difference has to be
// in the management of temporary variables in the calling function,
// not inside the math functions.
//
// If you compile assuming the presence of SSE2, the MSVC will abandon
// the traditional x87 FPU operations altogether and make everything use
// the SSE2 registers, which lessens this problem a little.

// permitted only on 360, as we've done careful tuning on its Altivec math.
// FourQuaternions, however, are always allowed, because vertical ops are
// fine on SSE.
#ifdef PLATFORM_PPC
#define ALLOW_SIMD_QUATERNION_MATH 1  // not on PC!
#endif



//---------------------------------------------------------------------
// Load/store quaternions
//---------------------------------------------------------------------
#ifndef _X360
// Using STDC or SSE
FORCEINLINE fltx4 LoadAlignedSIMD( const QuaternionAligned & pSIMD )
{
	fltx4 retval = LoadAlignedSIMD( pSIMD.Base() );
	return retval;
}

FORCEINLINE fltx4 LoadAlignedSIMD( const QuaternionAligned * RESTRICT pSIMD )
{
	fltx4 retval = LoadAlignedSIMD( pSIMD->Base() );
	return retval;
}

FORCEINLINE void StoreAlignedSIMD( QuaternionAligned * RESTRICT pSIMD, const fltx4 & a )
{
	StoreAlignedSIMD( pSIMD->Base(), a );
}
#else

// for the transitional class -- load a QuaternionAligned
FORCEINLINE fltx4 LoadAlignedSIMD( const QuaternionAligned & pSIMD )
{
	fltx4 retval = XMLoadVector4A( pSIMD.Base() );
	return retval;
}

FORCEINLINE fltx4 LoadAlignedSIMD( const QuaternionAligned * RESTRICT pSIMD )
{
	fltx4 retval = XMLoadVector4A( pSIMD );
	return retval;
}

FORCEINLINE void StoreAlignedSIMD( QuaternionAligned * RESTRICT pSIMD, const fltx4 & a )
{
	XMStoreVector4A( pSIMD->Base(), a );
}

// From a RadianEuler packed onto a fltx4, to a quaternion
fltx4 AngleQuaternionSIMD( FLTX4 vAngles );

#endif


#if ALLOW_SIMD_QUATERNION_MATH
//---------------------------------------------------------------------
// Make sure quaternions are within 180 degrees of one another, if not, reverse q
//---------------------------------------------------------------------
FORCEINLINE fltx4 QuaternionAlignSIMD( const fltx4 &p, const fltx4 &q )
{
	// decide if one of the quaternions is backwards
	fltx4 a = SubSIMD( p, q );
	fltx4 b = AddSIMD( p, q );
	a = Dot4SIMD( a, a );
	b = Dot4SIMD( b, b );
	fltx4 cmp = (fltx4) CmpGtSIMD( a, b );
	fltx4 result = MaskedAssign( cmp, NegSIMD(q), q );
	return result;
}

//---------------------------------------------------------------------
// Normalize Quaternion
//---------------------------------------------------------------------
#if USE_STDC_FOR_SIMD

FORCEINLINE fltx4 QuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius, result;
	radius = Dot4SIMD( q, q );

	if ( SubFloat( radius, 0 ) ) // > FLT_EPSILON && ((radius < 1.0f - 4*FLT_EPSILON) || (radius > 1.0f + 4*FLT_EPSILON))
	{
		float iradius = 1.0f / sqrt( SubFloat( radius, 0 ) );
		result = ReplicateX4( iradius );
		result = MulSIMD( result, q );
		return result;
	}
	return q;
}

#else

// SSE + X360 implementation
FORCEINLINE fltx4 QuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius, result, mask;
	radius = Dot4SIMD( q, q );
	mask = (fltx4) CmpEqSIMD( radius, Four_Zeros ); // all ones iff radius = 0
	result = ReciprocalSqrtSIMD( radius );
	result = MulSIMD( result, q );
	return MaskedAssign( mask, q, result );	// if radius was 0, just return q
}

#endif


//---------------------------------------------------------------------
// 0.0 returns p, 1.0 return q.
//---------------------------------------------------------------------
FORCEINLINE fltx4 QuaternionBlendNoAlignSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	fltx4 sclp, sclq, result;
	sclq = ReplicateX4( t );
	sclp = SubSIMD( Four_Ones, sclq );
	result = MulSIMD( sclp, p );
	result = MaddSIMD( sclq, q, result );
	return QuaternionNormalizeSIMD( result );
}


//---------------------------------------------------------------------
// Blend Quaternions
//---------------------------------------------------------------------
FORCEINLINE fltx4 QuaternionBlendSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	// decide if one of the quaternions is backwards
	fltx4 q2, result;
	q2 = QuaternionAlignSIMD( p, q );
	result = QuaternionBlendNoAlignSIMD( p, q2, t );
	return result;
}


//---------------------------------------------------------------------
// Multiply Quaternions
//---------------------------------------------------------------------
#ifndef _X360

// SSE and STDC
FORCEINLINE fltx4 QuaternionMultSIMD( const fltx4 &p, const fltx4 &q )
{
	// decide if one of the quaternions is backwards
	fltx4 q2, result;
	q2 = QuaternionAlignSIMD( p, q );
	SubFloat( result, 0 ) =  SubFloat( p, 0 ) * SubFloat( q2, 3 ) + SubFloat( p, 1 ) * SubFloat( q2, 2 ) - SubFloat( p, 2 ) * SubFloat( q2, 1 ) + SubFloat( p, 3 ) * SubFloat( q2, 0 );
	SubFloat( result, 1 ) = -SubFloat( p, 0 ) * SubFloat( q2, 2 ) + SubFloat( p, 1 ) * SubFloat( q2, 3 ) + SubFloat( p, 2 ) * SubFloat( q2, 0 ) + SubFloat( p, 3 ) * SubFloat( q2, 1 );
	SubFloat( result, 2 ) =  SubFloat( p, 0 ) * SubFloat( q2, 1 ) - SubFloat( p, 1 ) * SubFloat( q2, 0 ) + SubFloat( p, 2 ) * SubFloat( q2, 3 ) + SubFloat( p, 3 ) * SubFloat( q2, 2 );
	SubFloat( result, 3 ) = -SubFloat( p, 0 ) * SubFloat( q2, 0 ) - SubFloat( p, 1 ) * SubFloat( q2, 1 ) - SubFloat( p, 2 ) * SubFloat( q2, 2 ) + SubFloat( p, 3 ) * SubFloat( q2, 3 );
	return result;
}

#else 

// X360
extern const fltx4 g_QuatMultRowSign[4];
FORCEINLINE fltx4 QuaternionMultSIMD( const fltx4 &p, const fltx4 &q )
{
	fltx4 q2, row, result;
	q2 = QuaternionAlignSIMD( p, q );

	row = XMVectorSwizzle( q2, 3, 2, 1, 0 );
	row = MulSIMD( row, g_QuatMultRowSign[0] );
	result = Dot4SIMD( row, p );

	row = XMVectorSwizzle( q2, 2, 3, 0, 1 );
	row = MulSIMD( row, g_QuatMultRowSign[1] );
	row = Dot4SIMD( row, p );
	result = __vrlimi( result, row, 4, 0 );
	
	row = XMVectorSwizzle( q2, 1, 0, 3, 2 );
	row = MulSIMD( row, g_QuatMultRowSign[2] );
	row = Dot4SIMD( row, p );
	result = __vrlimi( result, row, 2, 0 );
	
	row = MulSIMD( q2, g_QuatMultRowSign[3] );
	row = Dot4SIMD( row, p );
	result = __vrlimi( result, row, 1, 0 );
	return result;
}

#endif


//---------------------------------------------------------------------
// Quaternion scale
//---------------------------------------------------------------------
#ifdef _X360

// X360
FORCEINLINE fltx4 QuaternionScaleSIMD( const fltx4 &p, float t )
{
	fltx4 sinom = Dot3SIMD( p, p );
	sinom = SqrtSIMD( sinom );
	sinom = MinSIMD( sinom, Four_Ones );
	fltx4 sinsom = ArcSinSIMD( sinom );
	fltx4 t4 = ReplicateX4( t );
	sinsom = MulSIMD( sinsom, t4 );
	sinsom = SinSIMD( sinsom );
	sinom = AddSIMD( sinom, Four_Epsilons );
	sinom = ReciprocalSIMD( sinom );
	t4 = MulSIMD( sinsom, sinom );
	fltx4 result = MulSIMD( p, t4 );

	// rescale rotation
	sinsom = MulSIMD( sinsom, sinsom );
	fltx4 r = SubSIMD( Four_Ones, sinsom );
	r = MaxSIMD( r, Four_Zeros );
	r = SqrtSIMD( r );

	// keep sign of rotation
	fltx4 cmp = CmpGeSIMD( p, Four_Zeros );
	r = MaskedAssign( cmp, r, NegSIMD( r ) );

	result = __vrlimi(result, r, 1, 0);
	return result;
}

// X360
// assumes t4 contains a float replicated to each slot
FORCEINLINE fltx4 QuaternionScaleSIMD( const fltx4 &p, const fltx4 &t4 )
{
	fltx4 sinom = Dot3SIMD( p, p );
	sinom = SqrtSIMD( sinom );
	sinom = MinSIMD( sinom, Four_Ones );
	fltx4 sinsom = ArcSinSIMD( sinom );
	sinsom = MulSIMD( sinsom, t4 );
	sinsom = SinSIMD( sinsom );
	sinom = AddSIMD( sinom, Four_Epsilons );
	sinom = ReciprocalSIMD( sinom );
	fltx4 result = MulSIMD( p, MulSIMD( sinsom, sinom ) );

	// rescale rotation
	sinsom = MulSIMD( sinsom, sinsom );
	fltx4 r = SubSIMD( Four_Ones, sinsom );
	r = MaxSIMD( r, Four_Zeros );
	r = SqrtSIMD( r );

	// keep sign of rotation
	fltx4 cmp = CmpGeSIMD( p, Four_Zeros );
	r = MaskedAssign( cmp, r, NegSIMD( r ) );

	result = __vrlimi(result, r, 1, 0);
	return result;
}

#elif defined(_PS3)

// X360
FORCEINLINE fltx4 QuaternionScaleSIMD( const fltx4 &p, float t )
{
	fltx4 sinom = Dot3SIMD( p, p );
	sinom = SqrtSIMD( sinom );
	sinom = MinSIMD( sinom, Four_Ones );
	fltx4 sinsom = ArcSinSIMD( sinom );
	fltx4 t4 = ReplicateX4( t );
	sinsom = MulSIMD( sinsom, t4 );
	sinsom = SinSIMD( sinsom );
	sinom = AddSIMD( sinom, Four_Epsilons );
	sinom = ReciprocalSIMD( sinom );
	t4 = MulSIMD( sinsom, sinom );
	fltx4 result = MulSIMD( p, t4 );

	// rescale rotation
	sinsom = MulSIMD( sinsom, sinsom );
	fltx4 r = SubSIMD( Four_Ones, sinsom );
	r = MaxSIMD( r, Four_Zeros );
	r = SqrtSIMD( r );

	// keep sign of rotation
	r = MaskedAssign( CmpGeSIMD( p, Four_Zeros ), r, NegSIMD( r ) );
	// set just the w component of result
	result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[3] ), r, result );

	return result;
}

// X360
// assumes t4 contains a float replicated to each slot
FORCEINLINE fltx4 QuaternionScaleSIMD( const fltx4 &p, const fltx4 &t4 )
{
	fltx4 sinom = Dot3SIMD( p, p );
	sinom = SqrtSIMD( sinom );
	sinom = MinSIMD( sinom, Four_Ones );
	fltx4 sinsom = ArcSinSIMD( sinom );
	sinsom = MulSIMD( sinsom, t4 );
	sinsom = SinSIMD( sinsom );
	sinom = AddSIMD( sinom, Four_Epsilons );
	sinom = ReciprocalSIMD( sinom );
	fltx4 result = MulSIMD( p, MulSIMD( sinsom, sinom ) );

	// rescale rotation
	sinsom = MulSIMD( sinsom, sinsom );
	fltx4 r = SubSIMD( Four_Ones, sinsom );
	r = MaxSIMD( r, Four_Zeros );
	r = SqrtSIMD( r );

	// keep sign of rotation
	r = MaskedAssign( CmpGeSIMD( p, Four_Zeros ), r, NegSIMD( r ) );
	// set just the w component of result
	result = MaskedAssign( LoadAlignedSIMD( g_SIMD_ComponentMask[3] ), r, result );

	return result;
}

#else

// SSE and STDC
FORCEINLINE fltx4 QuaternionScaleSIMD( const fltx4 &p, float t )
{
	float r;
	fltx4 q;

	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to 
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
	float sinom = sqrt( SubFloat( p, 0 ) * SubFloat( p, 0 ) + SubFloat( p, 1 ) * SubFloat( p, 1 ) + SubFloat( p, 2 ) * SubFloat( p, 2 ) );
	sinom = fmin( sinom, 1.f );

	float sinsom = sin( asin( sinom ) * t );

	t = sinsom / (sinom + FLT_EPSILON);
	SubFloat( q, 0 ) = t * SubFloat( p, 0 );
	SubFloat( q, 1 ) = t * SubFloat( p, 1 );
	SubFloat( q, 2 ) = t * SubFloat( p, 2 );

	// rescale rotation
	r = 1.0f - sinsom * sinsom;

	// Assert( r >= 0 );
	if (r < 0.0f) 
		r = 0.0f;
	r = sqrt( r );

	// keep sign of rotation
	SubFloat( q, 3 ) = fsel( SubFloat( p, 3 ), r, -r );
	return q;
}

#endif


//-----------------------------------------------------------------------------
// Quaternion sphereical linear interpolation
//-----------------------------------------------------------------------------
#ifndef _X360

// SSE and STDC
FORCEINLINE fltx4 QuaternionSlerpNoAlignSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	float omega, cosom, sinom, sclp, sclq;

	fltx4 result;

	// 0.0 returns p, 1.0 return q.
	cosom = SubFloat( p, 0 ) * SubFloat( q, 0 ) + SubFloat( p, 1 ) * SubFloat( q, 1 ) + 
		SubFloat( p, 2 ) * SubFloat( q, 2 ) + SubFloat( p, 3 ) * SubFloat( q, 3 );

	if ( (1.0f + cosom ) > 0.000001f ) 
	{
		if ( (1.0f - cosom ) > 0.000001f ) 
		{
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0f - t)*omega) / sinom;
			sclq = sin( t*omega ) / sinom;
		}
		else 
		{
			// TODO: add short circuit for cosom == 1.0f?
			sclp = 1.0f - t;
			sclq = t;
		}
		SubFloat( result, 0 ) = sclp * SubFloat( p, 0 ) + sclq * SubFloat( q, 0 );
		SubFloat( result, 1 ) = sclp * SubFloat( p, 1 ) + sclq * SubFloat( q, 1 );
		SubFloat( result, 2 ) = sclp * SubFloat( p, 2 ) + sclq * SubFloat( q, 2 );
		SubFloat( result, 3 ) = sclp * SubFloat( p, 3 ) + sclq * SubFloat( q, 3 );
	}
	else 
	{
		SubFloat( result, 0 ) = -SubFloat( q, 1 );
		SubFloat( result, 1 ) =  SubFloat( q, 0 );
		SubFloat( result, 2 ) = -SubFloat( q, 3 );
		SubFloat( result, 3 ) =  SubFloat( q, 2 );
		sclp = sin( (1.0f - t) * (0.5f * M_PI));
		sclq = sin( t * (0.5f * M_PI));
		SubFloat( result, 0 ) = sclp * SubFloat( p, 0 ) + sclq * SubFloat( result, 0 );
		SubFloat( result, 1 ) = sclp * SubFloat( p, 1 ) + sclq * SubFloat( result, 1 );
		SubFloat( result, 2 ) = sclp * SubFloat( p, 2 ) + sclq * SubFloat( result, 2 );
	}

	return result;
}

#else

// X360
FORCEINLINE fltx4 QuaternionSlerpNoAlignSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	return XMQuaternionSlerp( p, q, t );
}

#endif


FORCEINLINE fltx4 QuaternionSlerpSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	fltx4 q2, result;
	q2 = QuaternionAlignSIMD( p, q );
	result = QuaternionSlerpNoAlignSIMD( p, q2, t );
	return result;
}


#endif // ALLOW_SIMD_QUATERNION_MATH


/// class FourVectors stores 4 independent vectors for use in SIMD processing. These vectors are
/// stored in the format x x x x y y y y z z z z so that they can be efficiently SIMD-accelerated.
class ALIGN16 FourQuaternions
{
public:
	fltx4 x,y,z,w;

	FourQuaternions(void)
	{
	}

	FourQuaternions( const fltx4 &_x, 
					 const fltx4 &_y, 
					 const fltx4 &_z, 
					 const fltx4 &_w )
					 : x(_x), y(_y), z(_z), w(_w)
	{}

#if !defined(__SPU__)
	// four rotations around the same axis. angles should be in radians.
	FourQuaternions ( const fltx4 &axis, 
		const float &angle0, const float &angle1, const float &angle2, const float &angle3)
	{
		FromAxisAndAngles( axis, angle0, angle1, angle2, angle3 );
	}
#endif

	FourQuaternions( FourQuaternions const &src )
	{
		x=src.x;
		y=src.y;
		z=src.z;
		w=src.w;
	}

	FORCEINLINE void operator=( FourQuaternions const &src )
	{
		x=src.x;
		y=src.y;
		z=src.z;
		w=src.w;
	}

	/// this = this * q;
	FORCEINLINE FourQuaternions Mul( FourQuaternions const &q ) const;

	/// negate the vector part
	FORCEINLINE FourQuaternions Conjugate() const;

	/// for a quaternion representing a rotation of angle theta, return
	/// one of angle s*theta
	/// scale is four floats -- one for each quat
	FORCEINLINE FourQuaternions ScaleAngle( const fltx4 &scale ) const;

	/// ret = this * ( s * q ) 
	/// In other words, for a quaternion representing a rotation of angle theta, return
	/// one of angle s*theta
	/// s is four floats in a fltx4 -- one for each quaternion
	FORCEINLINE FourQuaternions MulAc( const fltx4 &s, const FourQuaternions &q ) const;

	/// ret = ( s * this ) * q
	FORCEINLINE FourQuaternions ScaleMul( const fltx4 &s, const FourQuaternions &q ) const;

	/// Slerp four quaternions at once, FROM me TO the specified out.
	FORCEINLINE FourQuaternions Slerp( const FourQuaternions &to, const fltx4 &t );

	FORCEINLINE FourQuaternions SlerpNoAlign( const FourQuaternions &originalto, const fltx4 &t );

#if !defined(__SPU__)
	/// given an axis and four angles, populate this quaternion with the equivalent rotations
	/// (ie, make these four quaternions represent four different rotations around the same axis)
	/// angles should be in RADIANS
	FORCEINLINE FourQuaternions &FromAxisAndAngles( const fltx4 &axis, 
		const float &angle0, const float &angle1,	const float &angle2, const float &angle3 );
	FORCEINLINE FourQuaternions &FromAxisAndAngles( const fltx4 &axis, const fltx4 &angles );
	// one convenience imp if you're doing this in degrees
	FORCEINLINE FourQuaternions &FromAxisAndAnglesInDegrees( const fltx4 &axis, const fltx4 &angles )
	{
		return FromAxisAndAngles( axis, MulSIMD(angles, Four_DegToRad));
	}
#endif

	// rotate (in place) a FourVectors by this quaternion. there's a corresponding RotateBy in FourVectors.
	FORCEINLINE void RotateFourVectors( FourVectors * RESTRICT vecs ) const RESTRICT ;


	/// LoadAndSwizzleAligned - load 4 QuaternionAligneds into a FourQuaternions, performing transpose op.
	/// all 4 vectors must be 128 bit boundary
	FORCEINLINE void LoadAndSwizzleAligned(const float *RESTRICT a, const float *RESTRICT b, const float *RESTRICT c, const float *RESTRICT d)
	{
#if defined( _X360 )
		fltx4 tx = LoadAlignedSIMD(a);
		fltx4 ty = LoadAlignedSIMD(b);
		fltx4 tz = LoadAlignedSIMD(c);
		fltx4 tw = LoadAlignedSIMD(d);
		fltx4 r0 = __vmrghw(tx, tz);
		fltx4 r1 = __vmrghw(ty, tw);
		fltx4 r2 = __vmrglw(tx, tz);
		fltx4 r3 = __vmrglw(ty, tw);

		x = __vmrghw(r0, r1);
		y = __vmrglw(r0, r1);
		z = __vmrghw(r2, r3);
		w = __vmrglw(r2, r3);
#else
		x		= LoadAlignedSIMD(a);
		y		= LoadAlignedSIMD(b);
		z		= LoadAlignedSIMD(c);
		w		= LoadAlignedSIMD(d);
		// now, matrix is:
		// x y z w
		// x y z w
		// x y z w
		// x y z w
		TransposeSIMD(x, y, z, w);
#endif
	}

	FORCEINLINE void LoadAndSwizzleAligned(const QuaternionAligned * RESTRICT a, 
		const QuaternionAligned * RESTRICT b, 
		const QuaternionAligned * RESTRICT c, 
		const QuaternionAligned * RESTRICT d)
	{
		LoadAndSwizzleAligned(a->Base(), b->Base(), c->Base(), d->Base() );
	}


	/// LoadAndSwizzleAligned - load 4 consecutive QuaternionAligneds into a FourQuaternions, 
	/// performing transpose op.
	/// all 4 vectors must be 128 bit boundary
	FORCEINLINE void LoadAndSwizzleAligned(const QuaternionAligned *qs)
	{
#if defined( _X360 )
		fltx4 tx = LoadAlignedSIMD(qs++);
		fltx4 ty = LoadAlignedSIMD(qs++);
		fltx4 tz = LoadAlignedSIMD(qs++);
		fltx4 tw = LoadAlignedSIMD(qs);
		fltx4 r0 = __vmrghw(tx, tz);
		fltx4 r1 = __vmrghw(ty, tw);
		fltx4 r2 = __vmrglw(tx, tz);
		fltx4 r3 = __vmrglw(ty, tw);

		x = __vmrghw(r0, r1);
		y = __vmrglw(r0, r1);
		z = __vmrghw(r2, r3);
		w = __vmrglw(r2, r3);
#else
		x		= LoadAlignedSIMD(qs++);
		y		= LoadAlignedSIMD(qs++);
		z		= LoadAlignedSIMD(qs++);
		w		= LoadAlignedSIMD(qs++);
		// now, matrix is:
		// x y z w
		// x y z w
		// x y z w
		// x y z w
		TransposeSIMD(x, y, z, w);
#endif
	}

	// Store the FourQuaternions out to four nonconsecutive ordinary quaternions in memory.
	FORCEINLINE void SwizzleAndStoreAligned(QuaternionAligned *a, QuaternionAligned *b, QuaternionAligned *c, QuaternionAligned *d)
	{
#if defined( _X360 )
		fltx4 r0 = __vmrghw(x, z);
		fltx4 r1 = __vmrghw(y, w);
		fltx4 r2 = __vmrglw(x, z);
		fltx4 r3 = __vmrglw(y, w);

		fltx4 rx = __vmrghw(r0, r1);
		fltx4 ry = __vmrglw(r0, r1);
		fltx4 rz = __vmrghw(r2, r3);
		fltx4 rw = __vmrglw(r2, r3);

		StoreAlignedSIMD(a, rx);
		StoreAlignedSIMD(b, ry);
		StoreAlignedSIMD(c, rz);
		StoreAlignedSIMD(d, rw);
#else
		fltx4 dupes[4] = { x, y, z, w };
		TransposeSIMD(dupes[0], dupes[1], dupes[2], dupes[3]);
		StoreAlignedSIMD(a, dupes[0]);
		StoreAlignedSIMD(b, dupes[1]);
		StoreAlignedSIMD(c, dupes[2]);
		StoreAlignedSIMD(d, dupes[3]);
#endif
	}

	// Store the FourQuaternions out to four consecutive ordinary quaternions in memory.
	FORCEINLINE void SwizzleAndStoreAligned(QuaternionAligned *qs)
	{
#if defined( _X360 )
		fltx4 r0 = __vmrghw(x, z);
		fltx4 r1 = __vmrghw(y, w);
		fltx4 r2 = __vmrglw(x, z);
		fltx4 r3 = __vmrglw(y, w);

		fltx4 rx = __vmrghw(r0, r1);
		fltx4 ry = __vmrglw(r0, r1);
		fltx4 rz = __vmrghw(r2, r3);
		fltx4 rw = __vmrglw(r2, r3);

		StoreAlignedSIMD(qs, rx);
		StoreAlignedSIMD(++qs, ry);
		StoreAlignedSIMD(++qs, rz);
		StoreAlignedSIMD(++qs, rw);
#else
		SwizzleAndStoreAligned(qs, qs+1, qs+2, qs+3);
#endif
	}

	// Store the FourQuaternions out to four consecutive ordinary quaternions in memory.
	// The mask specifies which of the quaternions are actually written out -- each	
	// word in the fltx4 should be all binary ones or zeros. Ones means the corresponding
	// quat will be written.
	FORCEINLINE void SwizzleAndStoreAlignedMasked(QuaternionAligned * RESTRICT qs, const bi32x4 &controlMask)
	{
		fltx4 originals[4];
		originals[0] = LoadAlignedSIMD(qs);
		originals[1] = LoadAlignedSIMD(qs+1);
		originals[2] = LoadAlignedSIMD(qs+2);
		originals[3] = LoadAlignedSIMD(qs+3);

		bi32x4 masks[4] = { SplatXSIMD(controlMask),
			SplatYSIMD(controlMask),
			SplatZSIMD(controlMask),
			SplatWSIMD(controlMask)	};

#if defined( _X360 )
		fltx4 r0 = __vmrghw(x, z);
		fltx4 r1 = __vmrghw(y, w);
		fltx4 r2 = __vmrglw(x, z);
		fltx4 r3 = __vmrglw(y, w);

		fltx4 rx = __vmrghw(r0, r1);
		fltx4 ry = __vmrglw(r0, r1);
		fltx4 rz = __vmrghw(r2, r3);
		fltx4 rw = __vmrglw(r2, r3);
#else
		fltx4 rx = x;
		fltx4 ry = y;
		fltx4 rz = z;
		fltx4 rw = w;
		TransposeSIMD( rx, ry, rz, rw );
#endif

		StoreAlignedSIMD( qs+0, MaskedAssign(masks[0], rx, originals[0]));
		StoreAlignedSIMD( qs+1, MaskedAssign(masks[1], ry, originals[1]));
		StoreAlignedSIMD( qs+2, MaskedAssign(masks[2], rz, originals[2]));
		StoreAlignedSIMD( qs+3, MaskedAssign(masks[3], rw, originals[3]));
	}
};



FORCEINLINE FourQuaternions FourQuaternions::Conjugate(  ) const
{
	return FourQuaternions( NegSIMD(x),  NegSIMD(y),  NegSIMD(z),  w );
}




FORCEINLINE const fltx4 Dot(const FourQuaternions &a, const FourQuaternions &b)
{
	return
		MaddSIMD(a.x, b.x,
			MaddSIMD(a.y, b.y,
				MaddSIMD(a.z,b.z, MulSIMD(a.w,b.w))
			)
		);
}


FORCEINLINE const FourQuaternions Madd(const FourQuaternions &a, const fltx4 &scale, const FourQuaternions &c)
{
	FourQuaternions ret;
	ret.x = MaddSIMD(a.x,scale,c.x);
	ret.y = MaddSIMD(a.y,scale,c.y);
	ret.z = MaddSIMD(a.z,scale,c.z);
	ret.w = MaddSIMD(a.w,scale,c.w);
	return ret;
}

FORCEINLINE const FourQuaternions Mul(const FourQuaternions &a, const fltx4 &scale)
{
	FourQuaternions ret;
	ret.x = MulSIMD(a.x,scale);
	ret.y = MulSIMD(a.y,scale);
	ret.z = MulSIMD(a.z,scale);
	ret.w = MulSIMD(a.w,scale);
	return ret;
}

FORCEINLINE const FourQuaternions Add(const FourQuaternions &a,const FourQuaternions &b)
{
	FourQuaternions ret;
	ret.x = AddSIMD(a.x,b.x);
	ret.y = AddSIMD(a.y,b.y);
	ret.z = AddSIMD(a.z,b.z);
	ret.w = AddSIMD(a.w,b.w);
	return ret;
}

FORCEINLINE const FourQuaternions Sub(const FourQuaternions &a,const FourQuaternions &b)
{
	FourQuaternions ret;
	ret.x = SubSIMD(a.x,b.x);
	ret.y = SubSIMD(a.y,b.y);
	ret.z = SubSIMD(a.z,b.z);
	ret.w = SubSIMD(a.w,b.w);
	return ret;
}

FORCEINLINE const FourQuaternions Neg(const FourQuaternions &q)
{
	FourQuaternions ret;
	ret.x = NegSIMD(q.x);
	ret.y = NegSIMD(q.y);
	ret.z = NegSIMD(q.z);
	ret.w = NegSIMD(q.w);
	return ret;
}

FORCEINLINE const FourQuaternions MaskedAssign(const bi32x4 &mask, const FourQuaternions &a, const FourQuaternions &b)
{
	FourQuaternions ret;
	ret.x = MaskedAssign(mask,a.x,b.x);
	ret.y = MaskedAssign(mask,a.y,b.y);
	ret.z = MaskedAssign(mask,a.z,b.z);
	ret.w = MaskedAssign(mask,a.w,b.w);
	return ret;
}

#ifdef DIFFERENT_NATIVE_VECTOR_TYPES
FORCEINLINE const FourQuaternions MaskedAssign(const fltx4 &mask, const FourQuaternions &a, const FourQuaternions &b)
{
	return MaskedAssign( ( bi32x4 )mask, a, b );
}
#endif


FORCEINLINE FourQuaternions QuaternionAlign( const FourQuaternions &p, const FourQuaternions &q )
{
	// decide if one of the quaternions is backwards
	bi32x4 cmp = CmpLtSIMD( Dot(p,q), Four_Zeros );
	return MaskedAssign( cmp, Neg(q), q );
}


FORCEINLINE const FourQuaternions QuaternionNormalize( const FourQuaternions &q )
{
	fltx4 radius = Dot( q, q );
	bi32x4 mask = CmpEqSIMD( radius, Four_Zeros ); // all ones iff radius = 0
	fltx4 invRadius = ReciprocalSqrtSIMD( radius );
	
	FourQuaternions ret = MaskedAssign(mask, q, Mul(q, invRadius));
	return ret;
}


#if !defined(__SPU__)
FORCEINLINE FourQuaternions &FourQuaternions::FromAxisAndAngles( const fltx4 &axis, 
											   const float &angle0, const float &angle1,	const float &angle2, const float &angle3 )
{
	return FromAxisAndAngles( axis, LoadGatherSIMD(angle0,angle1,angle2,angle3) );
}

FORCEINLINE FourQuaternions &FourQuaternions::FromAxisAndAngles( const fltx4 &axis, 
																const fltx4 &angles )
{
	// compute the half theta 
	fltx4 theta = MulSIMD( angles, Four_PointFives );
	// compute the sine and cosine of each angle simultaneously
	fltx4 vsines; fltx4 vcoses;
	SinCosSIMD( vsines, vcoses, theta );
	// now the sines and coses vectors contain the results for four angles.
	// for each of the angles, splat them out and then swizzle together so
	// as to get a < cos, sin, sin, sin > coefficient vector

	x = MulSIMD( vsines, SplatXSIMD( axis ) ); // sin(t0) * x, sin(t1) * x, etc 
	y = MulSIMD( vsines, SplatYSIMD( axis ) );
	z = MulSIMD( vsines, SplatZSIMD( axis ) );
	w = vcoses;


	return *this;
}
#endif


/// this = this * q;
FORCEINLINE FourQuaternions FourQuaternions::Mul( FourQuaternions const &q ) const
{
	// W = w1w2 - x1x2 - y1y2 - z1z2
	FourQuaternions ret;
	fltx4 signMask = LoadAlignedSIMD( (float *) g_SIMD_signmask ); 
	// as we do the multiplication, also do a dot product, so we know whether
	// one of the quats is backwards and if we therefore have to negate at the end
	fltx4 dotProduct = MulSIMD( w, q.w );

	ret.w = MulSIMD( w, q.w ); // W = w1w2
	ret.x = MulSIMD( w, q.x ); // X = w1x2
	ret.y = MulSIMD( w, q.y ); // Y = w1y2
	ret.z = MulSIMD( w, q.z ); // Z = w1z2

	dotProduct = MaddSIMD( x, q.x, dotProduct );
	ret.w = MsubSIMD( x, q.x, ret.w ); // W = w1w2 - x1x2
	ret.x = MaddSIMD( x, q.w, ret.x ); // X = w1x2 + x1w2
	ret.y = MsubSIMD( x, q.z, ret.y ); // Y = w1y2 - x1z2
	ret.z = MaddSIMD( x, q.y, ret.z ); // Z = w1z2 + x1y2

	dotProduct = MaddSIMD( y, q.y, dotProduct );
	ret.w = MsubSIMD( y, q.y, ret.w ); // W = w1w2 - x1x2 - y1y2
	ret.x = MaddSIMD( y, q.z, ret.x ); // X = w1x2 + x1w2 + y1z2
	ret.y = MaddSIMD( y, q.w, ret.y ); // Y = w1y2 - x1z2 + y1w2
	ret.z = MsubSIMD( y, q.x, ret.z ); // Z = w1z2 + x1y2 - y1x2

	dotProduct = MaddSIMD( z, q.z, dotProduct );
	ret.w = MsubSIMD( z, q.z, ret.w ); // W = w1w2 - x1x2 - y1y2 - z1z2
	ret.x = MsubSIMD( z, q.y, ret.x ); // X = w1x2 + x1w2 + y1z2 - z1y2
	ret.y = MaddSIMD( z, q.x, ret.y ); // Y = w1y2 - x1z2 + y1w2 + z1x2
	ret.z = MaddSIMD( z, q.w, ret.z ); // Z = w1z2 + x1y2 - y1x2 + z1w2

	fltx4 Zero = Four_Zeros;
	bi32x4 control = CmpLtSIMD( dotProduct, Four_Zeros );
	signMask = MaskedAssign(control, signMask, Zero); // negate quats where q1.q2 < 0
	ret.w = XorSIMD( signMask, ret.w );
	ret.x = XorSIMD( signMask, ret.x );
	ret.y = XorSIMD( signMask, ret.y );
	ret.z = XorSIMD( signMask, ret.z );

	return ret;
}


FORCEINLINE void FourQuaternions::RotateFourVectors( FourVectors * RESTRICT vecs ) const RESTRICT
{
	fltx4 tmpX, tmpY, tmpZ, tmpW;
	fltx4 outX, outY, outZ;

	tmpX = SubSIMD( MaddSIMD( w, vecs->x , MulSIMD( y, vecs->z ) ), 
					MulSIMD( z, vecs->y ) );

	tmpY = SubSIMD( MaddSIMD( w, vecs->y, MulSIMD( z, vecs->x ) ), 
					MulSIMD( x, vecs->z ) );

	tmpZ = SubSIMD( MaddSIMD( w, vecs->z, MulSIMD( x, vecs->y ) ),
					MulSIMD( y, vecs->x ) );

	tmpW = AddSIMD( MaddSIMD( x, vecs->x, MulSIMD( y, vecs->y ) ),
				    MulSIMD( z, vecs->z ) );


	outX = AddSIMD( SubSIMD( MaddSIMD( tmpW, x, MulSIMD( tmpX, w ) ), 
							 MulSIMD( tmpY, z ) ),
					MulSIMD( tmpZ, y ) );

	outY = AddSIMD( SubSIMD( MaddSIMD( tmpW, y, MulSIMD( tmpY, w ) ), 
							 MulSIMD( tmpZ, x ) ), 
					MulSIMD( tmpX, z ) );

	outZ = AddSIMD( SubSIMD( MaddSIMD( tmpW, z, MulSIMD( tmpZ, w ) ), 
							 MulSIMD( tmpX, y ) ), 
					MulSIMD( tmpY, x ) );

	// although apparently redundant, assigning the results to intermediate local variables
	// seems to improve code scheduling slightly in SN.
	vecs->x = outX;
	vecs->y = outY;
	vecs->z = outZ;
}


/*

void QuaternionScale( const Quaternion &p, float t, Quaternion &q )
{
	Assert( s_bMathlibInitialized );

	
	float r;

	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to 
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
	float sinom = sqrt( DotProduct( &p.x, &p.x ) );
	sinom = min( sinom, 1.f );

	float sinsom = sin( asin( sinom ) * t );

	t = sinsom / (sinom + FLT_EPSILON);
	VectorScale( &p.x, t, &q.x );

	// rescale rotation
	r = 1.0f - sinsom * sinsom;

	// Assert( r >= 0 );
	if (r < 0.0f) 
	r = 0.0f;
	r = sqrt( r );

	// keep sign of rotation
	if (p.w < 0)
	q.w = -r;
	else
	q.w = r;

	Assert( q.IsValid() );

	return;
}

*/

FORCEINLINE FourQuaternions FourQuaternions::ScaleAngle( const fltx4 &scale ) const
{
	FourQuaternions ret;
	static const fltx4 OneMinusEpsilon = {1.0f -  0.000001f, 1.0f -  0.000001f, 1.0f -  0.000001f, 1.0f -  0.000001f };
	const fltx4 Zero = Four_Zeros;
	fltx4 signMask = LoadAlignedSIMD( (float *) g_SIMD_signmask ); 
	// work out if there are any tiny scales or angles, which are unstable
	bi32x4 tinyAngles = CmpGtSIMD(w,OneMinusEpsilon);
	bi32x4 negativeRotations = CmpLtSIMD(w, Zero); // if any w's are <0, we will need to negate later down

	// figure out the theta
	fltx4 angles = ArcCosSIMD( w );

	// test also if w > -1
	fltx4 negativeWs = XorSIMD(signMask, w);
	tinyAngles = OrSIMD( CmpGtSIMD(negativeWs, OneMinusEpsilon ), tinyAngles );

	// meanwhile start working on computing the dot product of the
	// vector component, and trust in the scheduler to interleave them
	fltx4 vLenSq = MulSIMD( x, x );
	vLenSq = MaddSIMD( y, y, vLenSq );
	vLenSq = MaddSIMD( z, z, vLenSq );

	// scale the angles
	angles = MulSIMD( angles, scale );

	// clear out the sign mask where w>=0
	signMask = MaskedAssign( negativeRotations, signMask, Zero);

	// work out the new w component and vector length
	fltx4 vLenRecip = ReciprocalSqrtSIMD(vLenSq); // interleave with Cos to hide latencies
	fltx4 sine;
	SinCosSIMD( sine, ret.w, angles );
	ret.x = MulSIMD( x, vLenRecip ); // renormalize so the vector length + w = 1
	ret.y = MulSIMD( y, vLenRecip ); // renormalize so the vector length + w = 1
	ret.z = MulSIMD( z, vLenRecip ); // renormalize so the vector length + w = 1
	ret.x = MulSIMD( ret.x, sine );
	ret.y = MulSIMD( ret.y, sine );
	ret.z = MulSIMD( ret.z, sine );

	// negate where necessary
	ret.x = XorSIMD(ret.x, signMask);
	ret.y = XorSIMD(ret.y, signMask);
	ret.z = XorSIMD(ret.z, signMask);
	ret.w = XorSIMD(ret.w, signMask);

	// finally, toss results from where cos(theta) is close to 1 -- these are non rotations.
	ret.x = MaskedAssign(tinyAngles, x, ret.x);
	ret.y = MaskedAssign(tinyAngles, y, ret.y);
	ret.z = MaskedAssign(tinyAngles, z, ret.z);
	ret.w = MaskedAssign(tinyAngles, w, ret.w);

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: return = this * ( s * q )
// In other words, for a quaternion representing a rotation of angle theta, return
// one of angle s*theta
// s is four floats in a fltx4 -- one for each quaternion
//-----------------------------------------------------------------------------
 
FORCEINLINE FourQuaternions FourQuaternions::MulAc( const fltx4 &s, const FourQuaternions &q ) const
{
	/*
	void QuaternionMA( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt )
	{
		Quaternion p1, q1;

		QuaternionScale( q, s, q1 );
		QuaternionMult( p, q1, p1 );
		QuaternionNormalize( p1 );
		qt[0] = p1[0];
		qt[1] = p1[1];
		qt[2] = p1[2];
		qt[3] = p1[3];
	}
	*/

	return Mul(q.ScaleAngle(s));
}


FORCEINLINE FourQuaternions FourQuaternions::ScaleMul( const fltx4 &s, const FourQuaternions &q ) const
{
	return ScaleAngle(s).Mul(q);
}


FORCEINLINE FourQuaternions FourQuaternions::Slerp( const FourQuaternions &originalto, const fltx4 &t )
{
	FourQuaternions ret;
	static const fltx4 OneMinusEpsilon = {1.0f -  0.000001f, 1.0f -  0.000001f, 1.0f -  0.000001f, 1.0f -  0.000001f };

	// align if necessary.

	// actually, before we even do that, start by computing the dot product of 
	// the quaternions. it has lots of dependent ops and we can sneak it into
	// the pipeline bubbles as we figure out alignment. Of course we don't know
	// yet if we need to realign, so compute them both -- there's plenty of
	// space in the bubbles. They're roomy, those bubbles.
	fltx4 cosineOmega;
#if 0 // Maybe I don't need to do alignment seperately, using the xb360 technique...
	FourQuaternions to;
	{
		fltx4 diffs[4], sums[4], originalToNeg[4];
		fltx4 dotIfAligned, dotIfNotAligned;

		// compute negations of the TO quaternion.
		originalToNeg[0] = NegSIMD(originalto.x);
		originalToNeg[1] = NegSIMD(originalto.y);
		originalToNeg[2] = NegSIMD(originalto.z);
		originalToNeg[3] = NegSIMD(originalto.w);

			dotIfAligned    = MulSIMD(x, originalto.x);
			dotIfNotAligned = MulSIMD(x, originalToNeg[0]);

		diffs[0] = SubSIMD(x, originalto.x);
		diffs[1] = SubSIMD(y, originalto.y);
		diffs[2] = SubSIMD(z, originalto.z);
		diffs[3] = SubSIMD(w, originalto.w);

		sums[0] = AddSIMD(x, originalto.x);
		sums[1] = AddSIMD(y, originalto.y);
		sums[2] = AddSIMD(z, originalto.z);
		sums[3] = AddSIMD(w, originalto.w);

			dotIfAligned    = MaddSIMD(y, originalto.y,     dotIfAligned);
			dotIfNotAligned = MaddSIMD(y, originalToNeg[1], dotIfNotAligned);

		fltx4 diffsDot, sumsDot;

		diffsDot = MulSIMD(diffs[0], diffs[0]); // x^2
		sumsDot  = MulSIMD(sums[0],  sums[0] ); // x^2
			// do some work on the dot products while letting the multiplies cook
			dotIfAligned    = MaddSIMD(z, originalto.z,     dotIfAligned);
			dotIfNotAligned = MaddSIMD(z, originalToNeg[2], dotIfNotAligned);

		diffsDot = MaddSIMD(diffs[1], diffs[1], diffsDot); // x^2 + y^2 
		sumsDot  = MaddSIMD(sums[1],  sums[1],  sumsDot ); 
		diffsDot = MaddSIMD(diffs[2], diffs[2], diffsDot); // x^2 + y^2 + z^2
		sumsDot  = MaddSIMD(sums[2],  sums[2],  sumsDot ); 
		diffsDot = MaddSIMD(diffs[3], diffs[3], diffsDot); // x^2 + y^2 + z^2 + w^2
		sumsDot  = MaddSIMD(sums[3],  sums[3],  sumsDot ); 
			// do some work on the dot products while letting the multiplies cook
			dotIfAligned    = MaddSIMD(w, originalto.w,     dotIfAligned);
			dotIfNotAligned = MaddSIMD(w, originalToNeg[3], dotIfNotAligned);

		// are the differences greater than the sums?
		// if so, we need to negate that quaternion
		fltx4 mask = CmpGtSIMD(diffsDot, sumsDot); // 1 for diffs>0 and 0 elsewhere
		to.x = MaskedAssign(mask, originalToNeg[0], originalto.x);
		to.y = MaskedAssign(mask, originalToNeg[1], originalto.y);
		to.z = MaskedAssign(mask, originalToNeg[2], originalto.z);
		to.w = MaskedAssign(mask, originalToNeg[3], originalto.w);

		cosineOmega = MaskedAssign(mask, dotIfNotAligned, dotIfAligned);
	}

	// right, now to is aligned to be the short way round, and we computed
	// the dot product while we were figuring all that out.
#else
	const FourQuaternions &to = originalto;
	cosineOmega = MulSIMD(x, to.x);
	cosineOmega = MaddSIMD(y, to.y, cosineOmega);
	cosineOmega = MaddSIMD(z, to.z, cosineOmega);
	cosineOmega = MaddSIMD(w, to.w, cosineOmega);
#endif
	
	fltx4 Zero = Four_Zeros;
	bi32x4 cosOmegaLessThanZero = CmpLtSIMD(cosineOmega, Zero);
	// fltx4 shouldNegate = MaskedAssign(cosOmegaLessThanZero, Four_NegativeOnes , Four_Ones );
	fltx4 signMask = LoadAlignedSIMD( (float *) g_SIMD_signmask ); // contains a one in the sign bit -- xor against a number to negate it
	fltx4 sinOmega = Four_Ones;

	// negate cosineOmega where necessary
	cosineOmega = MaskedAssign( cosOmegaLessThanZero, XorSIMD(cosineOmega, signMask), cosineOmega );
	fltx4 oneMinusT = SubSIMD(Four_Ones,t);
	bi32x4 bCosOmegaLessThanOne = CmpLtSIMD(cosineOmega, OneMinusEpsilon); // we'll use this to mask out null slerps

	// figure out the sin component of the diff quaternion.
	// since sin^2(t) + cos^2(t) = 1...
	sinOmega = MsubSIMD( cosineOmega, cosineOmega, sinOmega ); // = 1 - cos^2(t) = sin^2(t)
	fltx4 invSinOmega = ReciprocalSqrtSIMD( sinOmega );  // 1/sin(t)
	sinOmega = MulSIMD( sinOmega, invSinOmega ); // = sin^2(t) / sin(t) = sin(t)

	// use the arctangent technique to work out omega from  tan^-1(sin/cos)
	fltx4 omega = ArcTan2SIMD(sinOmega, cosineOmega);

	// alpha = sin(omega * (1-T))/sin(omega)
	// beta  = sin(omega * T)/sin(omega)
	fltx4 alpha = MulSIMD(omega, oneMinusT);  // w(1-T)
	fltx4 beta  = MulSIMD(omega, t);		  // w(T)
	signMask = MaskedAssign(cosOmegaLessThanZero, signMask, Zero);

	alpha = SinSIMD(alpha);  // sin(w(1-T))
	beta  = SinSIMD(beta);   // sin(wT)
	
	alpha = MulSIMD(alpha, invSinOmega);
	beta  = MulSIMD(beta, invSinOmega);

	// depending on whether the dot product was less than zero, negate beta, or not
	beta  = XorSIMD(beta, signMask);

	// mask out singularities (where omega = 1)
	alpha = MaskedAssign( bCosOmegaLessThanOne, alpha, oneMinusT );
	beta  = MaskedAssign( bCosOmegaLessThanOne, beta , t );

 	ret.x = MulSIMD(x, alpha);
	ret.y = MulSIMD(y, alpha);
	ret.z = MulSIMD(z, alpha);
	ret.w = MulSIMD(w, alpha);

	ret.x = MaddSIMD(to.x, beta, ret.x);
	ret.y = MaddSIMD(to.y, beta, ret.y);
	ret.z = MaddSIMD(to.z, beta, ret.z);
	ret.w = MaddSIMD(to.w, beta, ret.w);

	return ret;
}



FORCEINLINE FourQuaternions FourQuaternions::SlerpNoAlign( const FourQuaternions &originalto, const fltx4 &t )
{
	FourQuaternions ret;
	static const fltx4 OneMinusEpsilon = {1.0f -  0.000001f, 1.0f -  0.000001f, 1.0f -  0.000001f, 1.0f -  0.000001f };

	// align if necessary.

	// actually, before we even do that, start by computing the dot product of 
	// the quaternions. it has lots of dependent ops and we can sneak it into
	// the pipeline bubbles as we figure out alignment. Of course we don't know
	// yet if we need to realign, so compute them both -- there's plenty of
	// space in the bubbles. They're roomy, those bubbles.
	fltx4 cosineOmega;

	const FourQuaternions &to = originalto;
	cosineOmega = MulSIMD(x, to.x);
	cosineOmega = MaddSIMD(y, to.y, cosineOmega);
	cosineOmega = MaddSIMD(z, to.z, cosineOmega);
	cosineOmega = MaddSIMD(w, to.w, cosineOmega);

	fltx4 sinOmega = Four_Ones;

	fltx4 oneMinusT = SubSIMD(Four_Ones,t);
	bi32x4 bCosOmegaLessThanOne = CmpLtSIMD(cosineOmega, OneMinusEpsilon); // we'll use this to mask out null slerps

	// figure out the sin component of the diff quaternion.
	// since sin^2(t) + cos^2(t) = 1...
	sinOmega = MsubSIMD( cosineOmega, cosineOmega, sinOmega ); // = 1 - cos^2(t) = sin^2(t)
	fltx4 invSinOmega = ReciprocalSqrtSIMD( sinOmega );  // 1/sin(t)
	sinOmega = MulSIMD( sinOmega, invSinOmega ); // = sin^2(t) / sin(t) = sin(t)

	// use the arctangent technique to work out omega from  tan^-1(sin/cos)
	fltx4 omega = ArcTan2SIMD(sinOmega, cosineOmega);

	// alpha = sin(omega * (1-T))/sin(omega)
	// beta  = sin(omega * T)/sin(omega)
	fltx4 alpha = MulSIMD(omega, oneMinusT);  // w(1-T)
	fltx4 beta  = MulSIMD(omega, t);		  // w(T)
	alpha = SinSIMD(alpha);  // sin(w(1-T))
	beta  = SinSIMD(beta);   // sin(wT)
	alpha = MulSIMD(alpha, invSinOmega);
	beta  = MulSIMD(beta, invSinOmega);

	// mask out singularities (where omega = 1)
	alpha = MaskedAssign( bCosOmegaLessThanOne, alpha, oneMinusT );
	beta  = MaskedAssign( bCosOmegaLessThanOne, beta , t );

	ret.x = MulSIMD(x, alpha);
	ret.y = MulSIMD(y, alpha);
	ret.z = MulSIMD(z, alpha);
	ret.w = MulSIMD(w, alpha);

	ret.x = MaddSIMD(to.x, beta, ret.x);
	ret.y = MaddSIMD(to.y, beta, ret.y);
	ret.z = MaddSIMD(to.z, beta, ret.z);
	ret.w = MaddSIMD(to.w, beta, ret.w);

	return ret;
}

/***** removed because one of the SWIG permutations doesn't include ssequaternion.h, causing a missing symbol on this function:
inline void FourVectors::RotateBy( const FourQuaternions &quats )
{
	quats.RotateFourVectors( this );
}
*/


#endif // SSEQUATMATH_H



#include "platform.h"
#include "box_buoyancy.h"
#include "mathlib/vector4d.h"
#include "hardware_clock_fast.h"



inline const Vector ToVector( const fltx4 & f4 )
{
	return Vector( SubFloat( f4, 0 ), SubFloat( f4, 1 ), SubFloat( f4, 2 ) );
}

#ifdef _X360
FORCEINLINE fltx4 PermYXZW( const fltx4 & a )
{
	return __vpermwi( a,  0x4B ); // 01001011b
}
FORCEINLINE fltx4 PermXZYW( const fltx4 & a )
{
	return __vpermwi( a,  0x27 ); // 00100111b
}
FORCEINLINE fltx4 PermZYXW( const fltx4 & a )
{
	return __vpermwi( a,  0x93 ); // 10010011b
}
FORCEINLINE fltx4 PermXXYW( const fltx4 & a )
{
	return __vpermwi( a,  0x07 ); // 00000111b
}
FORCEINLINE fltx4 PermYZZW( const fltx4 & a )
{
	return __vpermwi( a,  0x6B ); // 01101011b
}
FORCEINLINE fltx4 Sum3SIMD( const fltx4 &a )
{
	return __vmsum3fp( a, Four_Ones );
}
FORCEINLINE fltx4 CombineSIMD( const fltx4 & x, const fltx4 & y, const fltx4 & z, const fltx4 & w )
{
	fltx4 r0 = __vmrghw(x, z);
	fltx4 r1 = __vmrghw(y, w);

	return __vmrghw(r0, r1);
}

// Assumes Y(xbox),Z(PC) are splatted
FORCEINLINE fltx4 CombineXYZ_Special( const fltx4 & x, const fltx4 & y, const fltx4 & z )
{
	fltx4 r0 = __vmrghw(x, z);
	return __vmrghw(r0, y);
}

#elif defined( _PS3 )

const int32 ALIGN16 g_SIMD_YXZW[4] ALIGN16_POST = { 0x04050607, 0x00010203, 0x08090A0B, 0x0C0D0E0F };
const int32 ALIGN16 g_SIMD_XZYW[4] ALIGN16_POST = { 0x00010203, 0x08090A0B, 0x04050607, 0x0C0D0E0F };
const int32 ALIGN16 g_SIMD_ZYXW[4] ALIGN16_POST = { 0x08090A0B, 0x04050607, 0x00010203, 0x0C0D0E0F };
const int32 ALIGN16 g_SIMD_XXYW[4] ALIGN16_POST = { 0x00010203, 0x00010203, 0x04050607, 0x0C0D0E0F };
const int32 ALIGN16 g_SIMD_YZZW[4] ALIGN16_POST = { 0x04050607, 0x08090A0B, 0x08090A0B, 0x0C0D0E0F };

FORCEINLINE fltx4 PermYXZW( const fltx4 & a )
{
	return vec_perm( a, a, (vec_uchar16)LoadAlignedIntSIMD( g_SIMD_YXZW ) );
}
FORCEINLINE fltx4 PermXZYW( const fltx4 & a )
{
	return vec_perm( a, a, (vec_uchar16)LoadAlignedIntSIMD( g_SIMD_XZYW ) );
}
FORCEINLINE fltx4 PermZYXW( const fltx4 & a )
{
	return vec_perm( a, a, (vec_uchar16)LoadAlignedIntSIMD( g_SIMD_ZYXW ) );
}
FORCEINLINE fltx4 PermXXYW( const fltx4 & a )
{
	return vec_perm( a, a, (vec_uchar16)LoadAlignedIntSIMD( g_SIMD_XXYW ) );
}
FORCEINLINE fltx4 PermYZZW( const fltx4 & a )
{
	return vec_perm( a, a, (vec_uchar16)LoadAlignedIntSIMD( g_SIMD_YZZW ) );
}
FORCEINLINE fltx4 Sum3SIMD( const fltx4 &a )
{
	return SplatXSIMD( a ) + SplatYSIMD( a ) + SplatZSIMD( a );
}

const int32 ALIGN16 g_SIMD_XAXA[4] ALIGN16_POST = { 0x00010203, 0x10111213, 0x00010203, 0x10111213 };
const int32 ALIGN16 g_SIMD_XYAB[4] ALIGN16_POST = { 0x00010203, 0x10111213, 0x00010203, 0x10111213 };
FORCEINLINE fltx4 CombineSIMD( const fltx4 & x, const fltx4 & y, const fltx4 & z, const fltx4 & w )
{
	//fltx4 xy = vec_perm(x, y, LoadAlignedIntSIMD( g_SIMD_XAXA ) );
	//fltx4 zw = vec_perm(z, w, LoadAlignedIntSIMD( g_SIMD_XAXA ) );
	fltx4 xzxz = vec_mergeh(x, z);
	fltx4 ywyw = vec_mergeh(y, w);

	return vec_mergeh(xzxz, ywyw);
}

// Assumes Y(xbox),Z(PC) are splatted
FORCEINLINE fltx4 CombineXYZ_Special( const fltx4 & x, const fltx4 & y, const fltx4 & z )
{
	fltx4 r0 = vec_mergeh(x, z);
	return vec_mergeh(r0, y);
}
#else
FORCEINLINE fltx4 PermYXZW( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, _MM_SHUFFLE( 3, 2, 0, 1 ) );
}
FORCEINLINE fltx4 PermXZYW( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, _MM_SHUFFLE( 3, 1, 2, 0 ) );
}
FORCEINLINE fltx4 PermZYXW( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, _MM_SHUFFLE( 3, 0, 1, 2 ) );
}
FORCEINLINE fltx4 PermXXYW( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, _MM_SHUFFLE( 3, 1, 0, 0 ) );
}

FORCEINLINE fltx4 PermYZZW( const fltx4 & a )
{
	return _mm_shuffle_ps( a, a, _MM_SHUFFLE( 3, 2, 2, 1 ) );
}
FORCEINLINE fltx4 Sum3SIMD( const fltx4 &a )
{
	return SplatXSIMD( a ) + SplatYSIMD( a ) + SplatZSIMD( a );
}
FORCEINLINE fltx4 CombineSIMD( const fltx4 & row0, const fltx4 & row1, const fltx4 & row2, const fltx4 & row3 )
{
	fltx4 tmp0 = _mm_shuffle_ps( row0, row1, 0x44);
	fltx4 tmp1 = _mm_shuffle_ps( row2, row3, 0x44);
	return _mm_shuffle_ps(tmp0, tmp1, 0x88);
}

// Assumes Y(xbox),Z(PC) are splatted
FORCEINLINE fltx4 CombineXYZ_Special( const fltx4 & x, const fltx4 & y, const fltx4 & z )
{
	fltx4 tmp0 = _mm_shuffle_ps( x, y, 0x44);
	return _mm_shuffle_ps(tmp0, z, 0x88);
}


#endif





fltx4 GetBoxBuoyancy4x3( const fltx4& f4a, const fltx4& f4b, const fltx4&f4c, const fltx4&f4Origin )
{
	FourVectors box;
	box.LoadAndSwizzle( f4a, f4b, f4c, f4Origin );
	return GetBoxBuoyancy3x4( box );
}

void BenchmarkBoxBuoyancy4x3( const fltx4& f4a, const fltx4& f4b, const fltx4&f4c, const fltx4&f4Origin )
{
	FourVectors box;
	box.LoadAndSwizzle( f4a, f4b, f4c, f4Origin );
	fltx4 result = {0, 0, 0, 0};

	int start, end;
	const int nIterations = 1000000;

	start = GetHardwareClockFast();
	for ( int i = 0; i < nIterations; ++i )
	{
		result = result + GetBoxBuoyancy3x4( box );
		box.x = AndSIMD( box.x, box.x );
	}
	end = GetHardwareClockFast();
	Msg( "Box Buoyancy 4x3 Benchmark: %d ticks/box, volume %g \n", int32( ( end - start ) ) / nIterations, SubFloat( result, 3 ) / nIterations );
}



/*
inline fltx4 operator - ( const fltx4 & a, const fltx4 & b )
{
	return SubSIMD( a, b );
}

inline fltx4 operator + ( const fltx4 & a, const fltx4 & b )
{
	return AddSIMD( a, b );
}

inline fltx4 operator * ( const fltx4 & a, const fltx4 & b )
{
	return MulSIMD( a, b );
}
*/

inline fltx4 Bound( const fltx4 & a, const fltx4 &low, const fltx4 &high )
{
	return MinSIMD( MaxSIMD( a, low ), high );
}

inline fltx4 Limit01( const fltx4 & a )
{
	return MinSIMD( MaxSIMD( Four_Zeros, a ), Four_Ones );
}


const fltx4 Four_One6th = { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f };
const fltx4 Four_One4th = { 0.25f, 0.25f, 0.25f, 0.25f };
const fltx4 Four_One12th = { 1.0f / 12.0f, 1.0f / 12.0f, 1.0f / 12.0f, 1.0f / 12.0f };

// integral A .. 1 of : y (tipZ + (baseZ - tipZ) y) dy
inline fltx4 TriHelperIntegralAto1( const fltx4 &alpha, const fltx4 &tipZ, const fltx4 &baseZ )
{
	return MaddSIMD( Four_Thirds, baseZ,
	                 MsubSIMD( alpha * alpha, ( MsubSIMD( Four_Thirds, alpha * ( tipZ - baseZ ), Four_PointFives * tipZ ) ), Four_One6th * tipZ )
	               );
}


// integral A .. 1 of : y ((tipZ + (baseZ - tipZ) y)^2) dy
inline fltx4 TriHelperZ2IntegralAto1( const fltx4 &alpha, const fltx4 &tipZ, const fltx4 &baseZ )
{
	fltx4 alphaSqr = alpha * alpha;
	fltx4 alphaMinus1 = alpha - Four_Ones, alphaMinus1Sqr = alphaMinus1 * alphaMinus1;
	return Four_One4th*( Four_Ones - alphaSqr ) * ( alphaSqr + Four_Ones ) * baseZ*baseZ + ( Four_One6th + alphaSqr*alpha  * ( Four_PointFives * alpha - Four_TwoThirds ) )* baseZ *tipZ - alphaMinus1Sqr * alphaMinus1 * ( Four_One12th + Four_One4th * alpha ) * tipZ * tipZ;
}

// integral A .. 1 of : y (tipZ + (baseZ - tipZ) y) * (tipX + (baseX - tipX) y) dy
inline fltx4 TriHelperZ2IntegralAto1( const fltx4 &alpha, const fltx4 &tipZ, const fltx4 &baseZ, const fltx4 &tipX, const fltx4 &baseX )
{
	fltx4 alphaSqr = alpha * alpha;
	fltx4 alphaMinus1 = alpha - Four_Ones, alphaMinus1Sqr = alphaMinus1 * alphaMinus1;

	return ( alphaMinus1Sqr*tipX*( ( Four_Ones + alpha*( Four_Twos + Four_Threes*alpha ) )*baseZ + tipZ + ( Four_Twos - Four_Threes*alpha )*alpha*tipZ ) +
	         baseX*( -Four_Threes*( alphaSqr*alphaSqr - Four_Ones )*baseZ + tipZ + alphaSqr*alpha*( Four_Threes*alpha - Four_Fours )*tipZ ) ) * Four_One12th;
}


// integral 0 .. B of : y (tipZ + (baseZ - tipZ) y) dy
inline fltx4 TriHelperIntegral0toB( const fltx4 &beta, const fltx4 &tipZ, const fltx4 &baseZ )
{
	return beta * beta * ( MaddSIMD( Four_Thirds, ( baseZ - tipZ ) * beta, Four_PointFives * tipZ ) );
}


/*
double SubDbl( const fltx4& a, int i )
{
	return SubFloat( a, i );
}
*/

// integral 0 .. B of : y ((tipZ + (baseZ - tipZ) y)^2) dy
inline fltx4 TriHelperZ2Integral0toB( const fltx4 &beta, const fltx4 &tipZ, const fltx4 &baseZ )
{
	fltx4 dz = baseZ - tipZ;
	fltx4 bdz = beta * dz;

	fltx4 f4Integral = beta * beta * ( Four_One4th * bdz * bdz + Four_TwoThirds * bdz * tipZ + Four_PointFives * tipZ * tipZ );
	/*
		double testIntegral[4];
		for ( int i = 0; i < 4; ++i )
		{
			testIntegral[i] = SubDbl( beta, i ) * SubDbl( beta, i ) * ( SubDbl( Four_One4th, i ) * SubDbl( bdz, i ) * SubDbl( bdz, i ) + SubDbl( Four_TwoThirds, i ) * SubDbl( bdz, i ) * SubDbl( tipZ, i ) + SubDbl( Four_PointFives, i ) * SubDbl( tipZ, i ) * SubDbl( tipZ, i ) );
		}
	*/
	return f4Integral;
}

// integral 0 .. B of : y (tipZ + (baseZ - tipZ) y) (tipX + (baseX - tipX) y ) dy
// note: baseX should be the center of the base coordinate
inline fltx4 TriHelperZ2Integral0toB( const fltx4 &beta, const fltx4 &tipZ, const fltx4 &baseZ, const fltx4 &tipX, const fltx4 &baseX )
{
	fltx4 dz = baseZ - tipZ, dx = baseX - tipX;
	fltx4 betaSqr = beta * beta;

	fltx4 f4Integral = betaSqr * ( betaSqr * Four_One4th * dx * dz + Four_PointFives * tipX * tipZ + Four_Thirds * beta * ( baseZ * tipX + ( baseX - Four_Twos * tipX ) * tipZ ) );
	return f4Integral;
}


// this is 3 * Integral 0..1 of (z0+(z1-z0)y) dy
inline fltx4 TrplAvgSqrZ( const fltx4& z0, const fltx4 &z1 )
{
	return MaddSIMD( z0, z0 + z1, z1 * z1 );
}

inline fltx4 SixAvgSqrZX( const fltx4& z0, const fltx4 &z1, const fltx4& x0, const fltx4 &x1 )
{
	return x0 * MaddSIMD( Four_Twos, z0, z1 ) + x1 * MaddSIMD( Four_Twos, z1, z0 ) ;
}

const fltx4 f4Epsilon = {1e-6f, 1e-6f, 1e-6f, 1e-6f};


inline FourVectors Cross( const FourVectors &a, const FourVectors &b )
{
	FourVectors ret;
	ret.x=MsubSIMD( a.z, b.y, MulSIMD( a.y, b.z ) );
	ret.y=MsubSIMD( a.x, b.z, MulSIMD( a.z, b.x ) );
	ret.z=MsubSIMD( a.y, b.x, MulSIMD( a.x, b.y ) );
	return ret;
}

inline fltx4 CrossZ( const FourVectors &a, const FourVectors &b )
{
	return MsubSIMD( a.y, b.x, MulSIMD( a.x, b.y ) );
}

inline fltx4 Sqr( const fltx4 &a )
{
	return a * a;
}

inline FourVectors MsubSIMD( const FourVectors &a, const fltx4 &b, const FourVectors &c) // c-a*b
{
	FourVectors ret;
	ret.x = MsubSIMD(a.x, b, c.x );
	ret.y = MsubSIMD(a.y, b, c.y );
	ret.z = MsubSIMD(a.z, b, c.z );
	return ret;
}



const fltx4 g_f4_11h4 = {1,1,0.5f,4.0f};
const fltx4 g_f4_4424 = {4,4,2,4};
const fltx4 g_f4AlmostInifiniteSlope = {1e+24,1e+24,1e+24,1e+24};
const int32 ALIGN16 g_SIMD_signmask_W[4] ALIGN16_POST = { 0x80000000, 0x80000000, 0x80000000, 0xFFFFFFFF };
const int32 ALIGN16 g_SIMD_signmask_NoW[4] ALIGN16_POST = { 0x80000000, 0x80000000, 0x80000000, 0 };

// physical interpretation: we're integrating the pressure force (pascals) along the submerged surface.
// in other words, we substitute the usual volume integral for surface integral
// Xbox360: 1250 cycles; Core2 Quad: 500 cycles; Core i7: ? cycles   ; error: 2e-5
fltx4 GetBoxBuoyancy3x4( const FourVectors &box_in )
{
	FourVectors box; // sorted box
	// make (a,b,c).z > 0
	fltx4 f4SignMask = LoadAlignedSIMD( g_SIMD_signmask );
	fltx4 signZ = AndSIMD( box_in.z, f4SignMask );
	box.x = XorSIMD( box_in.x, signZ );
	box.y = XorSIMD( box_in.y, signZ );
	box.z = AndNotSIMD( f4SignMask, box_in.z );
	fltx4 boxCenterZ = SplatWSIMD( box_in.z ); // the height of the center of the box above the water level
	fltx4 boxCenterXY = AndSIMD( SetYSIMD( SplatWSIMD( box_in.x ), SplatWSIMD( box_in.y ) ), LoadAlignedSIMD( g_SIMD_SkipTailMask[2] ) );
	
	// there are a lot of scheduling holes on this stage, so we might as well precompute something
	// high point of the box, a+b+c
	fltx4 boxTopX = Sum3SIMD( box.x );
	fltx4 boxTopY = Sum3SIMD( box.y );
	fltx4 boxTopZrel = Sum3SIMD( box.z );
	fltx4 boxTopZabs = boxCenterZ + boxTopZrel, boxBotZ = boxCenterZ - boxTopZrel;

	// sort a.z > b.z > c.z > 0; sorting takes 43 cycles on xbox360
	bi32x4 swap_a_c = CmpLtSIMD( SplatXSIMD( box.z ), SplatZSIMD( box.z ) );
	box.x = MaskedAssign( swap_a_c, PermZYXW( box.x ), box.x );
	box.y = MaskedAssign( swap_a_c, PermZYXW( box.y ), box.y );
	box.z = MaskedAssign( swap_a_c, PermZYXW( box.z ), box.z );

	bi32x4 isBsmaller = CmpLtSIMD( SplatYSIMD( box.z ), box.z );
	bi32x4 ordered_a_b = SplatXSIMD( isBsmaller ); // if a > b, they're ordered correctly
	box.x = MaskedAssign( ordered_a_b, box.x, PermYXZW( box.x ) );
	box.y = MaskedAssign( ordered_a_b, box.y, PermYXZW( box.y ) );
	box.z = MaskedAssign( ordered_a_b, box.z, PermYXZW( box.z ) );

	bi32x4 swap_b_c = SplatZSIMD( isBsmaller ); // if b < c, we need to swap them
	box.x = MaskedAssign( swap_b_c, PermXZYW( box.x ), box.x );
	box.y = MaskedAssign( swap_b_c, PermXZYW( box.y ), box.y );
	box.z = MaskedAssign( swap_b_c, PermXZYW( box.z ), box.z );

	Assert( SubFloat( box.z, 0 ) >= SubFloat( box.z, 1 ) && SubFloat( box.z, 1 ) >= SubFloat( box.z, 2 ) && SubFloat( box.z, 2 ) >= 0 );

	// sorted and positive, time to integrate sides: (a,b) (a,c) (b,c)
	// (a-b).z > (b-a).z, so the a+b, a-b, b-a, -a-b is the order of corners, top-to-bottom
	FourVectors boxA, boxB; // these two represent a and b of each pair of edges defining
	boxA.x = PermXXYW( box.x );
	boxA.y = PermXXYW( box.y );
	boxA.z = PermXXYW( box.z );
	boxB.x = PermYZZW( box.x );
	boxB.y = PermYZZW( box.y );
	boxB.z = PermYZZW( box.z );
	FourVectors boxC; // "c" maps to ±c,b,a
	boxC.x = PermZYXW( box.x );
	boxC.y = PermZYXW( box.y );
	boxC.z = PermZYXW( box.z );

	// if a.z == 0 , b.z is also 0, so the whole rectangle is parallel to z=const
	bi32x4 isSideFlat = CmpLtSIMD( boxA.z, f4Epsilon );
	fltx4 rcpAz = AndNotSIMD( isSideFlat, ReciprocalSIMD( boxA.z ) );
	fltx4 rcp2Az = Four_PointFives * rcpAz;

	// the part of quad along a that's in the triangles cut by z=const surfaces
	// this is the same regardless of C
	//
	// tab size must = 4 spaces for the ASCII art below to make sense
	//
	//                     * (a+b)  cut = 0								a
	//				    /|									|			^
	//				   / |									|			|
	//		(a-b)     *--+        cut = f4CutPart			|			|
	//				  |	 |									|			|
	//				  |	 |									|			|     > b=
	//				  |	 |									V			|	 /
	//				  +--* (b-a)  cut = 1					cut,		|	/
	//				  | /									level,		|  /
	//				  |/									fraction,	| /
	//		(-a-b)    *			  cut = 1 + f4CutPart		etc.		|/
	//
	//
	//             (a+b)-(a-b)	  2b	b
	// computed as ----------- == -- == -
	//             (a+b)-(b-a)	  2a	a
	//
	fltx4 f4CutPart = MulSIMD( boxB.z, rcpAz );  // this must be between 0 (b is parallel to z=const) and 1 (a and b both have 45' slope)
	Assert( IsAllGreaterThanOrEq( Four_Ones + f4Epsilon, SetWToZeroSIMD( f4CutPart ) ) && IsAllGreaterThanOrEq( f4CutPart + f4Epsilon, SetWToZeroSIMD( Four_Zeros ) ) );
	//fltx4 rcpCutPart = AndSIMD( ReciprocalSIMD( f4CutPart ), CmpGtSIMD( f4CutPart, f4Epsilon ) );

	// integrate the full sides of the box, multiplied by the XY projection areas
	fltx4 f4SideProj = fabs( CrossZ( boxA, boxB ) );

	// here's the center-of-mass and total volume integral solution:
	//		{{4/3 (3 x0 z0 + xA zA + xB zB), 4/3 (3 y0 z0 + yA zA + yB zB), 2/3 (3 z0^2 + zA^2 + zB^2), 4 z0},
	//		{1/24 (4 x0 (3 z0 + zA + zB) + xA (4 z0 + 2 zA + zB) + xB (4 z0 + zA + 2 zB)), 
	// 		 1/24 (4 y0 (3 z0 + zA + zB) + yA (4 z0 + 2 zA + zB) + yB (4 z0 + zA + 2 zB)), 
	// 		 1/24 (6 z0^2 + zA^2 + zA zB + zB^2 + 4 z0 (zA + zB)), 
	// 		 1/6 (3 z0 + zA + zB)}}
	//fltx4 f4FullZ0_Cpos = boxCenterZ + boxC.z, f4FullZ0_Cneg = boxCenterZ - boxC.z;
	
	// 4/3 (3 x0 z0 + xA zA + xB zB) type of integral : take x0 z0 + (xA zA + xB zB) / 3
	// consider that x0 = ± boxC.x and z0 = boxCenterZ ± boxC.z, we're left with
	// ± boxCenter boxC.x + boxC.x boxC.z + (xA zA + xB zB) / 3
	// Again, the only part that changes is (± boxCenterZ boxC.x)

	fltx4 f4Full_X_common = boxC.x * boxC.z + Four_Thirds * ( boxA.x * boxA.z + boxB.x * boxB.z );
	fltx4 f4Full_X_Cpos = Four_Fours * (boxCenterZ * boxC.x + f4Full_X_common); 
	fltx4 f4Full_X_Cneg = Four_Fours * (f4Full_X_common - boxCenterZ * boxC.x);  

	// y is the same as x

	fltx4 f4Full_Y_common = boxC.y * boxC.z + Four_Thirds * ( boxA.y * boxA.z + boxB.y * boxB.z );
	fltx4 f4Full_Y_Cpos = Four_Fours * ( boxCenterZ * boxC.y + f4Full_Y_common ) ;  
	fltx4 f4Full_Y_Cneg = Four_Fours * ( f4Full_Y_common - boxCenterZ * boxC.y ) ;  

	// z is different: 2/3 (3 z0^2 + zA^2 + zB^2) ;  z0 = boxCenterZ ± boxC.z, 
	// so we can just add the difference of 4 * boxCenterZ * boxC.z to get from Cneg to Cpos
	fltx4 f4Full_Z_common = Four_TwoThirds * ( Sqr( boxA.z ) + Sqr( boxB.z ) );
	fltx4 f4Full_Z_Cpos = MaddSIMD( Four_Twos, Sqr( boxCenterZ + boxC.z ), f4Full_Z_common );
	fltx4 f4Full_Z_Cneg = MaddSIMD( Four_Twos, Sqr( boxCenterZ - boxC.z ), f4Full_Z_common );

	fltx4 f4Full_W_Cpos = Four_Fours * ( boxCenterZ + boxC.z ), f4Full_W_Cneg = Four_Fours * ( boxCenterZ - boxC.z );

	// this is how we'd  compute the center of mass for fully-submerged cube, for validation
#ifdef _DEBUG
	fltx4 f4TestVolume = Dot3SIMD( f4Full_W_Cpos - f4Full_W_Cneg, f4SideProj );
	fltx4 f4TestSideProjDivVolume = f4SideProj * ReciprocalSIMD( f4TestVolume );
	fltx4 f4TestLeverX = Dot3SIMD( f4Full_X_Cpos - f4Full_X_Cneg, f4TestSideProjDivVolume ), f4TestLeverY = Dot3SIMD( f4Full_Y_Cpos - f4Full_Y_Cneg, f4TestSideProjDivVolume );
	fltx4 f4TestLeverZ = Dot3SIMD( f4Full_Z_Cpos - f4Full_Z_Cneg, f4TestSideProjDivVolume );
	fltx4 f4TestResult = CombineSIMD( f4TestLeverX + SplatWSIMD(box_in.x), f4TestLeverY + SplatWSIMD(box_in.y), f4TestLeverZ, f4TestVolume ); (void)f4TestResult;
#endif

	//
	//
	/////////////////////////////////////////////////////////////////////////////
	//   Computing Center parallelogram component of the full surface integral
	//

	// To compute the integral across the submerged part of each of 6 faces, we'll compute these components and then selectively sum them up
	// to form the full integral: the top and bottom triangle.
	// if the water level is intersecting top triangle ((a-b).z < 0) , we'll subtract top triangle integral from full integral
	// if the water level is intersecting bottom triangle ((b-a).z < 0) , we'll select just the bottom triangle integral
	// .. and we'll have to compute the middle part because it's not symmetrical ..

	// .. on the second thought, we	compute the center (parallelogram) , upper tri and lower tri

	// for the center computation, we need the point of the middle of the center and m=b-ra parallel to the water
	// waterTop is{ 0 = at V0 top; cut = at V1; 1 = at V2; 1+cut = at V3 bottom of the quad }
	// waterBot is central-symmetrical, negative

	// to find the fraction of right side of rectangle (the +b side) that has z=0
	// this is different for +C and -C sides
	//
	//              (a+b) ± c + p	   a+b ± c + p
	// computed as --------------  == ------------                // note: ± is typed by Alt + 0177
	//			   (a+b)-(b-a)	       2 a
	// 
	// Warning: I take special care in cases of flat faces (z=const, when rcpAz is undefined)
	//          in these cases, submerged faces must have water<=0 and faces above water (z>0) must have water >= 1 + cut
	// Note:    If I take care not to compute fully-submerged or fully-above-water polytopes, I only need to check 
	//          below-water case for Cneg faces and above-water case for Cpos faces
	//     
	// The trick I'm using here to account for everything is perturb the face's slope slightly to effectively divide by epsilon

	fltx4 rcp2AzSpecial = MaskedAssign( isSideFlat, g_f4AlmostInifiniteSlope, rcp2Az );
	fltx4 f4WaterPart_Cpos = boxTopZabs * rcp2AzSpecial, f4WaterPart_Cneg = MaddSIMD( boxBotZ, rcp2AzSpecial, f4CutPart )  + Four_Ones;
	
	// on the central piece, we need to integrate along axes (a,m = b - cut*a) and ranges {-1+cut...max(-1+cut,1-max(w,cut)) , -1...1}
	// even cut and w have the same denominator: it's cut=2b/2a  and  water=topZ/2a

	//fltx4 f4HighLimit_Cpos = MaxSIMD( f4LowLimit, Four_Ones - MaxSIMD( f4WaterPart_Cpos, f4CutPart ) );
	//fltx4 f4HighLimit_Cneg = MaxSIMD( f4LowLimit, Four_Ones - MaxSIMD( f4WaterPart_Cneg, f4CutPart ) );
	fltx4 f4TopWaterInCenter_Cpos = MinSIMD( Four_Ones, MaxSIMD( f4CutPart, f4WaterPart_Cpos ) );
	fltx4 f4TopWaterInCenter_Cneg = MinSIMD( Four_Ones, MaxSIMD( f4CutPart, f4WaterPart_Cneg ) );
	// the range is full (1 means full span of the whole center parallelogram)
	// but the origin is to be multiplied by A, so 1 means half of the length (-1 means 0 area)
	fltx4 f4CenterRange_Cpos = Four_Ones - f4TopWaterInCenter_Cpos, f4CenterOriginA_Cpos = f4CutPart - f4TopWaterInCenter_Cpos;
	fltx4 f4CenterRange_Cneg = Four_Ones - f4TopWaterInCenter_Cneg, f4CenterOriginA_Cneg = f4CutPart - f4TopWaterInCenter_Cneg;
	// given the span (we're integrating from -span to +span), we can compute the center point for integration: ((r-1) + (1-max(w,r)))/2
	// we can also compute the area of projection, because we reduce the area of the face by 1-max(r,w), i.e. by the span
	fltx4 f4CenterProj_Cpos = f4SideProj * f4CenterRange_Cpos, f4CenterProj_Cneg = f4SideProj * f4CenterRange_Cneg;
	fltx4 f4CenterRangeSqr_Cpos = f4CenterRange_Cpos * f4CenterRange_Cpos;
	fltx4 f4CenterRangeSqr_Cneg = f4CenterRange_Cneg * f4CenterRange_Cneg;


	// to integrate the central piece, we need the center point (pos±(c-a*q)), q = ; and m=b-cut a
	// because it cancels out lots of terms in the integral

	FourVectors boxM = MsubSIMD( boxA, f4CutPart, boxB ); // m=b-ra, replacement for b in the integrals
	
		// here's the center-of-mass and total volume integral solution. M is our B in this case.
		//		{{4/3 (3 x0 z0 + xA zA + xM zM), 4/3 (3 y0 z0 + yA zA + yM zM), 2/3 (3 z0^2 + zA^2 + zM^2), 4 z0},
		//
		// and for triangles it would be this:
		//		{1/24 (4 x0 (3 z0 + zA + zM) + xA (4 z0 + 2 zA + zM) + xM (4 z0 + zA + 2 zM)), 
		// 		 1/24 (4 y0 (3 z0 + zA + zM) + yA (4 z0 + 2 zA + zM) + yM (4 z0 + zA + 2 zM)), 
		// 		 1/24 (6 z0^2 + zA^2 + zA zM + zM^2 + 4 z0 (zA + zM)), 
		// 		 1/6 (3 z0 + zA + zM)}}
		// ... but we only use the rectangular integral right now

		fltx4 f4CenterX0_Cpos =              boxC.x + f4CenterOriginA_Cpos * boxA.x, f4CenterX0_Cneg =              f4CenterOriginA_Cneg * boxA.x - boxC.x;
		fltx4 f4CenterY0_Cpos =              boxC.y + f4CenterOriginA_Cpos * boxA.y, f4CenterY0_Cneg =              f4CenterOriginA_Cneg * boxA.y - boxC.y;
		fltx4 f4CenterZ0_Cpos = boxCenterZ + boxC.z + f4CenterOriginA_Cpos * boxA.z, f4CenterZ0_Cneg = boxCenterZ + f4CenterOriginA_Cneg * boxA.z - boxC.z;

		// 4/3 (3 x0 z0 + xA zA + xB zB) type of integral : take x0 z0 + (xA zA + xB zB) / 3
		// xA zA + xB zB is the common part

		//fltx4 f4Center_X_common = Four_Thirds * (boxA.x * boxA.z + boxM.x * boxM.z );
		fltx4 boxMxz = boxM.x * boxM.z, boxAxz = boxA.x * boxA.z;
		fltx4 f4Center_X_Cpos = Four_Fours * MaddSIMD( f4CenterX0_Cpos, f4CenterZ0_Cpos, Four_Thirds * MaddSIMD( boxAxz, f4CenterRangeSqr_Cpos, boxMxz ) ); 
		fltx4 f4Center_X_Cneg = Four_Fours * MaddSIMD( f4CenterX0_Cneg, f4CenterZ0_Cneg, Four_Thirds * MaddSIMD( boxAxz, f4CenterRangeSqr_Cneg, boxMxz ) ); 

		// y is the same as x

		//fltx4 f4Center_Y_common = Four_Thirds * (boxA.y * boxA.z + boxM.y * boxM.z );
		fltx4 boxMyz = boxM.y * boxM.z, boxAyz  = boxA.y * boxA.z;
		fltx4 f4Center_Y_Cpos = Four_Fours * MaddSIMD( f4CenterY0_Cpos, f4CenterZ0_Cpos, Four_Thirds * MaddSIMD(boxAyz, f4CenterRangeSqr_Cpos, boxMyz ) ); 
		fltx4 f4Center_Y_Cneg = Four_Fours * MaddSIMD( f4CenterY0_Cneg, f4CenterZ0_Cneg, Four_Thirds * MaddSIMD(boxAyz, f4CenterRangeSqr_Cneg, boxMyz ) ); 

		// z is a bit different: 2/3 (3 z0^2 + zA^2 + zB^2) 
		// so we can just add the difference of 4 * boxCenterZ * boxC.z to get from Cneg to Cpos
		//fltx4 f4Center_Z_common = Four_TwoThirds * ( Sqr( boxA.z ) + Sqr( boxM.z ) );
		fltx4 boxMzz = boxM.z * boxM.z, boxAzz  = boxA.z * boxA.z;
		fltx4 f4Center_Z_Cpos = Four_Twos * MaddSIMD( f4CenterZ0_Cpos, f4CenterZ0_Cpos, Four_Thirds * MaddSIMD( boxAzz, f4CenterRangeSqr_Cpos, boxMzz ) );
		fltx4 f4Center_Z_Cneg = Four_Twos * MaddSIMD( f4CenterZ0_Cneg, f4CenterZ0_Cneg, Four_Thirds * MaddSIMD( boxAzz, f4CenterRangeSqr_Cneg, boxMzz ) );

		fltx4 f4Center_W_Cpos = Four_Fours * f4CenterZ0_Cpos, f4Center_W_Cneg = Four_Fours * f4CenterZ0_Cneg;

#ifdef _DEBUG
		fltx4 f4CenterVolume = Dot3SIMD( f4Center_W_Cpos, f4CenterProj_Cpos ) - Dot3SIMD( f4Center_W_Cneg, f4CenterProj_Cneg );
		fltx4 f4CenterLeverX = Dot3SIMD( f4Center_X_Cpos, f4CenterProj_Cpos ) - Dot3SIMD( f4Center_X_Cneg, f4CenterProj_Cneg );
		fltx4 f4CenterLeverY = Dot3SIMD( f4Center_Y_Cpos, f4CenterProj_Cpos ) - Dot3SIMD( f4Center_Y_Cneg, f4CenterProj_Cneg );
		fltx4 f4CenterLeverZ = Dot3SIMD( f4Center_Z_Cpos, f4CenterProj_Cpos ) - Dot3SIMD( f4Center_Z_Cneg, f4CenterProj_Cneg );
		
		// this is the condenced result of previous integration
		fltx4 f4CenterComponent = CombineSIMD( f4CenterLeverX, f4CenterLeverY, f4CenterLeverZ, f4CenterVolume );(void)f4CenterComponent;
#endif
	//
	//
	//////////////////////////////////////////////////////////////////////////
	//	  Computing triangle components
	//
	//    If top triangle is selected , Center and bottom tri are ignored and top tri is subtracted from "Full" side integrals

	// top triangle starts with the top vertex, spanning 0..-2*min(water,cut) along A and 0..-2*min(water,cut)/cut along B
	// the isTopTri_* selectors will select the top tris out if appropriate
	bi32x4 isCutLarge = CmpGtSIMD( f4CutPart, f4Epsilon ); // is the triangle part large enough to even consider it? in most cases it is
	bi32x4 isTopTri_Cpos = AndSIMD( CmpLeSIMD( f4WaterPart_Cpos, f4CutPart ), isCutLarge ), isTopTri_Cneg = AndSIMD( CmpLeSIMD( f4WaterPart_Cneg, f4CutPart ), isCutLarge );
	//fltx4 isBotTri_Cpos = AndNotSIMD( isTopTri_Cpos, isCutLarge ), isBotTri_Cneg = AndNotSIMD( isTopTri_Cneg, isCutLarge );
	// integrate above-water part
	fltx4 rcpCutPart = AndSIMD( ReciprocalSIMD( f4CutPart ), isCutLarge ); // when this is Inf, isCutLarge will select it off

	fltx4 f4WaterInTop_Cpos = MaxSIMD( Four_Zeros, MinSIMD( f4CutPart, f4WaterPart_Cpos ) );
	fltx4 f4WaterInTop_Cneg = MaxSIMD( Four_Zeros, MinSIMD( f4CutPart, f4WaterPart_Cneg ) ); // when water is below the tri, it'll actually be selected off, so the min(cut,water) isn't needed here really
	FourVectors boxTopTriB_Cpos = boxB * ( f4WaterInTop_Cpos * rcpCutPart ), boxTopTriB_Cneg = boxB * ( f4WaterInTop_Cneg * rcpCutPart );
	FourVectors boxTopTriA_Cpos = boxA * f4WaterInTop_Cpos, boxTopTriA_Cneg = boxA * f4WaterInTop_Cneg;
	fltx4 f4TopTriProj_Cpos = fabs( CrossZ( boxTopTriA_Cpos, boxTopTriB_Cpos ) ), f4TopTriProj_Cneg = fabs( CrossZ( boxTopTriA_Cneg, boxTopTriB_Cneg ) );

	fltx4 f4WaterInBot_common = Four_Ones + f4CutPart, f4CutPart_neg = -f4CutPart;
// 	fltx4 f4WaterInBot_Cpos = MaxSIMD( Four_Zeros, MinSIMD( f4CutPart, f4WaterInBot_common - f4WaterPart_Cpos ) );
// 	fltx4 f4WaterInBot_Cneg = MaxSIMD( Four_Zeros, MinSIMD( f4CutPart, f4WaterInBot_common - f4WaterPart_Cneg ) );
	fltx4 f4WaterInBot_Cpos_neg = MinSIMD( Four_Zeros, MaxSIMD( f4CutPart_neg, f4WaterPart_Cpos - f4WaterInBot_common) );
	fltx4 f4WaterInBot_Cneg_neg = MinSIMD( Four_Zeros, MaxSIMD( f4CutPart_neg, f4WaterPart_Cneg - f4WaterInBot_common) );

	// update: (looks like) for the bottom triangle, we need to integrate (0..+2) and (0..+2) in positive triangle, so we'll just need to flip
	//         the signs for the bottom triangle A and B vectors
	FourVectors boxBotTriB_Cpos = boxB * ( f4WaterInBot_Cpos_neg * rcpCutPart ), boxBotTriB_Cneg = boxB * ( f4WaterInBot_Cneg_neg * rcpCutPart );
	FourVectors boxBotTriA_Cpos = boxA * f4WaterInBot_Cpos_neg, boxBotTriA_Cneg = boxA * f4WaterInBot_Cneg_neg;
	fltx4 f4BotTriProj_Cpos = fabs( CrossZ( boxBotTriA_Cpos, boxBotTriB_Cpos ) ), f4BotTriProj_Cneg = fabs( CrossZ( boxBotTriA_Cneg, boxBotTriB_Cneg ) );

	// let's integrate along topTriA (0..-2) and topTriB (0..-2), a triangle . Here's the solved integral:
	// 	2/3 (xA (-2 z0 + 2 zA + zB) + xB (-2 z0 + zA + 2 zB) + x0 (3 z0 - 2 (zA + zB))), 
	// 	2/3 (yA (-2 z0 + 2 zA + zB) + yB (-2 z0 + zA + 2 zB) + y0 (3 z0 - 2 (zA + zB))), 
	// 	1/3 (3 z0^2 - 4 z0 (zA + zB) + 2 (zA^2 + zA zB + zB^2)), 
	// 	2/3 (3 z0 - 2 (zA + zB))
	//
	// here's collected by x0,y0,z0
	// 	2/3 (-2 xA - 2 xB) z0 + 2/3 (2 xA zA + xB zA + xA zB + 2 xB zB) + x0 (2 z0 - (4 (zA + zB))/3), 
	// 	2/3 (-2 yA - 2 yB) z0 + 2/3 (2 yA zA + yB zA + yA zB + 2 yB zB) + y0 (2 z0 - (4 (zA + zB))/3),
	// 	z0^2 - 4/3 z0 (zA + zB) + 2/3 (zA^2 + zA zB + zB^2),
	// 	2 z0 - (4 (zA + zB))/3

	// x0,y0,z0 are the boxTopZ for Cpos, and boxTopZ - 2 C for Cneg
	fltx4 f4TopTriX0_Cneg = MsubSIMD( Four_Twos, boxC.x, boxTopX );
	fltx4 f4TopTriY0_Cneg = MsubSIMD( Four_Twos, boxC.y, boxTopY );
	fltx4 f4TopTriZ0_Cneg = MsubSIMD( Four_Twos, boxC.z, boxTopZabs );

	fltx4 f4TopTri_X_Cpos = Four_TwoThirds * (boxTopTriA_Cpos.x * ( Four_Twos * ( boxTopTriA_Cpos.z - boxTopZabs ) + boxTopTriB_Cpos.z ) + 
		boxTopTriB_Cpos.x * ( boxTopTriA_Cpos.z + 
		Four_Twos * ( boxTopTriB_Cpos.z - boxTopZabs ) ) + 
		boxTopX * (Four_Threes * boxTopZabs - Four_Twos * ( boxTopTriA_Cpos.z + boxTopTriB_Cpos.z ) ) );

	fltx4 f4TopTri_Y_Cpos = Four_TwoThirds * (boxTopTriA_Cpos.y * ( Four_Twos * ( boxTopTriA_Cpos.z - boxTopZabs ) + boxTopTriB_Cpos.z ) + 
		boxTopTriB_Cpos.y * ( boxTopTriA_Cpos.z + 
		Four_Twos * ( boxTopTriB_Cpos.z - boxTopZabs ) ) + 
		boxTopY * (Four_Threes * boxTopZabs - Four_Twos * ( boxTopTriA_Cpos.z + boxTopTriB_Cpos.z ) ) );

	fltx4 f4TopTri_Z_Cpos = Four_Thirds * (Four_Threes * boxTopZabs * boxTopZabs - 
		Four_Fours * boxTopZabs * (boxTopTriA_Cpos.z + boxTopTriB_Cpos.z) + 
		Four_Twos * (boxTopTriA_Cpos.z * boxTopTriA_Cpos.z + 
		boxTopTriA_Cpos.z * boxTopTriB_Cpos.z + boxTopTriB_Cpos.z*boxTopTriB_Cpos.z));

	fltx4 f4TopTri_W_Cpos = Four_TwoThirds * ( Four_Threes * boxTopZabs - Four_Twos * ( boxTopTriA_Cpos.z + boxTopTriB_Cpos.z ) );

	fltx4 f4TopTri_X_Cneg = Four_TwoThirds * (boxTopTriA_Cneg.x * ( Four_Twos * ( boxTopTriA_Cneg.z - f4TopTriZ0_Cneg ) + boxTopTriB_Cneg.z ) + 
		boxTopTriB_Cneg.x * ( boxTopTriA_Cneg.z + 
		Four_Twos * ( boxTopTriB_Cneg.z - f4TopTriZ0_Cneg ) ) + 
		f4TopTriX0_Cneg * (Four_Threes * f4TopTriZ0_Cneg - Four_Twos * ( boxTopTriA_Cneg.z + boxTopTriB_Cneg.z ) ) );

	fltx4 f4TopTri_Y_Cneg = Four_TwoThirds * (boxTopTriA_Cneg.y * ( Four_Twos * ( boxTopTriA_Cneg.z - f4TopTriZ0_Cneg ) + boxTopTriB_Cneg.z ) + 
		boxTopTriB_Cneg.y * ( boxTopTriA_Cneg.z + 
		Four_Twos * ( boxTopTriB_Cneg.z - f4TopTriZ0_Cneg ) ) + 
		f4TopTriY0_Cneg * (Four_Threes * f4TopTriZ0_Cneg - Four_Twos * ( boxTopTriA_Cneg.z + boxTopTriB_Cneg.z ) ) );

	fltx4 f4TopTri_Z_Cneg = Four_Thirds * (Four_Threes * f4TopTriZ0_Cneg * f4TopTriZ0_Cneg - 
		Four_Fours * f4TopTriZ0_Cneg * (boxTopTriA_Cneg.z + boxTopTriB_Cneg.z) + 
		Four_Twos * (boxTopTriA_Cneg.z * boxTopTriA_Cneg.z + 
		boxTopTriA_Cneg.z * boxTopTriB_Cneg.z + boxTopTriB_Cneg.z*boxTopTriB_Cneg.z));

	fltx4 f4TopTri_W_Cneg = Four_TwoThirds * ( Four_Threes * f4TopTriZ0_Cneg - Four_Twos * ( boxTopTriA_Cneg.z + boxTopTriB_Cneg.z ) );
	
	
	
	fltx4 f4BotTriX0_Cpos = boxC.x - boxA.x - boxB.x;
	fltx4 f4BotTriY0_Cpos = boxC.y - boxA.y - boxB.y;
	fltx4 f4BotTriZ0_Cpos = boxC.z - boxA.z - boxB.z + boxCenterZ;

	fltx4 f4BotTri_X_Cpos = Four_TwoThirds * (boxBotTriA_Cpos.x * ( Four_Twos * ( boxBotTriA_Cpos.z - f4BotTriZ0_Cpos ) + boxBotTriB_Cpos.z ) + 
		boxBotTriB_Cpos.x * ( boxBotTriA_Cpos.z + 
		Four_Twos * ( boxBotTriB_Cpos.z - f4BotTriZ0_Cpos ) ) + 
		f4BotTriX0_Cpos * (Four_Threes * f4BotTriZ0_Cpos - Four_Twos * ( boxBotTriA_Cpos.z + boxBotTriB_Cpos.z ) ) );

	fltx4 f4BotTri_Y_Cpos = Four_TwoThirds * (boxBotTriA_Cpos.y * ( Four_Twos * ( boxBotTriA_Cpos.z - f4BotTriZ0_Cpos ) + boxBotTriB_Cpos.z ) + 
		boxBotTriB_Cpos.y * ( boxBotTriA_Cpos.z + 
		Four_Twos * ( boxBotTriB_Cpos.z - f4BotTriZ0_Cpos ) ) + 
		f4BotTriY0_Cpos * (Four_Threes * f4BotTriZ0_Cpos - Four_Twos * ( boxBotTriA_Cpos.z + boxBotTriB_Cpos.z ) ) );

	fltx4 f4BotTri_Z_Cpos = Four_Thirds * (Four_Threes * f4BotTriZ0_Cpos * f4BotTriZ0_Cpos - 
		Four_Fours * f4BotTriZ0_Cpos * (boxBotTriA_Cpos.z + boxBotTriB_Cpos.z) + 
		Four_Twos * (boxBotTriA_Cpos.z * boxBotTriA_Cpos.z + 
		boxBotTriA_Cpos.z * boxBotTriB_Cpos.z + boxBotTriB_Cpos.z*boxBotTriB_Cpos.z));

	fltx4 f4BotTri_W_Cpos = Four_TwoThirds * ( Four_Threes * f4BotTriZ0_Cpos - Four_Twos * ( boxBotTriA_Cpos.z + boxBotTriB_Cpos.z ) );

	fltx4 f4BotTriZ0_Cneg = boxCenterZ - boxTopZrel;
	fltx4 f4BotTri_X_Cneg = Four_TwoThirds * (boxBotTriA_Cneg.x * ( Four_Twos * ( boxBotTriA_Cneg.z - f4BotTriZ0_Cneg ) + boxBotTriB_Cneg.z ) + 
		boxBotTriB_Cneg.x * ( boxBotTriA_Cneg.z + 
		Four_Twos * ( boxBotTriB_Cneg.z - f4BotTriZ0_Cneg ) ) 
		-boxTopX * (Four_Threes * f4BotTriZ0_Cneg - Four_Twos * ( boxBotTriA_Cneg.z + boxBotTriB_Cneg.z ) ) );

	fltx4 f4BotTri_Y_Cneg = Four_TwoThirds * (boxBotTriA_Cneg.y * ( Four_Twos * ( boxBotTriA_Cneg.z - f4BotTriZ0_Cneg ) + boxBotTriB_Cneg.z ) + 
		boxBotTriB_Cneg.y * ( boxBotTriA_Cneg.z + 
		Four_Twos * ( boxBotTriB_Cneg.z - f4BotTriZ0_Cneg ) ) 
		-boxTopY * (Four_Threes * f4BotTriZ0_Cneg - Four_Twos * ( boxBotTriA_Cneg.z + boxBotTriB_Cneg.z ) ) );

	fltx4 f4BotTri_Z_Cneg = Four_Thirds * (Four_Threes * f4BotTriZ0_Cneg * f4BotTriZ0_Cneg - 
		Four_Fours * f4BotTriZ0_Cneg * (boxBotTriA_Cneg.z + boxBotTriB_Cneg.z) + 
		Four_Twos * (boxBotTriA_Cneg.z * boxBotTriA_Cneg.z + 
		boxBotTriA_Cneg.z * boxBotTriB_Cneg.z + boxBotTriB_Cneg.z*boxBotTriB_Cneg.z));

	fltx4 f4BotTri_W_Cneg = Four_TwoThirds * ( Four_Threes * f4BotTriZ0_Cneg - Four_Twos * ( boxBotTriA_Cneg.z + boxBotTriB_Cneg.z ) );

	fltx4 f4All_X_Cpos = MaskedAssign( isTopTri_Cpos, f4SideProj * f4Full_X_Cpos - f4TopTriProj_Cpos * f4TopTri_X_Cpos, f4BotTriProj_Cpos * f4BotTri_X_Cpos + f4CenterProj_Cpos * f4Center_X_Cpos );
	fltx4 f4All_X_Cneg = MaskedAssign( isTopTri_Cneg, f4SideProj * f4Full_X_Cneg - f4TopTriProj_Cneg * f4TopTri_X_Cneg, f4BotTriProj_Cneg * f4BotTri_X_Cneg + f4CenterProj_Cneg * f4Center_X_Cneg );
	fltx4 f4All_Y_Cpos = MaskedAssign( isTopTri_Cpos, f4SideProj * f4Full_Y_Cpos - f4TopTriProj_Cpos * f4TopTri_Y_Cpos, f4BotTriProj_Cpos * f4BotTri_Y_Cpos + f4CenterProj_Cpos * f4Center_Y_Cpos );
	fltx4 f4All_Y_Cneg = MaskedAssign( isTopTri_Cneg, f4SideProj * f4Full_Y_Cneg - f4TopTriProj_Cneg * f4TopTri_Y_Cneg, f4BotTriProj_Cneg * f4BotTri_Y_Cneg + f4CenterProj_Cneg * f4Center_Y_Cneg );
	fltx4 f4All_Z_Cpos = MaskedAssign( isTopTri_Cpos, f4SideProj * f4Full_Z_Cpos - f4TopTriProj_Cpos * f4TopTri_Z_Cpos, f4BotTriProj_Cpos * f4BotTri_Z_Cpos + f4CenterProj_Cpos * f4Center_Z_Cpos );
	fltx4 f4All_Z_Cneg = MaskedAssign( isTopTri_Cneg, f4SideProj * f4Full_Z_Cneg - f4TopTriProj_Cneg * f4TopTri_Z_Cneg, f4BotTriProj_Cneg * f4BotTri_Z_Cneg + f4CenterProj_Cneg * f4Center_Z_Cneg );
	fltx4 f4All_W_Cpos = MaskedAssign( isTopTri_Cpos, f4SideProj * f4Full_W_Cpos - f4TopTriProj_Cpos * f4TopTri_W_Cpos, f4BotTriProj_Cpos * f4BotTri_W_Cpos + f4CenterProj_Cpos * f4Center_W_Cpos );
	fltx4 f4All_W_Cneg = MaskedAssign( isTopTri_Cneg, f4SideProj * f4Full_W_Cneg - f4TopTriProj_Cneg * f4TopTri_W_Cneg, f4BotTriProj_Cneg * f4BotTri_W_Cneg + f4CenterProj_Cneg * f4Center_W_Cneg );

	fltx4 f4All_X = Sum3SIMD( f4All_X_Cpos - f4All_X_Cneg );
	fltx4 f4All_Y = Sum3SIMD( f4All_Y_Cpos - f4All_Y_Cneg );

	// <Sergiy> to be brutally honest, I don't care about Z integral. It represents the Z of the lever of archimedes force, and
	// it affects neither force nor torque exerted by the said force. Not computing it here reduces this routine from 1188 ticks to 900 ticks per run
	(void)f4All_Z_Cpos;
	(void)f4All_Z_Cneg;
	fltx4 f4All_Z = Four_Zeros;//Sum3SIMD( f4All_Z_Cpos - f4All_Z_Cneg ); 
	
	fltx4 f4All_W = Sum3SIMD( f4All_W_Cpos - f4All_W_Cneg );
#if 1
	// <Sergiy> again, to be brutally honest, I don't care about the actual lever of archimedes force.
	// I can just as well use lever * displaced_volume to compute the torque, and it'll actually be more precise, although less understandable.
	// 

	// this variant returns XYZ of the center of mass of displaced fluid multiplied by W, and W = volume of displaced fluid
	fltx4 f4All = CombineSIMD( f4All_X, f4All_Y, f4All_Z, f4All_W ) + f4All_W * boxCenterXY;
#else
	// this variant returns XYZ of the center of mass of displaced fluid, and W = volume of displaced fluid
	fltx4 rcpAllW = ReciprocalSIMD( f4All_W );
	fltx4 f4All = SetWSIMD( CombineXYZ_Special( f4All_X, f4All_Y, f4All_Z ) * rcpAllW + boxCenterXY, f4All_W );
#endif
	return f4All;
}



/*
float GetBoxBuoyancyTest( const matrix3x4_t & tm )
{

}
*/

Vector4D GetPyramidBuoyancy( const Vector &pos, const Vector &a, const Vector &b, const Vector &n )
{
	Vector verts[5], verts2[10];
	uint numVerts = 4, numVerts2 = 0;
	verts[0] = pos + n + a + b;
	verts[1] = pos + n + a - b;
	verts[2] = pos + n - a - b;
	verts[3] = pos + n - a + b;

	Vector prevVert = verts[3];
	for ( uint i = 0; i < numVerts; ++i )
	{
		if ( prevVert.z * verts[i].z < 0 )
		{
			// switching sign
			float flFraction = prevVert.z / ( prevVert.z - verts[i].z );
			verts2[numVerts2] = prevVert * ( 1 - flFraction ) +  verts[i] * flFraction;
			Assert( fabs( verts2[numVerts2].z ) < 1e-5f );
			verts2[numVerts2].z = 0;
			numVerts2++;
		}
		prevVert = verts2[numVerts2++] = verts[i];
	}

	float flSum = 0, flSign = 1.0f;
	Vector vecCenter( 0, 0, 0 );
	Vector normal = CrossProduct( a, b );
	Assert( DotProduct( normal, n ) >= -1e-6f );
	if ( DotProduct( pos + n, normal ) < 0 ) // pos + n is the center of the face
	{
		flSign = -1.0f;
	}

	// exclude all z>0 verts
	for ( uint i = 0 ; i < numVerts2; )
	{
		if ( verts2[i].z > 0 )
		{
			for ( uint j = i + 1 ; j < numVerts2; ++j )
			{
				verts2[j-1] = verts2[j];
			}
			--numVerts2;
		}
		else
		{
			++i;
		}
	}

	Vector rootVert = verts2[0];

	for ( uint i = 1; i + 1 < numVerts2 ; ++i )
	{
		Vector curVert = verts2[i], nextVert = verts2[i+1];
		{
			// this segment is guaranteed to be under water
			float flElementVolume = DotProduct( CrossProduct( curVert, rootVert ), nextVert ) / 6;
			flElementVolume = fabs( flElementVolume );
			flSum += flElementVolume ;

			Vector vecElementCenter =  ( rootVert + curVert + nextVert ) * 0.25f;
			vecCenter += flElementVolume * vecElementCenter;
		}
	}

	Vector4D result;
#if 1
	result.Init( vecCenter * flSign, flSum * flSign );
#else
	result.Init( flSum > 1e-8f ? vecCenter / flSum : Vector( 0, 0, 0 ), flSum * flSign );
#endif
	return result;
}

/*Vector4D GetQuadBuoyancy( const Vector &pos, const Vector &a, const Vector &b, const Vector &n )
{
	Vector verts[4], verts2[10];
	uint numVerts = 4, numVerts2 = 0;

	Vector acrossb = CrossProduct( a, b );

	float flAreaXIntegral = acrossb.x * 4 * ( pos.x + n.x );
	float flAreaYIntegral = acrossb.y * 4 * ( pos.y + n.y );
	float flAreaZIntegral = acrossb.z * 4 * ( pos.z + n.z );
	
	Vector4D vecIntegral;
	vecIntegral.w = flAreaZIntegral;
	Vector center = pos + n;
	Assert(DotProduct(n, acrossb) > 0);

	float x0 = center.x, y0 = center.y, z0 = center.z;
	float xA = a.x, yA = a.y, zA = a.z;
	float xB = b.x, yB = b.y, zB = b.z;

	vecIntegral.Init(
		4* x0 *z0 + (xA* zA + xB*zB)/3,
		4* y0 *z0 + (yA* zA + yB*zB)/3,
		2* z0 *z0 + (zA* zA + zB*zB)/3,
		4* z0);

	return vecIntegral * acrossb.z;
}  */


inline void Swap(Vector&a, Vector&b)
{
	Vector t = a;
	a = b;
	b = t;
}

/*
Vector4D GetBuoyancy( const Vector &pos, Vector box[3] )
{
	float rcpZ[3];
	for(int i = 0; i < 3; ++i)
	{
		if( box[i].z < 0 )
			box[i] = -box[i];
		for(int j = 0; j < i; ++j)
		{
			if(box[j].z < box[i].z)
				Swap(box[i], box[j]);
		}
	}
	for(int i = 0; i < 3; ++i)
		rcpZ[i] = box[i].z > 1e-7f? 1 / box[i].z : 0;



	
	uint numVerts = 4, numVerts2 = 0;

	Vector acrossb = CrossProduct( a, b );

	float flAreaXIntegral = acrossb.x * 4 * ( pos.x + n.x );
	float flAreaYIntegral = acrossb.y * 4 * ( pos.y + n.y );
	float flAreaZIntegral = acrossb.z * 4 * ( pos.z + n.z );

	Vector4D vecIntegral;
	vecIntegral.w = flAreaZIntegral;
	Vector center = pos + n;
	Assert(DotProduct(n, acrossb) > 0);

	float x0 = center.x, y0 = center.y, z0 = center.z;
	float xA = a.x, yA = a.y, zA = a.z;
	float xB = b.x, yB = b.y, zB = b.z;

	vecIntegral.Init(
		4* x0 *z0 + (xA* zA + xB*zB)/3,
		4* y0 *z0 + (yA* zA + yB*zB)/3,
		2* z0 *z0 + (zA* zA + zB*zB)/3,
		4* z0);

	return vecIntegral * acrossb.z;
}*/




Vector4D operator % ( const Vector4D & a, const Vector4D & b )
{
	Vector4D ave;
	ave.Init( fabs( a.w + b.w ) > 1e-6f ? ( a.AsVector3D() * a.w + b.AsVector3D() * b.w ) / ( a.w + b.w ) : Vector( 0, 0, 0 ), a.w + b.w );
	return ave;
}


Vector4D GetBoxBuoyancy( const Vector& a, const Vector& b, const Vector& c, const Vector& pos )
{
	return GetPyramidBuoyancy( pos, a, b, c ) + GetPyramidBuoyancy( pos, b, a, -c ) + GetPyramidBuoyancy( pos, c, a, b ) + GetPyramidBuoyancy( pos, a, c, -b ) + GetPyramidBuoyancy( pos, b, c, a ) + GetPyramidBuoyancy( pos, c, b, -a );
}





void BenchmarkBoxBuoyancy( Vector a, const Vector& b, const Vector& c, const Vector& pos )
{
	int start, end;
	const int nIterations = 100000;
	Vector4D result;

	start = GetHardwareClockFast();
	result.Init(0,0,0,0);
	for ( int i = 0; i < nIterations; ++i )
	{
		result = result % (GetPyramidBuoyancy( pos, a, b, c ) % GetPyramidBuoyancy( pos, b, a, -c ) % GetPyramidBuoyancy( pos, c, a, b ) % GetPyramidBuoyancy( pos, a, c, -b ) % GetPyramidBuoyancy( pos, b, c, a ) % GetPyramidBuoyancy( pos, c, b, -a )) ;
		a += Vector(1e-24f, 1e-25f, 1e-26f);
	}
	end = GetHardwareClockFast();
	Msg( "Box Buoyancy Scalar Benchmark: %d ticks/box, volume %g \n", int32( ( end - start ) ) / nIterations, result.w / nIterations );
}


const Vector RotateZ( const Vector & in, float flDegrees )
{
	Vector res;
	VectorRotate( in, QAngle(0,flDegrees,0), res );
	return res;
}

const Vector RotateY( const Vector & in, float flDegrees )
{
	Vector res;
	VectorRotate( in, QAngle(flDegrees,0,0), res );
	return res;
}

const Vector Rotate( const Vector & in, const QAngle &a )
{
	Vector res;
	VectorRotate( in, a, res );
	return res;
}


struct Test_t
{
	void Test()
	{
		PermTest();
#ifdef _DEBUG
		BuoyancyTest();
#else
		Benchmark();
#endif
	}

	bool TestAllEqual( const fltx4 & a, const fltx4 & b )
	{
		return IsAllEqual( a, b );
	}

	void PermTest()
	{
#ifdef _DEBUG
		fltx4 f4Canonical = {0.125f, 1.125f, 2.125f, 3.125f};
		float flCanonical[4] = {0.125f, 1.125f, 2.125f, 3.125f};
		fltx4 f4CanonicalYXZW = {1.125f, 0.125f, 2.125f, 3.125f};
		fltx4 f4CanonicalXZYW = {0.125f, 2.125f, 1.125f, 3.125f};
		fltx4 f4CanonicalZYXW = {2.125f, 1.125f, 0.125f, 3.125f};
		fltx4 f4CanonicalXXYW = {0.125f, 0.125f, 1.125f, 3.125f};
		fltx4 f4CanonicalYZZW = {1.125f, 2.125f, 2.125f, 3.125f};

		Assert( TestAllEqual( f4Canonical, LoadUnalignedSIMD( flCanonical ) ) );

		for ( int i = 0; i < 4; ++i )
		{
			float flSubFloat = SubFloat( f4Canonical, i );
			Assert( fabs( flSubFloat - float( i ) - 0.125f ) < 1e-6f );
		}


		Assert( TestAllEqual( PermYXZW( f4Canonical ), ( f4CanonicalYXZW ) ) );
		Assert( TestAllEqual( PermXZYW( f4Canonical ), ( f4CanonicalXZYW ) ) );
		Assert( TestAllEqual( PermZYXW( f4Canonical ), ( f4CanonicalZYXW ) ) );
		Assert( TestAllEqual( PermXXYW( f4Canonical ), ( f4CanonicalXXYW ) ) );
		Assert( TestAllEqual( PermYZZW( f4Canonical ), f4CanonicalYZZW ) );
#endif
	}

	void BuoyancyTest()
	{
		Vector test[][3] =
		{
			{Vector( 1, 0, 0 ), Vector( 0, 0, 1 ), Vector( 0, 0, 0.0f )},
			{Vector( 1, 0, 1 ), Vector( -1, 0, 1 ), Vector( 0, 0, -0.5f )},
			{Vector( 0, 1, 1 ), Vector( 0, -1, 1 ), Vector( 0, 0, 0.0f )},
			{Vector( 0, 2, 2 ), Vector( 0, -2, 2 ), Vector( 0, 0, 0.0f )},
			{Vector( 5, 0, 5 ), Vector( -1, 0, 1 ), Vector( 0, 0, 0.0f )},
			{Vector( 2, 0, 1 ), Vector( -1, 0, 2 ), Vector( 0, 0, 0.0f )},
			{RotateZ(Vector( 1, 0, 1 ),45), RotateZ(Vector( -1, 0, 1 ),45), Vector( 0, 0, 0.0f )},
			{RotateZ(Vector( 1, 0, 1 ),30), RotateZ(Vector( -1, 0, 1 ),30), Vector( 0, 0, 0.5f )},
			{RotateZ(Vector( sqrtf(0.5f), 0, sqrtf(0.5f) ),45), RotateZ(Vector( 0, 1, 0 ),45), Vector( 0, 0, 0.5f )},
			{RotateY(RotateZ(Vector(1,0,0),45),atan(sqrtf(2))*180/M_PI),RotateY(RotateZ(Vector(0,1,0),45),atan(sqrtf(2))*180/M_PI), Vector(0,0,0)}, // unit cube with tips extended high/low
			{RotateY(RotateZ(Vector(1,0,0),45),atan(sqrtf(2))*180/M_PI),RotateY(RotateZ(Vector(0,1,0),45),atan(sqrtf(2))*180/M_PI), Vector(0,0,0.01f)}, // unit cube with tips extended high/low
			{RotateY(RotateZ(Vector(1,0,0),45),atan(sqrtf(2))*180/M_PI),RotateY(RotateZ(Vector(0,1,0),45),atan(sqrtf(2))*180/M_PI), Vector(0,0,0.25f)}, // unit cube with tips extended high/low
			{RotateY(RotateZ(Vector(1,0,0),45),atan(sqrtf(2))*180/M_PI),RotateY(RotateZ(Vector(0,1,0),45),atan(sqrtf(2))*180/M_PI), Vector(0,0,0.5f)}, // unit cube with tips extended high/low
			{RotateY(RotateZ(Vector(1,0,0),45),atan(sqrtf(2))*180/M_PI),RotateY(RotateZ(Vector(0,1,0),45),atan(sqrtf(2))*180/M_PI), Vector(0,0,-0.25f)}, // unit cube with tips extended high/low
			{RotateY(RotateZ(Vector(1,0,0),45),atan(sqrtf(2))*180/M_PI),RotateY(RotateZ(Vector(0,1,0),45),atan(sqrtf(2))*180/M_PI), Vector(0,0,-0.5f)}, // unit cube with tips extended high/low
			{Vector( 2, 1, 1 ), Vector( -1, 1, 1 ), Vector( 0, 0, 0.0f )},
			{Vector( 2, 1, 1 ), Vector( -1, 1, 1 ), Vector( 0, 0, 0.5f )},
			{Vector( 0, 2, 1 ).Normalized(), Vector( 1, -1, 2 ).Normalized(), Vector( 0, 0, 0 )},
			{Vector( -0.804987f, 0.250343f, -0.811212f ), Vector( 0.474009f, -0.625978f,-0.663551f ).Normalized(), Vector( 1, 0, 0 )}
		};

		float flMaxError = 0;
		for ( int nAttempt = 0, numAttempts = 1000000; nAttempt < numAttempts; ++nAttempt )
		{
			Vector a = RandomVector( -1, 1 ), c = RandomVector( -1, 1 ), b = CrossProduct( a, c ), pos = RandomVector( -2, 2 );
			c = CrossProduct( a, b ).Normalized() * RandomVector( 0, 1.75f ).x;
			if ( nAttempt < sizeof( test ) / sizeof( test[0] ) )
			{
				a = test[nAttempt][0];
				b = test[nAttempt][1];
				c = CrossProduct( a, b ).Normalized() /* a.Length()*/;
				pos = test[nAttempt][2];
			}
			//pos.x = 0;
			//pos.y = 0;
			//pos.z = 0;
			matrix3x4_t tm;
			tm.Init( a, b, c, pos );
			FourVectors box;
			box.LoadAndSwizzle( LoadUnalignedSIMD( &a ), LoadUnalignedSIMD( &b ), LoadUnalignedSIMD( &c ), LoadUnalignedSIMD( &pos ) );

			//fltx4 f4Result0 = GetBoxBuoyancy3x4( box );
			Vector4D result1 = GetBoxBuoyancy(a,b,c,pos);
			fltx4 f4ResultV2 = GetBoxBuoyancy3x4( box );
			fltx4 f4Residual = f4ResultV2 - LoadUnalignedSIMD( &result1 );
			float flError = sqrtf( SubFloat( Dot4SIMD( f4Residual, f4Residual ), 0 ) );
			if( flError > flMaxError )
			{
				flMaxError = flError;
				Msg( "%d. Error %g\n", nAttempt, flError);
			}

			Assert( IsAllGreaterThan( ReplicateX4( 1e-4f ), fabs( f4Residual ) ) );

			float flBoxVolume = a.Length() * b.Length() * c.Length() * 8; (void)(flBoxVolume); // debug only

			if ( ( nAttempt % ( numAttempts / 10 ) ) == 0 )
			{
				DevMsg( "." );
			}
		}
		DevMsg( "Buoyancy test completed, benchmarking\n" );
	}

	void Benchmark()
	{
		for ( int i = 0; i < 100; ++i )
		{
			Vector a = RandomVector( -1, 1 ), c = RandomVector( -1, 1 ), b = CrossProduct( a, c ), pos = RandomVector( -2, 2 );
			c = CrossProduct( a, b ).Normalized() * RandomVector( 0, 1.75f ).x;
			BenchmarkBoxBuoyancy4x3( LoadUnalignedSIMD( &a ), LoadUnalignedSIMD( &b ), LoadUnalignedSIMD( &c ), LoadUnalignedSIMD( &pos ) );
			BenchmarkBoxBuoyancy( a,b,c,pos );
		}
	}
};
static Test_t s_test;

void TestBuoyancy()
{
	s_test.Test();
}

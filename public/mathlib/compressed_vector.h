//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef COMPRESSED_VECTOR_H
#define COMPRESSED_VECTOR_H

#ifdef _WIN32
#pragma once
#endif

#include <math.h>
#include <float.h>

// For vec_t, put this somewhere else?
#include "basetypes.h"

// For rand(). We really need a library!
#include <stdlib.h>

#include "tier0/dbg.h"
#include "mathlib/vector.h"

#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#ifdef _PS3
#if defined(__SPU__)
#include <spu_intrinsics.h>
#include <vmx2spu.h>
#endif
#include <vectormath/cpp/vectormath_aos.h>
#endif

#if defined( _X360 )
#pragma bitfield_order( push, lsb_to_msb )
#elif defined( _PS3 )
#pragma ms_struct on
#pragma reverse_bitfields on
#endif

#ifdef OSX
#pragma GCC diagnostic ignored "-Wtautological-compare"
#endif

class Quaternion48;


FORCEINLINE fltx4 UnpackQuaternion48SIMD( const Quaternion48 * RESTRICT pVec );


//=========================================================
// fit a 3D vector into 32 bits
//=========================================================

class Vector32
{
public:
	// Construction/destruction:
	Vector32(void); 
	Vector32(vec_t X, vec_t Y, vec_t Z);

	// assignment
	Vector32& operator=(const Vector &vOther);
	operator Vector ();

private:
	unsigned short x:10;
	unsigned short y:10;
	unsigned short z:10;
	unsigned short exp:2;
};

inline Vector32& Vector32::operator=(const Vector &vOther)	
{
	CHECK_VALID(vOther);

	static float expScale[4] = { 4.0f, 16.0f, 32.f, 64.f };

	float fmax = MAX( fabs( vOther.x ), fabs( vOther.y ) );
	fmax = fpmax( fmax, fabs( vOther.z ) );

	for (exp = 0; exp < 3; exp++)
	{
		if (fmax < expScale[exp])
			break;
	}
	Assert( fmax < expScale[exp] );

	float fexp = 512.0f / expScale[exp];

	x = clamp( (int)(vOther.x * fexp) + 512, 0, 1023 );
	y = clamp( (int)(vOther.y * fexp) + 512, 0, 1023 );
	z = clamp( (int)(vOther.z * fexp) + 512, 0, 1023 );
	return *this; 
}


inline Vector32::operator Vector ()
{
	Vector tmp;

	static float expScale[4] = { 4.0f, 16.0f, 32.f, 64.f };

	float fexp = expScale[exp] / 512.0f;

	tmp.x = (((int)x) - 512) * fexp;
	tmp.y = (((int)y) - 512) * fexp;
	tmp.z = (((int)z) - 512) * fexp; 
	return tmp; 
}


//=========================================================
// Fit a unit vector into 32 bits
//=========================================================

class Normal32
{
public:
	// Construction/destruction:
	Normal32(void); 
	Normal32(vec_t X, vec_t Y, vec_t Z);

	// assignment
	Normal32& operator=(const Vector &vOther);
	operator Vector ();

private:
	unsigned short x:15;
	unsigned short y:15;
	unsigned short zneg:1;
};


inline Normal32& Normal32::operator=(const Vector &vOther)	
{
	CHECK_VALID(vOther);

	x = clamp( (int)(vOther.x * 16384) + 16384, 0, 32767 );
	y = clamp( (int)(vOther.y * 16384) + 16384, 0, 32767 );
	zneg = (vOther.z < 0);
	//x = vOther.x; 
	//y = vOther.y; 
	//z = vOther.z; 
	return *this; 
}


inline Normal32::operator Vector ()
{
	Vector tmp;

	tmp.x = ((int)x - 16384) * (1 / 16384.0);
	tmp.y = ((int)y - 16384) * (1 / 16384.0);
	tmp.z = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y );
	if (zneg)
		tmp.z = -tmp.z;
	return tmp; 
}


//=========================================================
// 64 bit Quaternion
//=========================================================

class Quaternion64
{
public:
	// Construction/destruction:
	Quaternion64(void); 
	Quaternion64(vec_t X, vec_t Y, vec_t Z);

	// assignment
	// Quaternion& operator=(const Quaternion64 &vOther);
	Quaternion64& operator=(const Quaternion &vOther);
	operator Quaternion () const;
	inline fltx4 LoadUnalignedSIMD() const; // load onto a SIMD register without assumptions of being on a 16byte boundary  

private:
	Quaternion64( uint64 xx, uint64 yy, uint64 zz, uint64 ww ) : x(xx), y(yy), z(zz), wneg(ww) {}; // stricly for static construction
	uint64 x:21;
	uint64 y:21;
	uint64 z:21;
	uint64 wneg:1;
};


inline Quaternion64::operator Quaternion ()	 const
{
#if defined(__SPU__)
	fltx4 tmpV;
	QuaternionAligned tmpQ;

	tmpV = LoadUnalignedSIMD();
	StoreAlignedSIMD( (float *)&tmpQ, tmpV );

	return tmpQ;
#else
	Quaternion tmp;

	// shift to -1048576, + 1048575, then round down slightly to -1.0 < x < 1.0
 	tmp.x = ((int)x - 1048576) * (1 / 1048576.5f);
 	tmp.y = ((int)y - 1048576) * (1 / 1048576.5f);
 	tmp.z = ((int)z - 1048576) * (1 / 1048576.5f);

	tmp.w = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z );
	if (wneg)
		tmp.w = -tmp.w;
	return tmp; 
#endif
}

inline Quaternion64& Quaternion64::operator=(const Quaternion &vOther)	
{
	CHECK_VALID(vOther);

	x = clamp( (int)(vOther.x * 1048576) + 1048576, 0, 2097151 );
	y = clamp( (int)(vOther.y * 1048576) + 1048576, 0, 2097151 );
	z = clamp( (int)(vOther.z * 1048576) + 1048576, 0, 2097151 );
	wneg = (vOther.w < 0);
	return *this; 
}

inline fltx4 Quaternion64::LoadUnalignedSIMD() const 
{
#ifdef _PS3 // assume little endian packing

#if 1

	const static u32x4 xmask = { 0x00000000, 0x001fffff, 0, 0 }; // bottom 21 bits ( 0 .. 20 ) true
	const static u32x4 ymask = { 0x000003ff, 0xffe00000, 0, 0 }; // bits 21 .. 41 true
	const static u32x4 zmask = { 0x7ffffC00, 0x00000000, 0, 0 }; // bits 42 .. 62 true
	const static u32x4 wmask = { 0x80000000, 0x00000000, 0, 0 }; // only bit 63 is true


	const u32x4 qbits = (u32x4)( ::LoadUnalignedSIMD( this ) ) ;
	// fish x, y, and z and put them into the the first words of their respective vec registers
	// the end type for these registers must be signed for the following subtract, BUT!
	// the shift has to happen as an UNSIGNED type so that it doesn't sign-extend.
	// the code as present assumes that the fused multiply-add operation has an intermediate
	// precision higher than 32 bits -- otherwise, we'll need to perform the initial subtract as an 
	// int op because of course 21 bits is right at the limit of floating point precision.
	i32x4 ix =  (i32x4) (ShiftLeftByBits<32>(vec_and( qbits, xmask ))); // shift x by eleven bits so its 21 bits of precision are sitting at the low end of the first word
	i32x4 iy =  (i32x4) (ShiftLeftByBits<11>(vec_and( qbits, ymask ))); // shift y, which straddles the first two words, left by 10 bits so its 21 bits of precision are sitting at the low end of the first word
	i32x4 iz =  (i32x4) (ShiftRightByBits<10>(vec_and( qbits, zmask ))); // shift z, which straddles the first two words, left by 31 bits so its 21 bits of precision are sitting at the low end of the first word

	/* // this is how to put them into their respective words instead (but we don't want to do that because we need a dot product) 
	i32x4 iy =  (i32x4) (ShiftRightByBits<22>(vec_and( qbits, ymask ))); // shift y, which straddles the first two words, right by 22 bits so its 21 bits of precision are sitting at the low end of the second word
	i32x4 iz =  (i32x4) (ShiftRightByBits<33>(vec_and( qbits, zmask ))); // shift z right by 33 bits so its 21 bits of precision are sitting at the low end of the third word
	*/
	i32x4 wsignbit = (i32x4) (ShiftRightByBits<96>(vec_and( qbits, wmask ))); // shift the w bit RIGHT so that it sits at the sign bit of the LAST word.

	// convert each of the vectors from int to float. (because of the way the pipeline is organized, 
	// it's as fast to do this as it would have been to do by combining them into one register above
	// and converting all at once.) Also, we can do the fixed point conversion in the vcfsx op. It'll
	// map us onto [0,2] which we'll shift to [-1,1] -- it includes the endpoints of unlike the float-
	// by-float conversion above, but the better stability of the vector quaternion ops makes that okay.
	const fltx4 ONE = LoadOneSIMD();
#if defined(__SPU__)
	fltx4 fx = SubSIMD( vec_ctf( ix, 20 ), ONE);
	fltx4 fy = SubSIMD( vec_ctf( iy, 20 ), ONE);
	fltx4 fz = SubSIMD( vec_ctf( iz, 20 ), ONE);
#else
	fltx4 fx = SubSIMD( vec_vcfsx( ix, 20 ), ONE);
	fltx4 fy = SubSIMD( vec_vcfsx( iy, 20 ), ONE);
	fltx4 fz = SubSIMD( vec_vcfsx( iz, 20 ), ONE);
#endif

	// compute the dot product
	fltx4 fw = MsubSIMD( fz, fz, ONE ); // 1 - z*z
	fltx4 fxsqysq = MaddSIMD( fy, fy, MulSIMD( fx,fx ) ); // x*x + y*y
	fw = SubSIMD( fw, fxsqysq ); // 1 - x*x - y*y - z*z
	fw = SqrtSIMD( fw ); // unfortunately we really do need full precision here

	fltx4 result = Compress4SIMD( fx, fy, fz, fw );
	// and for the coup de grace, set the sign bit of fw appropriately
	result = OrSIMD( result, (fltx4)wsignbit );

	return result;



#else 
	// original version 

	/*
	union Qmask {
	struct qq {
	Quaternion64 mask;
	uint64 padding;
	} asQ ;
	u32x4 asVec;

	Qmask( const Quaternion64 &m ) : mask(m) {}
	};
	*/
	const static u32x4 xmask = { 0xfffff800, 0x00000000, 0, 0 }; // top 21 bits ( 0 .. 20 ) true
	const static u32x4 ymask = { 0x000007ff, 0xffc00000, 0, 0 }; // bits 21 .. 41 true
	const static u32x4 zmask = { 0x00000000, 0x003ffffe, 0, 0 }; // bits 42 .. 62 true
	const static u32x4 wmask = { 0x00000000, 0x00000001, 0, 0 }; // only bit 63 is true

	const u32x4 qbits = (u32x4)( ::LoadUnalignedSIMD( this ) ) ;
	// fish x, y, and z and put them into the the first words of their respective vec registers
	// the end type for these registers must be signed for the following subtract, BUT!
	// the shift has to happen as an UNSIGNED type so that it doesn't sign-extend.
	// the code as present assumes that the fused multiply-add operation has an intermediate
	// precision higher than 32 bits -- otherwise, we'll need to perform the initial subtract as an 
	// int op because of course 21 bits is right at the limit of floating point precision.
	i32x4 ix =  (i32x4) (ShiftRightByBits<11>(vec_and( qbits, xmask ))); // shift x by eleven bits so its 21 bits of precision are sitting at the low end of the first word
	i32x4 iy =  (i32x4) (ShiftLeftByBits<10>(vec_and( qbits, ymask ))); // shift y, which straddles the first two words, left by 10 bits so its 21 bits of precision are sitting at the low end of the first word
	i32x4 iz =  (i32x4) (ShiftLeftByBits<31>(vec_and( qbits, zmask ))); // shift z, which straddles the first two words, left by 31 bits so its 21 bits of precision are sitting at the low end of the first word
	/* // this is how to put them into their respective words instead (but we don't want to do that because we need a dot product) 
	i32x4 iy =  (i32x4) (ShiftRightByBits<22>(vec_and( qbits, ymask ))); // shift y, which straddles the first two words, right by 22 bits so its 21 bits of precision are sitting at the low end of the second word
	i32x4 iz =  (i32x4) (ShiftRightByBits<33>(vec_and( qbits, zmask ))); // shift z right by 33 bits so its 21 bits of precision are sitting at the low end of the third word
	*/
	i32x4 wsignbit = (i32x4) (ShiftRightByBits<33>(vec_and( qbits, wmask ))); // shift the w bit RIGHT so that it sits at the sign bit of the LAST word.

	// convert each of the vectors from int to float. (because of the way the pipeline is organized, 
	// it's as fast to do this as it would have been to do by combining them into one register above
	// and converting all at once.) Also, we can do the fixed point conversion in the vcfsx op. It'll
	// map us onto [0,2] which we'll shift to [-1,1] -- it includes the endpoints of unlike the float-
	// by-float conversion above, but the better stability of the vector quaternion ops makes that okay.
	const fltx4 ONE = LoadOneSIMD();
#if defined(__SPU__)
	fltx4 fx = SubSIMD( vec_ctf( ix, 20 ), ONE);
	fltx4 fy = SubSIMD( vec_ctf( iy, 20 ), ONE);
	fltx4 fz = SubSIMD( vec_ctf( iz, 20 ), ONE);
#else
	fltx4 fx = SubSIMD( vec_vcfsx( ix, 20 ), ONE);
	fltx4 fy = SubSIMD( vec_vcfsx( iy, 20 ), ONE);
	fltx4 fz = SubSIMD( vec_vcfsx( iz, 20 ), ONE);
#endif

	// compute the dot product
	fltx4 fw = MsubSIMD( ONE, fz, fz ); // 1 - z*z
	fltx4 fxsqysq = MaddSIMD( fy, fy, MulSIMD( fx,fx ) ); // x*x + y*y
	fw = SubSIMD( fw, fxsqysq ); // 1 - x*x - y*y - z*z
	fw = SqrtSIMD( fw ); // unfortunately we really do need full precision here

	fltx4 result = Compress4SIMD( fx, fy, fz, fw );
	// and for the coup de grace, set the sign bit of fw appropriately
	result = OrSIMD( result, (fltx4)wsignbit );

	return result;

#endif

#elif 0 // basic C implementation (which ends up being slower than writing the whole Q onto the stack and then reading it back at once)
	struct { float x; float y; float z; float w; } tmp;

	tmp.x = ((int)x - 1048576) * (1 / 1048576.5f);
	tmp.y = ((int)y - 1048576) * (1 / 1048576.5f);
	tmp.z = ((int)z - 1048576) * (1 / 1048576.5f);
	tmp.w = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z );
	if (wneg)
		tmp.w = -tmp.w;
	
	fltx4 ret = { tmp.x, tmp.y, tmp.z, tmp.w };
	return ret;
#else // naive implementation (which ends up being faster than the explicit c imp above)
	const QuaternionAligned q(Quaternion(*this)) ;
	return LoadAlignedSIMD( &q );
#endif
}

//=========================================================
// 48 bit Quaternion
//=========================================================

class Quaternion48
{
public:
	// Construction/destruction:
	Quaternion48(void); 
	Quaternion48(vec_t X, vec_t Y, vec_t Z);

	// assignment
	// Quaternion& operator=(const Quaternion48 &vOther);
	Quaternion48& operator=(const Quaternion &vOther);
	operator Quaternion () const;
	inline fltx4 LoadUnalignedSIMD() const; // load onto a SIMD register without assumptions of being on a 16byte boundary  

//private:
	unsigned short x:16;
	unsigned short y:16;
	unsigned short z:15;
	unsigned short wneg:1;
};


inline Quaternion48::operator Quaternion ()	const
{
#if defined(__SPU__)

	fltx4 tmpV;
	QuaternionAligned tmpQ;

	tmpV = LoadUnalignedSIMD();
	StoreAlignedSIMD( (float *)&tmpQ, tmpV );

	tmpV = UnpackQuaternion48SIMD( this );
	StoreAlignedSIMD( (float *)&tmpQ, tmpV );

	return tmpQ;

#else

	Quaternion tmp;

	tmp.x = ((int)x - 32768) * (1 / 32768.5);
	tmp.y = ((int)y - 32768) * (1 / 32768.5);
	tmp.z = ((int)z - 16384) * (1 / 16384.5);
	tmp.w = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z );
	if (wneg)
		tmp.w = -tmp.w;
	return tmp; 

#endif
}


inline Quaternion48& Quaternion48::operator=(const Quaternion &vOther)	
{
	CHECK_VALID(vOther);

	x = clamp( (int)(vOther.x * 32768) + 32768, 0, 65535 );
	y = clamp( (int)(vOther.y * 32768) + 32768, 0, 65535 );
	z = clamp( (int)(vOther.z * 16384) + 16384, 0, 32767 );
	wneg = (vOther.w < 0);
	return *this; 
}

inline fltx4 Quaternion48::LoadUnalignedSIMD() const 
{
#ifdef _PS3 // assume little endian packing

	const static u32x4 xmask = { 0x00000000, 0xffff0000, 0, 0 }; 
	const static u32x4 ymask = { 0x0000ffff, 0x00000000, 0, 0 }; 
	const static u32x4 zmask = { 0x7fff0000, 0x00000000, 0, 0 }; 
	const static u32x4 wmask = { 0x80000000, 0x00000000, 0, 0 }; 

	const u32x4 qbits = (u32x4)( ::LoadUnalignedSIMD( this ) ) ;
	// fish x, y, and z and put them into the the first words of their respective vec registers
	i32x4 ix =  (i32x4) (ShiftLeftByBits<16>(vec_and( qbits, xmask ))); 
	i32x4 iy =  (i32x4) ((vec_and( qbits, ymask ))); 
	i32x4 iz =  (i32x4) (ShiftRightByBits<16>(vec_and( qbits, zmask ))); 

	// shift the w bit RIGHT so that it sits at the sign bit of the LAST word.
	i32x4 wsignbit = (i32x4) (ShiftRightByBits<96>(vec_and( qbits, wmask ))); 

	// convert each of the vectors from int to float. (because of the way the pipeline is organized, 
	// it's as fast to do this as it would have been to do by combining them into one register above
	// and converting all at once.) Also, we can do the fixed point conversion in the vcfsx op. It'll
	// map us onto [0,2] which we'll shift to [-1,1] -- it includes the endpoints of unlike the float-
	// by-float conversion above, but the better stability of the vector quaternion ops makes that okay.
	const fltx4 ONE = LoadOneSIMD();
#if defined(__SPU__)
	fltx4 fx = SubSIMD( vec_ctf( ix, 15 ), ONE);
	fltx4 fy = SubSIMD( vec_ctf( iy, 15 ), ONE);
	fltx4 fz = SubSIMD( vec_ctf( iz, 14 ), ONE);
#else
	fltx4 fx = SubSIMD( vec_vcfsx( ix, 15 ), ONE);
	fltx4 fy = SubSIMD( vec_vcfsx( iy, 15 ), ONE);
	fltx4 fz = SubSIMD( vec_vcfsx( iz, 14 ), ONE);
#endif

	// compute the dot product
	fltx4 fw = MsubSIMD( fz, fz, ONE ); // 1 - z*z
	fltx4 fxsqysq = MaddSIMD( fy, fy, MulSIMD( fx,fx ) ); // x*x + y*y
	fw = SubSIMD( fw, fxsqysq ); // 1 - x*x - y*y - z*z
	fw = SqrtSIMD( fw ); // unfortunately we really do need full precision here

	fltx4 result = Compress4SIMD( fx, fy, fz, fw );
	// and for the coup de grace, set the sign bit of fw appropriately
	result = OrSIMD( result, (fltx4)wsignbit );

	return result;


#elif 0 // basic C implementation (which ends up being slower than writing the whole Q onto the stack and then reading it back at once)
	struct { float x; float y; float z; float w; } tmp;

	tmp.x = ((int)x - 32768) * (1 / 32768.5);
	tmp.y = ((int)y - 32768) * (1 / 32768.5);
	tmp.z = ((int)z - 16384) * (1 / 16384.5);
	tmp.w = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z );
	if (wneg)
		tmp.w = -tmp.w;
	
	fltx4 ret = { tmp.x, tmp.y, tmp.z, tmp.w };
	return ret;
#else // naive implementation (which ends up being faster than the explicit c imp above)
	const QuaternionAligned q(Quaternion(*this)) ;
	return LoadAlignedSIMD( &q );
#endif
}


//=========================================================
// 48 bit sorted Quaternion
//=========================================================


class Quaternion48S
{
public:
	// Construction/destruction:
	Quaternion48S(void); 
	Quaternion48S(vec_t X, vec_t Y, vec_t Z);

	// assignment
	// Quaternion& operator=(const Quaternion48 &vOther);
	Quaternion48S& operator=(const Quaternion &vOther);
	operator Quaternion () const;
	operator fltx4 () const RESTRICT ;
//private:
	// shift the quaternion so that the largest value is recreated by the sqrt()
	// abcd maps modulo into quaternion xyzw starting at "offset"
	// "offset" is split into two 1 bit fields so that the data packs into 6 bytes (3 shorts)
	unsigned short a:15;		// first of the 3 consecutive smallest quaternion elements 
	unsigned short offsetH:1;	// high bit of "offset"
	unsigned short b:15;
	unsigned short offsetL:1;	// low bit of "offset"
	unsigned short c:15;
	unsigned short dneg:1;		// sign of the largest quaternion element
};

#define SCALE48S 23168.0f		// needs to fit 2*sqrt(0.5) into 15 bits.
#define SHIFT48S 16384			// half of 2^15 bits.

inline Quaternion48S::operator Quaternion ()	const
{
#if defined(__SPU__)

	fltx4 tmpV;
	QuaternionAligned tmpQ;

	tmpV = *this;
	StoreAlignedSIMD( (float *)&tmpQ, tmpV );

	return tmpQ;

#else

	Quaternion tmp;

	COMPILE_TIME_ASSERT( sizeof( Quaternion48S ) == 6 );

	float *ptmp = &tmp.x;
	int ia = offsetL + offsetH * 2;
	int ib = ( ia + 1 ) % 4;
	int ic = ( ia + 2 ) % 4;
	int id = ( ia + 3 ) % 4;
	ptmp[ia] = ( (int)a - SHIFT48S ) * ( 1.0f / SCALE48S );
	ptmp[ib] = ( (int)b - SHIFT48S ) * ( 1.0f / SCALE48S );
	ptmp[ic] = ( (int)c - SHIFT48S ) * ( 1.0f / SCALE48S );
	ptmp[id] = sqrt( 1.0f - ptmp[ia] * ptmp[ia] - ptmp[ib] * ptmp[ib] - ptmp[ic] * ptmp[ic] );
	if (dneg)
		ptmp[id] = -ptmp[id];

	return tmp; 

#endif
}

inline Quaternion48S& Quaternion48S::operator=(const Quaternion &vOther)	
{
	CHECK_VALID(vOther);

	const float *ptmp = &vOther.x;

	// find largest field, make sure that one is recreated by the sqrt to minimize error
	int i = 0;
	if ( fabs( ptmp[i] ) < fabs( ptmp[1] ) )
	{
		i = 1;
	}
	if ( fabs( ptmp[i] ) < fabs( ptmp[2] ) )
	{
		i = 2;
	}
	if ( fabs( ptmp[i] ) < fabs( ptmp[3] ) )
	{
		i = 3;
	}

	int offset = ( i + 1 ) % 4; // make "a" so that "d" is the largest element
	offsetL = offset & 1;
	offsetH = offset > 1;
	a = clamp( (int)(ptmp[ offset ] * SCALE48S) + SHIFT48S, 0, (int)(SCALE48S * 2) );
	b = clamp( (int)(ptmp[ ( offset + 1 ) % 4 ] * SCALE48S) + SHIFT48S, 0, (int)(SCALE48S * 2) );
	c = clamp( (int)(ptmp[ ( offset + 2 ) % 4 ] * SCALE48S) + SHIFT48S, 0, (int)(SCALE48S * 2) );
	dneg = ( ptmp[ ( offset + 3 ) % 4 ] < 0.0f );

	return *this; 
}


// decode onto a SIMD register
inline Quaternion48S::operator fltx4 ()	const RESTRICT
{
	AssertMsg1( (((uintp) this) & 1) == 0, "Quaternion48S is unaligned at %p\n", this );
#ifdef PLATFORM_PPC // this algorithm depends heavily on the Altivec permute op, for which there is no analogue in SSE. This function should not be used on PC.
	// define some vector constants. the shift-scale will be done as a fused multiply-add,
	// with the scale already distributed onto the shift (the part subtracted)
	const static fltx4 vrSCALE48S = { (1.0f / SCALE48S), (1.0f / SCALE48S), (1.0f / SCALE48S), (1.0f / SCALE48S) };
	const static fltx4 vrSHIFT48S = { ((float) -SHIFT48S) / SCALE48S, ((float) -SHIFT48S) / SCALE48S, ((float) -SHIFT48S) / SCALE48S, ((float) -SHIFT48S) / SCALE48S  };

	// start by hoisting the q48 onto a SIMD word. 
	u32x4 source = (u32x4) LoadUnalignedSIMD( this );
	const u32x4 ZERO = (u32x4) LoadZeroSIMD();
	// also hoist the offset into an int word. Hopefully this executes in parallel with the vector ops thanks to SUPERSCALAR!
	const unsigned int offset = offsetL | ( offsetH << 1 );
	const bi32x4 vDMask = (bi32x4) LoadAlignedSIMD( g_SIMD_ComponentMask[(offset+3)%4] ); // lets vsel poke D into the right word

#if 0 // This code can be used to deal with a situation where LoadUnalignedSIMD() fails to properly load
    // vectors starting on halfword boundaries (rather than 32-bit aligned). Because this is a 48-bit
	// structure, sometimes it'll only be 16-bit aligned. I expected that lvlx would always load from
	// a word boundary, requiring me to shift misaligned vectors over by 16 bits, but  evidently,
	// lvlx actually works even on halfword boundaries. Who knew!
	// Anyway, this code is still here in case the problem crops up, as a hint to both cause and solution.
	if ( ((unsigned int) this) & 2 ) 
	{
		source = ShiftLeftByBits<16>(source);
	}
#endif

	// mask out the offset and dneg bits. Because of the packing #pragmas, the one-bit fields are actually at the MSB
	// of the halfwords, not the LSB as you might expect.
	ALIGN16 const static uint32 vMaskTopBits[4]  = { 0x80008000, 0x80000000, 0, 0 }; // just the LSB of each the first three halfwords
	u32x4 abc = AndNotSIMD( (u32x4) LoadAlignedSIMD(vMaskTopBits), source ); // now this is just the A, B, C halfwords. 
	// Next, unpack abc as unsigned numbers. We can do this with a permute op. In fact, we can exploit
	// the integer pipe and load the offset while we're loading the SIMD numbers, then use the integer offset to select
	// the permute, which will therefore also perform the rotate that maps abc to their rightful destinations.
	// the masks below are for the vperm instruction, which is a byte-by-byte mapping from source to destination. 
	// it's assumed that the FIRST parameter to vperm will be ZERO, and the second the data.  (that makes the masks a little clearer)
	// in the simplest case -- imagine each letter below represents one byte; the source vector looks like
	// AABB CCxx xxxx xxxx. We're going to permute it onto the work register like
	// 00AA 00BB 00CC 0000
	ALIGN16 const static uint32 vPermutations[4][4] = {
		// offset = 0 means  a->x, b->y, c->z, d->w
		{  0x00001011, 0x00001213, 0x00001415, 0x00000000	}, 
		// offset = 1 means a->y, b->z, c->w, d->a
		{  0x00000000, 0x00001011, 0x00001213, 0x00001415 	}, 
		{  0x00001415, 0x00000000, 0x00001011, 0x00001213   }, 
		{  0x00001213, 0x00001415, 0x00000000, 0x00001011   }
	};
	// compute two permutations on the input data: one where the zero-word is always in the w component,
	// which lets us do a 3-way rather than 4-way dot product; and another where the zero-word corresponds to
	// wherever D is supposed to go. 
	// Even though this seems redundant, the duplicated work ends up fitting into the pipeline bubbles,
	// and the savings between a 4-way and 3-way dot seem to be about 3ns.
	u32x4 abcfordot = PermuteVMX( ZERO, abc, LoadAlignedSIMD( vPermutations[0] ) );
	abc = PermuteVMX( ZERO, abc, LoadAlignedSIMD( vPermutations[offset] ) );

	// turn each of the ints into floats. Because we masked out the one-bit field at the top,
	// We can think of this as a conversion from fixed-point where there's no fractional bit.
	// This is done in line with the shift-scale operation, which is itself fused.
	// we do this twice: once for the vector with the guaranteed zero w-word, and 
	// once for the vector rotated by the offset. 
	fltx4 vfDest = AndNotSIMD( vDMask, MaddSIMD( UnsignedFixedIntConvertToFltSIMD( abc, 0 ), vrSCALE48S, vrSHIFT48S ) );
	fltx4 vfDestForDot = MaddSIMD( UnsignedFixedIntConvertToFltSIMD( abcfordot, 0 ), vrSCALE48S, vrSHIFT48S ) ;
	// compute magnitude of the vector we know to have a 0 in the w word.
	const fltx4 vDot = Dot3SIMD( vfDestForDot, vfDestForDot );
	// recover the "D" word
	const fltx4 vD = SqrtSIMD( SubSIMD( LoadOneSIMD(), vDot ) );
	// mask D into the converted-and-offset vector, then return.
	return MaskedAssign( vDMask, dneg ? NegSIMD(vD) : vD, vfDest );
#else
	AssertMsg( false, "Quaternion48S::operator fltx4  is slow on this platform and should not be used.\n" );
	QuaternionAligned q( (Quaternion) *this );
	return LoadAlignedSIMD( &q );
#endif
}


//=========================================================
// 32 bit Quaternion
//=========================================================

class Quaternion32
{
public:
	// Construction/destruction:
	Quaternion32(void); 
	Quaternion32(vec_t X, vec_t Y, vec_t Z);

	// assignment
	// Quaternion& operator=(const Quaternion48 &vOther);
	Quaternion32& operator=(const Quaternion &vOther);
	operator Quaternion ();
	inline fltx4 LoadUnalignedSIMD() const; // load onto a SIMD register without assumptions of being on a 16byte boundary  

private:
	unsigned int x:11;
	unsigned int y:10;
	unsigned int z:10;
	unsigned int wneg:1;
};


inline Quaternion32::operator Quaternion ()	
{
#if defined(__SPU__)

	fltx4 tmpV;
	QuaternionAligned tmpQ;

	tmpV = LoadUnalignedSIMD();
	StoreAlignedSIMD( (float *)&tmpQ, tmpV );

	return tmpQ;

#else

	Quaternion tmp;

	tmp.x = ((int)x - 1024) * (1 / 1024.0);
	tmp.y = ((int)y - 512) * (1 / 512.0);
	tmp.z = ((int)z - 512) * (1 / 512.0);
	tmp.w = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z );
	if (wneg)
		tmp.w = -tmp.w;
	return tmp; 

#endif
}

inline Quaternion32& Quaternion32::operator=(const Quaternion &vOther)	
{
	CHECK_VALID(vOther);

	x = clamp( (int)(vOther.x * 1024) + 1024, 0, 2047 );
	y = clamp( (int)(vOther.y * 512) + 512, 0, 1023 );
	z = clamp( (int)(vOther.z * 512) + 512, 0, 1023 );
	wneg = (vOther.w < 0);
	return *this; 
}



inline fltx4 Quaternion32::LoadUnalignedSIMD() const 
{
#ifdef _PS3 // assume little endian packing

	const static u32x4 xmask = { 0x000007ff, 0, 0, 0 }; 
	const static u32x4 ymask = { 0x001ff800, 0, 0, 0 }; 
	const static u32x4 zmask = { 0x7fe00000, 0, 0, 0 }; 
	const static u32x4 wmask = { 0x80000000, 0, 0, 0 }; 

	const u32x4 qbits = (u32x4)( ::LoadUnalignedSIMD( this ) ) ;
	// fish x, y, and z and put them into the the first words of their respective vec registers
	i32x4 ix =  (i32x4) ((vec_and( qbits, xmask ))); 
	i32x4 iy =  (i32x4) (ShiftRightByBits<11>(vec_and( qbits, ymask ))); 
	i32x4 iz =  (i32x4) (ShiftRightByBits<21>(vec_and( qbits, zmask ))); 

	// shift the w bit RIGHT so that it sits at the sign bit of the LAST word.
	i32x4 wsignbit = (i32x4) (ShiftRightByBits<96>(vec_and( qbits, wmask ))); 

	// convert each of the vectors from int to float. (because of the way the pipeline is organized, 
	// it's as fast to do this as it would have been to do by combining them into one register above
	// and converting all at once.) Also, we can do the fixed point conversion in the vcfsx op. It'll
	// map us onto [0,2] which we'll shift to [-1,1] -- it includes the endpoints of unlike the float-
	// by-float conversion above, but the better stability of the vector quaternion ops makes that okay.
	const fltx4 ONE = LoadOneSIMD();
#if defined(__SPU__)
	fltx4 fx = SubSIMD( vec_ctf( ix, 10 ), ONE);
	fltx4 fy = SubSIMD( vec_ctf( iy, 9 ), ONE);
	fltx4 fz = SubSIMD( vec_ctf( iz, 9 ), ONE);
#else
	fltx4 fx = SubSIMD( vec_vcfsx( ix, 10 ), ONE);
	fltx4 fy = SubSIMD( vec_vcfsx( iy, 10 ), ONE);
	fltx4 fz = SubSIMD( vec_vcfsx( iz, 9 ), ONE);
#endif

	// compute the dot product
	fltx4 fw = MsubSIMD( fz, fz, ONE ); // 1 - z*z
	fltx4 fxsqysq = MaddSIMD( fy, fy, MulSIMD( fx,fx ) ); // x*x + y*y
	fw = SubSIMD( fw, fxsqysq ); // 1 - x*x - y*y - z*z
	fw = SqrtSIMD( fw ); // unfortunately we really do need full precision here

	fltx4 result = Compress4SIMD( fx, fy, fz, fw );
	// and for the coup de grace, set the sign bit of fw appropriately
	result = OrSIMD( result, (fltx4)wsignbit );

	return result;

#else

	struct { float x; float y; float z; float w; } tmp;

	tmp.x = ((int)x - 1024) * (1 / 1024.0);
	tmp.y = ((int)y - 512) * (1 / 512.0);
	tmp.z = ((int)z - 512) * (1 / 512.0);
	tmp.w = sqrt( 1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z );
	if (wneg)
		tmp.w = -tmp.w;

	fltx4 ret = { tmp.x, tmp.y, tmp.z, tmp.w };
	return ret;

#endif
}


//=========================================================
// 16 bit float
//=========================================================


const int float32bias = 127;
const int float16bias = 15;

const float maxfloat16bits = 65504.0f;

class float16
{
public:
	// float16() {};
	//float16( float f ) { m_storage.rawWord = ConvertFloatTo16bits(f); }
	float16& operator=(const unsigned short &other)  { m_storage.rawWord = other; return *this; };

	void Init() { m_storage.rawWord = 0; }
//	float16& operator=(const float16 &other) { m_storage.rawWord = other.m_storage.rawWord; return *this; }
	//	float16& operator=(const float &other) { m_storage.rawWord = ConvertFloatTo16bits(other); return *this; }
//	operator unsigned short () { return m_storage.rawWord; }
//	operator float () { return Convert16bitFloatTo32bits( m_storage.rawWord ); }
	unsigned short GetBits() const 
	{ 
		return m_storage.rawWord; 
	}
	float GetFloat() const 
	{ 
		return Convert16bitFloatTo32bits( m_storage.rawWord ); 
	}
	void SetFloat( float in ) 
	{ 
		m_storage.rawWord = ConvertFloatTo16bits( in ); 
	}

	bool IsInfinity() const
	{
		return m_storage.bits.biased_exponent == 31 && m_storage.bits.mantissa == 0;
	}
	bool IsNaN() const
	{
		return m_storage.bits.biased_exponent == 31 && m_storage.bits.mantissa != 0;
	}

	bool operator==(const float16 other) const { return m_storage.rawWord == other.m_storage.rawWord; }
	bool operator!=(const float16 other) const { return m_storage.rawWord != other.m_storage.rawWord; }
	
//	bool operator< (const float other) const	   { return GetFloat() < other; }
//	bool operator> (const float other) const	   { return GetFloat() > other; }

	template< bool BRANCHLESS > // allows you to force branchy/branchless implementation regardless of the current platform
	static unsigned short ConvertFloatTo16bitsNonDefault( float input );
	static float Convert16bitFloatTo32bits( unsigned short input );
	
	// a special case useful for the pixel writer: take four input float values, which are already in memory (not on registers),
	// convert them all at once and write them sequentially through the output pointer.
	static void ConvertFourFloatsTo16BitsAtOnce( float16 * RESTRICT pOut,
		const float *a, const float *b, const float *c, const float *d  );
	
	// unfortunately, function templates can't have default template parameters in 2010-era C++ 
	inline static unsigned short ConvertFloatTo16bits( float input )
	{	// default to branchless on ppc and branchy on x86
#ifdef PLATFORM_PPC
		return ConvertFloatTo16bitsNonDefault<true>(input);
#else
		return ConvertFloatTo16bitsNonDefault<false>(input);
#endif
	}	

protected:
	union float32bits
	{
		float rawFloat;
		uint32 rawAsInt;
		struct 
		{
			unsigned int mantissa : 23;
			unsigned int biased_exponent : 8;
			unsigned int sign : 1;
		} bits;
	};

	union float16bits
	{
		unsigned short rawWord;
		struct
		{
			unsigned short mantissa : 10;
			unsigned short biased_exponent : 5;
			unsigned short sign : 1;
		} bits;
	};

	static bool IsNaN( float16bits in )
	{
		return in.bits.biased_exponent == 31 && in.bits.mantissa != 0;
	}
	static bool IsInfinity( float16bits in )
	{
		return in.bits.biased_exponent == 31 && in.bits.mantissa == 0;
	}

	// 0x0001 - 0x03ff
	float16bits m_storage;
};

class float16_with_assign : public float16
{
public:
	float16_with_assign() {}
	float16_with_assign( float f ) { m_storage.rawWord = ConvertFloatTo16bits(f); }

	float16& operator=(const float16 &other) { m_storage.rawWord = ((float16_with_assign &)other).m_storage.rawWord; return *this; }
	float16& operator=(const float &other) { m_storage.rawWord = ConvertFloatTo16bits(other); return *this; }
//	operator unsigned short () const { return m_storage.rawWord; }
	operator float () const { return Convert16bitFloatTo32bits( m_storage.rawWord ); }
};

//=========================================================
// Fit a 3D vector in 48 bits
//=========================================================

class Vector48
{
public:
	// Construction/destruction:
	Vector48(void) {}
	Vector48(vec_t X, vec_t Y, vec_t Z) { x.SetFloat( X ); y.SetFloat( Y ); z.SetFloat( Z ); }

	// assignment
	Vector48& operator=(const Vector &vOther);
	operator Vector ();

	const float operator[]( int i ) const { return (((float16 *)this)[i]).GetFloat(); }

	float16 x;
	float16 y;
	float16 z;
};

// The uses of isel below are malformed because the first expression is unsigned and thus always >= 0,
// so this whole expression maps to a simple assignment. This was found through a noisy clang
// warning. I am preprocessing this out until it is needed.
#if 0
inline void float16::ConvertFourFloatsTo16BitsAtOnce( float16 * RESTRICT pOut,
											const float *a, const float *b, const float *c, const float *d  )
{
	COMPILE_TIME_ASSERT( sizeof(float) == 4 );
	// being meant for use on the PPC, this is tuned for that. 
	// it is mostly branchless, except for the large outer for loop,
	// since there's enough instructions inside that unrolling is
	// a bad idea. This fucntion is four-at-once to simplify SIMDifying in the
	// future should a convenient SIMD way to decimate emerge
	// Also, because this is only used for the special case of converting
	// float arrays into float16 GPU textures, this turns denorms into zeroes
	// and infinities into MAXFLTs, since the shader can't deal with nonfinite
	// numbers anyway.

	// alias the input floats onto a union giving their mantissa etc
	const float32bits * const inFloat[4] = {  
		reinterpret_cast<const float32bits *>(a),
		reinterpret_cast<const float32bits *>(b),
		reinterpret_cast<const float32bits *>(c),
		reinterpret_cast<const float32bits *>(d) };

	const static unsigned int maxfloat16bitsAsInt = 0x477FE000; // 65504.0f

	const static unsigned int SIGNBIT = 0x80000000;

	
	for ( int i = 0 ; i < 4 ; ++i ) // performs better not unrolled (less stack spilling)
	{	
		unsigned int onGPR = inFloat[i]->rawAsInt;

		// make a mask for each word; will be all 1's if the float is 
		// negative, all 0s if it is positive. Can do this just by
		// using arithmetic shift to smear out the sign bit.
		int isNegative = ((int) onGPR) >> 31;

		// clamp to be within -maxfloat16bits, maxfloat16bits
		// can't just use isel because IEEE754 floats are sign-magnitude, not two's comp. However,
		// positive IEEE754s can be compared as if they were ints. So, we need to do a little extra
		// work to test the negative case efficiently.
		// clamp to -maxfloat16
#error See above for explanation of why this and other uses of isel in this file are broken.
		int clampedNeg = isel( ((int)(onGPR & ~SIGNBIT)) -  maxfloat16bitsAsInt, // -in >= maxfloatbits so in <= -maxfloat
			maxfloat16bitsAsInt | SIGNBIT, // -65504.0f
			onGPR	);
		// clamp to +maxfloat16
		int clampedPos = isel( ((int)(onGPR)) -  maxfloat16bitsAsInt, // in >= maxfloatbits 
			maxfloat16bitsAsInt , // -65504.0f
			onGPR	);

		// take advantage of PPC's andc operator to effectively do a masked-move
		onGPR = ( clampedNeg & isNegative ) | ( clampedPos & ~isNegative );
	

		// fish out the input exponent and mantis fields directly (using the union induces an LHS)
		int inExponent = (onGPR & 0x7f800000) >> 23;
		unsigned int inMantissa = (onGPR & 0x007FFFFF);

		int exponent = inExponent - 127 + 15; // rebias the exponent
		unsigned int mantissa = isel( exponent, inMantissa >> 13, (unsigned) 0 ); // squash the mantissa to zero if the number is too small to represent (no denorms)

		float16bits output;
		// saturate the mantissa if rebiased exponent >= 31 (too big to store) 
		output.bits.mantissa = isel( exponent - 31, (unsigned) 0x3ff, mantissa );
		// clamp the exponent to 0..30
		output.bits.biased_exponent = isel( exponent, isel( exponent - 31, 30, exponent ), 0 );
		output.bits.sign = isNegative; //  this doesn't lhs, but instead issues the insrdi op to a word on GPR
		pOut[i].m_storage.rawWord = output.rawWord;
	}
}
#endif

#ifdef _X360
#define __cntlzw _CountLeadingZeros
#endif

template< bool BRANCHLESS >
inline unsigned short float16::ConvertFloatTo16bitsNonDefault( float input )
{ 
	float16bits output;
	float32bits inFloat;
	//if ( !BRANCHLESS ) // x86 code
	{
		if ( input > maxfloat16bits )
			input = maxfloat16bits;
		else if ( input < -maxfloat16bits )
			input = -maxfloat16bits;


		inFloat.rawFloat = input;

	}
	/*
	// The use of isel is incorrect because the first expression is unsigned and therefore always passes
	// the test.
	else // PPC code
	{
		// force the float onto the stack and then a GPR so we eat the LHS only once.
		// you can't just write to one union member and then read back another; 
		// the compiler is inconsistent about supporting that kind of type-punning. 
		// (ie, it will work in one file, but not another.)
		memcpy(&inFloat.rawFloat, &input, sizeof(inFloat.rawFloat));
		// inFloat.rawFloat = input;
		// clamp using the GPR
		{
			const unsigned int maxfloat16bitsAsInt = 0x477FE000; // 65504.0f
			// clamp to be <= maxfloat16bits
			uint32 &rawint = inFloat.rawAsInt; // <--- lhs
			if ( rawint & 0x80000000 ) // negative
			{
				// because floats are sign-magnitude, not two's comp, need to 
				// flip the int positive briefly to do the isel comparison
#error See above for explanation of why this and other uses of isel in this file are broken.
				rawint = isel( ((int)(rawint & ~0x80000000)) -  maxfloat16bitsAsInt, // -in >= maxfloatbits so in <= -maxfloat
					maxfloat16bitsAsInt | 0x80000000, // -65504.0f
					rawint	);
			}
			else // positive
			{
				rawint = isel( ((int)(rawint)) -  maxfloat16bitsAsInt, // in >= maxfloatbits 
					maxfloat16bitsAsInt , // -65504.0f
					rawint	);
			}
		}
	}
	*/
	output.bits.sign = inFloat.bits.sign;

	if ( (inFloat.bits.biased_exponent==0) ) 
	{ 
		// zero and denorm both map to zero
		output.bits.mantissa = 0;
		output.bits.biased_exponent = 0;
	}
	else if ( inFloat.bits.biased_exponent==0xff )
	{
		if ( !BRANCHLESS )
		{
			if ( (inFloat.bits.mantissa==0) ) 
			{ 
				/*
				// infinity
				output.bits.mantissa = 0;
				output.bits.biased_exponent = 31;
				*/

				// infinity maps to maxfloat
				output.bits.mantissa = 0x3ff;
				output.bits.biased_exponent = 0x1e;
			}
			else if ( (inFloat.bits.mantissa!=0) ) 
			{ 
				/*
				// NaN
				output.bits.mantissa = 1;
				output.bits.biased_exponent = 31;
				*/

				// NaN maps to zero
				output.bits.mantissa = 0;
				output.bits.biased_exponent = 0;
			}
		}
		else // branchless, only meant for PPC really bc needing the cntlzw op.
		{
			// else if ( inFloat.bits.biased_exponent==0xff )  // either infinity (biased_exponent is 0xff) or NaN.
			{
#ifdef PLATFORM_PPC
#if defined(__SPU__)
				int mantissamask = __builtin_clz( output.bits.mantissa ) - 32; // this is 0 if the mantissa is zero, and negative otherwise
#else
				int mantissamask = __cntlzw( output.bits.mantissa ) - 32; // this is 0 if the mantissa is zero, and negative otherwise
#endif
#else
				int mantissamask = output.bits.mantissa ? -1 : 0;
#endif
				output.bits.mantissa		= isel( mantissamask, 0x3ff, 0 ); //infinity maps to maxfloat, NaN to zero
				output.bits.biased_exponent = isel( mantissamask, 0x1e, 0 );
				output.bits.sign = inFloat.bits.sign;
			}
		}
	}
	else 
	{ 
		// regular number
		int new_exp = inFloat.bits.biased_exponent-float32bias;
		// it's actually better to branch in these cases on PPC, 
		// because the variable bit shift is such a massive penalty 
		// that it's worth a branch penalty to avoid it.
		if (new_exp<-24) 
		{ 
			// this maps to 0
			output.bits.mantissa = 0;
			output.bits.biased_exponent = 0;
		}

		if (new_exp<-14) 
		{
			// this maps to a denorm
			output.bits.biased_exponent = 0;
			unsigned int exp_val = ( unsigned int )( -14 - new_exp );
			if( exp_val > 0 && exp_val < 11 )
			{
				output.bits.mantissa = ( 1 << ( 10 - exp_val ) ) + ( inFloat.bits.mantissa >> ( 13 + exp_val ) );
			}
		}
		else if (new_exp>15) 
		{ 
#if 0
			// map this value to infinity
			output.bits.mantissa = 0;
			output.bits.biased_exponent = 31;
#else
			// to big. . . maps to maxfloat
			output.bits.mantissa = 0x3ff;
			output.bits.biased_exponent = 0x1e;
#endif
		}
		else 
		{
			output.bits.biased_exponent = new_exp+15;
			output.bits.mantissa = (inFloat.bits.mantissa >> 13);
		}
		

	}
	return output.rawWord;
}

inline float float16::Convert16bitFloatTo32bits( unsigned short input )
{
	float32bits output;
	const float16bits &inFloat = *((float16bits *)&input);

	if( IsInfinity( inFloat ) )
	{
		return maxfloat16bits * ( ( inFloat.bits.sign == 1 ) ? -1.0f : 1.0f );
	}
	if( IsNaN( inFloat ) )
	{
		return 0.0;
	}
	if( inFloat.bits.biased_exponent == 0 && inFloat.bits.mantissa != 0 )
	{
		// denorm
		const float half_denorm = (1.0f/16384.0f); // 2^-14
		float mantissa = ((float)(inFloat.bits.mantissa)) / 1024.0f;
		float sgn = (inFloat.bits.sign)? -1.0f :1.0f;
		output.rawFloat = sgn*mantissa*half_denorm;
	}
	else
	{
		// regular number
		unsigned mantissa = inFloat.bits.mantissa;
		unsigned biased_exponent = inFloat.bits.biased_exponent;
		unsigned sign = ((unsigned)inFloat.bits.sign) << 31;
		biased_exponent = ( (biased_exponent - float16bias + float32bias) * (biased_exponent != 0) ) << 23;
		mantissa <<= (23-10);

		*((unsigned *)&output) = ( mantissa | biased_exponent | sign );
	}

	return output.rawFloat;
}




inline Vector48& Vector48::operator=(const Vector &vOther)	
{
	CHECK_VALID(vOther);

	x.SetFloat( vOther.x );
	y.SetFloat( vOther.y );
	z.SetFloat( vOther.z );
	return *this; 
}


inline Vector48::operator Vector ()
{
	Vector tmp;

	tmp.x = x.GetFloat();
	tmp.y = y.GetFloat();
	tmp.z = z.GetFloat(); 

	return tmp;
}

//=========================================================
// Fit a 2D vector in 32 bits
//=========================================================

class Vector2d32
{
public:
	// Construction/destruction:
	Vector2d32(void) {}
	Vector2d32(vec_t X, vec_t Y) { x.SetFloat( X ); y.SetFloat( Y ); }

	// assignment
	Vector2d32& operator=(const Vector &vOther);
	Vector2d32& operator=(const Vector2D &vOther);

	operator Vector2D ();

	void Init( vec_t ix = 0.f, vec_t iy = 0.f);

	float16_with_assign x;
	float16_with_assign y;
};

inline Vector2d32& Vector2d32::operator=(const Vector2D &vOther)	
{
	x.SetFloat( vOther.x );
	y.SetFloat( vOther.y );
	return *this; 
}

inline Vector2d32::operator Vector2D ()
{
	Vector2D tmp;

	tmp.x = x.GetFloat();
	tmp.y = y.GetFloat();

	return tmp;
}

inline void Vector2d32::Init( vec_t ix, vec_t iy )
{
	x.SetFloat(ix);
	y.SetFloat(iy);
}





//=========================================================
//      FAST SIMD BATCH OPERATIONS
//=========================================================

#ifdef _X360
//// Compressed vector formats: unpack Vector48 and Quaternion48 onto SIMD registers.
// Only available on 360 for now because SSE1 lacks the necessary operations. SSE2 could
// do it but we can't count on that yet.
// If you have many v48's or q48's to stream, please note the functions designed to
// work on them many at a time.

extern const uint16 ALIGN16 g_SIMD_Quat48_Unpack_Shift[]; //< Shuffles the z component of the quat48 left by one bit.
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute0[16];
extern const fltx4 g_SIMD_Quat48_Unpack_Magic_Constants;
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute1[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute2[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute3[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute4[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute5[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute6[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute7[16];

// unpack a single vector48 at the pointer into the x,y,z components of a fltx4.
// the w is total garbage.
FORCEINLINE fltx4 UnpackVector48SIMD( const Vector48 *pVec )
{
	// load the three 16-bit floats into the first 48 bits of ret:
	fltx4 ret = XMLoadVector4((const void *)&pVec->x); 
	// shuffle the top 64 bits of ret down to the least significant (the z,w) -- 16 of those bits are garbage.
	ret = __vrlimi( ret, ret, 2 | 1, 2 ); // rotate left by 2 words and insert into z,w components
	// now unpack the 16-bit floats into 32-bit floats. This is a hardware op, woohoo!
	ret = __vupkd3d( ret , VPACK_FLOAT16_4 );

	return ret;
}

// unpack a single Quaternion48 at the pointer into the x,y,z,w components of a fltx4
// FIXME!!!  If we need a version of this that runs on 360, there is a work-in-progress version that hasn't been debugged lower in the file.
FORCEINLINE fltx4 UnpackQuaternion48SIMD( const Quaternion48 * RESTRICT pVec )
{
	// A quaternion 48 stores the x and y components as 0..65535 , which is almost mapped onto -1.0..1.0 via (x - 32768) / 32768.5 .
	// z is stored as 0..32767, which is almost mapped onto -1..1 via (z - 16384) / 16384.5 .
	// w is inferred from 1 - the dot product of the other tree components. the top bit of what would otherwise be the 16-bit z is
	// w's sign bit.
	fltx4 q16s = XMLoadVector3((const void *)pVec);
	fltx4 shift = __lvx(&g_SIMD_Quat48_Unpack_Shift, 0); // load the aligned shift mask that we use to shuffle z.
	fltx4 permute = __lvx(&g_SIMD_Quat48_Unpack_Permute0, 0); // load the permute word that shuffles x,y,z into their own words
	bool wneg = pVec->wneg; // loading pVec into two different kinds of registers -- but not shuffling between (I hope!) so no LHS.

	q16s = __vperm( q16s, Four_Threes, permute ); // permute so that x, y, and z are now each in their own words. The top half is the floating point rep of 3.0f
	q16s = __vslh(q16s, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)

	// each word of q16s contains 3.0 + n * 2^-22 -- convert this so that we get numbers on the range -1..1
	const fltx4 vUpkMul = SplatXSIMD(g_SIMD_Quat48_Unpack_Magic_Constants); // { UnpackMul16s, UnpackMul16s, UnpackMul16s, UnpackMul16s };
	const fltx4 vUpkAdd = SplatYSIMD(g_SIMD_Quat48_Unpack_Magic_Constants);

	/*
	fltx4 ret = __vcfux( q16s, 0 ); // convert from uint16 to floats.

	// scale from 0..65535 to -1..1 : tmp.x = ((int)x - 32768) * (1 / 32768.0);
	ret = __vmaddfp( ret, g_SIMD_Quat48_DivByU15, Four_NegativeOnes  );
	*/
	fltx4 ret = __vmaddfp( q16s, vUpkMul, vUpkAdd );

	// now, work out what w must be. 
	fltx4 dotxyz = Dot3SIMD( ret, ret ); // all components are dot product of ret w/ self.
	dotxyz = ClampVectorSIMD( dotxyz, Four_Zeros, Four_Ones );

	fltx4 ww = SubSIMD( Four_Ones, dotxyz ); // all components are 1 - dotxyz
	ww = SqrtSIMD(ww); // all components are sqrt(1-dotxyz)
	if (wneg)
	{
		ret = __vrlimi( ret, NegSIMD(ww), 1, 0 ); // insert one element from the ww vector into the w component of ret
	}
	else
	{
		ret = __vrlimi( ret, ww, 1, 0 ); // insert one element from the ww vector into the w component of ret
	}
	return ret;
}

// Many-at-a-time unpackers. 


/// Unpack eight consecutive Vector48's in memory onto eight SIMD registers.
/// The Vector48 pointer must be 16-byte aligned. Eight Vector48s add up 
/// to 48 bytes long. You should maybe think about prefetching.
FORCEINLINE void UnpackEightVector48SIMD( fltx4 &out1, fltx4 &out2, fltx4 &out3, fltx4 &out4,
										 fltx4 &out5, fltx4 &out6, fltx4 &out7, fltx4 &out8,
										 Vector48 * RESTRICT pVecs )
{
	AssertMsg((reinterpret_cast<unsigned int>(pVecs) & 0x0F) == 0, "Input to UnpackEightVector48SIMD is not 16-byte aligned." );

	// first load the data onto three packed SIMD vectors, which contain eight Vector48s between them. 
	// I've named them very explicitly so you can follow the movement of the input data.
	fltx4 x0y0z0x1y1z1x2y2, z2x3y3z3x4y4z4x5, y5z5x6y6z6x7y7z7;
	x0y0z0x1y1z1x2y2 = __lvx( pVecs, 0 ); // load reintrepret_cast<fltx 4 *>(pVecs) + 0
	z2x3y3z3x4y4z4x5 = __lvx( pVecs, 16 ); // load  reintrepret_cast<fltx 4 *>(pVecs) + 1
	y5z5x6y6z6x7y7z7 = __lvx( pVecs, 32 ); // load reintrepret_cast<fltx 4 *>(pVecs) + 2

	// Now, start unpacking. The __vupkd3d operation can turn 16-bit floats into 32-bit floats in a single op!
	// It converts the contents of the z and w words of the input fltx4 , so we need to process a word to do 
	// one half, then rotate it to do the other half.
	fltx4 y1z1x2y2 = __vupkd3d( x0y0z0x1y1z1x2y2 , VPACK_FLOAT16_4 );
	x0y0z0x1y1z1x2y2 = __vrlimi( x0y0z0x1y1z1x2y2, x0y0z0x1y1z1x2y2, 0xf, 2 ); // actually y1z1x2y2x0y0z0x1 now. For perf it's important that the first param to vrlimi also be the assignee.
	fltx4 x4y4z4x5 = __vupkd3d( z2x3y3z3x4y4z4x5 , VPACK_FLOAT16_4 );
	z2x3y3z3x4y4z4x5 = __vrlimi( z2x3y3z3x4y4z4x5, z2x3y3z3x4y4z4x5, 0xf, 2 ); 
	fltx4 z6x7y7z7 = __vupkd3d( y5z5x6y6z6x7y7z7 , VPACK_FLOAT16_4 );
	y5z5x6y6z6x7y7z7 = __vrlimi( y5z5x6y6z6x7y7z7, y5z5x6y6z6x7y7z7, 0xf, 2 ); 
	fltx4 x0y0z0x1 = __vupkd3d( x0y0z0x1y1z1x2y2 , VPACK_FLOAT16_4 );
	fltx4 z2x3y3z3 = __vupkd3d( z2x3y3z3x4y4z4x5 , VPACK_FLOAT16_4 );
	fltx4 y5z5x6y6 = __vupkd3d( y5z5x6y6z6x7y7z7 , VPACK_FLOAT16_4 );

	// permute to populate the out-registers with part of their vectors:
	out1 = x0y0z0x1;	// DONE
	out2 = __vpermwi( y1z1x2y2, VPERMWI_CONST(0, 0, 1, 0) ); // __y1z1__
	out3 = __vpermwi( y1z1x2y2, VPERMWI_CONST(2, 3, 0, 0) ); // x2y2____
	out4 = __vpermwi( z2x3y3z3, VPERMWI_CONST(1, 2, 3, 0) ); // x3y3z3__ // DONE
	out5 = x4y4z4x5;	// DONE
	out6 = __vpermwi( y5z5x6y6, VPERMWI_CONST(0, 0, 1, 0) ); // __y5z5__
	out7 = __vpermwi( y5z5x6y6, VPERMWI_CONST(2, 3, 0, 0) ); // x6y6____
	out8 = __vpermwi( z6x7y7z7, VPERMWI_CONST(1, 2, 3, 0) ); // x7y7z7__ // DONE

	// there are four more to finish, which we do with a masked insert
	out2 = __vrlimi( out2, x0y0z0x1, 8, 3 ); // x1y1z1__ 
	out3 = __vrlimi( out3, z2x3y3z3, 2, 2 ); // x2y2x2__
	out6 = __vrlimi( out6, x4y4z4x5, 8, 3 ); // x5y5z5__ 
	out7 = __vrlimi( out7, z6x7y7z7, 2, 2 ); // x6y6z6__

	// and we're done!
}



/// Unpack eight consecutive Quaternion48's in memory onto eight SIMD registers.
/// The Quaternion48 pointer must be 16-byte aligned. Eight Quaternion48s add up 
/// to 48 bytes long. You should maybe think about prefetching.
//
// This could  be improved with verticalization, so that the W sqrts happen
// on two rather than eight vectors, and then transposing. This would make
// the initial permuatation even more complicated.
FORCEINLINE void UnpackEightQuaternion48SIMD( fltx4 &out0, fltx4 &out1, fltx4 &out2, fltx4 &out3,
											 fltx4 &out4, fltx4 &out5, fltx4 &out6, fltx4 &out7,
											 Quaternion48 * RESTRICT pVecs )
{
	AssertMsg((reinterpret_cast<unsigned int>(pVecs) & 0x0F) == 0, "Input to UnpackEightQuaternion48SIMD is not 16-byte aligned." );
	// each word of q16s contains 3.0 + n * 2^-22 -- convert this so that we get numbers on the range -1..1
	const fltx4 vUpkMul = SplatXSIMD(g_SIMD_Quat48_Unpack_Magic_Constants); // { UnpackMul16s, UnpackMul16s, UnpackMul16s, UnpackMul16s };
	const fltx4 vUpkAdd = SplatYSIMD(g_SIMD_Quat48_Unpack_Magic_Constants);
	const fltx4 shift = __lvx(&g_SIMD_Quat48_Unpack_Shift, 0); // load the aligned shift mask that we use to shuffle z left by one bit.

	// first load the data onto three packed SIMD vectors, which contain eight Quaternion48s between them. 
	// I've named them very explicitly so you can follow the movement of the input data.
	fltx4 x0y0z0x1y1z1x2y2, z2x3y3z3x4y4z4x5, y5z5x6y6z6x7y7z7;
	x0y0z0x1y1z1x2y2 = __lvx( pVecs, 0 ); // load reintrepret_cast<fltx 4 *>(pVecs) + 0
	z2x3y3z3x4y4z4x5 = __lvx( pVecs, 16 ); // load  reintrepret_cast<fltx 4 *>(pVecs) + 1
	y5z5x6y6z6x7y7z7 = __lvx( pVecs, 32 ); // load reintrepret_cast<fltx 4 *>(pVecs) + 2

	// shove each quat onto its own fltx4, by using the permute operation
	// each halfword argument goes into the bottom 16 bits of the floating
	// point rep of 3.0f, then we use a magic constant to scale them.
	out0 = __vperm( x0y0z0x1y1z1x2y2, Four_Threes, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute0) ); // __x0__y0__z0____
	out1 = __vperm( x0y0z0x1y1z1x2y2, Four_Threes, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute1) ); // __x1__y1__z1____
	// postpone 2 since it straddles two words, we'll get back to it
	out3 = __vperm( z2x3y3z3x4y4z4x5, Four_Threes, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute3) ); // __x3__y3__z3__z2  // z2 is important, goes into out2
	out4 = __vperm( z2x3y3z3x4y4z4x5, Four_Threes, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute4) ); // __x4__y4__z4__x5  // x5 is important, goes into out5
	// 5 straddles two words
	out6 = __vperm( y5z5x6y6z6x7y7z7, Four_Threes, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute6) ); // __x6__y6__z6____
	out7 = __vperm( y5z5x6y6z6x7y7z7, Four_Threes, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute7) ); // __x7__y7__z7____
	// now get back to the straddlers, which we make by blending together a prior output and the other source word
	out2 = __vperm( x0y0z0x1y1z1x2y2, out3, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute2)  ); // __x2__y2__z2____
	out5 = __vperm( y5z5x6y6z6x7y7z7, out4, *reinterpret_cast<const fltx4 *>(&g_SIMD_Quat48_Unpack_Permute5)  ); // __x5__y5__z5____

	// the top bit of the z component in each word isn't part of the number; it's
	// a flag indicating whether the eventual w component should be negative.
	// so, we need to move the 0x00008000 bit of the z word onto the top bit
	// of the w word, which is a rotation two bytes right, or 14 bytes left.
	fltx4 wneg[8];
	// juggle all the z halfwords left one bit (toss the wneg sign bit, multiply by two)
	wneg[0] = __vsldoi( out0, out0, 14 );
	out0 = __vslh(out0, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[1] = __vsldoi( out1, out1, 14 );
	out1 = __vslh(out1, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[2] = __vsldoi( out2, out2, 14 );
	out2 = __vslh(out2, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[3] = __vsldoi( out3, out3, 14 );
	out3 = __vslh(out3, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[4] = __vsldoi( out4, out4, 14 );
	out4 = __vslh(out4, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[5] = __vsldoi( out5, out5, 14 );
	out5 = __vslh(out5, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[6] = __vsldoi( out6, out6, 14 );
	out6 = __vslh(out6, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
	wneg[7] = __vsldoi( out7, out7, 14 );
	out7 = __vslh(out7, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)

	// create a mask that is just the sign bit of the w word.
	fltx4 vAllOneBits = __vspltisw(-1); // Shift 31
	fltx4 signMask = __vslw(vAllOneBits, vAllOneBits);  // all the sign bits
	signMask = __vrlimi( signMask, Four_Zeros, 14, 0 ); // zero out x,y,z words

	// this macro defines the operations that will be performed on each of the eight words:
	// * scale from 0..65535 to -1..1 : tmp.x = ((int)x - 32768) * (1 / 32768.0);
	// * take the xyz dot product to get 1 - w^2
	// * subtract from one to get w^2
	// * square root to get zero
	// * OR in the wneg sign mask to get sign for zero.
	// though the macro makes it look like these are being done in serial,
	// in fact the compiler will reorder them to minimize stalls. 
	fltx4 ONE = Four_Ones;
	fltx4 dotxyz[8];
	fltx4 ww[8];
	// out0 = __vmaddfp( out0, vUpkMul, vUpkAdd );
	//  dotxyz[0] = Dot3SIMD( out0, out0 );
	// clamnp dotxyz if it's more than 1.0
	// all components are 1 - dotxyz
	// clear all but w's sign bit in wneg
	// all components are sqrt(1-dotxyz)
	// toggle w's sign where necessary
	// insert one element from the ww vector into the w component of ret
#define COMPUTE( target, number ) \
	target ## number = __vmaddfp( target ## number, vUpkMul, vUpkAdd ); \
	dotxyz[number] = Dot3SIMD( target ## number, target ## number ); \
	dotxyz[number] = __vminfp( dotxyz[number], ONE ); \
	ww[number] = SubSIMD( ONE, dotxyz[number] ); \
	wneg[number] = AndSIMD( wneg[number], signMask ) ; \
	ww[number] = SqrtSIMD(ww[number]); \
	ww[number] = OrSIMD( ww[number], wneg[number] ); \
	target ## number = __vrlimi( target ## number, ww[number], 1, 0 ); 

	COMPUTE(out, 0);
	COMPUTE(out, 1);
	COMPUTE(out, 2);
	COMPUTE(out, 3);
	COMPUTE(out, 4);
	COMPUTE(out, 5);
	COMPUTE(out, 6);
	COMPUTE(out, 7);

#undef COMPUTE
}

#elif defined(_PS3)


// unpack a single vector48 at the pointer into the x,y,z components of a fltx4.
// the w is total garbage.
FORCEINLINE fltx4 UnpackVector48SIMD( const Vector48 *pVec )
{
	// PS3 libs just give us this
	Vectormath::Aos::Vector3 ret;
	Vectormath::Aos::loadHalfFloats( ret, reinterpret_cast<const uint16_t *>(&pVec->x) );
	return ret.get128();

}


extern const uint16 ALIGN16 g_SIMD_Quat48_Unpack_Shift[]; //< Shuffles the z component of the quat48 left by one bit.
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute0[16];
extern const fltx4 g_SIMD_Quat48_Unpack_Magic_Constants;
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute1[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute2[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute3[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute4[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute5[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute6[16];
extern const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute7[16];

// unpack a single Quaternion48 at the pointer into the x,y,z,w components of a fltx4
FORCEINLINE fltx4 UnpackQuaternion48SIMD( const Quaternion48 * RESTRICT pVec )
{
	// A quaternion 48 stores the x and y components as 0..65535 , which is almost mapped onto -1.0..1.0 via (x - 32768) / 32768.5 .
	// z is stored as 0..32767, which is almost mapped onto -1..1 via (z - 16384) / 16384.5 .
	// w is inferred from 1 - the dot product of the other tree components. the top bit of what would otherwise be the 16-bit z is
	// w's sign bit.
	fltx4 q16s = LoadUnaligned3SIMD((const void *)pVec);
#if defined(__SPU__)
	vec_ushort8 shift = vec_ld( 0, (short unsigned int *)g_SIMD_Quat48_Unpack_Shift ); // load the aligned shift mask that we use to shuffle z.
	vec_uchar16 permute = vec_ld(0, (unsigned char *)g_SIMD_Quat48_Unpack_Permute0 ); // load the permute word that shuffles x,y,z into their own words
#else
	vec_ushort8 shift = vec_ld( 0, g_SIMD_Quat48_Unpack_Shift ); // load the aligned shift mask that we use to shuffle z.
	vec_uchar16 permute = vec_ld(0, g_SIMD_Quat48_Unpack_Permute0 ); // load the permute word that shuffles x,y,z into their own words
#endif
	bool wneg = pVec->wneg; // loading pVec into two different kinds of registers -- but not shuffling between (I hope!) so no LHS.

	q16s = vec_perm( q16s, Four_Threes, permute ); // permute so that x, y, and z are now each in their own words. The top half is the floating point rep of 3.0f
#if defined(__SPU__)
	q16s = (fltx4) vec_sl( (vec_ushort8) q16s, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
#else
	q16s = (fltx4) vec_vslh( (vec_ushort8) q16s, shift); // shift the z component left by one bit, tossing out the wneg sign bit and mapping z from [0..2^15) to [0..2^16)
#endif

	// each word of q16s contains 3.0 + n * 2^-22 -- convert this so that we get numbers on the range -1..1
	const fltx4 vUpkMul = SplatXSIMD(g_SIMD_Quat48_Unpack_Magic_Constants); // { UnpackMul16s, UnpackMul16s, UnpackMul16s, UnpackMul16s };
	const fltx4 vUpkAdd = SplatYSIMD(g_SIMD_Quat48_Unpack_Magic_Constants);

	fltx4 ret = vec_madd( q16s, vUpkMul, vUpkAdd );

	// now, work out what w must be. 
	fltx4 dotxyz = Dot3SIMD( ret, ret ); // all components are dot product of ret w/ self.
	dotxyz = ClampVectorSIMD( dotxyz, Four_Zeros, Four_Ones );

	fltx4 ww = SubSIMD( Four_Ones, dotxyz ); // all components are 1 - dotxyz
	ww = SqrtSIMD(ww); // all components are sqrt(1-dotxyz)

	// insert one element from the ww vector into the w component of ret
	ret = MaskedAssign( LoadAlignedSIMD(g_SIMD_ComponentMask[3]), wneg ? NegSIMD(ww) : ww, ret );

	return ret;
}

#endif







#if defined( _X360 )
#pragma bitfield_order( pop )
#elif defined( _PS3 )
#pragma ms_struct off
#pragma reverse_bitfields off
#endif

#endif


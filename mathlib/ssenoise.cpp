//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Fast low quality noise suitable for real time use
//
//=====================================================================================//

#include <math.h>
#include <float.h>	// needed for flt_epsilon
#include "basetypes.h"
#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/ssemath.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include "noisedata.h"


#define MAGIC_NUMBER (1<<15)								// gives 8 bits of fraction

static fltx4 Four_MagicNumbers = { MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER };


static ALIGN16 int32 idx_mask[4]= {0xffff, 0xffff, 0xffff, 0xffff};

#define MASK255 (*((fltx4 *)(& idx_mask )))

// returns 0..1
static inline float GetLatticePointValue( int idx_x, int idx_y, int idx_z )
{
	int ret_idx = perm_a[idx_x & 0xff];
	ret_idx = perm_b[( idx_y + ret_idx ) & 0xff];
	ret_idx = perm_c[( idx_z + ret_idx ) & 0xff];
	return impulse_xcoords[ret_idx];

}

fltx4 NoiseSIMD( const fltx4 & x, const fltx4 & y, const fltx4 & z )
{
	// use magic to convert to integer index
	fltx4 x_idx = AndSIMD( MASK255, AddSIMD( x, Four_MagicNumbers ) );
	fltx4 y_idx = AndSIMD( MASK255, AddSIMD( y, Four_MagicNumbers ) );
	fltx4 z_idx = AndSIMD( MASK255, AddSIMD( z, Four_MagicNumbers ) );

	fltx4 lattice000 = Four_Zeros, lattice001 = Four_Zeros, lattice010 = Four_Zeros, lattice011 = Four_Zeros;
	fltx4 lattice100 = Four_Zeros, lattice101 = Four_Zeros, lattice110 = Four_Zeros, lattice111 = Four_Zeros;

	// FIXME: Converting the input vectors to int indices will cause load-hit-stores (48 bytes)
	//        Converting the indexed noise values back to vectors will cause more (128 bytes)
	//        The noise table could store vectors if we chunked it into 2x2x2 blocks.
	fltx4 xfrac = Four_Zeros, yfrac = Four_Zeros, zfrac = Four_Zeros;
#define DOPASS(i)															\
    {	unsigned int xi = SubInt( x_idx, i );								\
		unsigned int yi = SubInt( y_idx, i );								\
		unsigned int zi = SubInt( z_idx, i );								\
		SubFloat( xfrac, i ) = (xi & 0xff)*(1.0/256.0);						\
		SubFloat( yfrac, i ) = (yi & 0xff)*(1.0/256.0);						\
		SubFloat( zfrac, i ) = (zi & 0xff)*(1.0/256.0);						\
		xi>>=8;																\
		yi>>=8;																\
		zi>>=8;																\
																			\
		SubFloat( lattice000, i ) = GetLatticePointValue( xi,yi,zi );		\
		SubFloat( lattice001, i ) = GetLatticePointValue( xi,yi,zi+1 );		\
		SubFloat( lattice010, i ) = GetLatticePointValue( xi,yi+1,zi );		\
		SubFloat( lattice011, i ) = GetLatticePointValue( xi,yi+1,zi+1 );	\
		SubFloat( lattice100, i ) = GetLatticePointValue( xi+1,yi,zi );		\
		SubFloat( lattice101, i ) = GetLatticePointValue( xi+1,yi,zi+1 );	\
		SubFloat( lattice110, i ) = GetLatticePointValue( xi+1,yi+1,zi );	\
		SubFloat( lattice111, i ) = GetLatticePointValue( xi+1,yi+1,zi+1 );	\
    }

	DOPASS( 0 );
	DOPASS( 1 );
	DOPASS( 2 );
	DOPASS( 3 );

	// now, we have 8 lattice values for each of four points as m128s, and interpolant values for
	// each axis in m128 form in [xyz]frac. Perfom the trilinear interpolation as SIMD ops

	// first, do x interpolation
	fltx4 l2d00 = AddSIMD( lattice000, MulSIMD( xfrac, SubSIMD( lattice100, lattice000 ) ) );
	fltx4 l2d01 = AddSIMD( lattice001, MulSIMD( xfrac, SubSIMD( lattice101, lattice001 ) ) );
	fltx4 l2d10 = AddSIMD( lattice010, MulSIMD( xfrac, SubSIMD( lattice110, lattice010 ) ) );
	fltx4 l2d11 = AddSIMD( lattice011, MulSIMD( xfrac, SubSIMD( lattice111, lattice011 ) ) );

	// now, do y interpolation
	fltx4 l1d0 = AddSIMD( l2d00, MulSIMD( yfrac, SubSIMD( l2d10, l2d00 ) ) );
	fltx4 l1d1 = AddSIMD( l2d01, MulSIMD( yfrac, SubSIMD( l2d11, l2d01 ) ) );

	// final z interpolation
	fltx4 rslt = AddSIMD( l1d0, MulSIMD( zfrac, SubSIMD( l1d1, l1d0 ) ) );

	// map to 0..1
	return MulSIMD( Four_Twos, SubSIMD( rslt, Four_PointFives ) );


}

static inline void GetVectorLatticePointValue( int idx, fltx4 &x, fltx4 &y, fltx4 &z,
											   int idx_x, int idx_y, int idx_z )
{
	int ret_idx = perm_a[idx_x & 0xff];
	ret_idx = perm_b[( idx_y + ret_idx ) & 0xff];
	ret_idx = perm_c[( idx_z + ret_idx ) & 0xff];
	float const *pData = s_randomGradients + ret_idx * 3;
	SubFloat( x, idx ) = pData[0];
	SubFloat( y, idx ) = pData[1];
	SubFloat( z, idx ) = pData[2];

}

FourVectors DNoiseSIMD( const fltx4 & x, const fltx4 & y, const fltx4 & z )
{
	// use magic to convert to integer index
	fltx4 x_idx = AndSIMD( MASK255, AddSIMD( x, Four_MagicNumbers ) );
	fltx4 y_idx = AndSIMD( MASK255, AddSIMD( y, Four_MagicNumbers ) );
	fltx4 z_idx = AndSIMD( MASK255, AddSIMD( z, Four_MagicNumbers ) );

	fltx4 xlattice000 = Four_Zeros, xlattice001 = Four_Zeros, xlattice010 = Four_Zeros, xlattice011 = Four_Zeros;
	fltx4 xlattice100 = Four_Zeros, xlattice101 = Four_Zeros, xlattice110 = Four_Zeros, xlattice111 = Four_Zeros;
	fltx4 ylattice000 = Four_Zeros, ylattice001 = Four_Zeros, ylattice010 = Four_Zeros, ylattice011 = Four_Zeros;
	fltx4 ylattice100 = Four_Zeros, ylattice101 = Four_Zeros, ylattice110 = Four_Zeros, ylattice111 = Four_Zeros;
	fltx4 zlattice000 = Four_Zeros, zlattice001 = Four_Zeros, zlattice010 = Four_Zeros, zlattice011 = Four_Zeros;
	fltx4 zlattice100 = Four_Zeros, zlattice101 = Four_Zeros, zlattice110 = Four_Zeros, zlattice111 = Four_Zeros;

	// FIXME: Converting the input vectors to int indices will cause load-hit-stores (48 bytes)
	//        Converting the indexed noise values back to vectors will cause more (128 bytes)
	//        The noise table could store vectors if we chunked it into 2x2x2 blocks.
	fltx4 xfrac = Four_Zeros, yfrac = Four_Zeros, zfrac = Four_Zeros;
#define DODPASS(i)															\
    {	unsigned int xi = SubInt( x_idx, i );								\
		unsigned int yi = SubInt( y_idx, i );								\
		unsigned int zi = SubInt( z_idx, i );								\
		SubFloat( xfrac, i ) = (xi & 0xff)*(1.0/256.0);						\
		SubFloat( yfrac, i ) = (yi & 0xff)*(1.0/256.0);						\
		SubFloat( zfrac, i ) = (zi & 0xff)*(1.0/256.0);						\
		xi>>=8;																\
		yi>>=8;																\
		zi>>=8;																\
																			\
		GetVectorLatticePointValue( i, xlattice000, ylattice000, zlattice000, xi,yi,zi );		\
		GetVectorLatticePointValue( i, xlattice001, ylattice001, zlattice001, xi,yi,zi+1 );		\
		GetVectorLatticePointValue( i, xlattice010, ylattice010, zlattice010, xi,yi+1,zi );		\
		GetVectorLatticePointValue( i, xlattice011, ylattice011, zlattice011, xi,yi+1,zi+1 );	\
		GetVectorLatticePointValue( i, xlattice100, ylattice100, zlattice100, xi+1,yi,zi );		\
		GetVectorLatticePointValue( i, xlattice101, ylattice101, zlattice101, xi+1,yi,zi+1 );	\
		GetVectorLatticePointValue( i, xlattice110, ylattice110, zlattice110, xi+1,yi+1,zi );	\
		GetVectorLatticePointValue( i, xlattice111, ylattice111, zlattice111, xi+1,yi+1,zi+1 );	\
    }

	DODPASS( 0 );
	DODPASS( 1 );
	DODPASS( 2 );
	DODPASS( 3 );

	// now, we have 8 lattice values for each of four points as m128s, and interpolant values for
	// each axis in m128 form in [xyz]frac. Perfom the trilinear interpolation as SIMD ops

	// first, do x interpolation
	fltx4 xl2d00 = AddSIMD( xlattice000, MulSIMD( xfrac, SubSIMD( xlattice100, xlattice000 ) ) );
	fltx4 xl2d01 = AddSIMD( xlattice001, MulSIMD( xfrac, SubSIMD( xlattice101, xlattice001 ) ) );
	fltx4 xl2d10 = AddSIMD( xlattice010, MulSIMD( xfrac, SubSIMD( xlattice110, xlattice010 ) ) );
	fltx4 xl2d11 = AddSIMD( xlattice011, MulSIMD( xfrac, SubSIMD( xlattice111, xlattice011 ) ) );

	// now, do y interpolation
	fltx4 xl1d0 = AddSIMD( xl2d00, MulSIMD( yfrac, SubSIMD( xl2d10, xl2d00 ) ) );
	fltx4 xl1d1 = AddSIMD( xl2d01, MulSIMD( yfrac, SubSIMD( xl2d11, xl2d01 ) ) );

	// final z interpolation
	FourVectors rslt;
	rslt.x = AddSIMD( xl1d0, MulSIMD( zfrac, SubSIMD( xl1d1, xl1d0 ) ) );

	fltx4 yl2d00 = AddSIMD( ylattice000, MulSIMD( xfrac, SubSIMD( ylattice100, ylattice000 ) ) );
	fltx4 yl2d01 = AddSIMD( ylattice001, MulSIMD( xfrac, SubSIMD( ylattice101, ylattice001 ) ) );
	fltx4 yl2d10 = AddSIMD( ylattice010, MulSIMD( xfrac, SubSIMD( ylattice110, ylattice010 ) ) );
	fltx4 yl2d11 = AddSIMD( ylattice011, MulSIMD( xfrac, SubSIMD( ylattice111, ylattice011 ) ) );

	// now, do y interpolation
	fltx4 yl1d0 = AddSIMD( yl2d00, MulSIMD( yfrac, SubSIMD( yl2d10, yl2d00 ) ) );
	fltx4 yl1d1 = AddSIMD( yl2d01, MulSIMD( yfrac, SubSIMD( yl2d11, yl2d01 ) ) );

	// final z interpolation
	rslt.y = AddSIMD( yl1d0, MulSIMD( zfrac, SubSIMD( yl1d1, yl1d0 ) ) );

	fltx4 zl2d00 = AddSIMD( zlattice000, MulSIMD( xfrac, SubSIMD( zlattice100, zlattice000 ) ) );
	fltx4 zl2d01 = AddSIMD( zlattice001, MulSIMD( xfrac, SubSIMD( zlattice101, zlattice001 ) ) );
	fltx4 zl2d10 = AddSIMD( zlattice010, MulSIMD( xfrac, SubSIMD( zlattice110, zlattice010 ) ) );
	fltx4 zl2d11 = AddSIMD( zlattice011, MulSIMD( xfrac, SubSIMD( zlattice111, zlattice011 ) ) );

	// now, do y interpolation
	fltx4 zl1d0 = AddSIMD( zl2d00, MulSIMD( yfrac, SubSIMD( zl2d10, zl2d00 ) ) );
	fltx4 zl1d1 = AddSIMD( zl2d01, MulSIMD( yfrac, SubSIMD( zl2d11, zl2d01 ) ) );

	// final z interpolation
	rslt.z = AddSIMD( zl1d0, MulSIMD( zfrac, SubSIMD( zl1d1, zl1d0 ) ) );

	return rslt;


}

fltx4 NoiseSIMD( FourVectors const &pos )
{
	return NoiseSIMD( pos.x, pos.y, pos.z );
}

FourVectors DNoiseSIMD( FourVectors const &pos )
{
	return DNoiseSIMD( pos.x, pos.y, pos.z );
}

FourVectors CurlNoiseSIMD( FourVectors const &pos )
{
	FourVectors fl4Comp1 = DNoiseSIMD( pos );
	FourVectors fl4Pos = pos;
	fl4Pos.x = AddSIMD( fl4Pos.x, ReplicateX4( 43.256 ) );
	fl4Pos.y = AddSIMD( fl4Pos.y, ReplicateX4( -67.89 ) );
	fl4Pos.z = AddSIMD( fl4Pos.z, ReplicateX4( 1338.2 ) );
	FourVectors fl4Comp2 = DNoiseSIMD( fl4Pos );
	fl4Pos.x = AddSIMD( fl4Pos.x, ReplicateX4( -129.856 ) );
	fl4Pos.y = AddSIMD( fl4Pos.y, ReplicateX4( -967.23 ) );
	fl4Pos.z = AddSIMD( fl4Pos.z, ReplicateX4( 2338.98 ) );
	FourVectors fl4Comp3 = DNoiseSIMD( fl4Pos );
	
	// now we have the 3 derivatives of a vector valued field. return the curl of the field.
	FourVectors fl4Ret;
	fl4Ret.x = SubSIMD( fl4Comp3.y, fl4Comp2.z );
	fl4Ret.y = SubSIMD( fl4Comp1.z, fl4Comp3.x );
	fl4Ret.z = SubSIMD( fl4Comp2.x, fl4Comp1.y );
	return fl4Ret;

}


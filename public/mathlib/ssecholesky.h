//========= Copyright c 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef SSE_CHOLESKY_HDR
#define SSE_CHOLESKY_HDR

#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"

// This class holds cholesky decomposition of Four 3x3 matrices
struct SimdCholesky3x3_t
{
	/// lower diagonal matrix L such that LL' = input matrix
	fltx4 m_10, m_20, m_21; // These are off-diagonals used in compuations
	fltx4 m_inv00, m_inv11, m_inv22; // These are reciprocals of diagonals used in all computations
public:
	/// @group Construction and initialization {
	SimdCholesky3x3_t( const fltx4 &a00, const fltx4 & a10, const fltx4 & a11, const fltx4 & a20, const fltx4 & a21, const fltx4 & a22 )
	{
		Init( a00, a10, a11, a20, a21, a22 );
	}
	void Init( const fltx4 & a00, const fltx4 & a10, const fltx4 & a11, const fltx4 & a20, const fltx4 & a21, const fltx4 & a22 );

	//@}

	bool IsValid( )const ;
	fltx4 GetValidMask( )const;

	const FourVectors SolveRight( const FourVectors &b );
	const FourVectors SolveLeft( const FourVectors &b );

	/// Using this decomposition LL', solve the following equation and return the result: LL' x = rhs
	const FourVectors Solve( const FourVectors &rhs )
	{
		// L R x =           b
		//   R x =      L^-1 b
		//     x = R^-1 L^-1 b
		return SolveRight( SolveLeft( rhs ) );
	}
};



#endif

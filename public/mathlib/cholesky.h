//========= Copyright c 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef CHOLESKY_HDR
#define CHOLESKY_HDR

#include "mathlib/vector.h"
#include "mathlib/mathlib.h"

struct matrix3x4_t;

// this class holds cholesky decomposition of a 3x3 matrix
struct Cholesky3x3_t
{
	// lower diagonal matrix L such that LL' = input matrix
	float m_00, m_11, m_22; // these aren't used in computations! they're only for debugging and returning "canonical" form (L or R)
	float m_10, m_20, m_21; // these are off-diagonals used in compuations
	float m_inv00, m_inv11, m_inv22; // these are reciprocals of diagonals used in all computations
public:
	/// @group Construction and initialization {
	Cholesky3x3_t( const matrix3x4_t &m )
	{
		Init( m );
	}
	Cholesky3x3_t( float a00, float a10, float a11, float a20, float a21, float a22 )
	{
		Init( a00, a10, a11, a20, a21, a22 );
	}
	bool Init( float a00, float a10, float a11, float a20, float a21, float a22 );
	bool Init( const matrix3x4_t &m )
	{
		return Init( m[0][0], m[1][0], m[1][1], m[2][0], m[2][1], m[2][2] );
	}

	//@}

	void FillLeft( matrix3x4_t & l );
	void FillRight( matrix3x4_t & r );
	bool IsValid( );

	const Vector SolveRight( const Vector &b );
	const Vector SolveLeft( const Vector &b );

	// using this decomposition LL', solve the following equation and return the result: LL' x = rhs
	const Vector Solve( const Vector &rhs )
	{
		// L R x =           b
		//   R x =      L^-1 b
		//     x = R^-1 L^-1 b
		return SolveRight( SolveLeft( rhs ) );
	}
};

//
/// Solve this equation: M*(x,1)=(0,0,0,1) where M is 3x4 matrix and x is the unknown 3-vector
/// that's returned uses cholesky decomposition 3x3 , so assumes 3x3 part of M is symmetrical
/// positive definite
//
inline Vector CholeskySolve( const matrix3x4_t &m )
{
	Cholesky3x3_t cholesky( m );
	return -cholesky.Solve( Vector( m.m_flMatVal[0][3], m.m_flMatVal[1][3], m.m_flMatVal[2][3] ) );
}


#endif
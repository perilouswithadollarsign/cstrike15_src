//========= Copyright c 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Cholesky and LDL' decomposition-related code
//
//=====================================================================================//
#include "mathlib/cholesky.h"
#include "mathlib/ssecholesky.h"

inline float ClampNeg( float a )
{
	return a >= 0.0f ? a : 0.0f ; //( a + fabsf(a) ) * 0.5f;
}

inline float SafeSqrt( float a )
{
	return sqrtf( ClampNeg( a ) );
}

inline float SafeRecip( float a )
{
	return 1.0f / ( a > 1e-8f ? a : 1e-8f );
}

bool Cholesky3x3_t::IsValid( )
{
	return m_inv00 + m_inv11 + m_inv22 < 1e+7f;
}

const fltx4 Four_1e7 = { 1.0e+7, 1.0e+7, 1.0e+7, 1.0e+7 };

bool SimdCholesky3x3_t::IsValid( )const 
{
	return IsAllGreaterThan( Four_1e7, m_inv00 + m_inv11 + m_inv22 );
}

fltx4 SimdCholesky3x3_t::GetValidMask( )const
{
	return CmpGtSIMD( Four_1e7, m_inv00 + m_inv11 + m_inv22 );
}


// initializes this decomposition; see formula at http://planetmath.org/encyclopedia/CholeskyDecomposition.html
bool Cholesky3x3_t::Init( float a00, float a10, float a11, float a20, float a21, float a22 )
{
#ifdef _DEBUG
	memset( this, 0xCD, sizeof(*this) ); // to see what's changing easily
#endif
	m_00 = SafeSqrt( a00 ); m_inv00 = SafeRecip( m_00 );
	m_10 = ( a10 ) * m_inv00;
	m_11 = SafeSqrt( a11 - Sqr( m_10 ) ); m_inv11 = SafeRecip( m_11 );
	m_20 = ( a20 ) * m_inv00;
	m_21 = ( a21 - m_20 * m_10) * m_inv11;
	m_22 = SafeSqrt( a22 - Sqr( m_20 ) - Sqr( m_21 ) ); m_inv22 = SafeRecip( m_22 );

#ifdef _DEBUG
	if( IsValid() )
	{
		matrix3x4_t l,r, a;
		FillLeft(l);
		FillRight(r);
		MatrixMultiply(l, r, a);
		float flError = Sqr( a00 - a[0][0] ) + Sqr( a10 - a[1][0] ) + Sqr( a20 - a[2][0] ) + 
			Sqr( a11 - a[1][1] ) + Sqr( a21 - a[2][1] ) + Sqr( a22 - a[2][2] );
		Assert( flError < 1e-5f );
	}
#endif

	return IsValid();
}


void Cholesky3x3_t::FillLeft( matrix3x4_t & l )
{
	l[0][0] = m_00;
	l[0][1] = l[0][2] = l[0][3] = 0;
	l[1][0] = m_10; l[1][1] = m_11;
	l[1][2] = l[1][3] = 0;
	l[2][0] = m_20;	l[2][1] = m_21; l[2][2] = m_22;
	l[2][3] = 0;
}

void Cholesky3x3_t::FillRight( matrix3x4_t & r )
{
	r[0][0] = m_00;
	r[1][0] = r[2][0] = 0;
	r[0][1] = m_10; r[1][1] = m_11;
	r[2][1] = 0;
	r[0][2] = m_20;	r[1][2] = m_21; r[2][2] = m_22;
	r[0][3] = r[1][3] = r[2][3] = 0;
}


// solve this : L x = b
const Vector Cholesky3x3_t::SolveLeft( const Vector &b )
{
	Vector result;
	result.x = m_inv00 * b.x;
	result.y = m_inv11 * ( b.y - m_10 * result.x );
	result.z = m_inv22 * ( b.z - m_20 * result.x - m_21 * result.y );
	return result;
}

const Vector Cholesky3x3_t::SolveRight( const Vector &b )
{
	Vector result;
	result.z = m_inv22 * b.z;
	result.y = m_inv11 * ( b.y - m_21 * result.z );
	result.x = m_inv00 * ( b.x - m_20 * result.z - m_10 * result.y );
	return result;
}



// initializes this decomposition; see formula at http://planetmath.org/encyclopedia/CholeskyDecomposition.html
void SimdCholesky3x3_t::Init( const fltx4 & a00, const fltx4 & a10, const fltx4 & a11, const fltx4 & a20, const fltx4 & a21, const fltx4 & a22 )
{
	m_inv00 = ReciprocalSqrtSIMD( a00 );
	m_10 = ( a10 ) * m_inv00;
	m_inv11 = ReciprocalSqrtSIMD( a11 - ( m_10 * m_10 ) );
	m_20 = ( a20 ) * m_inv00;
	m_21 = ( a21 - m_20 * m_10 ) * m_inv11;
	m_inv22 = ReciprocalSqrtSIMD( a22 - ( m_20 * m_20 ) - ( m_21 * m_21 ) );
}


// solve this : L x = b
const FourVectors SimdCholesky3x3_t::SolveLeft( const FourVectors &b )
{
	FourVectors result;
	result.x = m_inv00 * b.x;
	result.y = m_inv11 * ( b.y - m_10 * result.x );
	result.z = m_inv22 * ( b.z - m_20 * result.x - m_21 * result.y );
	return result;
}

const FourVectors SimdCholesky3x3_t::SolveRight( const FourVectors &b )
{
	FourVectors result;
	result.z = m_inv22 * b.z;
	result.y = m_inv11 * ( b.y - m_21 * result.z );
	result.x = m_inv00 * ( b.x - m_20 * result.z - m_10 * result.y );
	return result;
}

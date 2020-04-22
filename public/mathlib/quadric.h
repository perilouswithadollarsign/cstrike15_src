//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Quadric math functionality used for squared distance error metrics.
//
//===========================================================================//

#ifndef QUADRIC_H
#define QUADRIC_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "vector.h"
#include "cholesky.h"

// this class holds a quadric error function and implements integrating and evaluating these functions.
// see "Surface Simplfication using Quadric Error metrics.  Garland, Heckbert"
// http://mgarland.org/files/papers/quadric2.pdf  (updated version)
// NOTE: This will be expanded using Hughes Hoppe's method for including interpolated vertex attributes in a future version
class CQuadricError
{
public:
	CQuadricError() {}
	// used this to track down error
	void CheckDebug()
	{
		Assert( IsFinite(m_coefficients[0]) );
	}

	// integrate these by summing coefficients
	FORCEINLINE CQuadricError&	operator+=(const CQuadricError &inError)
	{
		for ( int i = 0; i < ARRAYSIZE(m_coefficients); i++ )
		{
			m_coefficients[i] += inError.m_coefficients[i];
		}
		return *this;
	}

	CQuadricError operator+(const CQuadricError& inError0 ) const
	{
		CQuadricError tmp;
		for ( int i = 0; i < ARRAYSIZE(m_coefficients); i++ )
		{
			tmp.m_coefficients[i] = inError0.m_coefficients[i] + m_coefficients[i];
		}
		return tmp;
	}
	// assignment
	CQuadricError& operator=(const CQuadricError &inError0)
	{
		for ( int i = 0; i < ARRAYSIZE(m_coefficients); i++ )
		{
			m_coefficients[i] = inError0.m_coefficients[i];
		}
		return *this;
	}

	CQuadricError& operator*=( float flScale )
	{
		for ( int i = 0; i < ARRAYSIZE(m_coefficients); i++ )
		{
			m_coefficients[i] *= flScale;
		}
		return *this;
	}


	// solves for the point with minimum error (inverts the matrix)
	Vector SolveForMinimumError()
	{
		matrix3x4_t tmp( 
			m_coefficients[0],		m_coefficients[1]*0.5f,	m_coefficients[2]*0.5f,		m_coefficients[3]*0.5f, 
			m_coefficients[1]*0.5f, m_coefficients[4],		m_coefficients[5]*0.5f,		m_coefficients[6]*0.5f,
			m_coefficients[2]*0.5f, m_coefficients[5]*0.5f, m_coefficients[7],			m_coefficients[8]*0.5f );

		return CholeskySolve( tmp );
	}

	// clear all coefficients
	void SetToZero()
	{
		for ( int i = 0; i < ARRAYSIZE(m_coefficients); i++ )
		{
			m_coefficients[i] = 0.0f;
		}
	}

	// usually these are initialized by summing quadrics for the planes coincident at each vert (one per triangle)
	// these are helpers to do that
	void InitFromPlane( const Vector &vNormal, float flDist, float flScale )
	{
		float flScale2 = flScale * 2.0f;
		m_coefficients[0] = vNormal.x * vNormal.x * flScale;	// a^2
		m_coefficients[1] = vNormal.x * vNormal.y * flScale2;	// 2ab
		m_coefficients[2] = vNormal.x * vNormal.z * flScale2;	// 2ac
		m_coefficients[3] = vNormal.x * flDist * flScale2;		// 2ad
		m_coefficients[4] = vNormal.y * vNormal.y * flScale;	// b^2
		m_coefficients[5] = vNormal.y * vNormal.z * flScale2;	// 2bc
		m_coefficients[6] = vNormal.y * flDist * flScale2;		// 2bd
		m_coefficients[7] = vNormal.z * vNormal.z * flScale;	// c^2
		m_coefficients[8] = vNormal.z * flDist * flScale2;		// cd
		m_coefficients[9] = flDist * flDist * flScale;	// d^2
	}

	void InitFromTriangle( const Vector &v0, const Vector &v1, const Vector &v2, float flMinArea )
	{
		Vector vNormal = CrossProduct( v2 - v0, v1 - v0 );
		float flArea = 0.5f * vNormal.NormalizeInPlace();
		flArea = MAX(flMinArea, flArea);
		float flDist = -DotProduct(vNormal, v0);
		InitFromPlane( vNormal, flDist, flArea );
	}

	// this evaluates the error at a point in space
	inline float ComputeError( const Vector &v0 )
	{
		float x = v0.x;
		float y = v0.y;
		float z = v0.z;
		float flVertex[9];
		flVertex[0] = x * x;
		flVertex[1] = x * y;
		flVertex[2] = x * z;
		flVertex[3] = x;
		flVertex[4] = y * y;
		flVertex[5] = y * z;
		flVertex[6] = y;
		flVertex[7] = z * z;
		flVertex[8] = z;
		float flTotal = m_coefficients[9];
		for ( int i = 0; i < 9; i++ )
		{
			flTotal += flVertex[i] * m_coefficients[i];
		}
		return flTotal;
	}

private:
	float m_coefficients[10];
};

#endif // QUADRIC_H
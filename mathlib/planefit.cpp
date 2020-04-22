//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Code to compute the equation of a plane with a least-squares residual fit.
//
//===============================================================================

#include "vplane.h"
#include "mathlib.h"
#include <algorithm>
using namespace std;

//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

static const float DETERMINANT_EPSILON = 1e-6f;

template< int PRIMARY_AXIS >
bool ComputeLeastSquaresPlaneFit( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane );

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

bool ComputeLeastSquaresPlaneFitX( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	return ComputeLeastSquaresPlaneFit<0>( pPoints, nNumPoints, pFitPlane );
}

bool ComputeLeastSquaresPlaneFitY( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	return ComputeLeastSquaresPlaneFit<1>( pPoints, nNumPoints, pFitPlane );
}

bool ComputeLeastSquaresPlaneFitZ( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	return ComputeLeastSquaresPlaneFit<2>( pPoints, nNumPoints, pFitPlane );
}

float ComputeSquaredError( const Vector *pPoints, int nNumPoints, const VPlane *pFitPlane )
{
	float flSqrError = 0.0f;
	float flError = 0.0f;
	for ( int i = 0; i < nNumPoints; ++ i )
	{
		float flDist = pFitPlane->DistTo( pPoints[i] );
		flError += flDist;
		flSqrError += flDist * flDist;
	}

	return flSqrError;
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation
//////////////////////////////////////////////////////////////////////////

// Because this is not a least-squares orthogonal distance fit, an axis must be specified along which residuals are computed.
// A traditional least-squares linear regression computes residuals along the y-axis and fits to a function of x, meaning that vertical lines cannot be properly fit.
// Similarly, this algorithm cannot properly fit planes which lie along a plane parallel to the primary axis
// 
// PRIMARY_AXIS 
// X = 0
// Y = 1
// Z = 2
template< int PRIMARY_AXIS >
bool ComputeLeastSquaresPlaneFit( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	memset( pFitPlane, 0, sizeof( VPlane ) );

	if ( nNumPoints < 3 )
	{
		// We must have at least 3 points to fit a plane
		return false;
	}

	Vector vCentroid( 0, 0, 0 ); // averages: x-bar, y-bar, z-bar
	Vector vSquaredSums( 0, 0, 0 ); // x => x*x, y => y*y, z => z*z
	Vector vCrossSums( 0, 0, 0 ); // x => y*z, y => x*z, z >= x*y
	float flNumPoints = ( float )nNumPoints;
	
	for ( int i = 0; i < nNumPoints; ++ i )
	{
		vCentroid += pPoints[i];
		vSquaredSums += pPoints[i] * pPoints[i];
		vCrossSums.x += pPoints[i].y * pPoints[i].z;
		vCrossSums.y += pPoints[i].x * pPoints[i].z;
		vCrossSums.z += pPoints[i].x * pPoints[i].y;
	}

	vCentroid /= ( float ) nNumPoints;
	
	if ( PRIMARY_AXIS == 0 )
	{
		// swap X and Z
		swap( vCentroid.x, vCentroid.z );
		swap( vSquaredSums.x, vSquaredSums.z );
		swap( vCrossSums.x, vCrossSums.z );
	}
	else if ( PRIMARY_AXIS == 1 )
	{
		// Swap Y and Z
		swap( vCentroid.y, vCentroid.z );
		swap( vSquaredSums.y, vSquaredSums.z );
		swap( vCrossSums.y, vCrossSums.z );
	}

	// Solve system of equations:
	// (example assumes primary axis is Z)
	//
	// A * ( sum( xi * xi ) - n * vCentroid.x^2 ) + B * ( sum( xi * yi ) - n * vCentroid.x * vCentroid.y ) - sum( xi * zi ) + n * vCentroid.x * vCentroid.z = 0
	// A * ( sum( xi * yi ) - n * vCentroid.x * vCentroid.y ) + B * ( sum( yi * yi ) - n * vCentroid.y ^ 2 ) - sum( yi * zi ) + n * vCentroid.y * vCentroid.z = 0
	// C = vCentroid.z - A * vCentroid.x - B * vCentroid.y
	//
	// where z = Ax + By + C
	//
	// Transform to:
	// [ m11 m12 ] [ A ]  =  [ c1 ]
	// [ m21 m22 ] [ B ]  =  [ c2 ]
	//
	// M * x = C
	// Take the inverse of M, post-multiply by C:
	// x = M_inverse * C

	float flM11 = vSquaredSums.x - flNumPoints * vCentroid.x * vCentroid.x;
	float flM12 = vCrossSums.z - flNumPoints * vCentroid.x * vCentroid.y;
	float flC1 = vCrossSums.y - flNumPoints * vCentroid.x * vCentroid.z;
	float flM21 = vCrossSums.z - flNumPoints * vCentroid.x * vCentroid.y;
	float flM22 = vSquaredSums.y - flNumPoints * vCentroid.y * vCentroid.y;
	float flC2 = vCrossSums.x - flNumPoints * vCentroid.y * vCentroid.z;

	float flDeterminant = flM11 * flM22 - flM12 * flM21;
	if ( fabsf( flDeterminant ) > DETERMINANT_EPSILON )
	{
		float flInvDeterminant = 1.0f / flDeterminant;
		float flA = flInvDeterminant * ( flM22 * flC1 - flM12 * flC2 );
		float flB = flInvDeterminant * ( -flM21 * flC1 + flM11 * flC2 );
		float flC = vCentroid.z - flA * vCentroid.x - flB * vCentroid.y;

		pFitPlane->m_Normal = Vector( -flA, -flB, 1.0f );
		float flScale = pFitPlane->m_Normal.NormalizeInPlace();
		pFitPlane->m_Dist = flC * 1.0f / flScale;

		if ( PRIMARY_AXIS == 0 )
		{
			// swap X and Z
			swap( pFitPlane->m_Normal.x, pFitPlane->m_Normal.z );
		}
		else if ( PRIMARY_AXIS == 1 )
		{
			// Swap Y and Z
			swap( pFitPlane->m_Normal.y, pFitPlane->m_Normal.z );
		}

		return true;
	}

	// Bad determinant
	return false;
}


struct Complex_t
{
	float r;
	float i;

	Complex_t() { }
	Complex_t( float flR, float flI ) : r( flR ), i( flI ) { }

	static Complex_t FromPolar( float flRadius, float flTheta )
	{
		return Complex_t( flRadius * cosf( flTheta ), flRadius * sinf( flTheta ) );
	}

	static Complex_t SquareRoot( float flValue )
	{
		if ( flValue < 0.0f )
		{
			return Complex_t( 0.0f, sqrtf( -flValue ) );
		}
		else
		{
			return Complex_t( sqrtf( flValue ), 0.0f );
		}
	}

	Complex_t operator+( const Complex_t &rhs ) const
	{
		return Complex_t( r + rhs.r, i + rhs.i );
	}

	Complex_t operator-( const Complex_t &rhs ) const
	{
		return Complex_t( r - rhs.r, i - rhs.i );
	}

	Complex_t operator*( const Complex_t &rhs ) const
	{
		return Complex_t( r * rhs.r - i * rhs.i, r * rhs.i + i * rhs.r );
	}

	Complex_t operator*( float rhs ) const
	{
		return Complex_t( r * rhs, i * rhs );
	}

	Complex_t operator/( float rhs ) const
	{
		return Complex_t( r / rhs, i / rhs );
	}

	Complex_t CubeRoot() const
	{
		float flRadius = sqrtf( r * r + i * i );
		float flTheta = atan2f( i, r );
		//if ( flTheta < 0.0f ) flTheta += 2.0f * 3.14159f;

		// Demoivre's theorem for principal root
		return FromPolar( powf( flRadius, 1.0f / 3.0f ), flTheta / 3.0f );
	}
};

// [kutta]
// This code is a work-in-progress; need to write code to robustly find an eigenvector given its eigenvalue.
#if USE_ORTHOGONAL_LEAST_SQUARES

template< int PRIMARY_AXIS = 0 >
bool TryFindEigenvector( float flEigenvalue, const Vector *pMatrix, Vector *pEigenvector )
{
	const float flCoefficientEpsilon = 1e-3;

	const int nOtherRow1 = ( PRIMARY_AXIS + 1 ) % 3;
	const int nOtherRow2 = ( PRIMARY_AXIS + 2 ) % 3;

	bool bUseRow1 = fabsf( pMatrix[0][nOtherRow1] / flEigenvalue ) > flCoefficientEpsilon && fabsf( pMatrix[0][nOtherRow2] / flEigenvalue ) > flCoefficientEpsilon );
	bool bUseRow2 = fabsf( pMatrix[1][nOtherRow1] / flEigenvalue ) > flCoefficientEpsilon && fabsf( pMatrix[1][nOtherRow2] / flEigenvalue ) > flCoefficientEpsilon );
	bool bUseRow3 = fabsf( pMatrix[2][nOtherRow1] / flEigenvalue ) > flCoefficientEpsilon && fabsf( pMatrix[2][nOtherRow2] / flEigenvalue ) > flCoefficientEpsilon );

	// ...
}

bool ComputeLeastSquaresOrthogonalPlaneFit( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	memset( pFitPlane, 0, sizeof( VPlane ) );

	if ( nNumPoints < 3 )
	{
		// We must have at least 3 points to fit a plane
		return false;
	}

	Vector vCentroid( 0, 0, 0 ); // averages: x-bar, y-bar, z-bar
	Vector vSquaredSums( 0, 0, 0 ); // x => x*x, y => y*y, z => z*z
	Vector vCrossSums( 0, 0, 0 ); // x => y*z, y => x*z, z >= x*y
	float flNumPoints = ( float )nNumPoints;

	for ( int i = 0; i < nNumPoints; ++ i )
	{
		vCentroid += pPoints[i];
		vSquaredSums += pPoints[i] * pPoints[i];
		vCrossSums.x += pPoints[i].y * pPoints[i].z;
		vCrossSums.y += pPoints[i].x * pPoints[i].z;
		vCrossSums.z += pPoints[i].x * pPoints[i].y;
	}

	vCentroid /= ( float ) nNumPoints;

	// Re-center the squared and cross sums
	vSquaredSums.x -= flNumPoints * vCentroid.x * vCentroid.x;
	vSquaredSums.y -= flNumPoints * vCentroid.y * vCentroid.y;
	vSquaredSums.z -= flNumPoints * vCentroid.z * vCentroid.z;
	vCrossSums.x -= flNumPoints * vCentroid.y * vCentroid.z;
	vCrossSums.y -= flNumPoints * vCentroid.x * vCentroid.z;
	vCrossSums.z -= flNumPoints * vCentroid.x * vCentroid.y;

	// Best fit normal occurs at the minimum of the Rayleigh quotient:
	//
	// n' * M * n
	// ----------
	//   n' *  n
	//
	// Where M is the covariance matrix.
	// M is computed from ( A' * A ) where A is a 3xN matrix of x/y/z residuals for each point in the data set.


	// Solve for eigenvalues & eigenvectors of 3x3 real symmetric covariance matrix.
	// The resulting characteristic polynormial equation is a cubic of the form:
	// x^3 + Ax^2 + Bx + C = 0
	//
	// All roots of the equation and eigenvalues are positive real values; the lowest one corresponds to the eigenvalue which is the normal to the best fit plane.

	float flA = -( vSquaredSums.x + vSquaredSums.y + vSquaredSums.z );
	float flB = vSquaredSums.x * vSquaredSums.z + vSquaredSums.x * vSquaredSums.y + vSquaredSums.y * vSquaredSums.z - vCrossSums.x * vCrossSums.x - vCrossSums.y * vCrossSums.y - vCrossSums.z * vCrossSums.z;
	float flC = -( vSquaredSums.x * vSquaredSums.y * vSquaredSums.z + 2.0f * vCrossSums.x * vCrossSums.y * vCrossSums.z ) + ( vSquaredSums.x * vCrossSums.x * vCrossSums.x + vSquaredSums.y * vCrossSums.y * vCrossSums.y + vSquaredSums.z * vCrossSums.z * vCrossSums.z );

	// Using formula for roots of cubic polynomial, see http://en.wikipedia.org/wiki/Cubic_function
	float flM = 2.0f * flA * flA * flA - 9.0f * flA * flB + 27.0f * flC;
	float flK = flA * flA - 3.0f * flB;
	float flN = flM * flM - 4.0f * flK * flK * flK;

	Complex_t flSolutions[3];
	const Complex_t omega1( -0.5f, 0.5f * sqrtf( 3.0f ) );
	const Complex_t omega2( -0.5f, -0.5f * sqrtf( 3.0f ) );
	Complex_t complexA = Complex_t( flA, 0.0f );
	Complex_t complexM = Complex_t( flM, 0.0f );
	Complex_t intermediateA = ( ( complexM + Complex_t::SquareRoot( flN ) ) / 2.0f );
	Complex_t intermediateB = ( ( complexM - Complex_t::SquareRoot( flN ) ) / 2.0f );
	Complex_t cubeA = intermediateA.CubeRoot(); 
	Complex_t cubeB = intermediateB.CubeRoot();
	Complex_t tempA = cubeA * cubeA * cubeA;
	Complex_t tempB = cubeB * cubeB * cubeB;

	flSolutions[0] = ( complexA + cubeA + cubeB ) * -1.0f / 3.0f;
	flSolutions[1] = ( complexA + ( omega2 * cubeA ) + ( omega1 * cubeB ) ) * -1.0f / 3.0f;
	flSolutions[2] = ( complexA + ( omega1 * cubeA ) + ( omega2 * cubeB ) ) * -1.0f / 3.0f;

	float flMinEigenvalue = MIN( flSolutions[0].r, flSolutions[1].r );
	flMinEigenvalue = MIN( flMinEigenvalue, flSolutions[2].r );

	// Subtract eigenvalue from the diagonal of the matrix to get a 3x3, real-symmetric, non-invertible matrix.
	// Pick 2 non-zero rows from this matrix and construct a system of 2 equations.
	
	return true;
}

#endif // USE_ORTHOGONAL_LEAST_SQUARES

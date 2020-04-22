#include <basetypes.h>
#include <float.h>
#include "tier1/utlvector.h"
#include "eigen.h"
#include "mathlib/aabb.h"

static matrix3x4_t Transpose(const matrix3x4_t &a)
{
	return matrix3x4_t(a[0][0],a[1][0],a[2][0],0,
		a[0][1],a[1][1],a[2][1],0,
		a[0][2],a[1][2],a[2][2],0 );
}



Quaternion Diagonalizer( const matrix3x4_t &A, Vector &d )
{
	// A must be a symmetric matrix.
	// returns quaternion q such that its corresponding matrix Q
	// can be used to Diagonalize A
	// Note, this routine has been adapted to valve's matrix conventions
	// which is C-style row-major matrix using common mathtext  (openglish) column conventions for
	// representing transforms (rotation and position):   v' = Mv
	// Valve's quaternion conventions are the same that everybody uses novodex, d3d, ogl.
	// Diagonal matrix D = Transpose(Q) * A * Q;  and  A = QT*D*Q
	// The columns of Q are the eigenvectors D's diagonal is the eigenvalues
	// As per 'column' convention if Q = q.getmatrix(); then Q*v = q*v*conj(q)
	int maxsteps = 24;  // certainly wont need that many.
	int i;
	Quaternion q( 0, 0, 0, 1 );
	for ( i = 0; i < maxsteps; i++ )
	{
		matrix3x4_t Q  ; //   Q*v == q*v*conj(q)
		QuaternionMatrix( q, Q );
		matrix3x4_t D  = Transpose( Q ) * A * Q;  // A = Q*D*Q^T
		Vector offdiag( D[1][2], D[0][2], D[0][1] ); // elements not on the diagonal
		d = Vector( D[0][0], D[1][1], D[2][2] );
		Vector om( fabsf( offdiag.x ), fabsf( offdiag.y ), fabsf( offdiag.z ) ); // mag of each offdiag elem
		int k = ( om.x > om.y && om.x > om.z ) ? 0 : ( om.y > om.z ) ? 1 : 2; // index of largest element of offdiag
		int k1 = ( k + 1 ) % 3;
		int k2 = ( k + 2 ) % 3;
		if ( offdiag[k] == 0.0f ) break;  // diagonal already
		float thet = ( D[k2][k2] - D[k1][k1] ) / ( 2.0f * offdiag[k] );
		float sgn = ( thet > 0.0f ) ? 1.0f : -1.0f;
		thet    *= sgn; // make it positive
		float t = sgn / ( thet + ( ( thet < 1.E6f ) ? sqrtf( thet * thet + 1.0f ) : thet ) ) ; // sign(T)/(|T|+sqrt(T^2+1))
		float c = 1.0f / sqrtf( t * t + 1.0f ); //  c= 1/(t^2+1) , t=s/c
		if ( c == 1.0f ) break;  // no room for improvement - reached machine precision.
		Quaternion jr( 0, 0, 0, 0 ); // jacobi rotation for this iteration.
		jr[k] = sgn * sqrtf( ( 1.0f - c ) / 2.0f );  // using 1/2 angle identity sin(a/2) = sqrt((1-cos(a))/2)
		jr[k] *= -1.0f; // ??since our quat-to-matrix convention was for v*M instead of M*v
		jr.w  = sqrtf( 1.0f - jr[k] * jr[k] );
		if ( jr.w == 1.0f ) break; // reached limits of floating point precision
		QuaternionMult( q, jr, q ); //q =  q*jr;
		QuaternionNormalize( q );
	}
	return q;
}


//-----------------------------------------------------------------------------
// Computes the mean point of a set of points, used by ComputeCovariantMatrix
//-----------------------------------------------------------------------------
extern Vector ComputeMeanPoint( const Vector *pPointList, int nPointCount )
{
	Vector vMean( 0.0f, 0.0f, 0.0f );

	for ( int ii = 0; ii < nPointCount; ++ii )
	{
		vMean += pPointList[ii];
	}

	vMean /= static_cast< float >( nPointCount );

	return vMean;
}


//-----------------------------------------------------------------------------
// Computes a covariance matrix for a set of points which measures spatial
// dispersion of the points against the mean of the points,
// the covariance matrix is symmetric and suitable for use in Diagonalizer()
//-----------------------------------------------------------------------------
void ComputeCovarianceMatrix( matrix3x4_t &covarianceMatrix, const Vector *pPointList, int nPointCount )
{
	SetIdentityMatrix( covarianceMatrix );

	if ( nPointCount <= 0 )
		return;

	const Vector vMean = ComputeMeanPoint( pPointList, nPointCount );

	CUtlVector< Vector > skewedPointList;
	skewedPointList.CopyArray( pPointList, nPointCount );

	for ( int ii = 0; ii < nPointCount; ++ii )
	{
		skewedPointList[ ii ] -= vMean;
	}

	const float flPointCount = static_cast< float >( nPointCount );

	for ( int ii = 0; ii < 3; ++ii )
	{
		for ( int jj = 0; jj < 3; ++jj )
		{
			float flCovariance = 0.0f;

			for ( int kk = 0; kk < nPointCount; ++kk )
			{
				flCovariance += skewedPointList[kk][ii] * skewedPointList[kk][jj];
			}

			covarianceMatrix[ii][jj] = flCovariance / flPointCount;
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the center and scale using qEigenVectors as the orientation to
// transform a unit cube at the origin to contain the specified point list,
// calls ComputeCovarianceMatrix(), Diagonalizer()
//-----------------------------------------------------------------------------
void ComputeExtents( Vector &vCenter, Vector &vScale, const Quaternion &qEigen, const Vector *pPointList, int nPointCount )
{
	if ( nPointCount <= 0 )
		return;

	AABB_t bbox;

	// Compute bounding box in inverse eigen space
	const Quaternion qEigenInverse = QuaternionInvert( qEigen );
	const matrix3x4_t mEigenInverse = QuaternionMatrix( qEigenInverse );	// VectorRotate with a quaternion does this each call

	Vector vTmp;
	VectorRotate( pPointList[0], mEigenInverse, vTmp );

	bbox.SetToPoint( vTmp );

	for ( int ii = 1; ii < nPointCount; ++ii )
	{
		VectorRotate( pPointList[ii], mEigenInverse, vTmp );
		bbox |= vTmp;
	}

	VectorRotate( bbox.GetCenter(), qEigen, vCenter );	// Transform center back to eigen space
	vScale = bbox.GetSize();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
extern void ComputeBoundingBoxMatrix( matrix3x4_t &boundingBoxMatrix, const Vector *pPointList, int nPointCount )
{
	matrix3x4_t covarianceMatrix;
	ComputeCovarianceMatrix( covarianceMatrix, pPointList, nPointCount );

	Vector vEigenValues;
	const Quaternion qEigenVectors = Diagonalizer( covarianceMatrix, vEigenValues );

	Vector vCenter( 0.0f, 0.0f, 0.0f );
	Vector vScale( 1.0f, 1.0f, 1.0f );

	ComputeExtents( vCenter, vScale, qEigenVectors, pPointList, nPointCount );

	QuaternionMatrix( qEigenVectors, vCenter, vScale, boundingBoxMatrix );
}

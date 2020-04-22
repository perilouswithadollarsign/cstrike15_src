//========= Copyright c 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: anything remotely related to eigen-[problem,solver,value,vector, whatever] goes here
//
// $NoKeywords: $
//=============================================================================//

#ifndef EIGEN_H
#define EIGEN_H

#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
extern Quaternion Diagonalizer( const matrix3x4_t &A, Vector &d );

// Routines which use Diagonalizer() to compute tight oriented bounding boxes for point sets

// Computes the mean point of a set of points, used by ComputeCovariantMatrix
extern Vector ComputeMeanPoint( const Vector *pPointList, int nPointCount );
// Computes a covariance matrix for a set of points which measures spatial dispersion of the points against the mean of the points, the covariance matrix is symmetric and suitable for use in Diagonalizer()
extern void ComputeCovarianceMatrix( matrix3x4_t &covarianceMatrix, const Vector *pPointList, int nPointCount );
// Computes the center and scale using qEigenVectors as the orientation to transform a unit cube at the origin to contain the specified point list, calls ComputeCovarianceMatrix(), Diagonalizer()
extern void ComputeExtents( Vector &vCenter, Vector &vScale, const Quaternion &qEigenVectors, const Vector *pPointList, int nPointCount );
// Wraps up all of the above calls to compute the matrix to transform a unit cube at the origin to contain the specified point list
extern void ComputeBoundingBoxMatrix( matrix3x4_t &boundingBoxMatrix, const Vector *pPointList, int nPointCount );

#endif
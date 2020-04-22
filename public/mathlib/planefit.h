//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Code to compute the equation of a plane with a least-squares residual fit.
//
//===============================================================================

#ifndef PLANEFIT_H
#define PLANEFIT_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

class VPlane;

//-----------------------------------------------------------------------------
// Finds a plane to best fit a set of points.  The least-squares residual 
// error is computed along the X/Y/Z-axis, not orthogonally to the plane,
// since doing the latter requires an SVD or a 3x3 eigendecomposition.
//-----------------------------------------------------------------------------
bool ComputeLeastSquaresPlaneFitX( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane );
bool ComputeLeastSquaresPlaneFitY( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane );
bool ComputeLeastSquaresPlaneFitZ( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane );


//-----------------------------------------------------------------------------
// *WORK-IN-PROGRESS*
// Finds a plane to best fit a set of points.  The least-squares residual 
// error is computed along the optimal axis, orthogonally to the plane,
// and requires a 3x3 eigendecomposition.
//-----------------------------------------------------------------------------
bool ComputeLeastSquaresOrthogonalPlaneFit( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane );

//-----------------------------------------------------------------------------
// Given a plane and a set of points, computes the sum of
// squared orthogonal residuals.
//-----------------------------------------------------------------------------
float ComputeSquaredError( const Vector *pPoints, int nNumPoints, const VPlane *pFitPlane );

#endif // PLANEFIT_H

//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// A template implementation of a bezier curve class and associated helper
// functions.
//
//=============================================================================

#ifndef BEZIERCURVE_H
#define BEZIERCURVE_H

#ifdef _WIN32
#pragma once
#endif


const float ONE_THIRD  = 1.0f / 3.0f;
const float TWO_THIRDS = 2.0f / 3.0f;

//-----------------------------------------------------------------------------
// Generic order N Bezier curve evaluation. Evaluates the bezier curve at the
// specified 0 to 1 parameter and returns the result.
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
struct BezierEvaluateImpl
{		
	// This generic implementation performs an iterative set of lerps in order 
	// to compute the bezier evaluation for any order curve, it is not efficiently 
	// and here primarily to maintain generality. All order curve that are used 
	// with any frequency should have their own specialized implementations.
	static void BezierEvaluate( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
	{
		// This array is initialized with the control points and is then 
		// used to hold the intermediate results of each lerp in order 
		// to preserve the original set of control points.
		POINT_TYPE points[ ORDER + 1 ];

		for ( int i = 0; i <= ORDER; ++i ) 
		{
			points[ i ] = pControlPoints[ i ];
		}

		for ( int i = 1; i <= ORDER; ++i ) 
		{	
			for ( int j = 0; j <= ( ORDER - i ); ++j ) 
			{
				points[ j ] = ( ( 1.0f - t ) * points[ j ] ) + ( t * points[ j + 1 ] );
			}
		}

		result = points[ 0 ];
	}
};


//-----------------------------------------------------------------------------
// Partial specialization for linear evaluation.
//-----------------------------------------------------------------------------
template< class POINT_TYPE >
struct BezierEvaluateImpl< POINT_TYPE, 1 >
{
	static void BezierEvaluate( const POINT_TYPE *points, float t, POINT_TYPE &result )
	{
		float u = 1.0f - t;
		result = ( u * points[ 0 ] ) + ( t * points[ 1 ] );
	}
};


//-----------------------------------------------------------------------------
// Partial specialization for quadratic bezier curve evaluation
//-----------------------------------------------------------------------------
template< class POINT_TYPE >
struct BezierEvaluateImpl< POINT_TYPE, 2 >
{
	static void BezierEvaluate( const POINT_TYPE *points, float t, POINT_TYPE &result )
	{
		float u = 1.0f - t;
		float t2 = t * t;
		float u2 = u * u;
		result = ( u2 * points[ 0 ] ) + ( 2.0f * u * t * points[ 1 ] ) + ( t2 * points[ 2 ] );
	}
};

//-----------------------------------------------------------------------------
// Partial specialization for cubic Bezier curve evaluation.
//-----------------------------------------------------------------------------
template< class POINT_TYPE >
struct BezierEvaluateImpl< POINT_TYPE, 3 >
{
	static void BezierEvaluate( const POINT_TYPE *points, float t, POINT_TYPE &result )
	{
		float u = 1.0f - t;
		float t2 = t * t;
		float t3 = t * t * t;
		float u2 = u * u;
		float u3 = u * u * u;
		result = ( u3 * points[ 0 ] ) + ( 3.0f * u2 * t * points[ 1 ] ) + ( 3.0f * u * t2 * points[ 2 ] ) + ( t3 * points[ 3 ] );
	}
};


//-----------------------------------------------------------------------------
// Evaluate the bezier curve of the specified order given a set of control 
// points for the curve. Uses the BezierEvaluateImpl in order to allow 
// template partial specialization for specific order curves.
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
void BezierEvaluate( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
{
	BezierEvaluateImpl< POINT_TYPE, ORDER >::BezierEvaluate( pControlPoints, t, result );
}


//-----------------------------------------------------------------------------
// Generic order N Bezier curve tangent evaluation. Evaluates the derivative of 
// the Bezier curve at the specified 0 to 1 parameter and returns the result.
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
struct BezierTangentImpl
{		
	static void BezierTangent( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
	{
		POINT_TYPE derPoints[ ORDER ];
		for ( int i = 0; i < ORDER; ++i )
		{
			derPoints[ i ] = ( pControlPoints[ i + 1 ] - pControlPoints[ i ] ) * (float)ORDER;
		}
		BezierEvaluate< POINT_TYPE, ORDER - 1 >( derPoints, t, result );
	}
};


//-----------------------------------------------------------------------------
// Partial specialization for linear Bezier curve tangent evaluation
//-----------------------------------------------------------------------------
template< class POINT_TYPE >
struct BezierTangentImpl< POINT_TYPE, 1 >
{
	static void BezierTangent( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
	{
		POINT_TYPE derPoint;
		result = ( pControlPoints[ 1 ] - pControlPoints[ 0 ] );		
	}
};


//-----------------------------------------------------------------------------
// Partial specialization for quadratic Bezier curve tangent evaluation
//-----------------------------------------------------------------------------
template< class POINT_TYPE >
struct BezierTangentImpl< POINT_TYPE, 2 >
{
	static void BezierTangent( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
	{
		POINT_TYPE a = pControlPoints[ 0 ] + ( -2.0f * pControlPoints[ 1 ] ) + pControlPoints[ 2 ];
		POINT_TYPE b = ( -2.0f * pControlPoints[ 0 ] ) + ( 2.0f * pControlPoints[ 1 ] );
		result = ( 2.0f * a * t ) + b;
	}
};


//-----------------------------------------------------------------------------
// Partial specialization for cubic Bezier curve tangent evaluation
//-----------------------------------------------------------------------------
template< class POINT_TYPE >
struct BezierTangentImpl< POINT_TYPE, 3 >
{
	static void BezierTangent( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
	{
		POINT_TYPE a = (-1.0f * pControlPoints[ 0 ] ) + ( 3.0f * pControlPoints[ 1 ] ) + (-3.0f * pControlPoints[ 2 ] ) + pControlPoints[ 3 ];
		POINT_TYPE b = ( 3.0f * pControlPoints[ 0 ] ) + (-6.0f * pControlPoints[ 1 ] ) + ( 3.0f * pControlPoints[ 2 ] );
		POINT_TYPE c = (-3.0f * pControlPoints[ 0 ] ) + ( 3.0f * pControlPoints[ 1 ] );
		result = ( 3.0f * a * t * t ) + ( 2.0f * b * t ) + c;
	}
};


//-----------------------------------------------------------------------------
// Evaluate the derivative of the bezier curve in order to compute the tangent
// of the curve the the specified parameter. Uses BezierTangentImpl in order
// to allow template partial specialization for specific order curves.
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
void BezierTangent( const POINT_TYPE *pControlPoints, float t, POINT_TYPE &result )
{
	BezierTangentImpl< POINT_TYPE, ORDER >::BezierTangent( pControlPoints, t, result );
}



//-----------------------------------------------------------------------------
// The CBezierCurve represents an a order N bezier curve defined by control 
// points of an arbitrary dimension. The class has template parameters for both 
// the order ( ORDER ) and the point type ( POINT_TYPE ). In general the point
// type is expected to be vector, vector2d, or vector4d, but may work with other
// types if the appropriate operators are provided. 
//
//-----------------------------------------------------------------------------
template < class POINT_TYPE, int ORDER >
class CBezierCurve
{
protected:

	static const int NUM_POINTS = ORDER + 1;

public:

	// Default constructor, performs no initialization
	CBezierCurve() {}

	// Copy constructor
	CBezierCurve( const CBezierCurve &source );

	// Array constructor, initialize the bezier from an array of control points 
	explicit CBezierCurve( const POINT_TYPE controlPoints[ NUM_POINTS ] );

	// Set all of the control points of the curve
	void SetControlPoints( const POINT_TYPE controlPoints[ NUM_POINTS ] );

	// Evaluate the curve at specified 0 to 1 parameter, returning the point on the curve
	void Evaluate( float flParam, POINT_TYPE &point ) const;

	// Compute the tangent vector at the specified parameter of the curve
	void ComputeTangent( float flParam, POINT_TYPE &tangent ) const;

	// Get the specified control point
	const POINT_TYPE &ControlPoint( int index ) const { return m_ControlPoints[ index ]; }

protected:

	POINT_TYPE m_ControlPoints[ NUM_POINTS ];
};


//-----------------------------------------------------------------------------
// Copy constructor
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
CBezierCurve< POINT_TYPE, ORDER >::CBezierCurve( const CBezierCurve &source )
{
	m_ControlPoints = source.m_ControlPoints;
}


//-----------------------------------------------------------------------------
// Array constructor, initialize the bezier from an array of control points 
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
CBezierCurve< POINT_TYPE, ORDER >::CBezierCurve( const POINT_TYPE controlPoints[ NUM_POINTS ] )
{
	SetControlPoints( controlPoints );
}

//-----------------------------------------------------------------------------
// Set all of the control points of the curve
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
void CBezierCurve< POINT_TYPE, ORDER >::SetControlPoints( const POINT_TYPE controlPoints[ NUM_POINTS ] )
{
	for ( int i = 0; i < NUM_POINTS; ++i )
	{
		m_ControlPoints[ i ] = controlPoints[ i ];
	}
}


//-----------------------------------------------------------------------------
// Evaluate the bezier curve of the specified order given a set of control 
// points for the curve
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
void CBezierCurve< POINT_TYPE, ORDER >::Evaluate( float t, POINT_TYPE &result ) const
{
	BezierEvaluate< POINT_TYPE, ORDER >( m_ControlPoints, t, result );
}


//-----------------------------------------------------------------------------
// Compute the tangent vector at the specified parameter of the curve
//-----------------------------------------------------------------------------
template< class POINT_TYPE, int ORDER > 
void CBezierCurve< POINT_TYPE, ORDER>::ComputeTangent( float t, POINT_TYPE &tangent ) const
{
	BezierTangent< POINT_TYPE, ORDER >( m_ControlPoints, t, tangent );
}


//-----------------------------------------------------------------------------
// The CCubicBezierCurve class represents a third order specialization of the
// generic CBezierCurve class and provided additional functionality which is
// implemented specifically for the cubic form of the bezier curve. 
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
class CCubicBezierCurve : public CBezierCurve< POINT_TYPE, 3 >
{

public:

	// Default constructor, performs no initialization
	CCubicBezierCurve() {}

	// Array constructor, initialize the bezier from an array of control points 
	explicit CCubicBezierCurve( const POINT_TYPE controlPoints[ 4 ] );

	// Compute the specified number of points along the curve
	void ComputePoints( POINT_TYPE *pPoints, int numPoints ) const;

	// Fit the curve to a set of data points
	bool FitToPoints( const POINT_TYPE *pPoints, float *pParams, int nPoints, int nMaxSteps, float flMaxError );

private:

	// Compute the bezier factor values used for evaluation that are independent of the parameter value
	void ComputeFactors( POINT_TYPE &a, POINT_TYPE &b, POINT_TYPE &c, POINT_TYPE &d ) const;

	// Perform a single step of the iterative point fitting process
	bool FitToPointsStep( const POINT_TYPE *pPoints, float *pParams, int nPoints, bool bReparaterize );

	// Calculate the control points of the curve that best fit the sample points with the specified parameters
	void ComputeControlPointsForSamples( const POINT_TYPE *pPoints, float *pParams, int nNumPoints );

	// Re-parameterize the provided set of points, finding parameter values which provide 
	// points on the curve closer to the sample points than the current parameter values.
	void ReparameterizePoints( const POINT_TYPE *pPoints, float *pParams, int nNumPoints ) const;

	// Compute the maximum squared distance between the specified points and the curve
	float ComputeMaxError( const POINT_TYPE *pPoints, const float *pParams, int nNumPoints ) const;

	// Compute the sum of the squared distance between the specified points and the curve
	float ComputeErrorSum( const POINT_TYPE *pPoints, const float *pParams, int nNumPoints ) const;

};


//-----------------------------------------------------------------------------
// Array constructor, initialize the bezier from an array of control points
//-----------------------------------------------------------------------------
template< class POINT_TYPE > 
CCubicBezierCurve< POINT_TYPE >::CCubicBezierCurve( const POINT_TYPE controlPoints[ 4 ] )
{
	this->m_ControlPoints[ 0 ] = controlPoints[ 0 ];
	this->m_ControlPoints[ 1 ] = controlPoints[ 1 ];
	this->m_ControlPoints[ 2 ] = controlPoints[ 2 ];
	this->m_ControlPoints[ 3 ] = controlPoints[ 3 ];
}


//-----------------------------------------------------------------------------
// Compute the specified number of points along the curve
//-----------------------------------------------------------------------------
template< class POINT_TYPE > 
void CCubicBezierCurve< POINT_TYPE >::ComputePoints( POINT_TYPE *pPoints, int numPoints ) const
{
	// Must evaluate at least two points.
	if ( numPoints <= 1 )
		return;
	
	// Calculate the parameter increment for each step
	const float flStep = 1.0f / ( numPoints - 1 );

	// Compute the basis values that can be re-used for all of the point calculations
	POINT_TYPE a, b, c, d;
	ComputeFactors( a, b, c, d );

	float t = 0;
	for ( int i = 0; i < numPoints; ++i )
	{
		pPoints[ i ] = ( a * t * t * t ) + ( b * t * t ) + ( c * t ) + d;
		t = t + flStep;
	}
}


//-----------------------------------------------------------------------------
// Fit the curve to a set of data points
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
bool CCubicBezierCurve< POINT_TYPE >::FitToPoints( const POINT_TYPE *pPoints, float *pParams, int nPoints, int nMaxSteps, float flMaxError )
{
	if ( ( pPoints == NULL ) || ( pParams == NULL ) || ( nPoints < 2 ) )
		return false;
	
	// Compute the max error distance, the provided max error is assumed 
	// to be a percentage of based on the length of the curve.
	float flLengthSQ = pPoints[ 0 ].DistToSqr( pPoints[ nPoints - 1 ] );

	// Do one initial step using chord length parameterization.
	FitToPointsStep( pPoints, pParams, nPoints, false );
	float flError = ComputeMaxError( pPoints, pParams, nPoints );

	// Check to see if the error is reasonable enough to be solved by iteration.
	float flMaxIterationError = flLengthSQ * 0.1f;
	if ( flError > flMaxIterationError )
	{
		POINT_TYPE vSegment = pPoints[ nPoints - 1 ] - pPoints[ 0 ];
		this->m_ControlPoints[ 0 ] = pPoints[ 0 ];
		this->m_ControlPoints[ 1 ] = pPoints[ 0 ] + ( vSegment * ONE_THIRD  );
		this->m_ControlPoints[ 2 ] = pPoints[ 0 ] + ( vSegment * TWO_THIRDS );
		this->m_ControlPoints[ 3 ] = pPoints[ nPoints - 1 ];
		return false;
	}

	// Iteratively improve the solution by re-evaluating the parameter values to make them match
	// the sample points more closely and then re-fitting the curve using the least squares method.
	int iStep = 0;
	float flMaxErrorDist = flLengthSQ * ( flMaxError * flMaxError );

	while ( ( flError > flMaxErrorDist ) && ( iStep < nMaxSteps ) )
	{
		FitToPointsStep( pPoints, pParams, nPoints, true );
		flError = ComputeMaxError( pPoints, pParams, nPoints );
		++iStep;
	}

	return ( flError <= flMaxErrorDist );
}


//-----------------------------------------------------------------------------
// Compute the bezier factor values used for evaluation that are independent of
// the parameter value
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
void CCubicBezierCurve< POINT_TYPE >::ComputeFactors( POINT_TYPE &a, POINT_TYPE &b, POINT_TYPE &c, POINT_TYPE &d ) const
{
	const POINT_TYPE *pControlPoints = this->m_ControlPoints;

	a = (-1.0f * pControlPoints[ 0 ] ) + ( 3.0f * pControlPoints[ 1 ] ) + (-3.0f * pControlPoints[ 2 ] ) + pControlPoints[ 3 ];
	b = ( 3.0f * pControlPoints[ 0 ] ) + (-6.0f * pControlPoints[ 1 ] ) + ( 3.0f * pControlPoints[ 2 ] );
	c = (-3.0f * pControlPoints[ 0 ] ) + ( 3.0f * pControlPoints[ 1 ] );
	d = ( 1.0f * pControlPoints[ 0 ] );
}


//-----------------------------------------------------------------------------
// Perform a single step of the iterative point fitting process
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
bool CCubicBezierCurve< POINT_TYPE >::FitToPointsStep( const POINT_TYPE *pPoints, float *pParams, int nNumPoints, bool bReparameterize )
{
	POINT_TYPE *pControlPoints = this->m_ControlPoints;

	if ( ( pPoints == NULL ) || ( pParams == NULL ) )
		return false;

	if ( nNumPoints < 2 )
		return false;

	if ( nNumPoints == 2)
	{
		pControlPoints[ 0 ] = pPoints[ 0 ];
		pControlPoints[ 1 ] = pPoints[ 0 ] + ( pPoints[ 1 ] - pPoints[ 0 ] ) * ONE_THIRD;
		pControlPoints[ 2 ] = pPoints[ 0 ] + ( pPoints[ 1 ] - pPoints[ 0 ] ) * TWO_THIRDS;
		pControlPoints[ 3 ] = pPoints[ 1 ];
		pParams[ 0 ] = 0.0f;
		pParams[ 1 ] = 1.0f;
		return true;
	}

	if ( bReparameterize )
	{
		ReparameterizePoints( pPoints, pParams, nNumPoints );
	}
	else
	{
		// Chord length parameterization
		float length = 0;
		pParams[ 0 ] = 0;
		for ( int i = 1; i < nNumPoints; ++i )
		{
			float distance = pPoints[ i - 1 ].DistTo( pPoints[ i ] );
			length += distance;
			pParams[ i ] = length;
		}
		for ( int i = 0; i < nNumPoints; ++i )
		{
			pParams[ i ] = pParams[ i ] / length;
		}
	}

	ComputeControlPointsForSamples( pPoints, pParams, nNumPoints );

	return true;
}


//-----------------------------------------------------------------------------
// Calculate the control points of the curve that best fit the sample points 
// with the specified parameters.
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
void CCubicBezierCurve< POINT_TYPE >::ComputeControlPointsForSamples( const POINT_TYPE *pPoints, float *pParams, int nPoints )
{
	POINT_TYPE *pControlPoints = this->m_ControlPoints;

	// Set end control points to the first and last sample points
	pControlPoints[ 0 ] = pPoints[ 0 ];
	pControlPoints[ 3 ] = pPoints[ nPoints - 1 ];

	// Use the least squares method to calculate new controls points
	float a1  = 0; 
	float a2  = 0;
	float a12 = 0;
	POINT_TYPE c1;
	POINT_TYPE c2;

	c1.Init();
	c2.Init();

	Assert( c1.IsZero() );
	Assert( c2.IsZero() );

	for ( int i = 0; i < nPoints; ++i )
	{
		const POINT_TYPE &p = pPoints[ i ];

		float t = pParams[ i ];
		float t2 = t * t;
		float t3 = t * t2;
		float t4 = t * t3;
		float u = 1 - t;
		float u2 = u * u;
		float u3 = u * u2;
		float u4 = u * u3;

		a1  += t2 * u4;
		a2  += t4 * u2;
		a12 += t3 * u3;

		POINT_TYPE vP = p - (u3 * pControlPoints[ 0 ]) - ( t3 * pControlPoints[ 3 ] );
		c1 += ( ( 3 * t * u2 ) * vP );
		c2 += ( ( 3 * t2 * u ) * vP );      
	}

	a1 = 9.0f * a1;
	a2 = 9.0f * a2;
	a12 = 9.0f * a12;

	const float flFactorTolerance = 0.000001f;
	float flFactor = ( a1 * a2 - a12 * a12 );
	if ( fabs( flFactor ) < flFactorTolerance )
	{
		POINT_TYPE vSegment = pControlPoints[ 3 ] - pControlPoints[ 0 ];
		pControlPoints[ 1 ] = pControlPoints[ 0 ] + vSegment * ONE_THIRD;
		pControlPoints[ 2 ] = pControlPoints[ 0 ] + vSegment * TWO_THIRDS;
	}
	else
	{
		pControlPoints[ 1 ] = ( a2 * c1 - a12 * c2 ) / flFactor;
		pControlPoints[ 2 ] = ( a1 * c2 - a12 * c1 ) / flFactor;
	}

	// The subsequent re-parameterization relies on NewtonRaphson root finding which will
	// fail if the tangents have an x delta of 0 or less, so ensure this does not happen.
	float flMinStep = ( pControlPoints[ 3 ].x - pControlPoints[ 0 ].x ) * 0.0001f;
	float flMinX = pControlPoints[ 0 ].x + flMinStep;
	float flMaxX = pControlPoints[ 3 ].x - flMinStep;
	pControlPoints[ 1 ].x = MAX( flMinX, MIN( flMaxX, pControlPoints[ 1 ].x ) );
	pControlPoints[ 2 ].x = MAX( flMinX, MIN( flMaxX, pControlPoints[ 2 ].x ) );
}


//-----------------------------------------------------------------------------
// Find a better parameter for the specified point using the NewtonRaphson 
// method, or with simple iteration if the NewtonRaphson method fails.
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
void CCubicBezierCurve< POINT_TYPE >::ReparameterizePoints( const POINT_TYPE *pPoints, float *pParams, int nNumPoints ) const
{
	const POINT_TYPE *pControlPoints = this->m_ControlPoints;

	const float flTolerance = 0.0001f;

	POINT_TYPE der1[ 3 ];
	der1[ 0 ] = ( pControlPoints[ 1 ] - pControlPoints[ 0 ] ) * 3.0f;
	der1[ 1 ] = ( pControlPoints[ 2 ] - pControlPoints[ 1 ] ) * 3.0f;
	der1[ 2 ] = ( pControlPoints[ 3 ] - pControlPoints[ 2 ] ) * 3.0f;

	POINT_TYPE der2[ 2 ];
	der2[ 0 ] = ( der1[ 1 ] - der1[ 0 ] ) * 2.0f;
	der2[ 1 ] = ( der1[ 2 ] - der1[ 1 ] ) * 2.0f;

	// Compute the basis values that can be re-used for all of the point calculations
	POINT_TYPE b3a, b3b, b3c, b3d;
	ComputeFactors( b3a, b3b, b3c, b3d );

	POINT_TYPE b2a = (  1.0f * der1[ 0 ] ) + ( -2.0f * der1[ 1 ] ) + ( 1.0f * der1[ 2 ] );
	POINT_TYPE b2b = ( -2.0f * der1[ 0 ] ) + (  2.0f * der1[ 1 ] );
	POINT_TYPE b2c = (  1.0f * der1[ 0 ] );

	POINT_TYPE b1a = der2[ 1 ] - der2[ 0 ];
	POINT_TYPE b1b = der2[ 0 ];

	float flPrevParam = 0;
	for ( int iPoint = 0; iPoint < nNumPoints; ++iPoint )
	{
		float t = pParams[ iPoint ];
		const POINT_TYPE &point = pPoints[ iPoint ];

		POINT_TYPE curvePoint = ( b3a * t * t * t ) + ( b3b * t * t ) + ( b3c * t ) + b3d;
		POINT_TYPE der1Point  = ( b2a * t * t ) + ( b2b * t ) + b2c;
		POINT_TYPE der2Point  = ( b1a * t ) + b1b;

		// Attempt to find a better parameter for the point 
		// using the NewtonRaphson root finding method.
		POINT_TYPE vDelta = curvePoint - point;
		float flNumerator = vDelta.Dot( der1Point );
		float flDenominator = vDelta.Dot( der2Point ) + der1Point.Dot( der1Point );
		float flRootParam = ( flDenominator == 0.0f ) ? t :  t - ( flNumerator / flDenominator );

		// We are not interested in any solutions outside the 0 to 1 range, so 
		// clamp the result. This may give a result that is farther than the 
		// original parameter, in which case the original parameter will be used.
		flRootParam = MAX( 0.0f, MIN( 1.0f, flRootParam ) );

		// Evaluate the parameter returned by the root finding, to 
		// determine if it is actually better parameter for the point.
		float rp = flRootParam;
		POINT_TYPE rootCurvePoint = ( b3a * rp * rp * rp ) + ( b3b * rp * rp ) + ( b3c * rp ) + b3d;
		float flDist = point.DistToSqr( curvePoint );
		float flDistRoot = point.DistToSqr( rootCurvePoint );
		
		// If the parameter returned by the root finding method gives a point on the 
		// curve that is closer to the sample point than the current parameter make 
		// the new parameter the value found by the root finding method.
		float flNewParam = t;
		if ( flDistRoot <= flDist )
		{
			flNewParam = flRootParam;
		}
		else if ( flDist > flTolerance )
		{
			// If the root finding method failed, try to find a better parameter iteratively. This is 
			// basically a brute force method, but with a couple of observations which actually make it
			// reasonable. First the direction to iterate from the current parameter can be deduced 
			// from the dot product of the vector from the point and the tangent of the curve. Second
			// the range of iteration can be restricted such that values before the last parameter 
			// are not considered.
			POINT_TYPE stepPoint;
			float flStepParam = t;
			float flStepDist = 0;
			float flBestStepParam = t;
			float flBestStepDist = flDist;

			const int nMaxSteps = 10;
			const float flBaseStepSize = MAX( t - flPrevParam, 0.001f ) / ( float )nMaxSteps;
			float flStepSize = 0;
			int nStep = 0;

			// The numerator of the root finding method is the dot product between the vector from the 
			// sample point to the point on the curve and the tangent of the curve. The tangent of the
			// curve tells us which way the curve is going and we want to move the parameter along the
			// curve in the way which is moving closer to the point, so if the dot product of the 
			// tangent and the vector from the point on the curve to the sample point is positive then
			// moving in a positive direction along the curve will bring us closer to the sample point. 
			// However, the numerator value used the vector from the sample point to the curve point, 
			// so negative value implies a positive movement.
			if ( flNumerator < 0 )
			{
				flStepSize = flBaseStepSize;
			}
			else
			{
				flStepSize = -flBaseStepSize;
			}

			// Starting with the current parameter, move the parameter by the calculated step interval
			// and evaluate the result. Continue as long as the result is closer than the previous 
			// best result and the specified maximum number of steps has not been reached.
			while ( nStep < nMaxSteps )
			{
				flStepParam = MAX( 0.0f, MIN( 1.0f, flStepParam + flStepSize ) );

				float sp = flStepParam;
				stepPoint = ( b3a * sp * sp * sp ) + ( b3b * sp * sp ) + ( b3c * sp ) + b3d;
				flStepDist = point.DistToSqr( stepPoint );
				
				if ( flStepDist >= flBestStepDist )
					break;

				flBestStepParam = flStepParam;
				flBestStepDist = flStepDist;
				++nStep;
			}

			flNewParam = flBestStepParam;
		}

		// Update the parameter to the new value which provides 
		// a closer point on the curve to the sample point.
		Assert( flNewParam >= 0.0f );
		Assert( flNewParam <= 1.0f );
		pParams[ iPoint ] = flNewParam;

		// Save the old parameter so it may be used by the next point
		// to determine the iteration range if the root finding fails
		flPrevParam = t;
	}
}


//-----------------------------------------------------------------------------
// Compute the maximum squared distance between the specified points and the 
// curve
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
float CCubicBezierCurve< POINT_TYPE >::ComputeMaxError( const POINT_TYPE *pPoints, const float *pParams, int nNumPoints ) const
{
	float flMaxError = 0.0f;

	POINT_TYPE a, b, c, d;
	ComputeFactors( a, b, c, d );

	for ( int iPoint = 0; iPoint < nNumPoints; ++iPoint )
	{
		const POINT_TYPE &samplePoint = pPoints[ iPoint ];
		float t = pParams[ iPoint ];

		POINT_TYPE curvePoint = ( a * t * t * t ) + ( b * t * t ) + ( c * t ) + d;

		float flDistSQ = samplePoint.DistToSqr( curvePoint );
		flMaxError = MAX( flDistSQ, flMaxError );
	}

	return flMaxError;
}



//-----------------------------------------------------------------------------
// Compute the sum of the squared distance between the specified points and the
// curve
//-----------------------------------------------------------------------------
template < class POINT_TYPE >
float CCubicBezierCurve< POINT_TYPE >::ComputeErrorSum( const POINT_TYPE *pPoints, const float *pParams, int nNumPoints ) const
{
	float flErrorSum = 0.0f;

	POINT_TYPE a, b, c, d;
	ComputeFactors( a, b, c, d );

	for ( int iPoint = 0; iPoint < nNumPoints; ++iPoint )
	{
		const POINT_TYPE &samplePoint = pPoints[ iPoint ];
		float t = pParams[ iPoint ];

		POINT_TYPE curvePoint = ( a * t * t * t ) + ( b * t * t ) + ( c * t ) + d;

		float flDistSQ = samplePoint.DistToSqr( curvePoint );
		flErrorSum += flDistSQ;
	}

	return flErrorSum;
}


#endif 

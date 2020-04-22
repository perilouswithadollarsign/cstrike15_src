//--------------------------------------------------------------------------------------------------
/**
	@file		qhMath.h

	@author		Dirk Gregorius
	@version	0.1
	@date		30/11/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include "qhTypes.h"
#include "qhArray.h"

#include <math.h>
#include <limits>

class qhVector3;
class qhMatrix3;
class qhQuaternion;
class qhTransform;
class qhPlane;
class qhBounds3;

#ifdef max 
#undef max
#endif

#ifdef min
#undef min
#endif


#pragma warning( push )
#pragma warning( disable : 4201 ) // nameless structs/unions

//--------------------------------------------------------------------------------------------------
// qhConstants	
//--------------------------------------------------------------------------------------------------
#define QH_PI				qhReal( 3.14159265358 )
#define QH_2PI				qhReal( 6.28318530717 )
#define QH_SQRT2			qhReal( 1.41421356237 )	
#define QH_SQRT3			qhReal( 1.73205080757 )	

#define QH_DEG2RAD			( QH_PI / qhReal( 180 ) )
#define QH_RAD2DEG			( qhReal( 180 ) / QH_PI )


//--------------------------------------------------------------------------------------------------
// qhMath	
//--------------------------------------------------------------------------------------------------
qhReal qhSin( qhReal Rad );
qhReal qhCos( qhReal Rad );
qhReal qhTan( qhReal Rad );

qhReal qhArcSin( qhReal X );
qhReal qhArcCos( qhReal X );
qhReal qhArcTan( qhReal X );
qhReal qhArcTan2( qhReal Y, qhReal X  );

qhReal qhAbs( qhReal X );
qhReal qhSqrt( qhReal X );

template< typename T > T qhMin( T X, T Y );
template< typename T > T qhMax( T X, T Y );
template< typename T > T qhClamp( T X, T Min, T Max );


//--------------------------------------------------------------------------------------------------
// qhVector3
//--------------------------------------------------------------------------------------------------
class qhVector3
	{
	public:
		// Attributes
		qhReal X, Y, Z;

		// Construction
		qhVector3( void );
		qhVector3( qhReal X, qhReal Y, qhReal Z );
		qhVector3( const qhReal* V );

		// Conversion
		operator qhReal*( void );
		operator const qhReal*( void ) const;

		// Assignment
		qhVector3& operator*=( const qhVector3& V );
		qhVector3& operator+=( const qhVector3& V );
		qhVector3& operator-=( const qhVector3& V );
		qhVector3& operator*=( qhReal S );
		qhVector3& operator/=( qhReal S );

		// Unary operators
		qhVector3 operator+( void ) const;
		qhVector3 operator-( void ) const;
	};

// Binary operators
qhVector3 operator*( const qhMatrix3& M, const qhVector3& V );
qhVector3 operator*( const qhQuaternion& Q, const qhVector3& V );
qhVector3 operator*( const qhTransform& T, const qhVector3& V );

qhVector3 operator*( const qhVector3& V1, const qhVector3& V2 );
qhVector3 operator+( const qhVector3& V1, const qhVector3& V2 );
qhVector3 operator-( const qhVector3& V1, const qhVector3& V2 );
qhVector3 operator*( qhReal S, const qhVector3& V );
qhVector3 operator*( const qhVector3& V, qhReal S );
qhVector3 operator/( const qhVector3& V, qhReal S );

bool operator==( const qhVector3& V1, const qhVector3& V2 );
bool operator!=( const qhVector3& V1, const qhVector3& V2 );

// Standard vector operations
void qhStore( qhReal Dst[ 3 ], const qhVector3& V );

qhVector3 qhMul( const qhMatrix3& M, const qhVector3& V );
qhVector3 qhTMul( const qhMatrix3& M, const qhVector3& V );
qhVector3 qhMul( const qhQuaternion& Q, const qhVector3& V );
qhVector3 qhTMul( const qhQuaternion& Q, const qhVector3& V );
qhVector3 qhMul( const qhTransform& T, const qhVector3& V );
qhVector3 qhTMul( const qhTransform& T, const qhVector3& V );

qhVector3 qhMul( const qhVector3& V1, const qhVector3& V2 );
qhVector3 qhAdd( const qhVector3& V1, const qhVector3& V2 );
qhVector3 qhSub( const qhVector3& V1, const qhVector3& V2 );
qhVector3 qhCross( const qhVector3& V1, const qhVector3& V2 );
qhVector3 qhScale( const qhVector3& V, qhReal S );
qhVector3 qhNormalize( const qhVector3& V );
qhVector3 qhNegate( const qhVector3& V );

qhVector3 qhAbs( const qhVector3& V );
qhVector3 qhMin( const qhVector3& V1, const qhVector3& V2 );
qhVector3 qhMax( const qhVector3& V1, const qhVector3& V2 );
qhVector3 qhClamp( const qhVector3& V, const qhVector3& Min, const qhVector3& Max );

qhReal qhDot( const qhVector3& V1, const qhVector3& V2 );
qhReal qhLength( const qhVector3& V );
qhReal qhLengthSq( const qhVector3& V );
qhReal qhDistance( const qhVector3& V1, const qhVector3& V2 );
qhReal qhDistanceSq( const qhVector3& V1, const qhVector3& V2 );
qhReal qhDet( const qhVector3& V1, const qhVector3& V2, const qhVector3& V3 );

int qhMinElement( const qhVector3& V );
int qhMaxElement( const qhVector3& V );

// Constants
QH_GLOBAL_CONSTANT const qhVector3 QH_VEC3_ZERO	= qhVector3( 0, 0, 0 ); 
QH_GLOBAL_CONSTANT const qhVector3 QH_VEC3_AXIS_X = qhVector3( 1, 0, 0 ); 
QH_GLOBAL_CONSTANT const qhVector3 QH_VEC3_AXIS_Y = qhVector3( 0, 1, 0 ); 
QH_GLOBAL_CONSTANT const qhVector3 QH_VEC3_AXIS_Z = qhVector3( 0, 0, 1 ); 


//--------------------------------------------------------------------------------------------------
// qhMatrix3
//--------------------------------------------------------------------------------------------------
class qhMatrix3
{
public:
	// Attributes
	qhVector3 C1, C2, C3;

	// Construction
	qhMatrix3( void );
	qhMatrix3( qhReal A11, qhReal A22, qhReal A33 );
	qhMatrix3( const qhVector3& C1, const qhVector3& C2, const qhVector3& C3 );

	// Assignment
	qhMatrix3& operator*=( const qhMatrix3& M );
	qhMatrix3& operator+=( const qhMatrix3& M );
	qhMatrix3& operator-=( const qhMatrix3& M );
	qhMatrix3& operator*=( qhReal F );
	qhMatrix3& operator/=( qhReal F );

	// Unary operators
	qhMatrix3 operator+( void ) const;
	qhMatrix3 operator-( void ) const;
};

// Binary arithmetic operators
qhMatrix3 operator*( const qhMatrix3& M1, const qhMatrix3& M2 );
qhMatrix3 operator+( const qhMatrix3& M1, const qhMatrix3& M2 );
qhMatrix3 operator-( const qhMatrix3& M1, const qhMatrix3& M2 );
qhMatrix3 operator*( qhReal F, const qhMatrix3& M );
qhMatrix3 operator*( const qhMatrix3& M, qhReal F );
qhMatrix3 operator/( const qhMatrix3& M, qhReal F );

// Comparison operators
bool operator==( const qhMatrix3& M1, const qhMatrix3& M2 );
bool operator!=( const qhMatrix3& M1, const qhMatrix3& M2 );

// Standard matrix operations
qhMatrix3 qhMul( const qhMatrix3& M1, const qhMatrix3& M2 );
qhMatrix3 qhTMul( const qhMatrix3& M1, const qhMatrix3& M2 );
qhMatrix3 qhTranspose( const qhMatrix3& M );
qhMatrix3 qhAdjoint( const qhMatrix3& M );
qhMatrix3 qhInvert( const qhMatrix3& M );
qhMatrix3 qhInvertT( const qhMatrix3& M );
qhMatrix3 qhConvert( const qhQuaternion& Q );
qhMatrix3 qhSkew( const qhVector3& V );

qhReal qhTrace( const qhMatrix3& M );
qhReal qhDet( const qhMatrix3& M );

// Constants
QH_GLOBAL_CONSTANT const qhMatrix3 QH_MAT3_ZERO = qhMatrix3( qhVector3( 0, 0, 0 ), qhVector3( 0, 0, 0 ), qhVector3( 0, 0, 0 ) ); 
QH_GLOBAL_CONSTANT const qhMatrix3 QH_MAT3_IDENTITY	= qhMatrix3( qhVector3( 1, 0, 0 ), qhVector3( 0, 1, 0 ), qhVector3( 0, 0, 1 ) ); 


//--------------------------------------------------------------------------------------------------
// qhQuaternion
//--------------------------------------------------------------------------------------------------
class qhQuaternion
	{
	public:
		// Attributes
		qhReal X, Y, Z, W;

		// Construction
		qhQuaternion( void );
		qhQuaternion( qhReal X, qhReal Y, qhReal Z, qhReal W );
		qhQuaternion( const qhVector3& V, qhReal S );
		qhQuaternion( const qhReal* Q );

		qhVector3 V( void ) const { return qhVector3( X, Y, Z ); }
		qhReal S( void ) const { return W;  }

	};

qhQuaternion operator*( const qhQuaternion& Q1, const qhQuaternion& Q2 );

qhQuaternion qhRotation( const qhVector3& V1, const qhVector3& V2 );
qhQuaternion qhRotationX( qhReal Rad );
qhQuaternion qhRotationY( qhReal Rad );
qhQuaternion qhRotationZ( qhReal Rad );

qhQuaternion qhConjugate( const qhQuaternion& Q );
qhQuaternion qhNormalize( const qhQuaternion& Q );

qhReal qhDot( const qhQuaternion& Q1, const qhQuaternion& Q2 );
qhReal qhLength( const qhQuaternion& Q );
qhReal qhLengthSq( const qhQuaternion& Q );

// Constants
QH_GLOBAL_CONSTANT const qhQuaternion QH_QUAT_ZERO = qhQuaternion( 0, 0, 0, 0 );
QH_GLOBAL_CONSTANT const qhQuaternion QH_QUAT_IDENTITY = qhQuaternion( 0, 0, 0, 1 );


//--------------------------------------------------------------------------------------------------
// qhTranform
//--------------------------------------------------------------------------------------------------
class qhTransform
{
public:
	// Attributes
	qhMatrix3 Rotation;
	qhVector3 Translation;

};


//--------------------------------------------------------------------------------------------------
// qhPlane
//--------------------------------------------------------------------------------------------------
class qhPlane
	{
	public:
		// Attributes
		qhVector3 Normal;
		qhReal Offset;

		// Construction
		qhPlane( void );
		qhPlane( const qhVector3& Normal, qhReal Offset );
		qhPlane( const qhVector3& Normal, const qhVector3& Point );
		qhPlane( const qhVector3& Point1, const qhVector3& Point2, const qhVector3& Point3 );

		void Negate( void );
		void Normalize( void );
		void Translate( const qhVector3& Translation );
		
		qhReal Distance( const qhVector3& Point ) const;
	};

// Standard plane operations
void qhStore( qhReal Dst[ 4 ], const qhPlane& Plane );


//--------------------------------------------------------------------------------------------------
// qhPlane
//--------------------------------------------------------------------------------------------------
class qhBounds3
	{
	public:
		// Attributes
		qhVector3 Min, Max;

		// Construction
		qhBounds3( void );
		qhBounds3( const qhVector3& Min, const qhVector3& Max );

		// Assignment
		qhBounds3& operator+=( const qhVector3& Point );
		qhBounds3& operator+=( const qhBounds3& Bounds );
	
		// Standard bounds operations
		qhVector3 GetCenter( void ) const;
		qhVector3 GetExtent( void ) const;

		qhReal GetVolume( void ) const;
	};

// Binary arithmetic operators
qhBounds3 operator+( const qhBounds3& Bounds1, const qhBounds3& Bounds2 );

// Comparison operators
bool operator==( const qhBounds3& Bounds1, const qhBounds3& Bounds2 );
bool operator!=( const qhBounds3& Bounds1, const qhBounds3& Bounds2 );

QH_GLOBAL_CONSTANT const qhBounds3 QH_BOUNDS3_EMPTY	= qhBounds3( qhVector3( QH_REAL_MAX, QH_REAL_MAX, QH_REAL_MAX ), qhVector3( -QH_REAL_MAX, -QH_REAL_MAX, -QH_REAL_MAX ) );
QH_GLOBAL_CONSTANT const qhBounds3 QH_BOUNDS3_INFINITE = qhBounds3( qhVector3( -QH_REAL_MAX, -QH_REAL_MAX, -QH_REAL_MAX ), qhVector3( QH_REAL_MAX, QH_REAL_MAX, QH_REAL_MAX ) );


//--------------------------------------------------------------------------------------------------
// qhBox
//--------------------------------------------------------------------------------------------------
class qhBox
{
public:
	qhVector3 Center;
	qhQuaternion Orientation;
	qhVector3 Extent;

	void GetVertices( qhVector3 Vertices[ 8 ] ) const;
};

qhBox qhBestFit( const qhArray< qhVector3 >& Vertices, qhReal Threshold = qhReal( 1 ) );
qhBox qhBestFit( int VertexCount, const void* VertexBase, int VertexStride = 3 * sizeof( qhReal ), qhReal Threshold = qhReal( 1 ) );



#include "qhMath.inl"

#pragma warning( pop )
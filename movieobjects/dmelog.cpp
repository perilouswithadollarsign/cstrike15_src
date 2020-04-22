//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmelog.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"
#include "vstdlib/random.h"

#include "tier0/dbg.h"

#include <limits.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
				 
union compressed_value_t
{
	struct 
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
};

struct compressed_stream_t
{
	const compressed_value_t *Get( int index ) const
	{
		// the offset for data 0 is right after the structure, so no need to store/retrieve it
		unsigned int offset;
		if ( index == 0 )
		{
			offset = sizeof( compressed_stream_t );
		}
		else
		{
			offset = m_Offset[ index - 1 ];
		}
		
		return (compressed_value_t *)((byte *)this + offset );
	}

	Vector							m_vecScale;
	Vector							m_vecBaseValue;
	unsigned int					m_Offset[ 2 ]; // x (implied), y, z or pitch (implied), yaw, roll if EulerAngles
};

LayerSelectionData_t::DataLayer_t::DataLayer_t( float frac, CDmeLogLayer *layer ) : 
	m_flStartFraction( frac )
{
	m_hData = layer;
}

LayerSelectionData_t::LayerSelectionData_t() :
	m_DataType( AT_UNKNOWN )
{
	m_nTimes[ 0 ] = DMETIME_ZERO;
	m_nTimes[ 1 ] = DMETIME_ZERO;
	m_nTimes[ 2 ] = DMETIME_ZERO;
	m_nTimes[ 3 ] = DMETIME_ZERO;
}

void LayerSelectionData_t::Release()
{
	for ( int i = 0; i < m_vecData.Count(); ++i )
	{
		DataLayer_t *dl = &m_vecData[ i ];
		if ( dl->m_hData.Get() )
		{
			g_pDataModel->DestroyElement( dl->m_hData->GetHandle() );
		}
	}
	m_vecData.Purge();
}

//-----------------------------------------------------------------------------
// Interpolatable types
//-----------------------------------------------------------------------------
inline bool IsInterpolableType( DmAttributeType_t type )
{
	return  ( type == AT_FLOAT ) ||
		( type == AT_TIME ) ||
		( type == AT_COLOR ) ||
		( type == AT_VECTOR2 ) ||
		( type == AT_VECTOR3 ) ||
		( type == AT_QANGLE ) ||											  
		( type == AT_QUATERNION );
}

float DmeLog_TimeSelection_t::AdjustFactorForInterpolatorType( float flFactor, int nSide ) const
{
	return ComputeInterpolationFactor( flFactor, m_nFalloffInterpolatorTypes[ nSide ] );
}

//-----------------------------------------------------------------------------
// NOTE: See TimeSelectionTimes_t for return values, -1 means before, TS_TIME_COUNT means after
//-----------------------------------------------------------------------------
static inline int ComputeRegionForTime( DmeTime_t t, TimeSelection_t regionTimes )
{
	if ( t >= regionTimes[TS_LEFT_HOLD] )
	{
		if ( t <= regionTimes[TS_RIGHT_HOLD] )
			return 2;
		return ( t <= regionTimes[TS_RIGHT_FALLOFF] ) ? 3 : 4;
	}
	return ( t >= regionTimes[TS_LEFT_FALLOFF] ) ? 1 : 0;
}


//-----------------------------------------------------------------------------
// NOTE: See TimeSelectionTimes_t for return values, -1 means before, TS_TIME_COUNT means after
//-----------------------------------------------------------------------------
int DmeLog_TimeSelection_t::ComputeRegionForTime( DmeTime_t curtime ) const
{
	return ::ComputeRegionForTime( curtime, m_nTimes );
}


//-----------------------------------------------------------------------------
// per-type averaging methods
//-----------------------------------------------------------------------------
float DmeLog_TimeSelection_t::GetAmountForTime( DmeTime_t dmetime ) const
{
	return ::GetAmountForTime( dmetime, m_nTimes, m_nFalloffInterpolatorTypes );
}

// catch-all for non-interpolable types - just holds first value
template < class T >
T Average( const T *pValues, int nValues)
{
	if ( IsInterpolableType( CDmAttributeInfo< T >::AttributeType() ) )
	{
		static bool first = true;
		if ( first )
		{
			first = false;
			Warning( "CDmeLog: interpolable type %s doesn't have an averaging function!", CDmAttributeInfo< T >::AttributeTypeName() );
		}
	}

	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return T(); // uninitialized for most value classes!!!

	return pValues[ 0 ];
}

// float version
template <>
float Average( const float *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return 0.0f;

	float sum = 0.0f;
	for ( int i = 0; i < nValues; ++i )
	{
		sum += pValues[ i ];
	}
	return sum / nValues;
}

// Color version
template <>
Color Average( const Color *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Color( 0, 0, 0, 0 );

	float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
	for ( int i = 0; i < nValues; ++i )
	{
		r += pValues[ i ].r();
		g += pValues[ i ].g();
		b += pValues[ i ].b();
		a += pValues[ i ].a();
	}
	float inv = nValues;
	return Color( r * inv, g * inv, b * inv, a * inv );
}

// Vector2 version
template <>
Vector2D Average( const Vector2D *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Vector2D( 0.0f, 0.0f );

	Vector2D sum( 0.0f, 0.0f );
	for ( int i = 0; i < nValues; ++i )
	{
		sum += pValues[ i ];
	}
	return sum / nValues;
}

// Vector3 version
template <>
Vector Average( const Vector *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Vector( 0.0f, 0.0f, 0.0f );

	Vector sum( 0.0f, 0.0f, 0.0f );
	for ( int i = 0; i < nValues; ++i )
	{
		sum += pValues[ i ];
	}
	return sum / nValues;
}

// QAngle version
template <>
QAngle Average( const QAngle *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return QAngle( 0.0f, 0.0f, 0.0f );

	Quaternion ave;
	AngleQuaternion( pValues[ 0 ], ave );

	// this is calculating the average by slerping with decreasing weights
	// for example: ave = 1/3 * q2 + 2/3 ( 1/2 * q1 + 1/2 * q0 )
	for ( int i = 1; i < nValues; ++i )
	{
		Quaternion quat;
		AngleQuaternion( pValues[ i ], quat );
		QuaternionSlerp( ave, quat, 1 / float( i + 1 ), ave );
	}

	QAngle qangle;
	QuaternionAngles( ave, qangle );
	return qangle;
}

// Quaternion version
template <>
Quaternion Average( const Quaternion *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Quaternion( 0.0f, 0.0f, 0.0f, 1.0f );

	Quaternion ave = pValues[ 0 ];

	// this is calculating the average by slerping with decreasing weights
	// for example: ave = 1/3 * q2 + 2/3 ( 1/2 * q1 + 1/2 * q0 )
	for ( int i = 1; i < nValues; ++i )
	{
		QuaternionSlerp( ave, pValues[ i ], 1 / float( i + 1 ), ave );
	}

	return ave;
}

// DmeTime_t version
template <>
DmeTime_t Average( const DmeTime_t *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return DMETIME_ZERO;

	double sum = pValues[ 0 ].GetTenthsOfMS();
	for ( int i = 1; i < nValues; ++i )
	{
		sum += pValues[ i ].GetTenthsOfMS();
	}

	return DmeTime_t( ( int )( sum / nValues ) );
}



//-----------------------------------------------------------------------------
// per-type interpolation methods
//-----------------------------------------------------------------------------

// catch-all for non-interpolable types - just holds first value
template < class T >
T Interpolate( float t, const T& ti, const T& tj, LogComponents_t componentFlags = LOG_COMPONENTS_ALL )
{
	if ( IsInterpolableType( CDmAttributeInfo< T >::AttributeType() ) )
	{
		static bool first = true;
		if ( first )
		{
			first = false;
			Warning( "CDmeLog: interpolable type %s doesn't have an interpolation function!", CDmAttributeInfo< T >::AttributeTypeName() );
		}
	}

	return ti;
}

// float version
template <>
float Interpolate( float t, const float& ti, const float& tj, LogComponents_t componentFlags )
{
	return t * tj + (1.0f - t) * ti;
}

// Color version
template <>
Color Interpolate( float t, const Color& ti, const Color& tj, LogComponents_t componentFlags )
{
	int ri, gi, bi, ai;
	int rj, gj, bj, aj;

	ti.GetColor( ri, gi, bi, ai );
	tj.GetColor( rj, gj, bj, aj );

	return Color( t * rj + (1.0f - t) * ri,
				  t * gj + (1.0f - t) * gi,
				  t * bj + (1.0f - t) * bi,
				  t * aj + (1.0f - t) * ai);
}

// Vector2 version
template <>
Vector2D Interpolate( float t, const Vector2D& ti, const Vector2D& tj, LogComponents_t componentFlags )
{
	return t * tj + (1.0f - t) * ti;
}

// Vector3 version
template <>
Vector Interpolate( float t, const Vector& ti, const Vector& tj, LogComponents_t componentFlags )
{
	Vector result = t * tj + (1.0f - t) * ti;

	if  ( ( componentFlags & LOG_COMPONENTS_ALL ) != LOG_COMPONENTS_ALL )
	{
		result.x = ( componentFlags & LOG_COMPONENTS_X ) ? result.x : ti.x;
		result.y = ( componentFlags & LOG_COMPONENTS_Y ) ? result.y : ti.y;
		result.z = ( componentFlags & LOG_COMPONENTS_Z ) ? result.z : ti.z;
	}

	return result;
}

// QAngle version
template <>
QAngle Interpolate( float t, const QAngle& ti, const QAngle& tj, LogComponents_t componentFlags )
{
	QAngle qaResult;
	Quaternion q, qi, qj;	// Some Quaternion temps for doing the slerp

	AngleQuaternion( ti, qi );			// Convert QAngles to Quaternions
	AngleQuaternion( tj, qj );
	QuaternionSlerp( qi, qj, t, q );	// Do a slerp as Quaternions
	QuaternionAngles( q, qaResult );	// Convert back to QAngles
	return qaResult;
}

// Quaternion version
template <>
Quaternion Interpolate( float t, const Quaternion& ti, const Quaternion& tj, LogComponents_t componentFlags )
{
	static Quaternion s_value;
	QuaternionSlerp( ti, tj, t, s_value );
	QuaternionNormalize( s_value );
	return s_value;
}

// DmeTime_t version
template <>
DmeTime_t Interpolate( float t, const DmeTime_t& ti, const DmeTime_t& tj, LogComponents_t componentFlags )
{
	double a = ti.GetTenthsOfMS();
	double b = tj.GetTenthsOfMS();
	return DmeTime_t( ( int )( t * b + (1.0f - t) * a ) );
}


// catch-all for non-interpolable types - just holds first value
template < class T >
T Curve_Interpolate( float t, DmeTime_t times[ 4 ], const T values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	if ( IsInterpolableType( CDmAttributeInfo< T >::AttributeType() ) )
	{
		static bool first = true;
		if ( first )
		{
			first = false;
			Warning( "CDmeLog: interpolable type %s doesn't have an interpolation function!", CDmAttributeInfo< T >::AttributeTypeName() );
		}
	}

	return values[ 1 ];
}

// float version
template <>
float Curve_Interpolate( float t, DmeTime_t times[ 4 ], const float values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	Vector args[ 4 ];
	for ( int i = 0; i < 4; ++i )
	{
		args[ i ].Init( times[ i ].GetSeconds(), values[ i ], 0.0f );
	}

	Vector vOut;
	int dummy;
	int earlypart, laterpart;

	// Not holding out value of previous curve...
	Interpolator_CurveInterpolatorsForType( curveTypes[ 1 ], dummy, earlypart );
	Interpolator_CurveInterpolatorsForType( curveTypes[ 2 ], laterpart, dummy );

	if ( earlypart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		VectorLerp( args[ 1 ], args[ 2 ], t, vOut );
		vOut.y = args[ 1 ].y;
	}
	else if ( laterpart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		VectorLerp( args[ 1 ], args[ 2 ], t, vOut );
		vOut.y = args[ 2 ].y;
	}
	else
	{
		bool sameCurveType = earlypart == laterpart ? true : false;
		if ( sameCurveType )
		{
			Interpolator_CurveInterpolate( laterpart, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ], t, vOut );
		}
		else // curves differ, sigh
		{
			Vector vOut1, vOut2;

			Interpolator_CurveInterpolate( earlypart, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ], t, vOut1 );
			Interpolator_CurveInterpolate( laterpart, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ], t, vOut2 );

			VectorLerp( vOut1, vOut2, t, vOut );
		}
	}

	// FIXME:  This means we can only work with curves that range from 0.0 to 1.0f!!!
	float retval = clamp( vOut.y, fmin, fmax );
	return retval;
}

// this is necessary to work around the weirdness of the interpolation schemes
// they all ignore time, except for CR's NormalizeX, and that assumes that time is in the x component of the vector!
void CurveInterpolateVectorHelper( float t, DmeTime_t times[ 4 ], const Vector values[ 4 ], int curveType, Vector &vOut )
{
	if ( curveType != INTERPOLATE_CATMULL_ROM_NORMALIZEX )
		return Interpolator_CurveInterpolate_NonNormalized( curveType, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut );

	Vector xArgs[ 4 ], yArgs[ 4 ], zArgs[ 4 ];
	for ( int i = 0; i < 4; ++i )
	{
		xArgs[ i ].Init( times[ i ].GetSeconds(), values[ i ].x, 0.0f );
		yArgs[ i ].Init( times[ i ].GetSeconds(), values[ i ].y, 0.0f );
		zArgs[ i ].Init( times[ i ].GetSeconds(), values[ i ].z, 0.0f );
	}

	Vector xOut, yOut, zOut;
	Interpolator_CurveInterpolate( curveType, xArgs[ 0 ], xArgs[ 1 ], xArgs[ 2 ], xArgs[ 3 ], t, xOut );
	Interpolator_CurveInterpolate( curveType, yArgs[ 0 ], yArgs[ 1 ], yArgs[ 2 ], yArgs[ 3 ], t, yOut );
	Interpolator_CurveInterpolate( curveType, zArgs[ 0 ], zArgs[ 1 ], zArgs[ 2 ], zArgs[ 3 ], t, zOut );

	vOut.Init( xOut.y, yOut.y, zOut.y ); // .y is intentional across all three inputs
}

// Vector version
template <>
Vector Curve_Interpolate( float t, DmeTime_t times[ 4 ], const Vector values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	Vector vOut;
	int dummy;
	int earlypart, laterpart;

	// Not holding out value of previous curve...
	Interpolator_CurveInterpolatorsForType( curveTypes[ 1 ], dummy, earlypart );
	Interpolator_CurveInterpolatorsForType( curveTypes[ 2 ], laterpart, dummy );

	if ( earlypart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 1 ];
	}
	else if ( laterpart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 2 ];
	}
	else
	{
		bool sameCurveType = earlypart == laterpart;
		if ( sameCurveType )
		{
			CurveInterpolateVectorHelper( t, times, values, laterpart, vOut );
		}
		else // curves differ, sigh
		{
			Vector vOut1, vOut2;

			CurveInterpolateVectorHelper( t, times, values, earlypart, vOut1 );
			CurveInterpolateVectorHelper( t, times, values, laterpart, vOut2 );

			VectorLerp( vOut1, vOut2, t, vOut );
		}
	}

	return vOut;
}

// Quaternion version
template <>
Quaternion Curve_Interpolate( float t, DmeTime_t times[ 4 ], const Quaternion values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	Quaternion vOut;
	int dummy;
	int earlypart, laterpart;

	// Not holding out value of previous curve...
	Interpolator_CurveInterpolatorsForType( curveTypes[ 1 ], dummy, earlypart );
	Interpolator_CurveInterpolatorsForType( curveTypes[ 2 ], laterpart, dummy );

	if ( earlypart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 1 ];
	}
	else if ( laterpart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 2 ];
	}
	else
	{
		bool sameCurveType = ( earlypart == laterpart ) ? true : false;
		if ( sameCurveType )
		{
			Interpolator_CurveInterpolate_NonNormalized( laterpart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut );
		}
		else // curves differ, sigh
		{
			Quaternion vOut1, vOut2;

			Interpolator_CurveInterpolate_NonNormalized( earlypart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut1 );
			Interpolator_CurveInterpolate_NonNormalized( laterpart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut2 );

			QuaternionSlerp( vOut1, vOut2, t, vOut );
		}
	}

	return vOut;
}


template< class T >
T ScaleValue( const T& value, float scale )
{
	return value * scale;
}
template<>
bool ScaleValue( const bool& value, float scale )
{
	Assert( 0 );
	return value;
}
template<>
Color ScaleValue( const Color& value, float scale )
{
	Assert( 0 );
	return value;
}
template<>
Vector4D ScaleValue( const Vector4D& value, float scale )
{
	return Vector4D( value.x * scale, value.y * scale, value.z * scale, value.w * scale );
}

template<>
Quaternion ScaleValue( const Quaternion& value, float scale )
{
	Quaternion q;
	QuaternionScale( value, scale, q );
	return q;
}

template<>
VMatrix ScaleValue( const VMatrix& value, float scale )
{
	Assert( 0 );
	return value;
}

template<>
CUtlSymbolLarge ScaleValue( const CUtlSymbolLarge& value, float scale )
{
	Assert( 0 );
	return value;
}

template<>
DmeTime_t ScaleValue( const DmeTime_t& value, float scale )
{
	return DmeTime_t( ( int )( value.GetTenthsOfMS() * scale ) );
}

template< class T >
float LengthOf( const T& value )
{
	Assert( 0 );
	return 0.0f;
}

template<>
float LengthOf( const float& value )
{
	return fabs( value );
}

template<>
float LengthOf( const bool& value )
{
	return value ? 1.0f : 0.0f;
}

template<>
float LengthOf( const int& value )
{
	return fabs( ( float ) value );
}

template<>
float LengthOf( const Color& value )
{
	return (float)sqrt( (float)( value.r() * value.r() +
		value.g() * value.g() +
		value.b() * value.b() +
		value.a() * value.a()) );
}

template<>
float LengthOf( const Vector4D& value )
{
	return sqrt( value.x * value.x +
		value.y * value.y +
		value.z * value.z +
		value.w * value.w );
}

template<>
float LengthOf( const Quaternion& value )
{
	return sqrt( value.x * value.x +
		value.y * value.y +
		value.z * value.z );
}

template<>
float LengthOf( const VMatrix& value )
{
	return 0.0f;
}

template<>
float LengthOf( const CUtlSymbolLarge& value )
{
	return 0.0f;
}

template<>
float LengthOf( const Vector2D& value )
{
	return value.Length();
}

template<>
float LengthOf( const Vector& value )
{
	return value.Length();
}

template<>
float LengthOf( const QAngle& value )
{
	return value.Length();
}

template<>
float LengthOf( const DmeTime_t& value )
{
	return value.GetSeconds();
}


template< class T >
T Subtract( const T& v1, const T& v2 )
{
	return v1 - v2;
}

template<>
bool Subtract( const bool& v1, const bool& v2 )
{
	return v1;
}

template<>
CUtlSymbolLarge Subtract( const CUtlSymbolLarge& v1, const CUtlSymbolLarge& v2 )
{
	return v1;
}

template<>
Color Subtract( const Color& v1, const Color& v2 )
{
	Color ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = clamp( v1[ i ] - v2[ i ], 0, 255 );
	}
	return ret;
}

template<>
Vector4D Subtract( const Vector4D& v1, const Vector4D& v2 )
{
	Vector4D ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = v1[ i ] - v2[ i ];
	}
	return ret;
}

template<>
Quaternion Subtract( const Quaternion& v1, const Quaternion& v2 )
{
	Quaternion ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = v1[ i ];
	}
	return ret;
}

template< class T >
T Add( const T& v1, const T& v2 )
{
	return v1 + v2;
}

template<>
bool Add( const bool& v1, const bool& v2 )
{
	return v1;
}

template<>
CUtlSymbolLarge Add( const CUtlSymbolLarge& v1, const CUtlSymbolLarge& v2 )
{
	return v1;
}


template<>
Color Add( const Color& v1, const Color& v2 )
{
	Color ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = clamp( v1[ i ] + v2[ i ], 0, 255 );
	}
	return ret;
}

template<>
Vector4D Add( const Vector4D& v1, const Vector4D& v2 )
{
	Vector4D ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = v1[ i ] + v2[ i ];
	}
	return ret;
}

template<>
Quaternion Add( const Quaternion& v1, const Quaternion& v2 )
{
	Quaternion ret;
	QuaternionMult( v2, v1, ret );

	return ret;
}

template < class T >
T Difference( const T &v1, const T &v2 )
{
	return Subtract< T >( v1, v2 );
}

template <>
Quaternion Difference( const Quaternion &v1, const Quaternion &v2 )
{
	Quaternion inv;
	QuaternionInvert( v2, inv );

	Quaternion ret;
	QuaternionMult( v1, inv, ret );

	return ret;
}


template < class T >
T TransformRelative( const matrix3x4_t &transformMatrix, const T &value )
{
	return value;
}

template <>
Vector TransformRelative( const matrix3x4_t &transformMatrix, const Vector &value )
{
	Vector result;
	VectorRotate( value, transformMatrix, result );
	return result;
}

template <>
Quaternion TransformRelative( const matrix3x4_t &transformMatrix, const Quaternion &rotation )
{
	Quaternion result;
	Vector newAxis;
	Vector axis( rotation.x, rotation.y, rotation.z );
	float sa = VectorNormalize( axis );
	VectorRotate( axis, transformMatrix, newAxis );
	result.x = newAxis.x * sa;
	result.y = newAxis.y * sa;
	result.z = newAxis.z * sa;
	result.w = rotation.w;
	return result;
}

template <>
matrix3x4_t TransformRelative( const matrix3x4_t &transformMatrix, const matrix3x4_t &value )
{
	// result = transform * value * transform-1
	matrix3x4_t transformInv, temp, result;
	MatrixInvert( transformMatrix, transformInv );
	ConcatTransforms( value, transformInv, temp );
	ConcatTransforms( transformMatrix, temp, result );
	return result;
}


template < class T >
T TransformAbsolute( const matrix3x4_t &transformMatrix, const T &value )
{
	return value;
}

template <>
Vector TransformAbsolute( const matrix3x4_t &transformMatrix, const Vector &value )
{
	Vector result;
	VectorTransform( value, transformMatrix, result );
	return result;
}

template <>
Quaternion TransformAbsolute( const matrix3x4_t &transformMatrix, const Quaternion &rotation )
{
	Quaternion quat, result;
	MatrixQuaternion( transformMatrix, quat );
	QuaternionMult( quat, rotation, result );
	return result;
}

template <>
matrix3x4_t TransformAbsolute( const matrix3x4_t &transformMatrix, const matrix3x4_t &value )
{
	matrix3x4_t result;
	ConcatTransforms( transformMatrix, value, result );
	return result;
}


//-----------------------------------------------------------------------------
// Purpose: Generic type rotation function, provides stub for types to which 
// rotation does not apply, simply returns the original value.
//-----------------------------------------------------------------------------
template < class T >
T Rotate( const Quaternion &rotation, const Vector &pivot, const T &value, const Quaternion &curRotation, bool local = true );

template < class T >
T Rotate( const Quaternion &rotation, const Vector &pivot, const T &value, const Quaternion &curRotation, bool local )
{
	return value;
}


//-----------------------------------------------------------------------------
// Purpose: Rotate a position value around the specified pivot in the current
// local space. The local space is constructed from the provided current 
// position and rotation.
//-----------------------------------------------------------------------------
template <>
Vector Rotate( const Quaternion &rotation, const Vector &pivot, const Vector &value, const Quaternion &currentRotation, bool local )
{
	Quaternion deltaRotation = rotation;
	
	// Construct a matrix with the current rotation
	matrix3x4_t currentTransform;
	QuaternionMatrix( currentRotation, value, currentTransform );

	// Convert the parent space rotation into local space
	if ( !local )
	{
		matrix3x4_t invTransform;
		MatrixInvert( currentTransform, invTransform );
		deltaRotation = TransformRelative( invTransform, rotation );
	}

	// Construct the transform matrix for the delta rotation
	matrix3x4_t xform;
	AngleMatrix( RadianEuler( deltaRotation ), xform );

	// Apply the pivot offset to the rotation matrix
	matrix3x4_t temp = xform;
	temp[0][3] += pivot.x;
	temp[1][3] += pivot.y;
	temp[2][3] += pivot.z;

	matrix3x4_t pivotOffset;
	SetIdentityMatrix( pivotOffset );
	PositionMatrix( -pivot, pivotOffset );
	ConcatTransforms( temp, pivotOffset, xform );
	
	// Construct the current transform matrix and apply the rotation transform to it to.
	ConcatTransforms( currentTransform, xform, xform );

	Vector ret;
	MatrixPosition( xform, ret );

	return ret;
}


//-----------------------------------------------------------------------------
// Purpose: Apply a relative rotation to the input quaternion rotation value
//-----------------------------------------------------------------------------
template <>
Quaternion Rotate( const Quaternion &rotation, const Vector &pivot, const Quaternion &value, const Quaternion &curRotation, bool local )
{
	Quaternion ret;	
	if ( local )
	{	
		QuaternionMult( value, rotation, ret );	
	}
	else
	{
		QuaternionMult( rotation, value, ret );
	}
	return ret;
}


//-----------------------------------------------------------------------------
// Purpose: Scale the provided quaternion by its rotation around an axis in
// such a way that the direction of the rotation is maintained. For example, a 
// quaternion with rotation of 270 degrees around an axis scaled by 0.5 will 
// have a 135 degree rotation around the same axis, instead of 315 degree
// rotation that the standard QuaternionScale() function would return.
//-----------------------------------------------------------------------------
void ScaleRotationQuaternion( const Quaternion &p, float scale, Quaternion &q )
{
	// Construct the axis from the input quaternion
	Vector axis;
	axis.x = p.x;
	axis.y = p.y;
	axis.z = p.z;
	VectorNormalize( axis );

	// Calculate the angle of rotation, note that the QuaternionAxisAngle() function is 
	// not used because it will force the angle to be between -180 and 180, losing the 
	// complete rotation information this function is specifically trying to maintain.
	float angle;
	angle = RAD2DEG( 2 * acos( p.w ) );

	// Scale the angle by the specified amount and construct the new quaternion
	// with the same axis as the original but with the new scaled rotation angle.
	angle = angle * scale;
	AxisAngleQuaternion( axis, angle, q );
}


IMPLEMENT_ABSTRACT_ELEMENT( DmeLogLayer,			CDmeLogLayer );

IMPLEMENT_ELEMENT_FACTORY( DmeIntLogLayer,			CDmeIntLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeFloatLogLayer,		CDmeFloatLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeBoolLogLayer,			CDmeBoolLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeColorLogLayer,		CDmeColorLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVector2LogLayer,		CDmeVector2LogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVector3LogLayer,		CDmeVector3LogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVector4LogLayer,		CDmeVector4LogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeQAngleLogLayer,		CDmeQAngleLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeQuaternionLogLayer,	CDmeQuaternionLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVMatrixLogLayer,		CDmeVMatrixLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeStringLogLayer,		CDmeStringLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeTimeLogLayer,			CDmeTimeLogLayer );

//-----------------------------------------------------------------------------
// explicit template instantiation
//-----------------------------------------------------------------------------
template class CDmeTypedLogLayer<int>;
template class CDmeTypedLogLayer<float>;
template class CDmeTypedLogLayer<bool>;
template class CDmeTypedLogLayer<Color>;
template class CDmeTypedLogLayer<Vector2D>;
template class CDmeTypedLogLayer<Vector>;
template class CDmeTypedLogLayer<Vector4D>;
template class CDmeTypedLogLayer<QAngle>;
template class CDmeTypedLogLayer<Quaternion>;
template class CDmeTypedLogLayer<VMatrix>;
template class CDmeTypedLogLayer<CUtlSymbolLarge>;
template class CDmeTypedLogLayer<DmeTime_t>;


IMPLEMENT_ABSTRACT_ELEMENT( DmeCurveInfo,			CDmeCurveInfo );

IMPLEMENT_ELEMENT_FACTORY( DmeIntCurveInfo,			CDmeIntCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeFloatCurveInfo,		CDmeFloatCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeBoolCurveInfo,		CDmeBoolCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeColorCurveInfo,		CDmeColorCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVector2CurveInfo,		CDmeVector2CurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVector3CurveInfo,		CDmeVector3CurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVector4CurveInfo,		CDmeVector4CurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeQAngleCurveInfo,		CDmeQAngleCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeQuaternionCurveInfo,	CDmeQuaternionCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVMatrixCurveInfo,		CDmeVMatrixCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeStringCurveInfo,		CDmeStringCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeTimeCurveInfo,		CDmeTimeCurveInfo );

//-----------------------------------------------------------------------------
// explicit template instantiation
//-----------------------------------------------------------------------------
template class CDmeTypedCurveInfo<int>;
template class CDmeTypedCurveInfo<float>;
template class CDmeTypedCurveInfo<bool>;
template class CDmeTypedCurveInfo<Color>;
template class CDmeTypedCurveInfo<Vector2D>;
template class CDmeTypedCurveInfo<Vector>;
template class CDmeTypedCurveInfo<Vector4D>;
template class CDmeTypedCurveInfo<QAngle>;
template class CDmeTypedCurveInfo<Quaternion>;
template class CDmeTypedCurveInfo<VMatrix>;
template class CDmeTypedCurveInfo<CUtlSymbolLarge>;
template class CDmeTypedCurveInfo<DmeTime_t>;


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeLog,				CDmeLog );

IMPLEMENT_ELEMENT_FACTORY( DmeIntLog,			CDmeIntLog );
IMPLEMENT_ELEMENT_FACTORY( DmeFloatLog,			CDmeFloatLog );
IMPLEMENT_ELEMENT_FACTORY( DmeBoolLog,			CDmeBoolLog );
IMPLEMENT_ELEMENT_FACTORY( DmeColorLog,			CDmeColorLog );
IMPLEMENT_ELEMENT_FACTORY( DmeVector2Log,		CDmeVector2Log );
IMPLEMENT_ELEMENT_FACTORY( DmeVector3Log,		CDmeVector3Log );
IMPLEMENT_ELEMENT_FACTORY( DmeVector4Log,		CDmeVector4Log );
IMPLEMENT_ELEMENT_FACTORY( DmeQAngleLog,		CDmeQAngleLog );
IMPLEMENT_ELEMENT_FACTORY( DmeQuaternionLog,	CDmeQuaternionLog );
IMPLEMENT_ELEMENT_FACTORY( DmeVMatrixLog,		CDmeVMatrixLog );
IMPLEMENT_ELEMENT_FACTORY( DmeStringLog,		CDmeStringLog );
IMPLEMENT_ELEMENT_FACTORY( DmeTimeLog,			CDmeTimeLog );


//-----------------------------------------------------------------------------
// explicit template instantiation
//-----------------------------------------------------------------------------
template class CDmeTypedLog<int>;
template class CDmeTypedLog<float>;
template class CDmeTypedLog<bool>;
template class CDmeTypedLog<Color>;
template class CDmeTypedLog<Vector2D>;
template class CDmeTypedLog<Vector>;
template class CDmeTypedLog<Vector4D>;
template class CDmeTypedLog<QAngle>;
template class CDmeTypedLog<Quaternion>;
template class CDmeTypedLog<VMatrix>;
template class CDmeTypedLog<CUtlSymbolLarge>;
template class CDmeTypedLog<DmeTime_t>;


//-----------------------------------------------------------------------------
// instantiate and initialize static vars
//-----------------------------------------------------------------------------
float CDmeIntLog::s_threshold = 0.0f;
float CDmeFloatLog::s_threshold = 0.0f;
float CDmeBoolLog::s_threshold = 0.0f;
float CDmeColorLog::s_threshold = 0.0f;
float CDmeVector2Log::s_threshold = 0.0f;
float CDmeVector3Log::s_threshold = 0.01f; // 0.01"
float CDmeVector4Log::s_threshold = 0.0f;
float CDmeQAngleLog::s_threshold = 0.001f;
float CDmeQuaternionLog::s_threshold = 0.001f; // 0.001 degrees, which for a 10-joint deep skeleton w/ 1' bones, translates to 0.01"
float CDmeVMatrixLog::s_threshold = 0.0f;
float CDmeStringLog::s_threshold = 0.0f;
float CDmeTimeLog::s_threshold = 0.0f;

template< class T >
class CLogTimeIterator
{
public:
	CLogTimeIterator( CDmeTypedLogLayer< T > *pLayer, bool bReverse = false );

	// Ways to add keys
	void AddInterval( DmeTime_t tStart, DmeTime_t tEnd, DmeTime_t tSampleInterval );
	void AddLayer( CDmeTypedLogLayer< T > *pLayer, DmeTime_t tStartingOffset );
	void AddLayer();
	void AddKeyTime( DmeTime_t t );

	int InvalidIndex() const;

	int First() const; // returns false if no samples in interval
	int Next( int idx ) const; // returns false after last sample

	DmeTime_t GetKeyTime( int idx ) const;
	void GetValue( int idx, T &val ) const;

private:

	bool					m_bReverse;
	// For layer values
	CDmeTypedLogLayer< T > *m_pLayer;

	// For combination, we build out a full list on Init
	CUtlRBTree< DmeTime_t >	m_rbKeyTimes;
};

template< class T >
CLogTimeIterator< T >::CLogTimeIterator( CDmeTypedLogLayer< T > *pLayer, bool bReverse /*=false*/ ) :
	m_bReverse( bReverse ),
	m_pLayer( pLayer ),
	m_rbKeyTimes( 0, 0, DefLessFunc( DmeTime_t ) )
{
}

template< class T >
void CLogTimeIterator< T >::AddKeyTime( DmeTime_t t )
{
	int idx = m_rbKeyTimes.Find( t );
	if ( idx != m_rbKeyTimes.InvalidIndex() )
		return;
	m_rbKeyTimes.Insert( t );
}

template< class T >
void CLogTimeIterator< T >::AddInterval( DmeTime_t tStart, DmeTime_t tEnd, DmeTime_t tSampleInterval )
{
	if ( m_bReverse )
	{
		swap( tStart, tEnd );
		tSampleInterval = -tSampleInterval;
	}

	for ( DmeTime_t t = tStart; t < tEnd + tSampleInterval; t += tSampleInterval )
	{
		AddKeyTime( t );
	}
}

template< class T >
void CLogTimeIterator< T >::AddLayer()
{
	// Add m_pLayer
	AddLayer( m_pLayer, DMETIME_ZERO );
}

template< class T >
void CLogTimeIterator< T >::AddLayer( CDmeTypedLogLayer< T > *pLayer, DmeTime_t tStartingOffset )
{
	for ( int i = 0 ; i < pLayer->GetKeyCount(); ++i )
	{
		DmeTime_t tKeyTime = pLayer->GetKeyTime( i );
		tKeyTime -= tStartingOffset;
		AddKeyTime( tKeyTime );
	}
}

template< class T >
DmeTime_t CLogTimeIterator< T >::GetKeyTime( int idx ) const
{
	// Don't check idx in release since we want this to be fast as possible
	Assert( m_rbKeyTimes.IsValidIndex( idx ) );

	return m_rbKeyTimes[ idx ];
}


template< class T >
void CLogTimeIterator< T >::GetValue( int idx, T &val ) const
{
	// Don't check idx in release since we want this to be fast as possible
	Assert( m_rbKeyTimes.IsValidIndex( idx ) );

	DmeTime_t tValue = m_rbKeyTimes[ idx ];
	val = m_pLayer->GetValue( tValue );
}

template< class T >
int CLogTimeIterator< T >::InvalidIndex()  const 
{
	return m_rbKeyTimes.InvalidIndex();
}


// returns false if no samples in interval
template< class T >
int CLogTimeIterator< T >::First() const
{
	if ( m_rbKeyTimes.Count() <= 0 )
		return m_rbKeyTimes.InvalidIndex();

	return m_bReverse ? m_rbKeyTimes.LastInorder() : m_rbKeyTimes.FirstInorder();
}

// returns false after last sample
template< class T >
int CLogTimeIterator< T >::Next( int idx )	const
{
	if ( m_bReverse )
	{
		return m_rbKeyTimes.PrevInorder( idx );
	}

	return m_rbKeyTimes.NextInorder( idx );
}

static DmeTime_t RemapTime( DmeTime_t tKeyTime, const TimeSelection_t &tSrcTimes, const TimeSelection_t &tDstTimes )
{
	int nSrcTSI = 1;
	if ( tKeyTime < tSrcTimes[ TS_LEFT_HOLD ] )
	{
		if ( tKeyTime < tSrcTimes[ TS_LEFT_FALLOFF ] )
		{
			DmeTime_t lDelta = tSrcTimes[ TS_LEFT_FALLOFF ] - tKeyTime;
			tKeyTime = tDstTimes[ TS_LEFT_FALLOFF ] - lDelta;
			return tKeyTime;
		}
		nSrcTSI = 0;
	}
	else if ( tKeyTime > tSrcTimes[ TS_RIGHT_HOLD ] )
	{
		if ( tKeyTime > tSrcTimes[ TS_RIGHT_FALLOFF ] )
		{
			DmeTime_t rDelta = tKeyTime	- tSrcTimes[ TS_RIGHT_FALLOFF ];
			tKeyTime = tDstTimes[ TS_RIGHT_FALLOFF ] + rDelta;
			return tKeyTime;
		}
		nSrcTSI = 2;
	}
	int nDstTSI = nSrcTSI;
	bool bHold = nSrcTSI == 1;

	DmeTime_t tSrcDuration = tSrcTimes[ nSrcTSI + 1 ] - tSrcTimes[ nSrcTSI ];
	DmeTime_t tDstDuration = tDstTimes[ nDstTSI + 1 ] - tDstTimes[ nDstTSI ];
	if ( !bHold && tDstDuration == DMETIME_ZERO )
		return tDstTimes[ nDstTSI ];

	DmeTime_t tDstTime;
	if ( tKeyTime == tSrcTimes[ TS_LEFT_HOLD ] )
	{
		tDstTime = tDstTimes[ TS_LEFT_HOLD ];
	}
	else if ( tKeyTime == tSrcTimes[ TS_RIGHT_HOLD ] )
	{
		tDstTime = tDstTimes[ TS_RIGHT_HOLD ];
	}
	else
	{
		float flRatio = MIN( 1.0f, ( tKeyTime - tSrcTimes[ nSrcTSI ] ).GetSeconds() / tSrcDuration.GetSeconds() );
		tDstTime = tDstTimes[ nDstTSI ] + flRatio * tDstDuration;
	}

	return tDstTime;
}

void CDmeLogLayer::OnConstruction()
{
	m_pOwnerLog = NULL;
	m_lastKey = 0;
	m_bLeftInfinite = false;
	m_bRightInfinite = false;
	m_times.Init( this, "times" );
	m_CurveTypes.Init( this, "curvetypes" );
	m_NonInterpolatedSegments.Init( this, "noninterpolatedsegments" );
}

void CDmeLogLayer::OnDestruction()
{
}

CDmeLog *CDmeLogLayer::GetOwnerLog()
{
	return m_pOwnerLog;
}

const CDmeLog *CDmeLogLayer::GetOwnerLog() const
{
	return m_pOwnerLog;
}

DmeTime_t CDmeLogLayer::GetBeginTime( bool bAllowInfinite ) const
{	
	if ( bAllowInfinite && m_bLeftInfinite )
		return DmeTime_t::MinTime();	

	if ( m_times.Count() == 0 )
		return DmeTime_t::InvalidTime();

	return m_times[ 0 ];
}

DmeTime_t CDmeLogLayer::GetEndTime( bool bAllowInfinite ) const
{
	if ( bAllowInfinite && m_bRightInfinite )
		return DmeTime_t::MaxTime();

	uint tn = m_times.Count();
	if ( tn == 0 )
		return DmeTime_t::InvalidTime();

	return m_times[ tn - 1 ];
}


// Validates that all keys are correctly sorted in time
bool CDmeLogLayer::ValidateKeys() const
{
	int nCount = m_times.Count();
	for ( int i = 1; i < nCount; ++i )
	{
		if ( m_times[i] < m_times[i-1] )
		{
			Warning( "Error in log %s! Key times are out of order [keys %d->%d: %d->%d]!\n",
				GetName(), i-1, i, m_times[i-1].GetTenthsOfMS(), m_times[i].GetTenthsOfMS() );
			return false;
		}
	}
	return true;
}

int CDmeLogLayer::FindKey( DmeTime_t time ) const
{
	int tn = m_times.Count();
	if ( m_lastKey >= 0 && m_lastKey < tn )
	{
		if ( time >= m_times[ m_lastKey ] )
		{
			// common case - playing forward
			for ( ; m_lastKey < tn - 1; ++m_lastKey )
			{
				if ( time < m_times[ m_lastKey + 1 ] )
					return m_lastKey;
			}

			// if time past the end, return the last key
			return m_lastKey;
		}
		else
		{
			tn = m_lastKey;
		}
	}

	for ( int ti = tn - 1; ti >= 0; --ti )
	{
		if ( time >= m_times[ ti ] )
		{
			m_lastKey = ti;
			return ti;
		}
	}

	return -1;
}

void CDmeLogLayer::ScaleSampleTimes( float scale )
{
	int nCount = m_times.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		DmeTime_t t = m_times.Get( i ) * scale;
		if ( i > 0 && t <= m_times.Get( i - 1 ) )
		{
			t = m_times.Get( i - 1 ) + DMETIME_MINDELTA;
		}
		m_times.Set( i, t );
	}
}


//-----------------------------------------------------------------------------
// Returns the number of keys
//-----------------------------------------------------------------------------
int CDmeLogLayer::GetKeyCount() const
{
	return m_times.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nKeyIndex - 
//			keyTime - 
//-----------------------------------------------------------------------------
void CDmeLogLayer::SetKeyTime( int nKeyIndex, DmeTime_t keyTime )
{
	m_times.Set( nKeyIndex, keyTime );
}

//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
DmeTime_t CDmeLogLayer::GetKeyTime( int nKeyIndex ) const
{
	return m_times[ nKeyIndex ];
}


//-----------------------------------------------------------------------------
// Scale + bias key times
//-----------------------------------------------------------------------------
void CDmeLogLayer::ScaleBiasKeyTimes( double flScale, DmeTime_t nBias )
{
	// Don't waste time on the identity transform
	if ( ( nBias == DMETIME_ZERO ) && ( fabs( flScale - 1.0 ) < 1e-5 ) )
		return;

	int nCount = GetKeyCount();
	for ( int i = 0; i < nCount; ++i )
	{
		DmeTime_t t = GetKeyTime( i );
		t.SetSeconds( t.GetSeconds() * flScale );
		t += nBias;
		SetKeyTime( i, t );
	}
}


//-----------------------------------------------------------------------------
// Scale the keys within the source time selection to fill the destination time 
// selection, keys outside the time selection will be shifted 
//-----------------------------------------------------------------------------
void CDmeLogLayer::RescaleSamplesInTimeSelection( const TimeSelection_t &srcTimeSelection, const TimeSelection_t & dstTimeSelection )
{
	DmeTime_t srcLeftFalloff  = srcTimeSelection[ TS_LEFT_FALLOFF ];
	DmeTime_t srcLeftHold	  = srcTimeSelection[ TS_LEFT_HOLD ];
	DmeTime_t srcRightHold	  = srcTimeSelection[ TS_RIGHT_HOLD ];
	DmeTime_t srcRightFalloff = srcTimeSelection[ TS_RIGHT_FALLOFF ];

	DmeTime_t dstLeftFalloff  = dstTimeSelection[ TS_LEFT_FALLOFF ];
	DmeTime_t dstLeftHold	  = dstTimeSelection[ TS_LEFT_HOLD ];
	DmeTime_t dstRightHold	  = dstTimeSelection[ TS_RIGHT_HOLD ];
	DmeTime_t dstRightFalloff = dstTimeSelection[ TS_RIGHT_FALLOFF ];
											 
	DmeTime_t preOffset		= dstLeftFalloff - srcLeftFalloff;
	DmeTime_t postOffset	= dstRightFalloff - srcRightFalloff;
	DmeTime_t leftDuration	= dstLeftHold - dstLeftFalloff;
	DmeTime_t holdDuration	= dstRightHold - dstLeftHold;
	DmeTime_t rightDuration = dstRightFalloff - dstRightHold;


	int nNumKeys = GetKeyCount();
	
	CUtlVector < DmeTime_t > newKeyTimes;
	newKeyTimes.SetCount( nNumKeys );

	for ( int iKey = 0; iKey < nNumKeys; ++iKey )
	{
		DmeTime_t keyTime = m_times[ iKey ];

		if ( keyTime < srcLeftFalloff )
		{
			newKeyTimes[ iKey ] = keyTime + preOffset;
		}
		else if ( keyTime < srcLeftHold )
		{
			float flParam = GetFractionOfTimeBetween( keyTime, srcLeftFalloff, srcLeftHold );
			newKeyTimes[ iKey ] = dstLeftFalloff + ( flParam * leftDuration );
		}
		else if ( keyTime < srcRightHold )
		{	
			float flParam = GetFractionOfTimeBetween( keyTime, srcLeftHold, srcRightHold );
			newKeyTimes[ iKey ] = dstLeftHold + ( flParam * holdDuration );
		}
		else if ( keyTime < srcRightFalloff )
		{
			float flParam = GetFractionOfTimeBetween( keyTime, srcRightHold, srcRightFalloff );
			newKeyTimes[ iKey ] = dstRightHold + ( flParam * rightDuration );
		}
		else
		{
			newKeyTimes[ iKey] = keyTime + postOffset;
		}
	}

	m_times = newKeyTimes;

}


//-----------------------------------------------------------------------------
// Returns the index of a particular key
//-----------------------------------------------------------------------------
int CDmeLogLayer::FindKeyWithinTolerance( DmeTime_t nTime, DmeTime_t nTolerance )
{
	int nClosest = -1;
	DmeTime_t nClosestTolerance = DmeTime_t::MaxTime();
	DmeTime_t nCurrTolerance;
	int start = 0, end = GetKeyCount() - 1;
	while ( start <= end )
	{
		int mid = (start + end) >> 1;
		DmeTime_t nDelta = nTime - m_times[mid];
		if ( nDelta > DmeTime_t( 0 ) )
		{
			nCurrTolerance = nDelta;
			start = mid + 1;
		}
		else if ( nDelta < DmeTime_t( 0 ) )
		{
			nCurrTolerance = -nDelta;
			end = mid - 1;
		}
		else
		{
			return mid;
		}

		if ( nCurrTolerance < nClosestTolerance )
		{
			nClosest = mid;
			nClosestTolerance = nCurrTolerance;
		}
	}
	if ( nClosestTolerance > nTolerance )
		return -1;
	return nClosest;
}

void CDmeLogLayer::OnUsingCurveTypesChanged()
{
	if ( g_pDataModel->IsUnserializing() )
		return;

	if ( !IsUsingCurveTypes() )
	{
		m_CurveTypes.RemoveAll();
	}
	else
	{
		m_CurveTypes.RemoveAll();
		// Fill in an array with the default curve type for
		int c = m_times.Count();
		for ( int i = 0; i < c; ++i )
		{
			m_CurveTypes.AddToTail( GetDefaultCurveType() );
		}
	}
}

bool CDmeLogLayer::IsUsingCurveTypes() const
{
	return GetOwnerLog() ? GetOwnerLog()->IsUsingCurveTypes() : false;
}

int CDmeLogLayer::GetDefaultCurveType() const
{
	return GetOwnerLog()->GetDefaultCurveType();
}

void CDmeLogLayer::SetKeyCurveType( int nKeyIndex, int curveType )
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
		return;

	Assert( GetOwnerLog()->IsUsingCurveTypes() );
	Assert( m_CurveTypes.IsValidIndex( nKeyIndex ) );
	if ( !m_CurveTypes.IsValidIndex( nKeyIndex ) )
		return;

	m_CurveTypes.Set( nKeyIndex, curveType );
}

int CDmeLogLayer::GetKeyCurveType( int nKeyIndex ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
		return CURVE_DEFAULT;

	Assert( GetOwnerLog()->IsUsingCurveTypes() );
	Assert( m_CurveTypes.IsValidIndex( nKeyIndex ) );
	if ( !m_CurveTypes.IsValidIndex( nKeyIndex ) )
		return GetOwnerLog()->GetDefaultCurveType();

	return m_CurveTypes[ nKeyIndex ];
}


void CDmeLogLayer::SetInfinite( bool bLeftInfinite, bool bRightInfinite )
{
	m_bLeftInfinite = bLeftInfinite;
	m_bRightInfinite = bRightInfinite;
}


//-----------------------------------------------------------------------------
// Removes all keys outside the specified time range
//-----------------------------------------------------------------------------
void CDmeLogLayer::RemoveKeysOutsideRange( DmeTime_t tStart, DmeTime_t tEnd )
{
	int i;
	int nKeysToRemove = 0;
	int nKeyCount = m_times.Count();
	for ( i = 0; i < nKeyCount; ++i, ++nKeysToRemove )
	{
		if ( m_times[i] >= tStart )
			break;
	}

	if ( nKeysToRemove )
	{
		RemoveKey( 0, nKeysToRemove );
	}

	nKeyCount = m_times.Count();
	for ( i = 0; i < nKeyCount; ++i )
	{
		if ( m_times[i] > tEnd )
			break;
	}
	nKeysToRemove = nKeyCount - i;
	if ( nKeysToRemove )
	{
		RemoveKey( i, nKeysToRemove );
	}
}


SegmentInterpolation_t CDmeLogLayer::GetSegmentInterpolationSetting( int nKeyIndex ) const
{
	if( m_NonInterpolatedSegments.Count() == 0 ) //We don't allocate an array until at least one non-interpolated segment exists
		return SEGMENT_INTERPOLATE;

	//if at least one exists, the array size keeps parity with other arrays
	Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
	Assert( (nKeyIndex >= 0) && (nKeyIndex < m_NonInterpolatedSegments.Count()) );
	return m_NonInterpolatedSegments[ nKeyIndex ] ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;
}


SegmentInterpolation_t CDmeLogLayer::GetSegmentInterpolationSetting( int nStartKeyIndex, int nEndKeyIndex ) const
{
	if( m_NonInterpolatedSegments.Count() == 0 ) //We don't allocate an array until at least one non-interpolated segment exists
		return SEGMENT_INTERPOLATE;

	//if at least one exists, the array size keeps parity with other arrays
	Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );

	int iLastKey = m_NonInterpolatedSegments.Count();

	//it's easier to bounds check here in a central location than in all bits of leaf code that want to check a (usually forward looking) range
	nStartKeyIndex = MAX( nStartKeyIndex, 0 );
	nStartKeyIndex = MIN( nStartKeyIndex, iLastKey );
	
	nEndKeyIndex = MAX( nEndKeyIndex, nStartKeyIndex );
	nEndKeyIndex = MIN( nEndKeyIndex, iLastKey );

	for( int i = nStartKeyIndex; i <= nEndKeyIndex; ++i )
	{
		if( m_NonInterpolatedSegments[i] )
			return SEGMENT_NOINTERPOLATE;
	}

	return SEGMENT_INTERPOLATE;
}


SegmentInterpolation_t CDmeLogLayer::GetSegmentInterpolationSetting( DmeTime_t time ) const
{
	if( m_NonInterpolatedSegments.Count() == 0 ) //We don't allocate an array until at least one non-interpolated segment exists
		return SEGMENT_INTERPOLATE;

	return GetSegmentInterpolationSetting( FindKey( time ) );
}


SegmentInterpolation_t CDmeLogLayer::GetSegmentInterpolationSetting( DmeTime_t startTime, DmeTime_t endTime, bool bExcludeActualEndTimeKey ) const
{
	if( m_NonInterpolatedSegments.Count() == 0 ) //We don't allocate an array until at least one non-interpolated segment exists
		return SEGMENT_INTERPOLATE;

	int nStartKey = (startTime == DMETIME_INVALID) ? 0 : FindKey( startTime );
	int nEndKey = (endTime == DMETIME_INVALID) ? (m_NonInterpolatedSegments.Count() - 1) : FindKey( endTime );

	if( bExcludeActualEndTimeKey && (nEndKey > nStartKey) && (endTime != DMETIME_INVALID) )
	{
		--nEndKey;
	}

	return GetSegmentInterpolationSetting( nStartKey, nEndKey );
}

template < class T >
class CUndoLayerAdded : public CUndoElement
{
	typedef CUndoElement BaseClass;

public:
	CUndoLayerAdded( const char *desc, CDmeLog *pLog ) :
		BaseClass( desc ),
		m_bNeedsCleanup( false ),
		m_hLog( pLog )
	{
		Assert( pLog && pLog->GetFileId() != DMFILEID_INVALID );
	}

	virtual ~CUndoLayerAdded()
	{
		if ( m_bNeedsCleanup )
		{
			g_pDataModel->DestroyElement( m_hLayer );
		}
	}

	virtual void Undo()
	{
		m_bNeedsCleanup = true;
		CDmeLogLayer *pLayer = m_hLog->RemoveLayerFromTail();
		Assert( pLayer );
		m_hLayer = pLayer ? pLayer->GetHandle() : DMELEMENT_HANDLE_INVALID;
		g_pDataModel->MarkHandleInvalid( m_hLayer );
	}

	virtual void Redo()
	{
		m_bNeedsCleanup = false;
		g_pDataModel->MarkHandleValid( m_hLayer );
		m_hLog->AddLayerToTail( GetElement< CDmeTypedLogLayer< T > >( m_hLayer ) );
	}

	virtual const char	*GetDesc()
	{
		static char sz[ 512 ];
		int iLayer = m_hLog->GetTopmostLayer();
		if ( iLayer >= 0 )
		{
			CDmeLogLayer *layer = m_hLog->GetLayer( iLayer );
			Q_snprintf( sz, sizeof( sz ), "addlayer: log %p lc[%d], layer %p",
				m_hLog.Get(), m_hLog->GetNumLayers(), layer );
		}
		else
		{
			Q_snprintf( sz, sizeof( sz ), "addlayer: log %p lc[%d], layer NULL",
				m_hLog.Get(), m_hLog->GetNumLayers() );
		}
		return sz;
	}

private:
	CDmeHandle< CDmeLog >	m_hLog;
	bool					m_bNeedsCleanup;
	CDmeUndoHandle			m_hLayer;
};

template < class T >
class CUndoFlattenLayers : public CUndoElement
{
	typedef CUndoElement BaseClass;

public:

	CUndoFlattenLayers( const char *desc, CDmeTypedLog< T > *pLog, float threshold, int flags, int baseLayer ) :
		BaseClass( desc ),
		m_bNeedsCleanup( true ),
		m_hLog( pLog ),
		m_nFlags( flags ),
		m_flThreshold( threshold ),
		m_nBaseLayer( baseLayer ),
		m_nLogLayers( 0 )
	{
		Assert( pLog && pLog->GetFileId() != DMFILEID_INVALID );
		LatchCurrentLayers();
	}

	virtual ~CUndoFlattenLayers()
	{
		if ( m_bNeedsCleanup )
		{
			for ( int i = 0; i < m_hLayers.Count(); ++i )
			{
				m_hLayers[ i ] = DMELEMENT_HANDLE_INVALID;
#ifdef _DEBUG
				CDmElement *pElement = g_pDataModel->GetElement( m_hLayers[ i ] );
				Assert( !pElement || pElement->IsStronglyReferenced() );
#endif
			}
		}
	}

	virtual void Undo()
	{
		m_bNeedsCleanup = false;

		int startLayerCount = m_hLog->GetNumLayers();
		int undoLayerCount = m_hLayers.Count();
		Assert( startLayerCount >= 1 );
		Assert( undoLayerCount >= 1 );

		// Calculate the number of layers the log wil have after the undo operation.
		int newLayerCount = ( startLayerCount + undoLayerCount - 1 );
		Assert( m_nLogLayers == newLayerCount );

		if ( m_nLogLayers == newLayerCount )
		{
			for ( int i = 0; i < m_hLayers.Count(); ++i )
			{
				if ( i == 0 )
				{
					// Copy base layer in place so handles to the base layer remain valid
					CDmeTypedLogLayer< T > *base = m_hLog->GetLayer( m_nBaseLayer );
					base->CopyLayer( GetElement< CDmeTypedLogLayer< T > >( m_hLayers[ i ] ) );
					// Release it since we didn't txfer it over
					g_pDataModel->DestroyElement( m_hLayers[ i ] );
				}
				else
				{
					// This transfers ownership, so no Release needed
					m_hLog->AddLayerToTail( GetElement< CDmeTypedLogLayer< T > >( m_hLayers[ i ] ) );
				}
			}
		}

		m_hLayers.RemoveAll();
	}

	virtual void Redo()
	{
		m_bNeedsCleanup = true;
		Assert( m_hLayers.Count() == 0 );

		LatchCurrentLayers();
		// Flatten them again (won't create undo records since we're in undo already)

		m_hLog->FlattenLayers( m_flThreshold, m_nFlags, m_nBaseLayer );
	}

	virtual const char	*GetDesc()
	{
		static char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "flatten log %p lc[%d]",
			m_hLog.Get(), m_hLayers.Count() );
		return sz;
	}

private:

	void LatchCurrentLayers()
	{
		CDisableUndoScopeGuard guard;
		Assert( m_hLayers.Count() == 0 );
		Assert( m_hLog->GetNumLayers() >= 1 );
		
		// Save the number of layers in the log so that we can verify the
		// log is in the same state when performing the undo operation.
		m_nLogLayers = m_hLog->GetNumLayers();
	
		// Start with the layer that is specified as the base layer of the flatten operation,
		// and copy the contents of that layer and the of the layers above it.
		for ( int i = m_nBaseLayer; i < m_hLog->GetNumLayers(); ++i )
		{
			CDmeTypedLogLayer< T > *pLayer = CastElement< CDmeTypedLogLayer< T > >( CreateLayer< T >( m_hLog ) );
			pLayer->CopyLayer( m_hLog->GetLayer( i ) );
			m_hLayers.AddToTail( pLayer->GetHandle() );
		}
	}

	CDmeHandle< CDmeTypedLog< T > >	m_hLog;
	bool							m_bNeedsCleanup;
	CUtlVector< CDmeUndoHandle >	m_hLayers;
	int								m_nFlags;
	float							m_flThreshold;
	int								m_nBaseLayer;
	int								m_nLogLayers;
};

//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone
//-----------------------------------------------------------------------------
void ExtractAnimValue( int frame, const compressed_value_t *panimvalue, float scale, float &v1, float &v2 )
{
	if ( !panimvalue )
	{
		v1 = v2 = 0;
		return;
	}

	// Avoids a crash reading off the end of the data
	// There is probably a better long-term solution; Ken is going to look into it.
	if ( ( panimvalue->num.total == 1 ) && ( panimvalue->num.valid == 1 ) )
	{
		v1 = v2 = panimvalue[1].value * scale;
		return;
	}

	int k = frame;

	// find the data list that has the frame
	while (panimvalue->num.total <= k)
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;
		if ( panimvalue->num.total == 0 )
		{
			Assert( 0 ); // running off the end of the animation stream is bad
			v1 = v2 = 0;
			return;
		}
	}
	if (panimvalue->num.valid > k)
	{
		// has valid animation data
		v1 = panimvalue[k+1].value * scale;

		if (panimvalue->num.valid > k + 1)
		{
			// has valid animation blend data
			v2 = panimvalue[k+2].value * scale;
		}
		else
		{
			if (panimvalue->num.total > k + 1)
			{
				// data repeats, no blend
				v2 = v1;
			}
			else
			{
				// pull blend from first data block in next list
				v2 = panimvalue[panimvalue->num.valid+2].value * scale;
			}
		}
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
		if (panimvalue->num.total > k + 1)
		{
			// data repeats, no blend
			v2 = v1;
		}
		else
		{
			// pull blend from first data block in next list
			v2 = panimvalue[panimvalue->num.valid + 2].value * scale;
		}
	}
}


void ExtractAnimValue( int frame, const compressed_value_t *panimvalue, float scale, float &v1 )
{
	if ( !panimvalue )
	{
		v1 = 0;
		return;
	}

	int k = frame;

	while (panimvalue->num.total <= k)
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;
		if ( panimvalue->num.total == 0 )
		{
			Assert( 0 ); // running off the end of the animation stream is bad
			v1 = 0;
			return;
		}
	}
	if (panimvalue->num.valid > k)
	{
		v1 = panimvalue[k+1].value * scale;
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
	}
}

template< class T >
void GetComponentValues( const T &in, Vector &out )
{
	Assert( 0 );
}

template<>
void GetComponentValues( const Vector &in, Vector &out )
{
	out = in;
}

template<>
void GetComponentValues( const Quaternion &in, Vector &out )
{
	RadianEuler ang;
	QuaternionAngles( in, ang );

	for ( int i = 0; i < 3; ++i )
	{
		out[ i ] = ang[ i ];
		while (out[ i ] >= M_PI)
			out[ i ] -= M_PI * 2;
		while (out[ i ] < -M_PI)
			out[ i ] += M_PI * 2;
	}
}

template< class T >
void InitBounds( float &mins, float &maxs )
{
	Assert( 0 );
}

template<>
void InitBounds< Vector >( float &mins, float &maxs )
{
	mins = -128.0f;
	maxs = 128.0f;
}

template<>
void InitBounds< Quaternion >( float &mins, float &maxs )
{
	mins = -M_PI / 8.0;
	maxs = M_PI / 8.0;
}

template< class T >
void CDmeTypedLogLayer< T >::CompressValues( CDmaArray< T > &stream, CUtlBinaryBlock &block, float flMaxError = 0.1f )
{
	// Don't bother
	if ( stream.Count() <= 2 )
		return;

	// Allocate sufficient scratch space
	size_t memsize = ALIGN_VALUE( sizeof( compressed_stream_t ) + 3 * ( 2 * stream.Count() + 1 ) * sizeof( compressed_value_t ), 4 );

	byte *scratch = new byte[ memsize ];
	Q_memset( scratch, 0, memsize );

	compressed_stream_t *compressed = (compressed_stream_t *)scratch;
	// First payload goes here
	byte *pOut = (byte *)( compressed + 1 );

	Vector minv, maxv;
	for ( int k = 0; k < 3; ++k )
	{
		InitBounds< T >( minv[ k ], maxv[ k ] );
	}

	int nCount[ 3 ];

	CUtlVector< Vector > vecComponentStream;
	for ( int n = 0; n < stream.Count(); ++n )
	{
		Vector &compValue = vecComponentStream[ vecComponentStream.AddToTail() ];
		GetComponentValues( stream[ n ], compValue );
		if ( n == 0 )
		{
			compressed->m_vecBaseValue = compValue;
		}

		compValue -= compressed->m_vecBaseValue;
		for ( int k = 0; k < 3; ++k )
		{
			if (compValue[ k ] < minv[ k ])
				minv[ k ] = compValue[ k ];
			if (compValue[ k ] > maxv[ k ])
				maxv[ k ] = compValue[ k ];
		}
	}

	// Per component, compute scaled values and then rle them
	for ( int k = 0; k < 3; ++k )
	{
		float scale;
		if ( minv[ k ] < maxv[ k ] )
		{
			if ( -minv[ k ]> maxv[ k ] )
			{
				scale = minv[ k ] / -32768.0f;
			}
			else
			{
				scale = maxv[ k ] / 32767.0f;
			}
		}
		else
		{
			scale = 1.0f / 32.0f;
		}

		if ( scale > flMaxError )
		{
			// Dynamic range was too large
			Warning( "compression error would be too large %f [%f %f]\n", scale, minv[ k ], maxv[ k ] );
			delete[] scratch;
			return;
		}

		compressed->m_vecScale[ k ] = scale;

		CUtlVector< short > value;
		value.EnsureCount( vecComponentStream.Count() );
		// quantize the values into shorts
		for ( int n = 0; n < vecComponentStream.Count(); n++ )
		{
			value[ n ] = (short)( vecComponentStream[ n ][ k ] / scale );
		}

		// initialize animation RLE block
		compressed_value_t *pStart = (compressed_value_t *)pOut;
		compressed_value_t *pcount, *pvalue;
		pcount = pStart; 
		pvalue = pcount + 1;

		pcount->num.valid = 1;
		pcount->num.total = 1;
		pvalue->value = value[0];
		pvalue++;

		// build a RLE of deltas from the default pose
		for ( int m = 1; m < vecComponentStream.Count(); m++ )
		{
			if ( pcount->num.total == 255 )
			{
				// chain too long, force a new entry
				pcount = pvalue;
				pvalue = pcount + 1;
				pcount->num.valid++;
				pvalue->value = value[m];
				pvalue++;
			} 
			// insert value if they're not equal, 
			// or if we're not on a run and the run is less than 3 units
			else if ((value[m] != value[m-1]) 
				|| ((pcount->num.total == pcount->num.valid) && ((m < vecComponentStream.Count() - 1) && value[m] != value[m+1])))
			{
				if (pcount->num.total != pcount->num.valid)
				{
					pcount = pvalue;
					pvalue = pcount + 1;
				}
				pcount->num.valid++;
				pvalue->value = value[m];
				pvalue++;
			}
			pcount->num.total++;
		}

		nCount[ k ] = pvalue - pStart;

		size_t nSize = nCount[ k ] * sizeof( compressed_value_t );

		size_t offset = (byte *)pStart - (byte *)compressed;
		if ( k >= 1 )
		{
			compressed->m_Offset[ k - 1 ] = (unsigned int)offset;
		}
		pOut += nSize;
	}

	size_t nTotalMem = pOut - scratch;
	Assert( nTotalMem <= memsize );
	block.Set( scratch, nTotalMem );
	delete[] scratch;
}

//-----------------------------------------------------------------------------
// CDmeTypedLogLayer - a generic typed layer used by a log
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::OnConstruction()
{
	m_values.Init( this, "values" );
	m_Compressed.Init( this, "compressed", FATTRIB_HIDDEN );
}

template< class T >
void CDmeTypedLogLayer< T >::Compress()
{
	if ( IsCompressed() )
		return;

	CUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "CDmeTypedLogLayer::Compress" );

	Assert( m_Compressed->Length() == 0 );

	CUtlBinaryBlock block;
	CompressValues( m_values, block, 0.1f );
	m_Compressed = block;
	if ( IsCompressed() )
	{
		m_values.RemoveAll();
	}
}

template< class T >
void CDmeTypedLogLayer< T >::Decompress()
{
	if ( !IsCompressed() )
		return;

	CUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "CDmeTypedLogLayer::Decompress" );

	Assert( m_values.Count() == 0 );
	m_values.RemoveAll();

	CUtlVector< T > values;
	for ( int i = 0; i < m_times.Count(); ++i )
	{
		T &val = values[ values.AddToTail() ];
		GetCompressedValue( i, val );
	}

	m_values.CopyArray( values.Base(), values.Count() );
	CUtlBinaryBlock empty;
	m_Compressed = empty;
	Assert( !IsCompressed() );
}

template< class T >
bool CDmeTypedLogLayer< T >::IsCompressed() const
{
	return ( m_Compressed->Length() > 0 ) ? true : false;
}

template< class T >
size_t CDmeTypedLogLayer< T >::GetCompressedSize() const
{
	return m_Compressed->Length();
}

template< class T >
size_t CDmeTypedLogLayer< T >::GetDataSize() const
{
	return sizeof( T );
}


template< class T >
void CDmeTypedLogLayer< T >::SetOwnerLog( CDmeLog *owner )
{
	Assert( owner );
	Assert( assert_cast< CDmeTypedLog< T > * >( owner ) );
	m_pOwnerLog = owner;
}

template< class T >
CDmeTypedLog< T > *CDmeTypedLogLayer< T >::GetTypedOwnerLog()
{
	return assert_cast< CDmeTypedLog< T > * >( m_pOwnerLog );
}

template< class T >
const CDmeTypedLog< T >	*CDmeTypedLogLayer< T >::GetTypedOwnerLog() const
{
	return assert_cast< CDmeTypedLog< T > * >( m_pOwnerLog );
}

template< class T >
void CDmeTypedLogLayer< T >::OnDestruction()
{
}

template< class T >
void CDmeTypedLogLayer< T >::RemoveKeys( DmeTime_t starttime )
{
	// Manipulating values will require decompressing data
	Decompress();

	int ti = FindKey( starttime );
	if ( ti < 0 )
	{
		ClearKeys();
		return;
	}

	if ( starttime > m_times[ ti ] )
		++ti;

	int nKeys = m_times.Count() - ti;
	if ( nKeys == 0 )
		return;

	m_times.RemoveMultiple( ti, nKeys );
	m_values.RemoveMultiple( ti, nKeys );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.RemoveMultiple( ti, nKeys );
	}

	if( m_NonInterpolatedSegments.Count() > 0 )
	{
		m_NonInterpolatedSegments.RemoveMultiple( ti, nKeys );
	}

	if ( m_lastKey >= ti && m_lastKey < ti + nKeys )
	{
		m_lastKey = ( ti > 0 ) ? ti - 1 : 0;
	}
}

template< class T >
void CDmeTypedLogLayer< T >::ClearKeys()
{
	// Manipulating values will require decompressing data
	Decompress();

	m_times.RemoveAll();
	m_values.RemoveAll();
	m_CurveTypes.RemoveAll();
	m_NonInterpolatedSegments.RemoveAll();
	m_lastKey = 0;
}

template< class T >
void CDmeTypedLogLayer< T >::RemoveKey( int nKeyIndex, int nNumKeysToRemove /*= 1*/ )
{
	// Manipulating values will require decompressing data
	Decompress();

	m_times.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	m_values.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	}

	if( m_NonInterpolatedSegments.Count() > 0 )
	{
		m_NonInterpolatedSegments.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	}
}


//-----------------------------------------------------------------------------
// Sets all of the keys on the layer from the provided array of times and
// values.
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::SetAllKeys( const CUtlVector< DmeTime_t > &times, const CUtlVector< T > &values )
{
	// Manipulating values will require decompressing data
	Decompress();

	// This method may not be used for logs using curve types
	Assert( !IsUsingCurveTypes() );
	if ( IsUsingCurveTypes() )
		return;

	// The provided arrays must be the same size
	Assert( times.Count() == values.Count() );
	if ( times.Count() != values.Count() )
		return;

	m_times = times;
	m_values = values;
	m_lastKey = 0;

	if ( m_CurveTypes.Count() > 0 )
	{
		m_CurveTypes.RemoveAll();
	}

	m_NonInterpolatedSegments.RemoveAll(); 
}



//-----------------------------------------------------------------------------
// Copy all of the keys into the specified arrays
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::GetAllKeys( CUtlVector< DmeTime_t > &times, CUtlVector< T > &values ) const
{
	// Manipulating values will require decompressing data
	const_cast< CDmeTypedLogLayer< T > * >( this )->Decompress();

	// This method may not be used for logs using curve types
	Assert( !IsUsingCurveTypes() );
	if ( IsUsingCurveTypes() )
		return;

	times.CopyArray( m_times.Base(), m_times.Count() );
	values.CopyArray( m_values.Base(), m_values.Count() );
}

//-----------------------------------------------------------------------------
// Sets a key, removes all keys after this time
// FIXME: This needs to account for interpolation!!!
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::SetKey( DmeTime_t time, const T& value, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*=CURVE_DEFAULT*/, bool removeRedundant /*=true*/ )
{
	// Manipulating values will require decompressing data
	Decompress();

	Assert( time != DMETIME_INVALID );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	// Remove all keys after this time
	RemoveKeys( time );

	// Add the key and then check to see if the penultimate key is still necessary
	m_times.AddToTail( time );
	m_values.AddToTail( value );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.AddToTail( curveType );
	}

	if( m_NonInterpolatedSegments.Count() > 0 )
	{
		m_NonInterpolatedSegments.AddToTail( (interpSetting == SEGMENT_NOINTERPOLATE) );
		Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
	}
	else if( interpSetting == SEGMENT_NOINTERPOLATE )
	{
		//first key with non-interpolation setting. Create the whole set
		m_NonInterpolatedSegments.EnsureCount( m_times.Count() - 1 );
		m_NonInterpolatedSegments.AddToTail( (interpSetting == SEGMENT_NOINTERPOLATE) );
		Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
	}

	int nKeys = m_values.Count();
	if ( ( nKeys < 3 ) || 
		 ( IsUsingCurveTypes() && ( curveType != m_CurveTypes[ nKeys -1 ] || ( curveType != m_CurveTypes[ nKeys - 2 ] ) ) ) 
		 )
	{
		return;
	}

	// Done if not removing redundant penultimate keys.
	if ( !removeRedundant )
	{
		return;
	}

	// If adding the new means that the penultimate key's value was unneeded, then we will remove the penultimate key value
	T check = GetValueSkippingKey( nKeys - 2 );
	T oldPenultimateValue = m_values[ nKeys - 2 ];
	if ( GetTypedOwnerLog()->ValuesDiffer( oldPenultimateValue, check ) )
	{
		return;
	}

	if( (interpSetting == SEGMENT_NOINTERPOLATE) ||
		(GetSegmentInterpolationSetting( nKeys - 2 ) == SEGMENT_NOINTERPOLATE) ||
		(GetSegmentInterpolationSetting( nKeys - 3 ) == SEGMENT_NOINTERPOLATE) )
	{
		return;
	}
	
	// Remove penultimate, it's not needed
	m_times.Remove( nKeys - 2 );
	m_values.Remove( nKeys - 2 );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.Remove( nKeys - 2 );
	}

	if( m_NonInterpolatedSegments.Count() > 0 )
	{
		m_NonInterpolatedSegments.Remove( nKeys - 2 );
	}
}


//-----------------------------------------------------------------------------
// Finds a key within tolerance, or adds one
//-----------------------------------------------------------------------------
template< class T >
int CDmeTypedLogLayer< T >::FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*=CURVE_DEFAULT*/ )
{
	// Manipulating values will require decompressing data
	Decompress();

	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	// NOTE: This math must occur in 64bits because the max delta nDelta
	// can be 33 bits large. Bleah.
	int nClosest = -1;
	int64 nClosestTolerance = DmeTime_t::MinTime().GetTenthsOfMS();
	int64 nCurrTolerance;
	int start = 0, end = GetKeyCount() - 1;
	while ( start <= end )
	{
		int mid = (start + end) >> 1;
		int64 nDelta = (int64)nTime.GetTenthsOfMS() - (int64)m_times[mid].GetTenthsOfMS();
		if ( nDelta > 0 )
		{
			nCurrTolerance = nDelta;
			start = mid + 1;
		}
		else if ( nDelta < 0 )
		{
			nCurrTolerance = -nDelta;
			end = mid - 1;
		}
		else
		{
			nClosest = end = mid;
			nClosestTolerance = 0;
			break;
		}

		if ( nCurrTolerance < nClosestTolerance )
		{
			nClosest = mid;
			nClosestTolerance = nCurrTolerance;
		}
	}

	// At this point, end is the entry less than or equal to the entry
	if ( nClosest == -1 || nTolerance.GetTenthsOfMS() < nClosestTolerance )
	{
		++end;
		nClosest = m_times.InsertBefore( end, nTime );
		m_values.InsertBefore( end, value );
		if ( IsUsingCurveTypes() )
		{
			m_CurveTypes.InsertBefore( end, curveType );
		}

		if( m_NonInterpolatedSegments.Count() > 0 )
		{
			m_NonInterpolatedSegments.InsertBefore( end, (interpSetting == SEGMENT_NOINTERPOLATE) || m_NonInterpolatedSegments[end - 1] );
			Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
		}
		else if( interpSetting == SEGMENT_NOINTERPOLATE )
		{
			//first key with non-interpolation setting. Create the whole set
			m_NonInterpolatedSegments.EnsureCount( m_times.Count() - 1 );
			m_NonInterpolatedSegments.InsertBefore( end, (interpSetting == SEGMENT_NOINTERPOLATE) );
			Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
		}
	}
	return nClosest;
}


//-----------------------------------------------------------------------------
// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
//-----------------------------------------------------------------------------
template < class T >
int CDmeTypedLogLayer< T >::InsertKey( DmeTime_t nTime, const T& value, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*=CURVE_DEFAULT*/, bool bIgnoreTolerance /*= false*/ )
{
	// Manipulating values will require decompressing data
	Decompress();

	int idx = FindOrAddKey( nTime, bIgnoreTolerance ? DmeTime_t( -1 ) : DmeTime_t( 0 ), value, interpSetting );
	m_times .Set( idx, nTime );
	m_values.Set( idx, value );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.Set( idx, curveType );
	}

	if( interpSetting == SEGMENT_NOINTERPOLATE )
	{
		if( m_NonInterpolatedSegments.Count() > 0 )
		{
			m_NonInterpolatedSegments.Set( idx, (interpSetting == SEGMENT_NOINTERPOLATE) );
			Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
		}
		else if( interpSetting == SEGMENT_NOINTERPOLATE )
		{
			//first key with non-interpolation setting. Create the whole set
			m_NonInterpolatedSegments.EnsureCount( m_times.Count() );
			m_NonInterpolatedSegments.Set( idx, (interpSetting == SEGMENT_NOINTERPOLATE) );
			Assert( m_NonInterpolatedSegments.Count() == m_times.Count() );
		}
	}

	return idx;
}

template< class T >
int CDmeTypedLogLayer< T >::InsertKeyAtTime( DmeTime_t nTime, int curveType /*=CURVE_DEFAULT*/ )
{
	// Manipulating values will require decompressing data
	Decompress();

	T curVal = GetValue( nTime );
	return InsertKey( nTime, curVal, GetSegmentInterpolationSetting(nTime), curveType );
}

//-----------------------------------------------------------------------------
// Add keys at tStartTime and tEndTime, and remove all keys outside the range
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::TrimKeys( DmeTime_t tStartTime, DmeTime_t tEndTime )
{
	// Manipulating values will require decompressing data
	Decompress();

	int idx = FindKey( tStartTime ); // the key at or before tStartTime
	if ( idx >= 0 )
	{
		if ( m_times[ idx ] != tStartTime )
		{
			const T &value = GetValue( tStartTime ); // reference here only good until next GetValue()
			m_times .Set( idx, tStartTime );
			m_values.Set( idx, value );
		}
		RemoveKey( 0, idx );
	}

	int nKeys = m_times.Count();
	idx = FindKey( tEndTime );
	if ( idx >= 0 && idx + 1 < nKeys )
	{
		if ( m_times[ idx ] != tEndTime )
		{
			++idx; // the key at or after tEndTime
			const T &value = GetValue( tEndTime ); // reference here only good until next GetValue()
			m_times .Set( idx, tEndTime );
			m_values.Set( idx, value );
		}
		RemoveKey( idx + 1, nKeys - idx - 1 );
	}
}

static bool CanInterpolateType( DmAttributeType_t attType )
{
	switch ( attType )
	{
	default:
		return false;
	case AT_FLOAT:
	case AT_VECTOR3:
	case AT_QUATERNION:
	case AT_TIME:
		break;
	}
	return true;
}

template< class T >
const T& CDmeTypedLogLayer< T >::GetValue( DmeTime_t time ) const
{
	// Curve Interpolation only for 1-D float data right now!!!
	if ( IsUsingCurveTypes() && 
		CanInterpolateType( GetDataType() ) )
	{
		static T out;
		GetValueUsingCurveInfo( time, out );
		return out;
	}

	int tc = m_times.Count();

	Assert( IsCompressed() || m_values.Count() == tc );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == tc ) );

	int ti = FindKey( time );
	if ( ti < 0 )
	{
		if ( tc > 0 )
			return GetKeyValue( 0 );

		const CDmeTypedLog< T > *pOwner = GetTypedOwnerLog();
		if ( pOwner->HasDefaultValue() )
			return pOwner->GetDefaultValue();

		static T s_value;
		CDmAttributeInfo< T >::SetDefaultValue( s_value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		return s_value;
	}

	// Early out if we're at the end
	if ( ti >= tc - 1 )
		return GetKeyValue( ti );

	if( GetSegmentInterpolationSetting( ti ) == SEGMENT_NOINTERPOLATE )
		return GetKeyValue( ti );

	if ( !IsInterpolableType( GetDataType() ) )
		return GetKeyValue( ti );

	// Figure out the lerp factor
	float t = GetFractionOfTimeBetween( time, m_times[ti], m_times[ti+1] );
	static T s_value;
	T v1, v2;
	GetTwoKeyValues( ti, v1, v2 );
	s_value = Interpolate( t, v1, v2 );	// Compute the lerp between ti and ti+1
	return s_value;
}

template< class T >
void CDmeTypedLogLayer< T >::SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*= CURVE_DEFAULT*/ )
{
	DmAttributeType_t type = ( pAttr ? pAttr->GetType() : AT_UNKNOWN );

	if ( IsValueType( type ) )
	{
		Assert( pAttr->GetType() == GetDataType() );
		SetKey( time, pAttr->GetValue< T >(), interpSetting, curveType );
	}
	else if ( IsArrayType( type ) )
	{
		Assert( ArrayTypeToValueType( type ) == GetDataType() );
		CDmrArrayConst<T> array( pAttr );
		SetKey( time, array[ index ], interpSetting, curveType );
	}
	else
	{
		Assert( 0 );
	}
}

template< class T >
bool CDmeTypedLogLayer< T >::SetDuplicateKeyAtTime( DmeTime_t time )
{
	Decompress();

	int nKeys = m_times.Count();
	if ( nKeys == 0 || m_times[ nKeys - 1 ] == time )
		return false;

	T value = GetValue( time );
	SegmentInterpolation_t interpSetting = GetSegmentInterpolationSetting( time );
	// these two calls need to be separated (and we need to make an extra copy here) because
	// CUtlVector has an assert to try to safeguard against inserting an existing value
	// therefore, m_values.AddToTail( m_values[ i ] ) is illegal (or at least, triggers the assert)
	SetKey( time, value, interpSetting );
	return true;
}

//-----------------------------------------------------------------------------
// Returns the key time / value pair for the specified key
//-----------------------------------------------------------------------------
template < class T >
void CDmeTypedLogLayer< T >::GetKeyValue( int nKeyIndex, LogKeyValue_t< T > &keyValue ) const
{
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	keyValue.time = m_times[ nKeyIndex ];
	keyValue.value = GetKeyValue( nKeyIndex );
}

template< class T >
void CalcDecompressedAnimation( const compressed_stream_t &stream, int iFrame, T &out )
{
	Assert( 0 );
}

template<>
void CalcDecompressedAnimation( const compressed_stream_t &stream, int iFrame, Vector &out  )
{
	ExtractAnimValue( iFrame, stream.Get( 0 ), stream.m_vecScale[0], out.x );
	ExtractAnimValue( iFrame, stream.Get( 1 ), stream.m_vecScale[1], out.y );
	ExtractAnimValue( iFrame, stream.Get( 2 ), stream.m_vecScale[2], out.z );
	out += stream.m_vecBaseValue;
}

template<>
void CalcDecompressedAnimation( const compressed_stream_t &stream, int iFrame, Quaternion &out )
{
	RadianEuler			angle;
	ExtractAnimValue( iFrame, stream.Get( 0 ), stream.m_vecScale[0], angle.x );
	ExtractAnimValue( iFrame, stream.Get( 1 ), stream.m_vecScale[1], angle.y );
	ExtractAnimValue( iFrame, stream.Get( 2 ), stream.m_vecScale[2], angle.z );

	for ( int i = 0 ;i < 3; ++i )
	{
		angle[ i ] += stream.m_vecBaseValue[ i ];
	}

	AngleQuaternion( angle, out );
}

template< class T >
void CalcDecompressedAnimations( const compressed_stream_t &stream, int iFrame, T &out1, T &out2 )
{
	Assert( 0 );
}

template<>
void CalcDecompressedAnimations( const compressed_stream_t &stream, int iFrame, Vector &out1, Vector &out2  )
{
	ExtractAnimValue( iFrame, stream.Get( 0 ), stream.m_vecScale[0], out1.x, out2.x );
	ExtractAnimValue( iFrame, stream.Get( 1 ), stream.m_vecScale[1], out1.y, out2.y );
	ExtractAnimValue( iFrame, stream.Get( 2 ), stream.m_vecScale[2], out1.z, out2.z );
	out1 += stream.m_vecBaseValue;
	out2 += stream.m_vecBaseValue;
}

template<>
void CalcDecompressedAnimations( const compressed_stream_t &stream, int iFrame, Quaternion &out1, Quaternion &out2 )
{
	RadianEuler			angle1, angle2;
	ExtractAnimValue( iFrame, stream.Get( 0 ), stream.m_vecScale[0], angle1.x, angle2.x );
	ExtractAnimValue( iFrame, stream.Get( 1 ), stream.m_vecScale[1], angle1.y, angle2.y );
	ExtractAnimValue( iFrame, stream.Get( 2 ), stream.m_vecScale[2], angle1.z, angle2.z );

	for ( int i = 0 ;i < 3; ++i )
	{
		angle1[ i ] += stream.m_vecBaseValue[ i ];
		angle2[ i ] += stream.m_vecBaseValue[ i ];
	}

	AngleQuaternion( angle1, out1 );
	AngleQuaternion( angle2, out2 );
}

template< class T >
void CDmeTypedLogLayer< T >::GetCompressedValue( int nKeyIndex, T &value ) const
{
	const compressed_stream_t *pStream = ( const compressed_stream_t * )m_Compressed->Get();
	Assert( pStream );

	CalcDecompressedAnimation( *pStream, nKeyIndex, value );
}

template< class T >
void CDmeTypedLogLayer< T >::GetCompressedValues( int nKeyIndex, T &value1, T &value2 )	const
{
	const compressed_stream_t *pStream = ( const compressed_stream_t * )m_Compressed->Get();
	Assert( pStream );

	CalcDecompressedAnimations( *pStream, nKeyIndex, value1, value2 );
}

//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
template< class T >
const T& CDmeTypedLogLayer< T >::GetKeyValue( int nKeyIndex ) const
{
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	if ( !IsCompressed() )
	{
		return m_values[ nKeyIndex ];
	}

	// Get compressed value
	static T value;
	GetCompressedValue( nKeyIndex, value );
	return value;
}

template< class T >
void CDmeTypedLogLayer< T >::GetTwoKeyValues( int keyindex, T &v1, T &v2 ) const
{
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	if ( !IsCompressed() )
	{
		v1 = m_values[ keyindex ];
		v2 = m_values[ keyindex + 1 ];
		return;
	}

	// Get compressed value
	GetCompressedValues( keyindex, v1, v2 );
}
 
template< class T >
void CDmeTypedLogLayer< T >::GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index ) const
{
	DmAttributeType_t attrtype = pAttr->GetType();
	if ( IsValueType( attrtype ) )
	{
		Assert( attrtype == GetDataType() );
		pAttr->SetValue( GetValue( time ) );
	}
	else if ( IsArrayType( attrtype ) )
	{
		Assert( ArrayTypeToValueType( attrtype ) == GetDataType() );
		CDmrArray<T> array( pAttr );
		array.Set( index, GetValue( time ) );
	}
	else
	{
		Assert( 0 );
	}
}

template< class T >
float CDmeTypedLogLayer< T >::GetComponent( DmeTime_t time, int componentIndex ) const
{
	return ::GetComponent( GetValue( time ), componentIndex );
}

template< class T >
void CDmeTypedLogLayer< T >::SetKeyValue( int nKey, const T& value )
{
	Decompress();

	Assert( nKey >= 0 );
	Assert( nKey < m_values.Count() );

	m_values.Set( nKey,  value );
}


//-----------------------------------------------------------------------------
// resampling and filtering
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::Resample( DmeFramerate_t samplerate )
{
	Decompress();

	// FIXME:  Might have to revisit how to determine "curve types" for "resampled points...
	Assert( !IsUsingCurveTypes() );

	// make sure we resample to include _at_least_ the existing time range
	DmeTime_t begin = GetBeginTime( false );
	DmeTime_t end = GetEndTime( false );
	int nSamples = 2 + FrameForTime( end - begin, samplerate );

	CUtlVector< DmeTime_t > resampledTimes;
	CUtlVector< T > resampledValues;
	CUtlVector< int > resampledCurveTypes;
	CUtlVector< bool > resampledNoInterpSegments;

	resampledValues.EnsureCapacity( nSamples );
	resampledTimes.EnsureCapacity( nSamples );
	bool bHasNonInterpSegments = (m_NonInterpolatedSegments.Count() > 0);
	if( bHasNonInterpSegments )
	{
		resampledNoInterpSegments.EnsureCapacity( nSamples );
	}

	DmeTime_t time( begin );
	DmeTime_t lastInterpSampleTime = time;
	for ( int i = 0; i < nSamples; ++i )
	{
		resampledTimes.AddToTail( time );
		resampledValues.AddToTail( GetValue( time ) );
		if ( IsUsingCurveTypes() )
		{
			resampledCurveTypes.AddToTail( CURVE_DEFAULT );
		}

		time = time.TimeAtNextFrame( samplerate );

		if ( bHasNonInterpSegments )
		{
			//disable interpolation if we're about to pass through a non-interpolated segment.
			//non-interpolated segments keep their beginning value for the entire segment, so they must be front-loaded
			resampledNoInterpSegments.AddToTail( (GetSegmentInterpolationSetting( lastInterpSampleTime, time, true ) == SEGMENT_NOINTERPOLATE) );
			lastInterpSampleTime = time;
		}
	}

	m_times.SwapArray( resampledTimes );
	m_values.SwapArray( resampledValues );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.SwapArray( resampledCurveTypes );
	}

	if( bHasNonInterpSegments )
	{
		m_NonInterpolatedSegments.SwapArray( resampledNoInterpSegments );
	}
}

template< class T >
void CDmeTypedLogLayer< T >::Filter( int nSampleRadius )
{
	Decompress();

	// Doesn't mess with curvetypes!!!

	const CUtlVector< T > &values = m_values.Get();
	CUtlVector< T > filteredValues;

	int nValues = values.Count();
	filteredValues.EnsureCapacity( nValues );

	for ( int i = 0; i < nValues; ++i )
	{
		int nSamples = MIN( nSampleRadius, MIN( i, nValues - i - 1 ) );
		filteredValues.AddToTail( Average( values.Base() + i - nSamples, 2 * nSamples + 1 ) );
	}

	m_values.SwapArray( filteredValues );
}

template< class T >
void CDmeTypedLogLayer< T >::Filter2( DmeTime_t sampleRadius )
{
	Decompress();

	// Doesn't mess with curvetypes!!!

	const CUtlVector< T > &values = m_values.Get();
	CUtlVector< T > filteredValues;

	int nValues = values.Count();
	filteredValues.EnsureCapacity( nValues );

	DmeTime_t earliest = DMETIME_ZERO;
	if ( nValues > 0 )
	{
		earliest = m_times[ 0 ];
	}
	for ( int i = 0; i < nValues; ++i )
	{
		T vals[ 3 ];
		DmeTime_t t = GetKeyTime( i );
		DmeTime_t t0 = t - sampleRadius;
		DmeTime_t t1 = t + sampleRadius;

		if ( t0 >= earliest )
		{
			vals[ 0 ] = GetValue( t0 );
		}
		else
		{
			vals[ 0 ] = m_values[ 0 ];
		}
		vals[ 1 ] = GetValue( t );
		vals[ 2 ] = GetValue( t1 );

		if ( i == 0 || i == nValues - 1 )
		{
			filteredValues.AddToTail( values[ i ] );
		}
		else
		{
			filteredValues.AddToTail( Average( vals, 3 ) );
		}
	}

	m_values.SwapArray( filteredValues );
}

template< class T >
const T& CDmeTypedLogLayer< T >::GetValueSkippingKey( int nKeyToSkip ) const
{
	// Curve Interpolation only for 1-D float data right now!!!
	if ( IsUsingCurveTypes() && CanInterpolateType( GetDataType() ) )
	{
		static T out;
		GetValueUsingCurveInfoSkippingKey( nKeyToSkip, out );
		return out;
	}

	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	DmeTime_t time = GetKeyTime( nKeyToSkip );

	int prevKey = nKeyToSkip - 1;
	int nextKey = nKeyToSkip  + 1;

	DmeTime_t prevTime;
	T prevValue;
	int prevCurveType;
	DmeTime_t nextTime;
	T nextValue;
	int nextCurveType;

    GetBoundedSample( prevKey, prevTime, prevValue, prevCurveType );
    GetBoundedSample( nextKey, nextTime, nextValue, nextCurveType );

	// Figure out the lerp factor
	float t = GetFractionOfTimeBetween( time, prevTime, nextTime );

	static T s_value;
	if( (GetSegmentInterpolationSetting(prevKey) == SEGMENT_NOINTERPOLATE) || 
		(GetSegmentInterpolationSetting(nKeyToSkip) == SEGMENT_NOINTERPOLATE) )
	{
		s_value = prevValue;
	}
	else
	{
		s_value = Interpolate( t, prevValue, nextValue );
	}
	return s_value;
}

template< class T >
void CDmeTypedLog<T>::RemoveRedundantKeys( float threshold, bool bKeepEnds )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveRedundantKeys( threshold, bKeepEnds );
}

template< class T >
void CDmeTypedLogLayer<T>::RemoveRedundantKeys( float threshold, bool bKeepEnds )
{
	Decompress();

	Assert( GetTypedOwnerLog() );
	if ( !GetTypedOwnerLog() )
		return;

	float saveThreshold = CDmeTypedLog< T >::GetValueThreshold();
	CDmeTypedLog< T >::SetValueThreshold( threshold );

	RemoveRedundantKeys( bKeepEnds );

	CDmeTypedLog< T >::SetValueThreshold( saveThreshold );
}

// Implementation of Douglas-Peucker curve simplification routine (hacked to only care about error against original curve (sort of 1D)

template< class T >
void CDmeTypedLogLayer< T >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< T > *output )
{
	if ( endPoint <= startPoint + 1 )
	{
		return;
	}

	int maxPoint = startPoint;
	float maxDistanceSqr = 0.0f;
	bool bAnyNonInterp = false;

	for ( int i = startPoint + 1 ; i < endPoint; ++i )
	{
		DmeTime_t keyTime = GetKeyTime( i );
        T check = GetKeyValue( i );
		T check2 = output->GetValue( keyTime );
		T dist = Subtract( check, check2 );
		float distSqr = LengthOf( dist ) * LengthOf( dist );
		bAnyNonInterp |= (GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE);

		if ( distSqr < maxDistanceSqr )
			continue;

		maxPoint = i;
		maxDistanceSqr = distSqr;
	}

	if ( maxDistanceSqr > thresholdSqr )
	{
		output->InsertKey( GetKeyTime( maxPoint ), GetKeyValue( maxPoint ), bAnyNonInterp ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE);
		CurveSimplify_R( thresholdSqr, startPoint, maxPoint, output );
		CurveSimplify_R( thresholdSqr, maxPoint, endPoint, output );
	}
}

template<> void CDmeTypedLogLayer< bool >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< bool > *output ) {};
template<> void CDmeTypedLogLayer< int >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< int > *output ) {};
template<> void CDmeTypedLogLayer< Color >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< Color > *output ) {};
template<> void CDmeTypedLogLayer< Quaternion >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< Quaternion > *output ) {};
template<> void CDmeTypedLogLayer< VMatrix >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< VMatrix > *output ) {};

// We can't just walk the keys linearly since it'll accumulate too much error and give us a bad curve after simplification.  We do a recursive subdivide which has a worst case of O(n^2) but
//  probably is better than that in most cases.
template< class T >
void CDmeTypedLogLayer<T>::RemoveRedundantKeys( bool bKeepEnds )
{
	CDmeTypedLog< T > *pOwner = GetTypedOwnerLog();
	if ( !pOwner )
		return;

	int nKeys = GetKeyCount();
	if ( nKeys <= 2 )
		return;

	float thresh =  pOwner->GetValueThreshold();
	if ( thresh < 0.0f )
		return;

	Decompress();

	CDmeTypedLogLayer< T > *save = 0;
	{
		CDisableUndoScopeGuard guard;
		save = CastElement< CDmeTypedLogLayer< T > >( CreateLayer< T >( pOwner ) );
		Assert( save );
		bool bHasNonInterpSegments = (m_NonInterpolatedSegments.Count() > 0);

		save->m_bLeftInfinite = m_bLeftInfinite;
		save->m_bRightInfinite = m_bRightInfinite;
		save->m_times.EnsureCapacity( nKeys );
		save->m_values.EnsureCapacity( nKeys );
		if( bHasNonInterpSegments )
		{
			save->m_NonInterpolatedSegments.EnsureCapacity( nKeys );
		}

		// Insert start and end points as first "guess" at simplified curve. Skip 
		// preceding and ending keys that have the same value, unless the bKeepFirstLast 
		// flag is true, in which case the first and last key are always added.
		int nFirstKey = 0;
		int nLastKey = nKeys - 1;
		if ( !bKeepEnds )
		{
			for ( nFirstKey = 1; nFirstKey < nKeys; ++nFirstKey )
			{
				// FIXME: Should we use a tolerance check here?
				if ( GetKeyValue( nFirstKey ) != GetKeyValue( nFirstKey - 1 ) )
					break;
			}
			--nFirstKey;

			for ( nLastKey = nKeys; --nLastKey >= 1; )
			{
				// FIXME: Should we use a tolerance check here?
				if ( GetKeyValue( nLastKey ) != GetKeyValue( nLastKey - 1 ) )
					break;

				if( GetSegmentInterpolationSetting( nLastKey ) != GetSegmentInterpolationSetting( nLastKey - 1 ) )
					break;
			}
		}

		if ( nLastKey <= nFirstKey )
		{
			save->InsertKey( GetKeyTime( 0 ), GetKeyValue( 0 ), GetSegmentInterpolationSetting( 0 ) );
		}
		else
		{
			if ( GetDataType() == AT_FLOAT )
			{
				save->InsertKey( GetKeyTime( nFirstKey ), GetKeyValue( nFirstKey ), GetSegmentInterpolationSetting( nFirstKey ) );
				save->InsertKey( GetKeyTime( nLastKey ), GetKeyValue( nLastKey ), GetSegmentInterpolationSetting( nLastKey ) );

				// Recursively finds the point with the largest error from the "simplified curve" and subdivides the problem on both sides until the largest delta from the simplified
				//  curve is less than the tolerance (squared)
				CurveSimplify_R( thresh * thresh, nFirstKey, nLastKey, save );
			}
			else
			{
				save->InsertKey( GetKeyTime( nFirstKey ), GetKeyValue( nFirstKey ), GetSegmentInterpolationSetting( nFirstKey ) );

				// copy over keys that differ from their prior or next keys - this keeps the first and last key of a run of same-valued keys
				for ( int i = nFirstKey + 1; i < nLastKey; ++i )
				{
					// prev is from the saved log to allow deleting runs of same-valued keys
					const T &prev = save->GetKeyValue( save->GetKeyCount() - 1 ); 
					const T &curr = GetKeyValue( i );
					const T &next = GetKeyValue( i + 1  );
					if ( pOwner->ValuesDiffer( prev, curr ) || pOwner->ValuesDiffer( curr, next ) )
					{
						save->InsertKey( GetKeyTime( i ), curr, GetSegmentInterpolationSetting( i ) );
					}
				}

				save->InsertKey( GetKeyTime( nLastKey ), GetKeyValue( nLastKey ), GetSegmentInterpolationSetting( nLastKey ) );
			}
		}
	}

	// This operation is undoable
	CopyLayer( save );
	
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->DestroyElement( save->GetHandle() );
	}
}

// curve info helpers
template< class T >
const CDmeTypedCurveInfo< T > *CDmeTypedLogLayer<T>::GetTypedCurveInfo() const
{
	Assert( GetTypedOwnerLog() );
	return GetTypedOwnerLog()->GetTypedCurveInfo();
}

template< class T >
CDmeTypedCurveInfo< T > *CDmeTypedLogLayer<T>::GetTypedCurveInfo()
{
	Assert( GetTypedOwnerLog() );
	return GetTypedOwnerLog()->GetTypedCurveInfo();
}

template< class T >
bool CDmeTypedLogLayer< T >::IsUsingEdgeInfo() const
{
	return GetTypedOwnerLog()->IsUsingEdgeInfo();
}

template< class T >
const T& CDmeTypedLogLayer< T >::GetDefaultEdgeZeroValue() const
{
	return GetTypedOwnerLog()->GetDefaultEdgeZeroValue();
}

template< class T >
DmeTime_t CDmeTypedLogLayer< T >::GetRightEdgeTime() const
{
	return GetTypedOwnerLog()->GetRightEdgeTime();
}


template< class T >
void CDmeTypedLogLayer< T >::GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const
{
	GetTypedOwnerLog()->GetEdgeInfo( edge, active, val, curveType );
}

template< class T >
int CDmeTypedLogLayer< T >::GetEdgeCurveType( int edge ) const
{
	return GetTypedOwnerLog()->GetEdgeCurveType( edge );
}

template< class T >
void CDmeTypedLogLayer< T >::GetZeroValue( int side, T& val ) const
{
	return GetTypedOwnerLog()->GetZeroValue( side, val );
}

template< class T >
void CDmeTypedLogLayer< T >::GetBoundedSample( int keyindex, DmeTime_t& time, T& val, int& curveType ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		time = DmeTime_t( 0 );
		CDmAttributeInfo< T >::SetDefaultValue( val );
		curveType = CURVE_DEFAULT;
		return;
	}

	if ( keyindex < 0 )
	{
		time = DmeTime_t( 0 );
		GetZeroValue( 0, val );
		curveType = GetEdgeCurveType( 0 );
		return;
	}
	else if ( keyindex >= m_times.Count() )
	{
		time = GetTypedOwnerLog()->GetRightEdgeTime();
		if ( time == DmeTime_t( 0 ) && m_times.Count() > 0 )
		{
			// Push it one tms past the final end time
			time = m_times[ m_times.Count() - 1 ] + DMETIME_MINDELTA;
		}
		GetTypedOwnerLog()->GetZeroValue( 1, val );
		curveType = GetTypedOwnerLog()->GetEdgeCurveType( 1 );
		return;
	}

	time = m_times[ keyindex ];
	val = GetKeyValue( keyindex );
	if ( IsUsingCurveTypes() )
	{
		Assert( m_CurveTypes.Count() == m_times.Count() );
		if ( keyindex >= m_CurveTypes.Count() )
		{
			curveType = GetTypedOwnerLog()->GetDefaultCurveType();
		}
		else
		{
			curveType = m_CurveTypes[ keyindex ];
			if ( curveType == CURVE_DEFAULT )
			{
				curveType = GetTypedOwnerLog()->GetDefaultCurveType();
			}
		}
	}
}

template<>
void CDmeTypedLogLayer< float >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, float& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		out = 0.0f;
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	float v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = nKeyToSkip;
	DmeTime_t time = GetKeyTime( nKeyToSkip );

	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< float >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti + 1 );
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			GetZeroValue( 1, out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti + 1 );
			return;
		}
	}

	GetBoundedSample( ti - 2, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti - 1, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, Vector& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Vector >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Vector v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = nKeyToSkip;
	DmeTime_t time = GetKeyTime( nKeyToSkip );

	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti + 1 );
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti + 1 );
			return;
		}
	}

	GetBoundedSample( ti - 2, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti - 1, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, Quaternion& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Quaternion v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = nKeyToSkip;
	DmeTime_t time = GetKeyTime( nKeyToSkip );

	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti + 1 );
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti + 1 );
			return;
		}
	}

	GetBoundedSample( ti - 2, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti - 1, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}


template<>
void CDmeTypedLogLayer< float >::GetValueUsingCurveInfo( DmeTime_t time, float& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		out = 0.0f;
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	float v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = FindKey( time );
	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< float >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti );
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			GetZeroValue( 1, out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti );
			return;
		}
	}

	GetBoundedSample( ti - 1, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti + 0, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfo( DmeTime_t time, Vector& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Vector >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Vector v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = FindKey( time );
	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti );
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti );
			return;
		}
	}

	GetBoundedSample( ti - 1, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti + 0, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfo( DmeTime_t time, Quaternion& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( IsCompressed() || m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Quaternion v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = FindKey( time );
	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti );
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = GetKeyValue( ti );
			return;
		}
	}

	GetBoundedSample( ti - 1, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti + 0, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template< class T >
void CDmeTypedLogLayer< T >::CopyLayer( const CDmeLogLayer *src )
{
	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	m_times = pSrc->m_times;
	m_lastKey = pSrc->m_lastKey;
	m_bLeftInfinite = pSrc->m_bLeftInfinite;
	m_bRightInfinite = pSrc->m_bRightInfinite;
	m_values = pSrc->m_values;
	m_CurveTypes = pSrc->m_CurveTypes;
	m_Compressed = pSrc->m_Compressed;
	m_NonInterpolatedSegments = pSrc->m_NonInterpolatedSegments;
}

template< class T >
void CDmeTypedLogLayer< T >::InsertKeyFromLayer( DmeTime_t keyTime, const CDmeLogLayer *src, DmeTime_t srcKeyTime )
{
	Decompress();

	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	Assert( pSrc );

	// NOTE: This copy is necessary if src == this
	T value = pSrc->GetValue( srcKeyTime );
	InsertKey( keyTime, value, pSrc->GetSegmentInterpolationSetting(keyTime) );
}

template< class T >
void CDmeTypedLogLayer< T >::ExplodeLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps, DmeTime_t tResampleInterval )
{
	Decompress();

	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	Assert( pSrc );

	DmeTime_t tTimeOffset = DMETIME_ZERO;
	if ( bRebaseTimestamps )
	{
		tTimeOffset = -startTime;
	}

	m_times.RemoveAll();
	m_values.RemoveAll();
	m_CurveTypes.RemoveAll();

	bool usecurvetypes = pSrc->IsUsingCurveTypes();

	// Now copy the data for the layer
	for ( DmeTime_t t = startTime ; t + tResampleInterval < endTime; t += tResampleInterval )
	{
		DmeTime_t keyTime = DmeTime_t( t );
		if ( keyTime > endTime )
		{
			keyTime = endTime;
		}

		T val = pSrc->GetValue( keyTime );

		DmeTime_t nextTime = keyTime + tResampleInterval;
		nextTime = MIN( nextTime, endTime );
		SegmentInterpolation_t interpSetting = pSrc->GetSegmentInterpolationSetting( keyTime, nextTime, true );

		keyTime += tTimeOffset;

		InsertKey( keyTime, val, interpSetting, usecurvetypes ? GetDefaultCurveType() : CURVE_DEFAULT );
	}

	m_lastKey = m_times.Count() - 1;
}

template< class T >
void CDmeTypedLogLayer< T >::CopyPartialLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps )
{
	Decompress();

	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	Assert( pSrc );

	DmeTime_t nTimeOffset = DMETIME_ZERO;
	if ( bRebaseTimestamps )
	{
		nTimeOffset = -startTime;
	}

	m_times.RemoveAll();
	m_values.RemoveAll();
	m_CurveTypes.RemoveAll();
	m_NonInterpolatedSegments.RemoveAll();


	bool usecurvetypes = pSrc->IsUsingCurveTypes();
	bool bHasNonInterpolatedSegments = (pSrc->m_NonInterpolatedSegments.Count() > 0);

	// Now copy the data for the later
	int c = pSrc->m_times.Count();
	for ( int i = 0; i < c; ++i )
	{
		DmeTime_t keyTime = pSrc->m_times[ i ];
		if ( keyTime < startTime || keyTime > endTime )
			continue;

		m_times.AddToTail( pSrc->m_times[ i ] + nTimeOffset );
		m_values.AddToTail( pSrc->GetKeyValue( i ) );
		if ( usecurvetypes )
		{
			m_CurveTypes.AddToTail( pSrc->m_CurveTypes[ i ] );
		}

		if( bHasNonInterpolatedSegments )
		{
			m_NonInterpolatedSegments.AddToTail( pSrc->m_NonInterpolatedSegments[i] );
		}
	}

	m_lastKey = m_times.Count() - 1;
}


//-----------------------------------------------------------------------------
// Purpose: Mask the specified value with the provided component flags, if a 
// component is masked out then the original value of that component at the 
// specified time will be used.
//-----------------------------------------------------------------------------
template<>
Vector CDmeTypedLogLayer< Vector >::MaskValue( DmeTime_t time, const Vector& value, LogComponents_t componentFlags ) const
{
	Vector writeValue = value;
	if ( ( componentFlags & LOG_COMPONENTS_ALL ) != LOG_COMPONENTS_ALL )
	{
		Vector curVal = GetValue( time );
		writeValue = ::MaskValue( value, curVal, componentFlags );
	}

	return writeValue;
}

template<>
Quaternion CDmeTypedLogLayer< Quaternion >::MaskValue( DmeTime_t time, const Quaternion& value, LogComponents_t componentFlags ) const
{
	Quaternion writeValue = value;
	if ( ( componentFlags & LOG_COMPONENTS_ALL ) != LOG_COMPONENTS_ALL )
	{
		Quaternion curQuat = GetValue( time );
		writeValue = ::MaskValue( value, curQuat, componentFlags );
	}

	return writeValue;
}

template< class T >
T CDmeTypedLogLayer< T >::MaskValue( DmeTime_t time, const T& value, LogComponents_t componentFlags )	const
{
	return value;
}

template< class T >
// Masks all keys within the time range, returns true if keys were modified
bool CDmeTypedLogLayer< T >::MaskKeyRange( DmeTime_t tStartTime, DmeTime_t tEndTime, LogComponents_t nComponentFlags, bool bInfiniteLeft /*= false*/, bool bInfiniteRight /*= false*/ )
{
	bool bRet = false;

	T startVal = GetValue( tStartTime );
	T endVal = GetValue( tEndTime );

	if ( bInfiniteRight )
	{
		// we'll want to "hold" the masked start value through the region
		endVal = ::MaskValue( startVal, endVal, nComponentFlags );
	}
	else if ( bInfiniteLeft )
	{
		// we'll want to hold the "end" value backwards throug
		startVal = ::MaskValue( endVal, startVal, nComponentFlags );
	}

	DmeTime_t dt = tEndTime - tStartTime;
	if ( dt <= DMETIME_ZERO )
		return bRet;

	int nKeyCount = GetKeyCount();
	for ( int nKey = 0; nKey < nKeyCount; ++nKey )
	{
		DmeTime_t tKeyTime = GetKeyTime( nKey );
		if ( tKeyTime < tStartTime )
			continue;
		if ( tKeyTime > tEndTime )
			break;

		bRet = true;
		T value = GetKeyValue( nKey );
		float frac = ( tKeyTime - tStartTime ) / ( dt );
		T maskedValue = Interpolate< T >( frac, startVal, endVal );
		T newVal = ::MaskValue( maskedValue, value, nComponentFlags );
		SetKeyValue( nKey, newVal );
	}
	return bRet;
}

template< class T >
void CDmeTypedLogLayer< T >::MakeRoomForSamplesMaskedSubcomponents( CDmeLogLayer *pBaseLayer, DmeTime_t tStart, DmeTime_t tEnd, DmeTime_t tLeftShift, DmeTime_t tRightShift, LogComponents_t nComponents )
{
	CDmeTypedLogLayer< T > *pBase = ( CDmeTypedLogLayer< T > * )pBaseLayer;

	T startVal	= pBase->GetValue( tStart );
	T endVal	= pBase->GetValue( tEnd );

	DmeTime_t tNewStart = tStart + tLeftShift;
	DmeTime_t tNewEnd = tEnd + tRightShift;

	//I can't quite follow exactly how these shifts end up. Taking the safe route of propogating non-interpolated segements to the new layer
	SegmentInterpolation_t interpSetting = pBase->GetSegmentInterpolationSetting( MIN( tStart, tNewStart ), MAX( tEnd, tNewEnd ), false );
	bool bInterpEndpoints = pBase->GetSegmentInterpolationSetting( tStart, tEnd, false ) == SEGMENT_INTERPOLATE;

	int keyCount = pBase->GetKeyCount();
	for ( int key = 0; key < keyCount; ++key )
	{
		DmeTime_t keyTime = pBase->GetKeyTime( key );
		DmeTime_t newKeyTime = keyTime;

		T origValue = pBase->GetKeyValue( key );

		if ( keyTime <= tStart )
		{
			newKeyTime += tLeftShift;
		}
		else if ( keyTime >= tEnd )
		{
			newKeyTime += tRightShift;
		}
		else
		{
			if ( keyTime <= tNewStart || keyTime >= tNewEnd )
			{
				DmeTime_t oppositeKeyTime = ( keyTime <= tNewStart ) ?
					tStart + ( keyTime - tNewStart ) :
				    tEnd + ( keyTime - tNewEnd );

				T xyVal = pBase->GetValue( oppositeKeyTime );

				T unshiftedValue = ::MaskValue( xyVal, origValue, nComponents );
				InsertKey( keyTime, unshiftedValue, interpSetting );
			}
			else
			{
				float frac = GetFractionOfTimeBetween( keyTime, tNewStart, tNewEnd );
				T clearedValue = bInterpEndpoints ? Interpolate( frac, startVal, endVal ) : startVal;
				T val = ::MaskValue( clearedValue, origValue, nComponents );
				InsertKey( keyTime, val, interpSetting ); 
			}
			continue;
		}

		if ( newKeyTime != keyTime )
		{
			// xy comes from unshifted position
			T newValue = pBase->GetValue( newKeyTime );
			// z preserved from new key time
			T shiftedValue = ::MaskValue( origValue, newValue, nComponents );
			InsertKey( newKeyTime, shiftedValue, interpSetting );
			
			if ( keyTime <= ( tNewStart ) || 
				 keyTime >= ( tNewEnd ) )
			{
				DmeTime_t dt = newKeyTime - keyTime;
				DmeTime_t oppositeKeyTime = keyTime - dt;

				T xyVal = pBase->GetValue( oppositeKeyTime );

				T unshiftedValue = ::MaskValue( xyVal, origValue, nComponents );
				InsertKey( keyTime, unshiftedValue, interpSetting );
			}
			else
			{
				float frac = GetFractionOfTimeBetween( newKeyTime, tStart - tLeftShift, tEnd + tRightShift );
				T clearedValue = bInterpEndpoints ? Interpolate( frac, startVal, endVal ) : startVal;
				T val = ::MaskValue( clearedValue, origValue, nComponents );
				InsertKey( keyTime, val, interpSetting ); 
			}
		}
		else
		{
			InsertKey( keyTime, origValue, interpSetting ); 
		}
	}
}

//-----------------------------------------------------------------------------
// Creates a log of a specific type
//-----------------------------------------------------------------------------
template< class T >
CDmeLogLayer *CreateLayer< T >( CDmeTypedLog< T > *pOwnerLog )
{
	DmFileId_t fileid = pOwnerLog ? pOwnerLog->GetFileId() : DMFILEID_INVALID;
	CDmeLogLayer *layer = NULL;

	switch ( CDmAttributeInfo<T>::AttributeType() )
	{
	case AT_INT:
	case AT_INT_ARRAY:
		layer = CreateElement< CDmeIntLogLayer >( "int log", fileid );
		break;
	case AT_FLOAT:
	case AT_FLOAT_ARRAY:
		layer = CreateElement< CDmeFloatLogLayer >( "float log", fileid );
		break;
	case AT_BOOL:
	case AT_BOOL_ARRAY:
		layer = CreateElement< CDmeBoolLogLayer >( "bool log", fileid );
		break;
	case AT_COLOR:
	case AT_COLOR_ARRAY:
		layer = CreateElement< CDmeColorLogLayer >( "color log", fileid );
		break;
	case AT_VECTOR2:
	case AT_VECTOR2_ARRAY:
		layer = CreateElement< CDmeVector2LogLayer >( "vector2 log", fileid );
		break;
	case AT_VECTOR3:
	case AT_VECTOR3_ARRAY:
		layer = CreateElement< CDmeVector3LogLayer >( "vector3 log", fileid );
		break;
	case AT_VECTOR4:
	case AT_VECTOR4_ARRAY:
		layer = CreateElement< CDmeVector4LogLayer >( "vector4 log", fileid );
		break;
	case AT_QANGLE:
	case AT_QANGLE_ARRAY:
		layer = CreateElement< CDmeQAngleLogLayer >( "qangle log", fileid );
		break;
	case AT_QUATERNION:
	case AT_QUATERNION_ARRAY:
		layer = CreateElement< CDmeQuaternionLogLayer >( "quaternion log", fileid );
		break;
	case AT_VMATRIX:
	case AT_VMATRIX_ARRAY:
		layer = CreateElement< CDmeVMatrixLogLayer >( "vmatrix log", fileid );
		break;
	case AT_STRING:
	case AT_STRING_ARRAY:
		layer = CreateElement< CDmeStringLogLayer >( "string log", fileid );
		break;
	case AT_TIME:
	case AT_TIME_ARRAY:
		layer = CreateElement< CDmeTimeLogLayer >( "time log", fileid );
		break;
	}

	if ( layer )
	{
		layer->SetOwnerLog( pOwnerLog );
	}
	return layer;
}



//-----------------------------------------------------------------------------
//
// CDmeCurveInfo - abstract base class
//
//-----------------------------------------------------------------------------
void CDmeCurveInfo::OnConstruction()
{
	m_DefaultCurveType.Init( this, "defaultCurveType" );

	m_MinValue.InitAndSet( this, "minvalue", 0.0f );
	m_MaxValue.InitAndSet( this, "maxvalue", 1.0f );
}

void CDmeCurveInfo::OnDestruction()
{
}

// Global override for all keys unless overriden by specific key
void CDmeCurveInfo::SetDefaultCurveType( int curveType )
{
	m_DefaultCurveType = curveType;
}

int CDmeCurveInfo::GetDefaultCurveType() const
{
	return m_DefaultCurveType.Get();
}

void CDmeCurveInfo::SetMinValue( float val )
{
	m_MinValue = val;
}

float CDmeCurveInfo::GetMinValue() const
{
	return m_MinValue;
}

void CDmeCurveInfo::SetMaxValue( float val )
{
	m_MaxValue = val;
}

float CDmeCurveInfo::GetMaxValue() const
{
	return m_MaxValue;
}


//-----------------------------------------------------------------------------
//
// CDmeTypedCurveInfo - implementation class for all logs
//
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedCurveInfo< T >::OnConstruction()
{
	m_bUseEdgeInfo.Init( this, "useEdgeInfo" );
	m_DefaultEdgeValue.Init( this, "defaultEdgeZeroValue" );
	m_RightEdgeTime.Init( this, "rightEdgeTime" );

	for ( int i = 0; i < 2; ++i )
	{
		char edgename[ 32 ];
		Q_snprintf( edgename, sizeof( edgename ), "%s", i == 0 ? "left" : "right" );
		char name[ 32 ];
		Q_snprintf( name, sizeof( name ), "%sEdgeActive", edgename );
		m_bEdgeActive[ i ].Init( this, name );
		Q_snprintf( name, sizeof( name ), "%sEdgeValue", edgename );
		m_EdgeValue[ i ].Init( this, name );
		Q_snprintf( name, sizeof( name ), "%sEdgeCurveType", edgename );
		m_EdgeCurveType[ i ].Init( this, name );
	}
}

template< class T >
void CDmeTypedCurveInfo< T >::OnDestruction()
{
}

template< class T >
void CDmeTypedCurveInfo< T >::SetUseEdgeInfo( bool state )
{
	m_bUseEdgeInfo = state;
}

template< class T >
bool CDmeTypedCurveInfo< T >::IsUsingEdgeInfo() const
{
	return m_bUseEdgeInfo;
}

template< class T >
void CDmeTypedCurveInfo< T >::SetEdgeInfo( int edge, bool active, const T& val, int curveType )
{
	SetUseEdgeInfo( true );

	Assert( edge == 0 || edge == 1 );

	m_bEdgeActive[ edge ] = active;
	m_EdgeValue[ edge ] = val;
	m_EdgeCurveType[ edge ] = curveType;
}

template< class T >
void CDmeTypedCurveInfo< T >::SetDefaultEdgeZeroValue( const T& val )
{
	m_DefaultEdgeValue = val;
}

template< class T >
const T& CDmeTypedCurveInfo< T >::GetDefaultEdgeZeroValue() const
{
	return m_DefaultEdgeValue;
}

template< class T >
void CDmeTypedCurveInfo< T >::SetRightEdgeTime( DmeTime_t time )
{
	m_RightEdgeTime = time;
}

template< class T >
DmeTime_t CDmeTypedCurveInfo< T >::GetRightEdgeTime() const
{
	return m_RightEdgeTime;
}

template< class T >
void CDmeTypedCurveInfo< T >::GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const
{
	Assert( IsUsingEdgeInfo() );

	Assert( edge == 0 || edge == 1 );

	active = m_bEdgeActive[ edge ];
	val = m_EdgeValue[ edge ];
	curveType = m_EdgeCurveType[ edge ];
}

template< class T >
int CDmeTypedCurveInfo< T >::GetEdgeCurveType( int edge ) const
{
	Assert( edge == 0 || edge == 1 );

	if ( !m_bEdgeActive[ edge ] )
	{
		return m_DefaultCurveType;
	}

	if ( m_EdgeCurveType[ edge ] == CURVE_DEFAULT )
	{
		return m_DefaultCurveType;
	}

	return m_EdgeCurveType[ edge ];
}

template<>
void CDmeTypedCurveInfo<float>::GetZeroValue( int side, float& val ) const
{
	if ( !m_bUseEdgeInfo )
	{
		val = 0.0f;
		return;
	}

	if ( m_bEdgeActive[ side ] )
	{
		val = m_EdgeValue[ side ];
		return;
	}

	val = m_DefaultEdgeValue;
}

template<>
bool CDmeTypedCurveInfo<float>::IsEdgeActive( int edge ) const
{
	return m_bEdgeActive[ edge ];
}

template<>
void CDmeTypedCurveInfo<float>::GetEdgeValue( int edge, float& value ) const
{
	value = m_EdgeValue[ edge ];
}

template<>
void CDmeTypedCurveInfo<Vector>::GetZeroValue( int side, Vector& val ) const
{
	if ( !m_bUseEdgeInfo )
	{
		val = vec3_origin;
		return;
	}

	if ( m_bEdgeActive[ side ] )
	{
		val = m_EdgeValue[ side ];
		return;
	}

	val = m_DefaultEdgeValue;
}

template<>
void CDmeTypedCurveInfo<Quaternion>::GetZeroValue( int side, Quaternion& val ) const
{
	if ( !m_bUseEdgeInfo )
	{
		val.Init();
		return;
	}

	if ( m_bEdgeActive[ side ] )
	{
		val = m_EdgeValue[ side ];
		return;
	}

	val = m_DefaultEdgeValue;
}


//-----------------------------------------------------------------------------
//
// CDmeLog - abstract base class
//
//-----------------------------------------------------------------------------
void CDmeLog::OnConstruction()
{
	m_Layers.Init( this, "layers", FATTRIB_MUSTCOPY | FATTRIB_HAS_CALLBACK );
	m_CurveInfo.Init( this, "curveinfo", FATTRIB_MUSTCOPY | FATTRIB_HAS_CALLBACK );
}

void CDmeLog::OnDestruction()
{
}

int CDmeLog::GetTopmostLayer() const
{
	return m_Layers.Count() - 1;
}

int	CDmeLog::GetNumLayers() const
{
	return m_Layers.Count();
}

CDmeLogLayer *CDmeLog::GetLayer( int index )
{
	return m_Layers.IsValidIndex( index ) ? m_Layers[ index ] : NULL;
}

const CDmeLogLayer *CDmeLog::GetLayer( int index ) const
{
	return m_Layers.IsValidIndex( index ) ? m_Layers[ index ] : NULL;
}

bool CDmeLog::IsEmpty() const
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		if ( layer->GetKeyCount() > 0 )
			return false;
	}
	return true;
}

void CDmeLog::ScaleSampleTimes( float scale )
{
	int nLayers = m_Layers.Count();
	for ( int i = 0; i < nLayers; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		if ( !layer )
			continue;

		layer->ScaleSampleTimes( scale );
	}
}

void CDmeLog::FindLayersForTime( DmeTime_t time, CUtlVector< int >& list ) const
{
	list.RemoveAll();
	int c = m_Layers.Count();
	// The base layer is always available!!!
	if ( c > 0 )
	{
		list.AddToTail( 0 );
	}
	for ( int i = 1; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime( true );
		if ( layerStart == DmeTime_t::InvalidTime() )
			continue;
		DmeTime_t layerEnd = layer->GetEndTime( true );
		if ( layerEnd == DmeTime_t::InvalidTime() )
			continue;

		if ( time >= layerStart && time <= layerEnd )
		{
			list.AddToTail( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Find the top most layer for the specified time below the provided top layer
//-----------------------------------------------------------------------------
int CDmeLog::FindLayerForTimeBelowLayer( DmeTime_t time, int topLayerIndex ) const
{
	int c = topLayerIndex;
	for ( int i = c - 1; i >= 0; --i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime( true );
		if ( layerStart == DmeTime_t::InvalidTime() )
			continue;
		DmeTime_t layerEnd = layer->GetEndTime( true );
		if ( layerEnd == DmeTime_t::InvalidTime() )
			continue;

		if ( time >= layerStart && time <= layerEnd )
			return i;
	}
	return ( c > 0 ) ? 0 : -1;
}


int CDmeLog::FindLayerForTimeSkippingTopmost( DmeTime_t time ) const
{ 
	// This makes it never consider the topmost layer!!!
	return FindLayerForTimeBelowLayer( time, m_Layers.Count() - 1 );
}


int CDmeLog::FindLayerForTime( DmeTime_t time ) const
{
	int c = m_Layers.Count();
	for ( int i = c - 1; i >= 0; --i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime( true );
		if ( layerStart == DmeTime_t::InvalidTime() )
			continue;
		DmeTime_t layerEnd = layer->GetEndTime( true );
		if ( layerEnd == DmeTime_t::InvalidTime() )
			continue;

		if ( time >= layerStart && time <= layerEnd )
			return i;
	}
	return ( c > 0 ) ? 0 : -1;
}

DmeTime_t CDmeLog::GetBeginTime() const
{
	int c = m_Layers.Count();
	if ( c == 0 )
		return DmeTime_t::MinTime();

	DmeTime_t bestMin = DmeTime_t::MinTime();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime( false );
		if ( layerStart == DmeTime_t::InvalidTime() )
			continue;

		if ( bestMin == DmeTime_t::MinTime() )
		{
			bestMin = layerStart;
		}
		else if ( layerStart < bestMin )
		{
			bestMin = layerStart;
		}
	}

	return bestMin;
}

DmeTime_t CDmeLog::GetEndTime() const
{
	int c = m_Layers.Count();
	if ( c == 0 )
		return DmeTime_t::MaxTime();

	DmeTime_t bestMax = DmeTime_t::MaxTime();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer *layer = m_Layers[ i ];
		DmeTime_t layerEnd = layer->GetEndTime( false );
		if ( layerEnd == DmeTime_t::InvalidTime() )
			continue;

		if ( bestMax == DmeTime_t::MaxTime() )
		{
			bestMax = layerEnd;
		}
		else if ( layerEnd > bestMax )
		{
			bestMax = layerEnd;
		}
	}

	return bestMax;
}

//-----------------------------------------------------------------------------
// Returns the number of keys
//-----------------------------------------------------------------------------
int CDmeLog::GetKeyCount() const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return 0;
	return GetLayer( bestLayer )->GetKeyCount();
}


//-----------------------------------------------------------------------------
// Scale + bias key times
//-----------------------------------------------------------------------------
void CDmeLog::ScaleBiasKeyTimes( double flScale, DmeTime_t nBias )
{
	// Don't waste time on the identity transform
	if ( ( nBias == DMETIME_ZERO ) && ( fabs( flScale - 1.0 ) < 1e-5 ) )
		return;

	int nCount = GetNumLayers();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeLogLayer *pLayer = GetLayer( i );
		pLayer->ScaleBiasKeyTimes( flScale, nBias );
	}
}


//-----------------------------------------------------------------------------
// Resolve - keeps non-attribute data in sync with attribute data
//-----------------------------------------------------------------------------
void CDmeLog::Resolve()
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		layer->SetOwnerLog( this );
	}
}

void CDmeLog::OnAttributeChanged( CDmAttribute *pAttribute )
{
	if ( pAttribute == m_CurveInfo.GetAttribute() )
	{
		OnUsingCurveTypesChanged();
	}
}

void CDmeLog::OnUsingCurveTypesChanged()
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->OnUsingCurveTypesChanged();
	}
}

// curve info helpers
bool CDmeLog::IsUsingCurveTypes() const
{
	return m_CurveInfo.GetElement() != NULL;
}

const CDmeCurveInfo *CDmeLog::GetCurveInfo() const
{
	return m_CurveInfo.GetElement();
}

CDmeCurveInfo *CDmeLog::GetCurveInfo()
{
	return m_CurveInfo.GetElement();
}

// accessors for CurveInfo data
int CDmeLog::GetDefaultCurveType() const
{
	Assert( IsUsingCurveTypes() );
	return m_CurveInfo->GetDefaultCurveType();
}

// min/max accessors
float CDmeLog::GetMinValue() const
{
	Assert( IsUsingCurveTypes() );
	return m_CurveInfo->GetMinValue();
}

void CDmeLog::SetMinValue( float val )
{
	Assert( IsUsingCurveTypes() );
	m_CurveInfo->SetMinValue( val );
}

float CDmeLog::GetMaxValue() const
{
	Assert( IsUsingCurveTypes() );
	return m_CurveInfo->GetMaxValue();
}

void CDmeLog::SetMaxValue( float val )
{
	Assert( IsUsingCurveTypes() );
	m_CurveInfo->SetMaxValue( val );
}

void CDmeLog::SetKeyCurveType( int nKeyIndex, int curveType )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->SetKeyCurveType( nKeyIndex, curveType );
}

int CDmeLog::GetKeyCurveType( int nKeyIndex ) const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return CURVE_DEFAULT;

	return GetLayer( bestLayer )->GetKeyCurveType( nKeyIndex );
}


//-----------------------------------------------------------------------------
// Removes all keys in a certain time interval
//-----------------------------------------------------------------------------
bool CDmeLog::RemoveKeys( DmeTime_t tStartTime, DmeTime_t tEndTime )
{
	CDmeLogLayer *pLayer = GetLayer( GetTopmostLayer() );

	int nKeyCount = pLayer->GetKeyCount();
	int nFirstRemove = -1;
	int nLastRemove = -1;
	for ( int nKey = 0; nKey < nKeyCount; ++nKey )
	{
		DmeTime_t tKeyTime = pLayer->GetKeyTime( nKey );
		if ( tKeyTime < tStartTime )
			continue;
		if ( tKeyTime > tEndTime )
			break;
		if ( nFirstRemove == -1 )
		{
			nFirstRemove = nKey;
		}
		nLastRemove = nKey;
	}

	if ( nFirstRemove != -1 )
	{
		int nRemoveCount = nLastRemove - nFirstRemove + 1;
		pLayer->RemoveKey( nFirstRemove, nRemoveCount );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Add keys at tStartTime and tEndTime, and remove all keys outside the range
//-----------------------------------------------------------------------------
void CDmeLog::TrimKeys( DmeTime_t tStartTime, DmeTime_t tEndTime )
{
	CDmeLogLayer *pLayer = GetLayer( GetTopmostLayer() );
	if ( !pLayer )
		return;

	pLayer->TrimKeys( tStartTime, tEndTime );
}


//-----------------------------------------------------------------------------
// Get the number of bookmark times associated with the specified component of
// the log.
//-----------------------------------------------------------------------------
int	CDmeLog::GetNumBookmarks( int nComponentIndex ) const
{
	if ( nComponentIndex < GetNumBookmarkComponents() )
	{
		return m_BookmarkTimes[ nComponentIndex ].Count();
	}

	Assert( nComponentIndex < GetNumBookmarkComponents() );
	return -1;
}


//-----------------------------------------------------------------------------
// Get the time of the specified bookmark
//-----------------------------------------------------------------------------
DmeTime_t CDmeLog::GetBookmarkTime( int nBookmarkIndex, int nComponentIndex ) const
{
	if ( nComponentIndex < GetNumBookmarkComponents() )
	{
		if ( nBookmarkIndex < m_BookmarkTimes[ nComponentIndex ].Count() )	
		{
			return m_BookmarkTimes[ nComponentIndex ][ nBookmarkIndex ];
		}
		Assert( nBookmarkIndex < m_BookmarkTimes[ nComponentIndex ].Count() );
	}

	Assert( nComponentIndex < GetNumBookmarkComponents() );
	return DmeTime_t( 0 );
}


//-----------------------------------------------------------------------------
// Add a bookmark time for the specified component. Bookmarks times are stored
// in order, so the time will be inserted at the appropriate location in the 
// list.
//-----------------------------------------------------------------------------
void CDmeLog::AddBookmark( DmeTime_t time, int nComponentIndex )
{
	if ( nComponentIndex >= GetNumBookmarkComponents() )
		return;

	// Search the existing bookmarks to see if there is already one at the specified time.
	CDmaArray< DmeTime_t > &times = m_BookmarkTimes[ nComponentIndex ];
	int nBookmarks = times.Count();


	for ( int i = 0; i < nBookmarks; ++i )
	{
		if ( times[ i ] == time )
		{
			return;
		}
		else if ( times[ i ] > time )
		{
			times.InsertBefore( i, time );
			return;
		}
	}

	times.AddToTail( time );
}


//-----------------------------------------------------------------------------
// Remove the bookmark at the specified time.
//-----------------------------------------------------------------------------
bool CDmeLog::RemoveBookmark( DmeTime_t time, int nComponentIndex )
{
	if ( nComponentIndex >= GetNumBookmarkComponents() )
		return false;

	CDmaArray< DmeTime_t > &times = m_BookmarkTimes[ nComponentIndex ];
	int nBookmarks = times.Count();

	for ( int i = 0; i < nBookmarks; ++i )
	{
		if ( times[ i ] >= time )
		{
			// Remove the time entry if it matches the specified time, 
			// fast remove cannot be used because the times are sorted.
			if ( times[ i ] == time )
			{
				times.Remove( i );
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Remove all bookmarks from the log
//-----------------------------------------------------------------------------
void CDmeLog::RemoveAllBookmarks( int nComponentIndex )
{
	if ( nComponentIndex < GetNumBookmarkComponents() )
	{
		m_BookmarkTimes[ nComponentIndex ].RemoveAll();
	}
}


//-----------------------------------------------------------------------------
// Set all of the bookmark times for the log, clearing out any previous values
//-----------------------------------------------------------------------------
void CDmeLog::SetAllBookmarks( int nComponentIndex, const CUtlVector< DmeTime_t > &times )
{
	if ( nComponentIndex < GetNumBookmarkComponents() )
	{
		m_BookmarkTimes[ nComponentIndex ] = times;
	}
}


//-----------------------------------------------------------------------------
// CDmeTypedLog - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::OnConstruction()
{
	if ( !g_pDataModel->IsUnserializing() )
	{
		// Add the default layer!!!
		AddNewLayer();
		Assert( m_Layers.Count() == 1 );
	}

	m_UseDefaultValue.InitAndSet( this, "usedefaultvalue", false );
	m_DefaultValue.Init( this, "defaultvalue" );

	InitalizeBookmarkArrays();
}

template< class T >
void CDmeTypedLog< T >::OnDestruction()
{
}

template< class T >
void CDmeTypedLog< T >::ClearAndAddSampleAtTime( DmeTime_t time )
{
	const T &value = GetValue( time );
	SegmentInterpolation_t interpSetting = GetSegmentInterpolationSetting( time );
	ClearKeys();
	SetKey( time, value, interpSetting );
}


template< class T >
void CDmeTypedLog< T >::SetDefaultValue( const T& value )
{
	m_UseDefaultValue = true;
	m_DefaultValue.Set( value );
}

template< class T >
const T& CDmeTypedLog< T >::GetDefaultValue() const
{
	Assert( (bool)m_UseDefaultValue );
	return m_DefaultValue;
}

template< class T >
bool CDmeTypedLog< T >::HasDefaultValue() const
{
	return m_UseDefaultValue;
}

template< class T >
void CDmeTypedLog< T >::ClearDefaultValue()
{
	m_UseDefaultValue = false;
	T out;
	CDmAttributeInfo< T >::SetDefaultValue( out );
	m_DefaultValue.Set( out );
}


template< class T >
void CDmeTypedLog< T >::InitalizeBookmarkArrays()
{
	m_BookmarkTimes[ 0 ].Init( this, "bookmarks" );
}

template<>
void CDmeTypedLog< Vector >::InitalizeBookmarkArrays()
{
	m_BookmarkTimes[ 0 ].Init( this, "bookmarksX" );
	m_BookmarkTimes[ 1 ].Init( this, "bookmarksY" );
	m_BookmarkTimes[ 2 ].Init( this, "bookmarksZ" );
}

template<>
void CDmeTypedLog< Vector2D >::InitalizeBookmarkArrays()
{
	m_BookmarkTimes[ 0 ].Init( this, "bookmarksX" );
	m_BookmarkTimes[ 1 ].Init( this, "bookmarksY" );
}

template<>
void CDmeTypedLog< Vector4D >::InitalizeBookmarkArrays()
{
	m_BookmarkTimes[ 0 ].Init( this, "bookmarksX" );
	m_BookmarkTimes[ 1 ].Init( this, "bookmarksY" );
	m_BookmarkTimes[ 2 ].Init( this, "bookmarksZ" );
	m_BookmarkTimes[ 3 ].Init( this, "bookmarksW" );
}
template<>
void CDmeTypedLog< Quaternion >::InitalizeBookmarkArrays()
{
	m_BookmarkTimes[ 0 ].Init( this, "bookmarksX" );
	m_BookmarkTimes[ 1 ].Init( this, "bookmarksY" );
	m_BookmarkTimes[ 2 ].Init( this, "bookmarksZ" );
}


template< class T >
int CDmeTypedLog< T >::GetNumBookmarkComponents() const
{
	return 1;
}

template<>
int CDmeTypedLog< Vector >::GetNumBookmarkComponents() const
{
	return 3;
}

template<>
int CDmeTypedLog< Vector2D >::GetNumBookmarkComponents() const
{
	return 2;
}

template<>
int CDmeTypedLog< Vector4D >::GetNumBookmarkComponents() const
{
	return 4;
}

template<>
int CDmeTypedLog< Quaternion >::GetNumBookmarkComponents() const
{
	return 3;
}


// Only used by undo system!!!
template< class T >
void CDmeTypedLog< T >::AddLayerToTail( CDmeLogLayer *layer )
{
	Assert( layer );
	Assert( (static_cast< CDmeTypedLogLayer< T > * >( layer ))->GetTypedOwnerLog() == this );
	m_Layers.AddToTail( layer );
}

template< class T >
CDmeLogLayer *CDmeTypedLog< T >::RemoveLayerFromTail()
{
	Assert( m_Layers.Count() >= 1 );
	CDmeLogLayer *layer = m_Layers[ m_Layers.Count() -1 ];
	m_Layers.Remove( m_Layers.Count() - 1 );
	return layer;
}

template< class T >
CDmeLogLayer *CDmeTypedLog< T >::RemoveLayer( int iLayer )
{
	Assert( m_Layers.IsValidIndex( iLayer ) );
	CDmeLogLayer *layer = m_Layers[ iLayer ];
	m_Layers.Remove( iLayer );
	return layer;
}


template< class T >
CDmeLogLayer *CDmeTypedLog< T >::AddNewLayer()
{
	if ( g_pDataModel->UndoEnabledForElement( this ) )
	{
		CUndoLayerAdded<T> *pUndo = new CUndoLayerAdded<T>( "AddNewLayer", this );
		g_pDataModel->AddUndoElement( pUndo );
	}

	CDisableUndoScopeGuard guard;

	// Now add the layer to the stack!!!
	CDmeTypedLogLayer< T > *layer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer<T>( this ) );
	if ( layer )
	{
		layer->SetOwnerLog( this );
		m_Layers.AddToTail( layer );
	}

	return layer;
}

// curve info helpers
template< class T >
const CDmeTypedCurveInfo< T > *CDmeTypedLog<T>::GetTypedCurveInfo() const
{
	Assert( !m_CurveInfo.GetElement() || dynamic_cast< const CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() ) );
	return static_cast< const CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() );
}

template< class T >
CDmeTypedCurveInfo< T > *CDmeTypedLog<T>::GetTypedCurveInfo()
{
	Assert( !m_CurveInfo.GetElement() || dynamic_cast< CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() ) );
	return static_cast< CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() );
}

template< class T >
void CDmeTypedLog<T>::SetCurveInfo( CDmeCurveInfo *pCurveInfo )
{
	Assert( !pCurveInfo || dynamic_cast< CDmeTypedCurveInfo< T > * >( pCurveInfo ) );
	m_CurveInfo = pCurveInfo;
	OnUsingCurveTypesChanged(); // FIXME: Is this really necessary? OnAttributeChanged should have already called this!
}

template< class T >
CDmeCurveInfo *CDmeTypedLog<T>::GetOrCreateCurveInfo()
{
	CDmeCurveInfo *pCurveInfo = m_CurveInfo.GetElement();
	if ( pCurveInfo )
		return pCurveInfo;

	SetCurveInfo( CreateElement< CDmeTypedCurveInfo< T > >( "curveinfo", GetFileId() ) );
	return m_CurveInfo.GetElement();
}



template < class T >
struct ActiveLayer_t
{
	ActiveLayer_t() :
		priority( 0 ),
		firstTime( 0 ),
		lastTime( 0 ),
		layer( NULL )
	{
	}

	static bool PriorityLessFunc( ActiveLayer_t< T > * const & lhs, ActiveLayer_t< T > * const & rhs )
	{
		return lhs->priority < rhs->priority;
	}

	int		priority; // higher wins

	DmeTime_t		firstTime;
	DmeTime_t		lastTime;

	CDmeTypedLogLayer< T > *layer;
};

template < class T >
struct LayerEvent_t
{
	enum EventType_t
	{
		LE_START = 0,
		LE_END
	};

	LayerEvent_t() : m_pList( NULL ), m_Type( LE_START ), m_nLayer( 0 ), m_Time( 0 )
	{
	}

	static bool LessFunc( const LayerEvent_t& lhs, const LayerEvent_t& rhs )
	{
		return lhs.m_Time < rhs.m_Time;
	}

	CUtlVector< ActiveLayer_t< T > > *m_pList;
	EventType_t		m_Type;
	int				m_nLayer;
	DmeTime_t		m_Time;
	T				m_NeighborValue;
};

template< class T >
static const T& GetActiveLayerValue( CUtlVector< ActiveLayer_t< T > > &layerlist, DmeTime_t t, int nTopmostLayer )
{
	int nCount = layerlist.Count();
#ifdef _DEBUG
	Assert( nCount >= nTopmostLayer );
#endif

	for ( int i = nTopmostLayer; i >= 0; --i )
	{
		ActiveLayer_t< T > &layer = layerlist[i];
		if ( layer.firstTime > t || layer.lastTime < t )
			continue;

		return layer.layer->GetValue( t );
	}

	if ( nCount != 0 )
	{
		const CDmeTypedLog< T > *pOwner = layerlist[0].layer->GetTypedOwnerLog();
		if ( pOwner->HasDefaultValue() )
			return pOwner->GetDefaultValue();
	}

	static T defaultVal;
	CDmAttributeInfo<T>::SetDefaultValue( defaultVal );
	return defaultVal;
}


template< class T >
static void SpewEvents( CUtlRBTree< LayerEvent_t< T > > &events )
{
	for ( unsigned short idx = events.FirstInorder(); idx != events.InvalidIndex(); idx = events.NextInorder( idx ) )
	{
		LayerEvent_t< T > *pEvent = &events[ idx ];
		Msg( "Event %u layer %i at time %i type %s\n",
			(unsigned)idx, pEvent->m_nLayer, pEvent->m_Time.GetTenthsOfMS(), pEvent->m_Type == LayerEvent_t< T >::LE_START ? "start" : "end" );
	}
}

template< class T >
static void SpewKey( const T& )
{
	// Used for all non-specialized types below.
	Msg( "GenericType" );
}

template<>
static void SpewKey<float>( const float& val )
{
	Msg( "%f", val );
}

template<>
static void SpewKey<int>( const int& val )
{
	Msg( "%d", val );
}

template<>
static void SpewKey<Vector2D>( const Vector2D& val )
{
	Msg( "%f,%f", val.x, val.y );
}

template<>
static void SpewKey<Vector4D>( const Vector4D& val )
{
	Msg( "%f,%f,%f,%f", val.x, val.y, val.z, val.w );
}

template<>
static void SpewKey<DmeTime_t>( const DmeTime_t& val )
{
	Msg( "%d", val.GetTenthsOfMS() );
}

template<>
static void SpewKey<bool>( const bool& val )
{
	Msg( "%s", val ? "true" : "false" );
}

template<>
static void SpewKey<Color>( const Color& val )
{
	Msg( "%08x", val.GetRawColor() );
}

template< >
static void SpewKey( const Vector& val )
{
	Msg( "[%f %f %f]", val.x, val.y, val.z );
}

template< >
static void SpewKey( const Quaternion& val )
{
	Msg( "[%f %f %f %f]", val.x, val.y, val.z, val.w );
}

template< >
static void SpewKey( const QAngle& val )
{
	Msg( "[%f %f %f]", val.x, val.y, val.z );
}

template< class T >
static void SpewFlattenedKey( CDmeTypedLogLayer< T > *pLogLayer, ActiveLayer_t< T > *pActiveLayer, DmeTime_t t, const T& val )
{
	Msg( "Layer %d:  adding key at time %d [%d -> %d], value ", 
		pActiveLayer->priority, t.GetTenthsOfMS(), pActiveLayer->firstTime.GetTenthsOfMS(), pActiveLayer->lastTime.GetTenthsOfMS() );
	SpewKey( val );
	Msg( "\n" );
}

template< class T >
static void ComputeLayerEvents( CDmeTypedLog< T >* pLog, CUtlVector< ActiveLayer_t< T > > &layerlist, 
	CUtlRBTree< LayerEvent_t< T > > &events, int baseLayer )
{
	// Build a list of all known layers and a sorted list of layer "transitions"
	int numLayers = pLog->GetNumLayers();
	for ( int i = baseLayer; i < numLayers; ++i )
	{
		ActiveLayer_t< T > layer;
		layer.priority = i;
		layer.layer = static_cast< CDmeTypedLogLayer< T > * >( pLog->GetLayer( i ) );
		layer.firstTime = layer.layer->GetBeginTime( true );
		layer.lastTime = layer.layer->GetEndTime( true );

		// Skip invalid layers, base layer is always valid
		if ( ( layer.firstTime == DMETIME_INVALID || layer.lastTime == DMETIME_INVALID ) && ( i != baseLayer ) ) 
			continue;
		
		// Layer zero can capture everything from above...
		if ( i == baseLayer )
		{
			layer.firstTime = DmeTime_t::MinTime();
			layer.lastTime = DmeTime_t::MaxTime();
		}

		// Add layer to global list
		int nIndex = layerlist.AddToTail( layer );

		// Add layer start/end events
		DmeTime_t tNeighbor = ( layer.firstTime != DMETIME_MINTIME ) ? ( layer.firstTime - DMETIME_MINDELTA ) : DMETIME_MINTIME;
		LayerEvent_t< T > start;
		start.m_pList = &layerlist;
		start.m_nLayer = nIndex;
		start.m_Type = LayerEvent_t< T >::LE_START;
		start.m_Time = layer.firstTime;
		start.m_NeighborValue = GetActiveLayerValue( layerlist, tNeighbor, nIndex - 1 );
		events.Insert( start );

		tNeighbor = ( layer.lastTime != DMETIME_MAXTIME ) ? ( layer.lastTime + DMETIME_MINDELTA ) : DMETIME_MAXTIME;
		LayerEvent_t< T > end;
		end.m_pList = &layerlist;
		end.m_nLayer = nIndex;
		end.m_Type = LayerEvent_t< T >::LE_END;
		end.m_Time = layer.lastTime;
		end.m_NeighborValue = GetActiveLayerValue( layerlist, tNeighbor, nIndex - 1 );
		events.Insert( end );
	}
}


template< class T >
static void AddDiscontinitySample( CDmeTypedLogLayer< T > *pTargetLayer, CDmeTypedLog< T > *pLog, DmeTime_t tKeyTime, const T& val, const char *pSpewLabel )
{
	// Finally, add a helper key. 
	
	// NOTE: The SetKey function is called with removeReduntant false because the discontinuity sample
	// is at the very end of the range of the layer, and may in fact be overwritten by the following 
	// layer, meaning that if the proceeding value is removed when the discontinuity sample is overwritten
	// the results will no longer be correct because the last sample of the layer will have been lost.
	const bool bRemoveRedundant = false;

	if ( pLog->IsUsingCurveTypes() )
	{
		if ( pSpewLabel )
		{
			Msg( "Adding %s helper key at %d value ", pSpewLabel, tKeyTime.GetTenthsOfMS() );
			SpewKey( val );
			Msg( " [curvetype %s]\n", Interpolator_NameForCurveType( pLog->GetDefaultCurveType(), false ) );
		}
		pTargetLayer->SetKey( tKeyTime, val, pTargetLayer->GetSegmentInterpolationSetting( tKeyTime, DMETIME_INVALID, false ), pLog->GetDefaultCurveType(), bRemoveRedundant );
	}
	else
	{
		if ( pSpewLabel )
		{
			Msg( "Adding %s helper key at %d value ", pSpewLabel, tKeyTime.GetTenthsOfMS() );
			SpewKey( val );
			Msg( "\n" );
		}
		pTargetLayer->SetKey( tKeyTime, val, pTargetLayer->GetSegmentInterpolationSetting( tKeyTime, DMETIME_INVALID, false ), CURVE_DEFAULT, bRemoveRedundant );
	}
}


template< class T >
static DmeTime_t ProcessStartLayerStartEvent( 
	bool bSpew,
	bool bFixupDiscontinuities,
	CDmeTypedLog< T > *pLog, 
	LayerEvent_t< T > *pEvent, 
	CUtlVector< ActiveLayer_t< T > > &layerlist, 
	CUtlRBTree< ActiveLayer_t< T > * > &active, 
	CDmeTypedLogLayer< T > *flattenedlayer )
{
	Assert( pEvent->m_Type == LayerEvent_t< T >::LE_START );

	// Push it onto the active stack if it's not already on the stack
	if ( active.Find( &layerlist[ pEvent->m_nLayer ] ) != active.InvalidIndex() )
		return pEvent->m_Time;

	if ( bSpew )
	{
		Msg( "adding layer %d to stack\n", layerlist[ pEvent->m_nLayer ].priority );
	}
	active.Insert( &layerlist[ pEvent->m_nLayer ] );

	if ( !bFixupDiscontinuities || ( pEvent->m_Time == DMETIME_MINTIME ) )
		return pEvent->m_Time;

	// We'll need to add 2 new "discontinuity" fixup samples.
	// 1) A sample from the base layer @ start time - .1 msec
	// 2) A sample from the new layer @ start time
	int nActiveCount = active.Count();
	if ( nActiveCount >= 2 )
	{
		DmeTime_t tKeyTime = pEvent->m_Time - DmeTime_t( 1 );
		AddDiscontinitySample( flattenedlayer, pLog, tKeyTime, pEvent->m_NeighborValue, bSpew ? "start" : NULL ); 
	}
	AddDiscontinitySample( flattenedlayer, pLog, pEvent->m_Time, GetActiveLayerValue( layerlist, pEvent->m_Time, pEvent->m_nLayer ), bSpew ? "start" : NULL );
	return pEvent->m_Time;
}

template< class T >
static DmeTime_t ProcessStartLayerEndEvent( 
	bool bSpew,
	bool bFixupDiscontinuities,
	CDmeTypedLog< T > *pLog, 
	LayerEvent_t< T > *pEvent, 
	CUtlVector< ActiveLayer_t< T > > &layerlist, 
	CUtlRBTree< ActiveLayer_t< T > * > &active, 
	CDmeTypedLogLayer< T > *pBaseLayer )
{
	Assert( pEvent->m_Type == LayerEvent_t< T >::LE_END );

	// Push it onto the active stack if it's not already on the stack
	if ( bSpew )
	{
		Msg( "removing layer %d from stack\n", layerlist[ pEvent->m_nLayer ].priority );
	}

	// We'll need to add a "discontinuity" fixup sample from the 
	// 1) A sample from the ending layer @ start time
	// 2) A sample from the new layer @ start time + .1 msec 
	// NOTE: This will cause problems if there are non-default value keys at max time
	Assert( active.Count() >= 1 );
	if ( bFixupDiscontinuities && ( pEvent->m_Time != DMETIME_MAXTIME ) )
	{
		AddDiscontinitySample( pBaseLayer, pLog, pEvent->m_Time, GetActiveLayerValue( layerlist, pEvent->m_Time, pEvent->m_nLayer ), bSpew ? "end" : NULL );
		if ( active.Count() >= 2 )
		{
			DmeTime_t keyTime = pEvent->m_Time + DmeTime_t( 1 );
			AddDiscontinitySample( pBaseLayer, pLog, keyTime, pEvent->m_NeighborValue, bSpew ? "end" : NULL ); 
		}
	}

	active.Remove( &layerlist[ pEvent->m_nLayer ] );
	return ( active.Count() >= 2 ) ? pEvent->m_Time + DmeTime_t( 1 ) : pEvent->m_Time;
}

template< class T >
void CDmeTypedLog< T >::FlattenLayers( float threshold, int flags, int baseLayer /*=0*/ )
{
	// Already flattened
	int nLayersToFlatten = m_Layers.Count() - 1 - baseLayer;
	if ( nLayersToFlatten <= 0 )
		return;

	CDmeTypedLogLayer< T > *pBaseLayer = GetLayer( baseLayer );
	if ( pBaseLayer == NULL )
		return;

	if ( g_pDataModel->UndoEnabledForElement( this ) )
	{
		CUndoFlattenLayers<T> *pUndo = new CUndoFlattenLayers<T>( "FlattenLayers", this, threshold, flags, baseLayer );
		g_pDataModel->AddUndoElement( pUndo );
	}

	bool bSpew = ( flags & FLATTEN_SPEW ) != 0;
	bool bFixupDiscontinuities = ( flags & FLATTEN_NODISCONTINUITY_FIXUP ) == 0;

	// NOTE:  UNDO IS DISABLED FOR THE REST OF THIS OPERATION (the above function does what we need to preserve the layers)
	CDisableUndoScopeGuard guard;

	CDmeTypedLogLayer< T > *pFlattenedlayer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer< T >( this ) );
	pFlattenedlayer->SetOwnerLog( this );
	pFlattenedlayer->SetInfinite( pBaseLayer->IsLeftInfinite(), pBaseLayer->IsRightInfinite() );

	// Global list of layers
	CUtlVector< ActiveLayer_t< T > > layerlist;
	// List of all start/end layer events, sorted by the time at which the event occurs ( we walk this list in order )
	CUtlRBTree< LayerEvent_t< T > >	 events( 0, 0, LayerEvent_t< T >::LessFunc );
	// Stack of active events, sorted by event "priority", which means last item is the one writing data into the new base layer
	CUtlRBTree< ActiveLayer_t< T > * > active( 0, 0, ActiveLayer_t< T >::PriorityLessFunc );

	// Build layer list and list of start/end events and times
	ComputeLayerEvents( this, layerlist, events, baseLayer );

	// Debuggins
	if ( bSpew )
	{
		SpewEvents( events );
	}

	// Set the value equality threshold that will be used by SetKey to match the specified threshold
	float saveThreshold = CDmeTypedLog< T >::GetValueThreshold();
	CDmeTypedLog< T >::SetValueThreshold( 0.0f ); // don't remove redundant keys one-by-one in SetKey, wait until the explicit call to RemoveRedundantKeys()

	// Now walk from the earliest time in any layer until the latest time, going key by key and checking if the active layer should change as we go
	
	DmeTime_t iCurrentKeyTime = DmeTime_t::MinTime();
	unsigned short idx = events.FirstInorder();
	while ( 1 )
	{
		if ( idx == events.InvalidIndex() )
			break;

		LayerEvent_t< T > *pEvent = &events[ idx ];

		switch ( pEvent->m_Type )
		{
		default:
			iCurrentKeyTime = pEvent->m_Time;
			Assert( 0 );
			break;
		case LayerEvent_t< T >::LE_START:
			iCurrentKeyTime = ProcessStartLayerStartEvent( bSpew, bFixupDiscontinuities, this, pEvent, layerlist, active, pFlattenedlayer );
			break;

		case LayerEvent_t< T >::LE_END:
			iCurrentKeyTime = ProcessStartLayerEndEvent( bSpew, bFixupDiscontinuities, this, pEvent, layerlist, active, pFlattenedlayer );
			break;
		}

		int nNextIndex = events.NextInorder( idx );

		// We popped the last item off the stack
		if ( nNextIndex == events.InvalidIndex() )
		{
			Assert( active.Count() == 0 );
			break;
		}

		// Walk from current time up to the time of the next relevant event
		LayerEvent_t< T > *nextevent = &events[ nNextIndex ];
		DmeTime_t layerFinishTime = nextevent->m_Time;

		// The topmost layer is the active layer
		int layernum = active.LastInorder();
		if ( layernum == active.InvalidIndex() )
			break;

		ActiveLayer_t< T > *activeLayer = active[ layernum ];
		CDmeTypedLogLayer< T > *loglayer = activeLayer->layer;

		// Splat all keys between the current head position and the next event time (layerFinishTime) into the flattened layer
		int keyCount = loglayer->GetKeyCount();
		for ( int j = 0; j < keyCount; ++j )
		{
			DmeTime_t keyTime = loglayer->GetKeyTime( j );
			// Key is too early, skip
			if ( keyTime < iCurrentKeyTime )
				continue;

			// Done with this layer, set time exactly equal to end time so next layer can take over 
			//  at the correct spot
			if ( keyTime >= layerFinishTime )
			{
				iCurrentKeyTime = layerFinishTime;
				break;
			}

			// Advance the head position
			iCurrentKeyTime = keyTime;

			// Because it's a key, the interpolated value should == the actual value (not true for certain 4 point curve types, but we shouldn't support them
			//  for this type of operation anyway)
			const T& val = loglayer->GetKeyValue( j );

			// Debugging spew
			if ( bSpew )
			{
				SpewFlattenedKey( loglayer, activeLayer, iCurrentKeyTime, val );
			}

			// Now set the key into the flattened layer
			pFlattenedlayer->SetKey( iCurrentKeyTime, val, loglayer->GetSegmentInterpolationSetting(j), loglayer->IsUsingCurveTypes() ? loglayer->GetKeyCurveType( j ) : CURVE_DEFAULT );
		}

		bool bLeftInfinite = ( pFlattenedlayer->IsLeftInfinite() || loglayer->IsLeftInfinite() );
		bool bRightInfinite = ( pFlattenedlayer->IsRightInfinite() || loglayer->IsRightInfinite() );
		pFlattenedlayer->SetInfinite( bLeftInfinite, bRightInfinite );

		idx = nNextIndex;
	}

	// Restore the threshold value
	CDmeTypedLog< T >::SetValueThreshold( saveThreshold );


	// Blow away all of the existing layers except the original base layer
	while ( GetNumLayers() > ( baseLayer + 1 ) )
	{
		CDmeTypedLogLayer< T > *layer = static_cast< CDmeTypedLogLayer< T > * >( RemoveLayerFromTail() );
		g_pDataModel->DestroyElement( layer->GetHandle() );
	}

	// Compress the flattened layer
	pFlattenedlayer->RemoveRedundantKeys( threshold, true );
	
	// Copy the flattened layer over the existing base layer
	pBaseLayer->CopyLayer( pFlattenedlayer );

	g_pDataModel->DestroyElement( pFlattenedlayer->GetHandle() );
}

template< class T >
void CDmeTypedLog< T >::StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const DmeLogTransformParams_t &transformParams, const CDmAttribute *pAttr, uint arrayIndex /*= 0*/, bool bTimeFilter /*= true*/, int layerIndex /* = -1 */ )
{
	DmAttributeType_t type = pAttr->GetType();
	if ( IsValueType( type ) )
	{
		Assert( pAttr->GetType() == GetDataType() );
		StampKeyAtHead( tHeadPosition, tPreviousHeadPosition, params, transformParams, pAttr->GetValue< T >(), bTimeFilter, layerIndex );
	}
	else if ( IsArrayType( type ) )
	{
		Assert( ArrayTypeToValueType( type ) == GetDataType() );
		CDmrArrayConst<T> array( pAttr );
		StampKeyAtHead( tHeadPosition, tPreviousHeadPosition, params, transformParams, array[ arrayIndex ], bTimeFilter, layerIndex );
	}
	else
	{
		Assert( 0 );
	}
}

template< class T >
void CDmeTypedLog< T >::FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params )
{
	bool bWasAdvancing = params.IsTimeAdvancing();
	params.ResetTimeAdvancing();

	if ( !params.m_bAttachedMode )
		return;

	if ( !bWasAdvancing )
		return;

	// Should be in "layer recording" mode!!!
	Assert( GetNumLayers() >= 2 );
	int nBestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nBestLayer < 1 )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = GetLayer( nBestLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	int nKeyCount = pWriteLayer->GetKeyCount();
	if ( nKeyCount <= 0 )
		return;

	// The head is considered to be at the "last" value
 	T headValue = pWriteLayer->GetKeyValue( nKeyCount - 1 );

	DmeLogTransformParams_t defaultParams;
	_StampKeyAtHeadResample( tHeadPosition, params, defaultParams, headValue, true, false ); 
}

template< >
float CDmeTypedLog< float >::ClampValue( const float& value )
{
	float retval;
	if ( !IsUsingCurveTypes() )
	{
		retval = clamp( value, 0.0f, 1.0f );
	}
	else
	{
		retval = clamp( value, GetMinValue(), GetMaxValue() );
	}
	return retval;
}

template < class T >
T CDmeTypedLog< T >::MaskValue( DmeTime_t time, const T& value, LogComponents_t componentFlags ) const
{
	T writeValue = value;
	int nLayerForTime = FindLayerForTime( time );
	if ( nLayerForTime != -1 )
	{
		GetLayer( nLayerForTime )->MaskValue( time, value, componentFlags );
	}
	return writeValue;
}

template< class T >
void CDmeTypedLog< T >::StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const DmeLogTransformParams_t &transformParams, const T& value, bool bFilterByTimeSelection, int layerIndex /*= -1*/ )
{
	//T useValue = ClampValue( value );

	// This gets set if time ever starts moving (even if the user pauses time while still holding a slider)
	if ( params.IsTimeAdvancing() )
	{
		// If bFilterByTimeSelection is true, this uses the time selection as a "filter" to decide whether to stamp a new
		// key at the current position, otherwise will stamp a new key at the current time regardless of the time selection.
		_StampKeyAtHead( tHeadPosition, tPreviousHeadPosition, params, value, bFilterByTimeSelection, layerIndex );
	}
	else
	{
		_StampKeyAtHeadResample( tHeadPosition, params, transformParams, value, false, true, layerIndex );
	}
}

/*
template<>
void CDmeTypedLog< float >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const float& value );
template<>
void CDmeTypedLog< bool >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const bool& value );
template<>
void CDmeTypedLog< Color >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Color& value );
template<>
void CDmeTypedLog< Vector4D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector4D& value );
template<>
void CDmeTypedLog< Vector2D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector2D& value );
template<>
void CDmeTypedLog< VMatrix >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const VMatrix& value );
template<>
void CDmeTypedLog< Quaternion >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Quaternion& value );
template<>
void CDmeTypedLog< QAngle >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const QAngle& value );
*/

// Masks all keys within the time range, returns true if keys were modified
template< class T >
bool CDmeTypedLog< T >::MaskKeyRange( DmeTime_t tStartTime, DmeTime_t tEndTime, LogComponents_t nComponentFlags, bool bInfiniteLeft /*= false*/, bool bInfiniteRight /*= false*/ )
{
	CDmeTypedLogLayer<T> *pLayer = GetLayer( GetTopmostLayer() );
	return pLayer->MaskKeyRange( tStartTime, tEndTime, nComponentFlags, bInfiniteLeft, bInfiniteRight );
}

//-----------------------------------------------------------------------------
// Helper class used to compute falloff blend factors
//-----------------------------------------------------------------------------
enum TSRegion_t
{
	TS_REGION_IN		= -1,
	TS_REGION_HOLD		= 0,
	TS_REGION_OUT		= 1,
};

template< class T >
struct LogClampHelper_t
{
public:
	LogClampHelper_t() : m_tLastTime( DMETIME_MINTIME ) {}

	DmeTime_t m_tLastTime;
	T m_LastUnclampedValue;
};

template < class T >
void ComputeTransform( const T &position, const Quaternion &rotation, matrix3x4_t &transformMatrix )
{
	SetIdentityMatrix( transformMatrix );
}

template <>
void ComputeTransform( const Vector &position, const Quaternion &rotation,  matrix3x4_t &transformMatrix )
{
	AngleMatrix( RadianEuler( rotation ), position, transformMatrix );
}


template< class T >
class CLogFalloffBlend
{

public:
	void Init( CDmeTypedLog<T> *pLog, DmeTime_t tHead, const T& delta, bool bUsePresetRules, TSRegion_t nFalloffRegion, const DmeLog_TimeSelection_t &tsParams, const DmeLogTransformParams_t &transformParams );
	float ComputeBlendFactor( DmeTime_t tTime ) const;
	const T& GetDelta() const;
	void StampKey( CDmeTypedLogLayer<T>* pWriteLayer, DmeTime_t t, const CDmeTypedLogLayer<T>* pReadLayer, float flIntensity, LogClampHelper_t<T> &helper, bool bSpew, const T* pInterpTarget, const CDmeTypedLogLayer< Quaternion >* pRotationLayer, const DmeLog_TimeSelection_t &tsParams, const DmeLogTransformParams_t &transformParams ) const;
	void UpdateClampHelper( DmeTime_t t, const CDmeTypedLogLayer<T>* pReadLayer, float flIntensity, LogClampHelper_t<T> &helper, const T* pInterpTarget );

private:
	void ComputeDelta( CDmeTypedLog<T> *pLog, const T& delta, const T& holdValue );
	void InsertClampTransitionPoints( CDmeTypedLogLayer<T>* pWriteLayer, DmeTime_t t, LogClampHelper_t<T> &clampHelper, const T& val, bool bSpew ) const;
	void ComputeBounds( CDmeTypedLog<T> *pLog );


	bool		m_bUsePresetRules;
	T 			m_HeadValue;
	T 			m_BaseValue;
	T 			m_Delta;
	T 			m_MinValue;
	T 			m_MaxValue;
	Quaternion	m_HeadRotation;
	Quaternion  m_BaseRotation;
	matrix3x4_t	m_HeadTransform;
	matrix3x4_t	m_InvHeadTransform;
	DmeTime_t	m_tHeadTime;
	DmeTime_t	m_tBaseTime;
	DmeTime_t	m_tHoldTime;
	TSRegion_t	m_nTSRegion;
	int			m_nInterpolatorType;
	int			m_nCurveType;
	float		m_flOOTime;
};



template< class T >
void CLogFalloffBlend< T >::Init( CDmeTypedLog<T> *pLog, DmeTime_t tHead, const T& delta, bool bUsePresetRules, TSRegion_t nTSRegion, const DmeLog_TimeSelection_t &params, const DmeLogTransformParams_t &transformParams )
{
	
	m_bUsePresetRules = bUsePresetRules;
	m_nTSRegion = nTSRegion;
	m_tHeadTime = tHead;



	m_HeadValue = pLog->GetValueSkippingTopmostLayer( m_tHeadTime );
	ComputeBounds( pLog );


	if ( m_nTSRegion == TS_REGION_HOLD )
	{
		m_tBaseTime = m_tHoldTime = DMETIME_INVALID;
		m_flOOTime = 0.0f;
		m_BaseValue = m_HeadValue;
		m_Delta = delta;
		m_nInterpolatorType = INTERPOLATE_DEFAULT;
	}
	else
	{
		int nFalloffRegion = ( m_nTSRegion == TS_REGION_IN  ) ? 0 : 1;
		m_tBaseTime = params.m_nTimes[ TS_FALLOFF( nFalloffRegion ) ];
		m_tHoldTime = params.m_nTimes[ TS_HOLD   ( nFalloffRegion ) ];

		float flDuration = m_tHoldTime.GetSeconds() - m_tBaseTime.GetSeconds();
		m_flOOTime = ( flDuration != 0.0f ) ? 1.0f / flDuration : 0.0f;

		m_BaseValue = pLog->GetValueSkippingTopmostLayer( m_tBaseTime );
		T holdValue = pLog->GetValueSkippingTopmostLayer( m_tHoldTime );
		ComputeDelta( pLog, delta, holdValue );
		m_nInterpolatorType = params.m_nFalloffInterpolatorTypes[ nFalloffRegion ];
	}

	m_nCurveType = pLog->IsUsingCurveTypes() ? pLog->GetDefaultCurveType() : CURVE_DEFAULT;


	// Read the rotation at the base time and head time if the rotation layer is specified.
	m_HeadRotation = quat_identity;
	m_BaseRotation = quat_identity;
	if ( transformParams.m_pRotationLog != NULL )
	{
		m_HeadRotation = transformParams.m_pRotationLog->GetValueSkippingTopmostLayer( m_tHeadTime );
		m_BaseRotation = transformParams.m_pRotationLog->GetValueSkippingTopmostLayer( m_tBaseTime );
	}

	ComputeTransform( m_HeadValue, m_HeadRotation, m_HeadTransform );
	MatrixInvert( m_HeadTransform, m_InvHeadTransform );
}

template< class T >
void CLogFalloffBlend< T >::ComputeBounds( CDmeTypedLog<T> *pLog )
{
}

template<>
void CLogFalloffBlend< float >::ComputeBounds( CDmeTypedLog<float> *pLog )
{
	m_MinValue = pLog->IsUsingCurveTypes() ? pLog->GetMinValue() : 0.0f;
	m_MaxValue = pLog->IsUsingCurveTypes() ? pLog->GetMaxValue() : 1.0f;
}

template< class T >
void CLogFalloffBlend< T >::ComputeDelta( CDmeTypedLog<T> *pLog, const T& delta, const T& holdValue )
{
	// By default, no clamping
	m_Delta = delta;
}

template<>
void CLogFalloffBlend< float >::ComputeDelta( CDmeTypedLog<float> *pLog, const float& delta, const float& holdValue )
{
	if ( delta > 0.0f )
	{
		m_Delta = MIN( delta, m_MaxValue - holdValue ); // Max amount we can move up...
	}
	else
	{
		m_Delta = MAX( delta, m_MinValue - holdValue );  // Amount we can move down...
	}
}


template< class T >
float CLogFalloffBlend< T >::ComputeBlendFactor( DmeTime_t tTime ) const
{
	if ( m_nTSRegion == TS_REGION_HOLD )
		return 1.0f;

	// Clamp inside region; hold time beats base time (for zero width regions)
	if ( ( tTime - m_tHoldTime ) * -m_nTSRegion >= DMETIME_ZERO )
		return 1.0f;

	if ( ( tTime - m_tBaseTime ) * -m_nTSRegion <= DMETIME_ZERO )
		return 0.0f;

	float flFactor = ( tTime.GetSeconds() - m_tBaseTime.GetSeconds() ) * m_flOOTime;
	return ComputeInterpolationFactor( flFactor, m_nInterpolatorType );
}

template< class T >
const T& CLogFalloffBlend< T >::GetDelta( ) const
{
	return m_Delta;
}


//-----------------------------------------------------------------------------
// Insert points where clamping begins or ends
//-----------------------------------------------------------------------------
template< class T >
void CLogFalloffBlend< T >::InsertClampTransitionPoints( CDmeTypedLogLayer<T>* pWriteLayer, 
	DmeTime_t t, LogClampHelper_t<T> &clampHelper, const T& val, bool bSpew ) const
{
	// NOTE: By default, nothing clamps, so no transition points are needed
}

template<>
void CLogFalloffBlend< float >::InsertClampTransitionPoints( CDmeTypedLogLayer<float>* pWriteLayer, 
	DmeTime_t t, LogClampHelper_t<float> &clampHelper, const float& val, bool bSpew ) const
{
	bool bLastLess, bLastGreater, bCurrLess, bCurrGreater;
	DmeTime_t tCrossing, tDuration;
	double flOODv;

	// First time through? cache last values.
	if ( clampHelper.m_tLastTime == DMETIME_MINTIME )
		goto cacheLastValues;

	bLastLess = clampHelper.m_LastUnclampedValue < m_MinValue; 
	bLastGreater = clampHelper.m_LastUnclampedValue > m_MaxValue; 
	bCurrLess = val < m_MinValue; 
	bCurrGreater = val > m_MaxValue;
	if ( bLastLess == bCurrLess && bLastGreater == bCurrGreater )
		goto cacheLastValues;

	// NOTE: The check above means val != m_LastUnclampedValue
	flOODv = 1.0 / ( val - clampHelper.m_LastUnclampedValue );
	tDuration = t - clampHelper.m_tLastTime;

	// NOTE: Clamp semantics here favor keeping the non-clamped value
	// That's why when we start outside + end inside, we never overwrite the dest
	// and why when we start inside + end outside, we never overwrite the start
	// These two checks deal with starting outside + heading inside
	if ( bLastLess && !bCurrLess )
	{
		// Insert at min crossing
		double flFactor = ( m_MinValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime, t - DMETIME_MINDELTA );
		pWriteLayer->InsertKey( tCrossing, m_MinValue, SEGMENT_INTERPOLATE, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MinValue );
		}
	}
	else if ( bLastGreater && !bCurrGreater )
	{
		// Insert at max crossing
		double flFactor = ( m_MaxValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime, t - DMETIME_MINDELTA );
		pWriteLayer->InsertKey( tCrossing, m_MaxValue, SEGMENT_INTERPOLATE, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MaxValue );
		}
	}

	// These two checks deal with starting inside + heading outside
	if ( !bLastLess && bCurrLess )
	{
		// Insert at min crossing
		// NOTE: Clamp semantics here favor keeping the non-clamped value
		double flFactor = ( m_MinValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime + DMETIME_MINDELTA, t );
		pWriteLayer->InsertKey( tCrossing, m_MinValue, SEGMENT_INTERPOLATE, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MinValue );
		}
	}
	else if ( !bLastGreater && bCurrGreater )
	{
		// Insert at max crossing
		double flFactor = ( m_MaxValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime + DMETIME_MINDELTA, t );
		pWriteLayer->InsertKey( tCrossing, m_MaxValue, SEGMENT_INTERPOLATE, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MaxValue );
		}
	}

	// Cache off the last values
cacheLastValues:
	clampHelper.m_tLastTime = t;
	clampHelper.m_LastUnclampedValue = val;
}


//-----------------------------------------------------------------------------
// Stamp the key at the specified time
//-----------------------------------------------------------------------------
template< class T >
void CLogFalloffBlend< T >::StampKey( CDmeTypedLogLayer<T>* pWriteLayer, DmeTime_t t, 
	const CDmeTypedLogLayer<T>* pReadLayer, float flIntensity, LogClampHelper_t<T> &clampHelper, bool bSpew, 
	const T* pInterpTarget, const CDmeTypedLogLayer< Quaternion >* pRotationLayer,
	const DmeLog_TimeSelection_t &timeSelectionParams, const DmeLogTransformParams_t &transformParams ) const
{
	bool bIsVector = ( CDmAttributeInfo<T>::ATTRIBUTE_TYPE == AT_VECTOR3 );

	// Stamp the key at the current time
	T oldVal = pReadLayer->GetValue( t );
	T newVal = oldVal;

	if ( !timeSelectionParams.m_bManipulateInFalloff || m_bUsePresetRules ) // bUsePresetRules || translating || typing values into attribute slider || interpolate falloff mode
	{
		// If the transform mode is overwrite and stamping a key in the falloff region, instead of interpolating from the 
		// value at the current time interpolate from the base value, which is the value at the edge of the falloff.
		if ( ( m_nTSRegion != TS_REGION_HOLD ) && !m_bUsePresetRules && timeSelectionParams.m_TransformWriteMode == TRANSFORM_WRITE_MODE_OVERWRITE )
		{
			oldVal = m_BaseValue;
		}

		// In the falloff area the blend factor will be between 0 and 1 based on the interpolation type
		float flFactor = ComputeBlendFactor( t );
		flFactor *= flIntensity;

		T targetValue;

		if ( pInterpTarget ) // params.m_TransformWriteMode == TRANSFORM_WRITE_MODE_OVERWRITE || bUsePresetRules (ie head/in/out/default/zero/half/one/random/user-defined)
		{
			targetValue = *pInterpTarget;
		}
		else if ( bIsVector && timeSelectionParams.m_TransformWriteMode == TRANSFORM_WRITE_MODE_TRANSFORM )
		{
			targetValue = TransformAbsolute( transformParams.m_Transform, oldVal );
		}
		else // params.m_TransformWriteMode == TRANSFORM_WRITE_MODE_OFFSET || ( params.m_TransformWriteMode == TRANSFORM_WRITE_MODE_TRANSFORM && T != Vector )
		{
			targetValue = Add( oldVal, m_Delta );
		}
	
		newVal = Interpolate( flFactor, oldVal, targetValue );
	}
	else
	{
		// If the falloff mode is manipulation, the amount of the input manipulation is 
		// interpolated and then applied instead of interpolating the result of the manipulation.
		float flFactor = ComputeBlendFactor( t );

		// Retrieve the rotation value before the transform, this value will be used as
		// the rotation in constructing the local space in which the rotation will occur. 
		Quaternion oldRotation = quat_identity;
		if ( pRotationLayer )
		{
			oldRotation = pRotationLayer->GetValue( t );
		}

		// In overwrite mode the result of the falloff with a factor of 1.0 must match the new value at the head, but 
		// the result of the falloff with a factor of 0.0f must still match the value at edge of the time selection.
		if ( timeSelectionParams.m_TransformWriteMode == TRANSFORM_WRITE_MODE_OVERWRITE ) // !bUsePresetRules && manipulate falloff mode && rotating && params.m_TransformWriteMode == TRANSFORM_WRITE_MODE_OVERWRITE
		{
			Quaternion scaledRotation; // flFactor * rotationLocal
			ScaleRotationQuaternion( transformParams.m_RotationLocal, flFactor, scaledRotation );

			// Get the rotation at the head time and the base time (edge of falloff), and then 
			// calculate the difference in the rotation from the base time to the head time.
			Quaternion inv;
			Quaternion deltaRotation; // headRotation - baseRotation
			QuaternionInvert( m_BaseRotation, inv );
			QuaternionMult( inv, m_HeadRotation, deltaRotation );

			// Scale the rotation from the base to the head and then combine it with 
			// the scaled rotation in order to get the total rotation to apply. 
			Quaternion headScaledRotation;
			Quaternion totalRotation; // ( flFactor * deltaRotation ) + ( flFactor * rotationLocal )
			QuaternionScale( deltaRotation, flFactor, headScaledRotation );
			QuaternionMult( headScaledRotation, scaledRotation, totalRotation );

			if ( bIsVector ) 
			{
				// Calculate the inverse of the rotation from the base to the head and rotate the position
				// by this amount to get the position at the head without the effect of the rotation. Then
				// interpolate from the base position to the position at the head without rotation and the
				// apply the total rotation to the resulting position.
				Quaternion invDeltaRotation;
				QuaternionInvert( deltaRotation, invDeltaRotation );
				T targetPosition = Rotate( invDeltaRotation, transformParams.m_Pivot, m_HeadValue, m_HeadRotation );
				oldVal = Interpolate( flFactor, m_BaseValue, targetPosition );
				newVal = Rotate( totalRotation, transformParams.m_Pivot, oldVal, m_BaseRotation );
			}
			else if ( m_nTSRegion == TS_REGION_HOLD ) // in hold region
			{
				newVal = Rotate( scaledRotation, transformParams.m_Pivot, m_HeadValue, oldRotation );
			}
			else // in falloff region
			{
				newVal = Rotate( totalRotation, transformParams.m_Pivot, m_BaseValue, oldRotation );
			}
		}
		else // !bUsePresetRules && rotating && manipulate in falloff mode
		{
			if ( bIsVector && timeSelectionParams.m_TransformWriteMode == TRANSFORM_WRITE_MODE_TRANSFORM )
			{
				Quaternion scaledRotation;
				ScaleRotationQuaternion( transformParams.m_RotationLocal, flFactor, scaledRotation );

				// Compute the scaled rotation matrix using the transform pivot
				matrix3x4_t xform;
				AngleMatrix( RadianEuler( scaledRotation ), xform );

				matrix3x4_t m1, m2;
				AngleMatrix( vec3_angle, transformParams.m_Pivot, m1 );
				AngleMatrix( vec3_angle, -transformParams.m_Pivot, m2 );

				matrix3x4_t temp;
				ConcatTransforms( m1, xform, temp );
				ConcatTransforms( temp, m2, xform );
				ConcatTransforms( m_HeadTransform, xform, xform );

				matrix3x4_t scaledTransformMatrix;
				ConcatTransforms( xform, m_InvHeadTransform, scaledTransformMatrix );

				// Apply the transform using the active reference frame, if any.
				newVal = TransformAbsolute( scaledTransformMatrix, oldVal );
			}
			else
			{			
				Quaternion scaledRotation;
				ScaleRotationQuaternion( transformParams.m_RotationParent, flFactor, scaledRotation );
				newVal = Rotate( scaledRotation, transformParams.m_Pivot, oldVal, oldRotation, false );
			}
		}
	}

	InsertClampTransitionPoints( pWriteLayer, t, clampHelper, newVal, bSpew );

	T clampedVal = pWriteLayer->GetTypedOwnerLog()->ClampValue( newVal );
	T maskedVal = pWriteLayer->GetTypedOwnerLog()->MaskValue( t, clampedVal, timeSelectionParams.m_nComponentFlags );

	// Add a key to the new "layer" at this time with this value
	pWriteLayer->InsertKey( t, maskedVal, pReadLayer->GetSegmentInterpolationSetting(t), m_nCurveType, false );

	if ( bSpew )
	{
		Msg("	Key: %d ", t.GetTenthsOfMS() );
		SpewKey( clampedVal );
		Msg(" [" );
		SpewKey( newVal );
		Msg( "]\n" );
	}
}


//-----------------------------------------------------------------------------
// Stamp the key at the specified time
//-----------------------------------------------------------------------------
template< class T >
void CLogFalloffBlend< T >::UpdateClampHelper( DmeTime_t t, const CDmeTypedLogLayer<T>* pReadLayer, 
	float flIntensity, LogClampHelper_t<T> &clampHelper, const T* pInterpTarget )
{
	// Stamp the key at the current time
	T oldVal = pReadLayer->GetValue( t );

	// In the falloff area
	float flFactor = ComputeBlendFactor( t );
	flFactor *= flIntensity;

	T val;
	if ( !pInterpTarget )
	{
		val = ScaleValue( m_Delta, flFactor );
		val = Add( oldVal, val );
	}
	else
	{
		val = Interpolate( flFactor, oldVal, *pInterpTarget );
	}

	clampHelper.m_tLastTime = t;
	clampHelper.m_LastUnclampedValue = val;
}


//-----------------------------------------------------------------------------
// This is used to modify the entire portion of the curve under the time selection
//-----------------------------------------------------------------------------
static inline DmeTime_t ComputeResampleStartTime( const DmeLog_TimeSelection_t &params, int nSide )
{
	// NOTE: This logic will place the resampled points centered in the falloff regions
	TimeSelectionTimes_t start = ( nSide == 0 ) ? TS_LEFT_FALLOFF : TS_RIGHT_HOLD;
	TimeSelectionTimes_t end = ( nSide == 0 ) ? TS_LEFT_HOLD : TS_RIGHT_FALLOFF;

	// The falloff region must be re-sampled if not a linear interpolation or if the the transform write mode
	// is a rotation mode, since in that case the samples are not linear even if the falloff type is linear.
	if ( ( params.m_nFalloffInterpolatorTypes[nSide] != INTERPOLATE_LINEAR_INTERP ) || params.m_bManipulateInFalloff )
	{
		DmeTime_t tDuration = params.m_nTimes[end] - params.m_nTimes[start];
		if ( tDuration > params.m_nResampleInterval )
		{
			int nFactor = tDuration.GetTenthsOfMS() / params.m_nResampleInterval.GetTenthsOfMS();
			tDuration -= params.m_nResampleInterval * nFactor;
			tDuration /= 2;
			return params.m_nTimes[start] + tDuration;
		}
	}
	return DMETIME_MAXTIME;
}


//-----------------------------------------------------------------------------
// This is used to modify the entire portion of the curve under the time selection
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::_StampKeyAtHeadResample( DmeTime_t tHeadPosition, const DmeLog_TimeSelection_t& params, const DmeLogTransformParams_t &transformParams, const T& value, bool bSkipToHead, bool bClearPreviousKeys, int layerIndex  /*= -1*/ )
{
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval < DmeTime_t( 0 ) )
		return;

	// If a valid layer is specified, write to that layer, otherwise write to the topmost layer.
	if ( ( layerIndex <= 0 ) || ( layerIndex >= m_Layers.Count() ) )
	{
		layerIndex = GetTopmostLayer();
	}

	// Should be in "layer recording" mode!!!
	Assert( GetNumLayers() >= 2 );
	int nBestLayer = layerIndex; // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nBestLayer < 1 )
		return;
	CDmeTypedLogLayer< T > *pWriteLayer = GetLayer( nBestLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	if ( bClearPreviousKeys )
	{
		pWriteLayer->ClearKeys();
	}

	bool bSpew = false;

	// NOTE: The headDelta is only used when not blending toward a preset
	// When not blending toward a preset, just add the head delta onto everything.
	// When blending toward a preset, lerp towards the preset.
	T oldHeadValue;
	if ( params.m_pOldHeadValue )
	{
		if ( params.m_OldHeadValueIndex >= 0 )
		{
			CDmrArrayConst<T> headValueArray( params.m_pOldHeadValue );
			oldHeadValue = headValueArray[ params.m_OldHeadValueIndex ];
		}
		else
		{
			oldHeadValue = params.m_pOldHeadValue->GetValue< T >();
		}
	}
	else
	{
		oldHeadValue = GetValueSkippingTopmostLayer( tHeadPosition );
	}
	T headDelta = Difference( value, oldHeadValue );

	// When dragging preset fader, everything get's blended in by the amount of the preset being applied
	bool bUsePresetRules = ( RECORD_PRESET == params.GetRecordingMode() );
	bool bPerformInterpolation = bUsePresetRules;

	
	// If not using a preset, determine if the existing values should be interpolated toward the
	// new value or added to the new value based on the transform write mode for the appropriate operation.
	if ( !bUsePresetRules )
	{
		if ( params.m_TransformWriteMode == TRANSFORM_WRITE_MODE_OVERWRITE ) 
		{
			bPerformInterpolation = true;
		}

		// Use the original head position from when the operation took place
		if ( params.m_bManipulateInFalloff )
		{
			tHeadPosition = params.m_tHeadPosition;
		}
	}

	bool bUsePresetValue = bUsePresetRules && params.m_pPresetValue && params.m_pPresetValue->GetType() == CDmAttributeInfo<T>::ATTRIBUTE_TYPE;
	Assert( !params.m_pPresetValue || !IsArrayType( params.m_pPresetValue->GetType() ) );
	if ( bUsePresetRules && !bUsePresetValue )
		return;

	const T& interpTarget = bUsePresetValue ? params.m_pPresetValue->GetValue<T>() : value;


	// Compute falloff region blend factors
	CLogFalloffBlend< T > blend[ 3 ];
	blend[0].Init( this, tHeadPosition, headDelta, bUsePresetRules, TS_REGION_IN,   params, transformParams );
	blend[1].Init( this, tHeadPosition, headDelta, bUsePresetRules, TS_REGION_HOLD, params, transformParams );
	blend[2].Init( this, tHeadPosition, headDelta, bUsePresetRules, TS_REGION_OUT,  params, transformParams );


	// The algorithm we're going to use is to add samples in the following places:
	// 1) At each time selection transition point (start, end of falloff regions)
	//	  NOTE: If a falloff region has 0 size, we'll add points right outside the transition
	// 2) At the resample point (we're going to base this so the resamples always occur at the same spots)
	// 3) At any existing sample position
	// 4) Any time we switch from clamped to not clamped
	// By doing this, we will guarantee no bogus slope changes

	
	// First, compute times for transition regions
	DmeTime_t tTransitionTimes[TS_TIME_COUNT];
	memcpy( &tTransitionTimes, &params.m_nTimes, sizeof(params.m_nTimes) );
	if ( tTransitionTimes[TS_LEFT_FALLOFF] == tTransitionTimes[TS_LEFT_HOLD] )
	{
		tTransitionTimes[TS_LEFT_FALLOFF] -= DMETIME_MINDELTA;
	}
	if ( tTransitionTimes[TS_RIGHT_FALLOFF] == tTransitionTimes[TS_RIGHT_HOLD] )
	{
		tTransitionTimes[TS_RIGHT_FALLOFF] += DMETIME_MINDELTA;
	}

	DmeTime_t tStartTime = params.m_nTimes[ TS_LEFT_FALLOFF ];

	// Next, compute the first resample time for each region
	DmeTime_t tResampleStartTime[TS_TIME_COUNT];
	tResampleStartTime[TS_LEFT_FALLOFF] = DMETIME_MAXTIME;
	tResampleStartTime[TS_LEFT_HOLD] = ComputeResampleStartTime( params, 0 );
	tResampleStartTime[TS_RIGHT_HOLD] = DMETIME_MAXTIME;
	tResampleStartTime[TS_RIGHT_FALLOFF] = ComputeResampleStartTime( params, 1 );

	// Finally, figure out which layer we're reading from, 
	// where the next key is, and when we must stop reading from it
	int nReadLayer = FindLayerForTimeSkippingTopmost( tStartTime );
	CDmeTypedLogLayer< T > *pReadLayer = GetLayer( nReadLayer );
	int nLayerSampleIndex = pReadLayer->FindKey( tStartTime ) + 1;
	DmeTime_t tLayerEndTime = pReadLayer->GetEndTime( true );

	// Get the log layer for the rotation if the rotation layer was specified
	CDmeTypedLogLayer< Quaternion > *pRotationLayer = NULL;
	if ( transformParams.m_pRotationLog )
	{
		if ( nReadLayer < transformParams.m_pRotationLog->GetNumLayers() )
		{
			pRotationLayer = transformParams.m_pRotationLog->GetLayer( nReadLayer );
		}
	}


	// NOTE: This can happen after reading off the end of layer 0
	if ( tLayerEndTime <= tStartTime )
	{
		tLayerEndTime = DMETIME_MAXTIME;
	}
	DmeTime_t tNextSampleTime = nLayerSampleIndex >= pReadLayer->GetKeyCount() ? tLayerEndTime : pReadLayer->GetKeyTime( nLayerSampleIndex );
	if ( tNextSampleTime > tLayerEndTime )
	{
		tNextSampleTime = tLayerEndTime;
	}

	// If the time selection is infinite on either side skip 
	// the falloff section on the side that is infinite.
	int nNextTransition = params.m_bInfinite[ 0 ] ? TS_RIGHT_HOLD : TS_LEFT_HOLD;
	int nLastTransition = params.m_bInfinite[ 1 ] ? TS_RIGHT_FALLOFF : TS_TIME_COUNT;

	// Now keep going until we've hit the end point
	// NOTE: We use tTransitionTimes, *not* params.m_nTimes, so that we can get a single
	// sample before zero-width left falloff regions
	DmeTime_t tCurrent = tTransitionTimes[ nNextTransition - 1 ];
	DmeTime_t tResampleTime = tResampleStartTime[nNextTransition];

	const T* pInterpTarget = bPerformInterpolation ? &interpTarget : NULL;

	if ( bSpew )
	{
		Msg( "Stamp key at head resample: %s\n", GetName() );
	}

	LogClampHelper_t<T> clampHelper;
	while( nNextTransition < nLastTransition )
	{
		// Stamp the key at the current time
		if ( !bSkipToHead || ( tCurrent >= tHeadPosition ) )
		{
			blend[nNextTransition-1].StampKey( pWriteLayer, tCurrent, pReadLayer, params.m_flIntensity, clampHelper, bSpew, pInterpTarget, pRotationLayer, params, transformParams );
		}

		// Update the read layer sample
		if ( tCurrent == tNextSampleTime )
		{
			++nLayerSampleIndex;
			tNextSampleTime = nLayerSampleIndex >= pReadLayer->GetKeyCount() ? tLayerEndTime : pReadLayer->GetKeyTime( nLayerSampleIndex );
		}

		// Update the read layer
		if ( tCurrent == tLayerEndTime )
		{
			nReadLayer = FindLayerForTimeSkippingTopmost( tCurrent + DMETIME_MINDELTA );
			pReadLayer = GetLayer( nReadLayer );
			nLayerSampleIndex = pReadLayer->FindKey( tCurrent ) + 1;
			tLayerEndTime = pReadLayer->GetEndTime( true );

			// NOTE: This can happen after reading off the end of layer 0
			if ( tLayerEndTime <= tCurrent )
			{
				tLayerEndTime = DMETIME_MAXTIME;
			}

			tNextSampleTime = nLayerSampleIndex >= pReadLayer->GetKeyCount() ? tLayerEndTime : pReadLayer->GetKeyTime( nLayerSampleIndex );
			if ( tNextSampleTime > tLayerEndTime )
			{
				tNextSampleTime = tLayerEndTime;
			}
		}

		// Update the transition time
		if ( tCurrent == tTransitionTimes[nNextTransition] )
		{
			// NOTE: This is necessary because each blend region has different 'deltas'
			// to avoid overdriving in the falloff regions. Therefore, the 'previous value'
			// used in the clamping operation will be different 
			if ( nNextTransition < TS_RIGHT_FALLOFF )
			{
				blend[nNextTransition].UpdateClampHelper( tCurrent, pReadLayer, params.m_flIntensity, clampHelper, pInterpTarget );
			}

			// Also need to update the 'previous' value stored in the 
			++nNextTransition;

			// Update the first resample time
			tResampleTime = tResampleStartTime[nNextTransition];

			if ( bSpew )
			{
				Msg( "   Entering region %d\n", nNextTransition-1 );
			}
		}

		// Update the resample time
		if ( tCurrent == tResampleTime )
		{
			tResampleTime += params.m_nResampleInterval;
		}

		// Now that the key is stamped, update current time.
		tCurrent = tTransitionTimes[nNextTransition];
		if ( tResampleTime < tCurrent )
		{
			tCurrent = tResampleTime;
		}
		if ( tNextSampleTime < tCurrent )
		{
			tCurrent = tNextSampleTime;
		}
	}

	Assert( !params.m_bInfinite[ 1 ] || ( pWriteLayer->GetEndTime( false ) <= params.m_nTimes[ TS_RIGHT_HOLD ] ) );

	// Now apply final mask
	LogComponents_t nComponentFlags = params.m_nComponentFlags;
	if ( nComponentFlags != LOG_COMPONENTS_ALL )
	{
		// Mask output against base layer
		MaskAgainstLayer( pWriteLayer, pReadLayer, nComponentFlags );
	}
}

//-----------------------------------------------------------------------------
// Stamp the key directly into the log layer at the specified time. If 
// specified filter by the time selection, only stamping the key if the time is
// within the selection.
//-----------------------------------------------------------------------------
template< class T > 
void CDmeTypedLog< T >::_StampKeyAtTime( CDmeTypedLogLayer< T > *pWriteLayer, DmeTime_t t, const DmeLog_TimeSelection_t &params, const T& value, bool bFilterByTimeSelection, bool bForce )
{
	float flFraction = params.m_flIntensity;

	if ( bFilterByTimeSelection )
	{
		float flFraction = params.GetAmountForTime( t ) * params.m_flIntensity;
		if ( flFraction <= 0.0f && !bForce )
			return;
	}

	// When dragging preset fader, everything get's blended in by the amount of the preset being applied
	bool bUsePresetRules = ( RECORD_PRESET == params.GetRecordingMode() );
	if ( bUsePresetRules && !params.m_pPresetValue )
		return;

	Assert( !IsArrayType( params.m_pPresetValue->GetType() ) );

	// FIXME: Preset value should never be NULL. We need to grab it from the attribute
	const T& interpTarget = ( bUsePresetRules && params.m_pPresetValue ) ? params.m_pPresetValue->GetValue<T>() : value;
	T oldVal = GetValueSkippingTopmostLayer( t );
	T newVal = Interpolate( flFraction, oldVal, interpTarget, params.m_nComponentFlags );
	T writeVal = ClampValue( newVal );
	pWriteLayer->InsertKey( t, writeVal, GetSegmentInterpolationSetting_SkippingTopmostLayer(t), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
}


//-----------------------------------------------------------------------------
// In this case, we actually stamp a key right at the head position unlike the above method
//-----------------------------------------------------------------------------
template< class T > 
void CDmeTypedLog< T >::_StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t &params, const T& value, bool bFilterByTimeSelection, int layerIndex /*= -1*/ )
{

	// If a valid layer is specified, write to that layer, otherwise write to the topmost layer.
	if ( ( layerIndex <= 0 ) || ( layerIndex >= m_Layers.Count() ) )
	{
		layerIndex = GetTopmostLayer();
	}

	// Should be in "layer recording" mode!!!
	Assert( GetNumLayers() >= 2 );
	int nBestLayer = layerIndex; // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nBestLayer < 1 )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = GetLayer( nBestLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	// NOTE: This little trickery is necessary to generate samples right outside the
	// transition region in the case of zero length falloff regions
	DmeLog_TimeSelection_t tempParams = params;
	if ( tempParams.m_nTimes[TS_LEFT_FALLOFF] == tempParams.m_nTimes[TS_LEFT_HOLD] )
	{
		tempParams.m_nTimes[TS_LEFT_FALLOFF] -= DMETIME_MINDELTA;
	}
	if ( tempParams.m_nTimes[TS_RIGHT_FALLOFF] == tempParams.m_nTimes[TS_RIGHT_HOLD] )
	{
		tempParams.m_nTimes[TS_RIGHT_FALLOFF] += DMETIME_MINDELTA;
	}

	int nPrevRegion = tempParams.ComputeRegionForTime( tPreviousHeadPosition );
	int nCurrRegion = tempParams.ComputeRegionForTime( tHeadPosition );

	// Test for backward performance!
	if ( nCurrRegion < nPrevRegion )
	{
		V_swap( nCurrRegion, nPrevRegion );
	}

	// Insert samples at each transition point we skipped over
	if ( bFilterByTimeSelection )
	{
		for ( int i = nPrevRegion; i < nCurrRegion; ++i )
		{
			_StampKeyAtTime( pWriteLayer, tempParams.m_nTimes[ i ], params, value, true, true );
		}
	}

	_StampKeyAtTime( pWriteLayer, tHeadPosition, params, value, bFilterByTimeSelection );
}


template< class T >
void CDmeTypedLog< T >::RemoveKeys( DmeTime_t starttime )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveKeys( starttime );
}

template< class T >
void CDmeTypedLog< T >::RemoveKey( int nKeyIndex, int nNumKeysToRemove /*= 1*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveKey( nKeyIndex, nNumKeysToRemove );
}

template< class T >
void CDmeTypedLog< T >::ClearKeys()
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->ClearKeys();
}

//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
template< class T >
DmeTime_t CDmeTypedLog< T >::GetKeyTime( int nKeyIndex ) const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return DmeTime_t::MinTime();
	return GetLayer( bestLayer )->GetKeyTime( nKeyIndex );
}

template< class T >
void CDmeTypedLog< T >::SetKeyTime( int nKeyIndex, DmeTime_t keyTime )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;
	return GetLayer( bestLayer )->SetKeyTime( nKeyIndex, keyTime );
}

//-----------------------------------------------------------------------------
// Returns the index of a particular key
//-----------------------------------------------------------------------------
template< class T >
int CDmeTypedLog< T >::FindKeyWithinTolerance( DmeTime_t nTime, DmeTime_t nTolerance )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return -1;

	return GetLayer( bestLayer )->FindKeyWithinTolerance( nTime, nTolerance );
}

//-----------------------------------------------------------------------------
// tests whether two values differ by more than the threshold
//-----------------------------------------------------------------------------
template<>
bool CDmeTypedLog< Vector >::ValuesDiffer( const Vector& a, const Vector& b ) const
{
	return a.DistToSqr( b ) > s_threshold * s_threshold;
}

template<>
bool CDmeTypedLog< QAngle >::ValuesDiffer( const QAngle& a, const QAngle& b ) const
{
	return ( a - b ).LengthSqr() > s_threshold * s_threshold;
}

template<>
bool CDmeTypedLog< Quaternion >::ValuesDiffer( const Quaternion& a, const Quaternion& b ) const
{
	return QuaternionAngleDiff( a, b ) > s_threshold;
}

template<>
bool CDmeTypedLog< float >::ValuesDiffer( const float& a, const float& b ) const
{
	return fabs( a - b ) > s_threshold;
}

template< class T >
bool CDmeTypedLog< T >::ValuesDiffer( const T& a, const T& b ) const
{
	return a != b;
}

//-----------------------------------------------------------------------------
// Sets a key, removes all keys after this time
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::SetKey( DmeTime_t time, const T& value, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*=CURVE_DEFAULT*/)
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->SetKey( time, value, interpSetting, curveType );
}

template< class T >
CDmeTypedLogLayer< T > *CDmeTypedLog< T >::GetLayer( int index )
{
	if ( index < 0 )
		return NULL;

	return static_cast< CDmeTypedLogLayer< T > * >( m_Layers[ index ] );
}

template< class T >
const CDmeTypedLogLayer< T > *CDmeTypedLog< T >::GetLayer( int index ) const
{
	if ( index < 0 )
		return NULL;

	return static_cast< CDmeTypedLogLayer< T > * >( m_Layers[ index ] );
}


//-----------------------------------------------------------------------------
// Finds a key within tolerance, or adds one
//-----------------------------------------------------------------------------
template< class T >
int CDmeTypedLog< T >::FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*=CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return -1;

	return GetLayer( bestLayer )->FindOrAddKey( nTime, nTolerance, value, interpSetting, curveType );
}


//-----------------------------------------------------------------------------
// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
//-----------------------------------------------------------------------------
template < class T >
int CDmeTypedLog< T >::InsertKey( DmeTime_t nTime, const T& value, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*=CURVE_DEFAULT*/, bool bIgnoreTolerance /*= false*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return -1;

	return GetLayer( bestLayer )->InsertKey( nTime, value, interpSetting, curveType, bIgnoreTolerance );
}

template < class T >
int CDmeTypedLog< T >::InsertKeyAtTime( DmeTime_t nTime, int curveType /*=CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return -1;

	return GetLayer( bestLayer )->InsertKeyAtTime( nTime, curveType );
}

template< class T >
const T& CDmeTypedLog< T >::GetValue( DmeTime_t time ) const
{
	int bestLayer = FindLayerForTime( time );
	if ( bestLayer < 0 )
	{
		static T s_value;
		CDmAttributeInfo< T >::SetDefaultValue( s_value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		return s_value;
	}

	return GetLayer( bestLayer )->GetValue( time );
}

template< class T >
SegmentInterpolation_t CDmeTypedLog< T >::GetSegmentInterpolationSetting( DmeTime_t time ) const
{
	int bestLayer = FindLayerForTime( time );
	if ( bestLayer < 0 )
	{
		return SEGMENT_INTERPOLATE;
	}

	return GetLayer( bestLayer )->GetSegmentInterpolationSetting( time );
}

template< class T >
const T& CDmeTypedLog< T >::GetValueSkippingTopmostLayer( DmeTime_t time ) const
{
	int nLayer = FindLayerForTimeSkippingTopmost( time );
	if ( nLayer < 0 )
		return GetValue( time );
	return GetLayer( nLayer )->GetValue( time );
}

template< class T >
SegmentInterpolation_t CDmeTypedLog< T >::GetSegmentInterpolationSetting_SkippingTopmostLayer( DmeTime_t time ) const
{
	int nLayer = FindLayerForTimeSkippingTopmost( time );
	if ( nLayer < 0 )
		return GetSegmentInterpolationSetting( time );
	return GetLayer( nLayer )->GetSegmentInterpolationSetting( time );
}

template< class T >
const T& CDmeTypedLog< T >::GetValueBelowLayer( DmeTime_t time, int nTopLayerIndex ) const
{
	int nLayer = FindLayerForTimeBelowLayer( time, nTopLayerIndex );
	if ( nLayer < 0 )
		return GetValue( time );
	return GetLayer( nLayer )->GetValue( time );
}

template< class T >
void CDmeTypedLog< T >::SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index, SegmentInterpolation_t interpSetting /*= SEGMENT_INTERPOLATE*/, int curveType /*= CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return;

	GetLayer( bestLayer )->SetKey( time, pAttr, index, interpSetting, curveType );
}

template< class T >
bool CDmeTypedLog< T >::SetDuplicateKeyAtTime( DmeTime_t time )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return false;

	return GetLayer( bestLayer )->SetDuplicateKeyAtTime( time );
}


//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
template< class T >
const T& CDmeTypedLog< T >::GetKeyValue( int nKeyIndex ) const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
	{
		static T s_value;
		CDmAttributeInfo< T >::SetDefaultValue( s_value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		return s_value;
	}

	return GetLayer( bestLayer )->GetKeyValue( nKeyIndex );
}

template< class T >
void CDmeTypedLog< T >::GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index ) const
{
	int bestLayer = FindLayerForTime( time );
	if ( bestLayer < 0 )
	{
		T value;
		CDmAttributeInfo< T >::SetDefaultValue( value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		pAttr->SetValue( CDmAttributeInfo< T >::AttributeType(), &value );
		return;
	}

	GetLayer( bestLayer )->GetValue( time, pAttr, index );
}

template< class T >
void CDmeTypedLog< T >::GetValueSkippingTopmostLayer( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const 
{
	CUtlVector< int > layers;
	FindLayersForTime( time, layers );
	int layerCount = layers.Count();
	if ( layerCount <= 1 )
	{
		GetValue( time, pAttr, index );
		return;
	}

	int topMostLayer = GetTopmostLayer();
	int useLayer =  layers[ layerCount - 1 ];
	if ( topMostLayer == useLayer )
	{
		useLayer = layers[ layerCount - 2 ];
	}
	Assert( useLayer >= 0 );
	GetLayer( useLayer )->GetValue( time, pAttr, index );
}

template< class T >
float CDmeTypedLog< T >::GetComponent( DmeTime_t time, int componentIndex ) const
{
	return ::GetComponent( GetValue( time ), componentIndex );
}

//-----------------------------------------------------------------------------
// resampling and filtering
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::Resample( DmeFramerate_t samplerate )
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->Resample( samplerate );
	}
}

template< class T >
void CDmeTypedLog< T >::Filter( int nSampleRadius )
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->Filter( nSampleRadius );
	}
}

template< class T >
void CDmeTypedLog< T >::Filter2( DmeTime_t sampleRadius )
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->Filter2( sampleRadius );
	}
}

template< class T >
void CDmeTypedLog< T >::OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem )
{
	BaseClass::OnAttributeArrayElementAdded( pAttribute, nFirstElem, nLastElem );
	if ( pAttribute == m_Layers.GetAttribute() )
	{
		for ( int i = nFirstElem; i <= nLastElem; ++i )
		{
			CDmeLogLayer *pLayer = m_Layers[i];
			if ( !pLayer )
				continue;

			pLayer->SetOwnerLog( this );
		}
		return;
	}
}

template< class T >
void CDmeTypedLog< T >::SetUseEdgeInfo( bool state )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetUseEdgeInfo( state );
}

template< class T >
bool CDmeTypedLog< T >::IsUsingEdgeInfo() const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->IsUsingEdgeInfo();
}

template< class T >
void CDmeTypedLog< T >::SetEdgeInfo( int edge, bool active, const T& val, int curveType )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetEdgeInfo( edge, active, val, curveType );
}

template< class T >
void CDmeTypedLog< T >::SetDefaultEdgeZeroValue( const T& val )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetDefaultEdgeZeroValue( val );
}

template< class T >
const T& CDmeTypedLog< T >::GetDefaultEdgeZeroValue() const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->GetDefaultEdgeZeroValue();
}

template< class T >
void CDmeTypedLog< T >::SetRightEdgeTime( DmeTime_t time )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetRightEdgeTime( time );
}

template< class T >
DmeTime_t CDmeTypedLog< T >::GetRightEdgeTime() const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->GetRightEdgeTime();
}

template< class T >
void CDmeTypedLog< T >::GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->GetEdgeInfo( edge, active, val, curveType );
}

template< class T >
int CDmeTypedLog< T >::GetEdgeCurveType( int edge ) const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->GetEdgeCurveType( edge );
}

template< class T >
void CDmeTypedLog< T >::GetZeroValue( int side, T& val ) const
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->GetZeroValue( side, val );
}

template< class T >
bool CDmeTypedLog< T >::IsEdgeActive( int edge ) const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->IsEdgeActive( edge );
}

template< class T >
void CDmeTypedLog< T >::GetEdgeValue( int edge, T& val ) const
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->GetEdgeValue( edge, val );
}

template< class T > 
void CDmeTypedLog< T >::BlendTimesUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, DmeTime_t tStartOffset, bool bFeatherBlendInFalloff )
{
	// Top layer has the moved key times (not including tStartOffset)
	const CDmeTypedLogLayer< T > *topLayer = static_cast< const CDmeTypedLogLayer< T > * >( secondLayer );
	if ( !topLayer )
		return;

	// Base layer has the original key times (not including tStartOffset)
	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( firstLayer );
	if ( !baseLayer )
		return;

	// Output layer which has tStartOffset factored in
	CDmeTypedLogLayer< T > *newLayer = static_cast< CDmeTypedLogLayer< T > * >( outputLayer );
	if ( !newLayer )
		return;

	Assert( topLayer->GetKeyCount() == baseLayer->GetKeyCount() );

	newLayer->ClearKeys();

	LogComponents_t nComponentFlags = params.m_nComponentFlags;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t baseKeyTime = baseLayer->GetKeyTime( i );
		DmeTime_t checkTime = baseKeyTime + tStartOffset;
		if ( checkTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
			continue;
		if ( checkTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			break;

		float flInterp = bFeatherBlendInFalloff ? params.m_flIntensity * params.GetAmountForTime( checkTime ) : params.m_flIntensity;
		DmeTime_t targetKeyTime = topLayer->GetKeyTime( i );
		DmeTime_t blendedKeyTime = Lerp( flInterp, baseKeyTime, targetKeyTime );

		// Apply mask if needed
		T val = baseLayer->MaskValue( blendedKeyTime, baseLayer->GetKeyValue( i ), nComponentFlags );

		newLayer->InsertKey( blendedKeyTime + tStartOffset, val, baseLayer->GetSegmentInterpolationSetting( baseKeyTime, blendedKeyTime, false ) );
	}

	// If masking parts of logs, then stamp old key times, too, 
	//   so that we can preserver the specific peaks from the old data set
	if ( nComponentFlags != LOG_COMPONENTS_ALL )
	{
		for ( int i = 0; i < kc; ++i )
		{
			DmeTime_t baseKeyTime = baseLayer->GetKeyTime( i );

			DmeTime_t checkTime = baseKeyTime + tStartOffset;
			if ( checkTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
				continue;
			if ( checkTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
				break;

			T val = baseLayer->GetKeyValue( i );
			T newLayerValue = newLayer->GetValue( checkTime );

			T masked = ::MaskValue( newLayerValue, val, nComponentFlags );

			int iNextKey = ((i+1) < kc) ? i+1 : i;

			SegmentInterpolation_t interpSetting = (baseLayer->GetSegmentInterpolationSetting( i, iNextKey ) == SEGMENT_NOINTERPOLATE) || 
				(newLayer->GetSegmentInterpolationSetting( checkTime, baseLayer->GetKeyTime( iNextKey ) + tStartOffset, ((i+1) < kc) ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

			newLayer->InsertKey( checkTime, masked, interpSetting );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Blend the first and second layer together using the intensity 
// parameter of the time selection and then blend the result with the base 
// layer using the time falloff.
//-----------------------------------------------------------------------------
template< class T > 
void CDmeTypedLog< T >::BlendLayersUsingTimeSelection( const CDmeLogLayer *baseLayer, const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, DmeTime_t tStartOffset )
{
	const CDmeTypedLogLayer< T > *pBaseLayer = static_cast< const CDmeTypedLogLayer< T > * >( baseLayer );
	if ( !pBaseLayer )
		return;

	const CDmeTypedLogLayer< T > *pLayerA = static_cast< const CDmeTypedLogLayer< T > * >( firstLayer );
	if ( !pLayerA )
		return;

	const CDmeTypedLogLayer< T > *pLayerB = static_cast< const CDmeTypedLogLayer< T > * >( secondLayer );
	if ( !pLayerB )
		return;

	CDmeTypedLogLayer< T > *pOutputLayer = static_cast< CDmeTypedLogLayer< T > * >( outputLayer );
	if ( !pOutputLayer )
		return;

	LogComponents_t nComponents = params.m_nComponentFlags;

	int i;

	// Resample everything in the base layer first
	int kc = pBaseLayer->GetKeyCount();
	if ( bUseBaseLayerSamples )
	{
		for ( i = 0; i < kc; ++i )
		{
			DmeTime_t keyTime = pBaseLayer->GetKeyTime( i );
			if ( keyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
				continue;
			if ( keyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
				break;

			float falloff = params.GetAmountForTime( keyTime );

			T baseVal = pBaseLayer->GetKeyValue( i );
			T valA = pLayerA->GetValue( keyTime );
			T valB = pLayerB->GetValue( keyTime );
			T blended = Interpolate( params.m_flIntensity, valA, valB );
			T newVal = Interpolate( falloff, baseVal, blended, nComponents );

			DmeTime_t nextTime = ((i+1) < kc) ? pBaseLayer->GetKeyTime(i+1) : DMETIME_INVALID;

			SegmentInterpolation_t interpSetting = (pBaseLayer->GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE) || 
				(pLayerA->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
				(pLayerB->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

			pOutputLayer->SetKey( keyTime + tStartOffset, newVal, interpSetting, CURVE_DEFAULT, false );
		}
	}

	kc = pLayerA->GetKeyCount();
	for ( i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = pLayerA->GetKeyTime( i );
		DmeTime_t finalKeyTime = keyTime + tStartOffset;
		if ( finalKeyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
			continue;
		if ( finalKeyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			break;


		float falloff = params.GetAmountForTime( finalKeyTime );

		T baseVal = pBaseLayer->GetValue( keyTime );
		T valA = pLayerA->GetKeyValue( i );
		T valB = pLayerB->GetValue( keyTime );
		T blended = Interpolate( params.m_flIntensity, valA, valB );
		T newVal = Interpolate( falloff, baseVal, blended, nComponents );

		DmeTime_t nextTime = ((i+1) < kc) ? pLayerA->GetKeyTime(i+1) : DMETIME_INVALID;

		SegmentInterpolation_t interpSetting = (pBaseLayer->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
			(pLayerA->GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE) || 
			(pLayerB->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

		pOutputLayer->InsertKey( finalKeyTime, newVal, interpSetting );
	}

	kc = pLayerB->GetKeyCount();
	for ( i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = pLayerB->GetKeyTime( i );
		DmeTime_t finalKeyTime = keyTime + tStartOffset;
		if ( finalKeyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
			continue;
		if ( finalKeyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			break;

		float falloff = params.GetAmountForTime( finalKeyTime );

		T baseVal = pBaseLayer->GetValue( keyTime );
		T valA = pLayerA->GetValue( keyTime );
		T valB = pLayerB->GetKeyValue( i );
		T blended = Interpolate( params.m_flIntensity, valA, valB );
		T newVal = Interpolate( falloff, baseVal, blended, nComponents );

		DmeTime_t nextTime = ((i+1) < kc) ? pLayerB->GetKeyTime(i+1) : DMETIME_INVALID;

		SegmentInterpolation_t interpSetting = (pBaseLayer->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
			(pLayerA->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
			(pLayerB->GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

		pOutputLayer->InsertKey( finalKeyTime, newVal, interpSetting );
	}

	if ( nComponents != LOG_COMPONENTS_ALL )
	{
		// Mask output against true base layer
		MaskAgainstLayer( pOutputLayer, GetLayer( 0 ), nComponents );
	}

	if ( g_pDmElementFramework->GetPhase() == PH_EDIT )
	{
		pOutputLayer->RemoveRedundantKeys( params.m_flThreshold, false );
	}
}

template< class T > 
void CDmeTypedLog< T >::MaskAgainstLayer( CDmeTypedLogLayer< T > *pFinalLayer, const CDmeTypedLogLayer< T > *pReferenceLayer, LogComponents_t nComponentFlags )
{
	// Nothing to do
	if ( nComponentFlags == LOG_COMPONENTS_ALL )
		return;

	int kc = pFinalLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t t = pFinalLayer->GetKeyTime( i );
		T val = pFinalLayer->GetKeyValue( i );

		T maskedVal = pReferenceLayer->MaskValue( t, val, nComponentFlags );

		pFinalLayer->SetKeyValue( i, maskedVal );
	}
}


template< class T > 
void CDmeTypedLog< T >::BlendLayersUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, bool bUseFalloff, bool bSelectionSamples, DmeTime_t tStartOffset )
{
	const CDmeTypedLogLayer< T > *topLayer = static_cast< const CDmeTypedLogLayer< T > * >( secondLayer );
	if ( !topLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( firstLayer );
	if ( !baseLayer )
		return;

	CDmeTypedLogLayer< T > *newLayer = static_cast< CDmeTypedLogLayer< T > * >( outputLayer );
	if ( !newLayer )
		return;

	LogComponents_t nComponents = params.m_nComponentFlags;

	int i;
	// Resample everything in the base layer first
	int kc = baseLayer->GetKeyCount();
	if ( bUseBaseLayerSamples )
	{
		for ( i = 0; i < kc; ++i )
		{
			DmeTime_t keyTime = baseLayer->GetKeyTime( i );
			if ( keyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
				continue;
			if ( keyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
				break;

			float frac = bUseFalloff ? params.GetAmountForTime( keyTime ) : 1.0f;
			float frac2 = params.m_flIntensity;

			T baseVal = baseLayer->GetKeyValue( i );
			T newVal = topLayer->GetValue( keyTime );
			T blended = Interpolate( frac2 * frac, baseVal, newVal, nComponents );

			DmeTime_t nextTime = ((i+1) < kc) ? baseLayer->GetKeyTime(i+1) : DMETIME_INVALID;

			SegmentInterpolation_t interpSetting = (baseLayer->GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE) || 
				(topLayer->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

			newLayer->SetKey( keyTime + tStartOffset, blended, interpSetting, CURVE_DEFAULT, false );
		}
	}

	kc = topLayer->GetKeyCount();
	for ( i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = topLayer->GetKeyTime( i );
		DmeTime_t finalKeyTime = keyTime + tStartOffset;
		if ( finalKeyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
			continue;
		if ( finalKeyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			break;
		float frac = bUseFalloff ? params.GetAmountForTime( finalKeyTime ) : 1.0f;
		float frac2 = params.m_flIntensity;

		T baseVal = baseLayer->GetValue( keyTime );
		T newVal = topLayer->GetKeyValue( i );
		T blended = Interpolate( frac2 * frac, baseVal, newVal, nComponents );

		DmeTime_t nextTime = ((i+1) < kc) ? topLayer->GetKeyTime(i+1) : DMETIME_INVALID;

		SegmentInterpolation_t interpSetting = (baseLayer->GetSegmentInterpolationSetting( keyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
			(topLayer->GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

		newLayer->InsertKey( finalKeyTime, blended, interpSetting );
	}
	
	if ( g_pDmElementFramework->GetPhase() == PH_EDIT )
	{
 		newLayer->RemoveRedundantKeys( params.m_flThreshold, false );
	}

	// If the selection samples flag is set, insert keys at each of the time points of the time selection. 
	// This is used to ensure that there is a key with the appropriate value at the edge of the time selection. 
	// Note, this is done after the removal of redundant keys so that these keys will never be removed.
	if ( bSelectionSamples )
	{		
		for ( int i = 0; i < TS_TIME_COUNT; ++i )
		{	
			DmeTime_t time = params.m_nTimes[ i ];	
			float falloff = bUseFalloff ? params.GetAmountForTime( time ) : 1.0f;

			T baseVal = baseLayer->GetValue( time - tStartOffset );
			T newVal = topLayer->GetValue( time - tStartOffset );
			T blended = Interpolate( params.m_flIntensity * falloff, baseVal, newVal, nComponents );

			DmeTime_t nextTime = ((i+1) < TS_TIME_COUNT) ? params.m_nTimes[i+1] - tStartOffset : DMETIME_INVALID;

			SegmentInterpolation_t interpSetting = (baseLayer->GetSegmentInterpolationSetting( time - tStartOffset, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
				(topLayer->GetSegmentInterpolationSetting( time - tStartOffset, nextTime, true ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

			newLayer->InsertKey( time, blended, interpSetting );	
		}
	}

	if ( nComponents != LOG_COMPONENTS_ALL )
	{
		// Mask output against true base layer
		MaskAgainstLayer( newLayer, GetLayer( 0 ), nComponents );
	}
}

template< class T > 
void CDmeTypedLog< T >::BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params, int baseLayerIndex )
{
	Assert( GetNumLayers() >= 2 );
	int bestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( bestLayer <= 0 )
		return;

	Assert( baseLayerIndex < bestLayer );
	if ( baseLayerIndex >= bestLayer )
		return;

	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval < DmeTime_t( 0 ) )
		return;

	CDmeTypedLogLayer< T > *topLayer = GetLayer( bestLayer );
	Assert( topLayer );
	if ( !topLayer )
		return;

	CDmeTypedLogLayer< T > *baseLayer = GetLayer( baseLayerIndex );
	if ( !baseLayer )
		return;

	CDmeTypedLogLayer< T > *newLayer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer< T >( this ) );
	Assert( newLayer );
	if ( !newLayer )
		return;

	BlendLayersUsingTimeSelection( baseLayer, topLayer, newLayer, params, true, true, true, DMETIME_ZERO );

	// Store it back into the new topmost layer
	topLayer->CopyLayer( newLayer );

	g_pDataModel->DestroyElement( newLayer->GetHandle() );
}


void InterleaveSampleTimes( CUtlVector< DmeTime_t > &times, int nFirstTime, DmeTime_t startTime, DmeTime_t endTime, DmeTime_t resampleInterval )
{
	Assert( ( nFirstTime >= times.Count() ) || times[ nFirstTime ] > startTime );

	DmeTime_t sampleTime = startTime + resampleInterval;
	int nNumSampleTimes = ( ( endTime - sampleTime ).GetTenthsOfMS() / resampleInterval.GetTenthsOfMS() );

	CUtlVector< DmeTime_t > combineTimes( 0, times.Count() + nNumSampleTimes + 2 );
	combineTimes.AddMultipleToTail( nFirstTime, times.Base() );

	int nBlendTimeIndex = nFirstTime;
	int nNumBlendTimes = times.Count();

	DmeTime_t blendTime = ( nBlendTimeIndex < nNumBlendTimes ) ? times[ nBlendTimeIndex ] : DMETIME_MAXTIME;

	while ( ( sampleTime < endTime ) || ( blendTime < endTime ) )
	{	
		while ( ( sampleTime <= blendTime ) && ( sampleTime < endTime ) )
		{
			combineTimes.AddToTail( sampleTime );
			sampleTime = sampleTime + resampleInterval;
		}

		while ( ( blendTime <= sampleTime ) && ( blendTime < endTime ) )
		{
			times.AddToTail( blendTime );
			++nBlendTimeIndex;
			blendTime = ( nBlendTimeIndex < nNumBlendTimes ) ? times[ nBlendTimeIndex ] : DMETIME_MAXTIME;
		}
	}

	if ( nBlendTimeIndex < nNumBlendTimes )
	{
		Assert( ( combineTimes.Count() == 0 ) || ( times[ nBlendTimeIndex ] > combineTimes.Tail() ) );
		int nNumRemainingTimes = ( nNumBlendTimes - nBlendTimeIndex );
		combineTimes.AddMultipleToTail( nNumRemainingTimes, times.Base() + nBlendTimeIndex );
	}

	times = combineTimes;
}



template< class T > 
void CDmeTypedLog< T >::BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params, const CDmeTypedLogLayer< T > *pBaseLayer, const CDmeTypedLogLayer< T > *pTargetLayer, CDmeTypedLogLayer< T > *pOutputLayer )
{
	VPROF_BUDGET( "CDmeTypedLog< T >::BlendLayersUsingTimeSelection", VPROF_BUDGETGROUP_TOOLS );
	
	if ( ( pBaseLayer == NULL ) || ( pTargetLayer == NULL ) || ( pOutputLayer == NULL ) )
		return;

	DmeTime_t timeSelection[ TS_TIME_COUNT ];
	timeSelection[ TS_LEFT_FALLOFF ]  = MIN( params.m_nTimes[ TS_LEFT_FALLOFF ], params.m_nTimes[ TS_LEFT_HOLD ] - DMETIME_MINDELTA );
	timeSelection[ TS_LEFT_HOLD ]	  = params.m_nTimes[ TS_LEFT_HOLD ];
	timeSelection[ TS_RIGHT_HOLD ]	  = params.m_nTimes[ TS_RIGHT_HOLD ];
	timeSelection[ TS_RIGHT_FALLOFF ] = MAX( params.m_nTimes[ TS_RIGHT_FALLOFF ], params.m_nTimes[ TS_RIGHT_HOLD ] + DMETIME_MINDELTA );

	const DmeTime_t tsStartTime = timeSelection[ TS_LEFT_FALLOFF ];
	const DmeTime_t tsEndTime = timeSelection[ TS_RIGHT_FALLOFF ];

	int nBaseKeyCount = pBaseLayer->GetKeyCount();
	int nTargetKeyCount = pTargetLayer->GetKeyCount();
	int nMaxSamples = nBaseKeyCount + nTargetKeyCount;

	CUtlVector< DmeTime_t > times( 0, nMaxSamples );
	CUtlVector< T > values( 0, nMaxSamples );
	
	int nBaseKeyIndex = 0;
	DmeTime_t currentTime = DMETIME_MINTIME;

	// Copy all the keys from the base layer proceeding the time selection
	while ( nBaseKeyIndex < nBaseKeyCount )
	{
		currentTime = pBaseLayer->GetKeyTime( nBaseKeyIndex );
		if ( currentTime > tsStartTime )
			break;

		T keyValue = pBaseLayer->GetKeyValue( nBaseKeyIndex );
		++nBaseKeyIndex;

		times.AddToTail( currentTime );
		values.AddToTail( keyValue );
	}

	int nNumProceedingTimes = times.Count();
	

	// Add the times at which the layers will need to be blended
	int nTargetKeyIndex = pTargetLayer->FindKey( tsStartTime ) + 1;

	DmeTime_t baseKeyTime = ( nBaseKeyIndex < nBaseKeyCount ) ? pBaseLayer->GetKeyTime( nBaseKeyIndex ) : DMETIME_MAXTIME;
	DmeTime_t targetKeyTime = ( nTargetKeyIndex < nTargetKeyCount ) ? pTargetLayer->GetKeyTime( nTargetKeyIndex ) : DMETIME_MAXTIME;

	while ( ( baseKeyTime < tsEndTime ) || ( targetKeyTime < tsEndTime ) )
	{
		while ( ( baseKeyTime <= targetKeyTime ) && ( baseKeyTime < tsEndTime ) )
		{
			times.AddToTail( baseKeyTime );
			++nBaseKeyIndex;
			baseKeyTime = ( nBaseKeyIndex < nBaseKeyCount ) ? pBaseLayer->GetKeyTime( nBaseKeyIndex ) : DMETIME_MAXTIME;
		}
		
		while ( ( targetKeyTime <= baseKeyTime ) && ( targetKeyTime < tsEndTime ) )
		{
			times.AddToTail( targetKeyTime );
			++nTargetKeyIndex;
			targetKeyTime = ( nTargetKeyIndex < nTargetKeyCount ) ? pTargetLayer->GetKeyTime( nTargetKeyIndex ) : DMETIME_MAXTIME;
		}
	}


	// Add sample times in the falloff regions if the falloff type is not linear so that the falloff type will
	// be properly represented even if neither the base or the target layer have samples in the falloff region.
	if ( params.m_nFalloffInterpolatorTypes[ 0 ] != INTERPOLATE_LINEAR_INTERP )
	{	
		InterleaveSampleTimes( times, nNumProceedingTimes, timeSelection[ TS_LEFT_FALLOFF ], timeSelection[ TS_LEFT_HOLD ], params.m_nResampleInterval );
	}

	if ( params.m_nFalloffInterpolatorTypes[ 1 ] != INTERPOLATE_LINEAR_INTERP )
	{
		// Find the index of the time which is the first time after the right edge of the hold region of the time selection.
		int nIndex = times.Count();
		while ( nIndex > 0 )
		{
			if ( times[ nIndex - 1 ] <= timeSelection[ TS_RIGHT_HOLD ] )
				break;
			--nIndex;
		}

		if ( nIndex < times.Count() )
		{
			DmeTime_t testTime = times[ nIndex ];
			Assert( testTime > timeSelection[ TS_RIGHT_HOLD ] );
		}

		InterleaveSampleTimes( times, nIndex, timeSelection[ TS_RIGHT_HOLD ], timeSelection[ TS_RIGHT_FALLOFF ], params.m_nResampleInterval );
	}


	// Blend between the keys from both layers within the time selection
	int nNumTimes = times.Count();
	for ( int iTime = values.Count(); iTime < nNumTimes; ++iTime )
	{
		currentTime = times[ iTime ];

		float frac = params.GetAmountForTime( currentTime ) * params.m_flIntensity;
		
		T targetValue = pTargetLayer->GetValue( currentTime );
		T baseValue = pBaseLayer->GetValue( currentTime );
		T newValue;
		if ( frac < 1.0f )
		{				
			newValue = Interpolate( frac, baseValue, targetValue, params.m_nComponentFlags );
		}
		else
		{
			newValue = ::MaskValue( targetValue, baseValue, params.m_nComponentFlags );
		}

		values.AddToTail( newValue );
	}
	
	Assert( values.Count() == times.Count() );


	// Copy all the keys from the destination layer following the time selection
	while ( nBaseKeyIndex < nBaseKeyCount )
	{
		currentTime = pBaseLayer->GetKeyTime( nBaseKeyIndex );
		if ( currentTime > tsEndTime )
			break;

		T keyValue = pBaseLayer->GetKeyValue( nBaseKeyIndex );
		++nBaseKeyIndex;

		times.AddToTail( currentTime );
		values.AddToTail( keyValue );
	}

	// Update the destination layer
	pOutputLayer->SetAllKeys( times, values );
	

	// Remove redundant keys
	if ( g_pDmElementFramework->GetPhase() == PH_EDIT )
	{
		pOutputLayer->RemoveRedundantKeys( params.m_flThreshold, false );
	}

	// Add samples a the time selection times
	for ( int i = 0; i < TS_TIME_COUNT; ++i )
	{	
		currentTime = timeSelection[ i ];	

		float frac = params.GetAmountForTime( currentTime ) * params.m_flIntensity;
		T targetValue = pTargetLayer->GetValue( currentTime );
		T baseValue = pBaseLayer->GetValue( currentTime );
		T newValue = Interpolate( frac, baseValue, targetValue, params.m_nComponentFlags );

		DmeTime_t nextTime = ((i+1) < TS_TIME_COUNT) ? timeSelection[i+1] : DMETIME_INVALID;

		SegmentInterpolation_t interpSetting = (pBaseLayer->GetSegmentInterpolationSetting( currentTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) || 
			(pTargetLayer->GetSegmentInterpolationSetting( currentTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

		pOutputLayer->InsertKey( currentTime, newValue, interpSetting );	
	}

}


template< class T > 
void CDmeTypedLog< T >::RevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, const CDmeLogLayer *savedLayer )
{
	const CDmeTypedLogLayer< T > *pTargetLayer = static_cast< const CDmeTypedLogLayer< T > * >( savedLayer );
	if ( !pTargetLayer )
		return;

	Assert( GetNumLayers() >= 2 );
	int nTopLayerIndex = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nTopLayerIndex <= 0 )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = static_cast< CDmeTypedLogLayer< T > * >( GetLayer( nTopLayerIndex ) );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	const CDmeTypedLogLayer< T > *pBaseLayer = static_cast< CDmeTypedLogLayer< T > * >( GetLayer( 0 ) );
	Assert( pBaseLayer );
	if ( !pBaseLayer )
		return;
		
	BlendLayersUsingTimeSelection( params, pBaseLayer, pTargetLayer, pWriteLayer );

}


template< class T > 
void CDmeTypedLog< T >::RecaleAndRevealUsingTimeSelection( const DmeLog_TimeSelection_t &params,  TimeSelection_t &sourceTimeSelection, const CDmeLogLayer *pTargetLayer )
{
	CDisableUndoScopeGuard disableUndoSg;
	
	CDmeTypedLogLayer< T > *pNewLayer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer< T >( this ) );
	if ( !pNewLayer )
		return;
	
	pNewLayer->CopyLayer( pTargetLayer );
	pNewLayer->RescaleSamplesInTimeSelection( sourceTimeSelection, params.m_nTimes );

	{
		//CEnableUndoScopeGuard enableUndoSg;
		RevealUsingTimeSelection( params, pNewLayer );
	}

	g_pDataModel->DestroyElement( pNewLayer->GetHandle() );
}


template< class T > 
void RandomValue( IUniformRandomStream &random, const T& average, const T& oldValue, T& newValue )
{
	newValue = oldValue;
}

template<> void RandomValue( IUniformRandomStream &random, const Vector& average, const Vector& oldValue, Vector& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 3; ++i )
	{
		newValue[ i ] += random.RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( IUniformRandomStream &random, const Quaternion& average, const Quaternion& oldValue, Quaternion& newValue )
{
	QAngle newAngle;
	QuaternionAngles( oldValue, newAngle );

	QAngle avgA;
	QuaternionAngles( average, avgA );

	for ( int i = 0; i < 3; ++i )
	{
		newAngle[ i ] += random.RandomFloat( -fabs( avgA[ i ] ), fabs( avgA[ i ] ) );
	}
	AngleQuaternion( newAngle, newValue );
}

template<> void RandomValue( IUniformRandomStream &random, const Vector4D& average, const Vector4D& oldValue, Vector4D& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 4; ++i )
	{
		newValue[ i ] += random.RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( IUniformRandomStream &random, const Vector2D& average, const Vector2D& oldValue, Vector2D& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 2; ++i )
	{
		newValue[ i ] += random.RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( IUniformRandomStream &random, const float& average, const float& oldValue, float& newValue )
{
	newValue = oldValue + random.RandomFloat( -average, average );
}

template<> void RandomValue( IUniformRandomStream &random, const int& average, const int& oldValue, int& newValue )
{
	newValue = oldValue + random.RandomInt( -average, average );
}

template<> void RandomValue( IUniformRandomStream &random, const DmeTime_t& average, const DmeTime_t& oldValue, DmeTime_t& newValue )
{
	int n = average.GetTenthsOfMS();
	newValue = oldValue + DmeTime_t( random.RandomInt( -n, n ) );
}


// Builds a layer with samples matching the times in reference layer, from the data in pDataLayer, putting the resulting keys into pOutputLayer
template< class T >
void CDmeTypedLog< T >::BuildCorrespondingLayer( const CDmeLogLayer *pReferenceLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer )
{
	const CDmeTypedLogLayer< T > *ref = static_cast< const CDmeTypedLogLayer< T > * >( pReferenceLayer );
	const CDmeTypedLogLayer< T > *data = static_cast< const CDmeTypedLogLayer< T > * >( pDataLayer );
	CDmeTypedLogLayer< T > *out = static_cast< CDmeTypedLogLayer< T > * >( pOutputLayer );

	if ( !ref || !data || !out )
	{
		Assert( 0 );
		return;
	}

	bool usecurvetypes = ref->IsUsingCurveTypes();

	out->ClearKeys();
	int kc = ref->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = ref->GetKeyTime( i );
		T value = data->GetValue( keyTime );

		DmeTime_t nextTime = ((i+1) < kc) ? ref->GetKeyTime(i+1) : DMETIME_INVALID;

		out->InsertKey( keyTime, value, data->GetSegmentInterpolationSetting( keyTime, nextTime, true ), usecurvetypes ? GetDefaultCurveType() : CURVE_DEFAULT );
	}
}

template< class T >
void CDmeTypedLog< T >::StaggerUsingTimeSelection( const DmeLog_TimeSelection_t& params, DmeTime_t tStaggerAmount, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer )
{
	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( pWriteLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( pBaseLayer );
	if ( !baseLayer )
		return;

	writeLayer->ClearKeys();

	DmeLog_TimeSelection_t newParams;
	newParams = params;

	// Move the hold area by the stagger amount
	float flScaleFactor[ 2 ] = { 1.0f, 1.0f };

	newParams.m_nTimes[ TS_LEFT_HOLD ] += tStaggerAmount;
	newParams.m_nTimes[ TS_RIGHT_HOLD ] += tStaggerAmount;

	for ( int i = 0; i < 2 ; ++i )
	{
		DmeTime_t dt = params.m_nTimes[ 2 * i + 1 ] - params.m_nTimes[ 2 * i ];
		if ( dt > DMETIME_ZERO )
		{
			DmeTime_t newDt = newParams.m_nTimes[ 2 * i + 1 ] - newParams.m_nTimes[ 2 * i ];
			flScaleFactor[ i ] = newDt / dt;
		}
	}

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t curtime = baseLayer->GetKeyTime( i );
		T oldValue = baseLayer->GetKeyValue( i );

		// Classify time
		if ( curtime <= params.m_nTimes[ TS_LEFT_HOLD ] )
		{
			curtime = curtime * flScaleFactor[ 0 ];
		}
		else if ( curtime >= params.m_nTimes[ TS_RIGHT_HOLD ] )
		{
			curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ] - ( params.m_nTimes[ TS_RIGHT_FALLOFF ] - curtime ) * flScaleFactor[ 1 ];
		}
		else
		{
			curtime += tStaggerAmount;
		}

		writeLayer->InsertKey( curtime, oldValue, baseLayer->GetSegmentInterpolationSetting(i), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT );
	}
}

// returns -1 if no elements in vector are less than value
template< class T >
int FindLessInSortedVector( const T &value, const CUtlVector< T > &vector )
{
	int n = vector.Count();
	for ( int i = 0; i < n; ++i )
	{
		if ( vector[ i ] >= value )
			return i - 1;
	}
	return n - 1;
}

template< class T >
void CDmeTypedLog< T >::GenerateSplineUsingTimeSelection( const DmeLog_TimeSelection_t& params, const CUtlVector< DmeTime_t > &sortedSplineKeyTimes, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer )
{
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval <= DmeTime_t( 0 ) )
		return;

	const CDmeTypedLogLayer< T > *pBaseLayer = static_cast< const CDmeTypedLogLayer< T > * >( baseLayer );
	Assert( pBaseLayer );
	if ( !pBaseLayer )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = static_cast< CDmeTypedLogLayer< T > * >( writeLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	int nSplineKeys = sortedSplineKeyTimes.Count();
	if ( nSplineKeys == 0 )
		return;

	pWriteLayer->ClearKeys();

	int nKeys = pBaseLayer->GetKeyCount();
	for ( int i = 0; i < nKeys; ++i )
	{
		DmeTime_t t = pBaseLayer->GetKeyTime( i );

		int si = FindLessInSortedVector( t, sortedSplineKeyTimes );

		if ( si < 0 || si > nSplineKeys - 2 )
		{
			T val = pBaseLayer->GetValue( t );
			pWriteLayer->InsertKey( t, val, pBaseLayer->GetSegmentInterpolationSetting(i), CURVE_DEFAULT );
			continue;
		}

		DmeTime_t times[ 4 ];
		times[ 1 ] = sortedSplineKeyTimes[ si ];
		times[ 2 ] = sortedSplineKeyTimes[ si + 1 ];
		times[ 0 ] = si < 1				  ? times[ 1 ] - DMETIME_MINTIME : sortedSplineKeyTimes[ si - 1 ];
		times[ 3 ] = si > nSplineKeys - 3 ? times[ 2 ] + DMETIME_MINTIME : sortedSplineKeyTimes[ si + 2 ];

		SegmentInterpolation_t interpSetting = pBaseLayer->GetSegmentInterpolationSetting( times[0], times[3], true );
		if( interpSetting == SEGMENT_INTERPOLATE )
		{
			T values[ 4 ];
			values[ 0 ] = pBaseLayer->GetValue( times[ 0 ] );
			values[ 1 ] = pBaseLayer->GetValue( times[ 1 ] );
			values[ 2 ] = pBaseLayer->GetValue( times[ 2 ] );
			values[ 3 ] = pBaseLayer->GetValue( times[ 3 ] );

			int curveTypes[ 4 ] = { CURVE_CATMULL_ROM_TO_CATMULL_ROM, CURVE_CATMULL_ROM_TO_CATMULL_ROM, CURVE_CATMULL_ROM_TO_CATMULL_ROM, CURVE_CATMULL_ROM_TO_CATMULL_ROM };

			Assert( t >= times[ 1 ] && t <= times[ 2 ] );
			float frac = GetFractionOfTimeBetween( t, times[ 1 ], times[ 2 ] );
			T val = Curve_Interpolate( frac, times, values, curveTypes, -FLT_MAX, FLT_MAX );
			pWriteLayer->InsertKey( t, val, interpSetting, CURVE_DEFAULT );
		}
		else
		{
			//we pass through a non-interpolated segment for this range of the spline, easy solution is to continue our noninterpolation. Probably needs rethinking
			T val = pBaseLayer->GetValue( t );
			pWriteLayer->InsertKey( t, val, interpSetting, CURVE_DEFAULT );
		}
	}
}

template< class T >
void CDmeTypedLog< T >::CopySamplesFromPreset( const DmeLog_TimeSelection_t& params, const CDmAttribute *pPresetValue, const CDmAttribute *pPresetTimes, DmeTime_t tLogTimeOffset, const CDmeChannelsClip *pChannelsClip, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer )
{
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval <= DmeTime_t( 0 ) )
		return;

	const CDmeTypedLogLayer< T > *pBaseLayer = static_cast< const CDmeTypedLogLayer< T > * >( baseLayer );
	Assert( pBaseLayer );
	if ( !pBaseLayer )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = static_cast< CDmeTypedLogLayer< T > * >( writeLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	CDmrArrayConst< T         > presetValues( pPresetValue );
	CDmrArrayConst< DmeTime_t > presetTimes ( pPresetTimes );

	int nPresetKeys = presetTimes.Count();
	Assert( nPresetKeys != 0 && nPresetKeys == presetValues.Count() );
	if ( nPresetKeys == 0 || nPresetKeys != presetValues.Count() )
		return;

	pWriteLayer->ClearKeys();

	int nKeys = pBaseLayer->GetKeyCount();
	for ( int i = 0; i < nKeys; ++i )
	{
		DmeTime_t tLog = pBaseLayer->GetKeyTime( i );

		DmeTime_t tShot = pChannelsClip->FromChildMediaDuration( tLog - tLogTimeOffset ); // presetTimes are relative to the head, and in shot time

		int ti = FindLessInSortedVector( tShot, presetTimes.Get() );
		if ( ti < 0 )
		{
			pWriteLayer->InsertKey( tLog, presetValues[ 0 ], pBaseLayer->GetSegmentInterpolationSetting(i), CURVE_DEFAULT );
			continue;
		}
		if ( ti >= nPresetKeys - 1 )
		{
			pWriteLayer->InsertKey( tLog, presetValues[ nPresetKeys - 1 ], pBaseLayer->GetSegmentInterpolationSetting(i), CURVE_DEFAULT );
			continue;
		}

		float frac = GetFractionOfTimeBetween( tShot, presetTimes[ ti ], presetTimes[ ti + 1 ] );
		T value = Interpolate( frac, presetValues[ ti ], presetValues[ ti + 1 ] );
		pWriteLayer->InsertKey( tLog, value, pBaseLayer->GetSegmentInterpolationSetting(i), CURVE_DEFAULT );
	}
}

template< class T >
void DumpLayers( const CDmeTypedLogLayer< T > *baseLayer, CDmeTypedLogLayer< T > *writeLayer )
{
}

template<>
void DumpLayers< Vector >( const CDmeTypedLogLayer< Vector > *baseLayer, CDmeTypedLogLayer< Vector > *writeLayer )
{
	int kc = baseLayer->GetKeyCount();
	if ( kc != writeLayer->GetKeyCount() )
	{
		return;
	}

	for ( int i = 0; i < kc; ++i )
	{
		Vector v1 = baseLayer->GetKeyValue( i );
		Vector v2 = writeLayer->GetKeyValue( i );

		DmeTime_t t1 = baseLayer->GetKeyTime( i );
		DmeTime_t t2 = writeLayer->GetKeyTime( i );

		float spd1 = 0.0f;
		float spd2 = 0.0f;

		if ( i > 0 )
		{
			DmeTime_t dt1 = t1 - baseLayer->GetKeyTime( i - 1 );
			DmeTime_t dt2 = t2 - writeLayer->GetKeyTime( i - 1 );

			Vector d1 = v1 - baseLayer->GetKeyValue( i - 1 );
			Vector d2 = v2 - writeLayer->GetKeyValue( i - 1 );

			if ( dt1 > DMETIME_ZERO )
			{
				spd1 = d1.Length() / dt1.GetSeconds();
			}

			if ( dt2 > DMETIME_ZERO )
			{
				spd2 = d2.Length() / dt2.GetSeconds();
			}
		}
		Msg( "%i:  %d %d v1 %.3f v2 %.3f = (%f %f %f) (%f %f %f)\n",
			i, t1.GetTenthsOfMS(), t2.GetTenthsOfMS(),
			spd1, spd2, VectorExpand( v1 ), VectorExpand( v2 ) );
	}
}

template< class T >
void CDmeTypedLog< T >::HoldOrReleaseUsingTimeSelection( const DmeLog_TimeSelection_t& params, bool bHold, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer )
{
	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( pWriteLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( pBaseLayer );
	if ( !baseLayer )
		return;

	writeLayer->ClearKeys();

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t curtime = baseLayer->GetKeyTime( i );

		DmeTime_t targetTime = curtime;
		DmeTime_t baseTime = curtime;

		if ( curtime < params.m_nTimes[ TS_LEFT_HOLD ] && curtime > params.m_nTimes[ TS_LEFT_FALLOFF ] )
		{
			targetTime = params.m_nTimes[ TS_LEFT_HOLD ];
			baseTime = params.m_nTimes[ TS_LEFT_FALLOFF ];
		}
		else if ( curtime > params.m_nTimes[ TS_RIGHT_HOLD ] && curtime < params.m_nTimes[ TS_RIGHT_FALLOFF ] )
		{
			targetTime = params.m_nTimes[ TS_RIGHT_HOLD ];
			baseTime = params.m_nTimes[ TS_RIGHT_FALLOFF ];
		}

		float scale = params.GetAmountForTime( curtime );
		if ( bHold )
		{
			scale = scale * scale;
		}
		else
		{
			scale = sqrt( scale );
		}
		DmeTime_t keyTime = Lerp( scale, baseTime, targetTime );

		if ( i > 0 && keyTime <= writeLayer->GetKeyTime( i - 1 ) )
		{
			keyTime = writeLayer->GetKeyTime( i - 1 ) + DMETIME_MINTIME; // unlikely, but just in case, since otherwise the blend fails if the writeLayer has a missing key
		}

		writeLayer->InsertKey( keyTime, baseLayer->GetKeyValue( i ), baseLayer->GetSegmentInterpolationSetting( i ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
	}

	Assert( writeLayer->GetKeyCount() == kc );
}

#ifdef MSVC
#pragma warning( push )
// Potential division by zero... because flTotalDist is initialized to 0 and then used later as the divisor but
// compiled just looks at static values.  The loop after flTotalDist is initialized will make it non-zero before
// it's used as a divisor, so ignore the warning.  Warning disable has to be here because warnings in the range
// [4700, 4799] cannot be changed in the scope of a function
#pragma warning( disable : 4723 )
#endif // ifdef MSVC

template< class T >
void CDmeTypedLog< T >::SteadyUsingTimeSelection( const DmeLog_TimeSelection_t& params, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer )
{
	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( pWriteLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( pBaseLayer );
	if ( !baseLayer )
		return;

	writeLayer->ClearKeys();

	// Try to smooth out the spatial "velocity" over the time selection

	int keyCount = baseLayer->GetKeyCount();
	float flTotalDist = 0.0f;
	for ( int i = 1; i < keyCount; ++i )
	{
		const T &v0 = baseLayer->GetKeyValue( i - 1 );
		const T &v1 = baseLayer->GetKeyValue( i );
		float flDistance = LengthOf( Subtract( v0, v1 ) );
		flTotalDist += flDistance;
	}

	if ( keyCount <= 2 || flTotalDist == 0.0f )
	{
		for ( int i = 0; i < keyCount ; ++i )
		{
			writeLayer->InsertKey( baseLayer->GetKeyTime( i ), baseLayer->GetKeyValue( i ), baseLayer->GetSegmentInterpolationSetting( i ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
		}
		return;
	}

	DmeTime_t t0 = baseLayer->GetKeyTime( 0 );
	DmeTime_t tn = baseLayer->GetKeyTime( keyCount - 1 );

	DmeTime_t tTotalTime = tn - t0;

	int nCurveType = IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT;

	writeLayer->InsertKey( t0, baseLayer->GetKeyValue( 0 ), baseLayer->GetSegmentInterpolationSetting( 0 ), nCurveType, true ); 

	float flRunningDist = 0.0f;
	for ( int i = 1; i < keyCount - 1; ++i )
	{
		const T &v0 = baseLayer->GetKeyValue( i - 1 );
		const T &v1 = baseLayer->GetKeyValue( i );
		float flDistance = LengthOf( Subtract( v0, v1 ) );
		flRunningDist += flDistance;

		DmeTime_t t = t0 + tTotalTime * ( flRunningDist / flTotalDist );

		writeLayer->InsertKey( t, v1, baseLayer->GetSegmentInterpolationSetting( i - 1, i ), nCurveType, true ); 
	}

	writeLayer->InsertKey( tn, baseLayer->GetKeyValue( keyCount - 1 ), baseLayer->GetSegmentInterpolationSetting( keyCount - 1 ), nCurveType, true ); 
	
	Assert( writeLayer->GetKeyCount() == keyCount );
}


#ifdef MSVC
#pragma warning( pop )
#endif // ifdef MSVC


template< class T >
void CDmeTypedLog< T >::FilterUsingTimeSelection( IUniformRandomStream &random, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff )
{
	Assert( GetNumLayers() >= 2 );
	int bestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( bestLayer <= 0 )
		return;

	CDmeTypedLogLayer< T > *writeLayer = GetLayer( bestLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	CDmeTypedLogLayer< T > *baseLayer = GetLayer( 0 );
	if ( !baseLayer )
		return;

	FilterUsingTimeSelection( random, 1.0f, params, filterType, bResample, bApplyFalloff, baseLayer, writeLayer );
}

template< class T >
void CDmeTypedLog< T >::FilterUsingTimeSelection( IUniformRandomStream &random, float flScale, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer )
{
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval <= DmeTime_t( 0 ) )
		return;

	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( pWriteLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( pBaseLayer );
	if ( !baseLayer )
		return;

	writeLayer->ClearKeys();

	DmeTime_t resample = 0.5f * params.m_nResampleInterval;

	switch ( filterType )
	{
	default:
	case FILTER_SMOOTH:
		{
			int t;
			if ( bResample )
			{
				for ( t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
				{
					DmeTime_t curtime = DmeTime_t( t );
					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];
	
					T curValue = baseLayer->GetValue( curtime );
					writeLayer->SetKey( curtime, curValue, baseLayer->GetSegmentInterpolationSetting( curtime, curtime + resample, true ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT );
				}
			}
			else
			{
				// Do a second pass where we bias the keys in the falloff area back toward the original value
				int kc = baseLayer->GetKeyCount();
				for ( int i = 0; i < kc; ++i )
				{
					DmeTime_t curtime = baseLayer->GetKeyTime( i );
					if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
						continue;

					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						continue;

					T oldValue = baseLayer->GetKeyValue( i );
					writeLayer->InsertKey( curtime, oldValue, baseLayer->GetSegmentInterpolationSetting( curtime ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT );
				}
			}

			writeLayer->Filter2( params.m_nResampleInterval * 0.95f * flScale );

			if ( bApplyFalloff )
			{
				if ( bResample )
				{
					// Do a second pass where we bias the keys in the falloff area back toward the original value
					for ( t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
					{
						DmeTime_t curtime = DmeTime_t( t );
						if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
							curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];

						T oldValue = baseLayer->GetValue( curtime );

						if ( curtime >= params.m_nTimes[ TS_LEFT_HOLD ] && curtime <= params.m_nTimes[ TS_RIGHT_HOLD ] )
							continue;

						// Modulate these keys back down toward the original value
						T newValue = writeLayer->GetValue( curtime );

						float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

						newValue = Interpolate( frac, oldValue, newValue );

						// Overwrite key
						writeLayer->InsertKey( curtime, newValue, baseLayer->GetSegmentInterpolationSetting( curtime, curtime + resample, true ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT );
					}

				}
				else
				{
					// Do a second pass where we bias the keys in the falloff area back toward the original value
					int kc = writeLayer->GetKeyCount();
					for ( int i = 0; i < kc; ++i )
					{
						DmeTime_t curtime = writeLayer->GetKeyTime( i );
						if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
							continue;

						if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
							continue;

						if ( curtime >= params.m_nTimes[ TS_LEFT_HOLD ] && curtime <= params.m_nTimes[ TS_RIGHT_HOLD ] )
							continue;

						T oldValue = baseLayer->GetValue( curtime );

						// Modulate these keys back down toward the original value
						T newValue = writeLayer->GetValue( curtime );

						float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

						newValue = Interpolate( frac, oldValue, newValue );

						DmeTime_t nextTime = ((i + 1) < kc) ? writeLayer->GetKeyTime(i + 1) : DMETIME_INVALID;

						//don't interpolate if either of the two layers think they shouldn't
						SegmentInterpolation_t interpSetting = ((baseLayer->GetSegmentInterpolationSetting( curtime, nextTime, true ) == SEGMENT_NOINTERPOLATE) ||
																(writeLayer->GetSegmentInterpolationSetting( i ) == SEGMENT_NOINTERPOLATE)) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

						// Overwrite key
						writeLayer->InsertKey( curtime, newValue, interpSetting, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
					}
				}
			}

			if ( bResample )
			{
				writeLayer->RemoveRedundantKeys( params.m_flThreshold, false );
			}
		}
		break;
	case FILTER_INOUT:
		{
			// Compute average value in entire log
			DmeTime_t tIn = params.m_nTimes[ TS_LEFT_HOLD ];
			DmeTime_t tOut = params.m_nTimes[ TS_RIGHT_HOLD ];

			if ( tIn != tOut )
			{
				T inValue = baseLayer->GetValue( tIn );
				T outValue = baseLayer->GetValue( tOut );

				if ( bResample )
				{
					// Do a second pass where we bias the keys in the falloff area back toward the original value
					for ( int t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
					{
						DmeTime_t curtime = DmeTime_t( t );
						if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
							curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];

						if ( curtime < tIn || curtime > tOut )
						{
							T oldValue = baseLayer->GetValue( curtime );
							writeLayer->InsertKey( curtime, oldValue, baseLayer->GetSegmentInterpolationSetting( curtime, curtime + resample, true ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
						}
						else
						{
							float frac = GetFractionOfTimeBetween( curtime, tIn, tOut, true );
							T newValue = Interpolate( frac, inValue, outValue );
							writeLayer->InsertKey( curtime, newValue, baseLayer->GetSegmentInterpolationSetting( curtime, curtime + resample, true ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
						}
					}
				}
				else
				{
					int kc = baseLayer->GetKeyCount();
					for ( int i = 0; i < kc; ++i )
					{
						DmeTime_t curtime = baseLayer->GetKeyTime( i );
						if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
							continue;

						if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
							continue;

						if ( curtime < tIn || curtime > tOut )
						{
							T oldValue = baseLayer->GetValue( curtime );
							writeLayer->InsertKey( curtime, oldValue, baseLayer->GetSegmentInterpolationSetting( i ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
						}
						else
						{
							float frac = GetFractionOfTimeBetween( curtime, tIn, tOut, true );
							T newValue = Interpolate( frac, inValue, outValue );
							writeLayer->InsertKey( curtime, newValue, baseLayer->GetSegmentInterpolationSetting( i ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
						}
					}
				}
			}
		}
		break;
	case FILTER_JITTER:
		{
			// Compute average value in entire log
			Assert( !baseLayer->IsCompressed() );
			
			T average = Average( baseLayer->m_values.Base(), baseLayer->m_values.Count() );
			average = ScaleValue( average, 0.05f * flScale );

			if ( bResample )
			{
				int t;
				for ( t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
				{
					DmeTime_t curtime = DmeTime_t( t );
					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];

					float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

					T oldValue = baseLayer->GetValue( curtime );

					T newValue;
					RandomValue( random, average, oldValue, newValue );

					if ( frac != 1.0f )
					{
						newValue = Interpolate( frac, oldValue, newValue );
					}

					writeLayer->SetKey( curtime, newValue, baseLayer->GetSegmentInterpolationSetting( curtime, curtime + resample, true ), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
				}
				
			}
			else
			{
				int kc = baseLayer->GetKeyCount();
				for ( int i = 0; i < kc; ++i )
				{
					DmeTime_t curtime = baseLayer->GetKeyTime( i );
					if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
						continue;

					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						continue;

					float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

					T oldValue = baseLayer->GetValue( curtime );

					T newValue;
					RandomValue( random, average, oldValue, newValue );

					if ( frac != 1.0f )
					{
						newValue = Interpolate( frac, oldValue, newValue );
					}

					writeLayer->InsertKey( curtime, newValue, baseLayer->GetSegmentInterpolationSetting(i), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
				}
			}
		}
		break;
	case FILTER_SHARPEN:
	case FILTER_SOFTEN:
		{
			writeLayer->ClearKeys();

			bool bSharpen = filterType == FILTER_SHARPEN;
			int kc = baseLayer->GetKeyCount();
			for ( int i = 0; i < kc; ++i )
			{
				DmeTime_t curtime = baseLayer->GetKeyTime( i );
				if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
					continue;

				if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
					continue;

				float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

				T oldValue = baseLayer->GetValue( curtime );

				T newValue = oldValue;
				if ( frac != 1.0f )
				{
					T crossingValue[ 2 ] = { oldValue, oldValue };
					if ( curtime <= params.m_nTimes[ TS_LEFT_HOLD ] )
					{
						// Get the value at the crossing point (either green edge for sharpen, or left edge for soften...)
						crossingValue[ 0 ] = baseLayer->GetValue( params.m_nTimes[ TS_LEFT_FALLOFF ] );
						crossingValue[ 1 ] = baseLayer->GetValue( params.m_nTimes[ TS_LEFT_HOLD ] );
					}
					else if ( curtime >= params.m_nTimes[ TS_RIGHT_HOLD ] )
					{
						crossingValue[ 0 ] = baseLayer->GetValue( params.m_nTimes[ TS_RIGHT_FALLOFF ] );
						crossingValue[ 1 ] = baseLayer->GetValue( params.m_nTimes[ TS_RIGHT_HOLD ] );
					}
					else
					{
						Assert( 0 );
					}

					T dynamicRange = Subtract( crossingValue[ 1 ], crossingValue[ 0 ] );

					int iType = bSharpen ? INTERPOLATE_EASE_IN : INTERPOLATE_EASE_OUT;

					float flOut = ComputeInterpolationFactor( frac, iType );
					float flBias = clamp( flOut, 0.0f, 1.0f );
					float dFrac = flScale * ( frac - flBias );

					newValue = Add( oldValue, ScaleValue( dynamicRange, dFrac ) );
				}

				writeLayer->InsertKey( curtime, newValue, baseLayer->GetSegmentInterpolationSetting(i), IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
			}
		}
		break;
	}
}

template< class T >
void CDmeTypedLog< T >::PasteAndRescaleSamples( 
	const CDmeLogLayer *pBase, 
	const CDmeLogLayer *pDataLayer, 
	CDmeLogLayer *pOutputLayer, 
	const DmeLog_TimeSelection_t& srcParams, 
	const DmeLog_TimeSelection_t& destParams, 
	bool bBlendAreaInFalloffRegion,
	bool bReverse )
{
	Assert( GetNumLayers() >= 2 );
	if ( GetNumLayers() < 2 )
		return;

	CDmeTypedLogLayer< T > *pClipboard = CastElement< CDmeTypedLogLayer< T > >( const_cast< CDmeLogLayer * >( pDataLayer ) );

	// Could have passed in layer with wrong attribute type?!
	Assert( pClipboard );
	if ( !pClipboard )
		return;

	CDmeTypedLogLayer< T > *pBaseLayer = CastElement< CDmeTypedLogLayer< T > >( const_cast< CDmeLogLayer * >( pBase ) );
	CDmeTypedLogLayer< T > *pWriteLayer = CastElement< CDmeTypedLogLayer< T > >( pOutputLayer );
	Assert( pBaseLayer );
	Assert( pWriteLayer );

	TimeSelection_t tSrcTimes;
	tSrcTimes[ TS_LEFT_FALLOFF ]  = DmeTime_t( 0 );
	tSrcTimes[ TS_LEFT_HOLD ]	  = srcParams.m_nTimes[ 1 ] - srcParams.m_nTimes[ 0 ];
	tSrcTimes[ TS_RIGHT_HOLD ]	  = srcParams.m_nTimes[ 2 ] - srcParams.m_nTimes[ 0 ];
	tSrcTimes[ TS_RIGHT_FALLOFF ] = srcParams.m_nTimes[ 3 ] - srcParams.m_nTimes[ 0 ];

	const TimeSelection_t &tDstTimes = destParams.m_nTimes;

	DmeTime_t tFirstKey = tDstTimes[ 0 ] != tDstTimes[ 1 ] ? tDstTimes[ 0 ] : tDstTimes[ 0 ] - DMETIME_MINDELTA;
	pWriteLayer->InsertKey( tFirstKey, pBaseLayer->GetValue( tFirstKey ), pBaseLayer->GetSegmentInterpolationSetting( tFirstKey ) );

	DmeTime_t tLastKey = tDstTimes[ 3 ] != tDstTimes[ 2 ] ? tDstTimes[ 3 ] : tDstTimes[ 3 ] + DMETIME_MINDELTA;
	pWriteLayer->InsertKey( tLastKey, pBaseLayer->GetValue( tLastKey ), pBaseLayer->GetSegmentInterpolationSetting( tLastKey ) );

	DmeTime_t tLastWrittenTime = tFirstKey;

	LogComponents_t nComponentFlags = destParams.m_nComponentFlags;

	CLogTimeIterator< T > it( pClipboard, bReverse );
	
	it.AddLayer();

	// Remap the base layer keys into src layer timespace, so that we'll preserve the base layer samples after remapping them back out
	for ( int i = 0; i < pBaseLayer->GetKeyCount(); ++i )
	{
		DmeTime_t tBaseKeyTime = pBaseLayer->GetKeyTime( i );

			// Remap FROM dest TO src time!!!
		DmeTime_t tSrcKeyTime = RemapTime( tBaseKeyTime, tDstTimes, tSrcTimes );

		// Can't possibly matter?
		if ( tSrcKeyTime < tSrcTimes[ TS_LEFT_FALLOFF ] || 
			tSrcKeyTime > tSrcTimes[ TS_RIGHT_FALLOFF ] )
			continue;

		it.AddKeyTime( tSrcKeyTime );
	}


	for( int i = it.First() ; i != it.InvalidIndex(); i = it.Next( i ) )
	{
		DmeTime_t tKeyTime = it.GetKeyTime( i );
		T val;
		it.GetValue( i, val ); 
		int nSrcTSI = 1;
		if ( tKeyTime < tSrcTimes[ TS_LEFT_HOLD ] )
		{
			if ( tKeyTime <= tSrcTimes[ TS_LEFT_FALLOFF ] )
				continue;
			nSrcTSI = 0;
		}
		else if ( tKeyTime > tSrcTimes[ TS_RIGHT_HOLD ] )
		{
			if ( tKeyTime >= tSrcTimes[ TS_RIGHT_FALLOFF ] )
				continue;
			nSrcTSI = 2;
		}
		int nDstTSI = ( bReverse ? ( 2 - nSrcTSI ) : nSrcTSI );
		bool bHold = nSrcTSI == 1;

		DmeTime_t tSrcDuration = tSrcTimes[ nSrcTSI + 1 ] - tSrcTimes[ nSrcTSI ];
		DmeTime_t tDstDuration = tDstTimes[ nDstTSI + 1 ] - tDstTimes[ nDstTSI ];
		if ( !bHold && tDstDuration == DMETIME_ZERO )
			continue;

		DmeTime_t tDstTime;
		if ( tKeyTime == tSrcTimes[ TS_LEFT_HOLD ] )
		{
			tDstTime = bReverse ? tDstTimes[ TS_RIGHT_HOLD ] : tDstTimes[ TS_LEFT_HOLD ];
		}
		else if ( tKeyTime == tSrcTimes[ TS_RIGHT_HOLD ] )
		{
			tDstTime = bReverse ? tDstTimes[ TS_LEFT_HOLD ] : tDstTimes[ TS_RIGHT_HOLD ];
		}
		else
		{
			float flRatio = MIN( 1.0f, ( tKeyTime - tSrcTimes[ nSrcTSI ] ).GetSeconds() / tSrcDuration.GetSeconds() );
			flRatio = bReverse ? ( 1.0f - flRatio ) : flRatio;
			tDstTime = tDstTimes[ nDstTSI ] + flRatio * tDstDuration;

			if ( tDstTime < tLastWrittenTime + DMETIME_MINDELTA )
			{
				tDstTime = tLastWrittenTime + DMETIME_MINDELTA;
			}
			if ( tDstTime > tDstTimes[ nDstTSI + 1 ] - DMETIME_MINDELTA )
			{
				tDstTime = tDstTimes[ nDstTSI + 1 ] - DMETIME_MINDELTA;
			}

			if ( bBlendAreaInFalloffRegion && !bHold )
			{
				flRatio = destParams.AdjustFactorForInterpolatorType( flRatio, nDstTSI != 0 ? 1 : 0 );
				T baseValue = pBaseLayer->GetValue( tDstTime );
				val = Interpolate( nDstTSI == 0 ? flRatio : 1.0f - flRatio, baseValue, val );
			}
		}

		// Have the layer itself do the masking!!!
		T maskedVal = pBaseLayer->MaskValue( tDstTime, val, nComponentFlags );
		
		int iNext = it.Next( i );
		iNext = (iNext != it.InvalidIndex()) ? iNext : i;
		
		pWriteLayer->InsertKey( tDstTime, maskedVal, pClipboard->GetSegmentInterpolationSetting( i, iNext ) );
		tLastWrittenTime = tDstTime;
	}

	// Now walk the dest space and remap original samples into it
	if ( nComponentFlags != LOG_COMPONENTS_ALL )
	{
		for ( int i = 0; i < pBaseLayer->GetKeyCount(); ++i )
		{
			DmeTime_t tBaseKeyTime = pBaseLayer->GetKeyTime( i );

			DmeTime_t tSrcKeyTime = RemapTime( tBaseKeyTime, tDstTimes, tSrcTimes );

			T val;
			if ( tSrcKeyTime <= tSrcTimes[ TS_LEFT_FALLOFF ] ||
				 tSrcKeyTime >= tSrcTimes[ TS_RIGHT_FALLOFF ] )
			{
				val = pBaseLayer->GetValue( tBaseKeyTime );
			}
			else
			{
				val = pWriteLayer->GetValue( tBaseKeyTime );
			}

			DmeTime_t nextTime = ((i+1) < pBaseLayer->GetKeyCount()) ? pBaseLayer->GetKeyTime(i+1) : DMETIME_INVALID;
			SegmentInterpolation_t interpSetting = ((pBaseLayer->GetSegmentInterpolationSetting( tBaseKeyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE) ||
				(pWriteLayer->GetSegmentInterpolationSetting( tBaseKeyTime, nextTime, true ) == SEGMENT_NOINTERPOLATE)) ? SEGMENT_NOINTERPOLATE : SEGMENT_INTERPOLATE;

			// Have the layer itself do the masking!!!
			T maskedVal = pBaseLayer->MaskValue( tBaseKeyTime, val, nComponentFlags );
			pWriteLayer->InsertKey( tBaseKeyTime, maskedVal, interpSetting );
		}
	}

	// Update the bookmark times on the log to reflect the time changes as well
	int nNumComponents = GetNumBookmarkComponents();
	for ( int iComp = 0; iComp < nNumComponents; ++iComp )
	{
		CDmaArray< DmeTime_t > &bookmarkTimes = m_BookmarkTimes[ iComp ];

		int nNumBookmarks = bookmarkTimes.Count();
		if ( nNumBookmarks > 0 )
		{
			CEnableUndoScopeGuard sg;
		
			for ( int iBookmark = 0; iBookmark < nNumBookmarks; ++iBookmark )
			{
				DmeTime_t originalTime = bookmarkTimes[ iBookmark ];
				DmeTime_t newTime = RemapTime( originalTime, srcParams.m_nTimes, tDstTimes );
				bookmarkTimes.Set( iBookmark, newTime );
			}
		}
	}

}

template< class T >
void CDmeTypedLog< T >::PasteAndRescaleSamples( 
	const CDmeLogLayer *src,   // clipboard data
	const DmeLog_TimeSelection_t& srcParams,   // clipboard time selection
	const DmeLog_TimeSelection_t& destParams,   // current time selection
	bool bBlendAreaInFalloffRegion, // blending behavior in falloff area of current time selection
	bool bReverse ) // reverse the samples when pasting
{
	CDmeLogLayer *pBaseLayer = GetLayer( 0 );
	CDmeLogLayer *pWriteLayer = GetLayer( GetTopmostLayer() );
	PasteAndRescaleSamples( pBaseLayer, src, pWriteLayer, srcParams, destParams, bBlendAreaInFalloffRegion, bReverse );
}


//-----------------------------------------------------------------------------
// Purpose: Enforce a specified minimum range between the two values, 
// repositioning the values equidistant from the average at a distance of half 
// the minimum range if the values are the the specified distance apart
//-----------------------------------------------------------------------------
void EnsureSeparation( Vector &minVal, Vector &maxVal, float separation )
{
	Vector vecOffset( separation * 0.5f, separation * 0.5f, separation * 0.5f );
	Vector vecAvg = ( minVal + maxVal ) * 0.5f;
	Vector vecForceMin = vecAvg - vecOffset;
	Vector vecForceMax = vecAvg + vecOffset;
	VectorMin( minVal, vecForceMin, minVal );
	VectorMax( maxVal, vecForceMax, maxVal );
}

void EnsureSeparation( float &minVal, float &maxVal, float separation )
{
	float flAvg = ( minVal + maxVal ) * 0.5f;
	float flForceMin = flAvg - ( separation * 0.5f );
	float flForceMax = flAvg + ( separation * 0.5f );
	minVal = MIN( minVal, flForceMin );
	maxVal = MAX( maxVal, flForceMax );
}


template<>
void CDmeTypedLog< Vector >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );
	Assert( nChannels == 3 );

	if ( nChannels != 3 )
		return;

	// HACK HACK:  This is using layer 0 to compute the bounds, rather than the requested layer
	CDmeTypedLogLayer< Vector > *boundsLayer = static_cast< CDmeTypedLogLayer< Vector > * >( GetLayer( 0 ) );
	if ( !boundsLayer )
		return;

	CDmeTypedLogLayer< Vector > *baseLayer = static_cast< CDmeTypedLogLayer< Vector > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	Vector vecMins( FLT_MAX, FLT_MAX, FLT_MAX );
	Vector vecMaxs( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	// Compute bounds
	int kc = boundsLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		Vector keyValue = boundsLayer->GetKeyValue( i );
		for ( int j = 0; j < 3; ++j )
		{
			float val = keyValue[ j ];

			if ( val < vecMins[ j ] )
			{
				vecMins[ j ] = val;
			}
			if ( val > vecMaxs[ j ] )
			{
				vecMaxs[ j ] = val;
			}
		}
	}

	// Forces the normalization to represent at least the specified minimum range.
	EnsureSeparation( vecMins, vecMaxs, s_threshold * 10.0f );

	// Now add values, etc.
	kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Vector keyValue = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );

		for ( int j = 0; j < 3; ++j )
		{
			pChannels[ j ]->InsertKey( keyTime, keyValue[ j ], interpSetting );
		}
	}

	for ( int j = 0; j < 3; ++j )
	{
		for ( int i = 0; i < kc; ++i )
		{
			float keyValue = pChannels[ j ]->GetKeyValue( i );
			float normalized = RemapVal( keyValue, vecMins[ j ], vecMaxs[ j ], 0.0f, 1.0f );
			pChannels[ j ]->SetKeyValue( i, normalized );
		}

		if ( HasDefaultValue() )
		{
			pChannels[ j ]->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().Length(), vecMins[ j ], vecMaxs[ j ], 0.0f, 1.0f ) );
		}
	}
}

template<>
void CDmeTypedLog< Vector2D >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< Vector2D > *baseLayer = static_cast< CDmeTypedLogLayer< Vector2D > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Vector2D keyValue = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );

		float len = keyValue.Length();

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		pChannels[ 0 ]->InsertKey( keyTime, len, interpSetting );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = pChannels[ 0 ]->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		pChannels[ 0 ]->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().Length(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< Vector4D >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< Vector4D > *baseLayer = static_cast< CDmeTypedLogLayer< Vector4D > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Vector4D keyValue = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );

		float len = keyValue.Length();

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		pChannels[ 0 ]->InsertKey( keyTime, len, interpSetting );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = pChannels[ 0 ]->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		pChannels[ 0 ]->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().Length(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< int >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< int > *baseLayer = static_cast< CDmeTypedLogLayer< int > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		int keyValue = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );

		float len = (float)keyValue;

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		pChannels[ 0 ]->InsertKey( keyTime, len, interpSetting );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = pChannels[ 0 ]->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		pChannels[ 0 ]->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< float >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< float > *baseLayer = static_cast< CDmeTypedLogLayer< float > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		float len = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );
		
		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		pChannels[ 0 ]->InsertKey( keyTime, len, interpSetting );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = pChannels[ 0 ]->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		pChannels[ 0 ]->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< DmeTime_t >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< DmeTime_t > *baseLayer = static_cast< CDmeTypedLogLayer< DmeTime_t > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		DmeTime_t keyValue = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );

		float len = keyValue.GetSeconds();

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		pChannels[ 0 ]->InsertKey( keyTime, len, interpSetting );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = pChannels[ 0 ]->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		pChannels[ 0 ]->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().GetSeconds(), flMin, flMax, 0.0f, 1.0f ) );
	}
}



template<>
void CDmeTypedLog< Quaternion >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	Assert( GetDataType() != AT_FLOAT );
	Assert( nChannels == 3 || nChannels == 1 );
	if ( nChannels != 3 && nChannels != 1 )
		return;

	CDmeTypedLogLayer< Quaternion > *baseLayer = static_cast< CDmeTypedLogLayer< Quaternion > * >( GetLayer( nLayer ) );
	if ( !baseLayer )
		return;

	if ( nChannels == 1 )
	{
		int kc = baseLayer->GetKeyCount();
		for ( int i = 0; i < kc; ++i )
		{
			DmeTime_t tKeyTime = baseLayer->GetKeyTime( i );
			Quaternion keyValue = baseLayer->GetKeyValue( i );
			SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );
			float flNormalized = Normalize( keyValue );

			pChannels[ 0 ]->InsertKey( tKeyTime, flNormalized, interpSetting );
		}

		if ( HasDefaultValue() )
		{
			pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( Normalize( GetDefaultValue() ) );
		}
		return;
	}

	// HACK HACK:  This is using layer 0 to compute the bounds, rather than the requested layer
	CDmeTypedLogLayer< Quaternion > *boundsLayer = static_cast< CDmeTypedLogLayer< Quaternion > * >( GetLayer( 0 ) );
	if ( !boundsLayer )
		return;


	QAngle angMins( FLT_MAX, FLT_MAX, FLT_MAX );
	QAngle angMaxs( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	// Compute bounds
	int kc = boundsLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		Quaternion keyValue = boundsLayer->GetKeyValue( i );
		QAngle ang;
		QuaternionAngles( keyValue, ang );

		for ( int j = 0; j < 3; ++j )
		{
			float val = ang[ j ];

			if ( val < angMins[ j ] )
			{
				angMins[ j ] = val;
			}
			if ( val > angMaxs[ j ] )
			{
				angMaxs[ j ] = val;
			}
		}
	}

	// Forces the normalization to represent at least the specified minimum range.
	//EnsureSeparation( angMins, angMaxs, s_threshold * 10.0f );

	// Now add values, etc.
	kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Quaternion keyValue = baseLayer->GetKeyValue( i );
		SegmentInterpolation_t interpSetting = baseLayer->GetSegmentInterpolationSetting( i );
		QAngle ang;
		QuaternionAngles( keyValue, ang );

		for ( int j = 0; j < 3; ++j )
		{
			pChannels[ j ]->InsertKey( keyTime, ang[ j ], interpSetting );
		}
	}

	for ( int j = 0; j < 3; ++j )
	{
		for ( int i = 0; i < kc; ++i )
		{
			float keyValue = pChannels[ j ]->GetKeyValue( i );
			float normalized = RemapVal( keyValue, angMins[ j ], angMaxs[ j ], 0.0f, 1.0f );
			pChannels[ j ]->SetKeyValue( i, normalized );
		}
	}
}

//-----------------------------------------------------------------------------
// Creates a log of a specific type
//-----------------------------------------------------------------------------
CDmeLog *CDmeLog::CreateLog( DmAttributeType_t type, DmFileId_t fileid )
{
	switch ( type )
	{
	case AT_INT:
	case AT_INT_ARRAY:
		return CreateElement< CDmeIntLog >( "int log", fileid );
	case AT_FLOAT:
	case AT_FLOAT_ARRAY:
		return CreateElement< CDmeFloatLog >( "float log", fileid );
	case AT_BOOL:
	case AT_BOOL_ARRAY:
		return CreateElement< CDmeBoolLog >( "bool log", fileid );
	case AT_COLOR:
	case AT_COLOR_ARRAY:
		return CreateElement< CDmeColorLog >( "color log", fileid );
	case AT_VECTOR2:
	case AT_VECTOR2_ARRAY:
		return CreateElement< CDmeVector2Log >( "vector2 log", fileid );
	case AT_VECTOR3:
	case AT_VECTOR3_ARRAY:
		return CreateElement< CDmeVector3Log >( "vector3 log", fileid );
	case AT_VECTOR4:
	case AT_VECTOR4_ARRAY:
		return CreateElement< CDmeVector4Log >( "vector4 log", fileid );
	case AT_QANGLE:
	case AT_QANGLE_ARRAY:
		return CreateElement< CDmeQAngleLog >( "qangle log", fileid );
	case AT_QUATERNION:
	case AT_QUATERNION_ARRAY:
		return CreateElement< CDmeQuaternionLog >( "quaternion log", fileid );
	case AT_VMATRIX:
	case AT_VMATRIX_ARRAY:
		return CreateElement< CDmeVMatrixLog >( "vmatrix log", fileid );
	case AT_STRING:
	case AT_STRING_ARRAY:
		return CreateElement< CDmeStringLog >( "string log", fileid );
	case AT_TIME:
	case AT_TIME_ARRAY:
		return CreateElement< CDmeTimeLog >( "time log", fileid );
	}

	return NULL;
}

// Disallowed methods for types
//template<> void CDmeTypedLog< bool >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const bool& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< bool >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const bool& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< bool >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< bool >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Color >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Color& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Color >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Color& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Color >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Color >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Vector4D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Vector4D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector4D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector4D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector4D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector4D >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Vector2D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Vector2D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector2D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector2D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector2D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector2D >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }

//template<> void CDmeTypedLog< Vector >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Vector& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }

//template<> void CDmeTypedLog< VMatrix >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const VMatrix& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< VMatrix >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const VMatrix& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< VMatrix >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< VMatrix >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Quaternion >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Quaternion& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Quaternion >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Quaternion& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Quaternion >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Quaternion >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< QAngle >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const QAngle& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< QAngle >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const QAngle& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< QAngle >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< QAngle >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }


//-----------------------------------------------------------------------------
// Helpers for particular types of log layers
//-----------------------------------------------------------------------------
void GenerateRotationLog( CDmeQuaternionLogLayer *pLayer, const Vector &vecAxis, DmeTime_t pTime[4], float pRevolutionsPerSec[4] )
{
	for ( int i = 1; i < 4; ++i )
	{
		if ( pTime[i] < pTime[i-1] )
		{
			Warning( "Bogus times passed into GenerateRotationLog\n" );
			return;
		}
	}

	// Gets the initial value
	matrix3x4_t initial;
	Quaternion q = pLayer->GetValue( pTime[0] );
	QuaternionMatrix( q, initial );

	// Find the max rps, and compute the total rotation in degrees
	// by the time we reach the transition points. The total rotation = 
	// integral from 0 to t of 360 * ( rate[i] - rate[i-1] ) t / tl + rate[i-1] )
	// == 360 * ( ( rate[i] - rate[i-1] ) t^2 / 2 + rate[i-1] t )
	float pTotalRotation[4];
	float flMaxRPS = pRevolutionsPerSec[0];
	pTotalRotation[0] = 0.0f;
	for ( int i = 1; i < 4; ++i )
	{
		if ( pRevolutionsPerSec[i] > flMaxRPS )
		{
			flMaxRPS = pRevolutionsPerSec[i];
		}
		float dt = pTime[i].GetSeconds() - pTime[i-1].GetSeconds();
		float dRot = pRevolutionsPerSec[i] - pRevolutionsPerSec[i-1];
		pTotalRotation[i] = 360.0f * ( dRot * dt * 0.5 + pRevolutionsPerSec[i-1] * dt ) + pTotalRotation[i-1];
	}

	// We need to compute how long a single rotation takes, then create samples
	// at 1/4 the frequency of that amount of time
	VMatrix rot;
	matrix3x4_t total;
	QAngle angles;
	float flMaxRotationTime = (flMaxRPS != 0.0f) ? ( 0.125f / flMaxRPS ) : ( pTime[3].GetSeconds() - pTime[0].GetSeconds() );
	DmeTime_t dt( flMaxRotationTime );
	for ( DmeTime_t t = pTime[0]; t <= pTime[3]; t += dt )
	{
		int i = ( t < pTime[1] ) ? 1 : ( ( t < pTime[2] ) ? 2 : 3 );
		float flInterval = t.GetSeconds() - pTime[i-1].GetSeconds();
		float flOOSegmentDur = pTime[i].GetSeconds() - pTime[i-1].GetSeconds();
		if ( flOOSegmentDur == 0.0f )
		{
			Assert( flInterval == 0.0f );
			flOOSegmentDur = 1.0f;
		}
		else
		{
			flOOSegmentDur = 1.0f / flOOSegmentDur;
		}
		float dRot = pRevolutionsPerSec[i] - pRevolutionsPerSec[i-1];
		float flRotation = 360.0f * ( dRot * flInterval * flInterval * 0.5f * flOOSegmentDur + pRevolutionsPerSec[i-1] * flInterval ) + pTotalRotation[i-1];

		MatrixBuildRotationAboutAxis( rot, vecAxis, flRotation );
		ConcatTransforms( initial, rot.As3x4(), total );
		MatrixToAngles( VMatrix( total ), angles );
		AngleQuaternion( angles, q );
		pLayer->SetKey( t, q );
	}
}


//-----------------------------------------------------------------------------
// Transforms a position log
//-----------------------------------------------------------------------------
void RotatePositionLog( CDmeVector3LogLayer *pPositionLog, const matrix3x4_t& matrix )
{
	Assert( fabs( matrix[0][3] ) < 1e-3 && fabs( matrix[1][3] ) < 1e-3 && fabs( matrix[2][3] ) < 1e-3 );
	Vector position;
	int nCount = pPositionLog->GetKeyCount();
	for ( int i = 0; i < nCount; ++i )
	{
		const Vector &srcPosition = pPositionLog->GetKeyValue( i );
		VectorTransform( srcPosition, matrix, position );
		pPositionLog->SetKeyValue( i, position );
	}
}


//-----------------------------------------------------------------------------
// Transforms a orientation log
//-----------------------------------------------------------------------------
void RotateOrientationLog( CDmeQuaternionLogLayer *pOrientationLog, const matrix3x4_t& matrix, bool bPreMultiply = false )
{
	Assert( fabs( matrix[0][3] ) < 1e-3 && fabs( matrix[1][3] ) < 1e-3 && fabs( matrix[2][3] ) < 1e-3 );
	matrix3x4_t orientation, newOrientation;
	Quaternion q;
	int nCount = pOrientationLog->GetKeyCount();
	for ( int i = 0; i < nCount; ++i )
	{
		const Quaternion &srcQuat = pOrientationLog->GetKeyValue( i );
		QuaternionMatrix( srcQuat, orientation );
		if ( bPreMultiply )
		{
			ConcatTransforms( matrix, orientation, newOrientation );
		}
		else
		{
			ConcatTransforms( orientation, matrix, newOrientation );
		}
		MatrixQuaternion( newOrientation, q );
		pOrientationLog->SetKeyValue( i, q );
	}
}


float ComputeInterpolationFactor( float flFactor, int nInterpolatorType )
{
	static Vector s_pInterolationPoints[ 4 ] = 
	{
		Vector( 0.0f, 0.0f, 0.0f ),
		Vector( 0.0f, 0.0f, 0.0f ),
		Vector( 1.0f, 1.0f, 0.0f ),
		Vector( 1.0f, 1.0f, 0.0f )
	};

	Vector out;
	Interpolator_CurveInterpolate
		( 
		nInterpolatorType, 
		s_pInterolationPoints[ 0 ], // unused
		s_pInterolationPoints[ 1 ], 
		s_pInterolationPoints[ 2 ], 
		s_pInterolationPoints[ 3 ], // unused
		flFactor, 
		out 
		);
	return out.y; // clamp( out.y, 0.0f, 1.0f );
}

float GetAmountForTime( DmeTime_t dmetime, const TimeSelection_t &times, const int nInterpolationTypes[ 2 ] )
{
	if ( dmetime < times[ 0 ] || dmetime > times[ 3 ] )
		return 0.0f; // outside selection

	int nInterpolationType = INTERPOLATE_LINEAR_INTERP;
	float f = 0.0f;
	if ( dmetime >= times[ 1 ] )
	{
		if ( dmetime <= times[ 2 ] )
			return 1.0f; // hold

		f = ( times[ 3 ] - dmetime ).GetSeconds() / ( times[ 3 ] - times[ 2 ] ).GetSeconds();
		nInterpolationType = nInterpolationTypes[ 1 ];
	}
	else
	{
		f = ( dmetime - times[ 0 ] ).GetSeconds() / ( times[ 1 ] - times[ 0 ] ).GetSeconds();
		nInterpolationType = nInterpolationTypes[ 0 ];
	}

	float flOut = ComputeInterpolationFactor( f, nInterpolationType );
	return clamp( flOut, 0.0f, 1.0f );
}

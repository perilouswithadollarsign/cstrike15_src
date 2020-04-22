//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "mathlib/mathlib.h"
#if defined(__SPU__)
#include "ps3/spu_job_shared.h"
#endif

#include "bone_setup_PS3.h"
#include <string.h>

#if !defined(__SPU__)
#include "tier0/vprof.h"
#endif

#include "mathlib/ssequaternion.h"
#include "bone_utils_PS3.h"

// -----------------------------------------------------------------

// -----------------------------------------------------------------
// from mathlib_base.cpp
// -----------------------------------------------------------------

#if 0

void ConcatTransforms_Aligned_PS3( const matrix3x4a_t &m0, const matrix3x4a_t &m1, matrix3x4a_t &out )
{
// 	AssertAligned( &m0 );
// 	AssertAligned( &m1 );
// 	AssertAligned( &out );

	fltx4 lastMask = *(fltx4 *)(&g_SIMD_ComponentMask[3]);
	fltx4 rowA0 = LoadAlignedSIMD( m0.m_flMatVal[0] );
	fltx4 rowA1 = LoadAlignedSIMD( m0.m_flMatVal[1] );
	fltx4 rowA2 = LoadAlignedSIMD( m0.m_flMatVal[2] );

	fltx4 rowB0 = LoadAlignedSIMD( m1.m_flMatVal[0] );
	fltx4 rowB1 = LoadAlignedSIMD( m1.m_flMatVal[1] );
	fltx4 rowB2 = LoadAlignedSIMD( m1.m_flMatVal[2] );

	// now we have the rows of m0 and the columns of m1
	// first output row
	fltx4 A0 = SplatXSIMD(rowA0);
	fltx4 A1 = SplatYSIMD(rowA0);
	fltx4 A2 = SplatZSIMD(rowA0);
	fltx4 mul00 = MulSIMD( A0, rowB0 );
	fltx4 mul01 = MulSIMD( A1, rowB1 );
	fltx4 mul02 = MulSIMD( A2, rowB2 );
	fltx4 out0 = AddSIMD( mul00, AddSIMD(mul01,mul02) );

	// second output row
	A0 = SplatXSIMD(rowA1);
	A1 = SplatYSIMD(rowA1);
	A2 = SplatZSIMD(rowA1);
	fltx4 mul10 = MulSIMD( A0, rowB0 );
	fltx4 mul11 = MulSIMD( A1, rowB1 );
	fltx4 mul12 = MulSIMD( A2, rowB2 );
	fltx4 out1 = AddSIMD( mul10, AddSIMD(mul11,mul12) );

	// third output row
	A0 = SplatXSIMD(rowA2);
	A1 = SplatYSIMD(rowA2);
	A2 = SplatZSIMD(rowA2);
	fltx4 mul20 = MulSIMD( A0, rowB0 );
	fltx4 mul21 = MulSIMD( A1, rowB1 );
	fltx4 mul22 = MulSIMD( A2, rowB2 );
	fltx4 out2 = AddSIMD( mul20, AddSIMD(mul21,mul22) );

	// add in translation vector
	A0 = AndSIMD(rowA0,lastMask);
	A1 = AndSIMD(rowA1,lastMask);
	A2 = AndSIMD(rowA2,lastMask);
	out0 = AddSIMD(out0, A0);
	out1 = AddSIMD(out1, A1);
	out2 = AddSIMD(out2, A2);

	StoreAlignedSIMD( out.m_flMatVal[0], out0 );
	StoreAlignedSIMD( out.m_flMatVal[1], out1 );
	StoreAlignedSIMD( out.m_flMatVal[2], out2 );
}

void ConcatTransforms_PS3( const matrix3x4_t& in1, const matrix3x4_t& in2, matrix3x4_t& out )
{
#if 0
	// test for ones that'll be 2x faster
	if ( (((size_t)&in1) % 16) == 0 && (((size_t)&in2) % 16) == 0 && (((size_t)&out) % 16) == 0 )
	{
		ConcatTransforms_Aligned( in1, in2, out );
		return;
	}
#endif

	fltx4 lastMask = *(fltx4 *)(&g_SIMD_ComponentMask[3]);
	fltx4 rowA0 = LoadUnalignedSIMD( in1.m_flMatVal[0] );
	fltx4 rowA1 = LoadUnalignedSIMD( in1.m_flMatVal[1] );
	fltx4 rowA2 = LoadUnalignedSIMD( in1.m_flMatVal[2] );

	fltx4 rowB0 = LoadUnalignedSIMD( in2.m_flMatVal[0] );
	fltx4 rowB1 = LoadUnalignedSIMD( in2.m_flMatVal[1] );
	fltx4 rowB2 = LoadUnalignedSIMD( in2.m_flMatVal[2] );

	// now we have the rows of m0 and the columns of m1
	// first output row
	fltx4 A0 = SplatXSIMD(rowA0);
	fltx4 A1 = SplatYSIMD(rowA0);
	fltx4 A2 = SplatZSIMD(rowA0);
	fltx4 mul00 = MulSIMD( A0, rowB0 );
	fltx4 mul01 = MulSIMD( A1, rowB1 );
	fltx4 mul02 = MulSIMD( A2, rowB2 );
	fltx4 out0 = AddSIMD( mul00, AddSIMD(mul01,mul02) );

	// second output row
	A0 = SplatXSIMD(rowA1);
	A1 = SplatYSIMD(rowA1);
	A2 = SplatZSIMD(rowA1);
	fltx4 mul10 = MulSIMD( A0, rowB0 );
	fltx4 mul11 = MulSIMD( A1, rowB1 );
	fltx4 mul12 = MulSIMD( A2, rowB2 );
	fltx4 out1 = AddSIMD( mul10, AddSIMD(mul11,mul12) );

	// third output row
	A0 = SplatXSIMD(rowA2);
	A1 = SplatYSIMD(rowA2);
	A2 = SplatZSIMD(rowA2);
	fltx4 mul20 = MulSIMD( A0, rowB0 );
	fltx4 mul21 = MulSIMD( A1, rowB1 );
	fltx4 mul22 = MulSIMD( A2, rowB2 );
	fltx4 out2 = AddSIMD( mul20, AddSIMD(mul21,mul22) );

	// add in translation vector
	A0 = AndSIMD(rowA0,lastMask);
	A1 = AndSIMD(rowA1,lastMask);
	A2 = AndSIMD(rowA2,lastMask);
	out0 = AddSIMD(out0, A0);
	out1 = AddSIMD(out1, A1);
	out2 = AddSIMD(out2, A2);

	// write to output
	StoreUnalignedSIMD( out.m_flMatVal[0], out0 );
	StoreUnalignedSIMD( out.m_flMatVal[1], out1 );
	StoreUnalignedSIMD( out.m_flMatVal[2], out2 );
}



void MatrixAngles_PS3( const matrix3x4_t& matrix, float *angles )
{ 
	float forward[3];
	float left[3];
	float up[3];

	//
	// Extract the basis vectors from the matrix. Since we only need the Z
	// component of the up vector, we don't get X and Y.
	//
	forward[0] = matrix[0][0];
	forward[1] = matrix[1][0];
	forward[2] = matrix[2][0];
	left[0] = matrix[0][1];
	left[1] = matrix[1][1];
	left[2] = matrix[2][1];
	up[2] = matrix[2][2];

	float xyDist = sqrtf( forward[0] * forward[0] + forward[1] * forward[1] );

	// enough here to get angles?
	if ( xyDist > 0.001f )
	{
		// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
		angles[1] = RAD2DEG( atan2f( forward[1], forward[0] ) );

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG( atan2f( -forward[2], xyDist ) );

		// (roll)	z = ATAN( left.z, up.z );
		angles[2] = RAD2DEG( atan2f( left[2], up[2] ) );
	}
	else	// forward is mostly Z, gimbal lock-
	{
		// (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
		angles[1] = RAD2DEG( atan2f( -left[0], left[1] ) );

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG( atan2f( -forward[2], xyDist ) );

		// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
		angles[2] = 0.0f;
	}
}


void MatrixAngles_PS3( const matrix3x4_t& matrix, RadianEuler &angles, Vector &position )
{
	MatrixGetColumn_PS3( matrix, 3, position );
	MatrixAngles_PS3( matrix, angles );
}

void MatrixAngles_PS3( const matrix3x4_t &matrix, Quaternion &q, Vector &pos )
{
	float trace;
	trace = matrix[0][0] + matrix[1][1] + matrix[2][2] + 1.0f;
	if( trace > 1.0f + FLT_EPSILON ) 
	{
		// VPROF_INCREMENT_COUNTER("MatrixQuaternion A",1);
		q.x = ( matrix[2][1] - matrix[1][2] );
		q.y = ( matrix[0][2] - matrix[2][0] );
		q.z = ( matrix[1][0] - matrix[0][1] );
		q.w = trace;
	} 
	else if ( matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2] ) 
	{
		// VPROF_INCREMENT_COUNTER("MatrixQuaternion B",1);
		trace = 1.0f + matrix[0][0] - matrix[1][1] - matrix[2][2];
		q.x = trace;
		q.y = (matrix[1][0] + matrix[0][1] );
		q.z = (matrix[0][2] + matrix[2][0] );
		q.w = (matrix[2][1] - matrix[1][2] );
	} 
	else if (matrix[1][1] > matrix[2][2])
	{
		// VPROF_INCREMENT_COUNTER("MatrixQuaternion C",1);
		trace = 1.0f + matrix[1][1] - matrix[0][0] - matrix[2][2];
		q.x = (matrix[0][1] + matrix[1][0] );
		q.y = trace;
		q.z = (matrix[2][1] + matrix[1][2] );
		q.w = (matrix[0][2] - matrix[2][0] );
	}
	else
	{
		// VPROF_INCREMENT_COUNTER("MatrixQuaternion D",1);
		trace = 1.0f + matrix[2][2] - matrix[0][0] - matrix[1][1];
		q.x = (matrix[0][2] + matrix[2][0] );
		q.y = (matrix[2][1] + matrix[1][2] );
		q.z = trace;
		q.w = (matrix[1][0] - matrix[0][1] );
	}

	QuaternionNormalize_PS3( q );

#if 0
	// check against the angle version
	RadianEuler ang;
	MatrixAngles( matrix, ang );
	Quaternion test;
	AngleQuaternion( ang, test );
	float d = QuaternionDotProduct( q, test );
	Assert( fabs(d) > 0.99 && fabs(d) < 1.01 );
#endif

	MatrixGetColumn_PS3( matrix, 3, pos );
}
void MatrixGetColumn_PS3( const matrix3x4_t& in, int column, Vector &out )
{
	out.x = in[0][column];
	out.y = in[1][column];
	out.z = in[2][column];
}

void MatrixSetColumn_PS3( const Vector &in, int column, matrix3x4_t& out )
{
	out[0][column] = in.x;
	out[1][column] = in.y;
	out[2][column] = in.z;
}
void MatrixInvert_PS3( const matrix3x4_t& in, matrix3x4_t& out )
{
// 	if ( &in == &out )
// 	{
// 		V_swap(out[0][1],out[1][0]);
// 		V_swap(out[0][2],out[2][0]);
// 		V_swap(out[1][2],out[2][1]);
// 	}
// 	else
	{
		// transpose the matrix
		out[0][0] = in[0][0];
		out[0][1] = in[1][0];
		out[0][2] = in[2][0];

		out[1][0] = in[0][1];
		out[1][1] = in[1][1];
		out[1][2] = in[2][1];

		out[2][0] = in[0][2];
		out[2][1] = in[1][2];
		out[2][2] = in[2][2];
	}

	// now fix up the translation to be in the other space
 	float tmp[3];
 	tmp[0] = in[0][3];
 	tmp[1] = in[1][3];
 	tmp[2] = in[2][3];

	out[0][3] = -DotProduct_PS3( tmp, out[0] );
	out[1][3] = -DotProduct_PS3( tmp, out[1] );
	out[2][3] = -DotProduct_PS3( tmp, out[2] );
}

void SetIdentityMatrix_PS3( matrix3x4_t& matrix )
{
	memset( matrix.Base(), 0, sizeof(float)*3*4 );
	matrix[0][0] = 1.0f;
	matrix[1][1] = 1.0f;
	matrix[2][2] = 1.0f;
}
void VectorRotate_PS3( const float * RESTRICT in1, const matrix3x4_t& in2, float * RESTRICT out )
{
//	Assert( in1 != out );
	out[0] = DotProduct_PS3( in1, in2[0] );
	out[1] = DotProduct_PS3( in1, in2[1] );
	out[2] = DotProduct_PS3( in1, in2[2] );
}
void AngleMatrix_PS3( RadianEuler const &angles, const Vector &position, matrix3x4_t& matrix )
{
	AngleMatrix_PS3( angles, matrix );
	MatrixSetColumn_PS3( position, 3, matrix );
}
void AngleMatrix_PS3( const RadianEuler& angles, matrix3x4_t& matrix )
{
	QAngle quakeEuler( RAD2DEG( angles.y ), RAD2DEG( angles.z ), RAD2DEG( angles.x ) );

	AngleMatrix_PS3( quakeEuler, matrix );
}
void AngleMatrix_PS3( const QAngle &angles, const Vector &position, matrix3x4_t& matrix )
{
	AngleMatrix_PS3( angles, matrix );
	MatrixSetColumn_PS3( position, 3, matrix );
}
void AngleMatrix_PS3( const QAngle &angles, matrix3x4_t& matrix )
{
	float sr, sp, sy, cr, cp, cy;

#ifdef _X360
	fltx4 radians, scale, sine, cosine;
	radians = LoadUnaligned3SIMD( angles.Base() );
	scale = ReplicateX4( M_PI_F / 180.f ); 
	radians = MulSIMD( radians, scale );
	SinCos3SIMD( sine, cosine, radians ); 	

	sp = SubFloat( sine, 0 );	sy = SubFloat( sine, 1 );	sr = SubFloat( sine, 2 );
	cp = SubFloat( cosine, 0 );	cy = SubFloat( cosine, 1 );	cr = SubFloat( cosine, 2 );
#else
	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );
#endif

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;

	// NOTE: Do not optimize this to reduce multiplies! optimizer bug will screw this up.
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;

	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}

void AngleQuaternion_PS3( const RadianEuler &angles, Quaternion &outQuat )
{
	float sr, sp, sy, cr, cp, cy;

#ifdef _X360
	fltx4 radians, scale, sine, cosine;
	radians = LoadUnaligned3SIMD( &angles.x );
	scale = ReplicateX4( 0.5f ); 
	radians = MulSIMD( radians, scale );
	SinCos3SIMD( sine, cosine, radians ); 	

	// NOTE: The ordering here is *different* from the AngleQuaternion below
	// because p, y, r are not in the same locations in QAngle + RadianEuler. Yay!
	sr = SubFloat( sine, 0 );	sp = SubFloat( sine, 1 );	sy = SubFloat( sine, 2 );	
	cr = SubFloat( cosine, 0 );	cp = SubFloat( cosine, 1 );	cy = SubFloat( cosine, 2 );	
#else
	SinCos( angles.z * 0.5f, &sy, &cy );
	SinCos( angles.y * 0.5f, &sp, &cp );
	SinCos( angles.x * 0.5f, &sr, &cr );
#endif

	// NJS: for some reason VC6 wasn't recognizing the common subexpressions:
	float srXcp = sr * cp, crXsp = cr * sp;
	outQuat.x = srXcp*cy-crXsp*sy; // X
	outQuat.y = crXsp*cy+srXcp*sy; // Y

	float crXcp = cr * cp, srXsp = sr * sp;
	outQuat.z = crXcp*sy-srXsp*cy; // Z
	outQuat.w = crXcp*cy+srXsp*sy; // W (real component)
}

void Hermite_Spline_PS3( const Vector &p1, const Vector &p2, const Vector &d1, const Vector &d2, float t, Vector& output )
{
	float tSqr = t*t;
	float tCube = t*tSqr;

	Assert( &output != &p1 );
	Assert( &output != &p2 );
	Assert( &output != &d1 );
	Assert( &output != &d2 );

	float b1 = 2.0f*tCube-3.0f*tSqr+1.0f;
	float b2 = 1.0f - b1; // -2*tCube+3*tSqr;
	float b3 = tCube-2*tSqr+t;
	float b4 = tCube-tSqr;

	VectorScale_PS3( p1, b1, output );
	VectorMA_PS3( output, b2, p2, output );
	VectorMA_PS3( output, b3, d1, output );
	VectorMA_PS3( output, b4, d2, output );
}
float Hermite_Spline_PS3( float p1, float p2, float d1, float d2, float t )
{
	float output;
	float tSqr = t*t;
	float tCube = t*tSqr;

	float b1 = 2.0f*tCube-3.0f*tSqr+1.0f;
	float b2 = 1.0f - b1; // -2*tCube+3*tSqr;
	float b3 = tCube-2*tSqr+t;
	float b4 = tCube-tSqr;

	output = p1 * b1;
	output += p2 * b2;
	output += d1 * b3;
	output += d2 * b4;

	return output;
}


void Hermite_SplineBasis_PS3( float t, float basis[4] )
{
	float tSqr = t*t;
	float tCube = t*tSqr;

	basis[0] = 2.0f*tCube-3.0f*tSqr+1.0f;
	basis[1] = 1.0f - basis[0]; // -2*tCube+3*tSqr;
	basis[2] = tCube-2*tSqr+t;
	basis[3] = tCube-tSqr;
}
//#pragma optimize( "g", off )
void Hermite_Spline_PS3( const Vector &p0, const Vector &p1, const Vector &p2, float t, Vector& output )
{
	Vector e10, e21;
	VectorSubtract_PS3( p1, p0, e10 );
	VectorSubtract_PS3( p2, p1, e21 );
	Hermite_Spline_PS3( p1, p2, e10, e21, t, output );
}
//#pragma optimize( "", on )
float Hermite_Spline_PS3( float p0, float p1, float p2,	float t )
{
	return Hermite_Spline_PS3( p1, p2, p1 - p0, p2 - p1, t );
}


void Hermite_Spline_PS3( const Quaternion &q0, const Quaternion &q1, const Quaternion &q2, float t, Quaternion &output )
{
	// cheap, hacked version of quaternions
	Quaternion q0a;
	Quaternion q1a;

	QuaternionAlign_PS3( q2, q0, q0a );
	QuaternionAlign_PS3( q2, q1, q1a );

	output.x = Hermite_Spline_PS3( q0a.x, q1a.x, q2.x, t );
	output.y = Hermite_Spline_PS3( q0a.y, q1a.y, q2.y, t );
	output.z = Hermite_Spline_PS3( q0a.z, q1a.z, q2.z, t );
	output.w = Hermite_Spline_PS3( q0a.w, q1a.w, q2.w, t );

	QuaternionNormalize_PS3( output );
}



//-----------------------------------------------------------------------------
// Purpose: Converts a quaternion into engine angles
// Input  : *quaternion - q3 + q0.i + q1.j + q2.k
//			*outAngles - PITCH, YAW, ROLL
//-----------------------------------------------------------------------------
void QuaternionAngles_PS3( const Quaternion &q, RadianEuler &angles )
{
	Assert( s_bMathlibInitialized );
	Assert( q.IsValid() );

	// FIXME: doing it this way calculates too much data, needs to do an optimized version...
	matrix3x4_t matrix;
	QuaternionMatrix_PS3( q, matrix );
	MatrixAngles_PS3( matrix, angles );

	Assert( angles.IsValid() );
}


#endif


// some assumptions made about alignment here

#define _VEC_SWIZZLE_Y0X0X0Y0 (__vector unsigned char)	{ 0x04,0x05,0x06,0x07, 0x00,0x01,0x02,0x03, 0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07 }
#define _VEC_SWIZZLE_Y0Y0Z0Z0 (__vector unsigned char)	{ 0x04,0x05,0x06,0x07, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0A,0x0B, 0x08,0x09,0x0A,0x0B }
#define _VEC_SWIZZLE_Z0W0W0W0 (__vector unsigned char)	{ 0x08,0x09,0x0A,0x0B, 0x0C,0x0D,0x0E,0x0F, 0x0C,0x0D,0x0E,0x0F, 0x0C,0x0D,0x0E,0x0F }
#define _VEC_SWIZZLE_Z0Z0Y0X0 (__vector unsigned char)	{ 0x08,0x09,0x0A,0x0B, 0x08,0x09,0x0A,0x0B, 0x04,0x05,0x06,0x07, 0x00,0x01,0x02,0x03 }
#define _VEC_SWIZZLE_X0Y1Z1W1 (__vector unsigned char)	{ 0x00,0x01,0x02,0x03, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1A,0x1B, 0x1C,0x1D,0x1E,0x1F }
#define _VEC_SWIZZLE_X0Z1X0Z1 (__vector unsigned char)	{ 0x00,0x01,0x02,0x03, 0x18,0x19,0x1A,0x1B, 0x00,0x01,0x02,0x03, 0x18,0x19,0x1A,0x1B }
#define _VEC_SWIZZLE_X0Y0X1Y1 (__vector unsigned char)	{ 0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17 }
#define _VEC_SWIZZLE_X0Y0Z0X1 (__vector unsigned char)	{ 0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0A,0x0B, 0x10,0x11,0x12,0x13 }
#define _VEC_SWIZZLE_Y0X0W0Y1 (__vector unsigned char)	{ 0x04,0x05,0x06,0x07, 0x00,0x01,0x02,0x03, 0x0C,0x0D,0x0E,0x0F, 0x14,0x15,0x16,0x17 }
#define _VEC_SWIZZLE_X0Y0Z0Z1 (__vector unsigned char)	{ 0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0A,0x0B, 0x18,0x19,0x1A,0x1B }
#define _VEC_SWIZZLE_Z1W0Z1W0 (__vector unsigned char)	{ 0x18,0x19,0x1A,0x1B, 0x0C,0x0D,0x0E,0x0F, 0x18,0x19,0x1A,0x1B, 0x0C,0x0D,0x0E,0x0F }
#define _VEC_SWIZZLE_Z0W1Z0W1 (__vector unsigned char)	{ 0x08,0x09,0x0A,0x0B, 0x1C,0x1D,0x1E,0x1F, 0x08,0x09,0x0A,0x0B, 0x1C,0x1D,0x1E,0x1F }

#define _VEC_ONEZEROZEROZERO  (__vector float)			{ 1.0f, 0.0f, 0.0f, 0.0f }

#define _VEC_ZEROSIGNSIGNZERO (__vector unsigned int)	{ 0x0, 0x80000000, 0x80000000, 0x0 }
#define _VEC_ZEROSIGNSIGNSIGN (__vector unsigned int)	{ 0x0, 0x80000000, 0x80000000, 0x80000000 }
#define _VEC_SIGNZEROSIGNSIGN (__vector unsigned int)	{ 0x80000000, 0x0, 0x80000000, 0x80000000 }
#define _VEC_SIGNSIGNZEROSIGN (__vector unsigned int)	{ 0x80000000, 0x80000000, 0x0, 0x80000000 }
#define _VEC_SIGNZEROZEROZERO (__vector unsigned int)	{ 0x80000000, 0x0, 0x0, 0x0 }
#define _VEC_SIGNSIGNZEROZERO (__vector unsigned int)	{ 0x80000000, 0x80000000, 0x0, 0x0 }
#define _VEC_ZEROZEROZEROSIGN (__vector unsigned int)	{ 0x0, 0x0, 0x0, 0x80000000 }

#if defined(__SPU__)
// cyclic dependancy workaround (redefinition here) - TODO: remove
const fltx4 Four_Epsilons={FLT_EPSILON,FLT_EPSILON,FLT_EPSILON,FLT_EPSILON};
#endif


void QuaternionMatrix_PS3( const Quaternion &q, const Vector &pos, matrix3x4a_t& matrix )
{
	fltx4 v0, v1, v2, v3, v4, v5, v6, v7;

	v0 = LoadUnalignedSIMD( &q );					// x, y, z, w	(q)
	v6 = LoadUnalignedSIMD( &pos );					// px, py, pz, pw
	v1 = AddSIMD( v0, v0 );							// 2x, 2y, 2z, 2w

	v2 = vec_perm( v0, v0, _VEC_SWIZZLE_Y0X0X0Y0 );	// y, x, x, y
	v3 = vec_perm( v1, v1, _VEC_SWIZZLE_Y0Y0Z0Z0 );	// 2y, 2y, 2z, 2z
	v2 = MulSIMD( v2, v3 );							// 2yy, 2xy, 2xz, 2yz

	v4 = vec_perm( v0, v0, _VEC_SWIZZLE_Z0W0W0W0 );	// z, w, w, w
	v5 = vec_perm( v1, v1, _VEC_SWIZZLE_Z0Z0Y0X0 );	// 2z, 2z, 2y, 2x
	v4 = MulSIMD( v4, v5 );							// 2zz, 2zw, 2yw, 2xw

	v0 = MulSIMD( v0, v1 );							// 2xx, 2yy, 2zz, 2ww
	v0 = vec_perm( v0, v1, _VEC_SWIZZLE_X0Y1Z1W1 );	// 2xx, 2y, 2z, 2w
	
	// last two elements of third row
	v7 = SubSIMD( _VEC_ONEZEROZEROZERO, v0 );		// 1-2xx, --, --, --
	v7 = SubSIMD( v7, v2 );							// 1-2xx-2yy, --, --, --
	v7 = vec_perm( v7, v6, _VEC_SWIZZLE_X0Z1X0Z1 ); // 1-2xx-2yy, pz, --, --
 
	// first row
	// 1-2yy-2zz, 2xy-2zw, 2xz+2yw, px
	v2 = vec_xor( v2, (fltx4)_VEC_SIGNZEROZEROZERO );// -2yy, 2xy, 2xz, 2yz
	v4 = vec_xor( v4, (fltx4)_VEC_SIGNSIGNZEROSIGN );// -2zz, -2zw, 2yw, -2xw
	v4 = AddSIMD( v4, _VEC_ONEZEROZEROZERO );		 // 1-2zz, -2zw, 2yw, -2xw
	v3 = AddSIMD( v4, v2 );							 // 1-2zz-2yy, 2xy-2zw, 2xz+2yw, 2yz-2xw
	StoreAlignedSIMD( matrix[0], vec_perm( v3, v6, _VEC_SWIZZLE_X0Y0Z0X1 ) ); // 1-2zz-2yy, 2xy-2zw, 2xz+2yw, px

	// second row
	// 2xy+2wz, 1-2xx-2zz, 2yz-2xw, py
	v2 = vec_perm( v0, v2, _VEC_SWIZZLE_X0Y1Z1W1 );	 // 2xx, 2xy, 2xz, 2yz
	v2 = vec_xor( v2, (fltx4)_VEC_SIGNZEROZEROZERO );// -2xx, 2xy, 2xz, 2yz
	v4 = vec_xor( v4, (fltx4)_VEC_ZEROSIGNSIGNZERO );// 1-2zz, 2zw, -2yw, -2xw
	v3 = AddSIMD( v4, v2 );							 // 1-2xx-2zz, 2xy+2zw, 2xz-2yw, 2yz-2xw
	StoreAlignedSIMD( matrix[1], vec_perm( v3, v6, _VEC_SWIZZLE_Y0X0W0Y1 ) ); // 2xy+2zw, 1-2xx-2zz, 2yz-2xw, py

	// third row
	// 2xz-2yw, 2yz+2xw, 1-2xx-2yy, pz
	v2 = SubSIMD( v2, v4 );							// 2zz-1-2xx, 2xy-2zw, 2xz+2yw, 2yz+2xw
	v3 = vec_perm( v3, v2, _VEC_SWIZZLE_Z0W1Z0W1 ); // 2xz-2yw, 2yz+2xw, --, -- 
	v3 = vec_perm( v3, v7, _VEC_SWIZZLE_X0Y0X1Y1 );	// 2xz-2yw, 2yz+2xw, 1-2xx-2yy, pz
	StoreAlignedSIMD( matrix[2], v3 );				// 2xz+2yw, 2xw+2yz, 1-2xx-2yy, pz
}

void QuaternionAlign_PS3( const Quaternion &p, const Quaternion &q, QuaternionAligned &qt )
{
	fltx4 p1, q1, qt1;
	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	qt1 = QuaternionAlignSIMD( p1, q1 );
	StoreAlignedSIMD( (QuaternionAligned *)&qt, qt1 );

	AssertFatal( qt.IsValid() );
}

void QuaternionSlerp_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	fltx4 p1, q1, qt1;


	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	qt1 = QuaternionSlerpSIMD( p1, q1, t );

	StoreUnalignedSIMD( qt.Base(), qt1 );

	AssertFatal( qt.IsValid() );
}


void QuaternionSlerpNoAlign_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	fltx4 p1, q1, qt1;


	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	qt1 = QuaternionSlerpNoAlignSIMD( p1, q1, t );

	StoreUnalignedSIMD( qt.Base(), qt1 );

	AssertFatal( qt.IsValid() );
}

float QuaternionNormalize_PS3( Quaternion &q )
{
	fltx4 q1, radius, result;

	bi32x4 mask;

	q1 = LoadUnalignedSIMD( &q );
	radius = Dot4SIMD( q1, q1 );
	mask = CmpEqSIMD( radius, Four_Zeros ); // all ones iff radius = 0
	result = ReciprocalSqrtSIMD( radius );
	result = MulSIMD( result, q1 );
	result = MaskedAssign( mask, q1, result );	// if radius was 0, just return q

	StoreUnalignedSIMD( q.Base(), result );

	AssertFatal( q.IsValid() );

	return GetComponentSIMD( radius, 0 );
}

void QuaternionBlend_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	fltx4 psimd, qsimd, qtsimd;
	psimd = LoadUnalignedSIMD( p.Base() );
	qsimd = LoadUnalignedSIMD( q.Base() );
	qtsimd = QuaternionBlendSIMD( psimd, qsimd, t );
	StoreUnalignedSIMD( qt.Base(), qtsimd );

	AssertFatal( qt.IsValid() );
}

void QuaternionBlendNoAlign_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	fltx4 p1, q1, qt1;

	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	qt1 = QuaternionBlendNoAlignSIMD( p1, q1, t );

	StoreUnalignedSIMD( qt.Base(), qt1 );

	AssertFatal( qt.IsValid() );
}

void QuaternionIdentityBlend_PS3( const Quaternion &p, float t, Quaternion &qt )
{

	fltx4 p1, t1, tw, sclp, qt1;

	p1   = LoadUnalignedSIMD( &p );
	t1	 = ReplicateX4( t );
	sclp = SubSIMD( _VEC_ONEF, t1 );
	qt1  = MulSIMD( p1, sclp );
	tw   = _VEC_ZEROF;
	tw   = SetWSIMD( tw, t1 );

	tw   = XorSIMD( tw, AndSIMD( qt1, (fltx4)_VEC_ZEROZEROZEROSIGN ) );
	qt1  = AddSIMD( qt1, tw );

	QuaternionNormalizeSIMD( qt1 );

	StoreUnalignedSIMD( qt.Base(), qt1 );

	AssertFatal( qt.IsValid() );
}

void QuaternionScale_PS3( const Quaternion &p, float t, Quaternion &q )
{
	fltx4 p1, q1;

	p1 = LoadUnalignedSIMD( &p );
	q1 = QuaternionScaleSIMD( p1, t );

	StoreUnalignedSIMD( q.Base(), q1 );

	AssertFatal( q.IsValid() );
}


void QuaternionAdd_PS3( const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
	fltx4 p1, q1, qt1;
	fltx4 q2;

	p1 = LoadUnalignedSIMD( &p );
	q1 = LoadUnalignedSIMD( &q );
	q2 = QuaternionAlignSIMD( p1, q1 );

	qt1 = AddSIMD( p1, q2 );

	StoreUnalignedSIMD( qt.Base(), qt1 );

	AssertFatal( qt.IsValid() );
}


float QuaternionDotProduct_PS3( const Quaternion &p, const Quaternion &q )
{
	fltx4 p1, q1, qt1;

	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	qt1 = Dot4SIMD( p1, q1 );

#if !defined(__SPU__)
	QuaternionAligned qt;
	StoreAlignedSIMD( qt.Base(), qt1 );
	AssertFatal( qt.IsValid() );
#endif

	return GetComponentSIMD( qt1, 0 );
}

void QuaternionMult_PS3( const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
	fltx4 p1, q1, qt1;

	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	qt1 = QuaternionMultSIMD( p1, q1 );

	StoreUnalignedSIMD( qt.Base(), qt1 );

	AssertFatal( qt.IsValid() );
}

//-----------------------------------------------------------------------------
// Purpose: build boneToWorld transforms for a specific bone
//-----------------------------------------------------------------------------


void BuildBoneChain_PS3(
					const int *pBoneParent,
					const matrix3x4a_t &rootxform,
					const BoneVector pos[], 
					const BoneQuaternion q[], 
					int	iBone,
					matrix3x4a_t *pBoneToWorld )
{
	CBoneBitList_PS3 boneComputed;
	BuildBoneChainPartial_PS3( pBoneParent, rootxform, pos, q, iBone, pBoneToWorld, boneComputed, -1 );
	return;
}


void BuildBoneChain_PS3(
					const int *pBoneParent, 
					const matrix3x4a_t &rootxform,
					const BoneVector pos[], 
					const BoneQuaternion q[], 
					int	iBone,
					matrix3x4a_t *pBoneToWorld,
					CBoneBitList_PS3 &boneComputed )
{
	BuildBoneChainPartial_PS3( pBoneParent, rootxform, pos, q, iBone, pBoneToWorld, boneComputed, -1 );
}


void BuildBoneChainPartial_PS3(
						   const int *pBoneParent, 
						   const matrix3x4a_t &rootxform,
						   const BoneVector pos[], 
						   const BoneQuaternion q[], 
						   int	iBone,
						   matrix3x4a_t *pBoneToWorld,
						   CBoneBitList_PS3 &boneComputed,
						   int iRoot )
{
	if ( boneComputed.IsBoneMarked( iBone ) )
		return;

	matrix3x4a_t bonematrix;
	QuaternionMatrix_PS3( q[ iBone ], pos[ iBone ], bonematrix );

	int parent = pBoneParent[ iBone ];

	if( parent == -1 || iBone == iRoot ) 
	{
		ConcatTransforms_Aligned_PS3( rootxform, bonematrix, pBoneToWorld[ iBone ] );
	}
	else
	{
		// evil recursive!!!
		BuildBoneChainPartial_PS3( pBoneParent, rootxform, pos, q, parent, pBoneToWorld, boneComputed, iRoot );
		ConcatTransforms_Aligned_PS3( pBoneToWorld[ parent ], bonematrix, pBoneToWorld[ iBone ]);
	}

	boneComputed.MarkBone( iBone );
}

//-----------------------------------------------------------------------------
// Purpose: qt = ( s * p ) * q
//-----------------------------------------------------------------------------
void QuaternionSM_PS3( float s, const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
// 	Quaternion		p1, q1;
// 
// 	QuaternionScale_PS3( p, s, p1 );
// 	QuaternionMult_PS3( p1, q, q1 );
// 	QuaternionNormalize_PS3( q1 );
// 	qt[0] = q1[0];
// 	qt[1] = q1[1];
// 	qt[2] = q1[2];
// 	qt[3] = q1[3];

	fltx4 p1, q1, qt1;

	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	p1  = QuaternionScaleSIMD( p1, s );
	q1  = QuaternionMultSIMD( p1, q1 );
	qt1 = QuaternionNormalizeSIMD( q1 );

	StoreUnalignedSIMD( qt.Base(), qt1 );
}


//-----------------------------------------------------------------------------
// Purpose: qt = p * ( s * q )
//-----------------------------------------------------------------------------
void QuaternionMA_PS3( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt )
{
// 	Quaternion p1, q1;
// 
// 	QuaternionScale_PS3( q, s, q1 );
// 	QuaternionMult_PS3( p, q1, p1 );
// 	QuaternionNormalize_PS3( p1 );
// 	qt[0] = p1[0];
// 	qt[1] = p1[1];
// 	qt[2] = p1[2];
// 	qt[3] = p1[3];

	fltx4 p1, q1, qt1;

	q1  = LoadUnalignedSIMD( &q );
	p1  = LoadUnalignedSIMD( &p );
	q1  = QuaternionScaleSIMD( q1, s );
	p1  = QuaternionMultSIMD( p1, q1 );
	qt1 = QuaternionNormalizeSIMD( p1 );

	StoreUnalignedSIMD( qt.Base(), qt1 );
}

//-----------------------------------------------------------------------------
// Purpose: qt = p + s * q
//-----------------------------------------------------------------------------
void QuaternionAccumulate_PS3( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt )
{
// 	Quaternion q2;
// 	QuaternionAlign_PS3( p, q, q2 );
// 
// 	qt[0] = p[0] + s * q2[0];
// 	qt[1] = p[1] + s * q2[1];
// 	qt[2] = p[2] + s * q2[2];
// 	qt[3] = p[3] + s * q2[3];

	fltx4 p1, s1, q1, qt1;
	p1  = LoadUnalignedSIMD( &p );
	q1  = LoadUnalignedSIMD( &q );
	s1  = ReplicateX4( s );
	qt1 = QuaternionAlignSIMD( p1, q1 );

	qt1 = MaddSIMD( qt1, s1, p1 );

	StoreUnalignedSIMD( qt.Base(), qt1 );
}


void GetBoneMapBoneWeight_SPU( bonejob_SPU *pBonejob, accumposeentry_SPU *pPoseEntry, int *&pLS_boneMap, float *&pLS_boneWeight )
{
	int maxAnimBones = pBonejob->numBones;


	for( int lp = 0; lp < MAX_BLENDANIMS; lp++ )
	{
		if( pPoseEntry->animIndices[lp] != -1 )
		{
			maxAnimBones = MAX( maxAnimBones, pPoseEntry->anims[ pPoseEntry->animIndices[lp] ].animstudiohdr_numbones );
		}
	}

	if( pPoseEntry->pEA_seqgroup_boneMap )
	{
		pLS_boneMap = (int *)SPUmemcpy_UnalignedGet_MustSync( pLS_boneMap, (uint32)pPoseEntry->pEA_seqgroup_boneMap, sizeof(int) * maxAnimBones, DMATAG_ANIM_SYNC_BONEMAPWEIGHT );
	}

	Assert( pPoseEntry->pEA_seqdesc_boneWeight );
	pLS_boneWeight = (float *)SPUmemcpy_UnalignedGet_MustSync( pLS_boneWeight, (uint32)pPoseEntry->pEA_seqdesc_boneWeight, sizeof(float) * maxAnimBones, DMATAG_ANIM_SYNC_BONEMAPWEIGHT );
}

//-----------------------------------------------------------------------------
// Purpose: blend together in world space q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------

void WorldSpaceSlerp_SPU(
					 bonejob_SPU* pSPUJob,
					 accumposeentry_SPU *pPoseEntry,
					 BoneQuaternion *q1, 
					 BoneVector *pos1, 
					 const BoneQuaternion *q2, 
					 const BoneVector *pos2, 
					 const int *boneMap,
					 const float *boneWeight,
					 float s,
					 int boneMask )
{
	SNPROF_ANIM("WorldSpaceSlerp_SPU");

	int			i, j;
	float		s1; // weight of parent for q2, pos2
	float		s2; // weight for q2, pos2

	// make fake root transform
	matrix3x4a_t rootXform;
	SetIdentityMatrix_PS3( rootXform );

	// matrices for q2, pos2
//	matrix3x4a_t *srcBoneToWorld = g_matStack[0];
	matrix3x4a_t *srcBoneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pSPUJob->maxBones );
	CBoneBitList_PS3 srcBoneComputed;

//	matrix3x4a_t *destBoneToWorld = g_matStack[1];
	matrix3x4a_t *destBoneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pSPUJob->maxBones );
	CBoneBitList_PS3 destBoneComputed;

//	matrix3x4a_t *targetBoneToWorld = g_matStack[2];
	matrix3x4a_t *targetBoneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pSPUJob->maxBones );
//	CBoneBitList_PS3 targetBoneComputed;


	// get bonemap and boneweights

	for( i = 0; i < pSPUJob->numBones; i++ )
	{
		// skip unused bones
//		if (!(pStudioHdr->boneFlags(i) & boneMask))
		if ( !( pSPUJob->boneFlags[ i ] & boneMask ) )
		{
			continue;
		}

//		int n = pbone[i].parent;
		int n = pSPUJob->boneParent[ i ];

		s1 = 0.0f;
		if( pPoseEntry->pEA_seqgroup_boneMap )
		{
//			j = pSeqGroup->boneMap[i];
			j = boneMap[ i ];
			if( j >= 0 )
			{
//				s2 = s * seqdesc.weight( j );	// blend in based on this bones weight
				s2 = s * boneWeight[ j ];	// blend in based on this bones weight
				if( n != -1 )
				{
//					s1 = s * seqdesc.weight( pSeqGroup->boneMap[n] );
					s1 = s * boneWeight[ boneMap[ n ] ];
				}
			}
			else
			{
				s2 = 0.0f;
			}
		}
		else
		{
//			s2 = s * seqdesc.weight( i );	// blend in based on this bones weight
			s2 = s * boneWeight[ i ];	// blend in based on this bones weight
			if (n != -1)
			{
//				s1 = s * seqdesc.weight( n );
				s1 = s * boneWeight[ n ];
			}
		}

		if( s1 == 1.0f && s2 == 1.0f )
		{
			pos1[i] = pos2[i];
			q1[i] = q2[i];
		}
		else if( s2 > 0.0f )
		{
			BoneQuaternion	srcQ, destQ;
			BoneVector		srcPos, destPos;
			BoneQuaternion	targetQ;
			BoneVector		targetPos;
			BoneVector		tmp;

			BuildBoneChain_PS3( pSPUJob->boneParent, rootXform, pos1, q1, i, destBoneToWorld, destBoneComputed );
			BuildBoneChain_PS3( pSPUJob->boneParent, rootXform, pos2, q2, i, srcBoneToWorld, srcBoneComputed );

			MatrixAngles_PS3( destBoneToWorld[i], destQ, destPos );
			MatrixAngles_PS3( srcBoneToWorld[i], srcQ, srcPos );

			QuaternionSlerp_PS3( destQ, srcQ, s2, targetQ );
			AngleMatrix_PS3( targetQ, destPos, targetBoneToWorld[i] );

			// back solve
			if( n == -1 )
			{
				MatrixAngles_PS3( targetBoneToWorld[i], q1[i], tmp );
			}
			else
			{
				matrix3x4a_t worldToBone;
				MatrixInvert_PS3( targetBoneToWorld[n], worldToBone );

				matrix3x4a_t local;
				ConcatTransforms_Aligned_PS3( worldToBone, targetBoneToWorld[i], local );
				MatrixAngles_PS3( local, q1[i], tmp );

				// blend bone lengths (local space)
				pos1[i] = Lerp_PS3( s2, pos1[i], pos2[i] );
			}
		}
	}


	PopLSStack( sizeof(matrix3x4a_t) * pSPUJob->maxBones );
	PopLSStack( sizeof(matrix3x4a_t) * pSPUJob->maxBones );
	PopLSStack( sizeof(matrix3x4a_t) * pSPUJob->maxBones );

}


//-----------------------------------------------------------------------------
// Purpose: blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//
// assumes p, q arrays aligned
//-----------------------------------------------------------------------------
void SlerpBones_SPU( 
				bonejob_SPU* pSPUJob,
				accumposeentry_SPU *pPoseEntry,
				BoneQuaternion *q1, 
				BoneVector *pos1, 
				const BoneQuaternion *q2, 
				const BoneVector *pos2, 
				const int *boneMap,
				const float *boneWeight,
				float s,
				int boneMask )
{
	SNPROF_ANIM( "SlerpBones_SPU" );

	// Assert 16-byte alignment of in and out arrays.
// 	AssertMsg( 
// 		((reinterpret_cast<unsigned int>(q1)   & 0x0F)==0) &&
// 		((reinterpret_cast<unsigned int>(q2)   & 0x0F)==0) ,
// 		"Input arrays to SlerpBones are not aligned! Catastrophe is inevitable.\n"); 

	if( s <= 0.0f ) 
		return;

	if( s > 1.0f )
	{
		s = 1.0f;		
	}

	if( pPoseEntry->seqdesc_flags & STUDIO_WORLD )
	{
		WorldSpaceSlerp_SPU( pSPUJob, pPoseEntry, q1, pos1, q2, pos2, boneMap, boneWeight, s, boneMask );
		return;
	}

	int			i;

	// get bonemap and boneweights

// 	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();
// 
// 	const virtualgroup_t * RESTRICT pSeqGroup = NULL;
// 	if( pVModel )
// 	{
// 		pSeqGroup = pVModel->pSeqGroup( sequence );
// 	}

	// Build weightlist for all bones
	int nBoneCount = pSPUJob->numBones;
//	int nBoneCount = pStudioHdr->numbones();
//	float * RESTRICT pS2 = (float*)stackalloc( nBoneCount * sizeof(float) ); // 16-byte aligned
//	float *pS2 = g_floatStack[ 0 ];
	float *pS2 = (float *)PushLSStack( sizeof(float) * nBoneCount );


//	if( pSeqGroup ) // hoist this branch outside of the inner loop for speed (even correctly predicted branches are an eight cycle latency)
	if( pPoseEntry->pEA_seqgroup_boneMap )
	{
		for( i = 0; i < nBoneCount; i++ )
		{
			// skip unused bones
//			if( !(pStudioHdr->boneFlags(i) & boneMask) ||
//				pSeqGroup->boneMap[i] < 0 )
			if( !( pSPUJob->boneFlags[ i ] & boneMask ) ||
				boneMap[ i ] < 0 )
			{
				pS2[i] = 0.0f;
			}
			else
			{
				// boneMap[i] is not a float, don't be lured by the siren call of fcmp
//				pS2[i] = s * seqdesc.weight( pSeqGroup->boneMap[i] );
				pS2[i] = s * boneWeight[ boneMap[ i ] ];
			}
		}
	} 
	else // !pSeqGroup
	{
		for( i = 0; i < nBoneCount; i++ )
		{
			// skip unused bones
//			if( !(pStudioHdr->boneFlags(i) & boneMask) )
			if( !( pSPUJob->boneFlags[ i ] & boneMask ) )
			{
				pS2[i] = 0.0f;
			}
			else
			{
//				pS2[i] = s * seqdesc.weight( i );	// blend in based on this bones weight
				pS2[i] = s * boneWeight[ i ];	// blend in based on this bones weight
			}
		}
	}

	float weight;
	int nBoneCountRoundedFour = ( nBoneCount ) & (~(3));
	if( pPoseEntry->seqdesc_flags & STUDIO_DELTA )
	{
		// do as many as we can four at a time, then take care of stragglers.
		for( i = 0; i < nBoneCountRoundedFour; i+=4 )
		{
			fltx4 weightfour = LoadAlignedSIMD(pS2+i); // four weights

			FourQuaternions q1four, q2four;
			FourQuaternions result;

			q1four.LoadAndSwizzleAligned(q1+i); // four quaternions
			q2four.LoadAndSwizzleAligned(q2+i); // four quaternions

			if( pPoseEntry->seqdesc_flags & STUDIO_POST )
			{

				// result = q1 * ( weight * q2 ) 
				result = q1four.MulAc(weightfour, q2four);
			}
			else
			{

				// result = ( s * q1 ) * q2
				result = q2four.ScaleMul(weightfour, q1four);
			}

			// mask out unused channels, replacing them with original data
			{
				bi32x4 tinyScales = CmpLeSIMD( weightfour, Four_Zeros );
				result.x = MaskedAssign(tinyScales, q1four.x, result.x);
				result.y = MaskedAssign(tinyScales, q1four.y, result.y);
				result.z = MaskedAssign(tinyScales, q1four.z, result.z);
				result.w = MaskedAssign(tinyScales, q1four.w, result.w);
			}

			result.SwizzleAndStoreAlignedMasked(q1+i, CmpGtSIMD(weightfour,Four_Zeros) );

			fltx4 originalpos1simd[4], pos1simd[4], pos2simd[4];
			originalpos1simd[0] = pos1simd[0] = LoadAlignedSIMD(pos1[i+0].Base());
			originalpos1simd[1] = pos1simd[1] = LoadAlignedSIMD(pos1[i+1].Base());
			originalpos1simd[2] = pos1simd[2] = LoadAlignedSIMD(pos1[i+2].Base());
			originalpos1simd[3] = pos1simd[3] = LoadAlignedSIMD(pos1[i+3].Base());
			pos2simd[0] = LoadAlignedSIMD(pos2[i+0].Base());
			pos2simd[1] = LoadAlignedSIMD(pos2[i+1].Base());
			pos2simd[2] = LoadAlignedSIMD(pos2[i+2].Base());
			pos2simd[3] = LoadAlignedSIMD(pos2[i+3].Base());
			// should be able to use aligned loads??
// 			originalpos1simd[0] = pos1simd[0] = LoadUnalignedSIMD(pos1[i+0].Base());
// 			originalpos1simd[1] = pos1simd[1] = LoadUnalignedSIMD(pos1[i+1].Base());
// 			originalpos1simd[2] = pos1simd[2] = LoadUnalignedSIMD(pos1[i+2].Base());
// 			originalpos1simd[3] = pos1simd[3] = LoadUnalignedSIMD(pos1[i+3].Base());
// 			pos2simd[0] = LoadUnalignedSIMD(pos2[i+0].Base());
// 			pos2simd[1] = LoadUnalignedSIMD(pos2[i+1].Base());
// 			pos2simd[2] = LoadUnalignedSIMD(pos2[i+2].Base());
// 			pos2simd[3] = LoadUnalignedSIMD(pos2[i+3].Base());

			fltx4 splatweights[4] = { SplatXSIMD(weightfour),
									  SplatYSIMD(weightfour),
									  SplatZSIMD(weightfour),
									  SplatWSIMD(weightfour) };

			fltx4 Zero = Four_Zeros;
			pos1simd[0] = MaddSIMD(pos2simd[0], splatweights[0], pos1simd[0] );
				splatweights[0] = ( fltx4 ) CmpGtSIMD(splatweights[0], Zero);
			pos1simd[1] = MaddSIMD(pos2simd[1], splatweights[1], pos1simd[1] );
				splatweights[1] = ( fltx4 ) CmpGtSIMD(splatweights[1], Zero);
			pos1simd[2] = MaddSIMD(pos2simd[2], splatweights[2], pos1simd[2] );
				splatweights[2] = ( fltx4 ) CmpGtSIMD(splatweights[2], Zero);
			pos1simd[3] = MaddSIMD(pos2simd[3], splatweights[3], pos1simd[3] );
				splatweights[3] = ( fltx4 ) CmpGtSIMD(splatweights[3], Zero);

			// mask out unweighted bones
			/*
			if (pS2[i+0] > 0)
				StoreUnaligned3SIMD( pos1[i + 0].Base(), pos1simd[0] );
			if (pS2[i+1] > 0)
				StoreUnaligned3SIMD( pos1[i + 1].Base(), pos1simd[1] );
			if (pS2[i+2] > 0)
				StoreUnaligned3SIMD( pos1[i + 2].Base(), pos1simd[2] );
			if (pS2[i+3] > 0)
				StoreUnaligned3SIMD( pos1[i + 3].Base(), pos1simd[3] );
			*/

			StoreAlignedSIMD( pos1[i + 0].Base(), MaskedAssign( ( bi32x4 ) splatweights[0], pos1simd[0], originalpos1simd[0] ) );
			StoreAlignedSIMD( pos1[i + 1].Base(), MaskedAssign( ( bi32x4 ) splatweights[1], pos1simd[1], originalpos1simd[1] ) );
			StoreAlignedSIMD( pos1[i + 2].Base(), MaskedAssign( ( bi32x4 ) splatweights[2], pos1simd[2], originalpos1simd[2] ) );
			StoreAlignedSIMD( pos1[i + 3].Base(), MaskedAssign( ( bi32x4 ) splatweights[3], pos1simd[3], originalpos1simd[3] ) );
// 			StoreAligned3SIMD( pos1[i + 0].Base(), MaskedAssign( ( bi32x4 ) splatweights[0], pos1simd[0], originalpos1simd[0] ) );
// 			StoreAligned3SIMD( pos1[i + 1].Base(), MaskedAssign( ( bi32x4 ) splatweights[1], pos1simd[1], originalpos1simd[1] ) );
// 			StoreAligned3SIMD( pos1[i + 2].Base(), MaskedAssign( ( bi32x4 ) splatweights[2], pos1simd[2], originalpos1simd[2] ) );
// 			StoreAligned3SIMD( pos1[i + 3].Base(), MaskedAssign( ( bi32x4 ) splatweights[3], pos1simd[3], originalpos1simd[3] ) );
// 			StoreUnaligned3SIMD( pos1[i + 0].Base(), MaskedAssign( ( bi32x4 ) splatweights[0], pos1simd[0], originalpos1simd[0] ) );
// 			StoreUnaligned3SIMD( pos1[i + 1].Base(), MaskedAssign( ( bi32x4 ) splatweights[1], pos1simd[1], originalpos1simd[1] ) );
// 			StoreUnaligned3SIMD( pos1[i + 2].Base(), MaskedAssign( ( bi32x4 ) splatweights[2], pos1simd[2], originalpos1simd[2] ) );
// 			StoreUnaligned3SIMD( pos1[i + 3].Base(), MaskedAssign( ( bi32x4 ) splatweights[3], pos1simd[3], originalpos1simd[3] ) );

		}

		// take care of stragglers
		// odd that this is like this? for( false ; i < nBoneCount; i++ )
		for( ; i < nBoneCount; i++ )
		{
			weight = pS2[i];
			if ( weight <= 0.0f )
				continue;

			if ( pPoseEntry->seqdesc_flags & STUDIO_POST )
			{

				QuaternionMA_PS3( q1[i], weight, q2[i], q1[i] );
//				QuaternionMASIMD( q1[i], weight, q2[i], q1[i] );

				// FIXME: are these correct?
				pos1[i][0] = pos1[i][0] + pos2[i][0] * weight;
				pos1[i][1] = pos1[i][1] + pos2[i][1] * weight;
				pos1[i][2] = pos1[i][2] + pos2[i][2] * weight;
			}
			else
			{
				QuaternionSM_PS3( weight, q2[i], q1[i], q1[i] );
//				QuaternionSMSIMD( weight, q2[i], q1[i], q1[i] );

				// FIXME: are these correct?
				pos1[i][0] = pos1[i][0] + pos2[i][0] * weight;
				pos1[i][1] = pos1[i][1] + pos2[i][1] * weight;
				pos1[i][2] = pos1[i][2] + pos2[i][2] * weight;
			}
		}

		PopLSStack( sizeof(float) * nBoneCount );

// #if defined(__SPU__)
// 		if( pSPUJob->numBones == 70 )
// 		{
// 			Quaternion *pQ = &q1[1];
// 			VjobSpuLog("q1[1]: %f, %f, %f, %f\n", pQ->x, pQ->y, pQ->z, pQ->w );
// 		}
// #endif


		return;
	}

	//// SLERP PHASE

	// Some bones need to be slerped with alignment.
	// Others do not.
	// Some need to be ignored altogether.
	// Build arrays indicating which are which. 
	// This is the corral approach. Another approach
	// would be to compute both the aligned and unaligned
	// slerps of each bone in the first pass through the 
	// array, and then do a masked selection of each 
	// based on the masks. However there really isn't 
	// a convenient way to turn the int flags that
	// specify which approach to take, into fltx4 masks.

// 	int * RESTRICT aBonesSlerpAlign   = (int *)stackalloc(nBoneCount * sizeof(int));
// 	float * RESTRICT aBonesSlerpAlignWeights     = (float *)stackalloc(nBoneCount * sizeof(float));
// 	int * RESTRICT aBonesSlerpNoAlign = (int *)stackalloc(nBoneCount * sizeof(int));
// 	float * RESTRICT aBonesSlerpNoAlignWeights   = (float *)stackalloc(nBoneCount * sizeof(float));
// 	int		*aBonesSlerpAlign	  		= g_intStack[ 0 ];
// 	float	*aBonesSlerpAlignWeights	= g_floatStack[ 1 ];
// 	int		*aBonesSlerpNoAlign	  		= g_intStack[ 1 ];
// 	float	*aBonesSlerpNoAlignWeights	= g_floatStack[ 2 ];

	int		*aBonesSlerpAlign	  		= (int *)PushLSStack( sizeof(int) * nBoneCount );
	float	*aBonesSlerpAlignWeights	= (float *)PushLSStack( sizeof(float) * nBoneCount );
	int		*aBonesSlerpNoAlign	  		= (int *)PushLSStack( sizeof(int) * nBoneCount );
	float	*aBonesSlerpNoAlignWeights	= (float *)PushLSStack( sizeof(float) * nBoneCount );


	int numBonesSlerpAlign = 0;
	int numBonesSlerpNoAlign = 0;
	
	// BoneQuaternion * RESTRICT testOutput = (BoneQuaternion *)stackalloc(nBoneCount * sizeof(BoneQuaternion));

	// sweep forward through the array and determine where to corral each bone.
	for( i = 0 ; i < nBoneCount ; ++i )
	{
		float weight = pS2[i];
		if( weight == 1.0f )
		{
			q1[i] = q2[i];
			pos1[i] = pos2[i];
		}
		else if( weight > 0.0f ) // ignore small bones
		{	
//			if( pStudioHdr->boneFlags(i) & BONE_FIXED_ALIGNMENT )
			if( pSPUJob->boneFlags[ i ] & BONE_FIXED_ALIGNMENT )
			{
				aBonesSlerpNoAlign[ numBonesSlerpNoAlign ] = i;
				aBonesSlerpNoAlignWeights[ numBonesSlerpNoAlign ] = weight;
				++numBonesSlerpNoAlign;
			}
			else
			{
				aBonesSlerpAlign[ numBonesSlerpAlign ] = i;
				aBonesSlerpAlignWeights[ numBonesSlerpAlign ] = weight;
				++numBonesSlerpAlign;
			}
		}
	}

	// okay, compute all the aligned, and all the unaligned bones, four at
	// a time if possible.
	const fltx4 One = Four_Ones;
	/////////////////
	// // // Aligned!
	nBoneCountRoundedFour = ( numBonesSlerpAlign ) & ~3;
	for( i = 0 ; i < nBoneCountRoundedFour ; i+=4 )
	{
		fltx4 weights = LoadAlignedSIMD( aBonesSlerpAlignWeights+i );
		fltx4 oneMinusWeight = SubSIMD(One, weights);

		// position component:
		// pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * weight;
		fltx4 pos1simd[4];
		fltx4 pos2simd[4];
		pos1simd[0] = LoadAlignedSIMD(pos1[aBonesSlerpAlign[i+0]].Base()); 
		pos1simd[1] = LoadAlignedSIMD(pos1[aBonesSlerpAlign[i+1]].Base()); 
		pos1simd[2] = LoadAlignedSIMD(pos1[aBonesSlerpAlign[i+2]].Base()); 
		pos1simd[3] = LoadAlignedSIMD(pos1[aBonesSlerpAlign[i+3]].Base()); 
		pos2simd[0] = LoadAlignedSIMD(pos2[aBonesSlerpAlign[i+0]].Base()); 
		pos2simd[1] = LoadAlignedSIMD(pos2[aBonesSlerpAlign[i+1]].Base()); 
		pos2simd[2] = LoadAlignedSIMD(pos2[aBonesSlerpAlign[i+2]].Base()); 
		pos2simd[3] = LoadAlignedSIMD(pos2[aBonesSlerpAlign[i+3]].Base()); 
// 		pos1simd[0] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+0]].Base()); 
// 		pos1simd[1] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+1]].Base()); 
// 		pos1simd[2] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+2]].Base()); 
// 		pos1simd[3] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+3]].Base()); 
// 		pos2simd[0] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+0]].Base()); 
// 		pos2simd[1] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+1]].Base()); 
// 		pos2simd[2] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+2]].Base()); 
// 		pos2simd[3] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+3]].Base()); 

		pos1simd[0] = MulSIMD( SplatXSIMD(oneMinusWeight) , pos1simd[0] );
		pos1simd[1] = MulSIMD( SplatYSIMD(oneMinusWeight) , pos1simd[1] );
		pos1simd[2] = MulSIMD( SplatZSIMD(oneMinusWeight) , pos1simd[2] );
		pos1simd[3] = MulSIMD( SplatWSIMD(oneMinusWeight) , pos1simd[3] );

		fltx4 posWriteMasks[4]; // don't overwrite where there was zero weight
		{
			fltx4 splatweights[4];
			fltx4 Zero = Four_Zeros;
			splatweights[0] = SplatXSIMD(weights);
			splatweights[1] = SplatYSIMD(weights);
			splatweights[2] = SplatZSIMD(weights);
			splatweights[3] = SplatWSIMD(weights);

			pos1simd[0] = MaddSIMD( splatweights[0] , pos2simd[0], pos1simd[0] );
				posWriteMasks[0] = ( fltx4 ) CmpGtSIMD(splatweights[0], Zero);
			pos1simd[1] = MaddSIMD( splatweights[1] , pos2simd[1], pos1simd[1] );
				posWriteMasks[1] = ( fltx4 ) CmpGtSIMD(splatweights[1], Zero);
			pos1simd[2] = MaddSIMD( splatweights[2] , pos2simd[2], pos1simd[2] );
				posWriteMasks[2] = ( fltx4 ) CmpGtSIMD(splatweights[2], Zero);
			pos1simd[3] = MaddSIMD( splatweights[3] , pos2simd[3], pos1simd[3] );
				posWriteMasks[3] = ( fltx4 ) CmpGtSIMD(splatweights[3], Zero);
		}


		FourQuaternions q1four, q2four, result;		
		q1four.LoadAndSwizzleAligned(	q1 + aBonesSlerpAlign[i+0],
										q1 + aBonesSlerpAlign[i+1],
										q1 + aBonesSlerpAlign[i+2],
										q1 + aBonesSlerpAlign[i+3] );

#if 0
		// FIXME: the SIMD slerp doesn't handle quaternions that have opposite signs
		q2four.LoadAndSwizzleAligned(	q2 + aBonesSlerpAlign[i+0],
										q2 + aBonesSlerpAlign[i+1],
										q2 + aBonesSlerpAlign[i+2],
										q2 + aBonesSlerpAlign[i+3] );
		result = q2four.Slerp(q1four, oneMinusWeight);
#else
		// force the quaternions to be the same sign (< 180 degree separation)
		BoneQuaternion q20, q21, q22, q23;
		QuaternionAlign_PS3( q1[aBonesSlerpAlign[i+0]], q2[aBonesSlerpAlign[i+0]], q20 );
		QuaternionAlign_PS3( q1[aBonesSlerpAlign[i+1]], q2[aBonesSlerpAlign[i+1]], q21 );
		QuaternionAlign_PS3( q1[aBonesSlerpAlign[i+2]], q2[aBonesSlerpAlign[i+2]], q22 );
		QuaternionAlign_PS3( q1[aBonesSlerpAlign[i+3]], q2[aBonesSlerpAlign[i+3]], q23 );
		q2four.LoadAndSwizzleAligned( &q20, &q21, &q22, &q23 );
		result = q2four.SlerpNoAlign(q1four, oneMinusWeight);
#endif

		result.SwizzleAndStoreAligned( q1 + aBonesSlerpAlign[i+0],
			q1 + aBonesSlerpAlign[i+1],
			q1 + aBonesSlerpAlign[i+2],
			q1 + aBonesSlerpAlign[i+3] );

		StoreAlignedSIMD( pos1[aBonesSlerpAlign[i+0]].Base(), pos1simd[0] );
		StoreAlignedSIMD( pos1[aBonesSlerpAlign[i+1]].Base(), pos1simd[1] );
		StoreAlignedSIMD( pos1[aBonesSlerpAlign[i+2]].Base(), pos1simd[2] );
		StoreAlignedSIMD( pos1[aBonesSlerpAlign[i+3]].Base(), pos1simd[3] );
// 		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+0]].Base(), pos1simd[0] );
// 		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+1]].Base(), pos1simd[1] );
// 		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+2]].Base(), pos1simd[2] );
// 		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+3]].Base(), pos1simd[3] );
	}

	// handle stragglers
//	for( i ; i < numBonesSlerpAlign ; ++i )
	for( ; i < numBonesSlerpAlign ; ++i )
	{
		BoneQuaternion q3;
		weight = aBonesSlerpAlignWeights[i];
		int k = aBonesSlerpAlign[i];

		float s1 = 1.0 - weight;

		QuaternionSlerp_PS3( q2[k], q1[k], s1, q3 );

		q1[k][0] = q3[0];
		q1[k][1] = q3[1];
		q1[k][2] = q3[2];
		q1[k][3] = q3[3];

		pos1[k][0] = pos1[k][0] * s1 + pos2[k][0] * weight;
		pos1[k][1] = pos1[k][1] * s1 + pos2[k][1] * weight;
		pos1[k][2] = pos1[k][2] * s1 + pos2[k][2] * weight;
	}
	///////////////////
	// // // Unaligned!
	nBoneCountRoundedFour = (numBonesSlerpNoAlign) & ~3;
	for( i = 0 ; i < nBoneCountRoundedFour ; i+=4 )
	{
		fltx4 weights = LoadAlignedSIMD( aBonesSlerpNoAlignWeights+i );
		fltx4 oneMinusWeight = SubSIMD(One, weights);

		// position component:
		// pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * weight;
		fltx4 pos1simd[4];
		fltx4 pos2simd[4];
		pos1simd[0] = LoadAlignedSIMD(pos1[aBonesSlerpNoAlign[i+0]].Base()); 
		pos1simd[1] = LoadAlignedSIMD(pos1[aBonesSlerpNoAlign[i+1]].Base()); 
		pos1simd[2] = LoadAlignedSIMD(pos1[aBonesSlerpNoAlign[i+2]].Base()); 
		pos1simd[3] = LoadAlignedSIMD(pos1[aBonesSlerpNoAlign[i+3]].Base()); 
		pos2simd[0] = LoadAlignedSIMD(pos2[aBonesSlerpNoAlign[i+0]].Base()); 
		pos2simd[1] = LoadAlignedSIMD(pos2[aBonesSlerpNoAlign[i+1]].Base()); 
		pos2simd[2] = LoadAlignedSIMD(pos2[aBonesSlerpNoAlign[i+2]].Base()); 
		pos2simd[3] = LoadAlignedSIMD(pos2[aBonesSlerpNoAlign[i+3]].Base()); 
// 		pos1simd[0] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+0]].Base()); 
// 		pos1simd[1] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+1]].Base()); 
// 		pos1simd[2] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+2]].Base()); 
// 		pos1simd[3] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+3]].Base()); 
// 		pos2simd[0] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+0]].Base()); 
// 		pos2simd[1] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+1]].Base()); 
// 		pos2simd[2] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+2]].Base()); 
// 		pos2simd[3] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+3]].Base()); 

		pos1simd[0] = MulSIMD( SplatXSIMD(oneMinusWeight) , pos1simd[0] );
		pos1simd[1] = MulSIMD( SplatYSIMD(oneMinusWeight) , pos1simd[1] );
		pos1simd[2] = MulSIMD( SplatZSIMD(oneMinusWeight) , pos1simd[2] );
		pos1simd[3] = MulSIMD( SplatWSIMD(oneMinusWeight) , pos1simd[3] );

		pos1simd[0] = MaddSIMD( SplatXSIMD(weights) , pos2simd[0], pos1simd[0] );
		pos1simd[1] = MaddSIMD( SplatYSIMD(weights) , pos2simd[1], pos1simd[1] );
		pos1simd[2] = MaddSIMD( SplatZSIMD(weights) , pos2simd[2], pos1simd[2] );
		pos1simd[3] = MaddSIMD( SplatWSIMD(weights) , pos2simd[3], pos1simd[3] );

		FourQuaternions q1four, q2four, result;		
		q1four.LoadAndSwizzleAligned(	q1 + aBonesSlerpNoAlign[i+0],
			q1 + aBonesSlerpNoAlign[i+1],
			q1 + aBonesSlerpNoAlign[i+2],
			q1 + aBonesSlerpNoAlign[i+3] );
		q2four.LoadAndSwizzleAligned(	q2 + aBonesSlerpNoAlign[i+0],
			q2 + aBonesSlerpNoAlign[i+1],
			q2 + aBonesSlerpNoAlign[i+2],
			q2 + aBonesSlerpNoAlign[i+3] );

		result = q2four.SlerpNoAlign(q1four, oneMinusWeight);

		result.SwizzleAndStoreAligned( q1 + aBonesSlerpNoAlign[i+0],
			q1 + aBonesSlerpNoAlign[i+1],
			q1 + aBonesSlerpNoAlign[i+2],
			q1 + aBonesSlerpNoAlign[i+3] );

		StoreAlignedSIMD(pos1[aBonesSlerpNoAlign[i+0]].Base(), pos1simd[0]);
		StoreAlignedSIMD(pos1[aBonesSlerpNoAlign[i+1]].Base(), pos1simd[1]);
		StoreAlignedSIMD(pos1[aBonesSlerpNoAlign[i+2]].Base(), pos1simd[2]);
		StoreAlignedSIMD(pos1[aBonesSlerpNoAlign[i+3]].Base(), pos1simd[3]);
// 		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+0]].Base(), pos1simd[0]);
// 		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+1]].Base(), pos1simd[1]);
// 		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+2]].Base(), pos1simd[2]);
// 		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+3]].Base(), pos1simd[3]);
	}
	// handle stragglers
//	for( i ; i < numBonesSlerpNoAlign ; ++i )
	for( ; i < numBonesSlerpNoAlign ; ++i )
	{
		weight = aBonesSlerpNoAlignWeights[ i ];
		int k = aBonesSlerpNoAlign[ i ];

		float s1 = 1.0 - weight;

		BoneQuaternion q3;
		QuaternionSlerpNoAlign_PS3( q2[ k ], q1[ k ], s1, q3 );

		q1[k][0] = q3[0];
		q1[k][1] = q3[1];
		q1[k][2] = q3[2];
		q1[k][3] = q3[3];

		pos1[k][0] = pos1[k][0] * s1 + pos2[k][0] * weight;
		pos1[k][1] = pos1[k][1] * s1 + pos2[k][1] * weight;
		pos1[k][2] = pos1[k][2] * s1 + pos2[k][2] * weight;
	}

	// aBonesSlerpNoAlignWeights
	PopLSStack( sizeof(float) * nBoneCount );
	// aBonesSlerpNoAlign
	PopLSStack( sizeof(int) * nBoneCount );
	// aBonesSlerpAlignWeights
	PopLSStack( sizeof(float) * nBoneCount );
	// aBonesSlerpAlign
	PopLSStack( sizeof(int) * nBoneCount );

	// pS2
	PopLSStack( sizeof(float) * nBoneCount );
}


template <int N>
struct GetLog2_t
{};
template<>
struct GetLog2_t<0x00100000>
{
	enum {kLog2 = 20};
};


//---------------------------------------------------------------------
// Make sure quaternions are within 180 degrees of one another, if not, reverse q
//---------------------------------------------------------------------
FORCEINLINE fltx4 BoneQuaternionAlignSIMD( const fltx4 &p, const fltx4 &q )
{
	// decide if one of the quaternions is backwards
	bi32x4 cmp = CmpLtSIMD( Dot4SIMD(p,q), Four_Zeros );
	fltx4 result = MaskedAssign( cmp, NegSIMD(q), q );
	return result;
}


// SSE + X360 implementation
FORCEINLINE fltx4 BoneQuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius, result;
	bi32x4 mask;
	radius = Dot4SIMD( q, q );
	mask = CmpEqSIMD( radius, Four_Zeros ); // all ones iff radius = 0
	result = ReciprocalSqrtSIMD( radius );
	result = MulSIMD( result, q );
	return MaskedAssign( mask, q, result );	// if radius was 0, just return q
}





//-----------------------------------------------------------------------------
// Purpose: Inter-animation blend.  Assumes both types are identical.
//			blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------


void BlendBones_PS3( 
				const bonejob_SPU *pBonejob,
				const accumposeentry_SPU *pPoseEntry,
				BoneQuaternion *q1,
				BoneVector *pos1, 
				const int *boneMap, 
				const float *boneWeight, 
				const BoneQuaternion *q2, 
				const BoneVector *pos2, 
				float s,
				int boneMask )
{
	SNPROF_ANIM("BlendBones_PS3");


	int			i, j;
	Quaternion		q3;

	if( s <= 0.0f )
	{
		Assert(0); // shouldn't have been called
		return;
	}
	else if( s >= 1.0f )
	{
		//CMiniProfilerGuard mpguard(&g_lmp_BlendBones1, pStudioHdr->numbones());

		Assert(0); // shouldn't have been called
		for (i = 0; i < pBonejob->numBones; i++)
		{
			// skip unused bones
			if( !( pBonejob->boneFlags[i] & boneMask) )
			{
				continue;
			}

			if( pPoseEntry->pEA_seqgroup_boneMap )
			{
				j = boneMap[i];
			}
			else
			{
				j = i;
			}

			if( j >= 0 && boneWeight[j] > 0.0f )
			{
				q1[i] = q2[i];
				pos1[i] = pos2[i];
			}
		}

		return;
	}

	float s2 = s;
	float s1 = 1.0f - s2;

	//CMiniProfilerGuard mpguard(&g_lmp_BlendBones2,pStudioHdr->numbones()); // 130-180 ticks without profilers; 167-190 ticks with all profilers on

	int nMode = 2;//g_cv_BlendBonesMode.GetInt();
#ifndef	DEDICATED
	if(nMode)
	{
		const int numBones = pBonejob->numBones;
		const int *RESTRICT pBonePseudoWeight = (int*)boneWeight;//(int*)seqdesc.pBoneweight(0);  // we'll treat floats as ints to check for > 0.0
//		int *RESTRICT pActiveBones = (int*)stackalloc(numBones * sizeof(int) * 2), *RESTRICT pActiveBonesEnd = pActiveBones;
		int *pActiveBones = (int *)PushLSStack( numBones * sizeof(int) * 2  );
		int *pActiveBonesEnd = pActiveBones;


		{
//			BONE_PROFILE_LOOP(BlendBoneLoop2a,numBones); // 20 ticks straight; 12-14 ticks 4 at a time; 14-19 ticks 8 at a time (compiler generated code)

			i = 0;
#ifdef _X360 // on PC, this is slower
			for(; i+3 < numBones; i+=4)
			{
				int isBoneActiveA = pStudioHdr->boneFlags(i  ) & boneMask;
				int isBoneActiveB = pStudioHdr->boneFlags(i+1) & boneMask;
				int isBoneActiveC = pStudioHdr->boneFlags(i+2) & boneMask;
				int isBoneActiveD = pStudioHdr->boneFlags(i+3) & boneMask;
				isBoneActiveA = isBoneActiveA | -isBoneActiveA; // the high bit is now 1 iff the flags check 
				isBoneActiveB = isBoneActiveB | -isBoneActiveB; // the high bit is now 1 iff the flags check 
				isBoneActiveC = isBoneActiveC | -isBoneActiveC; // the high bit is now 1 iff the flags check 
				isBoneActiveD = isBoneActiveD | -isBoneActiveD; // the high bit is now 1 iff the flags check 
				isBoneActiveA = _rotl(isBoneActiveA,1) & 1;  // now it's either 0 or 1
				isBoneActiveB = _rotl(isBoneActiveB,1) & 1;  // now it's either 0 or 1
				isBoneActiveC = _rotl(isBoneActiveC,1) & 1;  // now it's either 0 or 1
				isBoneActiveD = _rotl(isBoneActiveD,1) & 1;  // now it's either 0 or 1
				*pActiveBonesEnd = i+0;
				pActiveBonesEnd += isBoneActiveA;
				*pActiveBonesEnd = i+1;
				pActiveBonesEnd += isBoneActiveB;
				*pActiveBonesEnd = i+2;
				pActiveBonesEnd += isBoneActiveC;
				*pActiveBonesEnd = i+3;
				pActiveBonesEnd += isBoneActiveD;
			}
#endif
			for(; i < numBones; ++i)
			{
				*pActiveBonesEnd = i;
				int isBoneActive = pBonejob->boneFlags[i] & boneMask;
				isBoneActive = isBoneActive | -isBoneActive; // the high bit is now 1 iff the flags check 
				isBoneActive = _rotl(isBoneActive,1) & 1;  // now it's either 0 or 1
				pActiveBonesEnd += isBoneActive;
			}
		}

		// now we have a list of bones whose flags & mask != 0
		// we need to create bone pay
		if( pPoseEntry->pEA_seqgroup_boneMap )// if( pSeqGroup )
		{
			int *pEnd = pActiveBones;
			{
//				BONE_PROFILE_LOOP(BlendBoneLoop2b,pActiveBonesEnd - pActiveBones);//21-25 straight; 16-18 4 at a time;

				int *RESTRICT pActiveBone = pActiveBones;
#ifdef _X360 // on PC, this is slower
				for(; pActiveBone + 3 < pActiveBonesEnd; pActiveBone += 4)
				{
					int nActiveBoneA = pActiveBone[0];
					int nActiveBoneB = pActiveBone[1];
					int nActiveBoneC = pActiveBone[2];
					int nActiveBoneD = pActiveBone[3];
					int nMappedBoneA = pSeqGroup->boneMap[nActiveBoneA];
					int nMappedBoneB = pSeqGroup->boneMap[nActiveBoneB];
					int nMappedBoneC = pSeqGroup->boneMap[nActiveBoneC];
					int nMappedBoneD = pSeqGroup->boneMap[nActiveBoneD];
					pEnd[numBones] = nMappedBoneA;
					*pEnd = nActiveBoneA;
					pEnd += _rotl(~nMappedBoneA,1) & 1; // if nMappedBone < 0, don't advance the end
					pEnd[numBones] = nMappedBoneB;
					*pEnd = nActiveBoneB;
					pEnd += _rotl(~nMappedBoneB,1) & 1; // if nMappedBone < 0, don't advance the end
					pEnd[numBones] = nMappedBoneC;
					*pEnd = nActiveBoneC;
					pEnd += _rotl(~nMappedBoneC,1) & 1; // if nMappedBone < 0, don't advance the end
					pEnd[numBones] = nMappedBoneD;
					*pEnd = nActiveBoneD;
					pEnd += _rotl(~nMappedBoneD,1) & 1; // if nMappedBone < 0, don't advance the end
				}
#endif
				for(; pActiveBone < pActiveBonesEnd; ++pActiveBone)
				{
					int nActiveBone = *pActiveBone;
					int nMappedBone = boneMap[ nActiveBone ];
					pEnd[ numBones ] = nMappedBone;
					*pEnd = nActiveBone;
					pEnd += _rotl(~nMappedBone,1) & 1; // if nMappedBone < 0, don't advance the end
				}
			}

			pActiveBonesEnd = pEnd; // the new end of the array of active bones, with negatively-mapped bones taken out
			// now get rid of non-positively-weighted bones
			pEnd = pActiveBones;
			{
//				BONE_PROFILE_LOOP(BlendBoneLoop2c,pActiveBonesEnd - pActiveBones);//18-23 straight; 14-17 ticks 4 at a time

				int *RESTRICT pActiveBone = pActiveBones;
#ifdef _X360 // on PC, this is slower
				int *RESTRICT pMappedBone = pActiveBones+numBones;
				for(; pActiveBone+3 < pActiveBonesEnd; pActiveBone += 4, pMappedBone += 4)
				{
					int nActiveBoneA = pActiveBone[0];
					int nActiveBoneB = pActiveBone[1];
					int nActiveBoneC = pActiveBone[2];
					int nActiveBoneD = pActiveBone[3];
					int nMappedBoneA = pMappedBone[0];
					int nMappedBoneB = pMappedBone[1];
					int nMappedBoneC = pMappedBone[2];
					int nMappedBoneD = pMappedBone[3];
					int pseudoWeightA = pBonePseudoWeight[nMappedBoneA];
					int pseudoWeightB = pBonePseudoWeight[nMappedBoneB];
					int pseudoWeightC = pBonePseudoWeight[nMappedBoneC];
					int pseudoWeightD = pBonePseudoWeight[nMappedBoneD];

					*pEnd = nActiveBoneA;
					pEnd += _rotl(-pseudoWeightA, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
					*pEnd = nActiveBoneB;
					pEnd += _rotl(-pseudoWeightB, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
					*pEnd = nActiveBoneC;
					pEnd += _rotl(-pseudoWeightC, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
					*pEnd = nActiveBoneD;
					pEnd += _rotl(-pseudoWeightD, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
				}
#endif
				for(; pActiveBone < pActiveBonesEnd; ++pActiveBone)
				{
					int nActiveBone = *pActiveBone;
					int nMappedBone = pActiveBone[numBones];
					int pseudoWeight = pBonePseudoWeight[nMappedBone];

					*pEnd = nActiveBone;
					pEnd += _rotl(-pseudoWeight, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
				}
			}
			pActiveBonesEnd = pEnd;
		}
		else
		{
			// one mapping stage off
			// now get rid of non-positively-weighted bones
			int *pEnd = pActiveBones;
//			{BONE_PROFILE_LOOP(BlendBoneLoop2d,pActiveBonesEnd-pActiveBones);//20-50
			for(int *RESTRICT pActiveBone = pActiveBones; pActiveBone < pActiveBonesEnd; ++pActiveBone)
			{
				int nActiveBone = *pActiveBone;
				int pseudoWeight = pBonePseudoWeight[nActiveBone];

				*pEnd = nActiveBone;
				pEnd += _rotl(-pseudoWeight, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
			}//}
			pActiveBonesEnd = pEnd;
		}

		enum
		{
			nBoneFixedAlignmentShift = GetLog2_t<BONE_FIXED_ALIGNMENT>::kLog2
		};

		// NOTE: When merging back to main, enable this code because Fixed-Alignment is not used in L4D, but may be used in main
		fltx4 scale1 = ReplicateX4( s1 );
		fltx4 scale2 = SubSIMD( Four_Ones, scale1 );
		//fltx4 maskW = LoadAlignedSIMD( (const float *)(g_SIMD_ComponentMask[3]) );


		// pass through all active bones to blend them; those that need it are already aligned
		{
			// 120-155 ticks 4 horizontal at a time; 130 ticks with 1 dot quaternion alignment
			// 
//			BONE_PROFILE_LOOP(BlendBoneLoop2g,pActiveBonesEnd-pActiveBones);

			const int *RESTRICT p = pActiveBones, *RESTRICT pNext;
#if 0//ndef _X360
			// swizzled (vertical) 4 at a time processing
			for(; (pNext = p+4) < pActiveBonesEnd; p = pNext)
			{
				int nBoneA = p[0], nBoneB = p[1], nBoneC = p[2], nBoneD = p[3];

				BoneQuaternion *RESTRICT pq1A = &q1[nBoneA]; 
				BoneQuaternion *RESTRICT pq1B = &q1[nBoneB]; 
				BoneQuaternion *RESTRICT pq1C = &q1[nBoneC]; 
				BoneQuaternion *RESTRICT pq1D = &q1[nBoneD]; 

				const BoneQuaternion *RESTRICT pq2A = &q2[nBoneA]; 
				const BoneQuaternion *RESTRICT pq2B = &q2[nBoneB]; 
				const BoneQuaternion *RESTRICT pq2C = &q2[nBoneC]; 
				const BoneQuaternion *RESTRICT pq2D = &q2[nBoneD]; 

				float *pp1A = pos1[nBoneA].Base();
				float *pp1B = pos1[nBoneB].Base();
				float *pp1C = pos1[nBoneC].Base();
				float *pp1D = pos1[nBoneD].Base();

				const float *pp2A = pos2[nBoneA].Base();
				const float *pp2B = pos2[nBoneB].Base();
				const float *pp2C = pos2[nBoneC].Base();
				const float *pp2D = pos2[nBoneD].Base();

				FourQuaternions four4q1, four4q2;
				four4q1.LoadAndSwizzleAligned(pq1A,pq1B,pq1C,pq1D);
				four4q2.LoadAndSwizzleAligned(pq2A,pq2B,pq2C,pq2D);

				FourVectors four4Pos1, four4Pos2;
				four4Pos1.LoadAndSwizzleUnaligned(pp1A,pp1B,pp1C,pp1D);
				four4Pos2.LoadAndSwizzleUnaligned(pp2A,pp2B,pp2C,pp2D);

				four4q1 = QuaternionAlign(four4q2, four4q1);

				FourQuaternions four4Blended = QuaternionNormalize(Madd( four4q1, scale1, Mul( four4q2 , scale2 )));
				// now blend the linear parts
				FourVectors f4PosBlended = Madd(four4Pos1, scale1, Mul(four4Pos2, scale2));
				f4PosBlended.TransposeOntoUnaligned3(*(fltx4*)pp1A, *(fltx4*)pp1B, *(fltx4*)pp1C, *(fltx4*)pp1D);

				four4Blended.SwizzleAndStoreAligned(pq1A,pq1B,pq1C,pq1D);
			}
#else
			// horizontal 4 at a time processing
			for(; (pNext = p+4) < pActiveBonesEnd; p = pNext)
			{
				int nBoneA = p[0], nBoneB = p[1], nBoneC = p[2], nBoneD = p[3];
				float *RESTRICT pq1A = q1[nBoneA].Base(), *pp1A = pos1[nBoneA].Base();
				float *RESTRICT pq1B = q1[nBoneB].Base(), *pp1B = pos1[nBoneB].Base();
				float *RESTRICT pq1C = q1[nBoneC].Base(), *pp1C = pos1[nBoneC].Base();
				float *RESTRICT pq1D = q1[nBoneD].Base(), *pp1D = pos1[nBoneD].Base();
				const float *RESTRICT pq2A = q2[nBoneA].Base(), *pp2A = pos2[nBoneA].Base();
				const float *RESTRICT pq2B = q2[nBoneB].Base(), *pp2B = pos2[nBoneB].Base();
				const float *RESTRICT pq2C = q2[nBoneC].Base(), *pp2C = pos2[nBoneC].Base();
				const float *RESTRICT pq2D = q2[nBoneD].Base(), *pp2D = pos2[nBoneD].Base();
				fltx4 f4q1A = LoadAlignedSIMD(pq1A), f4q2A = LoadAlignedSIMD(pq2A);
				fltx4 f4q1B = LoadAlignedSIMD(pq1B), f4q2B = LoadAlignedSIMD(pq2B);
				fltx4 f4q1C = LoadAlignedSIMD(pq1C), f4q2C = LoadAlignedSIMD(pq2C);
				fltx4 f4q1D = LoadAlignedSIMD(pq1D), f4q2D = LoadAlignedSIMD(pq2D);
				//ALIGN fltx4 f4Pos1A = LoadUnaligned3SIMD(pp1A), f4Pos2A = LoadUnaligned3SIMD(pp2A);
				fltx4 f4Pos1A = LoadAlignedSIMD(pp1A), f4Pos2A = LoadAlignedSIMD(pp2A);
				//ALIGN fltx4 f4Pos1B = LoadUnaligned3SIMD(pp1B), f4Pos2B = LoadUnaligned3SIMD(pp2B);
				fltx4 f4Pos1B = LoadAlignedSIMD(pp1B), f4Pos2B = LoadAlignedSIMD(pp2B);
				//ALIGN fltx4 f4Pos1C = LoadUnaligned3SIMD(pp1C), f4Pos2C = LoadUnaligned3SIMD(pp2C);
				fltx4 f4Pos1C = LoadAlignedSIMD(pp1C), f4Pos2C = LoadAlignedSIMD(pp2C);
				//ALIGN fltx4 f4Pos1D = LoadUnaligned3SIMD(pp1D), f4Pos2D = LoadUnaligned3SIMD(pp2D);
				fltx4 f4Pos1D = LoadAlignedSIMD(pp1D), f4Pos2D = LoadAlignedSIMD(pp2D);
				f4q1A = BoneQuaternionAlignSIMD(f4q2A, f4q1A);
				f4q1B = BoneQuaternionAlignSIMD(f4q2B, f4q1B);
				f4q1C = BoneQuaternionAlignSIMD(f4q2C, f4q1C);
				f4q1D = BoneQuaternionAlignSIMD(f4q2D, f4q1D);
				fltx4 f4BlendedA = MulSIMD( scale2, f4q2A );
				fltx4 f4BlendedB = MulSIMD( scale2, f4q2B );
				fltx4 f4BlendedC = MulSIMD( scale2, f4q2C );
				fltx4 f4BlendedD = MulSIMD( scale2, f4q2D );
				f4BlendedA = MaddSIMD( scale1, f4q1A, f4BlendedA );
				f4BlendedB = MaddSIMD( scale1, f4q1B, f4BlendedB );
				f4BlendedC = MaddSIMD( scale1, f4q1C, f4BlendedC );
				f4BlendedD = MaddSIMD( scale1, f4q1D, f4BlendedD );
				f4BlendedA = BoneQuaternionNormalizeSIMD(f4BlendedA);
				f4BlendedB = BoneQuaternionNormalizeSIMD(f4BlendedB);
				f4BlendedC = BoneQuaternionNormalizeSIMD(f4BlendedC);
				f4BlendedD = BoneQuaternionNormalizeSIMD(f4BlendedD);
				// now blend the linear parts
				fltx4 f4PosBlendedA = MaddSIMD(scale1, f4Pos1A, MulSIMD(scale2,f4Pos2A));
				fltx4 f4PosBlendedB = MaddSIMD(scale1, f4Pos1B, MulSIMD(scale2,f4Pos2B));
				fltx4 f4PosBlendedC = MaddSIMD(scale1, f4Pos1C, MulSIMD(scale2,f4Pos2C));
				fltx4 f4PosBlendedD = MaddSIMD(scale1, f4Pos1D, MulSIMD(scale2,f4Pos2D));
				//f4PosBlended = MaskedAssign(maskW, f4Pos1, f4PosBlended);

				StoreAlignedSIMD(pq1A,f4BlendedA);
				//ALIGN StoreUnaligned3SIMD(pp1A, f4PosBlendedA);
				StoreAlignedSIMD(pp1A, f4PosBlendedA);
				StoreAlignedSIMD(pq1B,f4BlendedB);
				//ALIGN StoreUnaligned3SIMD(pp1B, f4PosBlendedB);
				StoreAlignedSIMD(pp1B, f4PosBlendedB);
				StoreAlignedSIMD(pq1C,f4BlendedC);
				//ALIGN StoreUnaligned3SIMD(pp1C, f4PosBlendedC);
				StoreAlignedSIMD(pp1C, f4PosBlendedC);
				StoreAlignedSIMD(pq1D,f4BlendedD);
				//ALIGN StoreUnaligned3SIMD(pp1D, f4PosBlendedD);
				StoreAlignedSIMD(pp1D, f4PosBlendedD);
			}
#endif
			for(; p < pActiveBonesEnd; ++p)
			{
				int nBone = *p;
				float *RESTRICT pq1 = q1[nBone].Base(), *RESTRICT pp1 = pos1[nBone].Base();
				const float *RESTRICT pq2 = q2[nBone].Base(), *RESTRICT pp2 = pos2[nBone].Base();
				fltx4 f4q1 = LoadAlignedSIMD(pq1), f4q2 = LoadAlignedSIMD(pq2);
				//ALIGN fltx4 f4Pos1 = LoadUnaligned3SIMD(pp1), f4Pos2 = LoadUnaligned3SIMD(pp2);
				fltx4 f4Pos1 = LoadAlignedSIMD(pp1), f4Pos2 = LoadAlignedSIMD(pp2);
				f4q1 = BoneQuaternionAlignSIMD(f4q2, f4q1);
				fltx4 f4Blended = MulSIMD( scale2, f4q2 );
				f4Blended = MaddSIMD( scale1, f4q1, f4Blended );
				f4Blended = BoneQuaternionNormalizeSIMD(f4Blended);
				// now blend the linear parts
				fltx4 f4PosBlended = MaddSIMD(scale1, f4Pos1, MulSIMD(scale2,f4Pos2));
				//f4PosBlended = MaskedAssign(maskW, f4Pos1, f4PosBlended);

				StoreAlignedSIMD(pq1,f4Blended);
				//ALIGN StoreUnaligned3SIMD(pp1, f4PosBlended);
				StoreAlignedSIMD(pp1, f4PosBlended);
			}
		}

		PopLSStack( numBones * sizeof(int) * 2  );
	}
	else
#endif // POSIX
	{
		// 360-400 ticks per loop pass
		// there are usually 40-100 bones on average in a frame
		for( i = 0; i < pBonejob->numBones; i++ ) 
		{
			// skip unused bones
			if( !( pBonejob->boneFlags[i] & boneMask) )
			{
				continue;
			}

			if( pPoseEntry->pEA_seqgroup_boneMap )
			{
				j = boneMap[i];
			}
			else
			{
				j = i;
			}

			if( j >= 0 && boneWeight[j] > 0.0f )
			{
				if( pBonejob->boneFlags[i] & BONE_FIXED_ALIGNMENT)
				{
					QuaternionBlendNoAlign_PS3( q2[i], q1[i], s1, q3 );
				}
				else
				{
					QuaternionBlend_PS3( q2[i], q1[i], s1, q3 );
				}
				q1[i][0] = q3[0];
				q1[i][1] = q3[1];
				q1[i][2] = q3[2];
				q1[i][3] = q3[3];
				pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s2;
				pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s2;
				pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s2;
			}
		}
	}
}




//-----------------------------------------------------------------------------
// Purpose: Scale a set of bones.  Must be of type delta
//-----------------------------------------------------------------------------
void ScaleBones_PS3( 
				const bonejob_SPU *pBonejob,
				const accumposeentry_SPU *pPoseEntry,
				BoneQuaternion *q1, 
				BoneVector *pos1, 
				const int	*boneMap,
				const float *boneWeight,
				float s,
				int boneMask )
{
	SNPROF_ANIM("ScaleBones_PS3");

	int			i, j;
	Quaternion	q3;


	float s2 = s;
	float s1 = 1.0f - s2;

	for (i = 0; i < pBonejob->numBones; i++)
	{
		// skip unused bones
		if( !( pBonejob->boneFlags[i] & boneMask) )
		{
			continue;
		}

		if( pPoseEntry->pEA_seqgroup_boneMap )
		{
			j = boneMap[i];
		}
		else
		{
			j = i;
		}

		if( j >= 0 && boneWeight[j] > 0.0f )
		{
			QuaternionIdentityBlend_PS3( q1[i], s1, q1[i] );
			VectorScale_PS3( pos1[i], s2, pos1[i] );
		}
	}
}




// temp - debugging DMA's

// NOINLINE void *SPUmemcpy_UnalignedGet( void *ls, uint32 ea, uint32_t size )
// {
// 	void *aligned_ls;	
// 
// 	aligned_ls = (void *)((uint32)ls | (ea & 0xf));	// + 0xf in case ls not 16B aligned
// 
// #if defined(__SPU__)
// 	//Msg("GET ls:0x%x, ea:0x%x, size:%d\n", (uint32_t)aligned_ls, ea, size);
// 	// SPU
// 	cellDmaUnalignedGet( aligned_ls, ea, size, DMATAG_ANIM, 0, 0 );
// 	cellDmaWaitTagStatusAny( 1 << DMATAG_ANIM );
// #else
// 	// PPU
// 	memcpy( aligned_ls, (void *)ea, size );
// #endif
// 
// 
// 	return aligned_ls;
// }
// 
// 
// NOINLINE void SPUmemcpy_UnalignedPut( void *ls, uint32 ea, uint32_t size )
// {
// #if defined(__SPU__)
// 	//Msg("PUT ls:0x%x, ea:0x%x, size:%d\n", (uint32_t)ls, ea, size);
// 	// SPU
// 	cellDmaUnalignedPut( ls, ea, size, DMATAG_ANIM, 0, 0 );
// 	cellDmaWaitTagStatusAny( 1 << DMATAG_ANIM );
// #else
// 	Assert(((uint32)ls&0xf) == ea&0xf);
// 
// 	// PPU
// 	memcpy( (void *)ea, ls, size );
// #endif
// }

//==== Copyright (c) 1996-2011, Valve Corporation, All rights reserved. =====//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "mathlib/transform.h"
#include "mathlib/mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
 
const CTransform g_TransformIdentity( Vector( 0.0f, 0.0f, 0.0f ), Quaternion( 0.0f, 0.0f, 0.0f, 1.0f ) );

void SetIdentityTransform( CTransform &out )
{
	out.m_vPosition = vec3_origin;
	out.m_orientation = quat_identity;
}

void ConcatTransforms( const CTransform &in1, const CTransform &in2, CTransform &out )
{
	// Store in temp to avoid problems if out == in1 or out == in2
	CTransform result;
	QuaternionMult( in1.m_orientation, in2.m_orientation, result.m_orientation );
	QuaternionMultiply( in1.m_orientation, in2.m_vPosition, result.m_vPosition );
	result.m_vPosition += in1.m_vPosition;
	out = result;
}

void VectorIRotate( const Vector &v, const CTransform &t, Vector &out )
{
	// FIXME: Make work directly with the transform
	matrix3x4_t m;
	TransformMatrix( t, m );
	VectorIRotate( v, m, out );
}

void VectorITransform( const Vector &v, const CTransform &t, Vector &out )
{
	// FIXME: Make work directly with the transform
	matrix3x4_t m;
	TransformMatrix( t, m );
	VectorITransform( v, m, out );
}

void TransformSlerp( const CTransform &p, const CTransform &q, float t, CTransform &qt )
{
	QuaternionSlerp( p.m_orientation, q.m_orientation, t, qt.m_orientation );
	VectorLerp( p.m_vPosition, q.m_vPosition, t, qt.m_vPosition );
}

void TransformLerp( const CTransform &p, const CTransform &q, float t, CTransform &qt )
{
	QuaternionBlend( p.m_orientation, q.m_orientation, t, qt.m_orientation );
	VectorLerp( p.m_vPosition, q.m_vPosition, t, qt.m_vPosition );
}

void TransformMatrix( const CTransform &in, matrix3x4_t &out )
{
	QuaternionMatrix( in.m_orientation, in.m_vPosition, out );
}

void TransformMatrix( const CTransformUnaligned &in, matrix3x4_t &out )
{
	QuaternionMatrix( in.m_orientation, in.m_vPosition, out );
}

void TransformMatrix( const CTransform &in, const Vector &vScaleIn, matrix3x4_t &out )
{
	QuaternionMatrix( in.m_orientation, in.m_vPosition, vScaleIn, out );
}

void MatrixTransform( const matrix3x4_t &in, CTransformUnaligned &out )
{
	MatrixQuaternion( in, out.m_orientation );
	MatrixGetColumn( in, ORIGIN, out.m_vPosition );
}

void MatrixTransform( const matrix3x4_t &in, CTransform &out )
{
	MatrixQuaternion( in, out.m_orientation );
	MatrixGetColumn( in, ORIGIN, out.m_vPosition );
}

void MatrixTransform( const matrix3x4_t &in, CTransform &out, Vector &vScaleOut )
{
	matrix3x4_t norm;
	vScaleOut = MatrixNormalize( in, norm );
	MatrixTransform( norm, out );
}

void AngleTransform( const QAngle &angles, const Vector &origin, CTransform &out )
{
	AngleQuaternion( angles, out.m_orientation );
	out.m_vPosition = origin;
}

void TransformInvert( const CTransform &in, CTransform &out )
{
	QuaternionInvert( in.m_orientation, out.m_orientation );
	QuaternionMultiply( out.m_orientation, in.m_vPosition, out.m_vPosition );
	out.m_vPosition *= -1.0f;
}

void AxisAngleTransform( const Vector &vecAxis, float flAngleDegrees, CTransform &out )
{
	AxisAngleQuaternion( vecAxis, flAngleDegrees, out.m_orientation );
	out.m_vPosition = vec3_origin;
}

void TransformVectorsFLU( const CTransform &in, Vector* pForward, Vector *pLeft, Vector *pUp )
{
	QuaternionVectorsFLU( in.m_orientation, pForward, pLeft, pUp );
}

void TransformVectorsForward( const CTransform &in, Vector* pForward )
{
	QuaternionVectorsForward( in.m_orientation, pForward );
}

bool TransformsAreEqual( const CTransform &src1, const CTransform &src2, float flPosTolerance, float flRotTolerance )
{
	if ( !VectorsAreEqual( src1.m_vPosition, src2.m_vPosition, flPosTolerance ) )
		return false;
	return QuaternionsAreEqual( src1.m_orientation, src2.m_orientation, flRotTolerance );
}

// FIXME: optimize this with simd goodness
void TransformToWorldSpace( int nRootTransformCount, int nTransformCount, const int *pParentIndices, CTransform *pTransforms )
{
#ifdef _DEBUG
	for ( int i = 0; i < nRootTransformCount; ++i )
	{
		Assert( pParentIndices[i] < 0 );
	}
#endif

	for ( int i = nRootTransformCount; i < nTransformCount; ++i )
	{	
		int nParentBone = pParentIndices[i];
		Assert( nParentBone >= 0 && nParentBone < i );
		ConcatTransforms( pTransforms[ nParentBone ], pTransforms[ i ], pTransforms[ i ] );
	}
}

// FIXME: optimize this with simd goodness
void TransformToParentSpace( int nRootTransformCount, int nTransformCount, const int *pParentIndices, CTransform *pTransforms )
{
#ifdef _DEBUG
	for ( int i = 0; i < nRootTransformCount; ++i )
	{
		Assert( pParentIndices[i] < 0 );
	}
#endif

	bool *pComputedParentTransform = (bool*)stackalloc( nTransformCount * sizeof(bool) );
	memset( pComputedParentTransform, 0, nTransformCount * sizeof(bool) );
	CTransform *pWorldToParentTransforms = (CTransform*)stackalloc( nTransformCount * sizeof(CTransform) );

	for ( int b = nTransformCount; --b >= nRootTransformCount; )
	{
		int nParentBone = pParentIndices[ b ];
		if ( !pComputedParentTransform[ nParentBone ] )
		{
			TransformInvert( pTransforms[ nParentBone ], pWorldToParentTransforms[ nParentBone ] );
			pComputedParentTransform[ nParentBone ] = true;
		}
		ConcatTransforms( pWorldToParentTransforms[ nParentBone ], pTransforms[ b ], pTransforms[ b ] );
	}
}

#endif // !_STATIC_LINKED || _SHARED_LIB


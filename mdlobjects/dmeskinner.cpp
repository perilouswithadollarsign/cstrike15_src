//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// DmeEyeball
//
//=============================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeskinner.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemodel.h"
#include "mathlib/mathlib.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
//  Class for a unit Super Ellipsoid at the origin
//-----------------------------------------------------------------------------
class CSuperEllipsoid
{
public:
	CSuperEllipsoid(
		double powerXZ = 1.0,
		double powerY = 1.0 );

	bool IntersectFromOriginThroughPoint(
		Vector point,
		double dDistance ) const;

	// Evaluate for root finding: [ | x | ^ ( 2 / powerXZ ) + | z | ^ ( 2 / powerXZ ) ] ^ ( powerXZ / powerY ) + | y | ^ powerY - 1 = 0
	double RootEvaluate( const Vector &p ) const;

protected:
	static void EvaluateRayFromOrigin(
		const Vector &rayDirection,
		double dDistance,
		Vector &pointOnRay );

	static bool IntersectUnitBoxFromOrigin(
		const Vector &rayDirection,
		double &dMin,
		double &dMax );

	void SolveHit(
		double v0,
		const Vector &intPoint0,
		double v1,
		const Vector &intPoint1,
		Vector &intPoint ) const;

	static const double s_dEpsilon;

	double m_powerXZ;
	double m_powerY;
	int m_nMaxIterations;
};


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSkinnerVolume, CDmeSkinnerVolume );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSkinnerVolume::OnConstruction()
{
	m_mMatrix.Init( this, "matrix" );
	m_flStrength.Init( this, "strength", 1.0f );
	m_flFalloff.Init( this, "falloff", 0.0f );
	m_nFalloffType.Init( this, "falloffType", FT_LINEAR );
	m_flPowerY.InitAndSet( this, "powerY", 1.0f );
	m_flPowerXZ.InitAndSet( this, "powerXZ", 1.0f );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSkinnerVolume::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSkinnerJoint, CDmeSkinnerJoint );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSkinnerJoint::OnConstruction()
{
	m_mBindWorldMatrix.Init( this, "bindWorldMatrix" );
	m_eVolumeList.Init( this, "volumeList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSkinnerJoint::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSkinner, CDmeSkinner );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSkinner::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSkinner::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSkinner::ReskinMeshes( CDmeModel *pDmeModel, int nJointPerVertexCount )
{
	CUtlStack< CDmeDag * > depthFirstStack;
	depthFirstStack.Push( pDmeModel );

	CDmeDag *pDmeDag;
	CDmeMesh *pDmeMesh;

	while ( depthFirstStack.Count() > 0 )
	{
		depthFirstStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		pDmeMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
		if ( pDmeMesh )
		{
			ReskinMesh( pDmeModel, pDmeMesh, nJointPerVertexCount );
		}

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			depthFirstStack.Push( pDmeDag->GetChild( i ) );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CJointWeight
{
public:
	int m_nJointDataIndex;
	float m_flWeight;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CVertexWeight
{
public:
	CVertexWeight( int nWeightsPerVertex )
	: m_nWeightPerVertex( MAX( 0, nWeightsPerVertex ) )
	{
	}

	int m_nWeightPerVertex;

	CUtlVector< CJointWeight > m_weights;	// Sorted Weight List

	static int SortWeights( const CJointWeight *pLhs, const CJointWeight *pRhs );

	int Count() const {
		return MIN( m_nWeightPerVertex, m_weights.Count() );
	}

	void Reset()
	{
		m_weights.Purge();
	}

	void AddWeight(
		int nJointDataIndex,
		float flWeight );

	void Sort();

	float ComputeTotalWeight() const
	{
		float flTotalWeight = 0.0f;

		const int nWeightCount = Count();

		for ( int i = 0; i < nWeightCount; ++i )
		{
			flTotalWeight += m_weights[ i ].m_flWeight;
		}

		return flTotalWeight;
	}

	float GetWeight( int nWeightIndex ) const
	{
		if ( nWeightIndex < m_weights.Count() )
			return m_weights[ nWeightIndex ].m_flWeight;

		return 0.0f;
	}

	int GetJointIndex( int nWeightIndex ) const
	{
		if ( nWeightIndex < m_weights.Count() )
			return m_weights[ nWeightIndex ].m_nJointDataIndex;

		return 0;
	}
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVertexWeight::AddWeight(
	int nJointDataIndex,
	float flWeight )
{
	CJointWeight &jointWeight = m_weights[ m_weights.AddToTail() ];
	jointWeight.m_nJointDataIndex = nJointDataIndex;
	jointWeight.m_flWeight = flWeight;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVertexWeight::Sort()
{
	m_weights.Sort( SortWeights );
}


//-----------------------------------------------------------------------------
// Sort them from highest to lowest
//-----------------------------------------------------------------------------
int CVertexWeight::SortWeights( const CJointWeight *pLhs, const CJointWeight *pRhs )
{
	if ( pLhs->m_flWeight < pRhs->m_flWeight )
		return 1;

	if ( pLhs->m_flWeight > pRhs->m_flWeight )
		return -1;

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSkinner::ReskinMesh( CDmeModel *pDmeModel, CDmeMesh *pDmeMesh, int nJointPerVertexCount )
{
	if ( !pDmeMesh )
		return false;

	CDmeDag *pDmeDagMeshParent = FindReferringElement< CDmeDag >( pDmeMesh, "shape" );
	if ( !pDmeDagMeshParent )
		return false;

	CDmeVertexData *pDmeVertexData = pDmeMesh->GetBindBaseState();
	if ( !pDmeVertexData )
	{
		Error( "CDmeSkinner: No \"bind\" base state on DmeMesh \"%s\"\n", pDmeMesh->GetName() );
		return false;
	}

	FieldIndex_t nJointWeightsField = -1;
	FieldIndex_t nJointIndicesField = -1;
	pDmeVertexData->CreateJointWeightsAndIndices( nJointPerVertexCount, &nJointWeightsField, &nJointIndicesField );

	if ( nJointWeightsField < 0 || nJointIndicesField < 0 )
	{
		Error( "CDmeSkinner: Couldn't create jointWeights & jointIndices fields on DmeMesh \"%s\"\n", pDmeMesh->GetName() );
		return false;
	}
	pDmeVertexData->RemoveAllVertexData( nJointWeightsField );
	pDmeVertexData->RemoveAllVertexData( nJointIndicesField );

	const CUtlVector< Vector > &positions = pDmeVertexData->GetPositionData();

	CUtlVector< float > jointWeights;
	CUtlVector< int > jointIndices;

	struct s_VolumeStruct
	{
		CDmeSkinnerVolume *m_pDmeSkinnerVolume;
		VMatrix m_mMat;
	};

	struct s_JointStruct
	{
		s_JointStruct()
		: m_pDmeSkinnerJoint( NULL )
		, m_nJointIndex( -1 )
		{
		}

		s_JointStruct( const s_JointStruct &rhs )
		{
			m_pDmeSkinnerJoint = rhs.m_pDmeSkinnerJoint;
			m_nJointIndex = rhs.m_nJointIndex;
			m_volumes.CopyArray( rhs.m_volumes.Base(), m_volumes.Count() );
		}
		CDmeSkinnerJoint *m_pDmeSkinnerJoint;
		int m_nJointIndex;
		CUtlVector< s_VolumeStruct > m_volumes;
	};

	CUtlVector< s_JointStruct > volumeJointList;

	{
		matrix3x4_t gwm;
		pDmeDagMeshParent->GetAbsTransform( gwm );
		VMatrix gwvm;
		gwvm.CopyFrom3x4( gwm );

		VMatrix vm;		// Volume matrix
		VMatrix vmi;	// Volume matrix inverse
		matrix3x4_t m0;
		matrix3x4_t m1;

		CUtlStack< CDmeDag * > depthFirstStack;
		depthFirstStack.Push( this );
		CDmeDag *pDmeDag;
		CDmeSkinnerJoint *pDmeSkinnerJoint;
		CDmeSkinnerVolume *pDmeSkinnerVolume;

		while ( depthFirstStack.Count() > 0 )
		{
			depthFirstStack.Pop( pDmeDag );
			if ( !pDmeDag )
				continue;

			pDmeSkinnerJoint = CastElement< CDmeSkinnerJoint >( pDmeDag );
			if ( pDmeSkinnerJoint )
			{
				if ( pDmeSkinnerJoint->m_eVolumeList.Count() > 0 )
				{
					int nJointIndex = pDmeModel->GetJointIndex( pDmeSkinnerJoint->GetName() );
					if ( nJointIndex < 0 )	// This joint isn't in the joint list as well as it's children, so ignore
					{
						Warning( "DmeSkinner: Skinner Joint %s isn't in DmeModel %s.jointList, ignoring it and all children\n", pDmeSkinnerJoint->GetName(), pDmeModel->GetName() );
						continue;
					}

					s_JointStruct &js = volumeJointList[ volumeJointList.AddToTail() ];
					js.m_pDmeSkinnerJoint = pDmeSkinnerJoint;
					js.m_nJointIndex = nJointIndex;

					for ( int i = 0; i < pDmeSkinnerJoint->m_eVolumeList.Count(); ++i )
					{

						pDmeSkinnerVolume = CastElement< CDmeSkinnerVolume >( pDmeSkinnerJoint->m_eVolumeList[ i ] );
						if ( !pDmeSkinnerVolume )
							continue;

						s_VolumeStruct &vs = js.m_volumes[ js.m_volumes.AddToTail() ];
						vs.m_pDmeSkinnerVolume = pDmeSkinnerVolume;

						pDmeSkinnerVolume->m_mMatrix.Get().MatrixMul( pDmeSkinnerJoint->m_mBindWorldMatrix.Get(), vm );
						vm.InverseGeneral( vmi );
						vmi.MatrixMul( gwvm, vs.m_mMat );
						vs.m_mMat = vs.m_mMat.Transpose();
					}
				}
			}

			for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
			{
				depthFirstStack.Push( pDmeDag->GetChild( i ) );
			}
		}
	}

	Vector v;
	Vector sv;

	CVertexWeight vw( nJointPerVertexCount );

	for ( int i = 0; i < positions.Count(); ++i )
	{
		vw.Reset();

		const Vector &p = positions[ i ];
		for ( int j = 0; j < volumeJointList.Count(); ++j )
		{
			const s_JointStruct &js = volumeJointList[ j ];

			float flMaxWeight = 0.0f;

			for ( int k = 0; k < js.m_volumes.Count(); ++k )
			{
				const s_VolumeStruct &vs = js.m_volumes[ k ];

				const CDmeSkinnerVolume *pDmeSkinnerVolume = vs.m_pDmeSkinnerVolume;
				vs.m_mMat.V3Mul( p, v );

				float f = pDmeSkinnerVolume->m_flFalloff;
				float l = 0.0;
				float w = 0.0;

				if ( pDmeSkinnerVolume->IsEllipse() )
				{
					l = v.Length();
					w = RemapValClamped( l, 1.0f, f + 1.0f, 1.0f, 0.0f );
				}
				else
				{
					double dIFS = 1.0 / ( 1.0 + static_cast< double >( f ) );
					sv.x = v.x * dIFS;
					sv.y = v.y * dIFS;
					sv.z = v.z * dIFS;
					if ( CSuperEllipsoid( pDmeSkinnerVolume->m_flPowerXZ, pDmeSkinnerVolume->m_flPowerY ).IntersectFromOriginThroughPoint( sv, l ) )
					{
						if ( l == 0.0 )
						{
							w = 1.0;
						}
						else
						{
							w = RemapValClamped( sv.Length(), l * dIFS, l, 1.0, 0.0 );
						}
					}
				}

				float s = pDmeSkinnerVolume->m_flStrength;
				switch ( pDmeSkinnerVolume->m_nFalloffType )
				{
				case CDmeSkinnerVolume::FT_SMOOTH:
					w = ( cosf( ( 1.0f - w ) * M_PI ) + 1.0f ) / 2.0f;
					break;
				case CDmeSkinnerVolume::FT_SPIKE:
					w = 1.0f - cosf( w * M_PI / 2.0 );
					break;
				case CDmeSkinnerVolume::FT_DOME:
					w = cosf( ( 1.0f - w ) * M_PI / 2.0 );
					break;
				default:
					break;
				}

				w = fabs( w * s );

				if ( w > 0.00001 && w > flMaxWeight )
				{
					flMaxWeight = w;
				}
			}

			if ( flMaxWeight > 0.0 )
			{
				vw.AddWeight( js.m_nJointIndex, flMaxWeight );
			}
		}

		vw.Sort();

		const float flTotalWeight = vw.ComputeTotalWeight();
		if ( vw.Count() <= 0 || flTotalWeight <= 0 )
		{
			Warning( "Mesh Vertex %s.v[%d] is not influenced by any volume\n", pDmeDagMeshParent->GetName(), i );
		}
		else
		{
			for ( int j = 0; j < nJointPerVertexCount; ++j )
			{
				jointIndices.AddToTail( vw.GetJointIndex( j ) );
				jointWeights.AddToTail( vw.GetWeight( j ) / flTotalWeight );
			}
		}
	}

	pDmeVertexData->AddVertexData( nJointIndicesField, jointIndices.Count() );
	pDmeVertexData->SetVertexData( nJointIndicesField, 0, jointIndices.Count(), AT_INT, jointIndices.Base() );

	pDmeVertexData->AddVertexData( nJointWeightsField, jointWeights.Count() );
	pDmeVertexData->SetVertexData( nJointWeightsField, 0, jointWeights.Count(), AT_FLOAT, jointWeights.Base() );

	return true;
}



//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// TODO: This belongs in a lib somewhere, comes from sdktools/maya/vsSkinner
//
//=============================================================================


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
const double CSuperEllipsoid::s_dEpsilon = 1.0e-6;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSuperEllipsoid::CSuperEllipsoid(
	double powerXZ /* = 1.0 */,
	double powerY /* = 1.0 */ )
: m_powerXZ( powerXZ )
, m_powerY( powerY )
, m_nMaxIterations( 100 )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
double CSuperEllipsoid::RootEvaluate( const Vector &p ) const
{
	return pow( pow( abs( static_cast< double >( p.x ) ), 2.0 / m_powerXZ ) + pow( abs( static_cast< double >( p.z ) ), 2.0 / m_powerXZ ), m_powerXZ / m_powerY ) + pow( abs( static_cast< double >( p.y ) ), 2.0 / m_powerY ) - 1.0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CSuperEllipsoid::IntersectFromOriginThroughPoint(
	Vector point,
	double dDistance ) const
{
	if ( abs( point.x ) < s_dEpsilon ) point.x = 0.0;
	if ( abs( point.y ) < s_dEpsilon ) point.y = 0.0;
	if ( abs( point.z ) < s_dEpsilon ) point.z = 0.0;

	// Point Is The Origin
	if ( point.Length() < s_dEpsilon )
	{
		dDistance = 0.0;
		return true;
	}

	// Check for early exit cases
	const double dVal = RootEvaluate( point );

	// Close enough
	if ( abs( dVal ) < s_dEpsilon )
	{
		dDistance = point.Length();
		return true;
	}

	// Outside of the Super Ellipsoid
	if ( dVal > 0.0 )
		return false;

	Vector rayDirection = point;
	rayDirection.NormalizeInPlace();

	double dMin = 0.0;
	double dMax = 0.0;

	if ( !IntersectUnitBoxFromOrigin( rayDirection, dMin, dMax ) )
		return false;

	// This ought to work!
	dMin = point.Length();

	Vector intPoint0;
	EvaluateRayFromOrigin( rayDirection, dMin, intPoint0 );
	const double v0 = RootEvaluate( intPoint0 );

	if ( abs( v0 ) < s_dEpsilon )
	{
		dDistance = dMin;
		return true;
	}

	Vector intPoint1;
	EvaluateRayFromOrigin( rayDirection, dMax, intPoint1 );
	const double v1 = RootEvaluate( intPoint1 );

	if ( abs( v1 ) < s_dEpsilon )
	{
		dDistance = dMax;
		return true;
	}

	Vector intPoint;
	SolveHit( v0, intPoint0, v1, intPoint1, intPoint );
	dDistance = VectorLength( intPoint );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSuperEllipsoid::EvaluateRayFromOrigin(
	const Vector &rayDirection,
	double dDistance,
	Vector &pointOnRay )
{
	pointOnRay = rayDirection * dDistance; 
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CSuperEllipsoid::IntersectUnitBoxFromOrigin(
	const Vector &rayDirection,
	double &dMin,
	double &dMax )
{
	static const double dMaxValue  = 1.0;
	static const double dMinValue = -dMaxValue;

	const double dMaxBound = sqrt( 3.0 * dMaxValue * dMaxValue );
	const double dMinBound = -dMaxBound;

	double tMin = 0.0;
	double tMax = 0.0;

	/* Left/right. */

	if ( abs( rayDirection.x ) > s_dEpsilon )
	{
		if ( rayDirection.x > s_dEpsilon )
		{
			dMin = dMinValue / rayDirection.x;
			dMax = dMaxValue / rayDirection.x;

			if ( dMax < s_dEpsilon )
				return false;
		}
		else
		{
			dMax = dMinValue / rayDirection.x;

			if ( dMax < s_dEpsilon )
				return false;

			dMin = dMaxValue / rayDirection.x;
		}

		if ( dMin > dMax )
			return false;
	}
	else
	{
		dMin = dMinBound;
		dMax = dMaxBound;
	}

	/* Top/bottom. */

	if ( abs( rayDirection.y ) > s_dEpsilon )
	{
		if ( rayDirection.y > s_dEpsilon )
		{
			tMin = dMinValue / rayDirection.y;
			tMax = dMaxValue / rayDirection.y;
		}
		else
		{
			tMax = dMinValue / rayDirection.y;
			tMin = dMaxValue / rayDirection.y;
		}

		if ( tMax < dMax )
		{
			if ( tMax < s_dEpsilon )
				return false;

			if ( tMin > dMin )
			{
				if ( tMin > tMax )
					return false;

				dMin = tMin;
			}
			else
			{
				if ( dMin > tMax )
					return false;
			}

			dMax = tMax;
		}
		else
		{
			if ( tMin > dMin )
			{
				if ( tMin > dMax )
					return false;

				dMin = tMin;
			}
		}
	}

	/* Front/back. */

	if ( abs( rayDirection.z ) > s_dEpsilon )
	{
		if ( rayDirection.z > s_dEpsilon )
		{
			tMin = dMinValue / rayDirection.z;
			tMax = dMaxValue / rayDirection.z;
		}
		else
		{
			tMax = dMinValue / rayDirection.z;
			tMin = dMaxValue / rayDirection.z;
		}

		if ( tMax < dMax )
		{
			if ( tMax < s_dEpsilon )
				return false;

			if ( tMin > dMin )
			{
				if ( tMin > tMax )
					return false;

				dMin = tMin;
			}
			else
			{
				if ( dMin > tMax )
					return false;
			}

			dMax = tMax;
		}
		else
		{
			if ( tMin > dMin )
			{
				if ( tMin > dMax )
					return false;

				dMin = tMin;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSuperEllipsoid::SolveHit(
	double v0,
	const Vector &intPoint0,
	double v1,
	const Vector &intPoint1,
	Vector &intPoint ) const
{
	Vector p0 = intPoint0;
	Vector p1 = intPoint1;

	double x;
	Vector p2;
	double v2;
	Vector p3;
	double v3;

	for ( int i = 0; i < m_nMaxIterations; ++i )
	{
		if ( abs( v0 ) < s_dEpsilon )
		{
			intPoint = p0;
			return;
		}

		if ( fabs( v1 ) < s_dEpsilon )
		{
			intPoint = p1;
			return;
		}

		x = abs( v0 ) / abs( v1 - v0 );
		VectorSubtract( p1, p0, p2 );
		VectorMultiply( p2, x, p2 );
		VectorAdd( p0, p2, p2 );

		v2 = RootEvaluate( p2 );

		VectorSubtract( p1, p0, p3 );
		VectorMultiply( p3, 0.5, p3 );
		VectorAdd( p0, p3, p3 );

		v3 = RootEvaluate( p3 );

		if ( v2 * v3 < 0.0 )
		{
			v0 = v2;
			p0 = p2;
			v1 = v3;
			p1 = p3;
		}
		else
		{
			if ( abs( v2 ) < abs( v3 ) )
			{
				if ( v0 * v2 < 0.0 )
				{
					v1 = v2;
					p1 = p2;
				}
				else
				{
					v0 = v2;
					p0 = p2;
				}
			}
			else
			{
				if ( v0 * v3 < 0.0 )
				{
					v1 = v3;
					p1 = p3;
				}
				else
				{
					v0 = v3;
					p0 = p3;
				}
			}
		}
	}

	if ( fabs( v0 ) < fabs( v1 ) )
	{
		intPoint = p0;
	}
	else
	{
		intPoint = p1;
	}
}
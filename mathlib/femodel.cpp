//========= Copyright © Valve Corporation, All rights reserved. ============//
// 
// Quat-dominant finite element mesh simulation, a distillation of experiments made in femodel-v0
// Multigrid didn't work as well as advertised, so I dropped it - this is a single-level solver without hierarchy
// CG works really well, but is not very stable due to linearization issues, so it may make it here eventually as an auxiliary pass 
// Warmstarting works well, but doesn't offer any advantages in extreme cases (it can even add energy sometimes), so I don't see it as high value add-on
// The latest simple axial bend model ( distribute parallel impulses across 4 vertices of 2 adjacent triangles, precompute proportions) worked very well
// 2D LSq shape matching for triangles worked really well, too.
// So I found an fast approximate solution to Wahba's problem, which allows me to quickly shape-match 
// more than 3 vertices, with different masses. 2D LSq is still used for 2-vert-constrained elements
//
#include "mathlib/femodel.h"
#include "mathlib/cholesky.h"
#include "mathlib/ssecholesky.h"
#include "tier0/microprofiler.h"
#include "mathlib/svd.h"
#include "mathlib/femodel.inl"


const fltx4 Four_SinCosRotation2DMinWeights = { 1e-14f, 1e-14f, 1e-14f, 1e-14f };



void RelaxRod( const FeRodConstraint_t &c, float flModelScale, VectorAligned &a, VectorAligned &b )
{
	Vector vDist = b - a;
	float flDist = vDist.Length( );
	float flReqDist;
	if ( flDist < flModelScale * c.flMinDist )
	{
		flReqDist = flModelScale * c.flMinDist;
	}
	else if ( flDist > flModelScale * c.flMaxDist )
	{
		flReqDist = flModelScale * c.flMaxDist;
	}
	else
	{
		return; // no need to adjust the distance
	}
	Vector vDelta = vDist * ( c.flRelaxationFactor * ( flReqDist / flDist - 1 ) );
	a -= vDelta * c.flWeight0;
	b += vDelta * ( 1 - c.flWeight0 );
}

//----------------------------------------------------------------------------------------------------------
void CFeModel::RelaxRods( VectorAligned *pPos, float flStiffness, float flModelScale )const
{
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nRodCount - 1 );
	for ( uint i = 0; i < m_nRodCount; ++i )
	{
		const FeRodConstraint_t &c = m_pRods[ i ];
		VectorAligned &a = pPos[ c.nNode[ 0 ] ], &b = pPos[ c.nNode[ 1 ] ];
		RelaxRod( c, flModelScale, a, b );
	}
}



//----------------------------------------------------------------------------------------------------------
void CFeModel::RelaxRods2( VectorAligned *pPos, float flStiffness, float flModelScale )const
{
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nRodCount - 1 );
	for ( uint i = 0; i < m_nRodCount; ++i )
	{
		FeRodConstraint_t c = m_pRods[ i ];
		VectorAligned &a = pPos[ c.nNode[ 0 ] ], &b = pPos[ c.nNode[ 1 ] ];
		Vector vDist = b - a;
		float flDist = vDist.Length();
		float flReqDist = Min( Max( flDist, flModelScale * c.flMinDist ), flModelScale * c.flMaxDist );
		Vector vDelta = vDist * ( c.flRelaxationFactor * ( flReqDist / flDist - 1 ) );
		a -= vDelta * c.flWeight0;
		b += vDelta * ( 1 - c.flWeight0 );
	}
}


//----------------------------------------------------------------------------------------------------------
void CFeModel::RelaxRods2Ftl( VectorAligned *pPos, float flStiffness, float flModelScale )const
{
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nRodCount - 1 );
	for ( uint i = 0; i < m_nRodCount; ++i )
	{
		FeRodConstraint_t c = m_pRods[ i ];
		VectorAligned &a = pPos[ c.nNode[ 0 ] ], &b = pPos[ c.nNode[ 1 ] ];
		Vector vDist = b - a;
		float flDist = vDist.Length( );
		float flReqDist = Min( Max( flDist, flModelScale * c.flMinDist ), flModelScale * c.flMaxDist );
		Vector vDelta = vDist * ( c.flRelaxationFactor * ( flReqDist / flDist - 1 ) );
		b += vDelta;
	}
}


//----------------------------------------------------------------------------------------------------------
void CFeModel::RelaxRodsUninertial( VectorAligned *pPos1, VectorAligned *pPos0, float flStiffness, float flModelScale )const
{
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nRodCount - 1 );
	for ( uint i = 0; i < m_nRodCount; ++i )
	{
		FeRodConstraint_t c = m_pRods[ i ];
		VectorAligned &a1 = pPos1[ c.nNode[ 0 ] ], &b1 = pPos1[ c.nNode[ 1 ] ];
		Vector vDist = b1 - a1;
		float flDist = vDist.Length( ), flMaxDist = flModelScale * c.flMaxDist;
		if ( flDist > flMaxDist )
		{
			VectorAligned &a0 = pPos0[ c.nNode[ 0 ] ], &b0 = pPos0[ c.nNode[ 1 ] ];
			Vector vDelta = vDist * ( c.flRelaxationFactor * ( flMaxDist / flDist - 1 ) );
			a1 -= vDelta * c.flWeight0;
			b1 += vDelta * ( 1 - c.flWeight0 );
			a0 -= vDelta * c.flWeight0;
			b0 += vDelta * ( 1 - c.flWeight0 );
		}
	}
}


const fltx4 Four_2ToTheMinus30 = { 1.0f / float( 1 << 30 ), 1.0f / float( 1 << 30 ), 1.0f / float( 1 << 30 ), 1.0f / float( 1 << 30 ) };
const fltx4 Four_MaxWorldSize = { 1 << 20, 1 << 20, 1 << 20, 1 << 20 };


//----------------------------------------------------------------------------------------------------------
// 128 ticks (32 ticks per rod) on an i7
void CFeModel::RelaxSimdRods( fltx4 *pPos, float flStiffness, float flModelScale )const
{
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nSimdRodCount * 4 - 1 );
	fltx4 /*f4Stiffness = ReplicateX4( flStiffness ),*/ f4ModelScale = ReplicateX4( flModelScale );
	for ( uint i = 0; i < m_nSimdRodCount; ++i )
	{
		const FeSimdRodConstraint_t &c = m_pSimdRods[ i ];
		AssertDbg( c.nNode[ 0 ][ 0 ] < m_nNodeCount && c.nNode[ 1 ][ 0 ] < m_nNodeCount && c.nNode[ 0 ][ 1 ] < m_nNodeCount && c.nNode[ 1 ][ 1 ] < m_nNodeCount &&  c.nNode[ 0 ][ 2 ] < m_nNodeCount && c.nNode[ 1 ][ 2 ] < m_nNodeCount && c.nNode[ 0 ][ 3 ] < m_nNodeCount && c.nNode[ 1 ][ 3 ] < m_nNodeCount );
		FourVectors a, b;
		LOAD_NODES( a, c.nNode[ 0 ] );
		LOAD_NODES( b, c.nNode[ 1 ] );
		FourVectors vDist = b - a;
		fltx4 f4DistSqr = vDist.LengthSqr( );

		{
			// dealing with collapsed nodes, when two nodes have exactly the same coordinates
			f4DistSqr = MaxSIMD( f4DistSqr, Four_2ToTheMinus30 );
		}
		
		fltx4 f4DistRcp = ReciprocalSqrtSIMD( f4DistSqr );
		fltx4 f4Dist = f4DistRcp * f4DistSqr;
		fltx4 f4ReqDist = MinSIMD( MaxSIMD( f4Dist, f4ModelScale * c.f4MinDist ), f4ModelScale * c.f4MaxDist );
		FourVectors vDelta = vDist * ( c.f4RelaxationFactor * ( f4ReqDist * f4DistRcp - Four_Ones ) );
		a -= vDelta * c.f4Weight0;
		b += vDelta * ( Four_Ones - c.f4Weight0 );

		AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, a.Length( ) + b.Length( ) ) );

		SAVE_NODES( a, c.nNode[ 0 ] );
		SAVE_NODES( b, c.nNode[ 1 ] );
	}
}

//----------------------------------------------------------------------------------------------------------
// 128 ticks (32 ticks per rod) on an i7
void CFeModel::RelaxSimdRodsFtl( fltx4 *pPos, float flStiffness, float flModelScale )const
{
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nSimdRodCount * 4 - 1 );
	fltx4 /*f4Stiffness = ReplicateX4( flStiffness ),*/ f4ModelScale = ReplicateX4( flModelScale );
	for ( uint i = 0; i < m_nSimdRodCount; ++i )
	{
		const FeSimdRodConstraint_t &c = m_pSimdRods[ i ];
		AssertDbg( c.nNode[ 0 ][ 0 ] < m_nNodeCount && c.nNode[ 1 ][ 0 ] < m_nNodeCount && c.nNode[ 0 ][ 1 ] < m_nNodeCount && c.nNode[ 1 ][ 1 ] < m_nNodeCount &&  c.nNode[ 0 ][ 2 ] < m_nNodeCount && c.nNode[ 1 ][ 2 ] < m_nNodeCount && c.nNode[ 0 ][ 3 ] < m_nNodeCount && c.nNode[ 1 ][ 3 ] < m_nNodeCount );
		FourVectors a, b;
		LOAD_NODES( a, c.nNode[ 0 ] );
		LOAD_NODES( b, c.nNode[ 1 ] );
		FourVectors vDist = b - a;
		fltx4 f4DistSqr = vDist.LengthSqr( );

		{
			// dealing with collapsed nodes, when two nodes have exactly the same coordinates
			f4DistSqr = MaxSIMD( f4DistSqr, Four_2ToTheMinus30 );
		}

		fltx4 f4DistRcp = ReciprocalSqrtSIMD( f4DistSqr );
		fltx4 f4Dist = f4DistRcp * f4DistSqr;
		fltx4 f4ReqDist = MinSIMD( MaxSIMD( f4Dist, f4ModelScale * c.f4MinDist ), f4ModelScale * c.f4MaxDist );
		FourVectors vDelta = vDist * ( c.f4RelaxationFactor * ( f4ReqDist * f4DistRcp - Four_Ones ) );
		b += vDelta;

		AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, a.Length( ) + b.Length( ) ) );

		SAVE_NODES( b, c.nNode[ 1 ] );
	}
}



//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxTris( VectorAligned *pPos, float flStiffness, float flModelScale )const
{
	float flSumError = 0;
	// first the simplest case of quads hanging on the edge 0-1: this is a 2D case with 2 moving nodes and only 1 DOF (rotation around edge 0-1)
	for ( uint i = 0; i < m_nTriCount[ 2 ]; ++i )
	{
		const FeTri_t &tri = m_pTris[ i ];
		flSumError += RelaxTri2( flStiffness, flModelScale, tri, pPos[ tri.nNode[ 0 ] ], pPos[ tri.nNode[ 1 ] ], pPos[ tri.nNode[ 2 ] ] );
	}

	for ( uint i = m_nTriCount[ 2 ]; i < m_nTriCount[ 1 ]; ++i )
	{
		const FeTri_t &tri = m_pTris[ i ];
		flSumError += RelaxTri1( flStiffness, flModelScale, tri, pPos[ tri.nNode[ 0 ] ], pPos[ tri.nNode[ 1 ] ], pPos[ tri.nNode[ 2 ] ] );
	}

	for ( uint i = m_nTriCount[ 1 ]; i < m_nTriCount[ 0 ]; ++i )
	{
		const FeTri_t &tri = m_pTris[ i ];
		flSumError += RelaxTri0( flStiffness, flModelScale, tri, pPos[ tri.nNode[ 0 ] ], pPos[ tri.nNode[ 1 ] ], pPos[ tri.nNode[ 2 ] ] );
	}
	return flSumError;
}



//----------------------------------------------------------------------------------------------------------
fltx4 CFeModel::RelaxSimdTris( VectorAligned *pPos, float flStiffness, float flModelScale )const
{
	fltx4 flSumError = Four_Zeros, fl4Stiffness = ReplicateX4( flStiffness ), fl4ModelScale = ReplicateX4( flModelScale);
	// first the simplest case of quads hanging on the edge 0-1: this is a 2D case with 2 moving nodes and only 1 DOF (rotation around edge 0-1)
	for ( uint i = 0; i < m_nSimdTriCount[ 2 ]; ++i )
	{
		const FeSimdTri_t &tri = m_pSimdTris[ i ];
		flSumError += RelaxSimdTri2( fl4Stiffness, fl4ModelScale, tri, ( fltx4* ) pPos );
	}

	for ( uint i = m_nSimdTriCount[ 2 ]; i < m_nSimdTriCount[ 1 ]; ++i )
	{
		const FeSimdTri_t &tri = m_pSimdTris[ i ];
		flSumError += RelaxSimdTri1( fl4Stiffness, fl4ModelScale, tri, ( fltx4* ) pPos );
	}

	for ( uint i = m_nSimdTriCount[ 1 ]; i < m_nSimdTriCount[ 0 ]; ++i )
	{
		const FeSimdTri_t &tri = m_pSimdTris[ i ];
		flSumError += RelaxSimdTri0( fl4Stiffness, fl4ModelScale, tri, ( fltx4* ) pPos );
	}
	return flSumError;
}



//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxQuads( VectorAligned *pPos, float flStiffness, float flModelScale, int nExperimental )const
{
	float flSumError = 0;
	// first the simplest case of quads hanging on the edge 0-1: this is a 2D case with 2 moving nodes and only 1 DOF (rotation around edge 0-1)
	for ( uint i = 0; i < m_nQuadCount[ 2 ]; ++i )
	{
		const FeQuad_t &quad = m_pQuads[ i ];
		flSumError += RelaxQuad2( flStiffness, flModelScale, quad, pPos[ quad.nNode[ 0 ] ], pPos[ quad.nNode[ 1 ] ], pPos[ quad.nNode[ 2 ] ], pPos[ quad.nNode[ 3 ] ] );
	}

	for ( uint i = m_nQuadCount[ 2 ]; i < m_nQuadCount[ 1 ]; ++i )
	{
		const FeQuad_t &quad = m_pQuads[ i ];
		flSumError += RelaxQuad1( flStiffness, flModelScale, quad, pPos[ quad.nNode[ 0 ] ], pPos[ quad.nNode[ 1 ] ], pPos[ quad.nNode[ 2 ] ], pPos[ quad.nNode[ 3 ] ] );
	}
	if ( nExperimental )
	{
		for ( uint i = m_nQuadCount[ 1 ]; i < m_nQuadCount[ 0 ]; ++i )
		{
			const FeQuad_t &quad = m_pQuads[ i ];
			flSumError += RelaxQuad0flat( flStiffness, flModelScale, quad, pPos[ quad.nNode[ 0 ] ], pPos[ quad.nNode[ 1 ] ], pPos[ quad.nNode[ 2 ] ], pPos[ quad.nNode[ 3 ] ] );
		}
	}
	else
	{
		for ( uint i = m_nQuadCount[ 1 ]; i < m_nQuadCount[ 0 ]; ++i )
		{
			const FeQuad_t &quad = m_pQuads[ i ];
			flSumError += RelaxQuad0( flStiffness, flModelScale, quad, pPos[ quad.nNode[ 0 ] ], pPos[ quad.nNode[ 1 ] ], pPos[ quad.nNode[ 2 ] ], pPos[ quad.nNode[ 3 ] ] );
		}
	}
	return flSumError;
}

//----------------------------------------------------------------------------------------------------------
// Measured on i7-4930K Ivy Bridge: ~600 ticks / 4 quads
fltx4 CFeModel::RelaxSimdQuads( VectorAligned *pPos, float flStiffness, float flModelScale, int nExperimental )const
{
	fltx4 flSumError = Four_Zeros, fl4Stiffness = ReplicateX4( flStiffness ), fl4ModelScale = ReplicateX4( flModelScale );
	// first the simplest case of quads hanging on the edge 0-1: this is a 2D case with 2 moving nodes and only 1 DOF (rotation around edge 0-1)
	for ( uint i = 0; i < m_nSimdQuadCount[ 2 ]; ++i )
	{
		const FeSimdQuad_t &quad = m_pSimdQuads[ i ];
		flSumError += RelaxSimdQuad2( fl4Stiffness, fl4ModelScale, quad, ( fltx4* )pPos );
	}

	for ( uint i = m_nSimdQuadCount[ 2 ]; i < m_nSimdQuadCount[ 1 ]; ++i )
	{
		const FeSimdQuad_t &quad = m_pSimdQuads[ i ];
		flSumError += RelaxSimdQuad1( fl4Stiffness, fl4ModelScale, quad, ( fltx4* ) pPos );
	}

	if ( nExperimental )
	{
		for ( uint i = m_nSimdQuadCount[ 1 ]; i < m_nSimdQuadCount[ 0 ]; ++i )
		{
			const FeSimdQuad_t &quad = m_pSimdQuads[ i ];
			flSumError += RelaxSimdQuad0flat( fl4Stiffness, fl4ModelScale, quad, ( fltx4* ) pPos );
		}
	}
	else
	{
		for ( uint i = m_nSimdQuadCount[ 1 ]; i < m_nSimdQuadCount[ 0 ]; ++i )
		{
			const FeSimdQuad_t &quad = m_pSimdQuads[ i ];
			flSumError += RelaxSimdQuad0( fl4Stiffness, fl4ModelScale, quad, ( fltx4* ) pPos );
		}
	}
	return flSumError;
}




//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxTri2( float flStiffness, float flModelScale, const FeTri_t &tri, const VectorAligned &p0, const VectorAligned &p1, VectorAligned &p2 )const
{
	CFeTriBasis basis( p1 - p0, p2 - p0 ); // p1-p0 is the best choice for the axis, because we're rotating around it
	p2 = p0 + basis.LocalXYToWorld( flModelScale * tri.v2.x + 0.5f * ( basis.v1x - flModelScale * tri.v1x ), flModelScale * tri.v2.y );
	return 0;
}



FORCEINLINE float CrossProductZ( float v1x, float v1y, float v2x, float v2y )
{
	return v1x * v2y - v1y * v2x;
}


//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxTri1( float flStiffness, float flModelScale, const FeTri_t &tri, const VectorAligned &p0, VectorAligned &p1, VectorAligned &p2 )const
{
	CFeTriBasis basis( p1 - p0, p2 - p0 ); // rotating around p0, because center of mass = p0, because it's the only infinite-mass corner of this triangle
	float tri_v1x = flModelScale * tri.v1x;
	Vector2D tri_v2 = flModelScale * tri.v2;
	CSinCosRotation2D rotation( tri.w2 * DotProduct2D( tri_v2, basis.v2 ) + tri.w1 * ( tri_v1x * basis.v1x ), tri.w2 * CrossProductZ( tri_v2, basis.v2 ) );

	// we computed omega (sin, cos), apply it..
	p1 = p0 + basis.LocalXYToWorld( rotation.m_flCos * tri_v1x, rotation.m_flSin * tri_v1x );
	p2 = p0 + basis.LocalXYToWorld( rotation * tri_v2 );

	return 1.0f - rotation.m_flCos;
}



//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxTri0( float flStiffness, float flModelScale, const FeTri_t &tri, VectorAligned &p0, VectorAligned &p1, VectorAligned &p2 )const
{
	CFeTriBasis basis( p1 - p0, p2 - p0 ); // rotating around p0, because center of mass = p0, because it's the only infinite-mass corner of this triangle
	float tri_v1x = flModelScale * tri.v1x;
	Vector2D tri_v2 = flModelScale * tri.v2;
	Vector2D x0neg( basis.v1x * tri.w1 + basis.v2.x * tri.w2, basis.v2.y * tri.w2 ); // center of mass of the deformed triangle p0,p1,p2 in local coordinate system
	Vector2D r0neg( tri_v1x * tri.w1 + tri_v2.x * tri.w2, tri_v2.y * tri.w2 ); // center of mass of the target triangle configuration, in its coordinate system
	float w0 = 1.0f - ( tri.w1 + tri.w2 );

	Vector2D x2 = basis.v2 - x0neg, r2 = tri_v2 - r0neg;
	float x1x = basis.v1x - x0neg.x; // x1.y = -x0neg.y
	float r1x = tri_v1x - r0neg.x; // r1.y = -r0neg.y

	CSinCosRotation2D rotation( 
		tri.w2 * DotProduct2D( x2, r2 ) + tri.w1 * ( x1x * r1x ) + w0 * ( x0neg.x * r0neg.x ) + ( tri.w1 + w0 ) * ( x0neg.y * r0neg.y ),
		tri.w2 * CrossProductZ( r2, x2 ) - tri.w1 * CrossProductZ( r1x, r0neg.y, x1x, x0neg.y ) + w0 * CrossProductZ( r0neg, x0neg )
	);

#ifdef _DEBUG
	Vector2D x0 = -x0neg, x1 = Vector2D( basis.v1x, 0 ) + x0;
	Vector2D r0 = -r0neg, r1 = Vector2D( tri_v1x, 0 ) + r0;

	CSinCosRotation2D rotation2(
		tri.w2 * DotProduct2D( x2, r2 ) + tri.w1 * DotProduct2D( x1, r1 ) + w0 * DotProduct2D( x0, r0 ),
		tri.w2 * CrossProductZ( r2, x2 ) + tri.w1 * CrossProductZ( r1, x1 ) + w0 * CrossProductZ( r0, x0 )
		);

	Assert( Diff( rotation, rotation2 ) < 1e-4f );

	Vector2D y0 = rotation * r0, y1 = rotation * r1, y2 = rotation * r2;

	CSinCosRotation2D rotation3(
		tri.w2 * DotProduct2D( y2, x2 ) + tri.w1 * DotProduct2D( y1, x1 ) + w0 * DotProduct2D( y0, x0 ),
		tri.w2 * CrossProductZ( y2, x2 ) + tri.w1 * CrossProductZ( y1, x1 ) + w0 * CrossProductZ( y0, x0 )
		);
	Assert( fabsf( rotation3.m_flSin ) < 1e-5f );

#endif

	Vector2D dX0 = x0neg - rotation * r0neg;

	// we computed omega (sin, cos), apply it..
	p0 += basis.LocalXYToWorld( dX0 );
	p1 = p0 + basis.LocalXYToWorld( rotation.m_flCos * tri_v1x, rotation.m_flSin * tri_v1x );
	p2 = p0 + basis.LocalXYToWorld( rotation * tri_v2 );

	return 1.0f - rotation.m_flCos;
}




//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxQuad2( float flStiffness, float flModelScale, const FeQuad_t &quad, const VectorAligned &p0, const VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const
{
	Vector vCoM = ( p0 + p1 ) * 0.5f;
	CFeBasis basis( p1 - p0, p2 + p3 - 2 * p0 ); // p1-p0 is the best choice for the axis, because we're rotating around it	; Should be synced up with Builder:AdjustQuads

	// Numerical stability note: if we choose cross product or some other nice sounding but insanely arbitrary axis to rotate around here,
	// not only is it wrong, it'd also make it impossible to do a square element with diagonal static and diagonal dynamic elements, 
	// because the order of vertices around this quad would not be  natural (the diagonal vertices have to go first for the solve because that's our convention here)
	// so p2-p0 and p3-p1 tentative axes would be parallel, producing a very unstable solve (since we don't solve full Wahba problem, but just finding an approximate solution; 
	// although in this very special case of RelaxQuad2, because we make the problem essentially 2D, we solve pretty close)

	// assume center of mass = p0, because it doesn't matter where it is along the line p0..p1
	// find omega, refer to wahba1d.nb. We only have points 2 and 3 to compute and move, and even that only in the plane YZ (X is fixed, as we rotate around axis X)
	Vector x2 = p2 - vCoM, x3 = p3 - vCoM;
	Vector2D vLocalP2 = basis.WorldToLocalYZ( x2 ), vLocalP3 = basis.WorldToLocalYZ( x3 );
	// Note: e x X = CrossProduct( vAxisX, (?, x1, x2) ) = (0, -x2, x1 ). Note that we rename YZ->XY for Vector2D conversion, so (-x2, x1) -> ( -p.y, p.x )
	float flMass2 = quad.vShape[ 2 ].w, flMass3 = quad.vShape[ 3 ].w;
	CSinCosRotation2D rotation( ( vLocalP2.x * quad.vShape[ 2 ].y + vLocalP2.y * quad.vShape[ 2 ].z ) * flMass2 + ( vLocalP3.x * quad.vShape[ 3 ].y + vLocalP3.y * quad.vShape[ 3 ].z ) * flMass3,
								 ( vLocalP2.x * quad.vShape[ 2 ].z - vLocalP2.y * quad.vShape[ 2 ].y ) * flMass2 + ( vLocalP3.x * quad.vShape[ 3 ].z - vLocalP3.y * quad.vShape[ 3 ].y ) * flMass3 );
	Vector r2 = basis.LocalToWorld( quad.vShape[ 2 ].x, quad.vShape[ 2 ].y * rotation.GetCosine() - quad.vShape[ 2 ].z * rotation.GetSine(), quad.vShape[ 2 ].z * rotation.GetCosine() + quad.vShape[ 2 ].y * rotation.GetSine() );
	Vector r3 = basis.LocalToWorld( quad.vShape[ 3 ].x, quad.vShape[ 3 ].y * rotation.GetCosine() - quad.vShape[ 3 ].z * rotation.GetSine(), quad.vShape[ 3 ].z * rotation.GetCosine() + quad.vShape[ 3 ].y * rotation.GetSine() );
	float flError = ( r2 - x2 ).LengthSqr( ) + ( r3 - x3 ).LengthSqr( );
	p2 = vCoM + r2 * flModelScale;
	p3 = vCoM + r3 * flModelScale;
	return flError;
}


//----------------------------------------------------------------------------------------------------------
float CFeModel::RelaxQuad1( float flStiffness, float flModelScale, const FeQuad_t &quad, const VectorAligned &p0, VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const
{
	Vector vCoM = p0;
	CFeBasis basis( p2 - p0, p3 - p1 ); // rotating around p0, because center of mass = p0, because it's the only infinite-mass corner of this quad; Should be synced up with Builder:AdjustQuads
	// find omega, refer to wahba.nb. We only have points 1, 2 and 3 to compute and move, but otherwise this is the same as the free-quad version
	Vector x1 = p1 - vCoM, x2 = p2 - vCoM, x3 = p3 - vCoM;
	Vector r1 = basis.LocalToWorld( quad.vShape[ 1 ] * flModelScale ), r2 = basis.LocalToWorld( quad.vShape[ 2 ] * flModelScale ), r3 = basis.LocalToWorld( quad.vShape[ 3 ] * flModelScale );
	float m1 = quad.vShape[ 1 ].w, m2 = quad.vShape[ 2 ].w, m3 = quad.vShape[ 3 ].w;

	// refer to wahba.nb
	Vector rhs = -(m1 * CrossProduct( x1, r1 ) + m2 * CrossProduct( x2, r2 ) + m3 * CrossProduct( x3, r3 ) );
	CovMatrix3 cov;
	cov.InitForWahba( m1, x1 );
	cov.AddForWahba( m2, x2 );
	cov.AddForWahba( m3, x3 );
	
	Cholesky3x3_t chol( cov.m_vDiag.x, cov.m_flXY, cov.m_vDiag.y, cov.m_flXZ, cov.m_flYZ, cov.m_vDiag.z );
	CSinCosRotation rotation( chol.Solve( rhs ) );
	// yay! we computed omega (sin), apply it..
	r1 = rotation * r1; r2 = rotation * r2; r3 = rotation * r3;
	float flError = ( r1 - x1 ).LengthSqr( ) + ( r2 - x2 ).LengthSqr( ) + ( r3 - x3 ).LengthSqr( );
	p1 = r1 + vCoM; p2 = r2 + vCoM; p3 = r3 + vCoM;

	return flError;
}


//----------------------------------------------------------------------------------------------------------
// total theoretical cost is ~300 flops, practically taking 670 ticks (170/single quad) which when SIMDized can be 300 ticks to solve 4 quads, or 8 quads when using AVX
// this is as fast as solving 12 distance constraints, but the quad is solved precisely every time
// considering that a quad requires at least 6 distance constraints to make it rigid, and 2 passes won't solve it precisely, it's a prety good deal
float CFeModel::RelaxQuad0( float flStiffness, float flModelScale, const FeQuad_t &quad, VectorAligned &p0, VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const
{
	CFeBasis basis( p2 - p0, p3 - p1 ); // the agreed upon local frame of reference of the quad Finite Element. Should be synced up with Builder:AdjustQuads
	float m[ 4 ] = { quad.vShape[ 0 ].w, quad.vShape[ 1 ].w, quad.vShape[ 2 ].w, quad.vShape[ 3 ].w };
	Vector vCoM = p0 * m[ 0 ] + p1 * m[ 1 ] + p2 * m[ 2 ] + p3 * m[ 3 ]; // ~12 flops
	// find omega, refer to wahba.nb. We only have points 1, 2 and 3 to compute and move, but otherwise this is the same as the free-quad version
	Vector x[ 4 ] = { p0 - vCoM, p1 - vCoM, p2 - vCoM, p3 - vCoM }; // ~9 flops
	
	Vector r[ 4 ] = { basis.LocalToWorld( flModelScale * quad.vShape[ 0 ] ), basis.LocalToWorld( flModelScale * quad.vShape[ 1 ] ), basis.LocalToWorld( flModelScale * quad.vShape[ 2 ] ), basis.LocalToWorld( flModelScale * quad.vShape[ 3 ] ) }; // ~36 flops
	float flError;
// 	for ( int i = 0; i < 3; ++i )
	{

#ifdef _DEBUG
		Vector q[ 4 ] = { basis.WorldToLocal( x[ 0 ] ), basis.WorldToLocal( x[ 1 ] ), basis.WorldToLocal( x[ 2 ] ), basis.WorldToLocal( x[ 3 ] ) }; NOTE_UNUSED( q );
#endif

		// refer to wahba.nb
		Vector rhs = - ( m[ 0 ] * CrossProduct( x[ 0 ], r[ 0 ] ) + m[ 1 ] * CrossProduct( x[ 1 ], r[ 1 ] ) + m[ 2 ] * CrossProduct( x[ 2 ], r[ 2 ] ) + m[ 3 ] * CrossProduct( x[ 3 ], r[ 3 ] ) ); // ~27 flops

		// ~84 flops to sum up the covariance matrix
		CovMatrix3 cov;
		cov.InitForWahba( m[ 0 ], x[ 0 ] );
		cov.AddForWahba( m[ 1 ], x[ 1 ] );
		cov.AddForWahba( m[ 2 ], x[ 2 ] );
		cov.AddForWahba( m[ 3 ], x[ 3 ] );

		Cholesky3x3_t chol( cov.m_vDiag.x, cov.m_flXY, cov.m_vDiag.y, cov.m_flXZ, cov.m_flYZ, cov.m_vDiag.z ); // ~30 flops in SIMD
		CSinCosRotation rotation( chol.Solve( rhs ) );  // ~20 flops to solve and init rotation
		// yay! we computed omega (sin), apply it..
		r[ 0 ] = rotation * r[ 0 ]; r[ 1 ] = rotation * r[ 1 ]; r[ 2 ] = rotation * r[ 2 ]; r[ 3 ] = rotation * r[ 3 ];
#ifdef _DEBUG
		Vector s[ 4 ] = { basis.WorldToLocal( r[ 0 ] ), basis.WorldToLocal( r[ 1 ] ), basis.WorldToLocal( r[ 2 ] ), basis.WorldToLocal( r[ 3 ] ) }; NOTE_UNUSED( s );
#endif
		flError = ( r[ 0 ] - x[ 0 ] ).LengthSqr() + ( r[ 1 ] - x[ 1 ] ).LengthSqr() + ( r[ 2 ] - x[ 2 ] ).LengthSqr() + ( r[ 3 ] - x[ 3 ] ).LengthSqr();
	} // ~24 flops
	
	p0 = r[ 0 ] + vCoM;	p1 = r[ 1 ] + vCoM; p2 = r[ 2 ] + vCoM; p3 = r[ 3 ] + vCoM; // 33 flops

	return flError;
}




// ~121 flops
float CFeModel::RelaxQuad0flat( float flStiffness, float flModelScale, const FeQuad_t &quad, VectorAligned &p0, VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const
{
	CFeBasis basis( p2 - p0, p3 - p1 ); // ~9 flops; the agreed upon local frame of reference of the quad Finite Element
	float m[ 4 ] = { quad.vShape[ 0 ].w, quad.vShape[ 1 ].w, quad.vShape[ 2 ].w, quad.vShape[ 3 ].w };
	Vector vCoM = p0 * m[ 0 ] + p1 * m[ 1 ] + p2 * m[ 2 ] + p3 * m[ 3 ]; // ~12 flops
	// find omega, refer to wahba.nb. We only have points 1, 2 and 3 to compute and move, but otherwise this is the same as the free-quad version
	Vector2D x[ 4 ] = { basis.WorldToLocalXY( p0 - vCoM ), basis.WorldToLocalXY( p1 - vCoM ), basis.WorldToLocalXY( p2 - vCoM ), basis.WorldToLocalXY( p3 - vCoM ) }; // ~24 flops
	// refer to wahba.nb
	CSinCosRotation2D rotation( m[ 0 ] * DotProduct( quad.vShape[ 0 ], x[ 0 ] ) + m[ 1 ] * DotProduct( quad.vShape[ 1 ], x[ 1 ] ) + m[ 2 ] * DotProduct( quad.vShape[ 2 ], x[ 2 ] ) + m[ 3 ] * DotProduct( quad.vShape[ 3 ], x[ 3 ] ) ,
								m[ 0 ] * CrossProductZ( quad.vShape[ 0 ], x[ 0 ] ) + m[ 1 ] * CrossProductZ( quad.vShape[ 1 ], x[ 1 ] ) + m[ 2 ] * CrossProductZ( quad.vShape[ 2 ], x[ 2 ] ) + m[ 3 ] * CrossProductZ( quad.vShape[ 3 ], x[ 3 ] ) ); // flops
	basis.UnrotateXY( rotation ); // ~12 flops

	Vector r[ 4 ] = { basis.LocalToWorld( flModelScale * quad.vShape[ 0 ].AsVector3D( ) ), basis.LocalToWorld( flModelScale * quad.vShape[ 1 ].AsVector3D( ) ), basis.LocalToWorld( flModelScale * quad.vShape[ 2 ].AsVector3D( ) ), basis.LocalToWorld( flModelScale * quad.vShape[ 3 ].AsVector3D( ) ) }; // ~40 flops

	p0 = r[ 0 ] + vCoM;	p1 = r[ 1 ] + vCoM; p2 = r[ 2 ] + vCoM; p3 = r[ 3 ] + vCoM; // 12 flops

	return 1.0f - rotation.m_flCos; //  I really should scale it by the quad's perimeter or something...
}




//----------------------------------------------------------------------------------------------------------
fltx4 CFeModel::RelaxSimdQuad2( const fltx4& fl4Stiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const
{
	FourVectors p0, p1, p2, p3;
	LOAD_NODES( p0, quad.nNode[ 0 ] );
	LOAD_NODES( p1, quad.nNode[ 1 ] );
	LOAD_NODES( p2, quad.nNode[ 2 ] );
	LOAD_NODES( p3, quad.nNode[ 3 ] );
	FourVectors vCoM = ( p0 + p1 ) * Four_PointFives;
	CFeSimdBasis basis( p1 - p0, p2 + p3 - ( p0 + p0 ) ); // p1-p0 is the best choice for the axis, because we're rotating around it
	// assume center of mass = p0, because it doesn't matter where it is along the line p0..p1
	// find omega, refer to wahba1d.nb. We only have points 2 and 3 to compute and move, and even that only in the plane YZ (X is fixed, as we rotate around axis X)
	FourVectors x2 = p2 - vCoM, x3 = p3 - vCoM;
	FourVectorsYZ vLocalP2 = basis.WorldToLocalYZ( x2 ), vLocalP3 = basis.WorldToLocalYZ( x3 );
	// Note: e x X = CrossProduct( vAxisX, (?, x1, x2) ) = (0, -x2, x1 ). Note that we rename YZ->XY for Vector2D conversion, so (-x2, x1) -> ( -p.y, p.x )
	fltx4 flMass2 = quad.f4Weights[ 2 ], flMass3 = quad.f4Weights[ 3 ];
	CSimdSinCosRotation2D rotation( ( vLocalP2.y * quad.vShape[ 2 ].y + vLocalP2.z * quad.vShape[ 2 ].z ) * flMass2 + ( vLocalP3.y * quad.vShape[ 3 ].y + vLocalP3.z * quad.vShape[ 3 ].z ) * flMass3,
								     ( vLocalP2.y * quad.vShape[ 2 ].z - vLocalP2.z * quad.vShape[ 2 ].y ) * flMass2 + ( vLocalP3.y * quad.vShape[ 3 ].z - vLocalP3.z * quad.vShape[ 3 ].y ) * flMass3 );
	FourVectors r2 = basis.LocalToWorld( quad.vShape[ 2 ].x, quad.vShape[ 2 ].y * rotation.GetCosine( ) - quad.vShape[ 2 ].z * rotation.GetSine( ), quad.vShape[ 2 ].z * rotation.GetCosine( ) + quad.vShape[ 2 ].y * rotation.GetSine( ) );
	FourVectors r3 = basis.LocalToWorld( quad.vShape[ 3 ].x, quad.vShape[ 3 ].y * rotation.GetCosine( ) - quad.vShape[ 3 ].z * rotation.GetSine( ), quad.vShape[ 3 ].z * rotation.GetCosine( ) + quad.vShape[ 3 ].y * rotation.GetSine( ) );
	fltx4 flError = ( r2 - x2 ).LengthSqr( ) + ( r3 - x3 ).LengthSqr( );

	fltx4 wr = fl4ModelScale * fl4Stiffness, wx = Four_Ones - fl4Stiffness;
	p2 = vCoM + r2 * wr + x2 * wx;
	p3 = vCoM + r3 * wr + x3 * wx;

	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length( ) + p3.Length( ) ) );

	SAVE_NODES( p2, quad.nNode[ 2 ] );
	SAVE_NODES( p3, quad.nNode[ 3 ] );

	return flError;
}


//----------------------------------------------------------------------------------------------------------
fltx4 CFeModel::RelaxSimdQuad1( const fltx4& fl4Stiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const
{
	FourVectors p0, p1, p2, p3;
	LOAD_NODES( p0, quad.nNode[ 0 ] );
	LOAD_NODES( p1, quad.nNode[ 1 ] );
	LOAD_NODES( p2, quad.nNode[ 2 ] );
	LOAD_NODES( p3, quad.nNode[ 3 ] );
	const FourVectors &vCoM = p0;
	CFeSimdBasis basis( p2 - p0, p3 - p1 ); // rotating around p0, because center of mass = p0, because it's the only infinite-mass corner of this quad
	// find omega, refer to wahba.nb. We only have points 1, 2 and 3 to compute and move, but otherwise this is the same as the free-quad version
	FourVectors x1 = p1 - vCoM, x2 = p2 - vCoM, x3 = p3 - vCoM;
	FourVectors r1 = basis.LocalToWorld( fl4ModelScale * quad.vShape[ 1 ] ), r2 = basis.LocalToWorld( fl4ModelScale * quad.vShape[ 2 ] ), r3 = basis.LocalToWorld( fl4ModelScale * quad.vShape[ 3 ] );
	fltx4 m1 = quad.f4Weights[ 1 ], m2 = quad.f4Weights[ 2 ], m3 = quad.f4Weights[ 3 ];

	// refer to wahba.nb
	FourVectors rhs = m1 * CrossProduct( x1, r1 ) + m2 * CrossProduct( x2, r2 ) + m3 * CrossProduct( x3, r3 );
	FourCovMatrices3 cov;
	cov.InitForWahba( m1, x1 );
	cov.AddForWahba( m2, x2 );
	cov.AddForWahba( m3, x3 );

	SimdCholesky3x3_t chol( cov.m_vDiag.x, cov.m_flXY, cov.m_vDiag.y, cov.m_flXZ, cov.m_flYZ, cov.m_vDiag.z );
	CSimdSinCosRotation rotation( AndSIMD( chol.Solve( rhs ), chol.GetValidMask() ) );
	// yay! we computed omega (sin), apply it..
	r1 = rotation.Unrotate( r1 ); r2 = rotation.Unrotate( r2 ); r3 = rotation.Unrotate( r3 );
	fltx4 flError = ( r1 - x1 ).LengthSqr( ) + ( r2 - x2 ).LengthSqr( ) + ( r3 - x3 ).LengthSqr( );
	p1 = r1 + vCoM; p2 = r2 + vCoM; p3 = r3 + vCoM;

	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length( ) + p3.Length( ) ) );

	SAVE_NODES( p1, quad.nNode[ 1 ] );
	SAVE_NODES( p2, quad.nNode[ 2 ] );
	SAVE_NODES( p3, quad.nNode[ 3 ] );

	return flError;
}

fltx4 CFeModel::RelaxSimdQuad0( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const
{
	FourVectors p0, p1, p2, p3;
	LOAD_NODES( p0, quad.nNode[ 0 ] );
	LOAD_NODES( p1, quad.nNode[ 1 ] );
	LOAD_NODES( p2, quad.nNode[ 2 ] );
	LOAD_NODES( p3, quad.nNode[ 3 ] );
	CFeSimdBasis basis( p2 - p0, p3 - p1 ); // the agreed upon local frame of reference of the quad Finite Element
	const fltx4 *m = quad.f4Weights;
	FourVectors vCoM = p0 * m[ 0 ] + p1 * m[ 1 ] + p2 * m[ 2 ] + p3 * m[ 3 ]; // ~12 flops
	// find omega, refer to wahba.nb. We only have points 1, 2 and 3 to compute and move, but otherwise this is the same as the free-quad version
	FourVectors x[ 4 ] = { p0 - vCoM, p1 - vCoM, p2 - vCoM, p3 - vCoM }; // ~9 flops

	FourVectors r[ 4 ] = { basis.LocalToWorld( fl4ModelScale * quad.vShape[ 0 ] ), basis.LocalToWorld( fl4ModelScale * quad.vShape[ 1 ] ), basis.LocalToWorld( fl4ModelScale * quad.vShape[ 2 ] ), basis.LocalToWorld( fl4ModelScale * quad.vShape[ 3 ] ) }; // ~48 flops
	fltx4 fl4Error;
	// 	for ( int i = 0; i < 3; ++i )
	{

#ifdef _DEBUG
		//FourVectors q[ 4 ] = { basis.WorldToLocal( x[ 0 ] ), basis.WorldToLocal( x[ 1 ] ), basis.WorldToLocal( x[ 2 ] ), basis.WorldToLocal( x[ 3 ] ) }; NOTE_UNUSED( q );
#endif

		// refer to wahba.nb
		FourVectors rhs = ( m[ 0 ] * CrossProduct( x[ 0 ], r[ 0 ] ) + m[ 1 ] * CrossProduct( x[ 1 ], r[ 1 ] ) + m[ 2 ] * CrossProduct( x[ 2 ], r[ 2 ] ) + m[ 3 ] * CrossProduct( x[ 3 ], r[ 3 ] ) ); // ~27 flops

		// ~84 flops to sum up the covariance matrix
		FourCovMatrices3 cov;
		cov.InitForWahba( m[ 0 ], x[ 0 ] );
		cov.AddForWahba( m[ 1 ], x[ 1 ] );
		cov.AddForWahba( m[ 2 ], x[ 2 ] );
		cov.AddForWahba( m[ 3 ], x[ 3 ] );

		SimdCholesky3x3_t chol( cov.m_vDiag.x, cov.m_flXY, cov.m_vDiag.y, cov.m_flXZ, cov.m_flYZ, cov.m_vDiag.z ); // ~30 flops in SIMD
		CSimdSinCosRotation rotation( AndSIMD( chol.Solve( rhs ), chol.GetValidMask( ) ) );  // ~20 + 7 flops to solve and init rotation + mask out invalid columns
		// yay! we computed omega (sin), apply it..
		r[ 0 ] = rotation.Unrotate( r[ 0 ] ); r[ 1 ] = rotation.Unrotate( r[ 1 ] ); r[ 2 ] = rotation.Unrotate( r[ 2 ] ); r[ 3 ] = rotation.Unrotate( r[ 3 ] );
		fl4Error = ( r[ 0 ] - x[ 0 ] ).LengthSqr( ) + ( r[ 1 ] - x[ 1 ] ).LengthSqr( ) + ( r[ 2 ] - x[ 2 ] ).LengthSqr( ) + ( r[ 3 ] - x[ 3 ] ).LengthSqr( );

#ifdef _DEBUG
		//FourVectors s[ 4 ] = { basis.WorldToLocal( r[ 0 ] ), basis.WorldToLocal( r[ 1 ] ), basis.WorldToLocal( r[ 2 ] ), basis.WorldToLocal( r[ 3 ] ) }; NOTE_UNUSED( s );
		//const fltx4 fl4MaxExpectedError = { 64, 64, 64, 64 };
		//AssertDbg( IsAllGreaterThan( fl4MaxExpectedError, fl4Error ) );
#endif
	} // ~24 flops

	fltx4 flUnStiffness = Four_Ones - flStiffness;

	p0 = r[ 0 ] * flStiffness + x[ 0 ] * flUnStiffness + vCoM;
	p1 = r[ 1 ] * flStiffness + x[ 1 ] * flUnStiffness + vCoM;
	p2 = r[ 2 ] * flStiffness + x[ 2 ] * flUnStiffness + vCoM;
	p3 = r[ 3 ] * flStiffness + x[ 3 ] * flUnStiffness + vCoM; // 33 flops

	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length( ) + p3.Length() ) );

	SAVE_NODES( p0, quad.nNode[ 0 ] );
	SAVE_NODES( p1, quad.nNode[ 1 ] );
	SAVE_NODES( p2, quad.nNode[ 2 ] );
	SAVE_NODES( p3, quad.nNode[ 3 ] );
	return fl4Error;
}


FORCEINLINE fltx4 DotProduct( const FourVectors &v1, const FourVectors2D &v2 )
{
	return v1.x * v2.x + v1.y * v2.y;
}

fltx4 CFeModel::RelaxSimdQuad0flat( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const
{
	FourVectors p0, p1, p2, p3;
	LOAD_NODES( p0, quad.nNode[ 0 ] );
	LOAD_NODES( p1, quad.nNode[ 1 ] );
	LOAD_NODES( p2, quad.nNode[ 2 ] );
	LOAD_NODES( p3, quad.nNode[ 3 ] );
	CFeSimdBasis basis( p2 - p0, p3 - p1 ); // the agreed upon local frame of reference of the quad Finite Element
	FourVectors vCoM = p0 * quad.f4Weights[ 0 ] + p1 * quad.f4Weights[ 1 ] + p2 * quad.f4Weights[ 2 ] + p3 * quad.f4Weights[ 3 ]; // ~12 flops
	FourVectors2D x[ 4 ] = { basis.WorldToLocalXY( p0 - vCoM ), basis.WorldToLocalXY( p1 - vCoM ), basis.WorldToLocalXY( p2 - vCoM ), basis.WorldToLocalXY( p3 - vCoM ) }; // ~24 flops
	// refer to wahba.nb
	CSimdSinCosRotation2D rotation( 
		quad.f4Weights[ 0 ] * DotProduct( quad.vShape[ 0 ], x[ 0 ] ) + quad.f4Weights[ 1 ] * DotProduct( quad.vShape[ 1 ], x[ 1 ] ) + quad.f4Weights[ 2 ] * DotProduct( quad.vShape[ 2 ], x[ 2 ] ) + quad.f4Weights[ 3 ] * DotProduct( quad.vShape[ 3 ], x[ 3 ] ),
		quad.f4Weights[ 0 ] * CrossProductZ( quad.vShape[ 0 ], x[ 0 ] ) + quad.f4Weights[ 1 ] * CrossProductZ( quad.vShape[ 1 ], x[ 1 ] ) + quad.f4Weights[ 2 ] * CrossProductZ( quad.vShape[ 2 ], x[ 2 ] ) + quad.f4Weights[ 3 ] * CrossProductZ( quad.vShape[ 3 ], x[ 3 ] )
		); // ~flops
	basis.UnrotateXY( rotation ); // ~12 flops

	FourVectors r[ 4 ] = { basis.LocalToWorld( fl4ModelScale * quad.vShape[ 0 ] ), basis.LocalToWorld( fl4ModelScale * quad.vShape[ 1 ] ), basis.LocalToWorld( fl4ModelScale * quad.vShape[ 2 ] ), basis.LocalToWorld( fl4ModelScale * quad.vShape[ 3 ] ) }; // ~36 flops

	p0 = r[ 0 ] + vCoM;	p1 = r[ 1 ] + vCoM; p2 = r[ 2 ] + vCoM; p3 = r[ 3 ] + vCoM; // 33 flops

	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length( ) + p3.Length( ) ) );

	SAVE_NODES( p0, quad.nNode[ 0 ] );
	SAVE_NODES( p1, quad.nNode[ 1 ] );
	SAVE_NODES( p2, quad.nNode[ 2 ] );
	SAVE_NODES( p3, quad.nNode[ 3 ] );
	return Four_Ones - rotation.m_fl4Cos;
}




//----------------------------------------------------------------------------------------------------------
fltx4 CFeModel::RelaxSimdTri2( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdTri_t &tri, fltx4 *pPos )const
{
	FourVectors p0, p1, p2;
	LOAD_NODES( p0, tri.nNode[ 0 ] );
	LOAD_NODES( p1, tri.nNode[ 1 ] );
	LOAD_NODES( p2, tri.nNode[ 2 ] );
	
	CFeSimdTriBasis basis( p1 - p0, p2 - p0 ); // p1-p0 is the best choice for the axis, because we're rotating around it
	p2 = p0 + basis.LocalXYToWorld( fl4ModelScale * ( tri.v2.x + Four_PointFives * ( basis.v1x - tri.v1x ) ), fl4ModelScale * tri.v2.y );
	
	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length( ) ) );
	SAVE_NODES( p2, tri.nNode[ 2 ] );
	return Four_Zeros;
}



FORCEINLINE fltx4 CrossProductZ( const fltx4& v1x, const fltx4& v1y, const fltx4& v2x, const fltx4& v2y )
{
	return v1x * v2y - v1y * v2x;
}

FORCEINLINE fltx4 DotProduct2D( const FourVectors2D &v1, const FourVectors2D &v2 )
{
	return v1.x * v2.x + v1.y * v2.y;
}

FORCEINLINE fltx4 CrossProductZ( const FourVectors2D &v1, const FourVectors2D &v2 )
{
	return v1.x * v2.y - v1.y * v2.x;
}

//----------------------------------------------------------------------------------------------------------
fltx4 CFeModel::RelaxSimdTri1( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdTri_t &tri, fltx4 *pPos )const
{
	FourVectors p0, p1, p2;
	LOAD_NODES( p0, tri.nNode[ 0 ] );
	LOAD_NODES( p1, tri.nNode[ 1 ] );
	LOAD_NODES( p2, tri.nNode[ 2 ] );
	
	CFeSimdTriBasis basis( p1 - p0, p2 - p0 ); // rotating around p0, because center of mass = p0, because it's the only infinite-mass corner of this triangle
	CSimdSinCosRotation2D rotation( tri.w2 * DotProduct2D( tri.v2, basis.v2 ) + tri.w1 * ( tri.v1x * basis.v1x ), tri.w2 * CrossProductZ( tri.v2, basis.v2 ) );

	// we computed omega (sin, cos), apply it..
	p1 = p0 + basis.LocalXYToWorld( fl4ModelScale * ( rotation.m_fl4Cos * tri.v1x ), fl4ModelScale * ( rotation.m_fl4Sin * tri.v1x ) );
	p2 = p0 + basis.LocalXYToWorld( fl4ModelScale * ( rotation * tri.v2 ) );

	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length( ) ) );
	SAVE_NODES( p1, tri.nNode[ 1 ] );
	SAVE_NODES( p2, tri.nNode[ 2 ] );
	return Four_Ones - rotation.m_fl4Cos;
}



//----------------------------------------------------------------------------------------------------------
fltx4 CFeModel::RelaxSimdTri0( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdTri_t &tri, fltx4 *pPos )const
{
	FourVectors p0, p1, p2;
	LOAD_NODES( p0, tri.nNode[ 0 ] );
	LOAD_NODES( p1, tri.nNode[ 1 ] );
	LOAD_NODES( p2, tri.nNode[ 2 ] );
	fltx4 tri_v1x = fl4ModelScale * tri.v1x;
	FourVectors2D tri_v2 = fl4ModelScale * tri.v2;
	
	CFeSimdTriBasis basis( p1 - p0, p2 - p0 ); 
	FourVectors2D x0neg( basis.v1x * tri.w1 + basis.v2.x * tri.w2, basis.v2.y * tri.w2 ); // center of mass of the deformed triangle p0,p1,p2 in local coordinate system
	FourVectors2D r0neg( tri_v1x * tri.w1 + tri_v2.x * tri.w2, tri_v2.y * tri.w2 ); // center of mass of the target triangle configuration, in its coordinate system
	fltx4 w0 = Four_Ones - ( tri.w1 + tri.w2 );

	FourVectors2D x2 = basis.v2 - x0neg, r2 = tri_v2 - r0neg;
	fltx4 x1x = basis.v1x - x0neg.x; // x1.y = -x0neg.y
	fltx4 r1x = tri_v1x - r0neg.x; // r1.y = -r0neg.y

	CSimdSinCosRotation2D rotation(
		tri.w2 * DotProduct2D( x2, r2 ) + tri.w1 * ( x1x * r1x ) + w0 * ( x0neg.x * r0neg.x ) + ( tri.w1 + w0 ) * ( x0neg.y * r0neg.y ),
		tri.w2 * CrossProductZ( r2, x2 ) - tri.w1 * CrossProductZ( r1x, r0neg.y, x1x, x0neg.y ) + w0 * CrossProductZ( r0neg, x0neg )
		);

	FourVectors2D dX0 = x0neg - rotation * r0neg;

	// we computed omega (sin, cos), apply it..
	p0 += basis.LocalXYToWorld( dX0 );
	p1 = p0 + basis.LocalXYToWorld( rotation.m_fl4Cos * tri_v1x, rotation.m_fl4Sin * tri_v1x );
	p2 = p0 + basis.LocalXYToWorld( rotation * tri_v2 );

	AssertDbg( IsAllGreaterThan( Four_MaxWorldSize, p0.Length( ) + p1.Length( ) + p2.Length() ) );

	SAVE_NODES( p0, tri.nNode[ 0 ] );
	SAVE_NODES( p1, tri.nNode[ 1 ] );
	SAVE_NODES( p2, tri.nNode[ 2 ] );
	return Four_Ones - rotation.m_fl4Cos;
}


//----------------------------------------------------------------------------------------------------------
void CFeModel::RelaxBend( VectorAligned *pPos, float flStiffness )const
{
	for ( uint nEdge = 0; nEdge < m_nAxialEdgeCount; ++nEdge )
	{
		const FeAxialEdgeBend_t& edge = m_pAxialEdges[ nEdge ];
		VectorAligned &p0 = pPos[ edge.nNode[ 0 ] ], &p1 = pPos[ edge.nNode[ 1 ] ], &p2 = pPos[ edge.nNode[ 2 ] ], &p3 = pPos[ edge.nNode[ 3 ] ], &p4 = pPos[ edge.nNode[ 4 ] ], &p5 = pPos[ edge.nNode[ 5 ] ];
		Vector fe = p0 * ( 1.0f - edge.te ) + p1 * edge.te;
		float etvHalf = edge.tv * 0.5f;
		Vector fv = ( p2 + p3 ) * ( 0.5f - etvHalf ) + ( p4 + p5 ) * etvHalf;
		// alternative axis
		Vector vAxis = fv - fe;
		float flAxis = vAxis.Length( );

		Vector vEdge = p1 - p0, vVirtualEdge = ( p4 + p5 ) - ( p2 + p3 );
		Vector vCrossEdges = CrossProduct( vEdge, vVirtualEdge );
		float flCorrection;
		if ( flAxis > 0.001f ) // this is the upper bound on the distance between edges; I hope edges themselves are never this short, so we can consider them almost-intersecting if they're this close and fallback 
		{
			float flAdjDist = DotProduct( vAxis, vCrossEdges ) > 0 ? -edge.flDist : edge.flDist; // -1.0f: detect flipped edge, flip it back 
			flCorrection = 1.0f + flAdjDist / flAxis;
		}
		else
		{
			// recover from intersecting edges; not necessary for high curvatures if there are rigid rods that enforce some bend, but crucial to maintain low or zero angles of curvature
			// generally, rods are better for high curvature and useless for low; bends are better for low curvature and useless for high
			float flCrossEdges = vCrossEdges.Length();
			if ( flCrossEdges > FLT_EPSILON )
			{
				vAxis = vCrossEdges;
				flCorrection = edge.flDist / flCrossEdges;
			}
			else
			{
				vAxis.z = 1; // degenerate case, should probably recover anyway when polygon shape recovers
				flCorrection = edge.flDist;
			}
		}
		Vector vDelta = ( flStiffness * flCorrection ) * vAxis;

		p0 += vDelta * edge.flWeight[ 0 ];
		p1 += vDelta * edge.flWeight[ 1 ];
		Vector dw2 = vDelta * edge.flWeight[ 2 ];
		// Careful! The same weight will be applied to the same node twice sometimes here
		Vector p2new = p2 + dw2;
		Vector p3new = p3 + dw2;
		p2 = p2new;
		p3 = p3new;
		Vector dw3 = vDelta * edge.flWeight[ 3 ];
		Vector p4new = p4 + dw3;
		Vector p5new = p5 + dw3;
		p4 = p4new;
		p5 = p5new;
	}
}




void IntegrateSpring( const FeSpringIntegrator_t &integ, float flTimeStep, float flModelScale, VectorAligned &pos10, VectorAligned &pos11, VectorAligned &pos00, VectorAligned &pos01 )
{
	Vector vDeltaOrigin1 = pos10 - pos11;
	Vector vDeltaOrigin0 = pos00 - pos01;
	Vector vDeltaVelDt = ( vDeltaOrigin1 - vDeltaOrigin0 );

	float flDistance = vDeltaOrigin1.Length( );
	if ( flDistance < 1.0f )
	{
		return; // nothing to integrate
	}

	Vector vSpringDir = vDeltaOrigin1 / flDistance;

	float flHTerm = ( flDistance - integ.flSpringRestLength * flModelScale ) * integ.flSpringConstant * flTimeStep;
	float flDTerm = ( vDeltaVelDt.Dot( vSpringDir ) * integ.flSpringDamping );

	//Vector vSpringDeltaVel = vSpringAcceleration * -( flHTerm + flDTerm );
	Vector vIntegral = vSpringDir * ( ( flHTerm + flDTerm ) * ( flTimeStep /** 0.5f*/ ) ); 

	// reference from source1 code:
// 	if ( ( m_SpringType == CLOTH_SPRING_STRUCTURAL_HORIZONTAL || m_SpringType == CLOTH_SPRING_STRUCTURAL_VERTICAL ) && ( pParticle1->IsFixed( ) == true || pParticle2->IsFixed( ) == true ) )
// 	{
// 		m_vSpringForce *= 2.0f; // <--- this is why I'm multiplying by dt^2, not dt^2 / 2
// 	}
	// all working springs are structural horizontal or vertical

	// SERGIY TEST: is the sign correct here? the cloth1 cloth uses spring force = -(HTerm+DTerm)*d
	pos00 -= vIntegral *          integ.flNodeWeight0;
	pos01 += vIntegral * ( 1.0f - integ.flNodeWeight0 );
}


//----------------------------------------------------------------------------------------------------------
// for integrating accelerations into positions, pass dt*dt/2 for subsequent verlet integration;
// for integrating accelerations into velocities or velocities into positions, pass dt;
void CFeModel::IntegrateSprings( VectorAligned *pPos0, VectorAligned *pPos1, float flTimeStep, float flModelScale )	const
{
	for ( int i = 0; i < m_nSpringIntegratorCount; ++i )
	{
		const FeSpringIntegrator_t &integ = m_pSpringIntegrator[ i ];
		uint n0 = integ.nNode[ 0 ], n1 = integ.nNode[ 1 ];
		VectorAligned &pos10 = pPos1[ n0 ], &pos11 = pPos1[ n1 ];
		VectorAligned &pos00 = pPos0[ n0 ], &pos01 = pPos0[ n1 ];
		IntegrateSpring( integ, flTimeStep, flModelScale, pos10, pos11, pos00, pos01 );
	}
}



float CFeModel::ComputeElasticEnergyQuads( const VectorAligned *pPos, float flModelScale )const
{
	float flStiffness = 1.0f, flSumEnergy = 0;
#define QUAD_ENERGY(FUNC)																													 \
	const FeQuad_t &quad = m_pQuads[ i ];																									 \
	VectorAligned pos[ 4 ] = { pPos[ quad.nNode[ 0 ] ], pPos[ quad.nNode[ 1 ] ], pPos[ quad.nNode[ 2 ] ], pPos[ quad.nNode[ 3 ] ] };	     \
	FUNC( flStiffness, flModelScale, quad, pos[ 0 ], pos[ 1 ], pos[ 2 ], pos[ 3 ] );												 		 \
	for ( int j = 0; j < 4; ++j )																											 \
		if ( m_pNodeInvMasses[ quad.nNode[ j ] ] > 0 )																						 \
			flSumEnergy += ( pos[ j ] - pPos[ quad.nNode[ j ] ] ).LengthSqr( ) * 0.5f / m_pNodeInvMasses[ quad.nNode[ j ] ];				 

	// first the simplest case of quads hanging on the edge 0-1: this is a 2D case with 2 moving nodes and only 1 DOF (rotation around edge 0-1)
	for ( uint i = 0; i < m_nQuadCount[ 2 ]; ++i )
	{
		QUAD_ENERGY( RelaxQuad2 );
	}

	for ( uint i = m_nQuadCount[ 2 ]; i < m_nQuadCount[ 1 ]; ++i )
	{
		QUAD_ENERGY( RelaxQuad1 );
	}
	for ( uint i = m_nQuadCount[ 1 ]; i < m_nQuadCount[ 0 ]; ++i )
	{
		QUAD_ENERGY( RelaxQuad0 );
	}
#undef QUAD_ENERGY
	return flSumEnergy;
}

float CFeModel::ComputeElasticEnergyRods( const VectorAligned *pPos, float flModelScale )const
{
	float flSumEnergy = 0;
	//CMicroProfilerGuard mpg( &g_GlobalStats[ PhysicsGlobalProfiler::RelaxRods ], m_nRodCount - 1 );
	for ( uint i = 0; i < m_nRodCount; ++i )
	{
		const FeRodConstraint_t &c = m_pRods[ i ];
		uint n0 = c.nNode[ 0 ], n1 = c.nNode[ 1 ];
		VectorAligned a = pPos[ n0 ], b = pPos[ n1 ];
		RelaxRod( c, flModelScale, a, b );
		if ( m_pNodeInvMasses[ n0 ] > 0 )
			flSumEnergy += ( a - pPos[ n0 ] ).LengthSqr( ) * 0.5f / m_pNodeInvMasses[ n0 ];
		if ( m_pNodeInvMasses[ n1 ] > 0 )
			flSumEnergy += ( b - pPos[ n1 ] ).LengthSqr( ) * 0.5f / m_pNodeInvMasses[ n1 ];
	}
	return flSumEnergy;
}


float CFeModel::ComputeElasticEnergySprings( const VectorAligned *pPos0, const VectorAligned *pPos1, float flTimeStep, float flModelScale )	const
{
	float flSumEnergy = 0, dtSqr = flTimeStep;
	for ( int i = 0; i < m_nSpringIntegratorCount; ++i )
	{
		const FeSpringIntegrator_t &integ = m_pSpringIntegrator[ i ];
		uint n0 = integ.nNode[ 0 ], n1 = integ.nNode[ 1 ];
		VectorAligned pos10 = pPos1[ n0 ], pos11 = pPos1[ n1 ];
		VectorAligned pos00 = pPos0[ n0 ], pos01 = pPos0[ n1 ];
		IntegrateSpring( integ, flTimeStep, flModelScale, pos10, pos11, pos00, pos01 );
		if ( m_pNodeInvMasses[ n0 ] > 0 )
			flSumEnergy += ( pos10 - pPos1[ n0 ] ).LengthSqr( ) / ( m_pNodeInvMasses[ n0 ] * dtSqr );
		if ( m_pNodeInvMasses[ n1 ] > 0)
			flSumEnergy += ( pos11 - pPos1[ n1 ] ).LengthSqr( ) / ( m_pNodeInvMasses[ n1 ] * dtSqr );
	}
	return flSumEnergy;
}


int CFeModel::GetComplexity( )const
{
	return
		( m_nNodeCount - m_nStaticNodes ) * ( m_pNodeIntegrator ? 6 : 5 ) + m_nStaticNodes +
		m_nSimdQuadCount[ 0 ] * 20 +
		m_nSimdRodCount * 5 +
		m_nSimdTriCount[ 0 ] * 10 +
		m_nSimdSpringIntegratorCount * 3;
}



uint CFeModel::ComputeCollisionTreeDepthTopDown() const
{
	CUtlVector< uint16 > levels;
	return ComputeCollisionTreeDepthTopDown( levels );
}

uint CFeModel::ComputeCollisionTreeDepthTopDown( CUtlVector< uint16 > &levels ) const
{
	if ( GetDynamicNodeCount() > 0 )
	{
		levels.SetCount( GetDynamicNodeCount() * 2 - 1 );
		return ComputeCollisionTreeDepthTopDown( levels.Base() );
	}
	else
	{
		return 0;
	}
}


uint CFeModel::ComputeCollisionTreeDepthTopDown( uint16 *pLevels ) const
{
	if ( !m_pTreeChildren )
		return 0;

	const uint nDynCount = GetDynamicNodeCount();
	const uint nInvalidLevel = 0xFFFF;
	if ( IsDebug() )
	{
		for ( int nCluster = 2 * nDynCount - 1; nCluster-- > 0; )
		{
			pLevels[ nCluster ] = nInvalidLevel;
		}
	}

	uint nLevel = 0;
	pLevels[ 2 * nDynCount - 2 ] = nLevel; // top
	for ( int nParent = nDynCount - 1; nParent-- > 0; )
	{
		const FeTreeChildren_t &children = m_pTreeChildren[ nParent ];
		uint nNewLevel = 1 + pLevels[ nParent + nDynCount ];
		pLevels[ children.nChild[ 0 ] ] = pLevels[ children.nChild[ 1 ] ] = nNewLevel;
		nLevel = Max( nNewLevel, nLevel );
	}

	if ( IsDebug() )
	{
		// we should've filled out everything
		for ( int nCluster = 2 * nDynCount - 1; nCluster-- > 0; )
		{
			Assert( pLevels[ nCluster ] != nInvalidLevel );
		}
		for ( uint i = 0; i < 2 * nDynCount - 2; ++i )
		{
			Assert( pLevels[ i ] == 1 + pLevels[ m_pTreeParents[ i ] ] );
		}
	}
	return nLevel;
}


uint CFeModel::ComputeCollisionTreeHeightBottomUp( CUtlVector< uint16 > &levels ) const
{
	if ( GetDynamicNodeCount() > 1 )
	{
		levels.SetCount( GetDynamicNodeCount() - 1 );
		return ComputeCollisionTreeDepthTopDown( levels.Base() );
	}
	else
	{
		return 0;
	}
}


uint CFeModel::ComputeCollisionTreeHeightBottomUp() const
{
	CUtlVector< uint16 > levels;
	return ComputeCollisionTreeHeightBottomUp( levels );
}


uint CFeModel::ComputeCollisionTreeHeightBottomUp( uint16 *pLevels ) const
{
	if ( !m_pTreeParents )
		return 0;
	const uint nDynCount = GetDynamicNodeCount();
	for ( uint i = nDynCount - 1; i-- > 0; )
	{
		pLevels[ i ] = 1; // all clusters have at least level 1
	}
	uint nLevel = 0;
	for ( uint i = 0; i < nDynCount - 2; ++i )
	{
		AssertDbg( m_pTreeParents[ i + nDynCount ] > i + nDynCount && m_pTreeParents[ i + nDynCount ] < 2 * nDynCount - 1 ); // nodes and clusters go strictly child-to-parent
		uint16 &refParentLevel = pLevels[ m_pTreeParents[ i + nDynCount ] - nDynCount ];
		uint nChildLevel = pLevels[ i ];
		refParentLevel = Max< uint >( refParentLevel, nChildLevel + 1 );
		nLevel = Max< uint >( refParentLevel, nLevel );
	}

	if ( IsDebug() )
	{
		for ( uint i = nDynCount - 1; i-- > 0; )
		{
			const FeTreeChildren_t &tc = m_pTreeChildren[ i ];
			uint nChildLevel[ 2 ];
			for ( uint j = 0; j < 2; ++j )
			{
				nChildLevel[ j ] = tc.nChild[ j ] < nDynCount ? 0 : pLevels[ tc.nChild[ j ] - nDynCount ];
			}
			Assert( pLevels[ i ] == Max( nChildLevel[ 0 ], nChildLevel[ 1 ] ) + 1 );
		}
	}

	return nLevel;
}

// Careful! pDynPos is the array of DYNAMIC nodes, excluding m_nStaticNodes static nodes at the beginning of the array (don't need static positions to compute dynamic collision)
// tree parents are defined for each dynamic node, and for each compound node. There are always nDynCount - 1 compound nodes. AABB is always defined only for compound nodes.
void CFeModel::ComputeCollisionTreeBounds( const fltx4 *pDynPos, FeAabb_t *pClusters )const
{
	const uint nDynCount = GetDynamicNodeCount();
	for ( uint i = nDynCount - 1; i-- > 0; )
	{
		pClusters[ i ].m_vMaxBounds = -Four_FLT_MAX;
		pClusters[ i ].m_vMinBounds =  Four_FLT_MAX;
	}

	if ( m_pNodeCollisionRadii )
	{
		for ( uint i = 0; i < nDynCount; ++i )
		{
			uint nParent = m_pTreeParents[ i ] - nDynCount;
			Assert( nParent < nDynCount - 1 );
			pClusters[ nParent ].AddCenterAndExtents( pDynPos[ i ], ReplicateX4( m_pNodeCollisionRadii[ i ] ) );
		}
	}
	else
	{
		for ( uint i = 0; i < nDynCount; ++i )
		{
			uint nParent = m_pTreeParents[ i ] - nDynCount;
			Assert( nParent < nDynCount - 1 );
			pClusters[ nParent ] |= pDynPos[ i ];
		}
	}

	for ( uint i = 0; i < nDynCount - 2; ++i )
	{
		uint nParent = m_pTreeParents[ i + nDynCount ] - nDynCount;
		Assert( nParent < nDynCount - 1 );
		pClusters[ nParent ] |= pClusters[ i ];
	}
}


float CFeModel::ComputeCollisionTreeBoundsError( const fltx4 *pDynPos, const FeAabb_t *pClusters )const
{
	const uint nDynCount = GetDynamicNodeCount();
	fltx4 f4Error = Four_Zeros;

	if ( m_pNodeCollisionRadii )
	{
		for ( uint i = 0; i < nDynCount; ++i )
		{
			uint nParent = m_pTreeParents[ i ] - nDynCount;
			Assert( nParent < nDynCount - 1 );
			f4Error = MaxSIMD( f4Error, pClusters[ nParent ].GetDistVector( pDynPos[ i ] - ReplicateX4( m_pNodeCollisionRadii[ i ] ) ) );
			f4Error = MaxSIMD( f4Error, pClusters[ nParent ].GetDistVector( pDynPos[ i ] + ReplicateX4( m_pNodeCollisionRadii[ i ] ) ) );
		}
	}
	else
	{
		for ( uint i = 0; i < nDynCount; ++i )
		{
			uint nParent = m_pTreeParents[ i ] - nDynCount;
			Assert( nParent < nDynCount - 1 );
			f4Error = MaxSIMD( f4Error, pClusters[ nParent ].GetDistVector( pDynPos[ i ] ) );
		}
	}

	for ( uint i = 0; i < nDynCount - 2; ++i )
	{
		uint nParent = m_pTreeParents[ i + nDynCount ] - nDynCount;
		Assert( nParent < nDynCount - 1 );
		f4Error = MaxSIMD( f4Error, pClusters[ nParent ].GetDistVector( pClusters[ i ].m_vMinBounds ) );
		f4Error = MaxSIMD( f4Error, pClusters[ nParent ].GetDistVector( pClusters[ i ].m_vMaxBounds ) );
	}

	return SubFloat( SqrtSIMD( Dot3SIMD( f4Error, f4Error ) ), 0 );
}


uint CFeModel::ComputeCollisionTreeNodeCount( uint16 *pCounts ) const
{
	uint nDynCount = GetDynamicNodeCount();
	for ( uint i = 0; i < nDynCount - 1; ++i )
	{
		pCounts[ i ] = 0;
	}

	for ( uint i = 0; i < nDynCount; ++i )
	{
		uint nParent = m_pTreeParents[ i ] - nDynCount;
		AssertDbg( nParent < nDynCount - 1 );
		pCounts[ nParent ]++;
	}
	for ( uint i = nDynCount; i < 2 * nDynCount - 2; ++i )
	{
		uint nParent = m_pTreeParents[ i ] - nDynCount;
		AssertDbg( nParent < nDynCount - 1 );
		pCounts[ nParent ] += pCounts[ i - nDynCount ];
	}
	AssertDbg( nDynCount == pCounts[ nDynCount - 2 ] );
	return nDynCount;
}


void CFeModel::ApplyAirDrag( VectorAligned *pPos0, const VectorAligned *pPos1, float flExpDrag, float flVelDrag )
{
	fltx4 f4ExpDrag = ReplicateX4( flExpDrag ), f4VelDrag = ReplicateX4( flVelDrag );
	for ( uint i = m_nStaticNodes; i < m_nNodeCount; ++i )
	{
		fltx4 pos0 = LoadAlignedSIMD( pPos0[ i ].Base() ), pos1 = LoadAlignedSIMD( pPos1[ i ].Base() );
		fltx4 dvdt = pos1 - pos0, lenSqr = Dot3SIMD( dvdt, dvdt );

		fltx4 add = dvdt * MinSIMD( f4ExpDrag + f4VelDrag * SqrtEstSIMD( lenSqr ), Four_Ones );
		StoreAlignedSIMD( pPos0[ i ].Base(), pos0 + add );
	}
}

FORCEINLINE FourVectors ComputeQuadAirDrag( const FourVectors &vAn, const FourVectors &d, const fltx4 &f4ExpDrag, const fltx4 &f4VelDrag, const fltx4 &lenSqInvAn )
{
	//return MinSIMD( AbsSIMD( DotProduct( vAxisZ, q0 - p0 ) * f4VelDrag ), Four_Ones ) * ( q0 - p0 );
	fltx4 An_d = DotProduct( vAn, d );
	return ( MinSIMD( Four_Ones, f4ExpDrag + AbsSIMD( An_d ) * f4VelDrag ) * An_d * lenSqInvAn ) * vAn;
}

void CFeModel::ApplyQuadAirDrag( fltx4 *pPos0, const fltx4 *pPos1, float flExpDrag, float flVelDrag )
{
	fltx4 f4ExpDrag = ReplicateX4( flExpDrag ), f4VelDrag = ReplicateX4( flVelDrag );

	for ( uint nQuad = m_nSimdQuadCount[ 1 ]; nQuad < m_nSimdQuadCount[ 0 ]; ++nQuad )
	{
		const FeSimdQuad_t &quad = m_pSimdQuads[ nQuad ];

		FourVectors p0, p1, p2, p3, q0, q1, q2, q3;
		LOAD_NODES_POS( pPos0, p0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
		LOAD_NODES_POS( pPos1, q0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos1, q1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos1, q2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos1, q3, quad.nNode[ 3 ] );

		FourVectors vAn = CrossProduct( p2 - p0, p3 - p1);
		fltx4 lenSqAn = MaxSIMD( vAn.LengthSqr(), Four_2ToTheMinus30 );
		fltx4 lenSqInvAn = ReciprocalEstSIMD( lenSqAn );
		// fltx4 f4ScaledVelDrag = f4VelDrag * f4AxisZlenInv;
		// fltx4 f4AxisZlen = f4AxisZlenSqr * f4AxisZlenInv;
		p0 += ComputeQuadAirDrag( vAn, q0 - p0, f4ExpDrag, f4VelDrag, lenSqInvAn );
		p1 += ComputeQuadAirDrag( vAn, q1 - p1, f4ExpDrag, f4VelDrag, lenSqInvAn );
		p2 += ComputeQuadAirDrag( vAn, q2 - p2, f4ExpDrag, f4VelDrag, lenSqInvAn );
		p3 += ComputeQuadAirDrag( vAn, q3 - p3, f4ExpDrag, f4VelDrag, lenSqInvAn );

		SAVE_NODES_POS( pPos0, p0, quad.nNode[ 0 ] );
		SAVE_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		SAVE_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		SAVE_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
	}
}

void CFeModel::ApplyRodAirDrag( fltx4 *pPos0, const fltx4 *pPos1, float flExpDrag, float flVelDrag )
{
}


void CFeModel::ApplyQuadWind( VectorAligned *pPos1, const Vector &vWind, float flAirDrag )
{
	for ( uint nQuad = 0; nQuad < m_nQuadCount[ 0 ]; ++nQuad )
	{
		const FeQuad_t &quad = m_pQuads[ nQuad ];
		VectorAligned &p0 = pPos1[ quad.nNode[ 0 ] ], &p1 = pPos1[ quad.nNode[ 1 ] ], &p2 = pPos1[ quad.nNode[ 2 ] ], &p3 = pPos1[ quad.nNode[ 3 ] ];
		Vector vArea = CrossProduct( p2 - p0, p3 - p1 );
		float flArea = vArea.Length();
		Vector vNormal = vArea / flArea;
		float flNormalFlow = DotProduct( vNormal, vWind );
		Vector vNormalFlow = flNormalFlow * vNormal;
		Vector vTangentFlow = vWind - vNormalFlow;

		Vector vNormalImpulse = vArea * flNormalFlow;// == vNormalFlow * flArea;
		Vector vTangentImpulse = ( flAirDrag * flArea ) * vTangentFlow;
		Vector vQuadImpulse = vNormalImpulse + vTangentImpulse;

		p0 += quad.vShape[ 0 ].w * vQuadImpulse;
		p1 += quad.vShape[ 1 ].w * vQuadImpulse;
		p2 += quad.vShape[ 2 ].w * vQuadImpulse;
		p3 += quad.vShape[ 3 ].w * vQuadImpulse;
	}
}


void CFeModel::SmoothQuadVelocityField( fltx4 *pPos0, const fltx4 *pPos1, float flBlendFactor )
{
	fltx4 f4MulOriginal = ReplicateX4( flBlendFactor ), f4MulTarget = Four_Ones - f4MulOriginal, f4MulDoubleTarget = Four_PointFives * f4MulTarget;

	// with 2 points fixed, the target velocity is controlled by them only (conservation of momentum in massive-light particle interaction, assuming the 2 top particles have the same mass)
	for ( uint nQuad = 0; nQuad < m_nSimdQuadCount[ 2 ]; ++nQuad )
	{
		const FeSimdQuad_t &quad = m_pSimdQuads[ nQuad ];

		FourVectors p0, p1, p2, p3, q0, q1, q2, q3;
		LOAD_NODES_POS( pPos0, p0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
		LOAD_NODES_POS( pPos1, q0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos1, q1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos1, q2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos1, q3, quad.nNode[ 3 ] );

		// note: this is negative velocitie (backward-time velocities)
		FourVectors v0 = p0 - q0, v1 = p1 - q1, v2 = p2 - q2, v3 = p3 - q3;
		FourVectors vPremulTarget = ( v0 + v1 ) * f4MulDoubleTarget;

		p2 = q2 + v2 * f4MulOriginal + vPremulTarget;
		p3 = q3 + v3 * f4MulOriginal + vPremulTarget;

		SAVE_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		SAVE_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
	}

	// with 1 point fixed, the target velocity is that point's velocity (conservation of momentum in massive-light particle interaction)
	for ( uint nQuad = m_nSimdQuadCount[ 2 ]; nQuad < m_nSimdQuadCount[ 1 ]; ++nQuad )
	{
		const FeSimdQuad_t &quad = m_pSimdQuads[ nQuad ];

		FourVectors p0, p1, p2, p3, q0, q1, q2, q3;
		LOAD_NODES_POS( pPos0, p0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
		LOAD_NODES_POS( pPos1, q0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos1, q1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos1, q2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos1, q3, quad.nNode[ 3 ] );

		// note: this is negative velocitie (backward-time velocities)
		FourVectors v0 = p0 - q0, v1 = p1 - q1, v2 = p2 - q2, v3 = p3 - q3;
		FourVectors vPremulTarget = v0 * f4MulTarget;

		p1 = q1 + v1 * f4MulOriginal + vPremulTarget;
		p2 = q2 + v2 * f4MulOriginal + vPremulTarget;
		p3 = q3 + v3 * f4MulOriginal + vPremulTarget;

		SAVE_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		SAVE_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		SAVE_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
	}

	// with no points fixed, we're conserving momentum
	for ( uint nQuad = m_nSimdQuadCount[ 1 ]; nQuad < m_nSimdQuadCount[ 0 ]; ++nQuad )
	{
		const FeSimdQuad_t &quad = m_pSimdQuads[ nQuad ];

		FourVectors p0, p1, p2, p3, q0, q1, q2, q3;
		LOAD_NODES_POS( pPos0, p0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
		LOAD_NODES_POS( pPos1, q0, quad.nNode[ 0 ] );
		LOAD_NODES_POS( pPos1, q1, quad.nNode[ 1 ] );
		LOAD_NODES_POS( pPos1, q2, quad.nNode[ 2 ] );
		LOAD_NODES_POS( pPos1, q3, quad.nNode[ 3 ] );

		// note: this is negative velocitie (backward-time velocities)
		FourVectors v0 = p0 - q0, v1 = p1 - q1, v2 = p2 - q2, v3 = p3 - q3;
		const fltx4 *m = quad.f4Weights;
		FourVectors vPremulTarget = ( v0 * m[ 0 ] + v1 * m[ 1 ] + v2 * m[ 2 ] + v3 * m[ 3 ] ) * f4MulTarget; // ~12 flops

		p0 = q0 + v0 * f4MulOriginal + vPremulTarget;
		p1 = q1 + v1 * f4MulOriginal + vPremulTarget;
		p2 = q2 + v2 * f4MulOriginal + vPremulTarget;
		p3 = q3 + v3 * f4MulOriginal + vPremulTarget;

		SAVE_NODES_POS( pPos0, p0, quad.nNode[ 0 ] );
		SAVE_NODES_POS( pPos0, p1, quad.nNode[ 1 ] );
		SAVE_NODES_POS( pPos0, p2, quad.nNode[ 2 ] );
		SAVE_NODES_POS( pPos0, p3, quad.nNode[ 3 ] );
	}
}


void CFeModel::SmoothRodVelocityField( fltx4 *pPos0, const fltx4 *pPos1, float flBlendFactor )
{
	fltx4 f4MulOriginal = ReplicateX4( flBlendFactor ), f4MulTarget = Four_Ones - f4MulOriginal;

	for ( uint i = 0; i < m_nSimdRodCount; ++i )
	{
		const FeSimdRodConstraint_t &rod = m_pSimdRods[ i ];
		AssertDbg(
			   rod.nNode[ 0 ][ 0 ] < m_nNodeCount && rod.nNode[ 1 ][ 0 ] < m_nNodeCount
			&& rod.nNode[ 0 ][ 1 ] < m_nNodeCount && rod.nNode[ 1 ][ 1 ] < m_nNodeCount
			&& rod.nNode[ 0 ][ 2 ] < m_nNodeCount && rod.nNode[ 1 ][ 2 ] < m_nNodeCount
			&& rod.nNode[ 0 ][ 3 ] < m_nNodeCount && rod.nNode[ 1 ][ 3 ] < m_nNodeCount
			);
		FourVectors p0, p1, q0, q1;
		LOAD_NODES_POS( pPos0, p0, rod.nNode[ 0 ] );
		LOAD_NODES_POS( pPos0, p1, rod.nNode[ 1 ] );
		LOAD_NODES_POS( pPos1, q0, rod.nNode[ 0 ] );
		LOAD_NODES_POS( pPos1, q1, rod.nNode[ 1 ] );
		// this is conservation of momentum principle, and it works directly with m1 = 1-w0 because: invM1/(invM0+invM1) == m0/(m0+m1)
		FourVectors v0 = p0 - q0, v1 = p1 - q1;
		FourVectors vPremulTarget = ( v0 + ( v1 - v0 ) * rod.f4Weight0 ) * f4MulTarget;// v0 * ( Four_Ones - rod.f4Weight0 ) + v1 * rod.f4Weight0;
		p0 = q0 + v0 * f4MulOriginal + vPremulTarget;
		p1 = q1 + v1 * f4MulOriginal + vPremulTarget;

		SAVE_NODES_POS( pPos0, p0, rod.nNode[ 0 ] );
		SAVE_NODES_POS( pPos0, p1, rod.nNode[ 1 ] );
	}
}


SVD::SymMatrix3< float > AsSymMatrix3( const CovMatrix3 &cov )
{
	SVD::SymMatrix3< float > sym;
	sym.m00() = cov.m_vDiag.x;
	sym.m11() = cov.m_vDiag.y;
	sym.m22() = cov.m_vDiag.z;
	sym.m01() = cov.m_flXY;
	sym.m02() = cov.m_flXZ;
	sym.m12() = cov.m_flYZ;
	return sym;
}

matrix3x4_t AsMatrix3x4( SVD::Matrix3< float > &m, const Vector &vOrigin )
{
	matrix3x4_t res;
	for ( int i = 0; i < 3; ++i )
		for ( int j = 0; j < 3; ++j )
			res.m_flMatVal[ i ][ j ] = m.m[ i ][ j ]; 
	res.SetOrigin( vOrigin );
	return res;
}

matrix3x4_t AsMatrix3x4_Transposed( SVD::Matrix3< float > &m, const Vector &vOrigin )
{
	matrix3x4_t res;
	for ( int i = 0; i < 3; ++i )
		for ( int j = 0; j < 3; ++j )
			res.m_flMatVal[ i ][ j ] = m.m[ j ][ i ];
	res.SetOrigin( vOrigin );
	return res;
}

CovMatrix3 CovMatrix3::GetPseudoInverse()
{
	SVD::SymMatrix3< float > sym = AsSymMatrix3( *this );

	SVD::SymMatrix3< float > pi = SVD::PseudoInverse( sym );
	CovMatrix3 covPi;
	covPi.m_vDiag.x = pi.m00();
	covPi.m_vDiag.y = pi.m11();
	covPi.m_vDiag.z = pi.m22();
	covPi.m_flXY = pi.m01();
	covPi.m_flYZ = pi.m12();
	covPi.m_flXZ = pi.m02();
	return covPi;
}


void CFeModel::FitCenters( fltx4 *pPos )const
{
	uint nMatrix = 0, nRunningWeight = 0;
	for ( ; nMatrix < m_nFitMatrixCount[ 2 ]; ++nMatrix )
	{
		//AssertDbg( nRunningWeight == fm.nBegin );
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		AssertDbg( nRunningWeight + 2 <= fm.nEnd );
		const FeFitWeight_t *w = m_pFitWeights + nRunningWeight;

		// weights don't matter, because we'll just use the line between the two nodes as a hinge
		pPos[ fm.nNode ] = Four_PointFives * ( pPos[ w[ 0 ].nNode ] + pPos[ w[ 1 ].nNode ] );
		nRunningWeight = fm.nEnd;
	}

	for ( ; nMatrix < m_nFitMatrixCount[ 1 ]; ++nMatrix )
	{
		//AssertDbg( nRunningWeight == fm.nBegin );
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t &w = m_pFitWeights[ nRunningWeight ];
		pPos[ fm.nNode ] = pPos[ w.nNode ];// we'll hinge around this point
		nRunningWeight = fm.nEnd;
	}

	for ( ; nMatrix < m_nFitMatrixCount[ 0 ]; ++nMatrix )
	{
		//AssertDbg( nRunningWeight == fm.nBegin );
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *w = m_pFitWeights;
		fltx4 v4Center = Four_Zeros;
		fltx4 f4Sum = Four_Zeros;
		uint nEndDominant = fm.nBeginDynamic == nRunningWeight ? fm.nEnd : fm.nBeginDynamic; // count only static nodes if there are any static nodes here
		for ( ; nRunningWeight < nEndDominant; ++nRunningWeight )
		{
			fltx4 f4Weight = ReplicateX4( &w[ nRunningWeight ].flWeight );
			f4Sum += f4Weight;
			v4Center += pPos[ w[ nRunningWeight ].nNode ] * f4Weight;
		}
		pPos[ fm.nNode ] = v4Center * ReciprocalSIMD( f4Sum );// we'll hinge around this point
		nRunningWeight = fm.nEnd; // go to the next
	}
}



void CFeModel::FitTransforms( const VectorAligned *pPos, matrix3x4a_t *pOut )const
{
	uint nMatrix = 0;
	const FeFitWeight_t *pWeights = m_pFitWeights;
	matrix3x4a_t tm;
/*
	for ( ; nMatrix < m_nFitMatrixCount[ 2 ]; ++nMatrix )
	{
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *pWeightsEnd = m_pFitWeights + fm.nEnd;
		//Vector vAxis = pPos[ pWeights[ 0 ].nNode ] - pPos[ pWeights[ 1 ].nNode ];
		FitTransform( &tm, fm, pPos, pWeights + 2, pWeightsEnd );
		pOut[ fm.nCtrl ] = tm * TransformMatrix( fm.bone );
		pWeights = pWeightsEnd;
	}

	for ( ; nMatrix < m_nFitMatrixCount[ 1 ]; ++nMatrix )
	{
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *pWeightsEnd = m_pFitWeights + fm.nEnd;
		AssertDbg( pWeights + 1 <= pWeightsEnd );
		FitTransform( &tm, fm, pPos, pWeights + 1, pWeightsEnd );
		pOut[ fm.nCtrl ] = tm * TransformMatrix( fm.bone );
		pWeights = pWeightsEnd;
	}
*/

	for ( ; nMatrix < m_nFitMatrixCount[ 0 ]; ++nMatrix )
	{
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *pWeightsEnd = m_pFitWeights + fm.nEnd;
		FitTransform( &tm, fm, pPos, pWeights, pWeightsEnd );
		pOut[ fm.nCtrl ] = tm * TransformMatrix( fm.bone );
		pWeights = pWeightsEnd;
	}
}


void CFeModel::FeedbackFitTransforms( VectorAligned *pPos, float flStiffness )const
{
	uint nMatrix = 0;
	matrix3x4a_t tm;
	const FeFitWeight_t *pWeights = m_pFitWeights;
/*
	for ( ; nMatrix < m_nFitMatrixCount[ 2 ]; ++nMatrix )
	{
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *pWeightsEnd = m_pFitWeights + fm.nEnd;
		FitTransform( &tm, fm, pPos, pWeights + 2, pWeightsEnd );
		FeedbackFitTransform( tm, fm, pPos, m_pFitWeights + fm.nBeginDynamic, pWeightsEnd, flStiffness );
		pWeights = pWeightsEnd;
	}

	for ( ; nMatrix < m_nFitMatrixCount[ 1 ]; ++nMatrix )
	{
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *pWeightsEnd = m_pFitWeights + fm.nEnd;
		AssertDbg( pWeights + 1 <= pWeightsEnd );
		FitTransform( &tm, fm, pPos, pWeights + 1, pWeightsEnd );
		FeedbackFitTransform( tm, fm, pPos, m_pFitWeights + fm.nBeginDynamic, pWeightsEnd, flStiffness );
		pWeights = pWeightsEnd;
	}
*/

	for ( ; nMatrix < m_nFitMatrixCount[ 0 ]; ++nMatrix )
	{
		const FeFitMatrix_t &fm = m_pFitMatrices[ nMatrix ];
		const FeFitWeight_t *pWeightsEnd = m_pFitWeights + fm.nEnd;
		FitTransform( &tm, fm, pPos, pWeights, pWeightsEnd );
		FeedbackFitTransform( tm, fm, pPos, m_pFitWeights + fm.nBeginDynamic, pWeightsEnd, flStiffness );
		pWeights = pWeightsEnd;
	}
}



Vector GetColumn( const SVD::Matrix3< float > &m, int c )
{
	return Vector( m.m[ 0 ][ c ], m.m[ 1 ][ c ], m.m[ 2 ][ c ] );
}


void CFeModel::FeedbackFitTransform( const matrix3x4a_t &tm, const FeFitMatrix_t &fm, VectorAligned *pPos, const FeFitWeight_t *pWeights, const FeFitWeight_t *pWeightsEnd, float flStiffness )const
{
	for ( const FeFitWeight_t *p = pWeights; p < pWeightsEnd; ++p )
	{
		VectorAligned &refPos = pPos[ p->nNode ];
		Vector vQ = m_pInitPose[ p->nNode ].m_vPosition - fm.vCenter;
		refPos = refPos * ( 1.0f - flStiffness ) + VectorTransform( vQ, tm ) * flStiffness;
	}
}


void CFeModel::FitTransform( matrix3x4a_t*pOut, const FeFitMatrix_t &fm, const VectorAligned *pPos, const FeFitWeight_t *pWeights, const FeFitWeight_t *pWeightsEnd )const
{
	const VectorAligned &vDynCenter = pPos[ fm.nNode ];
	SVD::Matrix3<float> Apq;
	Apq.SetZero();
	for ( const FeFitWeight_t *p = pWeights; p < pWeightsEnd; ++p )
	{
		Vector vP = pPos[ p->nNode ] - vDynCenter, vQ = m_pInitPose[ p->nNode ].m_vPosition - fm.vCenter;
		float m = p->flWeight;
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 3; ++j )
				Apq.m[ i ][ j ] += vP[ i ] * vQ[ j ] * m;
	};
	SVD::SvdIterator<float> svd;
	//SVD::Matrix3<float> a = Apq * SVD::Matrix3<float>( AsSymMatrix3( fm.AqqInv ) ); // optimal but skewed fitting matrix; Aqq is not really necessary except when we clamp the singular values (allow some stretch)
	svd.Init( Apq );
	svd.Iterate( 6, FLT_EPSILON ); // the epsilon is the sum of squares of sines of Givens rotations, I think it's a very good measure of quality in this case
	SVD::Matrix3< float > v = svd.ComputeV();
	// B = US = AV
	SVD::Matrix3< float > us = Apq * v;
	// columns of U*S matrix have length of singular values. Clamp them, negate the smallest if needed (if the matrix is mirror matrix)
	// TODO: measure and maybe rewrite with SIMD, as this seems slow

	// sort by singular values
	int nLarge, nMedium, nSmall;
	float flSmallAxisParity;
 	if ( svd.ata.m00() > svd.ata.m11() )
	{
		if ( svd.ata.m11() > svd.ata.m22() )
		{
			nLarge = 0; nMedium = 1; nSmall = 2; flSmallAxisParity = 1.0f;
		}
		else if ( svd.ata.m22() > svd.ata.m00() )
		{
			nLarge = 2; nMedium = 0; nSmall = 1; flSmallAxisParity = 1.0f;
		}
		else
		{
			nLarge = 0; nMedium = 2; nSmall = 1; flSmallAxisParity = -1.0f;
		}
	}
	else
	{
		if ( svd.ata.m00() > svd.ata.m22() )
		{
			nLarge = 1; nMedium = 0; nSmall = 2; flSmallAxisParity = -1.0f;
		}
		else if ( svd.ata.m22() > svd.ata.m11() )
		{
			nLarge = 2; nMedium = 1; nSmall = 0; flSmallAxisParity = -1.0f;
		}
		else
		{
			nLarge = 1; nMedium = 2; nSmall = 0; flSmallAxisParity = 1.0f;
		}
	}
#if defined( _DEBUG ) && defined( DBGFLAG_ASSERT )
	float s[ 3 ] = { svd.ata.m00(), svd.ata.m11(), svd.ata.m22() };
	Assert( s[ nLarge ] >= s[ nMedium ] && s[ nMedium ] >= s[ nSmall ] );
#endif

	matrix3x4a_t u;
	Vector vMainAxis( us.m[0][nLarge], us.m[1][nLarge], us.m[2][nLarge ] );
	float flMainAxisLength = vMainAxis.Length();

	if ( flMainAxisLength < FLT_EPSILON )
	{
		// really, really small - very undefined matrix
		pOut->SetToIdentity();
		return;
	}
	vMainAxis /= flMainAxisLength;

	u.SetColumn( vMainAxis, ( MatrixAxisType_t )nLarge );

	Vector vMedAxis( us.m[0][nMedium], us.m[1][nMedium], us.m[2][nMedium ] );
	vMedAxis -= DotProduct( vMainAxis, vMedAxis ) * vMainAxis;
	float flMedAxisLength = vMedAxis.Length();
	if( flMedAxisLength > FLT_EPSILON)
	{
		vMedAxis /= flMedAxisLength;
	}
	else
	{
		// fallback
		vMedAxis = VectorPerpendicularToVector(vMainAxis);
	}
	u.SetColumn(vMedAxis, ( MatrixAxisType_t)nMedium );
	// the third axis doesn't matter if we don't clamp, and we can only recover it up to the sign
	u.SetColumn( CrossProduct( vMainAxis, vMedAxis ) * flSmallAxisParity, ( MatrixAxisType_t )nSmall );
	u.SetOrigin( vec3_origin );

#if 0
	// U matrix columns are the axes we need; some of them will be Zero
	fltx4 uRow0 = LoadUnaligned3SIMD( us.m[ 0 ] );
	fltx4 uRow1 = LoadUnaligned3SIMD( us.m[ 1 ] );
	fltx4 uRow2 = LoadUnaligned3SIMD( us.m[ 2 ] );

	fltx4 uLengthSqr = uRow0 * uRow0 + uRow1 * uRow1 + uRow2 * uRow2;
	fltx4 uLengthInv = ReciprocalSqrtSIMD( MaxSIMD( uLengthSqr, Four_Epsilons ) );

	// When/if we support non-orthouniform animation matrices, we should clamp the length, not set it to 1. That will make for much nicer softbody
	matrix3x4a_t u;
	u.SIMDRow( 0 ) = /*SetWToZeroSIMD*/( uRow0 * uLengthInv );
	u.SIMDRow( 1 ) = /*SetWToZeroSIMD*/( uRow1 * uLengthInv );
	u.SIMDRow( 2 ) = /*SetWToZeroSIMD*/( uRow2 * uLengthInv );

#ifdef _DEBUG
	fltx4 sSqr = { svd.ata.m00(), svd.ata.m11(), svd.ata.m22(), 0.0f };	NOTE_UNUSED( sSqr );
	fltx4 sInv = ReciprocalSqrtSIMD( MaxSIMD( sSqr, Four_Epsilons ) ); NOTE_UNUSED( sInv );// reciprocal estimate is actually fine here, but may produce slightly non-orthonormal matrices (error < 1%) which assert in a bunch of places
	// ( SVD::RsqrtEst( Max( us.ColLenSqr( 0 ), 1e-30f ) ), SVD::RsqrtEst( Max( us.ColLenSqr( 1 ), 1e-30f ) ), SVD::RsqrtEst( Max( us.ColLenSqr( 2 ), 1e-30f ) ) );
	// TODO: implement QR or some other recovery from near-0 singular values
#endif
	float det = u.GetDeterminant();

	if ( det < 0 )
	{
		// this is a mirror matrix;
		int nColumn = AsVector( uLengthSqr ).SmallestComponent(); //smallest sigma, largest sigma^-1
		u[ 0 ][ nColumn ] = -u[ 0 ][ nColumn ];
		u[ 1 ][ nColumn ] = -u[ 1 ][ nColumn ];
		u[ 2 ][ nColumn ] = -u[ 2 ][ nColumn ];
	}
#endif
	
	matrix3x4_t vt = AsMatrix3x4_Transposed( v, vec3_origin ), uvt;
	ConcatTransforms( u, vt, uvt );
	uvt.SetOrigin( vDynCenter );
	*pOut = uvt;
}


void CFeModel::FitTransform2D( matrix3x4a_t *pOut, const FeFitMatrix_t &fm, const Vector &vAxis, const VectorAligned *pPos, const FeFitWeight_t *pWeights, const FeFitWeight_t *pWeightsEnd )const
{
	FitTransform( pOut, fm, pPos, pWeights, pWeightsEnd );
/*
	const VectorAligned &vDynCenter = pPos[ fm.nNode ];
	class CFitTransform2D
	{
	public:
		CFitTransform2D( const Vector &vAxis )
		{
			m_vAxisZ = vAxis.NormalizedSafe( Vector( 0, 0, 1 ) );
			m_vAxisX = VectorPerpendicularToVector( m_vAxisZ );
			m_vAxisY = CrossProduct( m_vAxisZ, m_vAxisX );
		}
		Vector2D ToLocalXY( const Vector &v ) { return Vector2D( DotProduct( m_vAxisX, v ), DotProduct( m_vAxisY, v ) ); }
		Vector m_vAxisX;
		Vector m_vAxisY;
		Vector m_vAxisZ;
	};
	CFitTransform2D fitTm( vAxis );
	float flCos = 0, flSin = 0;
	for ( const FeFitWeight_t *p = pWeights; p < pWeightsEnd; ++p )
	{
		
		Vector2D p = fitTm.ToLocalXY( pPos[ p->nNode ] - vDynCenter ), q = fitTm.ToLocalXY( m_pInitPose[ p->nNode ].m_vPosition - fm.vCenter );
	}
	CSinCosRotation2D rotation( flCos, flSin );
*/
}


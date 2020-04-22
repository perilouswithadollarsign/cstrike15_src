//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "capsule.h"
#include "trace.h"
//#include "body.h"
//#include "mass.h"

//#include "distance.h"
//#include "gjk.h"
//#include "sat.h"

#define NUM_STACKS 8
#define NUM_SLICES 16


//--------------------------------------------------------------------------------------------------
// Local utilities 
//--------------------------------------------------------------------------------------------------
struct CapsuleCast2D_t
{
	float m_flCapsule, m_flRay;
};


//--------------------------------------------------------------------------------------------------
static void CastCapsuleRay2DCoaxialInternal( CapsuleCast2D_t &out, float mx, float dx, float h, float e )
{
	Assert( e >= 0 );
	float mxProj = mx + e; // m.x - (-e)
	if( mxProj < 0 )
	{
		// ray starts before the capsule cap
		out.m_flCapsule = 0;
		if( dx >= -mxProj ) // otherwise, ending before capsule starts: FLT_MAX
		{
			out.m_flRay = -mxProj / dx;
		}
	}
	else if( mx < h + e ) // otherwise, starting after capsule ends : FLT_MAX
	{
		out.m_flCapsule = Clamp( mx, 0.0f, h );
		out.m_flRay = 0;
	}
	else
	{
		// ray starts after the capsule cap
		out.m_flCapsule = h;
		float mxEnd = mx - ( h + e );
		if( -dx >= mxEnd )
		{
			out.m_flRay = mxEnd / -dx;
		}
	}
}


//--------------------------------------------------------------------------------------------------
static void CastCapsuleRay2DParallelInternal( CapsuleCast2D_t &out, const Vector2D &m, float dx, float h, float rr )
{
	float e2 = rr - Sqr( m.y );
	if( e2 > 0 ) // otherwise, going parallel and outside : FLT_MAX
	{
		// going parallel and inside the infinite slab at level m.y, left to right
		float e = sqrtf( e2 ); // -e..h+e is the extent 
		CastCapsuleRay2DCoaxialInternal( out, m.x, dx, h, e );
	}
}


//--------------------------------------------------------------------------------------------------
// Intersect 2D ray with 2D capsule; capsule has radius r, length h, it starts at (0,0) and ends at (h,0)
// ray goes from m, delta d
// return: time of hit
static void CastCapsuleRay2DInternal( CapsuleCast2D_t &out, const Vector2D &m, const Vector2D &d, float h, float rr )
{
	Assert( rr >= 0 );
	Assert( d.y > -FLT_EPSILON );
	Assert( d.y != 0.0f ); // otherwise it's going parallel
	float my2 = Sqr( m.y );
	out.m_flCapsule = Clamp( m.x, 0.0f, h );

	// Easy case we'll have to check a few times if we delay: are we starting in solid?
	// same idea as with box-box distance: cut out x=0..h, capsule becomes a circle, find distance to circle
	// I'm sure there's more elegant way to handle it
	if( Sqr( m.x - out.m_flCapsule ) + my2 < rr )
	{
		out.m_flRay = 0; // start-in-solid
		return;
	}
	// well, we don't start inside the capsule. Good to know
	float r = sqrtf( rr ), dd = Sqr( d.x ) + Sqr( d.y ), ddInv = 1.0f / dd, dymy = d.y * m.y;

	// first, intersect the ray with the rectangle

	float t = ( -r - m.y ) / d.y, t0 = fpmax( 0, t ), s0 = m.x + d.x * t0;

	// solutions: -b0±sqrt(b0^2-c0) , -b1±sqrt(b1^2-c1) with	± controlled by d.x sign
	// since we know we go left-bottom to right-top, we can just choose the circle we wanna hit
	// since we know we don't start-in-solid, we know the first root (if any) will be t>=0
	float mxh;
	if( s0 < 0 )
	{
		// we're entering through the left cap
		// if we hit, we hit left circle
		out.m_flCapsule = 0;
		mxh = m.x;
	}
	else if( s0 < h )
	{
		// we're entering through the side of the capsule
		out.m_flCapsule = s0;
		if( t >= 0 ) // only if we didn't enter before ray started; otherwise, since we didn't start-in-solid, we don't hit capsule at all
		{
			out.m_flRay = t; // the caller will sort out if it's >1 or not
		}
		return;
	}
	else
	{
		out.m_flCapsule = h;
		mxh = m.x - h;
	}

	float b = ( d.x * mxh + dymy ) * ddInv, c = ( mxh * mxh + my2 - rr ) * ddInv, D = b * b - c;
	if( D >= 0 )
	{
		float tc = -b - sqrtf( D );
		Assert( tc - t >= -1e-4f );	// the ray should really enter the circle after it entered the stripe of halfspaces 
		// if tc < 0, we entered capsule before ray began; since we didn't start-in-solid, it means we don't hit the capsule at all
		if( tc >= 0 )
		{
			out.m_flRay = tc;
		}
	}
}



static void CastCapsuleShortRay( CShapeCastResult &out, const Vector &sUnit, float sLen, const Vector &m, const Vector &vRayStart, const Vector vCenter[], float flRadius )
{
	// the ray is too short, just compute the distance to the capsule and compare with radius
	// if we really need both high precision and stability, we need to compute distance to capsule from both ends of the ray: the capsule curvature is very low in the vicinity of the ray and is o(d^2) effect
	float flProjOnCapsule = DotProduct( sUnit, m );
	Vector vDistance;
	if( flProjOnCapsule < 0 )
	{
		vDistance = m;
	}
	else if( flProjOnCapsule > sLen )
	{
		vDistance = vRayStart - vCenter[ 1 ];
		
	}
	else
	{
		vDistance = m - vCenter[ 0 ] * flProjOnCapsule ; 
	}
	
	float flDistFromCapsuleSqr = vDistance.LengthSqr();
	
	if( flDistFromCapsuleSqr > flRadius )
	{
		// the ray is outside of the capsule
		out.m_bStartInSolid = false;
		out.m_flHitTime = 1.0f;
	}
	else
	{
		out.m_bStartInSolid = true;
		out.m_flHitTime = 0;
		out.m_vHitNormal = flDistFromCapsuleSqr > 1e-8f ? vDistance / sqrtf( flDistFromCapsuleSqr ) : VectorPerpendicularToVector( sUnit );
		out.m_vHitPoint = vRayStart;
	}
}



void CastSphereRay( CShapeCastResult& out, const Vector &m, const Vector& p, const Vector& d, float flRadius );

//--------------------------------------------------------------------------------------------------
void CastCapsuleRay( CShapeCastResult& out, const Vector& vRayStart, const Vector& vRayDelta, const Vector vCenter[], float flRadius ) 
{
	Vector m = vRayStart - vCenter[0], s = vCenter[1] - vCenter[0];
	float sLen = s.Length();
	
	if( flRadius < 1e-5f )
	{
		return;
	}

	if( sLen < 1e-3f ) // note: we should filter out 0-length capsules somewhere outside of this function
	{
		CastSphereRay( out, m, vRayStart, vRayDelta, flRadius );
		return;
	}
	Vector sUnit = s / sLen;
	float dLen = vRayDelta.Length();
	if( dLen > 1e-4f )
	{
		Vector dUnit = vRayDelta / dLen;
		Vector z = CrossProduct( sUnit, dUnit );
		float zLenSqr = z.LengthSqr();
		float dsUnit = DotProduct( vRayDelta, sUnit );

		CapsuleCast2D_t cast;
		cast.m_flRay = FLT_MAX;

		if( zLenSqr > 256*256 * FLT_EPSILON * FLT_EPSILON ) // the tolerance here is found experimentally, with the target of achieving minimal orthogonality of 1e-3 between z^s and z^d
		{
			float zLen = sqrtf( zLenSqr );
			Vector zUnit = z / zLen;
	#ifdef _DEBUG
			// z must be orthogonal to capsule and ray (it's a cross product of the two); if it's not, we need to handle this case as parallel
			float flOrthogonality[2] = { DotProduct( zUnit, s ), DotProduct( zUnit, vRayDelta ) };
			Assert( fabsf( flOrthogonality[0] ) < 1e-3f * MAX( 1, MAX( zLen, sLen ) ) && fabsf( flOrthogonality[1] ) < 1e-3f * MAX( 1, MAX( zLen, dLen ) ) );
	#endif
			float mzUnit = DotProduct( m, zUnit ), rr = Sqr( flRadius ) - Sqr( mzUnit );
			if( rr <= 0 )
			{
				out.m_flHitTime = FLT_MAX;
				return;
			}
			else
			{
				Vector yUnit = CrossProduct( zUnit, sUnit );
				Vector2D mProj( DotProduct( m, sUnit ), DotProduct( m, yUnit ) );
				float dyUnit = DotProduct( vRayDelta, yUnit );
				CastCapsuleRay2DInternal( cast, mProj, Vector2D( dsUnit, dyUnit ), sLen, rr );
			}
		}
		else
		{
			// they're parallel..
			float msUnit = DotProduct( m, sUnit );
			Vector zAlt = m - sUnit * msUnit;
			float zAltLenSqr = zAlt.LengthSqr();
			if( zAltLenSqr < FLT_EPSILON * FLT_EPSILON )
			{
				// ray and capsule are coaxial...
				CastCapsuleRay2DCoaxialInternal( cast, msUnit, dsUnit, sLen, flRadius ); // note: we're passing radius!
			}
			else
			{
				// ray and capsule are parallel
				Vector zUnit = zAlt / sqrtf( zAltLenSqr ), yUnit = CrossProduct( zUnit, sUnit );
				CastCapsuleRay2DParallelInternal( cast, Vector2D( DotProduct( m, sUnit ), DotProduct( m, yUnit ) ), dsUnit, sLen, Sqr( flRadius ) - zAltLenSqr ); // r^2 may be negative here - it'll just return no hit
			}
		}

		Assert( cast.m_flRay >= 0 );
		out.m_flHitTime = cast.m_flRay;
		out.m_vHitPoint = vRayStart + vRayDelta * cast.m_flRay;
		out.m_vHitNormal = ( out.m_vHitPoint - ( vCenter[0] + sUnit * cast.m_flCapsule ) ).Normalized();
	}
	else
	{
		CastCapsuleShortRay( out, sUnit, sLen, m, vRayStart, vCenter, flRadius );
	}
}


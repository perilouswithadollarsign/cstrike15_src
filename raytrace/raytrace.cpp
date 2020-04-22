// $Id$

#include "raytrace.h"
#include <filesystem_tools.h>
#include <cmdlib.h>
#include <stdio.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static bool SameSign(float a, float b)
{
	int32 aa = *( (int * ) & a );
	int32 bb = *( (int * ) & b );
	return ( (aa ^ bb ) & 0x80000000 ) == 0;
}

int FourRays::CalculateDirectionSignMask(void) const
{
	// this code treats the floats as integers since all it cares about is the sign bit and
	// floating point compares suck.

	int ret;
	int ormask;
	int andmask;
	int32 const *treat_as_int = ( (int32 const * ) ( &direction ));

	ormask = andmask =* ( treat_as_int++ );
	ormask |=* treat_as_int;
	andmask &=* ( treat_as_int++ );
	ormask |=* ( treat_as_int );
	andmask &=* ( treat_as_int++ );
	ormask |=* ( treat_as_int );
	andmask &=* ( treat_as_int++ );
	if ( ormask >= 0 )
		ret = 0;
	else
	{
		if ( andmask < 0 )
			ret = 1;
		else return - 1;
	}
	ormask = andmask =* ( treat_as_int++ );
	ormask |=* treat_as_int;
	andmask &=* ( treat_as_int++ );
	ormask |=* ( treat_as_int );
	andmask &=* ( treat_as_int++ );
	ormask |=* ( treat_as_int );
	andmask &=* ( treat_as_int++ );
	if ( ormask < 0 )
	{
		if ( andmask < 0 )
			ret |= 2;
		else return - 1;
	}
	ormask = andmask =* ( treat_as_int++ );
	ormask |= *treat_as_int;
	andmask &= *( treat_as_int++ );
	ormask |= *( treat_as_int );
	andmask &= *( treat_as_int++ );
	ormask |= *( treat_as_int );
	andmask &= *( treat_as_int++ );
	if ( ormask < 0 )
	{
		if ( andmask < 0 )
			ret |= 4;
		else return - 1;
	}
	return ret;
}




void RayTracingEnvironment::MakeRoomForTriangles( int ntris )
{
	//OptimizedTriangleList.EnsureCapacity( ntris );
	if (! (Flags & RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS))
		TriangleColors.EnsureCapacity( ntris );
}


void RayTracingEnvironment::AddTriangle(int32 id, const Vector &v1,
										const Vector &v2, const Vector &v3,
										const Vector &color)
{
	AddTriangle( id, v1, v2, v3, color, 0, 0 );
}

void RayTracingEnvironment::AddTriangle(int32 id, const Vector &v1,
										const Vector &v2, const Vector &v3,
										const Vector &color, uint16 flags, int32 materialIndex)
{
	CacheOptimizedTriangle tmptri;
	tmptri.m_Data.m_GeometryData.m_nTriangleID = id;
	tmptri.Vertex( 0 ) = v1;
	tmptri.Vertex( 1 ) = v2;
	tmptri.Vertex( 2 ) = v3;
	tmptri.m_Data.m_GeometryData.m_nFlags = flags;
	OptimizedTriangleList.AddToTail( tmptri );
	if ( ! ( Flags & RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS ) )
		TriangleColors.AddToTail( color );
	if ( ! ( Flags & RTE_FLAGS_DONT_STORE_TRIANGLE_MATERIALS ) )
		TriangleMaterials.AddToTail( materialIndex );
// 	printf("add triange from (%f %f %f),(%f %f %f),(%f %f %f) id %d\n",
// 		   XYZ(v1),XYZ(v2),XYZ(v3),id);
}

void RayTracingEnvironment::AddQuad(
	int32 id, const Vector &v1, const Vector &v2, const Vector &v3,
	const Vector &v4,							// specify vertices in cw or ccw order
	const Vector &color)
{
	AddTriangle( id, v1, v2, v3, color );
	AddTriangle( id + 1, v1, v3, v4, color );
}


void RayTracingEnvironment::AddAxisAlignedRectangularSolid(int id,Vector minc, Vector maxc,
														   const Vector &color)
{

	// "far" face
	AddQuad( id,
			 Vector( minc.x, maxc.y, maxc.z ),
			 Vector( maxc.x, maxc.y, maxc.z ), Vector( maxc.x, minc.y, maxc.z ),
			 Vector( minc.x, minc.y, maxc.z ), color );
	// "near" face
	AddQuad( id,
			 Vector( minc.x, maxc.y, minc.z ),
			 Vector( maxc.x, maxc.y, minc.z ), Vector( maxc.x, minc.y, minc.z ),
			 Vector( minc.x, minc.y, minc.z ), color );

	// "left" face
	AddQuad( id,
			 Vector( minc.x, maxc.y, maxc.z ),
			 Vector( minc.x, maxc.y, minc.z ),
			 Vector( minc.x, minc.y, minc.z ),
			 Vector( minc.x, minc.y, maxc.z ), color );
	// "right" face
	AddQuad( id,
			 Vector( maxc.x, maxc.y, maxc.z ),
			 Vector( maxc.x, maxc.y, minc.z ),
			 Vector( maxc.x, minc.y, minc.z ),
			 Vector( maxc.x, minc.y, maxc.z ), color );

	// "top" face
	AddQuad( id,
			 Vector( minc.x, maxc.y, maxc.z ),
			 Vector( maxc.x, maxc.y, maxc.z ),
			 Vector( maxc.x, maxc.y, minc.z ),
			 Vector( minc.x, maxc.y, minc.z ), color );
	// "bot" face
	AddQuad( id,
			 Vector( minc.x, minc.y, maxc.z ),
			 Vector( maxc.x, minc.y, maxc.z ),
			 Vector( maxc.x, minc.y, minc.z ),
			 Vector( minc.x, minc.y, minc.z ), color );
}



static Vector GetEdgeEquation( Vector p1, Vector p2, int c1, int c2, Vector InsidePoint )
{
	float nx = p1[c2]- p2[c2];
	float ny = p2[c1]- p1[c1];
	float d =- ( nx * p1[c1]+ ny * p1[c2] );
// 	assert(fabs(nx*p1[c1]+ny*p1[c2]+d)<0.01);
// 	assert(fabs(nx*p2[c1]+ny*p2[c2]+d)<0.01);

	// use the convention that negative is "outside"
	float trial_dist = InsidePoint[c1]* nx + InsidePoint[c2]* ny + d;
	if ( trial_dist < 0 )
	{
		nx = -nx;
		ny = -ny;
		d = -d;
		trial_dist = -trial_dist;
	}
	nx /= trial_dist;										// scale so that it will be =1.0 at the oppositve vertex
	ny /= trial_dist;
	d /= trial_dist;

	return Vector( nx, ny, d );
}

void CacheOptimizedTriangle::ChangeIntoIntersectionFormat(void)
{
	// lose the vertices and use edge equations instead

	// grab the whole original triangle to we don't overwrite it
	TriGeometryData_t srcTri = m_Data.m_GeometryData;

	m_Data.m_IntersectData.m_nFlags = srcTri.m_nFlags;
	m_Data.m_IntersectData.m_nTriangleID = srcTri.m_nTriangleID;

	Vector p1 = srcTri.Vertex( 0 );
	Vector p2 = srcTri.Vertex( 1 );
	Vector p3 = srcTri.Vertex( 2 );

	Vector e1 = p2 - p1;
	Vector e2 = p3 - p1;

	Vector N = e1.Cross( e2 );
	N.NormalizeInPlace();
	// now, determine which axis to drop
	int drop_axis = 0;
	for(int c=1 ; c<3 ; c++)
		if ( fabs(N[c]) > fabs( N[drop_axis] ) )
			drop_axis = c;

	m_Data.m_IntersectData.m_flD = N.Dot( p1 );
	m_Data.m_IntersectData.m_flNx = N.x;
	m_Data.m_IntersectData.m_flNy = N.y;
	m_Data.m_IntersectData.m_flNz = N.z;

	// decide which axes to keep
	int nCoordSelect0 = ( drop_axis + 1 ) % 3;
	int nCoordSelect1 = ( drop_axis + 2 ) % 3;

	m_Data.m_IntersectData.m_nCoordSelect0 = nCoordSelect0;
	m_Data.m_IntersectData.m_nCoordSelect1 = nCoordSelect1;


	Vector edge1 = GetEdgeEquation( p1, p2, nCoordSelect0, nCoordSelect1, p3 );
	m_Data.m_IntersectData.m_ProjectedEdgeEquations[0] = edge1.x;
	m_Data.m_IntersectData.m_ProjectedEdgeEquations[1] = edge1.y;
	m_Data.m_IntersectData.m_ProjectedEdgeEquations[2] = edge1.z;

	Vector edge2 = GetEdgeEquation( p2, p3, nCoordSelect0, nCoordSelect1, p1 );
	m_Data.m_IntersectData.m_ProjectedEdgeEquations[3] = edge2.x;
	m_Data.m_IntersectData.m_ProjectedEdgeEquations[4] = edge2.y;
	m_Data.m_IntersectData.m_ProjectedEdgeEquations[5] = edge2.z;
}


int n_intersection_calculations=0;

int CacheOptimizedTriangle::ClassifyAgainstAxisSplit(int split_plane, float split_value)
{
	// classify a triangle against an axis-aligned plane
	float minc = Vertex( 0 )[split_plane];
	float maxc = minc;
	for( int v = 1; v < 3; v++ )
	{
		minc=MIN(minc,Vertex(v)[split_plane]);
		maxc=MAX(maxc,Vertex(v)[split_plane]);
	}

	if ( minc >= split_value )
		return PLANECHECK_POSITIVE;
	if ( maxc <= split_value )
		return PLANECHECK_NEGATIVE;
	if ( minc == maxc )
		return PLANECHECK_POSITIVE;
	return PLANECHECK_STRADDLING;
}

static Vector ProjectOntoPlaneFromAxis( const Vector &v, const Vector &vNormal, float flDist, int nAxis )
{
	float flAxisDist = DotProduct( v, vNormal ) - flDist;
	flAxisDist /= vNormal[nAxis];
	Vector vOut = v;
	vOut[nAxis] -= flAxisDist;

	return vOut;
}

void CacheOptimizedTriangle::ExtractVerticesFromIntersectionFormat( Vector &v0, Vector &v1, Vector &v2 ) const
{
	const TriIntersectData_t &data = m_Data.m_IntersectData;
	const float *pPlane0 = data.m_ProjectedEdgeEquations;
	const float *pPlane1 = data.m_ProjectedEdgeEquations + 3;

	Vector vNormal;
	vNormal.Init( data.m_flNx, data.m_flNy, data.m_flNz );
	int n0 = data.m_nCoordSelect0;
	int n1 = data.m_nCoordSelect1;
	int n2 = (n1+1)%3;
	float flOffset = -1.0f;
	float flOOscale = 1.0f / (pPlane0[0] * pPlane1[1] - pPlane1[0] * pPlane0[1]);
	// vert0 is at the intersection of the two edges with edge1's plane offset by 1
	v0[n0] = (pPlane0[1] * (pPlane1[2]+flOffset) - pPlane1[1] * pPlane0[2]) * flOOscale;
	v0[n1] = (pPlane1[0] * pPlane0[2] - pPlane0[0] * (pPlane1[2]+flOffset)) * flOOscale;
	v0[n2] = 0;

	// vert1 is at the intersection of the two edges with neither offset
	v1[n0] = (pPlane0[1] * pPlane1[2] - pPlane1[1] * pPlane0[2]) * flOOscale;
	v1[n1] = (pPlane1[0] * pPlane0[2] - pPlane0[0] * pPlane1[2]) * flOOscale;
	v1[n2] = 0;

	// vert2 is at the intersection of the two edges with edge0's plane offset by 1
	v2[n0] = (pPlane0[1] * pPlane1[2] - pPlane1[1] * (pPlane0[2]+flOffset)) * flOOscale;
	v2[n1] = (pPlane1[0] * (pPlane0[2]+flOffset) - pPlane0[0] * pPlane1[2]) * flOOscale;
	v2[n2] = 0;
	v0 = ProjectOntoPlaneFromAxis( v0, vNormal, data.m_flD, n2 );
	v1 = ProjectOntoPlaneFromAxis( v1, vNormal, data.m_flD, n2 );
	v2 = ProjectOntoPlaneFromAxis( v2, vNormal, data.m_flD, n2 );
}


#define MAILBOX_HASH_SIZE 256
#define MAX_TREE_DEPTH 21
#define MAX_NODE_STACK_LEN (40*MAX_TREE_DEPTH)

struct NodeToVisit {
	CacheOptimizedKDNode const *node;
	fltx4 TMin;
	fltx4 TMax;
};


static fltx4 FourEpsilons={1.0e-10,1.0e-10,1.0e-10,1.0e-10};
static fltx4 FourZeros={1.0e-10,1.0e-10,1.0e-10,1.0e-10};
static fltx4 FourNegativeEpsilons={-1.0e-10,-1.0e-10,-1.0e-10,-1.0e-10};

void RayTracingEnvironment::Trace4Rays( const FourRays &rays, fltx4 TMin, fltx4 TMax,
										RayTracingResult *rslt_out,
										int32 skip_id, ITransparentTriangleCallback *pCallback,
										RTECullMode_t cullMode )
{
	int msk = rays.CalculateDirectionSignMask();
	if ( msk !=- 1 )
		Trace4Rays( rays, TMin, TMax, msk, rslt_out, skip_id, pCallback, cullMode );
	else
	{
		// sucky case - can't trace 4 rays at once. in the worst case, need to trace all 4
		// separately, but usually we will still get 2x, Since our tracer only does 4 at a
		// time, we will have to cover up the undesired rays with the desired ray

		//!! speed!! there is room for some sse-ization here
		FourRays tmprays;
		tmprays.origin = rays.origin;

		uint8 need_trace[4] = { 1, 1, 1, 1 };
		for( int try_trace = 0; try_trace < 4; try_trace++ )
		{
			if ( need_trace[try_trace] )
			{
				need_trace[try_trace] = 2;			// going to trace it
				// replicate the ray being traced into all 4 rays
				tmprays.direction.x = ReplicateX4( rays.direction.X( try_trace ));
				tmprays.direction.y = ReplicateX4( rays.direction.Y( try_trace ));
				tmprays.direction.z = ReplicateX4( rays.direction.Z( try_trace ));
				// now, see if any of the other remaining rays can be handled at the same time.
				for( int try2 = try_trace + 1; try2 < 4; try2++ )
					if ( need_trace[try2] )
					{
						if (
							SameSign( rays.direction.X( try2 ),
									  rays.direction.X( try_trace )) &&
							SameSign( rays.direction.Y( try2 ),
									  rays.direction.Y( try_trace )) &&
							SameSign( rays.direction.Z( try2 ),
									  rays.direction.Z( try_trace )) )
						{
							need_trace[try2] = 2;
							tmprays.direction.X( try2 ) = rays.direction.X( try2 );
							tmprays.direction.Y( try2 ) = rays.direction.Y( try2 );
							tmprays.direction.Z( try2 ) = rays.direction.Z( try2 );
						}
					}
				// ok, now trace between 1 and 3 rays, and output the results
				RayTracingResult tmpresults;
				msk = tmprays.CalculateDirectionSignMask();
				assert( msk !=- 1 );
				Trace4Rays( tmprays, TMin, TMax, msk, &tmpresults, skip_id, pCallback, cullMode );
				// now, move results to proper place
				for( int i = 0; i < 4; i++ )
					if ( need_trace[i] == 2 )
					{
						need_trace[i] = 0;
						rslt_out->HitIds[i] = tmpresults.HitIds[i];
						SubFloat( rslt_out->HitDistance, i ) = SubFloat( tmpresults.HitDistance, i );
						rslt_out->surface_normal.X( i ) = tmpresults.surface_normal.X( i );
						rslt_out->surface_normal.Y( i ) = tmpresults.surface_normal.Y( i );
						rslt_out->surface_normal.Z( i ) = tmpresults.surface_normal.Z( i );
					}

			}
		}
	}
}

template< RTECullMode_t cullMode >
bi32x4 DidHit( fltx4 DDotN, bi32x4 epsilon_hit )
{
	if ( cullMode == RTE_CULL_FRONT )
	{
		bi32x4 did_hit_back = CmpGtSIMD( DDotN, Four_Zeros );
		return AndSIMD( epsilon_hit, did_hit_back );
	}
	else if ( cullMode == RTE_CULL_BACK )
	{
		bi32x4 did_hit_front = CmpLtSIMD( DDotN, Four_Zeros );
		return AndSIMD( epsilon_hit, did_hit_front );
	}
	else
	{
		return epsilon_hit;
	}
}

// wrapper for the low level trace4 rays routine
void RayTracingEnvironment::Trace4Rays( const FourRays &rays, fltx4 TMin, fltx4 TMax,int DirectionSignMask,
										RayTracingResult *rslt_out,
										int32 skip_id, ITransparentTriangleCallback *pCallback, RTECullMode_t cullMode )
{
	switch ( cullMode )
	{
	case RTE_CULL_FRONT:
		Trace4Rays<RTE_CULL_FRONT>( rays, TMin, TMax, DirectionSignMask, rslt_out, skip_id, pCallback );
		break;
	case RTE_CULL_BACK:
		Trace4Rays<RTE_CULL_BACK>( rays, TMin, TMax, DirectionSignMask, rslt_out, skip_id, pCallback );
		break;
	default:
		Trace4Rays<RTE_CULL_NONE>( rays, TMin, TMax, DirectionSignMask, rslt_out, skip_id, pCallback );
		break;
	}
}

template <RTECullMode_t cullMode>
void RayTracingEnvironment::Trace4Rays(const FourRays &rays, fltx4 TMin, fltx4 TMax,
									   int DirectionSignMask, RayTracingResult *rslt_out,
									   int32 skip_id, ITransparentTriangleCallback *pCallback )
{
	rays.Check();

	memset( rslt_out->HitIds, 0xff, sizeof( rslt_out->HitIds ) );

	rslt_out->HitDistance = ReplicateX4( 1.0e23 );

	rslt_out->surface_normal.DuplicateVector( Vector( 0., 0., 0. ) );
	FourVectors OneOverRayDir = rays.direction;
	OneOverRayDir.MakeReciprocalSaturate();
	
	// now, clip rays against bounding box
	for( int c = 0; c < 3; c++ )
	{
		fltx4 isect_min_t =
			MulSIMD( SubSIMD( ReplicateX4( m_MinBound[c] ), rays.origin[c] ), OneOverRayDir[c] );
		fltx4 isect_max_t =
			MulSIMD( SubSIMD( ReplicateX4( m_MaxBound[c] ), rays.origin[c] ), OneOverRayDir[c] );
		TMin = MaxSIMD( TMin, MinSIMD( isect_min_t, isect_max_t ));
		TMax = MinSIMD( TMax, MaxSIMD( isect_min_t, isect_max_t ));
	}
	bi32x4 active = CmpLeSIMD( TMin, TMax );					// mask of which rays are active
	if ( ! IsAnyTrue( active ) )
		return;												// missed bounding box

	int32 mailboxids[MAILBOX_HASH_SIZE];					// used to avoid redundant triangle tests
	memset( mailboxids, 0xff, sizeof( mailboxids ));				// !!speed!! keep around?

	int front_idx[3], back_idx[3];							// based on ray direction, whether to
															// visit left or right node first

	if ( DirectionSignMask & 1 )
	{
		back_idx[0] = 0;
		front_idx[0] = 1;
	}
	else
	{
		back_idx[0] = 1;
		front_idx[0] = 0;
	}
	if ( DirectionSignMask & 2 )
	{
		back_idx[1] = 0;
		front_idx[1] = 1;
	}
	else
	{
		back_idx[1] = 1;
		front_idx[1] = 0;
	}
	if ( DirectionSignMask & 4 )
	{
		back_idx[2] = 0;
		front_idx[2] = 1;
	}
	else
	{
		back_idx[2] = 1;
		front_idx[2] = 0;
	}

	NodeToVisit NodeQueue[MAX_NODE_STACK_LEN];
	CacheOptimizedKDNode const *CurNode =& ( OptimizedKDTree[0] );
	NodeToVisit *stack_ptr = &NodeQueue[MAX_NODE_STACK_LEN];
	while( 1 )
	{
		while ( CurNode->NodeType() != KDNODE_STATE_LEAF )		// traverse until next leaf
		{
			int split_plane_number = CurNode->NodeType();
			CacheOptimizedKDNode const *FrontChild = &( OptimizedKDTree[CurNode->LeftChild()] );
			
			fltx4 dist_to_sep_plane =						// dist=(split-org)/dir
				MulSIMD(
					SubSIMD( ReplicateX4( CurNode->SplittingPlaneValue ),
							 rays.origin[split_plane_number] ), OneOverRayDir[split_plane_number] );

			bi32x4 active = CmpLeSIMD( TMin, TMax );			// mask of which rays are active

			// now, decide how to traverse children. can either do front,back, or do front and push
			// back.
			bi32x4 hits_front = AndSIMD( active, CmpGeSIMD( dist_to_sep_plane, TMin ));
			if ( ! IsAnyTrue( hits_front ))
			{
				// missed the front. only traverse back
				//printf("only visit back %d\n",CurNode->LeftChild()+back_idx[split_plane_number]);
				CurNode = FrontChild + back_idx[split_plane_number];
				TMin = MaxSIMD( TMin, dist_to_sep_plane );

			}
			else
			{
				bi32x4 hits_back = AndSIMD( active, CmpLeSIMD( dist_to_sep_plane, TMax ));
				if ( ! IsAnyTrue( hits_back ) )
				{
					// missed the back - only need to traverse front node
					//printf("only visit front %d\n",CurNode->LeftChild()+front_idx[split_plane_number]);
					CurNode = FrontChild + front_idx[split_plane_number];
					TMax = MinSIMD( TMax, dist_to_sep_plane );
				}
				else
				{
					// at least some rays hit both nodes.
					// must push far, traverse near
 					//printf("visit %d,%d\n",CurNode->LeftChild()+front_idx[split_plane_number],
 					//	   CurNode->LeftChild()+back_idx[split_plane_number]);
					assert( stack_ptr > NodeQueue );
					-- stack_ptr;
					stack_ptr->node = FrontChild + back_idx[split_plane_number];
					stack_ptr->TMin = MaxSIMD( TMin, dist_to_sep_plane );
					stack_ptr->TMax = TMax;
					CurNode = FrontChild + front_idx[split_plane_number];
					TMax = MinSIMD( TMax, dist_to_sep_plane );
				}
			}
		}
		// hit a leaf! must do intersection check
		int ntris = CurNode->NumberOfTrianglesInLeaf();
		if ( ntris )
		{
			int32 const * tlist =& ( TriangleIndexList[CurNode->TriangleIndexStart()] );
			do
			{
				int tnum =* ( tlist++ );
				//printf("try tri %d\n",tnum);
				// check mailbox
				int mbox_slot = tnum & ( MAILBOX_HASH_SIZE - 1 );
				TriIntersectData_t const * tri = &( OptimizedTriangleList[tnum].m_Data.m_IntersectData );
				if ( ( mailboxids[mbox_slot] != tnum ) && ( tri->m_nTriangleID != skip_id ) )
				{
					n_intersection_calculations++;
					mailboxids[mbox_slot] = tnum;
					// compute plane intersection


					FourVectors N;
					N.x = ReplicateX4( tri->m_flNx );
					N.y = ReplicateX4( tri->m_flNy );
					N.z = ReplicateX4( tri->m_flNz );

					fltx4 DDotN = rays.direction * N;

					
					bi32x4 did_hit = OrSIMD( CmpGtSIMD( DDotN, FourEpsilons ),
											CmpLtSIMD( DDotN, FourNegativeEpsilons ) );
					
					did_hit = DidHit<cullMode>( DDotN, did_hit );

					fltx4 numerator = SubSIMD( ReplicateX4( tri->m_flD ), rays.origin * N );

					fltx4 isect_t = DivSIMD( numerator, DDotN );
					// now, we have the distance to the plane. lets update our mask
					did_hit = AndSIMD( did_hit, CmpGtSIMD( isect_t, FourZeros ) );
					//did_hit=AndSIMD(did_hit,CmpLtSIMD(isect_t,TMax));
					did_hit = AndSIMD( did_hit, CmpLtSIMD( isect_t, rslt_out->HitDistance ) );

					if ( ! IsAnyTrue( did_hit ) )
						continue;

					// now, check 3 edges
					fltx4 hitc1 = AddSIMD( rays.origin[tri->m_nCoordSelect0],
										   MulSIMD( isect_t, rays.direction[ tri->m_nCoordSelect0] ) );
					fltx4 hitc2 = AddSIMD( rays.origin[tri->m_nCoordSelect1],
										   MulSIMD( isect_t, rays.direction[tri->m_nCoordSelect1] ) );
					
					// do barycentric coordinate check
					fltx4 B0 = MulSIMD( ReplicateX4( tri->m_ProjectedEdgeEquations[0] ), hitc1 );

					B0 = AddSIMD(
						B0,
						MulSIMD( ReplicateX4( tri->m_ProjectedEdgeEquations[1] ), hitc2 ) );
					B0 = AddSIMD(
						B0, ReplicateX4( tri->m_ProjectedEdgeEquations[2] ) );

					did_hit = AndSIMD( did_hit, CmpGeSIMD( B0, FourZeros ) );

					fltx4 B1 = MulSIMD( ReplicateX4( tri->m_ProjectedEdgeEquations[3] ), hitc1 );
					B1 = AddSIMD(
						B1,
						MulSIMD( ReplicateX4( tri->m_ProjectedEdgeEquations[4]), hitc2 ) );

					B1 = AddSIMD(
						B1, ReplicateX4( tri->m_ProjectedEdgeEquations[5] ) );
					
					did_hit = AndSIMD( did_hit, CmpGeSIMD( B1, FourZeros ) );

					fltx4 B2 = AddSIMD( B1, B0 );
					did_hit = AndSIMD( did_hit, CmpLeSIMD( B2, Four_Ones ) );

					if ( ! IsAnyTrue( did_hit ) )
						continue;

					// if the triangle is transparent
					if ( tri->m_nFlags & FCACHETRI_TRANSPARENT )
					{
						if ( pCallback )
						{
							// assuming a triangle indexed as v0, v1, v2
							// the projected edge equations are set up such that the vert opposite the first
							// equation is v2, and the vert opposite the second equation is v0
							// Therefore we pass them back in 1, 2, 0 order
							// Also B2 is currently B1 + B0 and needs to be 1 - (B1+B0) in order to be a real
							// barycentric coordinate.  Compute that now and pass it to the callback
							fltx4 b2 = SubSIMD( Four_Ones, B2 );
							if ( pCallback->VisitTriangle_ShouldContinue( *tri, rays, &did_hit, &B1, &b2, &B0, tnum ) )
							{
								did_hit = (bi32x4)Four_Zeros;
							}
						}
					}
					// now, set the hit_id and closest_hit fields for any enabled rays
					i32x4 replicated_n = ReplicateIX4(tnum);
					StoreAlignedSIMD( (float * ) rslt_out->HitIds,
									  OrSIMD( AndSIMD( (bi32x4)replicated_n, did_hit ),
											  AndNotSIMD( did_hit, LoadAlignedSIMD(
															  ( float * ) rslt_out->HitIds )) ));
					rslt_out->HitDistance = OrSIMD( AndSIMD( isect_t, did_hit ),
													AndNotSIMD( did_hit, rslt_out->HitDistance ));

					rslt_out->surface_normal.x = OrSIMD(
						AndSIMD( N.x, did_hit ),
						AndNotSIMD( did_hit, rslt_out->surface_normal.x ));
					rslt_out->surface_normal.y = OrSIMD(
						AndSIMD( N.y, did_hit ),
						AndNotSIMD( did_hit, rslt_out->surface_normal.y ));
					rslt_out->surface_normal.z = OrSIMD(
						AndSIMD( N.z, did_hit ),
						AndNotSIMD( did_hit, rslt_out->surface_normal.z ));
					
				}
			} while (--ntris);
			// now, check if all rays have terminated
			bi32x4 raydone = CmpLeSIMD( TMax, rslt_out->HitDistance );
			if (! IsAnyTrue(raydone))
			{
				return;
			}
		}
		
 		if ( stack_ptr == &NodeQueue[MAX_NODE_STACK_LEN] )
		{
			return;
		}
		// pop stack!
		CurNode = stack_ptr->node;
		TMin = stack_ptr->TMin;
		TMax = stack_ptr->TMax;
		stack_ptr++;
	}
}


int RayTracingEnvironment::MakeLeafNode(int first_tri, int last_tri)
{
	CacheOptimizedKDNode ret;
	ret.Children = KDNODE_STATE_LEAF + ( TriangleIndexList.Count() << 2 );
	ret.SetNumberOfTrianglesInLeafNode( 1 + ( last_tri - first_tri ));
	for( int tnum = first_tri; tnum <= last_tri; tnum++ )
		TriangleIndexList.AddToTail( tnum );
	OptimizedKDTree.AddToTail( ret );
	return OptimizedKDTree.Count() - 1;
}


void RayTracingEnvironment::CalculateTriangleListBounds(int32 const *tris,int ntris,
														Vector &minout, Vector &maxout)
{
	minout = Vector( 1.0e23, 1.0e23, 1.0e23 );
	maxout = Vector( - 1.0e23, - 1.0e23, - 1.0e23 );
	for( int i = 0; i < ntris; i++ )
	{
		CacheOptimizedTriangle const &tri = OptimizedTriangleList[tris[i]];
		for( int v = 0; v < 3; v++ )
			for( int c = 0; c < 3; c++ )
			{
				minout[c]=MIN(minout[c],tri.Vertex(v)[c]);
							  maxout[c]=MAX(maxout[c],tri.Vertex(v)[c]);
			}
	}
}


// Both the "quick" and regular kd tree building algorithms here use the "surface area heuristic":
// the relative probability of hitting the "left" subvolume (Vl) from a split is equal to that
// subvolume's surface area divided by its parent's surface area (Vp) : P(Vl | V) = SA(Vl)/SA(Vp).
// The same holds for the right subvolume, Vp. Nl is the number of triangles in the left volume,
// and Nr in the right volume. if Ct is the cost of traversing one tree node, and Ci is the cost of
// intersection with the primitive, than the cost of splitting is estimated as:
//
//    Ct+Ci*((SA(Vl)/SA(V))*Nl+(SA(Vr)/SA(V)*Nr)).
// and the cost of not splitting is
//    Ci*N
//
//  This both provides a metric to minimize when computing how and where to split, and also a
//  termination criterion.
//
// the "quick" method just splits down the middle, while the slow method splits at the best
// discontinuity of the cost formula. The quick method splits along the longest axis ; the 
// regular algorithm tries all 3 to find which one results in the minimum cost
// 
// both methods use the additional optimization of "growing" empty nodes - if the split results in
// one side being devoid of triangles, the empty side is "grown" as much as possible.
//

#define COST_OF_TRAVERSAL 75								// approximate #operations
#define COST_OF_INTERSECTION 167							// approximate #operations


float RayTracingEnvironment::CalculateCostsOfSplit(
	int split_plane,int32 const *tri_list,int ntris,
	Vector MinBound,Vector MaxBound, float &split_value,
	int &nleft, int &nright, int &nboth)
{
	// determine the costs of splitting on a given axis, and label triangles with respect to
	// that axis by storing the value in coordselect0. It will also return the number of
	// tris in the left, right, and nboth groups, in order to facilitate memory
	nleft = nboth = nright = 0;

	// now, label each triangle. Since we have not converted the triangles into
	// intersection fromat yet, we can use the CoordSelect0 field of each as a temp.
	nleft = 0;
	nright = 0;
	nboth = 0;
	float min_coord = 1.0e23, max_coord =- 1.0e23;

	for( int t = 0; t < ntris; t++ )
	{
		CacheOptimizedTriangle &tri = OptimizedTriangleList[tri_list[t]];
		// determine max and min coordinate values for later optimization
		for( int v = 0; v < 3; v++ )
		{
			min_coord = MIN( min_coord, tri.Vertex(v)[split_plane] );
			max_coord = MAX( max_coord, tri.Vertex(v)[split_plane] );
		}
		switch( tri.ClassifyAgainstAxisSplit( split_plane, split_value ))
		{
			case PLANECHECK_NEGATIVE:
				nleft++;
				tri.m_Data.m_GeometryData.m_nTmpData0 = PLANECHECK_NEGATIVE;
				break;

			case PLANECHECK_POSITIVE:
				nright++;
				tri.m_Data.m_GeometryData.m_nTmpData0 = PLANECHECK_POSITIVE;
				break;

			case PLANECHECK_STRADDLING:
				nboth++;
				tri.m_Data.m_GeometryData.m_nTmpData0 = PLANECHECK_STRADDLING;
				break;
		}
	}
	// now, if the split resulted in one half being empty, "grow" the empty half
	if ( nleft && ( nboth == 0 ) && ( nright == 0 ))
		split_value = max_coord;
	if ( nright && ( nboth == 0 ) && ( nleft == 0 ))
		split_value = min_coord;

	// now, perform surface area/cost check to determine whether this split was worth it
	Vector LeftMins = MinBound;
	Vector LeftMaxes = MaxBound;
	Vector RightMins = MinBound;
	Vector RightMaxes = MaxBound;
	LeftMaxes[split_plane] = split_value;
	RightMins[split_plane] = split_value;
	float SA_L = BoxSurfaceArea( LeftMins, LeftMaxes );
	float SA_R = BoxSurfaceArea( RightMins, RightMaxes );
	float ISA = 1.0 / BoxSurfaceArea( MinBound, MaxBound );
	float cost_of_split = COST_OF_TRAVERSAL + COST_OF_INTERSECTION * ( nboth +
																	   ( SA_L * ISA * ( nleft )) + ( SA_R * ISA * ( nright )) );
	return cost_of_split;
}


#define NEVER_SPLIT 0

void RayTracingEnvironment::RefineNode( int node_number, int32 const * tri_list, int ntris,
										Vector MinBound, Vector MaxBound, int depth )
{
	if ( ntris < 3 )											// never split empty lists
	{
		// no point in continuing
		OptimizedKDTree[node_number].Children = KDNODE_STATE_LEAF + ( TriangleIndexList.Count() << 2 );
		OptimizedKDTree[node_number].SetNumberOfTrianglesInLeafNode( ntris );

#ifdef DEBUG_RAYTRACE
		OptimizedKDTree[node_number].vecMins = MinBound;
		OptimizedKDTree[node_number].vecMaxs = MaxBound;
#endif

		for( int t = 0; t < ntris; t++ )
			TriangleIndexList.AddToTail( tri_list[t] );
		return;
	}

	float best_cost = 1.0e23;
	int best_nleft = 0, best_nright = 0, best_nboth = 0;
	float best_splitvalue = 0;
	int split_plane = 0;

	int tri_skip = 1 + ( ntris / 10 );								// don't try all trinagles as split
	// points when there are a lot of them
	for( int axis = 0; axis < 3; axis++ )
	{
		for( int ts =- 1; ts < ntris; ts += tri_skip )
		{
			for( int tv = 0; tv < 3; tv++ )
			{
				int trial_nleft, trial_nright, trial_nboth;
				float trial_splitvalue;
				if ( ts ==- 1 )
					trial_splitvalue = 0.5 * ( MinBound[axis]+ MaxBound[axis] );
				else
				{
					// else, split at the triangle vertex if possible
					CacheOptimizedTriangle &tri = OptimizedTriangleList[tri_list[ts]];
					trial_splitvalue = tri.Vertex( tv )[axis];
					if ( (trial_splitvalue > MaxBound[axis] ) || ( trial_splitvalue < MinBound[axis] ))
						continue;							// don't try this vertex - not inside

				}
//				printf("ts=%d tv=%d tp=%f\n",ts,tv,trial_splitvalue);
				float trial_cost =
					CalculateCostsOfSplit( axis, tri_list, ntris, MinBound, MaxBound, trial_splitvalue,
										   trial_nleft, trial_nright, trial_nboth );
// 				printf("try %d cost=%f nl=%d nr=%d nb=%d sp=%f\n",axis,trial_cost,trial_nleft,trial_nright, trial_nboth,
// 					   trial_splitvalue);
				if ( trial_cost < best_cost )
				{
					split_plane = axis;
					best_cost = trial_cost;
					best_nleft = trial_nleft;
					best_nright = trial_nright;
					best_nboth = trial_nboth;
					best_splitvalue = trial_splitvalue;
					// save away the axis classification of each triangle
					for( int t = 0 ; t < ntris; t++ )
					{
						CacheOptimizedTriangle &tri = OptimizedTriangleList[tri_list[t]];
						tri.m_Data.m_GeometryData.m_nTmpData1 = tri.m_Data.m_GeometryData.m_nTmpData0;
					}
				}
				if ( ts ==- 1 )
					break;
			}
		}

	}
	float cost_of_no_split = COST_OF_INTERSECTION * ntris;
	if ( ( cost_of_no_split <= best_cost ) || NEVER_SPLIT || ( depth > MAX_TREE_DEPTH ))
	{
		// no benefit to splitting. just make this a leaf node
		OptimizedKDTree[node_number].Children = KDNODE_STATE_LEAF + ( TriangleIndexList.Count() << 2 );
		OptimizedKDTree[node_number].SetNumberOfTrianglesInLeafNode( ntris );
#ifdef DEBUG_RAYTRACE
		OptimizedKDTree[node_number].vecMins = MinBound;
		OptimizedKDTree[node_number].vecMaxs = MaxBound;
#endif
		for( int t = 0; t < ntris; t++ )
			TriangleIndexList.AddToTail( tri_list[t] );
	}
	else
	{
// 		printf("best split was %d at %f (mid=%f,n=%d, sk=%d)\n",split_plane,best_splitvalue,
// 			   0.5*(MinBound[split_plane]+MaxBound[split_plane]),ntris,tri_skip);
		// its worth splitting!
		// we will achieve the splitting without sorting by using a selection algorithm.
		int32 * new_triangle_list;
		new_triangle_list = new int32[ntris];

		// now, perform surface area/cost check to determine whether this split was worth it
		Vector LeftMins = MinBound;
		Vector LeftMaxes = MaxBound;
		Vector RightMins = MinBound;
		Vector RightMaxes = MaxBound;
		LeftMaxes[split_plane] = best_splitvalue;
		RightMins[split_plane] = best_splitvalue;

		int n_left_output = 0;
		int n_both_output = 0;
		int n_right_output = 0;
		for( int t = 0; t < ntris; t++ )
		{
			CacheOptimizedTriangle &tri = OptimizedTriangleList[tri_list[t]];
			switch( tri.m_Data.m_GeometryData.m_nTmpData1 )
			{
				case PLANECHECK_NEGATIVE:
//					printf("%d goes left\n",t);
					new_triangle_list[n_left_output++] = tri_list[t];
					break;
				case PLANECHECK_POSITIVE:
					n_right_output++;
//					printf("%d goes right\n",t);
					new_triangle_list[ntris - n_right_output] = tri_list[t];
					break;
				case PLANECHECK_STRADDLING:
//					printf("%d goes both\n",t);
					new_triangle_list[best_nleft + n_both_output] = tri_list[t];
					n_both_output++;
					break;


			}
		}
		int left_child = OptimizedKDTree.Count();
		int right_child = left_child + 1;
// 		printf("node %d split on axis %d at %f, nl=%d nr=%d nb=%d lc=%d rc=%d\n",node_number,
// 			   split_plane,best_splitvalue,best_nleft,best_nright,best_nboth,
// 			   left_child,right_child);
		OptimizedKDTree[node_number].Children = split_plane + ( left_child << 2 );
		OptimizedKDTree[node_number].SplittingPlaneValue = best_splitvalue;
#ifdef DEBUG_RAYTRACE
		OptimizedKDTree[node_number].vecMins = MinBound;
		OptimizedKDTree[node_number].vecMaxs = MaxBound;
#endif
		CacheOptimizedKDNode newnode;
		OptimizedKDTree.AddToTail( newnode );
		OptimizedKDTree.AddToTail( newnode );
		// now, recurse!
		if ( ( ntris < 20 ) && ( (best_nleft == 0 ) || ( best_nright == 0 )) )
			depth += 100;
		RefineNode( left_child, new_triangle_list, best_nleft + best_nboth, LeftMins, LeftMaxes, depth + 1 );
		RefineNode( right_child, new_triangle_list + best_nleft, best_nright + best_nboth,
					RightMins, RightMaxes, depth + 1 );
		delete[] new_triangle_list;
	}
}


void RayTracingEnvironment::SetupAccelerationStructure( void )
{
	CacheOptimizedKDNode root;
	OptimizedKDTree.AddToTail( root );
	int32 * root_triangle_list = new int32[OptimizedTriangleList.Count()];
	for( int t = 0; t < OptimizedTriangleList.Count(); t++ )
		root_triangle_list[t] = t;
	CalculateTriangleListBounds( root_triangle_list, OptimizedTriangleList.Count(), m_MinBound,
								 m_MaxBound );
	RefineNode( 0, root_triangle_list, OptimizedTriangleList.Count(), m_MinBound, m_MaxBound, 0 );
	delete[] root_triangle_list;

	// now, convert all triangles to "intersection format"
	for( int i = 0; i < OptimizedTriangleList.Count(); i++ )
		OptimizedTriangleList[i].ChangeIntoIntersectionFormat();
}



void RayTracingEnvironment::AddInfinitePointLight( Vector position, Vector intensity )
{
	LightDesc_t mylight( position, intensity );
	LightList.AddToTail( mylight );

}


#define RTENV_SERIALIZATION_VERSION 1

struct RayTracingSerializationHeader
{
	uint32 m_nVersionNumber;
	uint32 m_nSerializationFlags;
	uint32 m_nNumKDNodes;
	uint32 m_nNumTriangles;
	uint32 m_nNumTriangleIndices;
	uint32 m_nNumColors;
	Vector m_vMinBound;
	Vector m_vMaxBound;

	RayTracingSerializationHeader( void )
	{
		m_nVersionNumber = RTENV_SERIALIZATION_VERSION;
	}

	void Put( CUtlBuffer &outbuf )
	{
		outbuf.PutInt( m_nVersionNumber );
		outbuf.PutInt( m_nSerializationFlags );
		outbuf.PutInt( m_nNumKDNodes );
		outbuf.PutInt( m_nNumTriangles );
		outbuf.PutInt( m_nNumTriangleIndices );
		outbuf.PutInt( m_nNumColors );
		outbuf.PutFloat( m_vMinBound.x );
		outbuf.PutFloat( m_vMinBound.y );
		outbuf.PutFloat( m_vMinBound.z );
		outbuf.PutFloat( m_vMaxBound.x );
		outbuf.PutFloat( m_vMaxBound.y );
		outbuf.PutFloat( m_vMaxBound.z );
	}

	void Get( CUtlBuffer &inbuf )
	{
		m_nVersionNumber = inbuf.GetInt();
		m_nSerializationFlags = inbuf.GetInt();
		m_nNumKDNodes = inbuf.GetInt();
		m_nNumTriangles = inbuf.GetInt();
		m_nNumTriangleIndices = inbuf.GetInt();
		m_nNumColors = inbuf.GetInt();
		m_vMinBound.x = inbuf.GetFloat();
		m_vMinBound.y = inbuf.GetFloat();
		m_vMinBound.z = inbuf.GetFloat();
		m_vMaxBound.x = inbuf.GetFloat();
		m_vMaxBound.y = inbuf.GetFloat();
		m_vMaxBound.z = inbuf.GetFloat();
	}
};

size_t RayTracingEnvironment::GetSerializationNumBytes( uint32 nSerializationFlags ) const
{
	size_t nRet = sizeof( RayTracingSerializationHeader );
	nRet += sizeof( CacheOptimizedKDNode ) * OptimizedKDTree.Count();
	nRet += sizeof( CacheOptimizedTriangle ) * OptimizedTriangleList.Count();
	nRet += sizeof( int32 ) * TriangleIndexList.Count();

	if ( nSerializationFlags & RT_ENV_SERIALIZE_COLORS )
		nRet += sizeof( Vector ) * TriangleColors.Count();
	return nRet;
}


void RayTracingEnvironment::Serialize( CUtlBuffer &outbuf, uint32 nSerializationFlags ) const
{
	outbuf.ActivateByteSwappingIfBigEndian();
	RayTracingSerializationHeader hdr;
	hdr.m_nSerializationFlags = nSerializationFlags;
	hdr.m_nNumKDNodes = OptimizedKDTree.Count();
	hdr.m_nNumTriangles = OptimizedTriangleList.Count();
	hdr.m_nNumTriangleIndices = TriangleIndexList.Count();
	hdr.m_nNumColors = ( nSerializationFlags & RT_ENV_SERIALIZE_COLORS ) ? TriangleColors.Count() : 0;
	hdr.m_vMinBound = m_MinBound;
	hdr.m_vMaxBound = m_MaxBound;
	hdr.Put( outbuf );
	for( int i = 0 ; i < OptimizedKDTree.Count(); i++ )
	{
		CacheOptimizedKDNode const * pNode = &OptimizedKDTree[i];
		outbuf.PutInt( pNode->Children );
		if ( pNode->NodeType() == KDNODE_STATE_LEAF )
			outbuf.PutInt( * ( reinterpret_cast < int32 const *> ( &pNode->SplittingPlaneValue ) ) );
		else
			outbuf.PutFloat( pNode->SplittingPlaneValue );
	}
	for( int i = 0; i < OptimizedTriangleList.Count() ; i++ )
	{
		TriIntersectData_t const * pTri = &( OptimizedTriangleList[i].m_Data.m_IntersectData );
		outbuf.PutFloat( pTri->m_flNx );
		outbuf.PutFloat( pTri->m_flNy );
		outbuf.PutFloat( pTri->m_flNz );
		outbuf.PutFloat( pTri->m_flD );
		outbuf.PutFloat( pTri->m_nTriangleID );
		for( int j = 0; j < ARRAYSIZE( pTri->m_ProjectedEdgeEquations ); j++ )
			outbuf.PutFloat( pTri->m_ProjectedEdgeEquations[j] );
		outbuf.PutUnsignedChar( pTri->m_nCoordSelect0 );
		outbuf.PutUnsignedChar( pTri->m_nCoordSelect1 );
		outbuf.PutUnsignedChar( pTri->m_nFlags );
		outbuf.PutUnsignedChar( 0 );						// for unused.
	}
	for( int i = 0; i < TriangleIndexList.Count(); i++ )
		outbuf.PutInt( TriangleIndexList[i] );
	if ( nSerializationFlags & RT_ENV_SERIALIZE_COLORS )
		for( int i = 0 ; i < TriangleColors.Count() ; i++ )
		{
			Vector const &v = TriangleColors[i];
			outbuf.PutFloat( v.x );
			outbuf.PutFloat( v.y );
			outbuf.PutFloat( v.z );
		}
}




void RayTracingEnvironment::UnSerialize( CUtlBuffer &inbuf )
{
	inbuf.ActivateByteSwappingIfBigEndian();
	RayTracingSerializationHeader hdr;
	hdr.Get( inbuf );
	m_MinBound = hdr.m_vMinBound;
	m_MaxBound = hdr.m_vMaxBound;
	OptimizedKDTree.SetCount( hdr.m_nNumKDNodes );
	for( int i = 0; i < hdr.m_nNumKDNodes; i++ )
	{
		CacheOptimizedKDNode *pNode = &OptimizedKDTree[i];
		pNode->Children = inbuf.GetInt();
		if ( pNode->NodeType() == KDNODE_STATE_LEAF )
		{
			*( ( int32 * ) &pNode->SplittingPlaneValue ) = inbuf.GetInt();
		}
		else
		{
			pNode->SplittingPlaneValue = inbuf.GetFloat();
		}
	}
	// now, read the triangles
	OptimizedTriangleList.SetCount( hdr.m_nNumTriangles );
	for( int i = 0; i < OptimizedTriangleList.Count() ; i++ )
	{
		TriIntersectData_t * pTri = &( OptimizedTriangleList[i].m_Data.m_IntersectData );
		pTri->m_flNx = inbuf.GetFloat();
		pTri->m_flNy = inbuf.GetFloat();
		pTri->m_flNz = inbuf.GetFloat();
		pTri->m_flD = inbuf.GetFloat();
		pTri->m_nTriangleID = inbuf.GetFloat();
		for( int j = 0; j < ARRAYSIZE( pTri->m_ProjectedEdgeEquations ); j++ )
		{
			pTri->m_ProjectedEdgeEquations[j] = inbuf.GetFloat();
		}
		pTri->m_nCoordSelect0 = inbuf.GetUnsignedChar();
		pTri->m_nCoordSelect1 = inbuf.GetUnsignedChar();
		pTri->m_nFlags = inbuf.GetUnsignedChar();
		inbuf.GetUnsignedChar();						// for unused.
	}
	TriangleIndexList.SetCount( hdr.m_nNumTriangleIndices );
	for( int i = 0; i < TriangleIndexList.Count(); i++ )
	{
		TriangleIndexList[i] = inbuf.GetInt();
	}
	TriangleColors.SetCount( hdr.m_nNumColors );
	for( int i = 0 ; i < TriangleColors.Count() ; i++ )
	{
		Vector &v = TriangleColors[i];
		v.x = inbuf.GetFloat();
		v.y = inbuf.GetFloat();
		v.z = inbuf.GetFloat();
	}
}





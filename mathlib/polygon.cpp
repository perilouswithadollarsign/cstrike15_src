//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Utility functions for polygon simplification / convex decomposition.
//
//===============================================================================

#include "polygon.h"
#include "quadric.h"

// Assumes that points are ordered clockwise when looking at the face of the polygon
void SimplifyPolygon( CUtlVector< Vector > *pPoints, const Vector &vNormal, float flMaximumDeviation )
{
	if ( pPoints->Count() < 3 )
	{
		return;
	}

	int nVertices = pPoints->Count();
	Vector vPrevious = pPoints->Element( nVertices - 1 );
	Vector vCurrent = pPoints->Element( 0 );
	Vector vNext;
	
	// Walk around the polygon removing redundant vertices
	for ( int i = 0; i < nVertices; ++ i )
	{
		vNext = pPoints->Element( ( i + 1 ) % nVertices );

		// Is the current vertex redundant?

		Vector vCurrentToNext = vNext - vCurrent;
		Vector vCurrentToPrevious = vPrevious - vCurrent;
		// Compute the area of of the error triangle added (if negative) or removed (if positive) with the removal of this vertex
		float flAreaDeviation = 0.5f * vCurrentToPrevious.Cross( vCurrentToNext ).Dot( vNormal );

		// Only consider a point for removal if it would decrease the polygon area (be conservative)
		if ( flAreaDeviation >= 0.0f && flAreaDeviation < flMaximumDeviation )
		{
			// redundant
			pPoints->Remove( i );
			-- nVertices;
			-- i;
		}
		else
		{
			vPrevious = vCurrent;
		}

		vCurrent = vNext;
	}
}

static void ComputeQuadric( const CUtlVector< Vector > &points, int nVertex, const Vector &vNormal, CQuadricError *pError )
{
	int nVertices = points.Count();
	Vector vPrevious = points[( nVertex + nVertices - 1 ) % nVertices];
	Vector vCurrent = points[nVertex];
	Vector vNext = points[( nVertex + 1 ) % nVertices];
	Vector vAbove = vCurrent + vNormal;

	pError->InitFromPlane( vNormal, -DotProduct( vNormal, vCurrent ), 1.0f );
	
	CQuadricError line1, line2;
	line1.InitFromTriangle( vPrevious, vCurrent, vAbove, 0.0f );
	*pError += line1;
	line2.InitFromTriangle( vNext, vCurrent, vAbove, 0.0f );
	*pError += line2;
}

void SimplifyPolygonQEM( CUtlVector< Vector > *pPoints, const Vector &vNormal, float flMaximumSquaredError, bool bUseOptimalPointPlacement )
{
	if ( pPoints->Count() < 3 )
	{
		return;
	}
	
	CUtlVector< CQuadricError > quadrics;
	quadrics.EnsureCapacity( pPoints->Count() );
	
	int nVertices = pPoints->Count();
	Vector vPrevious = pPoints->Element( nVertices - 1 );
	Vector vCurrent = pPoints->Element( 0 );
	Vector vNext;

	// Compute quadric error functions for each vertex
	for ( int i = 0; i < nVertices; ++ i )
	{
		vNext = pPoints->Element( ( i + 1 ) % nVertices );

		Vector vAbove = vCurrent + vNormal;
		quadrics.AddToTail();
		quadrics[i].InitFromPlane( vNormal, -DotProduct( vNormal, vCurrent ), 1.0f );
		CQuadricError line1, line2;
		line1.InitFromTriangle( vPrevious, vCurrent, vAbove, 0.0f );
		quadrics[i] += line1;
		line2.InitFromTriangle( vNext, vCurrent, vAbove, 0.0f );
		quadrics[i] += line2;

		vPrevious = vCurrent;
		vCurrent = vNext;
	}

	// @TODO: use a sorted heap instead of pseudo-bubble-sort
	// We like quadrilaterals...don't try to go simpler than that
	while ( pPoints->Count() > 4 )
	{
		float flLowestError = flMaximumSquaredError;
		Vector vBestCollapse;
		int nCollapseIndex = -1;
		for ( int i = 0; i < nVertices; ++ i )
		{
			int nNextIndex = ( i + 1 ) % nVertices;
			CQuadricError sum = quadrics[i] + quadrics[nNextIndex];

			if ( bUseOptimalPointPlacement )
			{
				// Solve for optimal point and collapse to it
				Vector vOptimalPoint = sum.SolveForMinimumError();
				float flError = sum.ComputeError( vOptimalPoint );
				if ( flError < flLowestError )
				{
					flLowestError = flError;
					vBestCollapse = vOptimalPoint;
					nCollapseIndex = i;
				}
			}
			else
			{
				// Only collapse to endpoints
				Vector vA = pPoints->Element( i );
				float flErrorA = sum.ComputeError( vA );
				if ( flErrorA < flLowestError )
				{
					flLowestError = flErrorA;
					vBestCollapse = vA;
					nCollapseIndex = i;
				}
				Vector vB = pPoints->Element( nNextIndex );
				float flErrorB = sum.ComputeError( vB );
				if ( flErrorB < flLowestError )
				{
					flLowestError = flErrorB;
					vBestCollapse = vB;
					nCollapseIndex = i;
				}
			}
		}

		if ( nCollapseIndex != -1 )
		{
			pPoints->Element( nCollapseIndex ) = vBestCollapse;
			int nNextIndex = ( nCollapseIndex + 1 ) % nVertices;
			
			pPoints->Remove( nNextIndex );
			quadrics.Remove( nNextIndex );
			-- nVertices;
			
			if ( nNextIndex < nCollapseIndex ) 
				-- nCollapseIndex;

			int nPrevIndex = ( nCollapseIndex + nVertices - 1 ) % nVertices;
			nNextIndex = ( nCollapseIndex + 1 ) % nVertices;
			
			ComputeQuadric( *pPoints, nPrevIndex, vNormal, &quadrics[nPrevIndex] );
			ComputeQuadric( *pPoints, nCollapseIndex, vNormal, &quadrics[nCollapseIndex] );
			ComputeQuadric( *pPoints, nNextIndex, vNormal, &quadrics[nNextIndex] );
		}
		else
		{
			// we're done
			break;
		}
	}
}

bool IsConcave( const Vector &v0, const Vector &v1, const Vector &v2, const Vector &vNormal )
{
	Vector vRay1 = v2 - v1;
	Vector vRay2 = v0 - v1;
	float flSign = vRay2.Cross( vRay1 ).Dot( vNormal );
	return ( flSign < 0 );
}

bool IsConcave( const Vector *pPolygonPoints, int nPointCount, int nVertex, const Vector &vNormal )
{
	Assert( nPointCount >= 3 );
	int nPrevVertex = ( nVertex + nPointCount - 1 ) % nPointCount;
	int nNextVertex = ( nVertex + 1 ) % nPointCount;

	return IsConcave( pPolygonPoints[nPrevVertex], pPolygonPoints[nVertex], pPolygonPoints[nNextVertex], vNormal );
}

bool IsConcave( const Vector *pPolygonPoints, int nPointCount, const Vector &vNormal )
{
	Assert( nPointCount >= 3 );

	Vector vPrevVertex = pPolygonPoints[nPointCount - 2];
	Vector vVertex = pPolygonPoints[nPointCount - 1];
	Vector vNextVertex;
	for ( int i = 0; i < nPointCount; ++ i )
	{
		vNextVertex = pPolygonPoints[i];
		if ( IsConcave( vPrevVertex, vVertex, vNextVertex, vNormal ) )
		{
			return true;
		}
		vPrevVertex = vVertex;
		vVertex = vNextVertex;
	}

	return false;
}

static bool IsPointInPolygon( const CUtlVector< Vector > &polygonPoints, const SubPolygon_t &convexRegion, const Vector &vNormal, const Vector &vPoint )
{
	int nIndex = convexRegion.m_Indices[convexRegion.m_Indices.Count() - 1];
	Vector v0 = SubPolygon_t::GetPoint( polygonPoints, nIndex );

	for ( int i = 0; i < convexRegion.m_Indices.Count(); ++ i )
	{
		nIndex = convexRegion.m_Indices[i];
		Vector v1 = SubPolygon_t::GetPoint( polygonPoints, nIndex );
		
		Vector vRay = v1 - v0;
		Vector vRight = vRay.Cross( vNormal );
		if ( vRight.Dot( vPoint - v0 ) < -POINT_IN_POLYGON_EPSILON )
		{
			return false;
		}

		v0 = v1;
	}

	return true;
}

static bool PointsInsideConvexArea( const CUtlVector< Vector > &polygonPoints, const SubPolygon_t &originalPolygon, const Vector &vNormal, int nFirstIndex, int nLastIndex, const SubPolygon_t &convexRegion )
{
	nFirstIndex %= originalPolygon.m_Indices.Count();
	nLastIndex %= originalPolygon.m_Indices.Count();
	if ( nFirstIndex < 0 ) nFirstIndex += originalPolygon.m_Indices.Count();
	if ( nLastIndex < 0 ) nLastIndex += originalPolygon.m_Indices.Count();

	for ( int i = nFirstIndex; i != nLastIndex; i = ( i + 1 ) % originalPolygon.m_Indices.Count() )
	{
		int nVertex = originalPolygon.GetVertexIndex( i );
		Vector vPoint = SubPolygon_t::GetPoint( polygonPoints, nVertex );
		if ( IsPointInPolygon( polygonPoints, convexRegion, vNormal, vPoint ) )
		{
			bool bIsDoubleVertex = false;
			// Allow points on the boundary of the convex region if they are coincident with one of the points of the region itself
			for ( int j = 0; j < convexRegion.m_Indices.Count(); ++ j )
			{
				Vector vConvexRegionTestPoint = SubPolygon_t::GetPoint( polygonPoints, convexRegion.GetVertexIndex( j ) );
				if ( VectorsAreEqual( vConvexRegionTestPoint, vPoint, POINT_IN_POLYGON_EPSILON ) )
				{
					bIsDoubleVertex = true;
					break;
				}
			}
			if ( !bIsDoubleVertex )
			{
				return true;
			}
		}
	}
	return false;
}


static const float LINE_INTERSECT_EPSILON = 1e-3f;

// superceded by CalcLineToLineIntersectionSegment

// bool LineSegmentsIntersect( const Vector &vNormal, const Vector &v1a, const Vector &v1b, const Vector &v2a, const Vector &v2b, float flEpsilon, float *pTimeOfIntersection1, float *pTimeOfIntersection2 )
// {
// 	Vector vDir1 = v1b - v1a;
// 	Vector vDir2 = v2b - v2a;
// 	Vector v1Perpendicular = vDir1.Cross( vNormal );
// 	Vector vStartDiff = v1a - v2a;
// 	float flNumerator = vStartDiff.Dot( v1Perpendicular );
// 	float flDenominator = vDir2.Dot( v1Perpendicular );
// 	// @TODO: we should probably use a different epsilon since the denominator is in different units, but this works well so far
// 	if ( fabsf( flDenominator ) < flEpsilon )
// 	{
// 		return false;
// 	}
// 	float t2 = flNumerator / flDenominator;
// 	if ( t2 >= -flEpsilon && t2 <= ( 1.0f + flEpsilon ) )
// 	{
// 		Vector vClosestPoint2 = t2 * vDir2 + v2a;
// 		float flLength1 = vDir1.NormalizeInPlace();
// 		// Can't be 0 otherwise flDenominator would have been 0
// 		float t1 = ( vClosestPoint2 - v1a ).Dot( vDir1 ) / flLength1;
// 
// 		if ( t1 >= -flEpsilon && t1 <= ( 1.0f + flEpsilon ) )
// 		{
// 			*pTimeOfIntersection1 = t1;
// 			*pTimeOfIntersection2 = t2;
// 			return true;
// 		}
// 	}
// 	return false;
// }

//-----------------------------------------------------------------------------
// Note: this is not a general purpose intersection test;
// it makes some assumptions that the winding of the polygon is 
// counter-clockwise (because it's expected to be a hole) and that,
// if the line segment vA-vB intersects a line segment in the hole 
// (denoted by vHoleA-vHoleB, in counter-clockwise ordering),
// then the line segment vA-vHoleB must not intersect any other part of 
// the hole polygon.
//-----------------------------------------------------------------------------
static bool LineSegmentIntersectsPolygon( const CUtlVector< Vector > &polygonPoints, const Vector &vNormal, const Vector &vA, const Vector &vB, const SubPolygon_t &hole, float *pLowestTimeOfIntersection, Vector *pA, Vector *pB, int *pHoleVertexIndex )
{
	*pLowestTimeOfIntersection = 2.0f; // a valid time is between 0 and 1, anything outside the range is considered invalid
	float flTimeOfIntersection;
	Vector vPrev = SubPolygon_t::GetPoint( polygonPoints, hole.m_Indices[hole.m_Indices.Count() - 1] );
	bool bIntersect = false;
	Vector vInside = ( vB - vA ).Cross( vNormal );
	
	for ( int i = 0; i < hole.m_Indices.Count(); ++ i )
	{
		float flOtherTimeOfIntersection;
		Vector vCurrent = SubPolygon_t::GetPoint( polygonPoints, hole.m_Indices[i] );
		
		// @TODO: make sure the replacement is ok before deleting
		//if ( LineSegmentsIntersect( vNormal, vA, vB, vPrev, vCurrent, &flTimeOfIntersection, &flOtherTimeOfIntersection ) )

		CalcLineToLineIntersectionSegment( vA, vB, vPrev, vCurrent, NULL, NULL, &flTimeOfIntersection, &flOtherTimeOfIntersection );
		if ( flTimeOfIntersection >= -LINE_INTERSECT_EPSILON && flTimeOfIntersection <= 1.0f + LINE_INTERSECT_EPSILON && flOtherTimeOfIntersection >= -LINE_INTERSECT_EPSILON && flTimeOfIntersection <= 1.0f + LINE_INTERSECT_EPSILON )
		{
			// If the line segment intersection occurs right at the beginning of the hole line segment, ignore it because we'll catch it as an intersection at the end of 
			// another hole line segment.
			// This is required because we want to guarantee that a line segment from vA to vCurrent does not intersect the polygon at any point.
			if ( flTimeOfIntersection < *pLowestTimeOfIntersection && flOtherTimeOfIntersection > LINE_INTERSECT_EPSILON )
			{
				*pLowestTimeOfIntersection = flTimeOfIntersection;
				*pA = vPrev;
				*pB = vCurrent;

				float flPrevInsideDistance = ( vPrev - vA ).Dot( vInside );
				float flCurrentInsideDistance = ( vCurrent - vA ).Dot( vInside );
				if ( flCurrentInsideDistance > flPrevInsideDistance )
				{
					*pHoleVertexIndex = i;					
				}
				else
				{
					*pHoleVertexIndex = ( i + hole.m_Indices.Count() - 1 ) % hole.m_Indices.Count();					
				}
				
				bIntersect = true;
			}
		}
		vPrev = vCurrent;
	}
	return bIntersect;
}

void FindLineSegmentIntersectingDiagonal( const CUtlVector< Vector > &polygonPoints, const Vector &vNormal, const CUtlVector< SubPolygon_t > &holes, const Vector &vA, const Vector &vB, Vector *pHoleSegmentA, Vector *pHoleSegmentB, int *pHoleIndex, int *pHoleVertexIndex )
{
	float flLowestTimeOfIntersection = 2.0f;
	int nHoleVertexIndex;
	*pHoleIndex = -1;

	// Test for holes
	for ( int i = 0; i < holes.Count(); ++ i )
	{
		float flTimeOfIntersection;
		Vector vTempA, vTempB;
		if ( LineSegmentIntersectsPolygon( polygonPoints, vNormal, vA, vB, holes[i], &flTimeOfIntersection, &vTempA, &vTempB, &nHoleVertexIndex ) )
		{
			if ( flTimeOfIntersection < flLowestTimeOfIntersection )
			{
				flLowestTimeOfIntersection = flTimeOfIntersection;
				*pHoleSegmentA = vTempA;
				*pHoleSegmentB = vTempB;
				*pHoleIndex = i;
				*pHoleVertexIndex = nHoleVertexIndex;
			}
		}
	}
}

void DecomposePolygon_Step( const CUtlVector< Vector > &polygonPoints, const Vector &vNormal, CUtlVector< SubPolygon_t > *pHoles, SubPolygon_t *pNewPartition, SubPolygon_t *pOriginalPolygon, int *pFirstIndex )
{
	Assert( *pFirstIndex >= 0 && *pFirstIndex < pOriginalPolygon->m_Indices.Count() );
	
	// Always start decomposition on a notch
	Vector vPrev = SubPolygon_t::GetPoint( polygonPoints, pOriginalPolygon->GetVertexIndex( *pFirstIndex - 1 ) );
	Vector vCurrent = SubPolygon_t::GetPoint( polygonPoints, pOriginalPolygon->GetVertexIndex( *pFirstIndex ) );
	Vector vNext = SubPolygon_t::GetPoint( polygonPoints, pOriginalPolygon->GetVertexIndex( *pFirstIndex + 1 ) );
	for ( int i = 0; i < pOriginalPolygon->m_Indices.Count(); ++ i )
	{
		if ( IsConcave( vPrev, vCurrent, vNext, vNormal) )
		{
			*pFirstIndex = ( *pFirstIndex + i ) % pOriginalPolygon->m_Indices.Count();
			break;
		}
		else
		{
			vPrev = vCurrent;
			vCurrent = vNext;
			vNext = SubPolygon_t::GetPoint( polygonPoints, pOriginalPolygon->GetVertexIndex( *pFirstIndex + i + 2 ) );
		}
	}
	
	// On termination of the loop, pOriginalPolygon->m_Indices[*pFirstIndex] is the vertex index (in polygonPoints) from
	// which to begin decomposition


	// Attempt decomposition (first clockwise, then counter-clockwise if that fails)
	for ( int i = 0; i < 2; ++ i )
	{
		bool bClockwise = ( i == 0 );

		pNewPartition->m_Indices.RemoveAll();

		// Grab the first 2 vertices from the remaining polygon and add to the new potential convex partition
		int nFirstVertex = pOriginalPolygon->GetVertexIndex( *pFirstIndex );
		int nSecondVertex = pOriginalPolygon->GetVertexIndex( *pFirstIndex + ( bClockwise ? 1 : -1 ) );
		if ( bClockwise )
		{
			pNewPartition->m_Indices.AddToTail( nFirstVertex );
			pNewPartition->m_Indices.AddToTail( nSecondVertex );
		}
		else
		{
			pNewPartition->m_Indices.AddToTail( nSecondVertex );
			pNewPartition->m_Indices.AddToTail( nFirstVertex );
		}		

		Vector vFirst = SubPolygon_t::GetPoint( polygonPoints, nFirstVertex );
		Vector vSecond = SubPolygon_t::GetPoint( polygonPoints, nSecondVertex );
		Vector vPrevPrev = vFirst;
		vPrev = vSecond;

		int nNextIndex = *pFirstIndex + ( bClockwise ? 2 : -2 );
		int nNextVertex = pOriginalPolygon->GetVertexIndex( nNextIndex );

		// At the start of each iteration, *pFirstIndex refers to the index of the first vertex from the original polygon that is in the partition
		// and nNextIndex refers to 1 past the index of the last vertex from the original polygon that is in the partition.
		// If clockwise, you can find the new convex partition by iterating indices in original polygon from [*pFirstIndex, nNextIndex-1], 
		// if counter-clockwise, iterate from [nNextIndex+1, *pFirstIndex].
		while ( pNewPartition->m_Indices.Count() < pOriginalPolygon->m_Indices.Count() ) 
		{
			vCurrent = SubPolygon_t::GetPoint( polygonPoints, nNextVertex );

			bool bConcave;
			if ( bClockwise )
			{
				bConcave = IsConcave( vPrevPrev, vPrev, vCurrent, vNormal ) || IsConcave( vPrev, vCurrent, vFirst, vNormal ) || IsConcave( vCurrent, vFirst, vSecond, vNormal );
			}
			else
			{
				bConcave = IsConcave( vCurrent, vPrev, vPrevPrev, vNormal ) || IsConcave( vFirst, vCurrent, vPrev, vNormal ) || IsConcave( vSecond, vFirst, vCurrent, vNormal );
			}

			if ( bConcave )
			{
				// Shape is no longer convex with the addition of vCurrent
				break;
			}
			else
			{
				if ( bClockwise )
				{
					pNewPartition->m_Indices.AddToTail( nNextVertex );
					++ nNextIndex;
				}
				else
				{
					pNewPartition->m_Indices.AddToHead( nNextVertex );				
					-- nNextIndex;
				}

				nNextVertex = pOriginalPolygon->GetVertexIndex( nNextIndex );
				vPrevPrev = vPrev;
				vPrev = vCurrent;
			}
		} 

		int nFirstIndexInRemainingPolygon = bClockwise ? nNextIndex : *pFirstIndex + 1;
		int nLastIndexInRemainingPolygon = bClockwise ? *pFirstIndex : nNextIndex + 1;
		
		// Test to see if any points in the remaining polygon are within the bounds of the convex polygon we're about to peel off.
		while ( pNewPartition->m_Indices.Count() >= 3 && PointsInsideConvexArea( polygonPoints, *pOriginalPolygon, vNormal, nFirstIndexInRemainingPolygon, nLastIndexInRemainingPolygon, *pNewPartition ) )
		{
			if ( bClockwise )
			{
				pNewPartition->m_Indices.RemoveMultipleFromTail( 1 );
				-- nNextIndex;
			}
			else
			{
				pNewPartition->m_Indices.RemoveMultipleFromHead( 1 );
				++ nNextIndex;
			}
		}
		
		// Found a convex chunk of the original concave polygon, now check for
		// holes or concavities which intersect this convex chunk.
		if ( pNewPartition->m_Indices.Count() >= 3 )
		{
			Vector vA = SubPolygon_t::GetPoint( polygonPoints, pNewPartition->m_Indices[pNewPartition->m_Indices.Count() - 1] );
			Vector vB = SubPolygon_t::GetPoint( polygonPoints, pNewPartition->m_Indices[0] );
			Vector vHoleSegmentA, vHoleSegmentB;
			int nHoleIndex = -1;
			int nLastHoleIndex;
			int nHoleVertexIndex;

			// See if the diagonal which closes this convex region intersects any holes
			FindLineSegmentIntersectingDiagonal( polygonPoints, vNormal, *pHoles, vA, vB, &vHoleSegmentA, &vHoleSegmentB, &nHoleIndex, &nHoleVertexIndex );
			
			// If there was an intersection, keep refining the diagonal until it no longer changes
			if ( nHoleIndex != -1 )
			{
				do 
				{
					nLastHoleIndex = nHoleIndex;
					vB = SubPolygon_t::GetPoint( polygonPoints, pHoles->Element( nHoleIndex ).GetVertexIndex( nHoleVertexIndex ) );
					FindLineSegmentIntersectingDiagonal( polygonPoints, vNormal, *pHoles, vA, vB, &vHoleSegmentA, &vHoleSegmentB, &nHoleIndex, &nHoleVertexIndex );

					Assert( nHoleIndex != -1 );
				} while ( nHoleIndex != nLastHoleIndex );
			}
			
			// If there was no intersection, check to see if this convex region completely encloses any holes			
			if ( nHoleIndex == -1 )
			{
				Vector vHolePoint;
				int nHoleInRegionIndex = -1;
				for ( int i = 0; i < pHoles->Count(); ++ i )
				{
					vHolePoint = SubPolygon_t::GetPoint( polygonPoints, pHoles->Element( i ).m_Indices[0] );
					if ( IsPointInPolygon( polygonPoints, *pNewPartition, vNormal, vHolePoint ) )
					{
						// hole is within the region
						nHoleInRegionIndex = i;
						break;
					}
				}

				if ( nHoleInRegionIndex != -1 )
				{
					// If any holes are enclosed, "fix" the diagonal to connect to an arbitrary vertex on one of the enclosed holes
					vB = vHolePoint;

					// Now test to see if this new diagonal intersects any holes
					FindLineSegmentIntersectingDiagonal( polygonPoints, vNormal, *pHoles, vA, vB, &vHoleSegmentA, &vHoleSegmentB, &nHoleIndex, &nHoleVertexIndex );

					// If there was an intersection, keep refining the diagonal until it no longer changes
					if ( nHoleIndex != -1 )
					{
						do 
						{
							nLastHoleIndex = nHoleIndex;
							vB = SubPolygon_t::GetPoint( polygonPoints, pHoles->Element( nHoleIndex ).GetVertexIndex( nHoleVertexIndex ) );
							FindLineSegmentIntersectingDiagonal( polygonPoints, vNormal, *pHoles, vA, vB, &vHoleSegmentA, &vHoleSegmentB, &nHoleIndex, &nHoleVertexIndex );
							Assert( nHoleIndex != -1 );
						} while ( nHoleIndex != nLastHoleIndex );
					}
				}
			}

			// At this point, we should either have the original convex region which contains no holes and intersects with nothing,
			// or a refined diagonal which connects to a hole without intersecting any other holes or convex region points
			if ( nHoleIndex != -1 )
			{
				// We have a refined diagonal; absorb the hole into the original polygon.
				// Reject this partition but add the hole's vertices to the original polygon.

				int nInsertAfterIndex = ( bClockwise ? nNextIndex + pOriginalPolygon->m_Indices.Count() - 1 : *pFirstIndex ) % pOriginalPolygon->m_Indices.Count();
				const SubPolygon_t *pHolePolygon = &pHoles->Element( nHoleIndex );
				int nConnectBackToVertex = pOriginalPolygon->m_Indices[nInsertAfterIndex];
				for ( int i = 0; i < pHolePolygon->m_Indices.Count(); ++ i )
				{
					pOriginalPolygon->m_Indices.InsertAfter( nInsertAfterIndex, pHolePolygon->m_Indices[( i + nHoleVertexIndex ) % pHolePolygon->m_Indices.Count()] );
					++ nInsertAfterIndex;
				}
				pOriginalPolygon->m_Indices.InsertAfter( nInsertAfterIndex, pHolePolygon->m_Indices[nHoleVertexIndex] );
				++ nInsertAfterIndex;
				pOriginalPolygon->m_Indices.InsertAfter( nInsertAfterIndex, nConnectBackToVertex );
				
				pNewPartition->m_Indices.RemoveAll();
				pHoles->Remove( nHoleIndex );
			}
			else
			{
				// We have the original, valid diagonal.
				// Remove the corresponding indices from the original polygon to peel off the new convex region
				int nIndexToRemove = ( ( bClockwise ? *pFirstIndex : nNextIndex + 1 ) + 1 ) % pOriginalPolygon->m_Indices.Count();
				if ( nIndexToRemove < 0 ) nIndexToRemove += pOriginalPolygon->m_Indices.Count();

				for ( int i = 1; i < pNewPartition->m_Indices.Count() - 1; ++ i )
				{
					nIndexToRemove = nIndexToRemove % pOriginalPolygon->m_Indices.Count();
					Assert( pOriginalPolygon->m_Indices[nIndexToRemove] == pNewPartition->m_Indices[i] );
					pOriginalPolygon->m_Indices.Remove( nIndexToRemove );
				}

				*pFirstIndex = nIndexToRemove % pOriginalPolygon->m_Indices.Count();
			}

			// Done!
			return;
		}
	}

	// Couldn't find a match either clockwise or counter-clockwise
	*pFirstIndex = ( *pFirstIndex + 1 ) % pOriginalPolygon->m_Indices.Count();
}

void DecomposePolygon( const CUtlVector< Vector > &polygonPoints, const Vector &vNormal, SubPolygon_t *pOriginalPolygon, CUtlVector< SubPolygon_t > *pHoles, CUtlVector< SubPolygon_t > *pPartitions )
{
	int nFirstIndex = 0; // The Nth vertex in the remaining polygon

	SubPolygon_t *pNewPartition = NULL;
	while ( pOriginalPolygon->m_Indices.Count() >= 3 )
	{
		if ( !pNewPartition )
		{
			pNewPartition = &pPartitions->Element( pPartitions->AddToTail() );
		}
		DecomposePolygon_Step( polygonPoints, vNormal, pHoles, pNewPartition, pOriginalPolygon, &nFirstIndex );
		if ( pNewPartition->m_Indices.Count() >= 3 )
		{
			pNewPartition = NULL;
		}
		else
		{
			pNewPartition->m_Indices.RemoveAll();
		}
	}
}

bool IsPointInPolygonPrism( const Vector *pPolygonPoints, int nPointCount, const Vector &vPoint, float flThreshold, float *pHeight )
{
	Assert( nPointCount >= 3 );

	Vector vNormal = ( pPolygonPoints[0] - pPolygonPoints[1] ).Cross( pPolygonPoints[2] - pPolygonPoints[1] );
	Vector vPrev = pPolygonPoints[nPointCount - 1];
	for ( int nVertexIndex = 0; nVertexIndex < nPointCount; ++ nVertexIndex )
	{
		Vector vCurrent = pPolygonPoints[nVertexIndex];
		Vector vAbove = vPrev + vNormal;
		Vector vOutwardPlaneNormal = ( vPrev - vCurrent ).Cross( vAbove - vCurrent );

		if ( ( vPoint - vCurrent ).Dot( vOutwardPlaneNormal ) > flThreshold )
		{
			// Outside the prism
			return false;
		}
		vPrev = vCurrent;
	}

	if ( pHeight != NULL )
	{
		vNormal.NormalizeInPlace();
		*pHeight = ( vPoint - pPolygonPoints[0] ).Dot( vNormal );
	}

	return true;
}

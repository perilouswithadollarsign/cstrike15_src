//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Utility functions for polygon simplification / convex decomposition.
//
//===============================================================================

#ifndef POLYGON_H
#define POLYGON_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"

//-----------------------------------------------------------------------------
// NOTE: Polygons are assumed to be wound clockwise unless otherwise noted.
// Holes in polygons wind counter-clockwise.
//-----------------------------------------------------------------------------



static const float POINT_IN_POLYGON_EPSILON = 0.01f;

//-----------------------------------------------------------------------------
// Simplifies a polygon by removing points such that the area will not
// decrease by more than the specified amount (area increases are not
// allowed).
// With a low max deviation, this will perfectly remove all colinear points.
//-----------------------------------------------------------------------------
void SimplifyPolygon( CUtlVector< Vector > *pPoints, const Vector &vNormal, float flMaxDeviation );

//-----------------------------------------------------------------------------
// Simplifies a polygon using quadric error metrics.
//-----------------------------------------------------------------------------
void SimplifyPolygonQEM( CUtlVector< Vector > *pPoints, const Vector &vNormal, float flMaximumSquaredError, bool bUseOptimalPointPlacement );

//-----------------------------------------------------------------------------
// Returns whether a vertex of a polygon (v1) is concave, given its normal 
// and previous & next vertices (v0 and v2, respectively).
//-----------------------------------------------------------------------------
bool IsConcave( const Vector &v0, const Vector &v1, const Vector &v2, const Vector &vNormal );

//-----------------------------------------------------------------------------
// Returns whether a vertex (points[nVertex]) of a polygon is 
// concave, given the polygon's vertices, and its normal.
//-----------------------------------------------------------------------------
bool IsConcave( const Vector *pPolygonPoints, int nPointCount, int nVertex, const Vector &vNormal );

//-----------------------------------------------------------------------------
// Returns whether a polygon is concave.
//-----------------------------------------------------------------------------
bool IsConcave( const Vector *pPolygonPoints, int nPointCount, const Vector &vNormal );

//-----------------------------------------------------------------------------
// Given a set of points (i.e. vertex buffer), this represents an ordered
// subset of points which comprise a polygon (i.e. index buffer).
//-----------------------------------------------------------------------------
struct SubPolygon_t
{
	CUtlVector< int > m_Indices;
	
	int GetVertexIndex( int i ) const
	{
		i = i % m_Indices.Count();
		if ( i < 0 )
		{
			i += m_Indices.Count();
		}
		return m_Indices[i];
	}

	static const Vector &GetPoint( const Vector *pPolygonPoints, int nPointCount, int nVertex )
	{
		nVertex = nVertex % nPointCount;
		if ( nVertex < 0 )
		{
			nVertex += nPointCount;
		}
		return pPolygonPoints[nVertex];
	}

	static const Vector &GetPoint( const CUtlVector< Vector > &originalPoints, int nVertex )
	{
		return GetPoint( originalPoints.Base(), originalPoints.Count(), nVertex );
	}
};

//-----------------------------------------------------------------------------
// Attempts to strip off one convex region from a concave/convex polygon.
//-----------------------------------------------------------------------------
void DecomposePolygon_Step( const CUtlVector< Vector > &polygonPoints, const Vector &vNormal, CUtlVector< SubPolygon_t > *pHoles, SubPolygon_t *pNewPartition, SubPolygon_t *pRemainingPolygon, int *pFirstIndex );

//-----------------------------------------------------------------------------
// Decomposes a polygon into one or more convex, non-overlapping parts.
//-----------------------------------------------------------------------------
void DecomposePolygon( const CUtlVector< Vector > &polygonPoints, const Vector &vNormal, SubPolygon_t *pOriginalPolygon, CUtlVector< SubPolygon_t > *pHoles, CUtlVector< SubPolygon_t > *pPartitions );

//-----------------------------------------------------------------------------
// Is a point in the prism formed by extruding the polygon?
// If so, what is its height above/below the plane of the polygon?
//-----------------------------------------------------------------------------
bool IsPointInPolygonPrism( const Vector *pPolygonPoints, int nPointCount, const Vector &vPoint, float flThreshold = 0.0f, float *pHeight = NULL );

#endif // POLYGON_H
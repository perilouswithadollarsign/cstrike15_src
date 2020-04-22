//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Utility functions for BSP and map file math operations.
//
//===============================================================================

#ifndef VBSPMATHUTIL_H
#define VBSPMATHUTIL_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "mathlib/vector.h"
#include "worldsize.h"
#include "bspfile.h"

DECLARE_LOGGING_CHANNEL( LOG_VBSP2 );

//-----------------------------------------------------------------------------
// A threshold used to determine whether coordinates of two normals are equal.
//-----------------------------------------------------------------------------
static const float c_flNormalEpsilon = 0.00001f;

//-----------------------------------------------------------------------------
// A threshold used to determine whether the distance value of two planes
// are equal.  Also used for rounding coordinates to the nearest integer.
//-----------------------------------------------------------------------------
static const float c_flDistanceEpsilon = 0.01f;

//-----------------------------------------------------------------------------
// Threshold used to determine whether something is entirely on one side
// of a plane or another.
//-----------------------------------------------------------------------------
static const float c_flPlaneSideEpsilon = 0.1f;

//-----------------------------------------------------------------------------
// Threshold used to determine whether points should be snapped to each other.
//-----------------------------------------------------------------------------
static const float c_flWeldVertexEpsilon = 0.1f;

//-----------------------------------------------------------------------------
// Threshold used when clipping to determine whether a point is on a plane
// or on a particular side.
//-----------------------------------------------------------------------------
static const float c_flPlaneClipEpsilon = 0.01f;

//-----------------------------------------------------------------------------
// Polygons with more points than this are not handled properly by
// the system.
//-----------------------------------------------------------------------------
static const int MAX_POINTS_ON_POLYGON = 64;

//-----------------------------------------------------------------------------
// Represents a plane.  
// Unlike a standard plane equation, a point P is defined to be on a plane if
// P * m_vNormal - m_flDistance == 0.
// A point P is defined to be in the positive half-space if 
// P * m_vNormal - m_flDistance > 0.
// (In a standard plane equation that minus sign is a plus sign)
//-----------------------------------------------------------------------------
struct Plane_t
{
	Vector m_vNormal;
	float m_flDistance;
	// A value in the range [0,5] which indicates the axis alignment,
	// e.g. PLANE_X, PLANE_Y, PLANE_Z, PLANE_ANYX, PLANE_ANYY, PLANE_ANYZ
	int m_Type;
};

//-----------------------------------------------------------------------------
// Flags indicating on which side of a plane something lies.
//-----------------------------------------------------------------------------
enum PlaneSide_t
{
	PLANE_SIDE_INVALID = 0,
	PLANE_SIDE_FRONT = 1,
	PLANE_SIDE_BACK = 2,
	PLANE_SIDE_BOTH = PLANE_SIDE_FRONT | PLANE_SIDE_BACK,
	// "Facing" means that the point or face is directly on the plane;
	// this may be combined with front or back to take into account
	// direction.
	PLANE_SIDE_FACING = 4,
};

//-----------------------------------------------------------------------------
// Returns which side of a plane a bounding box is on.
//-----------------------------------------------------------------------------
PlaneSide_t GetPlaneSide( const Vector &vMin, const Vector &vMax, Plane_t *pPlane );

//-----------------------------------------------------------------------------
// Snaps the plane to be axis-aligned if it is within an epsilon of axial.
//-----------------------------------------------------------------------------
bool SnapVector( Vector &vNormal );

//-----------------------------------------------------------------------------
// Snaps the plane to be axis-aligned if it is within an epsilon of axial.
// Recalculates dist if the vNormal was snapped. Rounds dist to integer
// if it is within an epsilon of integer.
// 
// vNormal - Plane vNormal vector (assumed to be unit length).
// flDistance - Plane constant.
// v0, v1, v2 - Three points on the plane.
//-----------------------------------------------------------------------------
void SnapPlane( Vector &vNormal, vec_t &flDistance, const Vector &v0, const Vector &v1, const Vector &v2 );

//-----------------------------------------------------------------------------
// Returns true if one plane representation is equal to the other,
// within an epsilon threshold.
//-----------------------------------------------------------------------------
bool PlaneEqual( Plane_t *pPlane, const Vector &vNormal, float flDistance );

//-----------------------------------------------------------------------------
// Returns a value classifying the plane based on its axis alignment, 
// e.g. PLANE_X, PLANE_Y, PLANE_Z, PLANE_ANYX, PLANE_ANYY, PLANE_ANYZ
//-----------------------------------------------------------------------------
int GetPlaneTypeFromNormal( const Vector &vNormal );

//-----------------------------------------------------------------------------
// Compute a normal for a triangle, given three points.
// The points are clockwise when looking at the triangle from the normal side.
//-----------------------------------------------------------------------------
Vector TriangleNormal( const Vector &v0, const Vector &v1, const Vector &v2 );

//-----------------------------------------------------------------------------
// A polygon used to represent faces on maps, BSPs, etc.
// This class is copyable.
//-----------------------------------------------------------------------------
class Polygon_t
{
public:
	CCopyableUtlVector< Vector > m_Points;
};

//-----------------------------------------------------------------------------
// Computes whether a given polygon is tiny, relative to an epsilon threshold
//-----------------------------------------------------------------------------
bool IsPolygonTiny( Polygon_t *pPolygon );

//-----------------------------------------------------------------------------
// Checks all points in a polygon against the min and max coordinates.
//-----------------------------------------------------------------------------
bool IsPolygonHuge( Polygon_t *pPolygon );

//-----------------------------------------------------------------------------
// Computes the area of a polygon
//-----------------------------------------------------------------------------
float ComputePolygonArea( const Polygon_t &polygon );

//-----------------------------------------------------------------------------
// Creates a large polygon with extremal coordinates that lies on the plane
//-----------------------------------------------------------------------------
void CreatePolygonFromPlane( const Vector &vNormal, float flDistance, Polygon_t *pPolygon );

//-----------------------------------------------------------------------------
// Clips a polygon against a plane, creating* either:
// 1) One new, identical polygon (pOn), if the polygon is coincident 
//    with the plane.
// 2) One new, identical polygon (either pFront or pBack) if the polygon 
//    is entirely on one side of the plane.
// 3) Two new, different polygons (pFront and pBack) if the polygon is 
//    clipped by the plane
//
// If pOn is NULL, the "on" case is ignored and treated as either front
// or back appropriately.
//
// *The passed-in polygons (pOn, pFront, pBack) have their point arrays
// cleared at the start of the function.  They are considered to be
// "created" upon exit if their point array has a non-zero point count.
//-----------------------------------------------------------------------------
void ChopPolygon( const Polygon_t &polygon, const Vector &vNormal, float flDistance, Polygon_t *pOn, Polygon_t *pFront, Polygon_t *pBack );

//-----------------------------------------------------------------------------
// Clips a polygon against a plane. If the polygon is completely clipped,
// pPolygon->m_Points will be empty ( Count() == 0 ).
//-----------------------------------------------------------------------------
void ChopPolygonInPlace( Polygon_t *pPolygon, const Vector &vNormal, float flDistance );

//-----------------------------------------------------------------------------
// A plane which can be stored efficiently in a hash table for fast lookup.
//-----------------------------------------------------------------------------
struct HashedPlane_t : public Plane_t
{
	int m_nNextPlaneIndex;
};

// Make sure this is a power of 2, code depends on it.
static const int PLANE_HASH_TABLE_SIZE = 1024;

//-----------------------------------------------------------------------------
// A class to hash and pool planes.
//-----------------------------------------------------------------------------
class CPlaneHash
{
public:
	CPlaneHash();

	int FindPlaneIndex( const Vector &vNormal, float flDistance );
	int FindPlaneIndex( Vector vPoints[3] );

	CCopyableUtlVector< HashedPlane_t > m_Planes;

private:
	HashedPlane_t *AllocateNewPlane();

	// Hash table consisting of indices into the m_Planes array
	int m_HashTable[PLANE_HASH_TABLE_SIZE];
};

//-----------------------------------------------------------------------------
// A class to hash, weld, and pool vertices.
//-----------------------------------------------------------------------------
class CVertexHash
{
public:
	CVertexHash();
	
	CUtlVector< Vector > &GetVertices() { return m_Vertices; }
	const CUtlVector< Vector > &GetVertices() const { return m_Vertices; }
	void Purge();

	// If bAlwaysAdd is true, this will skip the hash check and always add a new vertex 
	// (useful if vertex indices must be kept consistent and duplicates might exist)
	int FindVertexIndex( const Vector &vertex, bool bAlwaysAdd = false );

private:
	static const int m_nHashBitShift = 8;
	static const int m_nHashLength = COORD_EXTENT >> m_nHashBitShift;
	// Hash along the X and Y axes by grouping vertices into hash buckets
	// which are NxN square columns of space.
	// ~64 KB
	int m_nVertexHash[m_nHashLength * m_nHashLength];
	CUtlVector< int > m_VertexHashChain;
	CUtlVector< Vector > m_Vertices;
};

#endif // VBSPMATHUTIL_H
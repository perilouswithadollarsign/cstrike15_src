//--------------------------------------------------------------------------------------------------
/**
	@file		qhHalfEdge.h

	@author		Dirk Gregorius
	@version	0.1
	@date		30/11/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include "qhTypes.h"
#include "qhMath.h"
#include "qhList.h"

struct qhVertex;
struct qhHalfEdge;
struct qhFace;

#define QH_MARK_VISIBLE		0
#define QH_MARK_DELETE		1
#define QH_MARK_CONCAVE		2
#define QH_MARK_CONFIRM		3


//--------------------------------------------------------------------------------------------------
// qhVertex
//--------------------------------------------------------------------------------------------------
struct qhVertex 
	{
	qhVertex* Prev;
	qhVertex* Next;

	int Mark;
	qhVector3 Position;
	qhHalfEdge* Edge;
	qhFace* ConflictFace;
	};


//--------------------------------------------------------------------------------------------------
// qhHalfEdge
//--------------------------------------------------------------------------------------------------
struct qhHalfEdge
	{
	qhHalfEdge* Prev;
	qhHalfEdge* Next;

	qhVertex* Origin;
	qhFace* Face;
	qhHalfEdge* Twin;

	bool IsConvex( qhReal Tolerance ) const;
	};


//--------------------------------------------------------------------------------------------------
// qhFace
//--------------------------------------------------------------------------------------------------
struct qhFace
	{
	qhFace* Prev;
	qhFace* Next;

	qhHalfEdge* Edge;

	int Mark;
	qhReal Area;
	qhVector3 Centroid;
	qhPlane Plane;
	bool Flipped;

	qhList< qhVertex > ConflictList;
	};


//--------------------------------------------------------------------------------------------------
// Utilities
//--------------------------------------------------------------------------------------------------
void qhLinkFace( qhFace* Face, int Index, qhHalfEdge* Twin );
void qhLinkFaces( qhFace* Face1, int Index1, qhFace* Face2, int Index2 );
void qhNewellPlane( qhFace* Face );

int qhVertexCount( const qhFace* Face );
bool qhIsConvex( const qhFace* Face, qhReal Tolerance );
bool qhCheckConsistency( const qhFace* Face );






//--------------------------------------------------------------------------------------------------
// qhHalfEdge.cpp
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------
#include "qhHalfEdge.h"


//--------------------------------------------------------------------------------------------------
// Utilities
//--------------------------------------------------------------------------------------------------
bool qhHalfEdge::IsConvex( qhReal Tolerance ) const
	{
	return Face->Plane.Distance( Twin->Face->Centroid ) < -Tolerance;
	}


//--------------------------------------------------------------------------------------------------
void qhLinkFace( qhFace* Face, int Index, qhHalfEdge* Twin )
	{
	qhHalfEdge* Edge = Face->Edge;
	while ( Index-- > 0 )
		{
		Edge = Edge->Next;
		}

	QH_ASSERT( Edge != Twin );
	Edge->Twin = Twin;
	Twin->Twin = Edge;
	}


//--------------------------------------------------------------------------------------------------
void qhLinkFaces( qhFace* Face1, int Index1, qhFace* Face2, int Index2 )
	{
	qhHalfEdge* Edge1 = Face1->Edge;
	while ( Index1-- > 0 )
		{
		Edge1 = Edge1->Next;
		}

	qhHalfEdge* Edge2 = Face2->Edge;
	while ( Index2-- > 0 )
		{
		Edge2 = Edge2->Next;
		}

	QH_ASSERT( Edge1 != Edge2 );
	Edge1->Twin = Edge2;
	Edge2->Twin = Edge1;
	}


//--------------------------------------------------------------------------------------------------
void qhNewellPlane( qhFace* Face )
	{	
	int Count = 0;
	qhVector3 Centroid = QH_VEC3_ZERO; 
	qhVector3 Normal = QH_VEC3_ZERO;

	qhHalfEdge* Edge = Face->Edge;
	QH_ASSERT( Edge->Face == Face );

	do 
		{
		qhHalfEdge* Twin = Edge->Twin;
		QH_ASSERT( Twin->Twin == Edge );
		
		const qhVector3& V1 = Edge->Origin->Position;
		const qhVector3& V2 = Twin->Origin->Position;

		Count++;
		Centroid += V1;

		// This seems to be more robust than N += Cross( V1, V2 )
		Normal.X += ( V1.Y - V2.Y ) * ( V1.Z + V2.Z );
		Normal.Y += ( V1.Z - V2.Z ) * ( V1.X + V2.X );
		Normal.Z += ( V1.X - V2.X ) * ( V1.Y + V2.Y );
		
		Edge = Edge->Next;
		} 
	while ( Edge != Face->Edge );
	
	QH_ASSERT( Count > 2 );
	Centroid /= qhReal( Count );
	Face->Centroid = Centroid;

	qhReal Area = qhLength( Normal );
	QH_ASSERT( Area > qhReal( 0.0 ) );
	Normal /= Area;

	Face->Plane = qhPlane( Normal, Centroid );
	Face->Area = Area;
	}


//--------------------------------------------------------------------------------------------------
bool qhIsConvex( const qhFace* Face, qhReal Tolerance )
	{
	const qhHalfEdge* Edge = Face->Edge;

	do 
		{
		qhVector3 Tail = Edge->Origin->Position;
		qhVector3 Head = Edge->Twin->Origin->Position;

		qhVector3 Offset = Head - Tail;
		qhVector3 Normal = qhCross( Offset, Face->Plane.Normal );
		
		qhPlane Plane( Normal, Head );
		Plane.Normalize();
		
		qhReal Distance = Plane.Distance( Edge->Next->Twin->Origin->Position );
		if ( Distance > -Tolerance )
			{
			return false;
			}

		Edge = Edge->Next;
		} 
	while ( Edge != Face->Edge );

	return true;
	}


//--------------------------------------------------------------------------------------------------
int qhVertexCount( const qhFace* Face )
	{
	int VertexCount = 0;

	const qhHalfEdge* Edge = Face->Edge;
	do 
		{
		VertexCount++;
		Edge = Edge->Next;
		} 
	while ( Edge != Face->Edge );
		
	return VertexCount;
	}


//--------------------------------------------------------------------------------------------------
bool qhCheckConsistency( const qhFace* Face )
	{
	if ( Face->Mark == QH_MARK_DELETE )
		{
		// Face is not on the hull
		return false;
		}

	if ( qhVertexCount( Face ) < 3 )
		{
		// Invalid geometry
		return false;
		}

	const qhHalfEdge* Edge = Face->Edge;

	do 
		{
		const qhHalfEdge* Twin = Edge->Twin;
	
		if ( Twin == NULL )
			{
			// Unreflected edge
			return false;
			}

		if ( Twin->Face == NULL )
			{
			// Missing face
			return false;
			}

		if ( Twin->Face == Face )
			{
			// Edge is connecting the same face
			return false;
			}

		if ( Twin->Face->Mark == QH_MARK_DELETE )
			{
			// Face is not on hull 
			return false;
			}

		if ( Twin->Twin != Edge )
			{
			// Edge reflected incorrectly
			return false;
			}

		if ( Edge->Origin != Twin->Next->Origin )
			{
			// Topology error
			return false;
			}

		if ( Twin->Origin != Edge->Next->Origin )
			{
			// Topology error
			return false;
			}	

		if ( Edge->Face != Face )
			{
			// Topology error
			return false;
			}

		Edge = Edge->Next;
		} 
	while ( Edge != Face->Edge );

	return true;
	}





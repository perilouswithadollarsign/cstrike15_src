//--------------------------------------------------------------------------------------------------
// qhConvex.inl
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
// qhConvex
//--------------------------------------------------------------------------------------------------
inline qhVector3 qhConvex::GetCentroid( void ) const
	{
	qhVector3 Centroid = QH_VEC3_ZERO;
	if ( !mVertexList.Empty() )
		{
		int VertexCount = 0;
		for ( const qhVertex* Vertex = mVertexList.Begin(); Vertex != mVertexList.End(); Vertex = Vertex->Next )
			{
			Centroid += Vertex->Position;
			VertexCount++;
			}

		Centroid /= qhReal( VertexCount );
		}

	return Centroid;
	}


//--------------------------------------------------------------------------------------------------
inline int qhConvex::GetVertexCount( void ) const
	{
	return mVertexList.Size();
	}


//--------------------------------------------------------------------------------------------------
inline int qhConvex::GetEdgeCount( void ) const
	{
	int Count = 0;
	for ( const qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		qhHalfEdge* Edge = Face->Edge;
		QH_ASSERT( Edge != NULL );

		do 
			{
			Count += 1;
			Edge = Edge->Next;
			} 
		while ( Edge != Face->Edge );
		}

	return Count;
	}


//--------------------------------------------------------------------------------------------------
inline int qhConvex::GetFaceCount( void ) const
	{
	return mFaceList.Size();
	}


//--------------------------------------------------------------------------------------------------
inline const qhList< qhVertex >& qhConvex::GetVertexList( void ) const
	{
	return mVertexList;
	}


//--------------------------------------------------------------------------------------------------
inline const qhList< qhFace >& qhConvex::GetFaceList( void ) const
	{
	return mFaceList;
	}


//--------------------------------------------------------------------------------------------------
inline int qhConvex::GetIterationCount( void ) const
{
	return mIterations.Size();
}


//--------------------------------------------------------------------------------------------------
inline const qhIteration& qhConvex::GetIteration( int i ) const
{
	return mIterations[ i ];
}



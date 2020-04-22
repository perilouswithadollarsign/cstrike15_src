#include "fow.h"
#include "fow_trisoup.h"
#include "fow_lineoccluder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: constructor to init this collection with the id
// Input  : nID - unused
//-----------------------------------------------------------------------------
CFoW_TriSoupCollection::CFoW_TriSoupCollection( unsigned nID )
{
}


//-----------------------------------------------------------------------------
// Purpose: destructor to dealloc the occluders
//-----------------------------------------------------------------------------
CFoW_TriSoupCollection::~CFoW_TriSoupCollection( void )
{
	Clear();
}


//-----------------------------------------------------------------------------
// Purpose: clears all entries from the collection ( useful for hammer editing only )
//-----------------------------------------------------------------------------
void CFoW_TriSoupCollection::Clear( void )
{
	m_Occluders.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: adds a tri to the collection.  this is immediately split up into the horizontal slices.  very slow!
// Input  : pFoW - the main FoW object
//			vPointA - a point on the tri
//			vPointB - a point on the tri
//			vPointC - a point on the tri
//-----------------------------------------------------------------------------
void CFoW_TriSoupCollection::AddTri( CFoW *pFoW, Vector &vPointA, Vector &vPointB, Vector &vPointC )
{
	Vector	vVerts[ 3 ], vOutVerts[ 8 ];
	int		nBottomZ;
	int		nGridSize;
	int		nGridUnits;
	Vector	vNormal, vTestNormal;
	float	flIntercept;
	float	*pflVerticalLevels;

	vVerts[ 0 ] = vPointA;
	vVerts[ 1 ] = vPointB;
	vVerts[ 2 ] = vPointC;

	pFoW->GetVerticalGridInfo( nBottomZ, nGridSize, nGridUnits, &pflVerticalLevels );

	ComputeTrianglePlane( vPointA, vPointB, vPointC, vNormal, flIntercept );
	vTestNormal = vNormal;
	vTestNormal.z = 0.0f;
	vTestNormal.NormalizeInPlace();
	Vector2D	vTestNormal2( vTestNormal.x, vTestNormal.y );

	for ( int i = 0; i < nGridUnits; i++ )
	{
		nBottomZ = pflVerticalLevels[ i ] + 16.0f;

		int nCount = HorizontalSplitTri( vVerts, 3, vOutVerts, nBottomZ, 0.0f );
		if ( nCount == 2 )
		{
			CFoW_LineOccluder *pOccluder = new CFoW_LineOccluder( vOutVerts[ 0 ].x, vOutVerts[ 0 ].y, vOutVerts[ 1 ].x, vOutVerts[ 1 ].y, vTestNormal2, i );
			m_Occluders.AddToTail( pOccluder );
			pFoW->AddTriSoupOccluder( pOccluder, i );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: adds all occluders back into the visibility tree
// Input  : pFoW - the main FoW object
//-----------------------------------------------------------------------------
void CFoW_TriSoupCollection::RepopulateOccluders( CFoW *pFoW )
{
	for ( int i = 0; i < m_Occluders.Count(); i++ )
	{
		pFoW->AddTriSoupOccluder( m_Occluders[ i ], m_Occluders[ i ]->GetSliceNum() );
	}
}


#if 0
void CFoW_TriSoupCollection::ObstructViewer( CFoW *FoW, CFoW_Viewer *Viewer )
{
	for ( int i = 0; i < m_Occluders.Count(); i++ )
	{
		m_Occluders[ i ].ObstructViewer( FoW, Viewer );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: clip a poly to the horizontal plane and return the poly on the front side of the plane
// Input  : *pInVerts - input polygon
//			nVertCount - # verts in input poly
//			*pOutVerts - destination poly
//			flDist - plane constant
//			flOnPlaneEpsilon - the fudge factor for determining if a point is within the plane edge
// Output : int - # verts in output poly
//-----------------------------------------------------------------------------
int CFoW_TriSoupCollection::HorizontalSplitTri( Vector *pInVerts, int nVertCount, Vector *pOutVerts, float flDist, float flOnPlaneEpsilon )
{
	vec_t	*pDists = ( vec_t * )stackalloc( sizeof( vec_t ) * nVertCount * 4 ); //4x vertcount should cover all cases
	int		*pSides = ( int * )stackalloc( sizeof( vec_t ) * nVertCount * 4 );
	int		nCounts[ 3 ];
	vec_t	flDot;
	int		i, j;
	Vector	vMid = vec3_origin;
	int		nOutCount;
	Vector	vNormal( 0.0f, 0.0f, 1.0f );

	nCounts[ 0 ] = nCounts[ 1 ] = nCounts[ 2 ] = 0;

	// determine sides for each point
	for ( i = 0; i < nVertCount; i++ )
	{
		flDot = DotProduct( pInVerts[ i ], vNormal ) - flDist;
		pDists[ i ] = flDot;
		if ( flDot > flOnPlaneEpsilon )
		{
			pSides[ i ] = SIDE_FRONT;
		}
		else if ( flDot < -flOnPlaneEpsilon )
		{
			pSides[ i ] = SIDE_BACK;
		}
		else
		{
			pSides[ i ] = SIDE_ON;
		}
		nCounts[ pSides[ i ] ]++;
	}
	pSides[ i ] = pSides[ 0 ];
	pDists[ i ] = pDists[ 0 ];

	if ( !nCounts[ SIDE_FRONT ] )
	{	// if this has 2 sides that are side_on, then this should be a tri coming soon with the same two sides that are on, but with one on side_back
		return 0;
	}

	nOutCount = 0;
	for ( i = 0; i < nVertCount; i++ )
	{
		int nCurrent = ( i + 0 ) % nVertCount;
		int nNext = ( i + 1 ) % nVertCount;

		Vector &p1 = pInVerts[ nCurrent ];

		if ( pSides[ nCurrent ] == SIDE_ON )
		{
			VectorCopy( p1, pOutVerts[ nOutCount ] );
			nOutCount++;
			continue;
		}

		if ( pSides[ nCurrent ] == SIDE_FRONT )
		{
			//			VectorCopy( p1, outVerts[outCount]);
			//			outCount++;
		}

		if ( pSides[ nNext ] == SIDE_ON || pSides[ nNext ] == pSides[ nCurrent ] )
		{
			continue;
		}

		// generate a split point
		Vector &p2 = pInVerts[ nNext ];

		flDot = pDists[ nCurrent ] / ( pDists[ nCurrent ] - pDists[ nNext ] );
		for ( j = 0; j < 3; j++ )
		{	// avoid round off error when possible
			if ( vNormal[ j ] == 1 )
			{
				vMid[ j ] = flDist;
			}
			else if ( vNormal[ j ] == -1 )
			{
				vMid[ j ] = -flDist;
			}
			else
			{
				vMid[ j ] = p1[ j ] + flDot * ( p2[ j ] - p1[ j ] );
			}
		}

		VectorCopy ( vMid, pOutVerts[ nOutCount ] );
		nOutCount++;
	}

	return nOutCount;
}

#include <tier0/memdbgoff.h>


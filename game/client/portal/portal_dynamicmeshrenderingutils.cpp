//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "Portal_DynamicMeshRenderingUtils.h"
#include "iviewrender.h"

extern ConVar mat_wireframe;


int ClipPolyToPlane_LerpTexCoords( PortalMeshPoint_t *inVerts, int vertCount, PortalMeshPoint_t *outVerts, const Vector& normal, float dist, float fOnPlaneEpsilon )
{
	vec_t	*dists = (vec_t *)stackalloc( sizeof(vec_t) * vertCount * 4 ); //4x vertcount should cover all cases
	int		*sides = (int *)stackalloc( sizeof(int) * vertCount * 4 );
	int		counts[3];
	vec_t	dot;
	int		i, j;
	Vector	mid = vec3_origin;
	int		outCount;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0; i < vertCount; i++ )
	{
		dot = DotProduct( inVerts[i].vWorldSpacePosition, normal) - dist;
		dists[i] = dot;
		if ( dot > fOnPlaneEpsilon )
			sides[i] = SIDE_FRONT;
		else if ( dot < -fOnPlaneEpsilon )
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0])
		return 0;

	if (!counts[1])
	{
		// Copy to output verts
		//for ( i = 0; i < vertCount; i++ )
		memcpy( outVerts, inVerts, sizeof( PortalMeshPoint_t ) * vertCount );
		return vertCount;
	}

	outCount = 0;
	for ( i = 0; i < vertCount; i++ )
	{
		if (sides[i] == SIDE_ON)
		{
			memcpy( &outVerts[outCount], &inVerts[i], sizeof( PortalMeshPoint_t ) );
			++outCount;
			continue;
		}
		if (sides[i] == SIDE_FRONT)
		{
			memcpy( &outVerts[outCount], &inVerts[i], sizeof( PortalMeshPoint_t ) );
			++outCount;
		}
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		Vector& p1 = inVerts[i].vWorldSpacePosition;
		

		// generate a split point
		int i2 = (i+1)%vertCount;
		Vector& p2 = inVerts[i2].vWorldSpacePosition;
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	
			mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, outVerts[outCount].vWorldSpacePosition);
		
		outVerts[outCount].texCoord.x = inVerts[i].texCoord.x + dot*(inVerts[i2].texCoord.x - inVerts[i].texCoord.x);
		outVerts[outCount].texCoord.y = inVerts[i].texCoord.y + dot*(inVerts[i2].texCoord.y - inVerts[i].texCoord.y);
		
		++outCount;
	}

	return outCount;
}

// Returns true if clipping took place.
// Outputs two polygons: One for each side of the plane
void ProjectPortalPolyToPlane( PortalMeshPoint_t *pInVerts, int nVertCount, 
							   const Vector& normal, float flDist, const Vector &vCameraPos )
{
	for ( int i = 0; i < nVertCount; i++ )
	{
		// project point onto plane
		Vector vDir( pInVerts[i].vWorldSpacePosition - vCameraPos );
		float flT = ( flDist - DotProduct( vCameraPos, normal ) ) / DotProduct( vDir, normal );
		pInVerts[i].vWorldSpacePosition = vCameraPos + flT * vDir;

		/*
		double dirx, diry, dirz;
		dirx = pInVerts[i].vWorldSpacePosition.x - vCameraPos.x;
		diry = pInVerts[i].vWorldSpacePosition.y - vCameraPos.y;
		dirz = pInVerts[i].vWorldSpacePosition.z - vCameraPos.z;
		double dot1 =  vCameraPos.x * normal.x + vCameraPos.y * normal.y + vCameraPos.z * normal.z;
		double dot2 =  dirx * normal.x + diry * normal.y + dirz * normal.z;
		double flT2 = ( double( flDist ) - dot1 ) / dot2;
		pInVerts[i].vWorldSpacePosition.x = double( vCameraPos.x ) + flT2 * dirx;
		pInVerts[i].vWorldSpacePosition.y = double( vCameraPos.y ) + flT2 * diry;
		pInVerts[i].vWorldSpacePosition.z = double( vCameraPos.z ) + flT2 * dirz;
		*/
	}
}

// Clips a convex poly to a single plane
bool ClipPortalPolyToPlane( PortalMeshPoint_t *pInVerts, int nVertCount, 
						    PortalMeshPoint_t *pFrontVerts, int *pFrontVertCount,
						    const Vector& normal, float flDist )
{
	if ( nVertCount < 3 )
	{
		*pFrontVertCount = 0;
		return false;
	}
	
	float *dists = (float *)stackalloc( sizeof(float) * nVertCount );
	int *sides = (int *)stackalloc( sizeof(int) * nVertCount );
	int count[2] = { 0, 0 };

	// determine sides for each point
	for ( int i = 0; i < nVertCount; i++ )
	{
		float dot = DotProduct( pInVerts[i].vWorldSpacePosition, normal) - flDist;
		dists[i] = dot;
		sides[i] = ( dot >= 0.0f ) ? SIDE_FRONT : SIDE_BACK;
		count[ sides[i] ]++;
	}

	// no need to clip anything
	if ( count[0] == 0 )
	{
		*pFrontVertCount = 0;
		return false;
	}

	int	outCount1 = 0;
	for ( int i = 0; i < nVertCount; i++ )
	{
		int i2 = ( i + 1 ) % nVertCount;

		if ( sides[i] == SIDE_FRONT )
		{
			memcpy( &pFrontVerts[outCount1], &pInVerts[i], sizeof( PortalMeshPoint_t ) );
			++outCount1;

		}
		if ( sides[i2] == sides[i] )
		{
			continue;
		}

		// generate a split point
		float dot = dists[i] / ( dists[i] - dists[i2] );

		Vector& p1 = pInVerts[i].vWorldSpacePosition;
		Vector& p2 = pInVerts[i2].vWorldSpacePosition;
		Vector midPt = p1 + dot * ( p2 - p1 );

		Vector2D& uv1 = pInVerts[i].texCoord;
		Vector2D& uv2 = pInVerts[i2].texCoord;
		Vector2D midUv = uv1 + dot * ( uv2 - uv1 );

		VectorCopy( midPt, pFrontVerts[outCount1].vWorldSpacePosition );
		pFrontVerts[outCount1].texCoord = midUv;

		++outCount1;
	}

	*pFrontVertCount = outCount1;
	return true;
}


void RenderPortalMeshConvexPolygon( PortalMeshPoint_t *pVerts, int iVertCount, const IMaterial *pMaterial, void *pBind )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( (IMaterial *)pMaterial, pBind );

	//PortalMeshPoint_t *pMidVerts = (PortalMeshPoint_t *)stackalloc( sizeof( PortalMeshPoint_t ) * iVertCount );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, iVertCount - 2 );

	//any convex polygon can be rendered with a triangle strip by starting at a vertex and alternating vertices from each side
	int iForwardCounter = 0;
	int iReverseCounter = iVertCount - 1; //guaranteed to be >= 2 to start

	do
	{
		PortalMeshPoint_t *pVertex = &pVerts[iForwardCounter];
		meshBuilder.Position3fv( &pVertex->vWorldSpacePosition.x );
		meshBuilder.TexCoord2fv( 0, &pVertex->texCoord.x );
		meshBuilder.AdvanceVertex();
		++iForwardCounter;

		if( iForwardCounter > iReverseCounter )
			break;

		pVertex = &pVerts[iReverseCounter];
		meshBuilder.Position3fv( &pVertex->vWorldSpacePosition.x );
		meshBuilder.TexCoord2fv( 0, &pVertex->texCoord.x );
		meshBuilder.AdvanceVertex();
		--iReverseCounter;
	} while( iForwardCounter <= iReverseCounter );

	meshBuilder.End();
	pMesh->Draw();
}


void Clip_And_Render_Convex_Polygon( PortalMeshPoint_t *pVerts, int iVertCount, const IMaterial *pMaterial, void *pBind )
{
	PortalMeshPoint_t *pInVerts = (PortalMeshPoint_t *)stackalloc( iVertCount * 4 * sizeof( PortalMeshPoint_t ) ); //really only should need 2x points, but I'm paranoid
	PortalMeshPoint_t *pOutVerts = (PortalMeshPoint_t *)stackalloc( iVertCount * 4 * sizeof( PortalMeshPoint_t ) );
	PortalMeshPoint_t *pTempVerts;


	//clip by the viewing frustum
	{
		VPlane *pFrustum = view->GetFrustum();
		
		//clip by first plane and put output into pInVerts
		iVertCount = ClipPolyToPlane_LerpTexCoords( pVerts, iVertCount, pInVerts, pFrustum[0].m_Normal, pFrustum[0].m_Dist, 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 1; i != FRUSTUM_NUMPLANES; ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, pFrustum[i].m_Normal, pFrustum[i].m_Dist, 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw
	}

	CMatRenderContextPtr pRenderContext( materials );
	
	RenderPortalMeshConvexPolygon( pOutVerts, iVertCount, pMaterial, pBind );
	if( mat_wireframe.GetBool() )
		RenderPortalMeshConvexPolygon( pOutVerts, iVertCount, materials->FindMaterial( "shadertest/wireframe", TEXTURE_GROUP_CLIENT_EFFECTS, false ), pBind );

	stackfree( pOutVerts );
	stackfree( pInVerts );
}















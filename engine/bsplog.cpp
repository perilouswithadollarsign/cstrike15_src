//===== Copyright © Valve Corporation, All rights reserved. ========//
#include "platform.h"
#include "bsplog.h"
#include "qhConvex.h"
#include "cmodel.h"
#include "cmodel_private.h"
#include "utlhashtable.h"
#include "tier1/fmtstr.h"

CBspDebugLog::CBspDebugLog( const char *pName )
{
	m_nBaseVertex = 0;
	m_File = g_pFullFileSystem->Open( pName, "wt" );
	g_pFullFileSystem->FPrintf( m_File, "mtllib bsp_debug_log.mtl\n" );
	m_bFlush = false;
	m_nBrushCount = 0;
	m_nBoxCount = 0;
	/*
	newmtl initialShadingGroup
	illum 4
	Kd 0.50 0.50 0.50
	Ka 0.00 0.00 0.00
	Tf 1.00 1.00 1.00
	Ni 1.00
	newmtl start
	illum 4
	Kd 0.50 1.00 0.50
	Ka 0.00 0.00 0.00
	Tf 1.00 1.00 1.00
	Ni 1.00
	newmtl end
	illum 4
	Kd 0.50 0.50 1.00
	Ka 0.00 0.00 0.00
	Tf 1.00 1.00 1.00
	Ni 1.00
	newmtl irrelevant
	illum 4
	Kd 0.50 0.50 0.50
	Ka 0.00 0.00 0.00
	Tf 0.40 0.40 0.40
	Ni 1.00
	newmtl relevant
	illum 4
	Kd 0.75 0.75 0.75
	Ka 0.00 0.00 0.00
	Tf 1.00 1.00 1.00
	Ni 1.00
	*/
}



CBspDebugLog::~CBspDebugLog()
{
	g_pFullFileSystem->Close( m_File );
}



void CBspDebugLog::AddBox( const char *pName, const char *pMtl, const Vector &mins, const Vector &maxs )
{
	g_pFullFileSystem->FPrintf( m_File, "g default\nusemtl %s\n", pMtl );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", mins.x, mins.y, maxs.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", maxs.x, mins.y, maxs.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", mins.x, maxs.y, maxs.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", maxs.x, maxs.y, maxs.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", mins.x, maxs.y, mins.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", maxs.x, maxs.y, mins.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", mins.x, mins.y, mins.z );
	g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", maxs.x, mins.y, mins.z );
	int n = m_nBaseVertex;
	g_pFullFileSystem->FPrintf( m_File, "g %s\nusemtl %s\n", pName, pMtl );
	g_pFullFileSystem->FPrintf( m_File, "f %d %d %d %d\n", n + 1, n + 2, n + 4, n + 3 );
	g_pFullFileSystem->FPrintf( m_File, "f %d %d %d %d\n", n + 3, n + 4, n + 6, n + 5 );
	g_pFullFileSystem->FPrintf( m_File, "f %d %d %d %d\n", n + 5, n + 6, n + 8, n + 7 );
	g_pFullFileSystem->FPrintf( m_File, "f %d %d %d %d\n", n + 7, n + 8, n + 2, n + 1 );
	g_pFullFileSystem->FPrintf( m_File, "f %d %d %d %d\n", n + 2, n + 8, n + 6, n + 4 );
	g_pFullFileSystem->FPrintf( m_File, "f %d %d %d %d\n", n + 7, n + 1, n + 3, n + 5 );
	m_nBaseVertex += 8;
	g_pFullFileSystem->FPrintf( m_File, "s off\n" );
	if ( m_bFlush )																   
		g_pFullFileSystem->Flush( m_File );

	++m_nBoxCount;
}

void CBspDebugLog::AddBrush( const char *pName, const char *pMtl, cbrushside_t * RESTRICT pSides, int nSides )
{
	Vector vCenter = vec3_origin;
	float flMargin = -0.25f;
	float flPadding = -0.5f; // must be deeper than margin
	bool bFoundCenter = false;
	for ( int nAttempt = 0; nAttempt < 160; ++nAttempt )
	{
		bFoundCenter = true;
		for ( int nSide = 0; nSide < nSides; ++nSide )
		{
			const cplane_t &plane = *pSides[ nSide ].plane;
			float f = DotProduct( vCenter, plane.normal ) - plane.dist;
			if ( f < flMargin )
				continue;
			
			bFoundCenter = false;
			vCenter -= plane.normal * ( f - flPadding );
		}
		if ( bFoundCenter )
			break;
	}
	if ( !bFoundCenter )
	{
		Msg( "Cannot find brush %s convex hull", pName );
		return;
	}

	CUtlVector< qhVector3 > dualVerts;
	dualVerts.SetCount( nSides );
	for ( int i = 0; i < nSides; ++i )
	{
		const cplane_t &plane = *pSides[ i ].plane;
		
		float d = plane.dist - DotProduct( plane.normal, vCenter );
		Vector dv = plane.normal / d;
		dualVerts[ i ].X = dv.x;
		dualVerts[ i ].Y = dv.y;
		dualVerts[ i ].Z = dv.z;
	}

	qhConvex dualConvex;
	dualConvex.Construct( nSides, dualVerts.Base(), 0.00001f );

	g_pFullFileSystem->FPrintf( m_File, "g default\nusemtl %s\n", pMtl );

	CUtlHashtable< qhFace*, int > dualFaceToPrimalVertexIndex;


	CUtlVector< CUtlString > primalFaces;

	for ( const qhVertex *pDualVertex = dualConvex.GetVertexList().Begin(); pDualVertex != dualConvex.GetVertexList().End(); pDualVertex = pDualVertex->Next )
	{
		//Vector dualPos( dualVertex.Position.X, dualVertex.Position.Y, dualVertex.Position.Z );
		qhHalfEdge *pEdge = pDualVertex->Edge;
		CUtlString primalVerts;
		do 
		{
			qhFace *pFace = pEdge->Face;
			UtlHashHandle_t nFind = dualFaceToPrimalVertexIndex.Find( pFace );
			int nPrimalVert;
			if ( nFind == dualFaceToPrimalVertexIndex.InvalidHandle() )
			{
				Vector vPrimal = vCenter + Vector( pFace->Plane.Normal.X, pFace->Plane.Normal.Y, pFace->Plane.Normal.Z ) / pFace->Plane.Offset;
				g_pFullFileSystem->FPrintf( m_File, "v %f %f %f\n", vPrimal.x, vPrimal.y, vPrimal.z );
				dualFaceToPrimalVertexIndex.Insert( pFace, nPrimalVert = ++m_nBaseVertex );
			}
			else
			{
				nPrimalVert = dualFaceToPrimalVertexIndex.Element( nFind );
			}
			CUtlString pv;
			pv.Format( " %d", nPrimalVert );
			primalVerts = pv + primalVerts;

			pEdge = pEdge->Twin->Next;
		}
		while (pEdge != pDualVertex->Edge);
		primalFaces.AddToTail( primalVerts );
	}

	g_pFullFileSystem->FPrintf( m_File, "g %s\nusemtl %s\n", pName, pMtl );
	for ( int i = 0; i < primalFaces.Count(); ++i )
	{
		g_pFullFileSystem->FPrintf( m_File, "f%s\n", primalFaces[ i ].Get() );
	}
	
	if ( m_bFlush )
	{
		g_pFullFileSystem->Flush( m_File );
	}

	m_nBrushCount++;
}

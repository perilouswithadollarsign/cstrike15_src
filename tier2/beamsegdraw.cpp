//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "tier2/beamsegdraw.h"
#include "materialsystem/imaterialvar.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
// CBeamSegDraw implementation.
//
//-----------------------------------------------------------------------------
void CBeamSegDraw::Start( IMatRenderContext *pRenderContext, int nSegs, IMaterial *pMaterial, CMeshBuilder *pMeshBuilder, int nMeshVertCount )
{
	m_pRenderContext = pRenderContext;
	Assert( nSegs >= 2 );

	m_nSegsDrawn = 0;
	m_nTotalSegs = nSegs;

	m_pRenderContext->GetWorldSpaceCameraPosition( &m_vecCameraPos );

	if ( pMeshBuilder )
	{
		m_pMeshBuilder = pMeshBuilder;
		m_nMeshVertCount = nMeshVertCount;
	}
	else
	{
		m_pMeshBuilder = NULL;
		m_nMeshVertCount = 0;

		IMesh *pMesh = m_pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
		m_Mesh.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, (nSegs-1) * 2 );
	}
}

inline void CBeamSegDraw::ComputeNormal( const Vector &vecCameraPos, const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal )
{
	// vTangentY = line vector for beam
	Vector vTangentY;
	VectorSubtract( vStartPos, vNextPos, vTangentY );
	
	// vDirToBeam = vector from viewer origin to beam
	Vector vDirToBeam;
	VectorSubtract( vStartPos, vecCameraPos, vDirToBeam );

	// Get a vector that is perpendicular to us and perpendicular to the beam.
	// This is used to fatten the beam.
	CrossProduct( vTangentY, vDirToBeam, *pNormal );
	VectorNormalizeFast( *pNormal );
}

inline void CBeamSegDraw::SpecifySeg( const Vector &vecCameraPos, const Vector &vNormal )
{
	// SUCKY: Need to do a fair amount more work to get the tangent owing to the averaged normal
	Vector vDirToBeam, vTangentY;
	VectorSubtract( m_Seg.m_vPos, vecCameraPos, vDirToBeam );
	CrossProduct( vDirToBeam, vNormal, vTangentY );
	VectorNormalizeFast( vTangentY );

	// Build the endpoints.
	Vector vPoint1, vPoint2;
	VectorMA( m_Seg.m_vPos,  m_Seg.m_flWidth*0.5f, vNormal, vPoint1 );
	VectorMA( m_Seg.m_vPos, -m_Seg.m_flWidth*0.5f, vNormal, vPoint2 );

	if ( m_pMeshBuilder )
	{
		// Specify the points.
		m_pMeshBuilder->Position3fv( vPoint1.Base() );
		m_pMeshBuilder->Color4ubv( (const unsigned char *) &m_Seg.m_color );
		m_pMeshBuilder->TexCoord2f( 0, 0, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TexCoord2f( 1, 0, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TangentS3fv( vNormal.Base() );
		m_pMeshBuilder->TangentT3fv( vTangentY.Base() );
		m_pMeshBuilder->AdvanceVertex();
		
		m_pMeshBuilder->Position3fv( vPoint2.Base() );
		m_pMeshBuilder->Color4ubv( (const unsigned char *) &m_Seg.m_color );
		m_pMeshBuilder->TexCoord2f( 0, 1, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TexCoord2f( 1, 1, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TangentS3fv( vNormal.Base() );
		m_pMeshBuilder->TangentT3fv( vTangentY.Base() );
		m_pMeshBuilder->AdvanceVertex();

		if ( m_nSegsDrawn > 1 )
		{
			int nBase = ( ( m_nSegsDrawn - 2 ) * 2 ) + m_nMeshVertCount;

			m_pMeshBuilder->FastIndex( nBase );
			m_pMeshBuilder->FastIndex( nBase + 1 );
			m_pMeshBuilder->FastIndex( nBase + 2 );
			m_pMeshBuilder->FastIndex( nBase + 1 );
			m_pMeshBuilder->FastIndex( nBase + 3 );
			m_pMeshBuilder->FastIndex( nBase + 2 );
		}
	}
	else
	{
		// Specify the points.
		m_Mesh.Position3fv( vPoint1.Base() );
		m_Mesh.Color4ubv( (const unsigned char *)  &m_Seg.m_color );
		m_Mesh.TexCoord2f( 0, 0, m_Seg.m_flTexCoord );
		m_Mesh.TexCoord2f( 1, 0, m_Seg.m_flTexCoord );
		m_Mesh.TangentS3fv( vNormal.Base() );
		m_Mesh.TangentT3fv( vTangentY.Base() );
		m_Mesh.AdvanceVertex();
		
		m_Mesh.Position3fv( vPoint2.Base() );
		m_Mesh.Color4ubv( (const unsigned char *)  &m_Seg.m_color );
		m_Mesh.TexCoord2f( 0, 1, m_Seg.m_flTexCoord );
		m_Mesh.TexCoord2f( 1, 1, m_Seg.m_flTexCoord );
		m_Mesh.TangentS3fv( vNormal.Base() );
		m_Mesh.TangentT3fv( vTangentY.Base() );
		m_Mesh.AdvanceVertex();
	}
}

void CBeamSegDraw::NextSeg( BeamSeg_t *pSeg )
{
 	if ( m_nSegsDrawn > 0 )
	{
		// Get a vector that is perpendicular to us and perpendicular to the beam.
		// This is used to fatten the beam.
		Vector vNormal, vAveNormal;
		ComputeNormal( m_vecCameraPos, m_Seg.m_vPos, pSeg->m_vPos, &vNormal );

		if ( m_nSegsDrawn > 1 )
		{
			// Average this with the previous normal
			VectorAdd( vNormal, m_vNormalLast, vAveNormal );
			vAveNormal *= 0.5f;
			VectorNormalizeFast( vAveNormal );
		}
		else
		{
			vAveNormal = vNormal;
		}

		m_vNormalLast = vNormal;
		SpecifySeg( m_vecCameraPos, vAveNormal );
	}

	m_Seg = *pSeg;
	++m_nSegsDrawn;

 	if( m_nSegsDrawn == m_nTotalSegs )
	{
		SpecifySeg( m_vecCameraPos, m_vNormalLast );
	}
}

void CBeamSegDraw::LoadSIMDData( FourVectors * RESTRICT pV4StartPos, FourVectors * RESTRICT pV4EndPos, FourVectors * RESTRICT pV4HalfWidth, int nV4SegCount, const BeamSeg_t *  pSegs )
{
	const BeamSeg_t *RESTRICT pCurSeg = pSegs;
	for ( int i = 0; i < nV4SegCount; ++i, pCurSeg+= 4 )
	{
		pV4StartPos[i].LoadAndSwizzleAligned( pCurSeg[0].m_vPos, pCurSeg[1].m_vPos, pCurSeg[2].m_vPos, pCurSeg[3].m_vPos );
		pV4EndPos[i].LoadAndSwizzleAligned( pCurSeg[1].m_vPos, pCurSeg[2].m_vPos, pCurSeg[3].m_vPos, pCurSeg[4].m_vPos );
		// load and broadcast the halfwidths
		// if you load from values in memory (rather than computed values in registers),
		// .Load is faster.
		pV4HalfWidth[i].Load( pCurSeg[0].m_flWidth , pCurSeg[1].m_flWidth , pCurSeg[2].m_flWidth , pCurSeg[3].m_flWidth  );
		pV4HalfWidth[i] *= Four_PointFives;
	}
}


void CBeamSegDraw::ComputeRenderInfo( BeamSegRenderInfo_t * pRenderInfo, const Vector &vecCameraPos, int nSegCount, const BeamSeg_t *pSrcSegs ) RESTRICT
{
	int nPaddedSegCount = ( ( nSegCount + 3 ) >> 2 ) << 2;

	// FIXME: Can we figure out a way of avoiding this extra copy?
	// NOTE: We need an extra one since LoadSIMDData will load off the end
	BeamSeg_t *pSegs = (BeamSeg_t*)stackalloc( ( nPaddedSegCount + 1 ) * sizeof(BeamSeg_t) );
	memcpy( pSegs, pSrcSegs, nSegCount * sizeof(BeamSeg_t) );
	int nEndSegCount = ( nPaddedSegCount - nSegCount + 1 );
	BeamSeg_t endCap = pSrcSegs[nSegCount-1];
	endCap.m_vPos += pSrcSegs[nSegCount-1].m_vPos - pSrcSegs[nSegCount-2].m_vPos;
	for ( int i = 0; i < nEndSegCount; ++i )
	{
		memcpy( &pSegs[nSegCount+i], &endCap, sizeof(BeamSeg_t) );
	}

	FourVectors v4CameraPos;
	v4CameraPos.LoadAndSwizzle( vecCameraPos );

	int nV4SegCount = nPaddedSegCount >> 2;
	FourVectors *pV4StartPos = (FourVectors*)stackalloc( nV4SegCount * sizeof(FourVectors) );
	FourVectors *pV4EndPos = (FourVectors*)stackalloc( nV4SegCount * sizeof(FourVectors) );
	FourVectors *pV4HalfWidth = (FourVectors*)stackalloc( nV4SegCount * sizeof(FourVectors) );
	FourVectors *pV4AveNormals = (FourVectors*)stackalloc( ( nV4SegCount + 2 ) * sizeof(FourVectors) );
	CBeamSegDraw::LoadSIMDData( pV4StartPos, pV4EndPos, pV4HalfWidth, nV4SegCount, pSegs );

	fltx4 v4LMask =		{ 0.25f, 0.25f, 0.25f,  0.0f };
	fltx4 v4LPrevMask = {  0.0f,  0.0f,  0.0f, 0.25f };
	fltx4 v4RMask =		{  0.0f, 0.25f, 0.25f, 0.25f };
	fltx4 v4RNextMask = { 0.25f,  0.0f,  0.0f,  0.0f };
	fltx4 v4AveFactor = {  0.5f,  0.5f,  0.5f,  0.5f };

	// This is the only ones that need initial data are the first two
	memset( pV4AveNormals, 0, 2 * sizeof(FourVectors) );

	// Yes, that 1 is correct. We're going to write bogus crap on either end
	FourVectors eps( FLT_EPSILON );
	FourVectors *pV4CurAveNormal = &pV4AveNormals[1];
	for ( int i = 0; i < nV4SegCount; ++i, ++pV4CurAveNormal )
	{
		// prefetch
		PREFETCH360( pRenderInfo + (i << 2), 0 );
		PREFETCH360( pRenderInfo + (i << 2) + 1, 0 );
		FourVectors v4TangentY = pV4StartPos[i] - pV4EndPos[i];
		FourVectors v4CameraToStart = pV4StartPos[i] - v4CameraPos;
		FourVectors v4Normal = v4TangentY ^ v4CameraToStart;
		v4Normal += eps;
		v4Normal = VectorNormalizeFast( v4Normal );
		FourVectors v4LNormal = RotateLeft( v4Normal );
		FourVectors v4RNormal = RotateRight( v4Normal );
		pV4CurAveNormal[0] = Madd( v4Normal, v4AveFactor, pV4CurAveNormal[0] );
		pV4CurAveNormal[0] = Madd( v4LNormal, v4LMask, pV4CurAveNormal[0] );
		pV4CurAveNormal[0] = Madd( v4RNormal, v4RMask, pV4CurAveNormal[0] );
		pV4CurAveNormal[-1] = Madd( v4LNormal, v4LPrevMask, pV4CurAveNormal[-1] );
		pV4CurAveNormal[1] = Mul( v4RNormal, v4RNextMask );
	}

	// FIXME: Do I need to fixup the endpoints, to clamp their normals to the unsmoothed value?
	// Maybe I can get away with not doing that.
	FourVectors *pV4Normals	= &pV4AveNormals[1];
	FourVectors *pV4TangentY = (FourVectors*)stackalloc( nV4SegCount * sizeof(FourVectors) );
	FourVectors *pV4Point1 = (FourVectors*)stackalloc( nV4SegCount * sizeof(FourVectors) );
	FourVectors *pV4Point2 = (FourVectors*)stackalloc( nV4SegCount * sizeof(FourVectors) );
	for ( int i = 0; i < nV4SegCount; ++i )
	{
		// prefetch
		// (write top half rows)
		PREFETCH360( pRenderInfo + (i << 2) + 2, 0 );
		PREFETCH360( pRenderInfo + (i << 2) + 3, 0 );

		FourVectors v4Normal = VectorNormalizeFast( pV4Normals[i] );
		FourVectors v4CameraToStart = pV4StartPos[i] - v4CameraPos;
		FourVectors v4TangentY = v4CameraToStart ^ v4Normal;
		pV4Normals[i] = v4Normal;
		v4TangentY += eps;
		pV4TangentY[i] = VectorNormalizeFast( v4TangentY );
		FourVectors v4Offset = Mul( v4Normal, pV4HalfWidth[i] );
		pV4Point1[i] = pV4StartPos[i] + v4Offset;
		pV4Point2[i] = pV4StartPos[i] - v4Offset;
	}

	const BeamSeg_t * RESTRICT pSeg = pSrcSegs;
	// The code below has a few load-hit-stores (due to the
	// transform to vector for store). For an alternate 
	// pathway that does the same thing without the LHS,
	// see changelist 588032. It was more complicated, but
	// didn't profile to actually be any faster, so it
	// was taken back out.
	for ( int i = 0 ; i < nSegCount; ++i, ++pRenderInfo, ++pSeg )
	{
		int nIndex = ( i >> 2 );
		int j = ( i & 0x3 );
		pRenderInfo->m_vecCenter = pV4StartPos[nIndex].Vec( j ); 
		pRenderInfo->m_vecPoint1 = pV4Point1[nIndex].Vec( j );
		pRenderInfo->m_vecPoint2 = pV4Point2[nIndex].Vec( j );
		pRenderInfo->m_vecTangentS = pV4Normals[nIndex].Vec( j );
		pRenderInfo->m_vecTangentT = pV4TangentY[nIndex].Vec( j );
		pRenderInfo->m_flTexCoord = pSeg->m_flTexCoord;
		pRenderInfo->m_color = pSeg->m_color;
	}
}


void CBeamSegDraw::End()
{
	if ( m_pMeshBuilder )
	{
		m_pMeshBuilder = NULL;
		return;
	}

	m_Mesh.End( false, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSegDrawArbitrary::SetNormal( const Vector &normal )
{
	m_vNormalLast = normal;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSegDrawArbitrary::NextSeg( BeamSeg_t *pSeg )
{
	if ( m_nSegsDrawn > 0 )
	{
		Vector	segDir = ( m_PrevSeg.m_vPos - pSeg->m_vPos );
		VectorNormalize( segDir );

		Vector	normal = CrossProduct( segDir, m_vNormalLast );
		SpecifySeg( normal );
	}

	m_PrevSeg = m_Seg;
	m_Seg = *pSeg;
	++m_nSegsDrawn;
}


void CBeamSegDrawArbitrary::SpecifySeg( const Vector &vNormal )
{
	// Build the endpoints.
	Vector vPoint1, vPoint2;
	Vector vDelta;
	VectorMultiply( vNormal, m_Seg.m_flWidth*0.5f, vDelta );
	VectorAdd( m_Seg.m_vPos, vDelta, vPoint1 );
	VectorSubtract( m_Seg.m_vPos, vDelta, vPoint2 );

	m_Mesh.Position3fv( vPoint1.Base() );
	m_Mesh.Color4ub( m_Seg.m_color.r, m_Seg.m_color.g, m_Seg.m_color.b, m_Seg.m_color.a );
	m_Mesh.TexCoord2f( 0, 0, m_Seg.m_flTexCoord );
	m_Mesh.TexCoord2f( 1, 0, m_Seg.m_flTexCoord );
	m_Mesh.AdvanceVertex();
	
	m_Mesh.Position3fv( vPoint2.Base() );
	m_Mesh.Color4ub( m_Seg.m_color.r, m_Seg.m_color.g, m_Seg.m_color.b, m_Seg.m_color.a );
	m_Mesh.TexCoord2f( 0, 1, m_Seg.m_flTexCoord );
	m_Mesh.TexCoord2f( 1, 1, m_Seg.m_flTexCoord );
	m_Mesh.AdvanceVertex();
}

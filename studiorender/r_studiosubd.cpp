//===== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Support for mapping from a quad mesh to Bicubic Patches, as a means
//          of rendering approximate Catmull-Clark subdivision surfaces
//
//===========================================================================//

#include "studio.h"
#include "studiorendercontext.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "mathlib/mathlib.h"
#include "studiorender.h"
#include "optimize.h"
#include "tier1/convar.h"
#include "tier1/keyvalues.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define  R_STUDIOSUBD 
#include "r_studiosubd_patches.h"

#ifdef _DEBUG
// Temporary debug arrays
extern CUtlVector<Vector4D> g_DebugCornerPositions;
extern CUtlVector<Vector4D> g_DebugEdgePositions;
extern CUtlVector<Vector4D> g_DebugInteriorPositions;
#endif

//
//  Check out CL# 584588 for an SSE-ized version of the older versions of these
//	routines, which came from an older MS doc, by way of the DX10 SDK
//

static void R_TransformVert( const Vector *pSrcPos,	matrix3x4_t *pSkinMat, Vector4DAligned &pos )
{
	VPROF_BUDGET( "R_TransformVert", _T("SubD Rendering") );

	// NOTE: Could add SSE stuff here, if we knew what SSE stuff could make it faster
	pos.x  = pSrcPos->x  * (*pSkinMat)[0][0] + pSrcPos->y  * (*pSkinMat)[0][1] + pSrcPos->z  * (*pSkinMat)[0][2] + (*pSkinMat)[0][3];
	pos.y  = pSrcPos->x  * (*pSkinMat)[1][0] + pSrcPos->y  * (*pSkinMat)[1][1] + pSrcPos->z  * (*pSkinMat)[1][2] + (*pSkinMat)[1][3];
	pos.z  = pSrcPos->x  * (*pSkinMat)[2][0] + pSrcPos->y  * (*pSkinMat)[2][1] + pSrcPos->z  * (*pSkinMat)[2][2] + (*pSkinMat)[2][3];
	pos.w = 1.0f;
}


// This function is duplicate code ****
static matrix3x4_t *ComputeSkinMatrixSSE( mstudioboneweight_t &boneweights, matrix3x4_t *pPoseToWorld, matrix3x4_t &scratchMatrix )
{
	VPROF_BUDGET( "ComputeSkinMatrixSSE", _T("SubD Rendering") );

	// NOTE: pPoseToWorld, being cache aligned, doesn't need explicit initialization
#if defined( _WIN32 ) && !defined( WIN64 ) && !defined( _X360 ) 
	switch( boneweights.numbones )
	{
	default:
	case 1:
		return &pPoseToWorld[boneweights.bone[0]];

	case 2:
		{
			matrix3x4_t &boneMat0 = pPoseToWorld[boneweights.bone[0]];
			matrix3x4_t &boneMat1 = pPoseToWorld[boneweights.bone[1]];
			float *pWeights = boneweights.weight;

			_asm
			{
				mov		eax, DWORD PTR [pWeights]
				movss	xmm6, dword ptr[eax]		; boneweights.weight[0]
				movss	xmm7, dword ptr[eax + 4]	; boneweights.weight[1]

				mov		eax, DWORD PTR [boneMat0]
				mov		ecx, DWORD PTR [boneMat1]
				mov		edi, DWORD PTR [scratchMatrix]

				// Fill xmm6, and 7 with all the bone weights
				shufps	xmm6, xmm6, 0
				shufps	xmm7, xmm7, 0

				// Load up all rows of the three matrices
				movaps	xmm0, XMMWORD PTR [eax]
				movaps	xmm1, XMMWORD PTR [ecx]
				movaps	xmm2, XMMWORD PTR [eax + 16]
				movaps	xmm3, XMMWORD PTR [ecx + 16]
				movaps	xmm4, XMMWORD PTR [eax + 32]
				movaps	xmm5, XMMWORD PTR [ecx + 32]

				// Multiply the rows by the weights
				mulps	xmm0, xmm6
				mulps	xmm1, xmm7
				mulps	xmm2, xmm6
				mulps	xmm3, xmm7
				mulps	xmm4, xmm6
				mulps	xmm5, xmm7

				addps	xmm0, xmm1
				addps	xmm2, xmm3
				addps	xmm4, xmm5

				movaps	XMMWORD PTR [edi], xmm0
				movaps	XMMWORD PTR [edi + 16], xmm2
				movaps	XMMWORD PTR [edi + 32], xmm4
			}
		}
		return &scratchMatrix;

	case 3:
		{
			matrix3x4_t &boneMat0 = pPoseToWorld[boneweights.bone[0]];
			matrix3x4_t &boneMat1 = pPoseToWorld[boneweights.bone[1]];
			matrix3x4_t &boneMat2 = pPoseToWorld[boneweights.bone[2]];
			float *pWeights = boneweights.weight;

			_asm
			{
				mov		eax, DWORD PTR [pWeights]
				movss	xmm5, dword ptr[eax]		; boneweights.weight[0]
				movss	xmm6, dword ptr[eax + 4]	; boneweights.weight[1]
				movss	xmm7, dword ptr[eax + 8]	; boneweights.weight[2]

				mov		eax, DWORD PTR [boneMat0]
				mov		ecx, DWORD PTR [boneMat1]
				mov		edx, DWORD PTR [boneMat2]
				mov		edi, DWORD PTR [scratchMatrix]

				// Fill xmm5, 6, and 7 with all the bone weights
				shufps	xmm5, xmm5, 0
				shufps	xmm6, xmm6, 0
				shufps	xmm7, xmm7, 0

				// Load up the first row of the three matrices
				movaps	xmm0, XMMWORD PTR [eax]
				movaps	xmm1, XMMWORD PTR [ecx]
				movaps	xmm2, XMMWORD PTR [edx]

				// Multiply the rows by the weights
				mulps	xmm0, xmm5
				mulps	xmm1, xmm6
				mulps	xmm2, xmm7

				addps	xmm0, xmm1
				addps	xmm0, xmm2
				movaps	XMMWORD PTR [edi], xmm0

				// Load up the second row of the three matrices
				movaps	xmm0, XMMWORD PTR [eax + 16]
				movaps	xmm1, XMMWORD PTR [ecx + 16]
				movaps	xmm2, XMMWORD PTR [edx + 16]

				// Multiply the rows by the weights
				mulps	xmm0, xmm5
				mulps	xmm1, xmm6
				mulps	xmm2, xmm7

				addps	xmm0, xmm1
				addps	xmm0, xmm2
				movaps	XMMWORD PTR [edi + 16], xmm0	

				// Load up the third row of the three matrices
				movaps	xmm0, XMMWORD PTR [eax + 32]
				movaps	xmm1, XMMWORD PTR [ecx + 32]
				movaps	xmm2, XMMWORD PTR [edx + 32]

				// Multiply the rows by the weights
				mulps	xmm0, xmm5
				mulps	xmm1, xmm6
				mulps	xmm2, xmm7

				addps	xmm0, xmm1
				addps	xmm0, xmm2
				movaps	XMMWORD PTR [edi + 32], xmm0	
			}
		}
		return &scratchMatrix;

	case 4:
		{
			matrix3x4_t &boneMat0 = pPoseToWorld[boneweights.bone[0]];
			matrix3x4_t &boneMat1 = pPoseToWorld[boneweights.bone[1]];
			matrix3x4_t &boneMat2 = pPoseToWorld[boneweights.bone[2]];
			matrix3x4_t &boneMat3 = pPoseToWorld[boneweights.bone[3]];
			float *pWeights = boneweights.weight;

			_asm
			{
				mov		eax, DWORD PTR [pWeights]
				movss	xmm4, dword ptr[eax]		; boneweights.weight[0]
				movss	xmm5, dword ptr[eax + 4]	; boneweights.weight[1]
				movss	xmm6, dword ptr[eax + 8]	; boneweights.weight[2]
				movss	xmm7, dword ptr[eax + 12]	; boneweights.weight[3]

				mov		eax, DWORD PTR [boneMat0]
				mov		ecx, DWORD PTR [boneMat1]
				mov		edx, DWORD PTR [boneMat2]
				mov		esi, DWORD PTR [boneMat3]
				mov		edi, DWORD PTR [scratchMatrix]

				// Fill xmm5, 6, and 7 with all the bone weights
				shufps	xmm4, xmm4, 0
				shufps	xmm5, xmm5, 0
				shufps	xmm6, xmm6, 0
				shufps	xmm7, xmm7, 0

				// Load up the first row of the four matrices
				movaps	xmm0, XMMWORD PTR [eax]
				movaps	xmm1, XMMWORD PTR [ecx]
				movaps	xmm2, XMMWORD PTR [edx]
				movaps	xmm3, XMMWORD PTR [esi]

				// Multiply the rows by the weights
				mulps	xmm0, xmm4
				mulps	xmm1, xmm5
				mulps	xmm2, xmm6
				mulps	xmm3, xmm7

				addps	xmm0, xmm1
				addps	xmm2, xmm3
				addps	xmm0, xmm2
				movaps	XMMWORD PTR [edi], xmm0

				// Load up the second row of the three matrices
				movaps	xmm0, XMMWORD PTR [eax + 16]
				movaps	xmm1, XMMWORD PTR [ecx + 16]
				movaps	xmm2, XMMWORD PTR [edx + 16]
				movaps	xmm3, XMMWORD PTR [esi + 16]

				// Multiply the rows by the weights
				mulps	xmm0, xmm4
				mulps	xmm1, xmm5
				mulps	xmm2, xmm6
				mulps	xmm3, xmm7

				addps	xmm0, xmm1
				addps	xmm2, xmm3
				addps	xmm0, xmm2
				movaps	XMMWORD PTR [edi + 16], xmm0	

				// Load up the third row of the three matrices
				movaps	xmm0, XMMWORD PTR [eax + 32]
				movaps	xmm1, XMMWORD PTR [ecx + 32]
				movaps	xmm2, XMMWORD PTR [edx + 32]
				movaps	xmm3, XMMWORD PTR [esi + 32]

				// Multiply the rows by the weights
				mulps	xmm0, xmm4
				mulps	xmm1, xmm5
				mulps	xmm2, xmm6
				mulps	xmm3, xmm7

				addps	xmm0, xmm1
				addps	xmm2, xmm3
				addps	xmm0, xmm2
				movaps	XMMWORD PTR [edi + 32], xmm0	
			}
		}
		return &scratchMatrix;
	}
#else
#ifndef LINUX
	#pragma message( "ComputeSkinMatrixSSE C implementation only" )
#endif
	extern matrix3x4_t *ComputeSkinMatrix( mstudioboneweight_t &boneweights, matrix3x4_t *pPoseToWorld, matrix3x4_t &scratchMatrix );
	return ComputeSkinMatrix( boneweights, pPoseToWorld, scratchMatrix );
#endif

	Assert( 0 );
	return NULL;
}

#ifdef _DEBUG
static ConVar mat_tess_dump( "mat_tess_dump", "0", FCVAR_CHEAT );
#endif

void CStudioRender::SkinSubDCage( mstudiovertex_t *pVertices, int nNumVertices,
								  matrix3x4_t *pPoseToWorld, CCachedRenderData &vertexCache,
								  unsigned short* pGroupToMesh, fltx4 *vOutput, bool bDoFlex )
{
	VPROF_BUDGET( "CStudioRender::SkinSubDCage", _T("SubD Rendering") );

	Vector *pSrcPos;
	ALIGN16 matrix3x4_t *pSkinMat, temp ALIGN16_POST;

	Assert( nNumVertices > 0 );

	for ( int j=0; j < nNumVertices; ++j )
	{
		mstudiovertex_t &vert = pVertices[pGroupToMesh[j]];

		pSkinMat = ComputeSkinMatrixSSE( vert.m_BoneWeights, pPoseToWorld, temp );

		if ( bDoFlex && vertexCache.IsVertexFlexed( pGroupToMesh[j] ) )
		{
			CachedPosNormTan_t* pFlexedVertex = vertexCache.GetFlexVertex( pGroupToMesh[j] );
			pSrcPos = &pFlexedVertex->m_Position.AsVector3D();

			// Copy strange signed, 0..3 wrinkle tangent-flip encoding over to tangent.w
			pFlexedVertex->m_TangentS.w = pFlexedVertex->m_Position.w;
		}
		else // non-flexed case
		{
			pSrcPos = &vert.m_vecPosition;
		}

		// Transform into world space
		Vector4DAligned vTemp;
		R_TransformVert( pSrcPos, pSkinMat, *(Vector4DAligned*)&vTemp );
		vOutput[j] = LoadAlignedSIMD( (float *) &vTemp );
	}
}

inline unsigned short *InitializeTopologyIndexStruct( TopologyIndexStruct &quad, unsigned short *topologyIndex )
{
	quad.vtx1RingSize             = topologyIndex; topologyIndex += 4;
	quad.vtx1RingCenterQuadOffset = topologyIndex; topologyIndex += 4;
	quad.valences				  = topologyIndex; topologyIndex += 4;
	quad.minOneRingOffset		  = topologyIndex; topologyIndex += 4;
	quad.bndVtx					  = topologyIndex; topologyIndex += 4;
	quad.bndEdge				  = topologyIndex; topologyIndex += 4;
	quad.cornerVtx				  = topologyIndex; topologyIndex += 4;
	quad.loopGapAngle			  = topologyIndex; topologyIndex += 4;
	quad.nbCornerVtx			  = topologyIndex; topologyIndex += 4;
	quad.edgeBias				  = topologyIndex; topologyIndex += 8;
	quad.vUV0					  = topologyIndex; topologyIndex += 4;
	quad.vUV1					  = topologyIndex; topologyIndex += 4;
	quad.vUV2					  = topologyIndex; topologyIndex += 4;
	quad.vUV3					  = topologyIndex; topologyIndex += 4;
	quad.oneRing                  = topologyIndex; 
	topologyIndex += quad.vtx1RingSize[0]+quad.vtx1RingSize[1]+quad.vtx1RingSize[2]+quad.vtx1RingSize[3];

	return topologyIndex;
}

static ConVar mat_tessellation_update_buffers( "mat_tessellation_update_buffers", "1", FCVAR_CHEAT );
static ConVar mat_tessellation_cornertangents( "mat_tessellation_cornertangents", "1", FCVAR_CHEAT );
static ConVar mat_tessellation_accgeometrytangents( "mat_tessellation_accgeometrytangents", "0", FCVAR_CHEAT );

#ifdef _DEBUG

bool NotQuiteEqual( Vector4D &vA, Vector4D &vB )
{
	float flEpsilon = 0.05f;
	Vector4D vDelta = vA - vB;
	float flDist = sqrt( vDelta.x * vDelta.x + vDelta.y * vDelta.y + vDelta.z * vDelta.z );
	bool bSameVector = ( vA.x == vB.x ) && ( vA.y == vB.y ) && ( vA.z == vB.z );

	return ( flDist < flEpsilon ) && !bSameVector;
}

void DumpDebugPositions()
{

	for ( int i=0; i< g_DebugCornerPositions.Count(); i++ )
	{
		bool bCrack = false;
		for ( int j=0; j< g_DebugCornerPositions.Count(); j++ )
		{
			if ( NotQuiteEqual( g_DebugCornerPositions[i], g_DebugCornerPositions[j] ) )
			{
				bCrack = true;
				Assert(0);
			}
		}

		DevMsg( "%s C - %.15f, %.15f, %.15f\n", bCrack ? "*** " : "    ", g_DebugCornerPositions[i].x, g_DebugCornerPositions[i].y, g_DebugCornerPositions[i].z );
	}

	for ( int i=0; i< g_DebugEdgePositions.Count(); i++ )
	{
		bool bCrack = false;
		for ( int j=0; j< g_DebugEdgePositions.Count(); j++ )
		{
			if ( NotQuiteEqual( g_DebugEdgePositions[i], g_DebugEdgePositions[j] ) )
			{
				bCrack = true;
			}
		}

		DevMsg( "%s E - %.15f, %.15f, %.15f\n", bCrack ? "*** " : "    ", g_DebugEdgePositions[i].x, g_DebugEdgePositions[i].y, g_DebugEdgePositions[i].z );
	}

	for ( int i=0; i< g_DebugInteriorPositions.Count(); i++ )
	{
		bool bCrack = false;
		for ( int j=0; j< g_DebugInteriorPositions.Count(); j++ )
		{
			if ( NotQuiteEqual( g_DebugInteriorPositions[i], g_DebugInteriorPositions[j] ) )
			{
				bCrack = true;
			}
		}

		DevMsg( "%s I - %.15f, %.15f, %.15f\n", bCrack ? "*** " : "    ", g_DebugInteriorPositions[i].x, g_DebugInteriorPositions[i].y, g_DebugInteriorPositions[i].z );
	}
}

#endif // _DEBUG

void GenerateWorldSpacePatches( float *pSubDBuff, int nNumPatches, unsigned short *pTopologyIndices, fltx4 *pWSVertices, bool bRegularPatch )
{
	VPROF_BUDGET( "CStudioRender::GenerateWorldSpacePatches", _T("SubD Rendering") );

	TopologyIndexStruct quad;
	unsigned short *nextPatchIndices = InitializeTopologyIndexStruct( quad, pTopologyIndices );

	set_ShowACCGeometryTangents(mat_tessellation_accgeometrytangents.GetBool());
	set_UseCornerTangents(mat_tessellation_cornertangents.GetBool());

	ALIGN16 Vector4D Geo[16] ALIGN16_POST;
	ALIGN16 Vector4D TanU[12] ALIGN16_POST;
	ALIGN16 Vector4D TanV[12] ALIGN16_POST;

#ifdef _DEBUG
	if ( mat_tess_dump.GetBool() )
	{
		// Debug Arrays
		g_DebugCornerPositions.EnsureCapacity( nNumPatches * 4 );
		g_DebugEdgePositions.EnsureCapacity( nNumPatches * 8 );
		g_DebugInteriorPositions.EnsureCapacity( nNumPatches * 4 );

		// Empty the arrays this time around
		g_DebugCornerPositions.RemoveAll();
		g_DebugEdgePositions.RemoveAll();
		g_DebugInteriorPositions.RemoveAll();
	}
#endif

	for( int p = 0; p < nNumPatches; p++ )
	{
#if defined( USE_OPT )
		ComputeACCAllPatches( pWSVertices, &quad, Geo, TanU, TanV, bRegularPatch );
#else
		ComputeACCGeometryPatch( pWSVertices, &quad, Geo );
		ComputeACCTangentPatches( pWSVertices, &quad, Geo, TanU, TanV );
#endif

		for ( int i=0; i < 16; i++ )
		{
			pSubDBuff[ i * 3 + 0 ] = Geo[i].x;
			pSubDBuff[ i * 3 + 1 ] = Geo[i].y;
			pSubDBuff[ i * 3 + 2 ] = Geo[i].z;

		}

		for ( int i=0; i<12; i++ )
		{
			pSubDBuff[ i * 3 + 0 + 48 ] = TanU[ i ].x;
			pSubDBuff[ i * 3 + 1 + 48 ] = TanU[ i ].y;
			pSubDBuff[ i * 3 + 2 + 48 ] = TanU[ i ].z;
		}

		for ( int i=0; i<12; i++ )
		{
			pSubDBuff[ i * 3 + 0 + 84 ] = TanV[ i ].x;
			pSubDBuff[ i * 3 + 1 + 84 ] = TanV[ i ].y;
			pSubDBuff[ i * 3 + 2 + 84 ] = TanV[ i ].z;
		}

		pSubDBuff += 120; // 30 * sizeof( float )

		nextPatchIndices = InitializeTopologyIndexStruct( quad, nextPatchIndices );
	}

#ifdef _DEBUG
	if ( mat_tess_dump.GetBool() )
	{
		// These should be a particular size
		Assert( g_DebugCornerPositions.Count() == ( nNumPatches * 4 ) );
		Assert( g_DebugEdgePositions.Count() == ( nNumPatches * 8 ) );
		Assert( g_DebugInteriorPositions.Count() == ( nNumPatches * 4 ) );

		DumpDebugPositions();
		mat_tess_dump.SetValue( 0 );		// Turn back off
	}
#endif

}

//-----------------------------------------------------------------------------------
// Top level function for mapping a quad mesh to an array of Bicubic Bezier patches
//-----------------------------------------------------------------------------------
void CStudioRender::GenerateBicubicPatches( mstudiomesh_t* pmesh, studiomeshgroup_t* pGroup, bool bDoFlex )
{
#if defined( LINUX )
  	Assert(0);
#else
	VPROF_BUDGET( "CStudioRender::GenerateBicubicPatches", _T("SubD Rendering") );

	FillTables(); // This only does work the first time through

	Assert( pmesh );
	Assert( pGroup );

	const mstudio_meshvertexdata_t *vertData = pmesh->GetVertexData( m_pStudioHdr );
	Assert( vertData );

	mstudiovertex_t *pVertices = vertData->Vertex( 0 );

	m_vSkinnedSubDVertices.SetCount( pGroup->m_NumVertices );

	// First, apply software flexing and skinning to the vertices
	SkinSubDCage( pVertices, pGroup->m_NumVertices, m_PoseToWorld,
				  m_VertexCache, pGroup->m_pGroupIndexToMeshIndex, m_vSkinnedSubDVertices.Base(), bDoFlex );

	// Early out
	if ( mat_tessellation_update_buffers.GetBool() == false )
		return;

	// Lock the subd buffers
	int nNumPatches = 0;
	for ( int s=0; s<pGroup->m_NumStrips; ++s )
	{
		nNumPatches += pGroup->m_pUniqueFaces[s];
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	float *pSubDBuff = pRenderContext->LockSubDBuffer( nNumPatches );

	// Now we are in world space, we can map to array of Bicubic patches
	int totalIndices = 0;
	float *pCurrentPtr = pSubDBuff;
	for ( int s=0; s<pGroup->m_NumStrips; ++s )
	{
		OptimizedModel::StripHeader_t *pStrip = &pGroup->m_pStripData[s];
		int StripFaces = pGroup->m_pUniqueFaces[s];

		GenerateWorldSpacePatches( pCurrentPtr, StripFaces, &pGroup->m_pTopologyIndices[totalIndices], m_vSkinnedSubDVertices.Base(), ( pStrip->flags & OptimizedModel::STRIP_IS_QUADLIST_REG ) != 0 );

		totalIndices += pStrip->numTopologyIndices;
		pCurrentPtr += StripFaces * 120;
	}

	// Unlock subd buffers
	pRenderContext->UnlockSubDBuffer( );

#endif // !LINUX
}


// Transform Tangent vector
static void R_TransformTangent( const Vector4D *pSrcTangentS, matrix3x4_t *pSkinMat, Vector4DAligned &tangentS )
{
	VPROF_BUDGET( "R_TransformTangent", _T("SubD Rendering") );

	tangentS.x = pSrcTangentS->x * (*pSkinMat)[0][0] + pSrcTangentS->y * (*pSkinMat)[0][1]	+ pSrcTangentS->z * (*pSkinMat)[0][2];
	tangentS.y = pSrcTangentS->x * (*pSkinMat)[1][0] + pSrcTangentS->y * (*pSkinMat)[1][1]	+ pSrcTangentS->z * (*pSkinMat)[1][2];
	tangentS.z = pSrcTangentS->x * (*pSkinMat)[2][0] + pSrcTangentS->y * (*pSkinMat)[2][1]	+ pSrcTangentS->z * (*pSkinMat)[2][2];
	tangentS.w = pSrcTangentS->w;
}

// Transforms per-vertex tangent vector, copies texture coordinates etc into dynamic VB
void CStudioRender::SoftwareProcessQuadMesh( mstudiomesh_t* pmesh, CMeshBuilder& meshBuilder, 
											 int numFaces, unsigned short* pGroupToMesh,
											 unsigned short *pTopologyIndices, bool bTangentSpace, bool bDoFlex )
{
	VPROF_BUDGET( "CStudioRender::SoftwareProcessQuadMesh", _T("SubD Rendering") );

	Vector4D *pStudioTangentS = NULL;

	ALIGN16 QuadTessVertex_t quadVertex ALIGN16_POST;

	// QuadTessVertex_t currently has the following map:
	// +-----------------------------------+
	// |  tanX  |  tanY  |  tanZ  | sBWrnk | <- Tangent in .xyz, Binormal sign flip bit plus wrinkle in .w
	// +-----------------------------------+
	// |  tcU0  |  tcV0  |  tcU1  |  tcV1  | <- Interior TC, Parametric V Edge TC
	// +-----------------------------------+
	// |  tcU2  |  tcV2  |  tcU3  |  tcV3  | <- Parametric U Edge TC, Corner TC
	// +-----------------------------------+

	quadVertex.m_vTangent.Init( 1.0f, 0.0f, 0.0f, 1.0f );

	ALIGN16 matrix3x4_t *pSkinMat, matTemp ALIGN16_POST;

	Assert( numFaces > 0 );

	const mstudio_meshvertexdata_t *pVertData = pmesh->GetVertexData( m_pStudioHdr );
	Assert( pVertData );
	if ( !pVertData )
		return;

	mstudiovertex_t *pVertices = pVertData->Vertex( 0 );


	if ( bTangentSpace )
	{
		pStudioTangentS = pVertData->TangentS( 0 );
	}

	TopologyIndexStruct quad;
	unsigned short *nextPatchIndices = InitializeTopologyIndexStruct( quad, pTopologyIndices );

	for ( int i=0; i < numFaces; ++i )						// Run over faces
	{
		int patchCorner = 0;

#if 0
		Vector4D debugTangent[4];
		for ( int j=0; j < 4; ++j )
		{
			int idx = quad.oneRing[patchCorner];
			memcpy( &debugTangent[j], &pStudioTangentS[idx], sizeof( Vector4D ) );
			patchCorner += quad.vtx1RingSize[j];
		}

		// These should be the same sign for a given patch.
		// If they're not, that's bad
		Assert( ( debugTangent[0].w == debugTangent[1].w ) &&
				( debugTangent[1].w == debugTangent[2].w ) &&
				( debugTangent[2].w == debugTangent[3].w ) );

		patchCorner = 0;
#endif

		for ( int j=0; j < 4; ++j )							// Four verts per face
		{
			int idx = quad.oneRing[patchCorner];
			mstudiovertex_t &vert = pVertices[idx];

			if ( bTangentSpace )
			{
				pSkinMat = ComputeSkinMatrixSSE( vert.m_BoneWeights, m_PoseToWorld, matTemp );

				if ( bDoFlex && m_VertexCache.IsVertexFlexed( idx ) )
				{
					CachedPosNormTan_t* pFlexedVertex = m_VertexCache.GetFlexVertex( idx );
					R_TransformTangent( &(pFlexedVertex->m_TangentS), pSkinMat, *(Vector4DAligned*)&quadVertex.m_vTangent );
				}
				else // non-flexed case
				{
					R_TransformTangent( &pStudioTangentS[idx], pSkinMat, *(Vector4DAligned*)&quadVertex.m_vTangent );
					quadVertex.m_vTangent.w *= 2; // non-flexed vertex should have wrinkle of -2 or +2
				}
			}

			// Store 4 texcoords per quad corner
			quadVertex.m_vUV01.x = pVertices[ quad.vUV0[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV01.y = pVertices[ quad.vUV0[j] ].m_vecTexCoord.y;
			quadVertex.m_vUV01.z = pVertices[ quad.vUV1[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV01.w = pVertices[ quad.vUV1[j] ].m_vecTexCoord.y;
			quadVertex.m_vUV23.x = pVertices[ quad.vUV2[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV23.y = pVertices[ quad.vUV2[j] ].m_vecTexCoord.y;
			quadVertex.m_vUV23.z = pVertices[ quad.vUV3[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV23.w = pVertices[ quad.vUV3[j] ].m_vecTexCoord.y;

			meshBuilder.FastQuadVertexSSE( quadVertex );

			patchCorner += quad.vtx1RingSize[j];
		}

		nextPatchIndices = InitializeTopologyIndexStruct( quad, nextPatchIndices );
	}

	meshBuilder.FastAdvanceNVertices( numFaces * 4 );
}

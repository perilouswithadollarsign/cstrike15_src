//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "render_pch.h"
#include "modelloader.h"
#include "gl_model_private.h"
#include "gl_lightmap.h"
#include "disp.h"
#include "mathlib/mathlib.h"
#include "gl_rsurf.h"
#include "gl_matsysiface.h"
#include "zone.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ivballoctracker.h"
#include "mathlib/vector.h"
#include "iscratchpad3d.h"
#include "tier0/fasttimer.h"
#include "lowpassstream.h"
#include "con_nprint.h"
#include "tier2/tier2.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void BuildTagData( CCoreDispInfo *pCoreDisp, CDispInfo *pDisp );
void SmoothDispSurfNormals( CCoreDispInfo **ppListBase, int nListSize );

// This makes sure that whoever is creating and deleting CDispInfos frees them all
// (and calls their destructors) before the module is gone.
class CConstructorChecker
{
public:
			CConstructorChecker()	{m_nConstructedObjects = 0;}
			~CConstructorChecker()	{Assert(m_nConstructedObjects == 0);}
	int		m_nConstructedObjects;
} g_ConstructorChecker;
 


//-----------------------------------------------------------------------------
// Static helpers.
//-----------------------------------------------------------------------------

static void BuildDispGetSurfNormals( Vector points[4], Vector normals[4] )
{
	//
	// calculate the displacement surface normal
	//
	Vector tmp[2];
	Vector normal;
	tmp[0] = points[1] - points[0];
	tmp[1] = points[3] - points[0];
	normal = tmp[1].Cross( tmp[0] );
	VectorNormalize( normal );

	for( int i = 0; i < 4; i++ )
	{
		normals[i] = normal;
	}
}


static bool FindExtraDependency( unsigned short *pDependencies, int nDependencies, int iDisp )
{
	for( int i=0; i < nDependencies; i++ )
	{
		if ( pDependencies[i] == iDisp )
			return true;
	}
	return false;
}


static CDispGroup* FindCombo( CUtlVector<CDispGroup*> &combos, int idLMPage, IMaterial *pMaterial )
{
	for( int i=0; i < combos.Count(); i++ )
	{
		if( combos[i]->m_LightmapPageID == idLMPage && combos[i]->m_pMaterial == pMaterial )
			return combos[i];
	}
	return NULL;
}


static CDispGroup* AddCombo( CUtlVector<CDispGroup*> &combos, int idLMPage, IMaterial *pMaterial )
{
	CDispGroup *pCombo = new CDispGroup;
	pCombo->m_LightmapPageID = idLMPage;
	pCombo->m_pMaterial = pMaterial;
	pCombo->m_nVisible = 0;
	combos.AddToTail( pCombo );
	return pCombo;
}


static inline CDispInfo* GetModelDisp( model_t const *pWorld, int i )
{
	return static_cast< CDispInfo* >(
		DispInfo_IndexArray( pWorld->brush.pShared->hDispInfos, i ) );
}			


static void BuildDispSurfInit( 
	model_t *pWorld, 
	CCoreDispInfo *pBuildDisp,
	SurfaceHandle_t worldSurfID )
{
	if( !IS_SURF_VALID( worldSurfID ) )
		return;
	ASSERT_SURF_VALID( worldSurfID );
	
	Vector surfPoints[4];
	Vector surfNormals[4];
	Vector2D surfTexCoords[4];
	Vector2D surfLightCoords[4][4];

	if ( MSurf_VertCount( worldSurfID ) != 4 )
		return;

#ifndef DEDICATED
	BuildMSurfaceVerts( pWorld->brush.pShared, worldSurfID, surfPoints, surfTexCoords, surfLightCoords );
#endif
	BuildDispGetSurfNormals( surfPoints, surfNormals );

	CCoreDispSurface *pDispSurf = pBuildDisp->GetSurface();

	int surfFlag = pDispSurf->GetFlags();
	int nLMVects = 1;
	if( MSurf_Flags( worldSurfID ) & SURFDRAW_BUMPLIGHT )
	{
		surfFlag |= CCoreDispInfo::SURF_BUMPED;
		nLMVects = NUM_BUMP_VECTS + 1;
	}

	pDispSurf->SetPointCount( 4 );
	for( int i = 0; i < 4; i++ )
	{
		pDispSurf->SetPoint( i, surfPoints[i] );
		pDispSurf->SetPointNormal( i, surfNormals[i] );
		pDispSurf->SetTexCoord( i, surfTexCoords[i] );

		for( int j = 0; j < nLMVects; j++ )
		{
			pDispSurf->SetLuxelCoord( j, i, surfLightCoords[i][j] );
		}
	}

	Vector vecS = MSurf_TexInfo( worldSurfID )->textureVecsTexelsPerWorldUnits[0].AsVector3D();
	Vector vecT = MSurf_TexInfo( worldSurfID )->textureVecsTexelsPerWorldUnits[1].AsVector3D();
	VectorNormalize( vecS );
	VectorNormalize( vecT );
	pDispSurf->SetSAxis( vecS );
	pDispSurf->SetTAxis( vecT );

	pDispSurf->SetFlags( surfFlag );
	pDispSurf->FindSurfPointStartIndex();
	pDispSurf->AdjustSurfPointData();

#ifndef DEDICATED
	//
	// adjust the lightmap coordinates -- this is currently done redundantly!
	// the will be fixed correctly when the displacement common code is written.
	// This is here to get things running for (GDC, E3)
	//
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, worldSurfID );
	int lightmapWidth = MSurf_LightmapExtents( worldSurfID )[0];
	int lightmapHeight = MSurf_LightmapExtents( worldSurfID )[1];

	Vector2D uv( 0.0f, 0.0f );
	for ( int ndxLuxel = 0; ndxLuxel < 4; ndxLuxel++ )
	{
		switch( ndxLuxel )
		{
		case 0: { uv.Init( 0.0f, 0.0f ); break; }
		case 1: { uv.Init( 0.0f, ( float )lightmapHeight ); break; }
		case 2: { uv.Init( ( float )lightmapWidth, ( float )lightmapHeight ); break; }
		case 3: { uv.Init( ( float )lightmapWidth, 0.0f ); break; }
		}
		
		uv.x += 0.5f;
		uv.y += 0.5f;
		
		uv *= ctx.m_Scale; 
		uv += ctx.m_Offset; 
		
		pDispSurf->SetLuxelCoord( 0, ndxLuxel, uv );
	}
#endif
}

VertexFormat_t ComputeDisplacementStaticMeshVertexFormat( const IMaterial * pMaterial, const CDispGroup *pCombo, const ddispinfo_t *pMapDisps )
{
	VertexFormat_t vertexFormat = pMaterial->GetVertexFormat();

	// FIXME: set VERTEX_FORMAT_COMPRESSED if there are no artifacts and if it saves enough memory (use 'mem_dumpvballocs')
	vertexFormat &= ~VERTEX_FORMAT_COMPRESSED;
	// FIXME: check for and strip unused vertex elements (TANGENT_S/T?)

	return vertexFormat;
}

void AddEmptyMesh( 
	model_t *pWorld,
	CDispGroup *pCombo, 
	const ddispinfo_t *pMapDisps,
	int *pDispInfos,
	int nDisps,
	int nTotalVerts, 
	int nTotalIndices )
{
	CMatRenderContextPtr pRenderContext( materials );

	CGroupMesh *pMesh = new CGroupMesh;
	pCombo->m_Meshes.AddToTail( pMesh );

	VertexFormat_t vertexFormat = ComputeDisplacementStaticMeshVertexFormat( pCombo->m_pMaterial, pCombo, pMapDisps );
	pMesh->m_pMesh = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_DISP );
	pMesh->m_pGroup = pCombo;
	pMesh->m_nVisible = 0;

	CMeshBuilder builder;
	builder.Begin( pMesh->m_pMesh, MATERIAL_TRIANGLES, nTotalVerts, nTotalIndices );

		// Just advance the verts and indices and leave the data blank for now.
		builder.AdvanceIndices( nTotalIndices );
		builder.AdvanceVertices( nTotalVerts );

	builder.End();


	pMesh->m_DispInfos.SetSize( nDisps );
	pMesh->m_Visible.SetSize( nDisps );
	pMesh->m_VisibleDisps.SetSize( nDisps );

	int iVertOffset = 0;
	int iIndexOffset = 0;
	for( int iDisp=0; iDisp < nDisps; iDisp++ )
	{
		CDispInfo *pDisp = GetModelDisp( pWorld, pDispInfos[iDisp] );
		const ddispinfo_t *pMapDisp = &pMapDisps[ pDispInfos[iDisp] ];

		pDisp->m_pMesh = pMesh;
		pDisp->m_iVertOffset = iVertOffset;
		pDisp->m_iIndexOffset = iIndexOffset;

		int nVerts, nIndices;
		CalcMaxNumVertsAndIndices( pMapDisp->power, &nVerts, &nIndices );
		iVertOffset += nVerts;
		iIndexOffset += nIndices;
		
		pMesh->m_DispInfos[iDisp] = pDisp;
	}

	Assert( iVertOffset == nTotalVerts );
	Assert( iIndexOffset == nTotalIndices );
}


void FillStaticBuffer( 
	CGroupMesh *pMesh,
	CDispInfo *pDisp,
	const CCoreDispInfo *pCoreDisp,
	const CDispVert *pVerts,
	int nLightmaps )
{
#ifndef DEDICATED
	// Put the verts into the buffer.
	int nVerts, nIndices;
	CalcMaxNumVertsAndIndices( pDisp->GetPower(), &nVerts, &nIndices );
	
	CMeshBuilder builder;
	builder.BeginModify( pMesh->m_pMesh, pDisp->m_iVertOffset, nVerts, 0, 0 );
	
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, pDisp->GetParent() );
	
	for( int i=0; i < nVerts; i++ )
	{
		// NOTE: position comes from our system-memory buffer so when you're restoring
		//       static buffers (from alt+tab), it includes changes from terrain mods.
		const Vector &vPos = pCoreDisp->GetVert( i );
		builder.Position3f( vPos.x, vPos.y, vPos.z );
		
		const Vector &vNormal = pCoreDisp->GetNormal( i );
		builder.Normal3f( vNormal.x, vNormal.y, vNormal.z );
		
		Vector vec;
		pCoreDisp->GetTangentS( i, vec );
		builder.TangentS3f( VectorExpand( vec ) );
		
		pCoreDisp->GetTangentT( i, vec );
		builder.TangentT3f( VectorExpand( vec ) );
		
		Vector2D texCoord;
		pCoreDisp->GetTexCoord( i, texCoord );
		builder.TexCoord2f( 0, texCoord.x, texCoord.y );
		
		Vector2D lightCoord;
		{
			pCoreDisp->GetLuxelCoord( 0, i, lightCoord );
			builder.TexCoord2f( DISP_LMCOORDS_STAGE, lightCoord.x, lightCoord.y );
		}
		
		if ( ( pCoreDisp->GetFlags() & DISP_INFO_FLAG_HAS_MULTIBLEND ) != 0 )
		{
			Vector4D	vMultiBlend, vAlphaBlend;
			Vector		vMultiBlendColor[ MAX_MULTIBLEND_CHANNELS ];
				
			pCoreDisp->GetMultiBlend( i, vMultiBlend, vAlphaBlend, vMultiBlendColor[ 0 ], vMultiBlendColor[ 1 ], vMultiBlendColor[ 2 ], vMultiBlendColor[ 3 ] );
			builder.TexCoord4fv( DISP_MULTIBLEND_STAGE, vAlphaBlend.Base() );

			for( int i = 0; i < MAX_MULTIBLEND_CHANNELS; i++ )
			{
				builder.TexCoord4f( DISP_MULTIBLEND_STAGE + i + 1, vMultiBlendColor[ i ].x, vMultiBlendColor[ i ].y, vMultiBlendColor[ i ].z, vMultiBlend.Base()[ i ] );
			}

			builder.Specular4fv( vMultiBlend.Base() ); 
		}

		float flAlpha = ( ( CCoreDispInfo * )pCoreDisp )->GetAlpha( i );
		flAlpha *= ( 1.0f / 255.0f );
		flAlpha = clamp( flAlpha, 0.0f, 1.0f );
		builder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );

		if( nLightmaps > 1 )
		{
			SurfComputeLightmapCoordinate( ctx, pDisp->GetParent(), pDisp->m_Verts[i].m_vPos, lightCoord );
			builder.TexCoord2f( 2, ctx.m_BumpSTexCoordOffset, 0.0f );
		}
		
		builder.AdvanceVertex();
	}
	
	builder.EndModify();
#endif
}


void CDispInfo::CopyMapDispData( const ddispinfo_t *pBuildDisp )
{
	m_iLightmapAlphaStart = pBuildDisp->m_iLightmapAlphaStart;
	m_Power = pBuildDisp->power;

	Assert( m_Power >= 2 && m_Power <= NUM_POWERINFOS );
	m_pPowerInfo = ::GetPowerInfo( m_Power );

	// Max # of indices:
	// Take the number of triangles (2 * (size-1) * (size-1))
	// and multiply by 3!
	// These can be non-null in the case of task switch restore
	int size = GetSideLength();
	m_Indices.SetSize( 6 * (size-1) * (size-1) );

	// Per-node information
	if (m_pNodeInfo)
		delete[] m_pNodeInfo;

	m_pNodeInfo = new DispNodeInfo_t[m_pPowerInfo->m_NodeCount];
}


void DispInfo_CreateMaterialGroups( model_t *pWorld, const MaterialSystem_SortInfo_t *pSortInfos )
{
	for ( int iDisp=0; iDisp < pWorld->brush.pShared->numDispInfos; iDisp++ )
	{
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );

		int idLMPage = pSortInfos[MSurf_MaterialSortID( pDisp->m_ParentSurfID )].lightmapPageID;
		
		CDispGroup *pCombo = FindCombo( g_DispGroups, idLMPage, MSurf_TexInfo( pDisp->m_ParentSurfID )->material );
		if( !pCombo )
			pCombo = AddCombo( g_DispGroups, idLMPage, MSurf_TexInfo( pDisp->m_ParentSurfID )->material );

		MEM_ALLOC_CREDIT();
		pCombo->m_DispInfos.AddToTail( iDisp );
	}
}


void DispInfo_LinkToParentFaces( model_t *pWorld, const ddispinfo_t *pMapDisps, int nDisplacements )
{
	for ( int iDisp=0; iDisp < nDisplacements; iDisp++ )
	{
		const ddispinfo_t *pMapDisp = &pMapDisps[iDisp];
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );

		// Set its parent.
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( pMapDisp->m_iMapFace );
		Assert( pMapDisp->m_iMapFace < pWorld->brush.pShared->numsurfaces );
		Assert( MSurf_Flags( surfID ) & SURFDRAW_HAS_DISP );
		surfID->pDispInfo = pDisp;
		pDisp->SetParent( surfID );
	}
}


void DispInfo_CreateEmptyStaticBuffers( model_t *pWorld, const ddispinfo_t *pMapDisps )
{
	// For each combo, create empty buffers.
	for( int i=0; i < g_DispGroups.Count(); i++ )
	{
		CDispGroup *pCombo = g_DispGroups[i];

		int nTotalVerts=0, nTotalIndices=0;
		int iStart = 0;
		for( int iDisp=0; iDisp < pCombo->m_DispInfos.Count(); iDisp++ )
		{
			const ddispinfo_t *pMapDisp = &pMapDisps[pCombo->m_DispInfos[iDisp]];

			int nVerts, nIndices;
			CalcMaxNumVertsAndIndices( pMapDisp->power, &nVerts, &nIndices );
			
			// If we're going to pass our vertex buffer limit, or we're at the last one,
			// make a static buffer and fill it up.
			if( (nTotalVerts + nVerts) > MAX_STATIC_BUFFER_VERTS || 
				(nTotalIndices + nIndices) > MAX_STATIC_BUFFER_INDICES )
			{
				AddEmptyMesh( pWorld, pCombo, pMapDisps, &pCombo->m_DispInfos[iStart], iDisp-iStart, nTotalVerts, nTotalIndices );
				Assert( nTotalVerts > 0 && nTotalIndices > 0 );

				nTotalVerts = nTotalIndices = 0;
				iStart = iDisp;
				--iDisp;
			}
			else if( iDisp == pCombo->m_DispInfos.Count()-1 )
			{
				AddEmptyMesh( pWorld, pCombo, pMapDisps, &pCombo->m_DispInfos[iStart], iDisp-iStart+1, nTotalVerts+nVerts, nTotalIndices+nIndices );
				break;
			}
			else
			{
				nTotalVerts += nVerts;
				nTotalIndices += nIndices;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWorld - 
//			iDisp - 
//			*pMapDisp - 
//			*pCoreDisp - 
//			*pVerts - 
//			pWorld - 
//			iDisp - 
// Output : Returns true on success, false on failure.
// Information: Setup the CCoreDispInfo using the ddispinfo_t and have it translate the data
// into a format we'll copy into the rendering structures. This roundaboutness is because
// of legacy code. It should all just be stored in the map file, but it's not a high priority right now.
//-----------------------------------------------------------------------------
bool DispInfo_CreateFromMapDisp( model_t *pWorld, int iDisp, const ddispinfo_t *pMapDisp, CCoreDispInfo *pCoreDisp, const CDispVert *pVerts,
								 const CDispTri *pTris, const CDispMultiBlend *pMultiBlend, const MaterialSystem_SortInfo_t *pSortInfos, bool bRestoring )
{
	// Get the matching CDispInfo to fill in.
	CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );

	// Initialize the core disp info with data from the map displacement.
	pCoreDisp->GetSurface()->SetPointStart( pMapDisp->startPosition );
	pCoreDisp->InitDispInfo( pMapDisp->power, pMapDisp->minTess, pMapDisp->smoothingAngle, pVerts, pTris, pMapDisp->minTess, pMultiBlend );
	pCoreDisp->SetNeighborData( pMapDisp->m_EdgeNeighbors, pMapDisp->m_CornerNeighbors );

	// Copy the allowed verts list.
	ErrorIfNot( pCoreDisp->GetAllowedVerts().GetNumDWords() == sizeof( pMapDisp->m_AllowedVerts ) / 4, ( "DispInfo_StoreMapData: size mismatch in 'allowed verts' list" ) );
	for ( int iVert = 0; iVert < pCoreDisp->GetAllowedVerts().GetNumDWords(); ++iVert )
	{
		pCoreDisp->GetAllowedVerts().SetDWord( iVert, pMapDisp->m_AllowedVerts[iVert] );
	}

	// Build the reset of the intermediate data from the initial map displacement data.
	BuildDispSurfInit( pWorld, pCoreDisp, pDisp->GetParent() );	
	if ( !pCoreDisp->Create() )
		return false;

	// Save the point start index - needed for overlays.
	pDisp->m_iPointStart = pCoreDisp->GetSurface()->GetPointStartIndex();

	// Now setup the CDispInfo.
	pDisp->m_Index = static_cast<unsigned short>( iDisp );

	// Store ddispinfo_t data.
	pDisp->CopyMapDispData( pMapDisp );

	// Store CCoreDispInfo data.	
	if( !pDisp->CopyCoreDispData( pWorld, pSortInfos, pCoreDisp, bRestoring ) )
		return false;

	// Initialize all the active and other verts after setting up neighbors.
	pDisp->InitializeActiveVerts();
	pDisp->m_iLightmapSamplePositionStart = pMapDisp->m_iLightmapSamplePositionStart;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWorld - 
//			iDisp - 
//			*pCoreDisp - 
//			*pVerts - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void DispInfo_CreateStaticBuffersAndTags( model_t *pWorld, int iDisp, CCoreDispInfo *pCoreDisp, const CDispVert *pVerts )
{
	// Get the matching CDispInfo to fill in.
	CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );

	// Now copy the CCoreDisp's data into the static buffer.
	FillStaticBuffer( pDisp->m_pMesh, pDisp, pCoreDisp, pVerts, pDisp->NumLightMaps() );

	// Now build the tagged data for visualization.
	BuildTagData( pCoreDisp, pDisp );
}

// On the xbox, we lock the meshes ahead of time.
void SetupMeshReaders( model_t *pWorld, int nDisplacements )
{
	for ( int iDisp=0; iDisp < nDisplacements; iDisp++ )
	{
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );
		
		MeshDesc_t desc;
		memset( &desc, 0, sizeof( desc ) );
		
		desc.m_VertexSize_Position = sizeof( CDispRenderVert );
		desc.m_VertexSize_TexCoord[0] = sizeof( CDispRenderVert );
		desc.m_VertexSize_TexCoord[DISP_LMCOORDS_STAGE] = sizeof( CDispRenderVert );
		desc.m_VertexSize_Normal = sizeof( CDispRenderVert );
		desc.m_VertexSize_TangentS = sizeof( CDispRenderVert );
		desc.m_VertexSize_TangentT = sizeof( CDispRenderVert );

		CDispRenderVert *pBaseVert = pDisp->m_Verts.Base();
		desc.m_pPosition = (float*)&pBaseVert->m_vPos;
		desc.m_pTexCoord[0] = (float*)&pBaseVert->m_vTexCoord;
		desc.m_pTexCoord[DISP_LMCOORDS_STAGE] = (float*)&pBaseVert->m_LMCoords;
		desc.m_pNormal = (float*)&pBaseVert->m_vNormal;
		desc.m_pTangentS = (float*)&pBaseVert->m_vSVector;
		desc.m_pTangentT = (float*)&pBaseVert->m_vTVector;

		desc.m_nIndexSize = 1;
		desc.m_pIndices = pDisp->m_Indices.Base();
		
		pDisp->m_MeshReader.BeginRead_Direct( desc, pDisp->NumVerts(), pDisp->m_nIndices );
	}		
}

void UpdateDispBBoxes( model_t *pWorld, int nDisplacements )
{
	for ( int iDisp=0; iDisp < nDisplacements; iDisp++ )
	{
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );
		pDisp->UpdateBoundingBox();
	}
}		


#include "tier0/memdbgoff.h"
bool DispInfo_LoadDisplacements( model_t *pWorld, bool bRestoring )
{
	MEM_ALLOC_CREDIT_( "DispInfo_LoadDisplacements" );

	const MaterialSystem_SortInfo_t *pSortInfos = materialSortInfoArray;

	int nDisplacements = CMapLoadHelper::LumpSize( LUMP_DISPINFO ) / sizeof( ddispinfo_t );	
	int nSamplePositionBytes = CMapLoadHelper::LumpSize( LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS );

	// Setup the world's list of displacements.
	if ( bRestoring )
	{
		/* Breakpoint-able: */
		if (pWorld->brush.pShared->numDispInfos != nDisplacements)
		{ 
			volatile int a = 0; a = a + 1; 
		}

		if ( !pWorld->brush.pShared->numDispInfos && nDisplacements )
		{
			// Attempting to restore displacements before displacements got loaded
			return false;
		}

		ErrorIfNot( 
			pWorld->brush.pShared->numDispInfos == nDisplacements,
			("DispInfo_LoadDisplacments: dispcounts (%d and %d) don't match.", pWorld->brush.pShared->numDispInfos, nDisplacements)
			);
	}
	else
	{
		// Create the displacements.
		pWorld->brush.pShared->numDispInfos = nDisplacements;
		pWorld->brush.pShared->hDispInfos = DispInfo_CreateArray( pWorld->brush.pShared->numDispInfos );

		static ConVarRef r_dlightsenable( "r_dlightsenable" );
		if ( r_dlightsenable.GetBool() )
		{
			// Load lightmap sample positions (only needed to support dlights).
			HUNK_ALLOC_CREDIT_( "g_DispLightmapSamplePositions" );
			g_DispLightmapSamplePositions.SetSize( nSamplePositionBytes );
			CMapLoadHelper lhDispLMPositions( LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS );
			lhDispLMPositions.LoadLumpData( 0, nSamplePositionBytes, g_DispLightmapSamplePositions.Base() );
		}
	}

	// Free old data.
	DispInfo_ReleaseMaterialSystemObjects( pWorld );
	
	// load the displacement info structures into temporary space
	// using temporary storage that is not the stack for compatibility with console stack
#if 0 //#ifndef _GAMECONSOLE		// With large MAX_MAP_DISPINFO we always want to use heap to avoid a stack overflow on PC as well.
	ddispinfo_t tempDisps[MAX_MAP_DISPINFO];
#else
	CUtlMemory< ddispinfo_t > m_DispInfoBuf( 0, MAX_MAP_DISPINFO );
	ddispinfo_t *tempDisps = m_DispInfoBuf.Base();
#endif
	ErrorIfNot( 
		nDisplacements <= MAX_MAP_DISPINFO,
		("DispInfo_LoadDisplacements: nDisplacements (%d) > MAX_MAP_DISPINFO (%d)", nDisplacements, MAX_MAP_DISPINFO)
		);
	CMapLoadHelper lhDispInfo( LUMP_DISPINFO );
	lhDispInfo.LoadLumpData( 0, nDisplacements * sizeof( ddispinfo_t ), tempDisps );

	// Now hook up the displacements to their parents.
	DispInfo_LinkToParentFaces( pWorld, tempDisps, nDisplacements );

	// First, create "groups" (or "combos") which contain all the displacements that 
	// use the same material and lightmap.
	DispInfo_CreateMaterialGroups( pWorld, pSortInfos );

	// Now make the static buffers for each material/lightmap combo.
	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( "DispInfo_LoadDisplacements" );
	DispInfo_CreateEmptyStaticBuffers( pWorld, tempDisps );
	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( NULL );

	// Now setup each displacement one at a time.
	// using temporary storage that is not the stack for compatibility with console stack
#ifndef _GAMECONSOLE
	CDispVert tempVerts[MAX_DISPVERTS];
#else
	CUtlMemory< CDispVert > m_DispVertsBuf( 0, MAX_DISPVERTS );
	CDispVert *tempVerts = m_DispVertsBuf.Base();
#endif

#ifndef _GAMECONSOLE
	CDispTri tempTris[MAX_DISPTRIS];
#else
	// using temporary storage that is not the stack for compatibility with console stack
	CUtlMemory< CDispTri > m_DispTrisBuf( 0, MAX_DISPTRIS );
	CDispTri *tempTris = m_DispTrisBuf.Base();
#endif

#ifndef _GAMECONSOLE
	CDispMultiBlend tempMultiBlend[MAX_DISPVERTS];
#else
	CUtlMemory< CDispMultiBlend > m_DispMultiBlendBuf( 0, MAX_DISPVERTS );
	CDispMultiBlend *tempMultiBlend = m_DispMultiBlendBuf.Base();
#endif

	int iCurVert = 0;
	int iCurTri = 0;
	int iCurMultiBlend = 0;

	// Core displacement list.
	CUtlVector<CCoreDispInfo*> aCoreDisps;
	int iDisp = 0;
	for ( iDisp = 0; iDisp < nDisplacements; ++iDisp )
	{
		CCoreDispInfo *pCoreDisp = new CCoreDispInfo;
		aCoreDisps.AddToTail( pCoreDisp );
	}
	
	CMapLoadHelper lhDispVerts( LUMP_DISP_VERTS );
	CMapLoadHelper lhDispTris( LUMP_DISP_TRIS );
	CMapLoadHelper lhDispMultiBLend( LUMP_DISP_MULTIBLEND );

	for ( iDisp = 0; iDisp < nDisplacements; ++iDisp )
	{
		// Get the current map displacement.
		ddispinfo_t *pMapDisp = &tempDisps[iDisp];
		if ( !pMapDisp )
			continue;

		// Load the vertices from the file.
		int nVerts = NUM_DISP_POWER_VERTS( pMapDisp->power );
		ErrorIfNot( nVerts <= MAX_DISPVERTS, ( "DispInfo_LoadDisplacements: invalid vertex count (%d)", nVerts ) );
		lhDispVerts.LoadLumpData( iCurVert * sizeof(CDispVert), nVerts*sizeof(CDispVert), tempVerts );

		if ( ( pMapDisp->minTess & DISP_INFO_FLAG_HAS_MULTIBLEND ) != 0 )
		{
			lhDispMultiBLend.LoadLumpData( iCurMultiBlend * sizeof( CDispMultiBlend ), nVerts * sizeof( CDispMultiBlend ), tempMultiBlend );
			iCurMultiBlend += nVerts;
		}

		iCurVert += nVerts;

		// Load the triangle indices from the file.
		int nTris = NUM_DISP_POWER_TRIS( pMapDisp->power );
		ErrorIfNot( nTris <= MAX_DISPTRIS, ( "DispInfo_LoadDisplacements: invalid tri count (%d)", nTris ) );
		lhDispTris.LoadLumpData( iCurTri * sizeof(CDispTri), nTris*sizeof(CDispTri), tempTris );
		iCurTri += nTris;

		// Now create the CoreDispInfo and the base CDispInfo.
		if ( !DispInfo_CreateFromMapDisp( pWorld, iDisp, pMapDisp, aCoreDisps[iDisp], tempVerts, tempTris, tempMultiBlend, pSortInfos, bRestoring ) )
			return false;
	}	

	// Smooth Normals.
	SmoothDispSurfNormals( aCoreDisps.Base(), nDisplacements );

	// Fill in the static buffers.
	for ( iDisp = 0; iDisp < nDisplacements; ++iDisp )
	{
		DispInfo_CreateStaticBuffersAndTags( pWorld, iDisp, aCoreDisps[iDisp], tempVerts );
		
		// Copy over the now blended normals
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );
		pDisp->CopyCoreDispVertData( aCoreDisps[iDisp], pDisp->m_BumpSTexCoordOffset );

	}

	// Destroy core displacement list.
	aCoreDisps.PurgeAndDeleteElements();

	// If we're not using LOD, then maximally tesselate all the displacements and
	// make sure they never change.
	for ( iDisp=0; iDisp < nDisplacements; iDisp++ )
	{
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );
		
		pDisp->m_ActiveVerts = pDisp->m_AllowedVerts;
	}

	for ( iDisp=0; iDisp < nDisplacements; iDisp++ )
	{
		CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );
		pDisp->TesselateDisplacement();			
	}		

	SetupMeshReaders( pWorld, nDisplacements );

	UpdateDispBBoxes( pWorld, nDisplacements );

	return true;
}
#include "tier0/memdbgon.h"


void DispInfo_ReleaseMaterialSystemObjects( model_t *pWorld )
{
	CMatRenderContextPtr pRenderContext( materials );

	// Free all the static meshes.
	for( int iGroup=0; iGroup < g_DispGroups.Count(); iGroup++ )
	{
		CDispGroup *pGroup = g_DispGroups[iGroup];
	
		for( int iMesh=0; iMesh < pGroup->m_Meshes.Count(); iMesh++ )
		{
			CGroupMesh *pMesh = pGroup->m_Meshes[iMesh];

			pRenderContext->DestroyStaticMesh( pMesh->m_pMesh );
		}

		pGroup->m_Meshes.PurgeAndDeleteElements();
	}

	g_DispGroups.PurgeAndDeleteElements();


	// Clear pointers in the dispinfos.
	if( pWorld )
	{
		for( int iDisp=0; iDisp < pWorld->brush.pShared->numDispInfos; iDisp++ )
		{
			CDispInfo *pDisp = GetModelDisp( pWorld, iDisp );
			if ( !pDisp )
			{
				AssertOnce( 0 );
				continue;
			}
			
			pDisp->m_pMesh = NULL;
			pDisp->m_iVertOffset = pDisp->m_iIndexOffset = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// CDispInfo implementation.
//-----------------------------------------------------------------------------

CDispInfo::CDispInfo()
{
	m_ParentSurfID = SURFACE_HANDLE_INVALID;

    m_bTouched = false;

	++g_ConstructorChecker.m_nConstructedObjects;

	m_BBoxMin.Init();
	m_BBoxMax.Init();

	m_idLMPage = -1;

	m_pPowerInfo = NULL;

	m_ViewerSphereCenter.Init( 1e24, 1e24, 1e24 );
	
	m_bInUse = false;

	m_pNodeInfo = 0;

	m_pMesh = NULL;

	m_Tag = NULL;
	m_pDispArray = NULL;

	m_FirstDecal = DISP_DECAL_HANDLE_INVALID;
	m_FirstShadowDecal = DISP_SHADOW_HANDLE_INVALID;

	for ( int i=0; i < 4; i++ )
	{
		m_EdgeNeighbors[i].SetInvalid();
		m_CornerNeighbors[i].SetInvalid();
	}
}


CDispInfo::~CDispInfo()
{
	if (m_pNodeInfo)
		delete[] m_pNodeInfo;

	delete[] m_pWalkIndices;
	delete[] m_pBuildIndices;

	--g_ConstructorChecker.m_nConstructedObjects;

	// All the decals should have been freed through 
	// CModelLoader::Map_UnloadModel -> R_DecalTerm
	Assert( m_FirstDecal == DISP_DECAL_HANDLE_INVALID );
	Assert( m_FirstShadowDecal == DISP_SHADOW_HANDLE_INVALID );
}


void CDispInfo::CopyCoreDispVertData( const CCoreDispInfo *pCoreDisp, float bumpSTexCoordOffset )
{
#ifndef DEDICATED
	if( NumLightMaps() <= 1 )
	{
		bumpSTexCoordOffset = 0.0f;
	}
	// Copy vertex positions (for backfacing tests).
	HUNK_ALLOC_CREDIT_( "CopyCoreDispVertData" );
	m_Verts.SetSize( m_pPowerInfo->m_MaxVerts );
	m_BumpSTexCoordOffset = bumpSTexCoordOffset;
	for( int i=0; i < NumVerts(); i++ )
	{
		pCoreDisp->GetVert( i, m_Verts[i].m_vPos );

		pCoreDisp->GetTexCoord( i, m_Verts[i].m_vTexCoord );
		pCoreDisp->GetLuxelCoord( 0, i, m_Verts[i].m_LMCoords );

		// mat_normals needs this as well as the dynamic lighting code
		pCoreDisp->GetNormal( i, m_Verts[i].m_vNormal );
		pCoreDisp->GetTangentS( i, m_Verts[i].m_vSVector );
		pCoreDisp->GetTangentT( i, m_Verts[i].m_vTVector );
	} 
#endif
}

bool CDispInfo::CopyCoreDispData( 
	model_t *pWorld,
	const MaterialSystem_SortInfo_t *pSortInfos,
	const CCoreDispInfo *pCoreDisp,
	bool bRestoring )
{
	m_idLMPage = pSortInfos[MSurf_MaterialSortID( GetParent() )].lightmapPageID;

#ifndef DEDICATED
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, GetParent() );
#endif

	// Restoring is only for alt+tabbing, which can't happen on consoles
	if ( IsPC() && bRestoring )
	{
#ifndef DEDICATED
		// When restoring, have to recompute lightmap coords
		if( NumLightMaps() > 1 )
		{
			m_BumpSTexCoordOffset = ctx.m_BumpSTexCoordOffset;
		}
		else
		{
			m_BumpSTexCoordOffset = 0.0f;
		}
		for( int i=0; i < NumVerts(); i++ )
		{
			pCoreDisp->GetLuxelCoord( 0, i, m_Verts[i].m_LMCoords );
		}
#endif // DEDICATED
		return true;
	}

	// When restoring, leave all this data the same.
	const CCoreDispSurface *pSurface = pCoreDisp->GetSurface();
	for( int index=0; index < 4; index++ )
	{
		pSurface->GetTexCoord( index, m_BaseSurfaceTexCoords[index] );
		m_BaseSurfacePositions[index] = pSurface->GetPoint( index );
	}

#ifndef DEDICATED
	CopyCoreDispVertData( pCoreDisp, ctx.m_BumpSTexCoordOffset );
#endif

	// Copy neighbor info.
	for ( int iEdge=0; iEdge < 4; iEdge++ )
	{
		m_EdgeNeighbors[iEdge] = *pCoreDisp->GetEdgeNeighbor( iEdge );
		m_CornerNeighbors[iEdge] = *pCoreDisp->GetCornerNeighbors( iEdge );
	}

	// Copy allowed verts.
	m_AllowedVerts = pCoreDisp->GetAllowedVerts();

	m_nIndices = 0;
	return true;
}


int CDispInfo::NumLightMaps()
{
	return (MSurf_Flags( m_ParentSurfID ) & SURFDRAW_BUMPLIGHT) ? NUM_BUMP_VECTS+1 : 1;
}

//-----------------------------------------------------------------------------
// Purpose:
// NOTE: You cannot use the builddisp.cpp IsTriWalkable, IsTriBuildable functions
//       because the flags are different having been collapsed in vbsp 
//-----------------------------------------------------------------------------
void BuildTagData( CCoreDispInfo *pCoreDisp, CDispInfo *pDisp )
{
	int nWalkTest = 0;
	int nBuildTest = 0;
	int iTri;
	for ( iTri = 0; iTri < pCoreDisp->GetTriCount(); ++iTri )
	{
		if ( pCoreDisp->IsTriTag( iTri, DISPTRI_TAG_WALKABLE ) )
		{
			nWalkTest++;
		}

		if ( pCoreDisp->IsTriTag( iTri, DISPTRI_TAG_BUILDABLE ) )
		{
			nBuildTest++;
		}
	}

	nWalkTest *= 3;
	nBuildTest *= 3;

	pDisp->m_pWalkIndices = new unsigned short[nWalkTest];
	pDisp->m_pBuildIndices = new unsigned short[nBuildTest];

	int nWalkCount = 0;
	int nBuildCount = 0;
	for ( iTri = 0; iTri < pCoreDisp->GetTriCount(); ++iTri )
	{
		if ( pCoreDisp->IsTriTag( iTri, DISPTRI_TAG_WALKABLE ) )
		{
			pCoreDisp->GetTriIndices( iTri, 
								      pDisp->m_pWalkIndices[nWalkCount],
									  pDisp->m_pWalkIndices[nWalkCount+1],
									  pDisp->m_pWalkIndices[nWalkCount+2] );

			nWalkCount += 3;
		}

		if ( pCoreDisp->IsTriTag( iTri, DISPTRI_TAG_BUILDABLE ) )
		{
			pCoreDisp->GetTriIndices( iTri, 
								      pDisp->m_pBuildIndices[nBuildCount],
									  pDisp->m_pBuildIndices[nBuildCount+1],
									  pDisp->m_pBuildIndices[nBuildCount+2] );

			nBuildCount += 3;
		}
	}

	Assert( nWalkCount == nWalkTest );
	Assert( nBuildCount == nBuildTest );

	pDisp->m_nWalkIndexCount = nWalkCount;
	pDisp->m_nBuildIndexCount = nBuildCount;
}


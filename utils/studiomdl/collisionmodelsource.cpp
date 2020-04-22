//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Builds physics2 collision models from studio model source
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "studiomdl.h"
#include "collisionmodelsource.h"

//-----------------------------------------------------------------------------
// Purpose: Transforms the source's verts into "world" space
// Input  : *psource - 
//			*worldVerts - 
//-----------------------------------------------------------------------------
void CCollisionModelSource::ConvertToWorldSpace( CUtlVector<Vector> &worldVerts, s_source_t *pmodel )
{
	int i, n;

	if (!m_bAssumeWorldspace)
	{
		matrix3x4_t boneToWorld[MAXSTUDIOSRCBONES];	// bone transformation matrix
		CalcBoneTransforms( g_panimation[0], 0, boneToWorld );

		for (i = 0; i < pmodel->numvertices; i++)
		{
			Vector tmp,tmp2;
			worldVerts[i].Init(0,0,0 );

			int nBoneCount = pmodel->vertex[i].boneweight.numbones;
			for (n = 0; n < nBoneCount; n++)
			{
				// convert to Half-Life world space
				// convert vertex into original models' bone local space
				int localBone = pmodel->vertex[i].boneweight.bone[n];
				int globalBone = pmodel->boneLocalToGlobal[localBone];
				Assert( localBone >= 0 );
				Assert( globalBone >= 0 );

				matrix3x4_t boneToPose;
				ConcatTransforms( pmodel->boneToPose[localBone], g_bonetable[globalBone].srcRealign, boneToPose );
				VectorITransform( pmodel->vertex[i].position, boneToPose, tmp2 );

				// now transform to that bone's world-space position in this animation
				VectorTransform(tmp2, boneToWorld[globalBone], tmp );
				VectorMA( worldVerts[i], pmodel->vertex[i].boneweight.weight[n], tmp, worldVerts[i] );
			}
		}
	}
	else
	{
		matrix3x4_t srcBoneToWorld[MAXSTUDIOSRCBONES];	// bone transformation matrix
		BuildRawTransforms( pmodel, "BindPose", 0, pmodel->scale, pmodel->adjust, pmodel->rotation, 0, srcBoneToWorld );

		for (i = 0; i < pmodel->numvertices; i++)
		{
			Vector tmp;
			worldVerts[i].Init( 0, 0, 0 );

			int nBoneCount = pmodel->vertex[i].boneweight.numbones;
			for (n = 0; n < nBoneCount; n++)
			{
				int localBone = pmodel->vertex[i].boneweight.bone[n];
				Assert( localBone >= 0 );

				// convert vertex into world space
				VectorTransform( pmodel->vertex[i].position, srcBoneToWorld[localBone], tmp );
				// just assume the model is in identity space 
				// FIXME: shouldn't this do an inverse xform of the default boneToWorld?

				VectorMA( worldVerts[i], pmodel->vertex[i].boneweight.weight[n], tmp, worldVerts[i] );
			}
		}
	}

	if ( g_flCollisionPrecision > 0 )
	{
#ifdef DEBUG
		printf("Applying collision precision truncation: %f\n", g_flCollisionPrecision );
#endif
		for ( int i = 0; i < worldVerts.Count(); i++ )
		{
			worldVerts[i].x -= fmod( worldVerts[i].x, g_flCollisionPrecision );
			worldVerts[i].y -= fmod( worldVerts[i].y, g_flCollisionPrecision );
			worldVerts[i].z -= fmod( worldVerts[i].z, g_flCollisionPrecision );
		}
	}

}




//-----------------------------------------------------------------------------
// Purpose: Transforms the set of verts into the space of a particular bone
// Input  : *psource - 
//			boneIndex - 
//			*boneVerts - 
//-----------------------------------------------------------------------------
void CCollisionModelSource::ConvertToBoneSpace( int boneIndex, CUtlVector<Vector> &boneVerts )
{
	int i;

	int remapIndex = m_pModel->boneLocalToGlobal[boneIndex];
	matrix3x4_t boneToPose;
	if ( remapIndex < 0 )
	{
		MdlWarning("Error! physics for unused bone %s\n", m_pModel->localBone[boneIndex].name );
		MatrixCopy( m_pModel->boneToPose[boneIndex], boneToPose );
	}
	else
	{
		ConcatTransforms( m_pModel->boneToPose[boneIndex], g_bonetable[remapIndex].srcRealign, boneToPose );
	}

	for (i = 0; i < m_pModel->numvertices; i++)
	{
		VectorITransform(m_pModel->vertex[i].position, boneToPose, boneVerts[i] );
	}
}


bool CCollisionModelSource::ShouldProcessBone( int boneIndex )
{
	if ( boneIndex >= 0 )
	{
		if ( m_bonemap[boneIndex] == boneIndex )
			return true;
	}
	return false;
}


// called before processing, after the model has been simplified.
// Update internal state due to simplification
void CCollisionModelSource::Simplify()
{
	if ( m_pModel )
	{
		for ( int i = 0; i < m_pModel->numbones; i++ )
		{
			if ( m_pModel->boneLocalToGlobal[i] < 0 )
			{
				SkipBone(i);
			}

			// Walk the parents of this bone, if they map to the same global bone then go ahead and 
			// merge them now so we can aggregate the collision models
			int nMatchingParent = i;
			int nParentCheck = m_pModel->localBone[nMatchingParent].parent;
			int nGlobalMatch = m_pModel->boneLocalToGlobal[i];
			while ( nParentCheck >= 0 && m_pModel->boneLocalToGlobal[nParentCheck] == nGlobalMatch )
			{
				nMatchingParent = nParentCheck;
				nParentCheck = m_pModel->localBone[nParentCheck].parent;
			}
			if ( nMatchingParent != i )
			{
				MergeBones( nMatchingParent, i );
			}
		}
	}

	extern int g_rootIndex;
	const char *pAnimationRootBone = g_bonetable[g_rootIndex].name;

	// merge this root bone with the root of animation
	MergeBones( pAnimationRootBone, m_rootName );

}


void CCollisionModelSource::SkipBone( int boneIndex )
{
	if ( boneIndex >= 0 )
		m_bonemap[boneIndex] = -1;
}


void CCollisionModelSource::InitBoneMap( void )
{
	m_bonemap.SetSize(m_pModel->numbones);
	for ( int i = 0; i < m_pModel->numbones; i++ )
	{
		m_bonemap[i] = i;
	}
}


void CCollisionModelSource::MergeBones( int parent, int child )
{
	if ( parent < 0 || child < 0 )
		return;

	int map = parent;
	int safety = 0;
	while ( m_bonemap[map] != map )
	{
		map = m_bonemap[map];
		safety++;
		// infinite loop?
		if ( safety > m_pModel->numbones )
			break;

		if ( map < 0 )
			break;
	}

	m_bonemap[child] = map;
}

void CCollisionModelSource::MergeBones(const char *parent, const char *child)
{
	MergeBones(FindLocalBoneNamed( parent ), FindLocalBoneNamed( child ));
}


//-----------------------------------------------------------------------------
// Purpose: Search a source for a bone with a specified name
// Input  : *pSource - 
//			*pName - 
// Output : int boneIndex, -1 if none
//-----------------------------------------------------------------------------

int FindLocalBoneNamed( const s_source_t *pSource, const char *pName )
{
	if ( pName && pSource )
	{
		int i;
		for ( i = 0; i < pSource->numbones; i++ )
		{
			if ( !stricmp( pName, pSource->localBone[i].name ) )
				return i;
		}

		pName = RenameBone( pName );

		for ( i = 0; i < pSource->numbones; i++ )
		{
			if ( !stricmp( pName, pSource->localBone[i].name ) )
				return i;
		}
	}

	return -1;
}

int CCollisionModelSource::FindLocalBoneNamed( const char *pName )
{
	return ::FindLocalBoneNamed(m_pModel, pName);
}



//-----------------------------------------------------------------------------
// Purpose: Test this face to see if any of its verts are assigned to a particular bone
//			*pmodel - 
//			*face - 
//			boneIndex - 
// Output : Returns true if this face has a vert assigned to boneIndex
//-----------------------------------------------------------------------------
bool CCollisionModelSource::FaceHasVertOnBone( const s_face_t &face, int boneIndex )
{
	if ( boneIndex < 0 )
		return true;

	int j;
	s_boneweight_t *pweight;
	pweight = &m_pModel->vertex[ face.a ].boneweight;
	for ( j = 0; j < pweight->numbones; j++ )
	{
		// assigned to boneIndex?
		if ( RemapBone( pweight->bone[j] ) == boneIndex )
			return true;
	}

	pweight = &m_pModel->vertex[ face.b ].boneweight;
	for ( j = 0; j < pweight->numbones; j++ )
	{
		// assigned to boneIndex?
		if ( RemapBone( pweight->bone[j] ) == boneIndex )
			return true;
	}

	pweight = &m_pModel->vertex[ face.c ].boneweight;
	for ( j = 0; j < pweight->numbones; j++ )
	{
		// assigned to boneIndex?
		if ( RemapBone( pweight->bone[j] ) == boneIndex )
			return true;
	}

	return false;
}


int	CCollisionModelSource::RemapBone( int boneIndex ) const
{
	if ( boneIndex >= 0 )
		return m_bonemap[boneIndex];
	return boneIndex;
}



s_face_t CCollisionModelSource::GetGlobalFace( s_mesh_t *pMesh, int nFace )
{
	s_face_t output;
	GlobalFace(&output, pMesh, m_pModel->face + pMesh->faceoffset + nFace);
	return output;
}


void CCollisionModelSource::FindBoundBones(s_mesh_t *pMesh, CUtlVector<int>&setBones)
{
	s_face_t *pFaces = m_pModel->face + pMesh->faceoffset;
	s_vertexinfo_t *pVertices = m_pModel->vertex + pMesh->vertexoffset;

	for ( int nFace = 0; nFace < pMesh->numfaces; nFace++ )
	{
		FindBoundBones(pVertices[pFaces[nFace].a].boneweight, setBones);
		FindBoundBones(pVertices[pFaces[nFace].b].boneweight, setBones);
		FindBoundBones(pVertices[pFaces[nFace].c].boneweight, setBones);
	}
}

void CCollisionModelSource::FindBoundBones(s_boneweight_t &weights, CUtlVector<int>&setBones)
{
	for(int nBoundBone = 0; nBoundBone < weights.numbones; ++nBoundBone)
	{	int boneIndex = RemapBone(weights.bone[nBoundBone]);
		if(!setBones.HasElement(boneIndex))
			setBones.AddToTail(boneIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fixup the pointers in this face to reference the mesh globally (source relative)
//			(faces are mesh relative, each source has several meshes)
// Input  : *pout - 
//			*pmesh - 
//			*pin - 
//-----------------------------------------------------------------------------
void GlobalFace( s_face_t *pout, s_mesh_t *pmesh, s_face_t *pin )
{
	pout->a = pmesh->vertexoffset + pin->a;
	pout->b = pmesh->vertexoffset + pin->b;
	pout->c = pmesh->vertexoffset + pin->c;
}

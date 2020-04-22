//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Builds physics2 collision models from studio model source
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include <string.h>
#include "tier1/tier1.h"
#include "tier1/smartptr.h"
#include "tier2/p4helpers.h"
#include "datalinker.h"
#include "vphysics2_interface.h"
#include "vphysics2_interface_flags.h"
#include "alignedarray.h"
#include "studiomdl.h"
#include "filesystem_tools.h"
#include "collisionmodelsource.h"
#include "physics2collision.h"
#include "physdll.h"
#include "phzfile.h"

static IPhysics2Cook *g_pCook;


struct bodypart_t
{
	IPhysics2CookedMeshBase* mesh;
	int bone;
	bodypart_t(){}
	bodypart_t(IPhysics2CookedMeshBase* _mesh, int _bone):mesh(_mesh),bone(_bone){}
};

class CPhysics2CollisionBuilder: public CCollisionModelSource
{
public:
	void Init(CCollisionModelSource *pSource)
	{
		*static_cast<CCollisionModelSource*>(this) = *pSource;
	}
	void Shutdown()
	{
		Destroy(m_bodyparts);
	}
	void Destroy(CUtlVector<bodypart_t>&bodyparts)
	{
		for(int i = 0; i < bodyparts.Size(); ++i)
			g_pCook->Destroy(bodyparts[i].mesh);
	}

	void Build()
	{
		if(m_isJointed)
			BuildJointed();
		else
			BuildRigid();
	}
	void BuildRigid();
	void BuildJointed();
	void Write();

	void Destroy(CUtlVector<IPhysics2CookedMeshBase*> &arrPolytopes);

	CUtlVector<bodypart_t> m_bodyparts;
};

class CMeshAdaptor:public CPhysics2CustomMeshBase
{
public:
	virtual uint GetType()const {return PHYSICS2_SHAPE_TYPE_CUSTOM;}
	virtual uint NumVertices() const {return m_pMesh->numvertices;}
	virtual uint NumTriangles() const {return m_pMesh->numfaces;}
	virtual void GetVertices(float *pVertsOut, uint nByteStride, const fltx4 &factor = Four_Ones)
	{
		uint numVerts = m_pMesh->numvertices;
		byte *pOut = (byte*)pVertsOut;
		for(uint i =0; i< numVerts; ++i)
		{
			fltx4 vert = MulSIMD(LoadUnaligned3SIMD(&m_pVerts[i+m_pMesh->vertexoffset].x), factor);
			StoreUnaligned3SIMD((float*)pOut, vert);
			pOut += nByteStride;
		}
	}
	virtual void GetTriangles(int *pTrisOut, uint nByteStride)
	{
		uint numTris = m_pMesh->numfaces;
		byte *pOut = (byte*)pTrisOut;

		for(uint i = 0;i < numTris; ++i)
		{
			const s_face_t *pFace = m_pFaces + i + m_pMesh->faceoffset;
			Assert(pFace->a+m_pMesh->vertexoffset < (uint)m_pMesh->numvertices && pFace->b+m_pMesh->vertexoffset < (uint)m_pMesh->numvertices && pFace->c+m_pMesh->vertexoffset < (uint)m_pMesh->numvertices
				);
			((int*)pOut)[0] = pFace->a;
			((int*)pOut)[1] = pFace->b;
			((int*)pOut)[2] = pFace->c;
			pOut += nByteStride;
		}
	}
	virtual uint GetSizeOf()const {return sizeof(*this);}

	const s_face_t *m_pFaces;// non-offset faces
	const Vector *m_pVerts; // non-offset verts 
	const s_mesh_t *m_pMesh; 
};


static CPhysics2CollisionBuilder g_builder;

void Physics2Collision_Build(CCollisionModelSource *pSource)
{
	g_pCook = g_pPhysics2->GetCook();

	g_builder.Init(pSource);
	g_builder.Build();
}

void Physics2Collision_Write()
{
	g_builder.Write();
}


void CPhysics2CollisionBuilder::BuildJointed()
{
	// first, go through all meshes and determine what bones they belong to
	//CUtlVector<CUtlVector<int> > arrMeshBones(0,m_pModel->nummeshes);
	// remap it : bone -> faces
	CUtlVector<CUtlVector<s_face_t> > arrBoneFaces;
	arrBoneFaces.SetSize(m_pModel->numbones);

	// Constructing elements of the array. This is irritating, there should be a method to do that..
	for(int i =0; i < m_pModel->numbones; ++i)
		new(&arrBoneFaces[i])CUtlVector<s_mesh_t *>(32);
	//for(int i = 0; i < m_pModel->nummeshes; ++i)
	//	new(&arrMeshBones[i])CUtlVector<int>();
	
	// for each mesh, find bone(s) it belongs to and push it to that bone (those bones)
	for(int nMesh = 0; nMesh < m_pModel->nummeshes; ++nMesh)
	{
		s_mesh_t *pMesh = m_pModel->mesh + m_pModel->meshindex[nMesh];

		for(int nFace = 0; nFace < pMesh->numfaces; ++nFace)
		{
			s_face_t face = GetGlobalFace(pMesh, nFace);
			s_boneweight_t &boneweight = m_pModel->vertex[face.a].boneweight;
			if(boneweight.numbones)
			{
				int boneIndex = RemapBone(boneweight.bone[0]);
				if(boneIndex >= 0 && boneIndex < m_pModel->numbones)
					arrBoneFaces[boneIndex].AddToTail(face);
			}
		}
	}

	// for each bone, we have 0..many meshes now; compile the meshes; we don't try to share the meshes between different bones here,
	// the idea is that we'll have rigid binding to skeleton, possibly sometimes multiple meshes to the same bone, but not the same mesh
	// to multiple bones
	CUtlVector<Vector> bonespaceVerts;
	bonespaceVerts.SetCount(m_pModel->numvertices);
	for(int nBone = 0; nBone < m_pModel->numbones; ++nBone)
	{
		CUtlVector<IPhysics2CookedMeshBase*> arrPolytopes;
		bodypart_t bodypart;
		bodypart.bone = nBone;
		bodypart.mesh = NULL;
		CUtlVector<s_face_t> &arrFaces = arrBoneFaces[nBone];
		if(ShouldProcessBone(nBone) && arrFaces.Size())
		{
			// convert ALL vertices into this bone's frame (it's easier)
			ConvertToBoneSpace(nBone, bonespaceVerts);
			// cook one polytope for each s_mesh_t (out of the Mesh interface)
			s_mesh_t mesh;
			mesh.faceoffset = 0;
			mesh.numfaces = arrFaces.Size();
			mesh.vertexoffset = 0;
			mesh.numvertices = bonespaceVerts.Size();

			CMeshAdaptor adaptor;
			adaptor.m_pMesh = &mesh;
			adaptor.m_pFaces = arrFaces.Base();
			adaptor.m_pVerts = bonespaceVerts.Base();
			if(IPhysics2CookedPolytope *pCookedPolytope = g_pCook->CookPolytope(&adaptor))
				arrPolytopes.AddToTail(pCookedPolytope);
		}
		if(arrPolytopes.Size() > 1)
		{
			if(m_allowConcaveJoints)
			{
				bodypart.mesh = g_pCook->CookMopp(arrPolytopes.Base(), arrPolytopes.Size());
			}
			else
			{
				bodypart.mesh = g_pCook->CookPolytopeFromMeshes(arrPolytopes.Base(), arrPolytopes.Size());
			}

			Destroy(arrPolytopes);
		}
		else
		if(arrPolytopes.Size() == 1)
		{
			bodypart.mesh = arrPolytopes[0];
		}
		if(bodypart.mesh)
			m_bodyparts.AddToTail(bodypart);
	}
}




void CPhysics2CollisionBuilder::BuildRigid()
{
	CUtlVector<Vector> worldspaceVerts;
	worldspaceVerts.SetCount(m_pModel->numvertices);
	ConvertToWorldSpace( worldspaceVerts );

	m_bodyparts.SetSize(0);
	bool bValid = true;
	if ( m_allowConcave )
	{
		CUtlVector<CMeshAdaptor> arrMeshes;
		int numMeshes = m_pModel->nummeshes;
		arrMeshes.SetCount(numMeshes);
		for ( int i = 0; i < numMeshes; i++ )
		{
			s_mesh_t *pMesh = m_pModel->mesh + m_pModel->meshindex[i];
			arrMeshes[i].m_pFaces = m_pModel->face;
			arrMeshes[i].m_pVerts = worldspaceVerts.Base();//m_pModel->vertex;
			arrMeshes[i].m_pMesh = pMesh;
		}
		// this is one way to do it: make one polysoup
		//g_pCook->CookPolysoupFromMeshes(arrMeshes.Base(), numMeshes);
		
		// another way is to create a bunch of convex polytopes
		for ( int i = 0; i < numMeshes; i++ )
		{
			IPhysics2CookedPolytope *polytope = g_pCook->CookPolytope(&arrMeshes[i]);
			if(polytope)
			{
				m_bodyparts.AddToTail(bodypart_t(polytope, -1));
			}
		}
	}

	if ( m_bodyparts.Count() > m_maxConvex )
	{
		MdlWarning("COSTLY COLLISION MODEL!!!! (%d parts - %d allowed)\n", m_bodyparts.Count(), m_maxConvex );
		bValid = false;
	}

	if ( !bValid && m_bodyparts.Count() )
	{
		MdlWarning("Error with convex elements of %s, building single convex!!!!\n", m_pModel->filename );
		Destroy(m_bodyparts);
	}

	// either we don't want concave, or there was an error building it
	if ( !m_bodyparts.Count() )
	{
		CUtlVector_Vector4DAligned arrVerts;
		arrVerts.SetSize(worldspaceVerts.Count());
		for(int i = 0;i < worldspaceVerts.Count(); ++i)
		{
			const Vector &v = worldspaceVerts[i];
			arrVerts[i].Init(v.x,v.y,v.z);
		}
		IPhysics2CookedPolytope *polytope = g_pCook->CookPolytopeFromVertices((Vector4DAligned*)arrVerts.Base(), worldspaceVerts.Count());
		m_bodyparts.AddToTail(bodypart_t(polytope,-1));
	}

	if(m_bodyparts.Size() > 1)
	{
		// fold it into one single neat mesh
		CUtlVector<IPhysics2CookedMeshBase*>arrMeshes(m_bodyparts.Size(),m_bodyparts.Size());
		for(int i = 0;i < m_bodyparts.Size(); ++i)
			arrMeshes[i] = m_bodyparts[i].mesh;
		IPhysics2CookedMopp *mopp = g_pCook->CookMopp(arrMeshes.Base(), m_bodyparts.Size());
		Destroy(m_bodyparts);
		if(mopp)
			m_bodyparts.AddToTail(bodypart_t(mopp, -1));
	}

}

void CPhysics2CollisionBuilder::Destroy(CUtlVector<IPhysics2CookedMeshBase*> &arrPolytopes)
{
	for ( int i = 0; i < arrPolytopes.Count(); i++ )
		g_pCook->Destroy( arrPolytopes[i] );
	arrPolytopes.Purge();
}




void CPhysics2CollisionBuilder::Write()
{
	char filename[512];

	strcpy( filename, gamedir );
	strcat( filename, "models/" );	
	strcat( filename, m_pOverrideName ? m_pOverrideName : outname );	
	Q_SetExtension( filename, ".phz", sizeof( filename ) );
	if(!m_bodyparts.Size())
	{
		CPlainAutoPtr< CP4File > spFile( g_p4factory->AccessFile( filename ) );
		unlink(filename);
		return;
	}


	DataLinker::Stream stream;
	Physics2CollisionHeader_t *pHeader = stream.Write<Physics2CollisionHeader_t>();
	pHeader->m_dataVersion = g_pPhysics2->GetSerializeVersion();
	pHeader->m_numBones = m_bodyparts.Size();
	//if(!m_pModel->numbones)
	//	pHeader->m_numBones = 1; // there's still 1 pseudo-bone there
	Physics2RigidPolyShape_t *pRigids = stream.IStream::WriteAndLinkStrided(&pHeader->m_shapes, sizeof(Physics2RigidPolyShape_t), m_bodyparts.Count());

	///
	// Note: I want all inertia descriptors to reside together for cache coherency in dynamics phase, so I'm writing inertia first, then the shapes
	///

	for(int nBodyPart = 0; nBodyPart < m_bodyparts.Size(); ++nBodyPart)
	{
		int boneIndex = m_bodyparts[nBodyPart].bone;
		IPhysics2CookedMeshBase *pMesh = m_bodyparts[nBodyPart].mesh;
		const char *boneName = boneIndex < 0 ? "" : m_pModel->localBone[boneIndex].name;
		// we'll leave all offsets to NULL if there's no mesh for that bone
		if(pMesh)
		{
			IPhysics2CookedInertia *pInertia = g_pCook->CookInertia(pMesh->GetShape()); // the inertia of the model as a rigid whole
			if(pInertia)
			{
				stream.IStream::Link(&pRigids[nBodyPart].m_inertia, pInertia->Serialize(&stream));
				g_pCook->Destroy(pInertia);
			}
			else
				Warning("Could not cook inertia for '%s' #d\n", boneName, boneIndex);
		}
		pRigids[nBodyPart].m_localBoneIndex = boneIndex;

		if(boneIndex >= 0)
		{
			int globalBoneIndex = m_pModel->boneLocalToGlobal[boneIndex];
			pRigids[nBodyPart].m_globalBoneIndex = globalBoneIndex;
		}
	}

	for(int nBodyPart = 0; nBodyPart < m_bodyparts.Size(); ++nBodyPart)
	{
		bodypart_t &bp = m_bodyparts[nBodyPart];
		// we'll leave all offsets to NULL if there's no mesh for that bone
		pRigids[nBodyPart].m_shapeType = bp.mesh->GetType();
		stream.IStream::Link(&pRigids[nBodyPart].m_shape, bp.mesh->Serialize(&stream));
	}

	*(char*)stream.WriteBytes(1) = '\n'; // for debugging
	for(int nBodyPart = 0; nBodyPart < m_bodyparts.Size(); ++nBodyPart)
	{
		bodypart_t &bp = m_bodyparts[nBodyPart];
		if(bp.bone >= 0)
		{
			const char *name = m_pModel->localBone[bp.bone].name;
			if(name)
			{
				int nameLen = strlen(name);
				char *pNameOut = (char*)stream.WriteBytes(nameLen + 2);
				stream.IStream::Link(&pRigids[nBodyPart].m_name, pNameOut);
				memcpy(pNameOut, name, nameLen+1);
				pNameOut[nameLen+1] = '\n'; // for debugging
			}
		}
	}

	uint nDataSize = stream.GetTotalSize();
	void *pData = MemAlloc_AllocAligned(nDataSize, 16, __FILE__, __LINE__);
	if(stream.Compile(pData))
	{
		CPlainAutoPtr< CP4File > spFile( g_p4factory->AccessFile( filename ) );
		spFile->Edit();
		FILE *fp = fopen( filename, "wb" );
		if(fp)
		{
			int numWritten = fwrite(pData, nDataSize, 1, fp);
			fclose(fp);
		}
		else
		{
			MdlWarning("Error writing %s!!!\n", filename );
		}
	}
	else
	{
		MdlWarning("Cannot compile the phz data\n");
	}

	MemAlloc_FreeAligned(pData, __FILE__, __LINE__);
}


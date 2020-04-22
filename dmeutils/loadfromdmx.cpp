//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh class dmx loading functions
//
//===========================================================================//

#include "movieobjects/dmemodel.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeattachment.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmecombinationoperator.h"
#include "mdlobjects/dmebbox.h"
#include "mdlobjects/dmelod.h"
#include "mdlobjects/dmelodlist.h"
#include "mdlobjects/dmebodygroup.h"
#include "mdlobjects/dmebodygrouplist.h"
#include "mdlobjects/dmehitbox.h"
#include "mdlobjects/dmehitboxset.h"
#include "mdlobjects/dmehitboxsetlist.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/dmesequencelist.h"
#include "mdlobjects/dmecollisionmodel.h"
#include "mdlobjects/dmecollisionjoints.h"
#include "mdlobjects/dmeincludemodellist.h"
#include "mdlobjects/dmedefinebone.h"
#include "mdlobjects/dmedefinebonelist.h"
#include "mdlobjects/dmematerialgroup.h"
#include "mdlobjects/dmematerialgrouplist.h"
#include "mdlobjects/dmeeyeball.h"
#include "mdlobjects/dmeeyeballglobals.h"
#include "mdlobjects/dmeboneweight.h"
#include "mdlobjects/dmebonemask.h"
#include "mdlobjects/dmebonemasklist.h"
#include "mdlobjects/dmeik.h"
#include "mdlobjects/dmeanimcmd.h"
#include "mdlobjects/dmemotioncontrol.h"
#include "mdlobjects/dmeposeparameter.h"
#include "mdlobjects/dmeposeparameterlist.h"
#include "mdlobjects/dmeanimblocksize.h"
#include "dmeutils/dmmeshutils.h"
#include "meshutils/mesh.h"


#define MAX_BIND_POSE_BONES 256
struct LoadMeshInfo_t
{
	CDmeModel *m_pModel;
	float m_flScale;
	int *m_pBoneRemap;
	matrix3x4_t m_pBindPose[MAX_BIND_POSE_BONES];
};

class CDMXLoader
{
public:
	bool LoadVertices( CDmeDag *pDmeDag, CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale, int nBoneAssign, int *pBoneRemap, int nStartingUniqueCount );
	bool LoadMesh( CDmeDag *pDmeDag, CDmeMesh *pMesh, CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale,
					 int nBoneAssign, int *pBoneRemap );
	bool LoadMesh( CDmeMesh *pMesh, CDmeDag *pDag, const matrix3x4_t &dagToBindPose = g_MatrixIdentity, float flScale = 1.0f, int nBoneAssign = -1, int *pBoneRemap = NULL, const CUtlVector< CUtlString > *pDeltaNames = NULL );
	bool LoadMeshes( const LoadMeshInfo_t &info, CDmeDag *pDag, const matrix3x4_t &parentToBindPose, int nBoneAssign );
	bool LoadMeshes( CDmeModel *pModel, float flScale, int *pBoneRemap = NULL );
	bool LoadMaterialGroups( CDmeMaterialGroupList *pMaterialGroupList );
	bool LoadDMX( const char *pDMXFile );
	CUtlVector<CMesh*> &GetOutputMeshes() { return m_outputMeshes; }
	CUtlVector< CDmeDag* > &GetOutputDmeDags() { return m_OutputDmeDags; }

private:
	CUtlVector<CMesh*>			m_outputMeshes;
	CUtlVector< CDmeDag* >			m_OutputDmeDags; // DAG per mesh, when not collapsing meshes
};

void GenerateAttributesFromVertexData( CMeshVertexAttribute *pAttributes, int &nAttributes, int &nVertexStrideFloats, CDmeVertexData *pBindState )
{
	const CUtlVector<Vector> &positions = pBindState->GetPositionData( );
	const CUtlVector<Vector> &normals = pBindState->GetNormalData( );
	const CUtlVector<Vector2D> &texcoords = pBindState->GetTextureCoordData( );
	const CUtlVector<Vector4D> &tangents = pBindState->GetTangentData( );
	const CUtlVector<Color> &colors = pBindState->GetColorData();

	int nLocalAttributes = 0;
	nVertexStrideFloats = 0;
	if ( positions.Count() )
	{
		pAttributes[ nLocalAttributes ].m_nOffsetFloats = nVertexStrideFloats;
		pAttributes[ nLocalAttributes ].m_nType = VERTEX_ELEMENT_POSITION;
		nLocalAttributes ++;
		nVertexStrideFloats += 3;

		if ( nLocalAttributes > nAttributes )
			goto endGetAttributes;
	}
	if ( normals.Count() )
	{
		pAttributes[ nLocalAttributes ].m_nOffsetFloats = nVertexStrideFloats;
		pAttributes[ nLocalAttributes ].m_nType = VERTEX_ELEMENT_NORMAL;
		nLocalAttributes ++;
		nVertexStrideFloats += 3;

		if ( nLocalAttributes > nAttributes )
			goto endGetAttributes;
	}
	if ( texcoords.Count() )
	{
		pAttributes[ nLocalAttributes ].m_nOffsetFloats = nVertexStrideFloats;
		pAttributes[ nLocalAttributes ].m_nType = VERTEX_ELEMENT_TEXCOORD2D_0;
		nLocalAttributes ++;
		nVertexStrideFloats += 2;

		if ( nLocalAttributes > nAttributes )
			goto endGetAttributes;
	}
	if ( tangents.Count() )
	{
		pAttributes[ nLocalAttributes ].m_nOffsetFloats = nVertexStrideFloats;
		pAttributes[ nLocalAttributes ].m_nType = VERTEX_ELEMENT_TANGENT_WITH_FLIP;
		nLocalAttributes ++;
		nVertexStrideFloats += 4;

		if ( nLocalAttributes > nAttributes )
			goto endGetAttributes;
	}
	if ( colors.Count() )
	{
		pAttributes[ nLocalAttributes ].m_nOffsetFloats = nVertexStrideFloats;
		pAttributes[ nLocalAttributes ].m_nType = VERTEX_ELEMENT_COLOR;
		nLocalAttributes ++;
		nVertexStrideFloats += 1;

		if ( nLocalAttributes > nAttributes )
			goto endGetAttributes;
	}

endGetAttributes:
	nAttributes = nLocalAttributes;
}

//-----------------------------------------------------------------------------
// Convert a single CDmeMesh to a CMesh
//-----------------------------------------------------------------------------
bool ConvertMeshFromDMX( CMesh *pMeshOut, CDmeMesh *pDmeMesh )
{
	if ( !pMeshOut || !pDmeMesh )
		return false;

	CDmeVertexData *pDmeBindBaseState = pDmeMesh->GetBindBaseState();
	if ( !pDmeBindBaseState )
		return false;

	CDmeDag *pDmeDag = pDmeMesh->GetParent();
	if ( !pDmeDag )
		return false;

	matrix3x4_t mShapeToWorld;
	pDmeDag->GetShapeToWorldTransform( mShapeToWorld );

	CDMXLoader dmxLoader;
	if ( dmxLoader.LoadMesh( pDmeDag, pDmeMesh, pDmeBindBaseState, mShapeToWorld, 1.0f, 0, NULL ) )
	{
		CUtlVector< CMesh * > &outputMeshes = dmxLoader.GetOutputMeshes();
		if ( outputMeshes.Count() <= 0 )
			return false;

		DuplicateMesh( pMeshOut, *outputMeshes[0] );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Reads the mesh data from the DMX data
//-----------------------------------------------------------------------------
bool CDMXLoader::LoadMesh( CDmeDag *pDmeDag, CDmeMesh *pMesh, CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale,
					 int nBoneAssign, int *pBoneRemap )
{
	matrix3x4_t normalMat;
	MatrixInverseTranspose( mat, normalMat );

	const CUtlVector<Vector> &positions = pBindState->GetPositionData( );
	const CUtlVector<Vector> &normals = pBindState->GetNormalData( );
	const CUtlVector<Vector2D> &texcoords = pBindState->GetTextureCoordData( );
	const CUtlVector<Vector4D> &tangents = pBindState->GetTangentData( );
	const CUtlVector<Color> &colors = pBindState->GetColorData();

	bool bHasPosition = ( positions.Count() > 0 );
	bool bHasNormal = ( normals.Count() > 0 );
	bool bHasTexcoord = ( texcoords.Count() > 0 );
	bool bHasTangent = ( tangents.Count() > 0 );
	bool bHasColor = ( colors.Count() > 0 );
	bool bFlipVCoordinate = pBindState->IsVCoordinateFlipped();

	int nAttributes = 32;
	int nVertexStrideFloats = 0;
	CMeshVertexAttribute attributes[ 32 ];
	GenerateAttributesFromVertexData( attributes, nAttributes, nVertexStrideFloats, pBindState );
	
	// Create a mesh per face set
	int nFaceSetCount = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		CDmeMaterial *pMaterial = pFaceSet->GetMaterial();

		int nIndexCount = pFaceSet->NumIndices();
		int nTrueIndexCount = 0;
		int nFirstIndex = 0;
		while( nFirstIndex < nIndexCount )
		{
			int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
			int nOutCount = ( nVertexCount - 2 ) * 3;
			nTrueIndexCount += nOutCount;
			nFirstIndex += nVertexCount + 1;
		}

		// Each face set is an individual mesh for now
		CMesh *pUtilMesh = new CMesh();

		// Allocate space for VB/IB/Attrib
		pUtilMesh->AllocateMesh( nTrueIndexCount, nTrueIndexCount, nVertexStrideFloats, attributes, nAttributes );
		uint32 *pUtilIndices = pUtilMesh->m_pIndices;
		int nUtilIndices = 0;

		// Set material name
		pUtilMesh->m_materialName = pMaterial->GetMaterialName();

		// Set vertices and indices
		nFirstIndex = 0;
		while( nFirstIndex < nIndexCount )
		{
			int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
			
			if ( nVertexCount >= 3 )
			{
				int nOutCount = ( nVertexCount - 2 ) * 3;
				int pIndices[ 128 ];// = ( int* )_alloca( nOutCount * sizeof( uint32 ) );
				Assert( nOutCount <= 128 );
				pMesh->ComputeTriangulatedIndices( pBindState, pFaceSet, nFirstIndex, pIndices, nOutCount );

				for ( int i=0; i<nOutCount; ++i )
				{
					float *pUtilVertex = pUtilMesh->GetVertex( nUtilIndices );
					int nVertexOffset = 0;
					int iVertIndex = pIndices[ i ];

					if ( bHasPosition )
					{
						int nI = pBindState->GetPositionIndex( iVertIndex );

						Vector vTrans;
						VectorTransform( positions[ nI ], mat, vTrans );
						vTrans *= flScale;

						Q_memcpy( pUtilVertex + nVertexOffset, &vTrans, sizeof( Vector ) );
						nVertexOffset += 3;
					}

					if ( bHasNormal )
					{
						int nI = pBindState->GetNormalIndex( iVertIndex );

						Vector vTrans;
						VectorRotate( normals[ nI ], normalMat, vTrans );
						VectorNormalize( vTrans );

						Q_memcpy( pUtilVertex + nVertexOffset, &vTrans, sizeof( Vector ) );
						nVertexOffset += 3;
					}

					if ( bHasTexcoord )
					{
						int nI = pBindState->GetTexCoordIndex( iVertIndex );

						Vector2D vTrans;
						vTrans = texcoords[ nI ];
						if ( bFlipVCoordinate )
						{
							vTrans.y = 1.0f - vTrans.y;
						}

						Q_memcpy( pUtilVertex + nVertexOffset, &vTrans, sizeof( Vector2D ) );
						nVertexOffset += 2;
					}

					if ( bHasTangent )
					{
						int nI = pBindState->GetTangentIndex( iVertIndex );

						Vector vTrans;
						VectorRotate( tangents[ nI ].AsVector3D(), normalMat, vTrans );
						VectorNormalize( vTrans );
						Vector4D vTrans4D( vTrans.x, vTrans.y, vTrans.z, tangents[ nI ].w );

						Q_memcpy( pUtilVertex + nVertexOffset, &vTrans4D, sizeof( Vector4D ) );
						nVertexOffset += 4;
					}

					if ( bHasColor )
					{
						int nI = pBindState->GetColorIndex( iVertIndex );
						Q_memcpy( pUtilVertex + nVertexOffset, &colors[ nI ], sizeof( uint32 ) );
						nVertexOffset += 1;
					}

					pUtilIndices[ nUtilIndices ] = nUtilIndices;
					nUtilIndices ++;
				}
			}

			nFirstIndex += nVertexCount + 1; // -1 between faces, to skip over it
		}

		// Clean and weld the mesh
		float flEpsilon = 1e-6;
		float *pEpsilons = new float[ pUtilMesh->m_nVertexStrideFloats ];
		for ( int e=0; e<pUtilMesh->m_nVertexStrideFloats; ++e )
		{
			pEpsilons[ e ] = flEpsilon;
		}
		CMesh tempMesh;
		WeldVertices( &tempMesh, *pUtilMesh, pEpsilons, pUtilMesh->m_nVertexStrideFloats );
		delete []pEpsilons;
		pUtilMesh->FreeAllMemory();
		CleanMesh( pUtilMesh, tempMesh );
		tempMesh.FreeAllMemory();

		m_OutputDmeDags.AddToTail( pDmeDag );
		m_outputMeshes.AddToTail( pUtilMesh );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Method used to add mesh data
//-----------------------------------------------------------------------------
bool CDMXLoader::LoadMeshes( const LoadMeshInfo_t &info, CDmeDag *pDag, const matrix3x4_t &parentToBindPose, int nBoneAssign )
{
	// We want to create an aggregate matrix transforming from this dag to its closest
	// parent which actually is an animated joint. This is done so we can autoskin
	// meshes to their closest parents if they have not been skinned.
	matrix3x4_t dagToBindPose;
	int nFoundIndex = info.m_pModel->GetJointIndex( pDag );
	if ( nFoundIndex >= 0 /* && ( pDag == info.m_pModel || CastElement< CDmeJoint >( pDag ) ) */ )
	{
		nBoneAssign = nFoundIndex;
	}

	if ( nFoundIndex >= 0 )
	{
		ConcatTransforms( parentToBindPose, info.m_pBindPose[nFoundIndex], dagToBindPose );
	}
	else
	{
		// NOTE: This isn't particularly kosher; we're using the current pose instead of the bind pose
		// because there's no transform in the bind pose
		matrix3x4_t dagToParent;
		pDag->GetTransform()->GetTransform( dagToParent );
		ConcatTransforms( parentToBindPose, dagToParent, dagToBindPose );
	}

	CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
	if ( pMesh )
	{
		CDmeVertexData *pBindState = pMesh->FindBaseState( "bind" );
		if ( !pBindState )
			return false;

		if ( !LoadMesh( pDag, pMesh, pBindState, dagToBindPose, info.m_flScale, nBoneAssign, info.m_pBoneRemap ) )
			return false;
	}

	int nCount = pDag->GetChildCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !LoadMeshes( info, pChild, dagToBindPose, nBoneAssign ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Method used to add mesh data
//-----------------------------------------------------------------------------
bool CDMXLoader::LoadMeshes( CDmeModel *pModel, float flScale, int *pBoneRemap )
{
	matrix3x4_t mat;
	SetIdentityMatrix( mat );

	LoadMeshInfo_t info;
	info.m_pModel = pModel;
	info.m_flScale = flScale;
	info.m_pBoneRemap = pBoneRemap;

	CDmeTransformList *pBindPose = pModel->FindBaseState( "bind" );
	int nCount = pBindPose ? pBindPose->GetTransformCount() : pModel->GetJointCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeTransform *pTransform = pBindPose ? pBindPose->GetTransform(i) : pModel->GetJointTransform(i);

		matrix3x4_t jointTransform;
		pTransform->GetTransform( info.m_pBindPose[i] );
	}

	int nChildCount = pModel->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pModel->GetChild( i );
		if ( !LoadMeshes( info, pChild, mat, -1 ) )
			return false;
	}

	return true;
}

bool CDMXLoader::LoadDMX( const char *pDMXFile )
{
	bool bRet = true;
	DmFileId_t fileId;

	// When reading, keep the CRLF; this will make ReadFile read it in binary format
	// and also append a couple 0s to the end of the buffer.
	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pDMXFile, NULL, NULL, &pRoot ) == DMFILEID_INVALID )
		return false;

	CDmeModel *pModel = pRoot->GetValueElement< CDmeModel >( "model" );
	if ( !LoadMeshes( pModel, 1.0f, NULL ) )
			return false;

	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return bRet;
}

//-----------------------------------------------------------------------------
// Main entry point for loading DMX files
//-----------------------------------------------------------------------------
bool LoadMeshesFromDMX( CUtlVector<CMesh*> &outputMeshes, const char *pDMXFile )
{
	CDMXLoader loader;
	if ( !loader.LoadDMX( pDMXFile ) )
		return false;

	CUtlVector<CMesh*> &meshList = loader.GetOutputMeshes();
	outputMeshes.AddVectorToTail( meshList );

	return true;
}

bool LoadMeshes( CUtlVector<CMesh*> &outputMeshes, CDmeModel *pModel, float flScale, int *pBoneRemap )
{
	CDMXLoader loader;
	if ( !loader.LoadMeshes( pModel, flScale, pBoneRemap ) )
		return false;

	CUtlVector<CMesh*> &meshList = loader.GetOutputMeshes();
	outputMeshes.AddVectorToTail( meshList );
	return true;
}


//  A stub that loads mesh without splitting position stream by tangent/uv/etc. in source2; here it just loads the mesh as usual
bool LoadCollisionMeshes( CUtlVector< CMesh* > &outputMeshes, CUtlVector< CDmeDag* > &outputDags, CDmeModel *pModel, float flScale )
{
	CDMXLoader loader;//( OUTPUT_MESH_TRIANGLE_MESH );
	//loader.SetFieldFilter( nFieldMask );
	//loader.SetCollapseDeltaMeshes( true );
	//loader.SetKeepPositionConnectivity( true ); // this setting will make loader not merge/split vertices we don't, thus maintaining original connectivity of the mesh
	//loader.SetCollapseMeshesByMaterial( false );
	if ( !loader.LoadMeshes( pModel, flScale ) )
	{
		return false;
	}

	outputMeshes.AddVectorToTail( loader.GetOutputMeshes() );
	outputDags.AddVectorToTail( loader.GetOutputDmeDags() );
	return true;
}

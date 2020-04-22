//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh class dmx saving functions
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
#include "meshutils/mesh.h"

CDmeVertexDataBase::StandardFields_t g_AttribToField[] =
{
	CDmeVertexData::FIELD_POSITION, //VERTEX_ELEMENT_POSITION		= 0,
	CDmeVertexData::FIELD_POSITION, //VERTEX_ELEMENT_POSITION4D	= 1,
	CDmeVertexData::FIELD_NORMAL, //VERTEX_ELEMENT_NORMAL		= 2,
	CDmeVertexData::FIELD_NORMAL, //VERTEX_ELEMENT_NORMAL4D		= 3,
	CDmeVertexData::FIELD_COLOR, //VERTEX_ELEMENT_COLOR		= 4,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_SPECULAR		= 5,
	CDmeVertexData::FIELD_TANGENT, //VERTEX_ELEMENT_TANGENT_S	= 6,
	CDmeVertexData::FIELD_TANGENT, //VERTEX_ELEMENT_TANGENT_T	= 7,
	CDmeVertexData::FIELD_WRINKLE, //VERTEX_ELEMENT_WRINKLE		= 8,
	CDmeVertexData::FIELD_JOINT_INDICES, //VERTEX_ELEMENT_BONEINDICES  = 9,
	CDmeVertexData::FIELD_JOINT_WEIGHTS, //VERTEX_ELEMENT_BONEWEIGHTS1	= 10,
	CDmeVertexData::FIELD_JOINT_WEIGHTS, //VERTEX_ELEMENT_BONEWEIGHTS2	= 11,
	CDmeVertexData::FIELD_JOINT_WEIGHTS, //VERTEX_ELEMENT_BONEWEIGHTS3	= 12,
	CDmeVertexData::FIELD_JOINT_WEIGHTS, //VERTEX_ELEMENT_BONEWEIGHTS4	= 13,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_USERDATA1	= 14,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_USERDATA2	= 15,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_USERDATA3	= 16,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_USERDATA4	= 17,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_0	= 18,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_1	= 19,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_2	= 20,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_3	= 21,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_4	= 22,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_5	= 23,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_6	= 24,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD1D_7	= 25,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_0	= 26,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_1	= 27,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_2	= 28,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_3	= 29,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_4	= 30,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_5	= 31,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_6	= 32,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD2D_7	= 33,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_0	= 34,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_1	= 35,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_2	= 36,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_3	= 37,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_4	= 38,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_5	= 39,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_6	= 40,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD3D_7	= 41,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_0	= 42,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_1	= 43,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_2	= 44,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_3	= 45,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_4	= 46,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_5	= 47,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_6	= 48,
	CDmeVertexData::FIELD_TEXCOORD, //VERTEX_ELEMENT_TEXCOORD4D_7	= 49,
};

DmAttributeType_t g_atForSizeFloats[] = 
{
	AT_FLOAT,
	AT_VECTOR2,
	AT_VECTOR3,
	AT_VECTOR4
};

void ConvertMeshToDMX( CDmeMesh *pDmeMeshOut, CMesh *pMeshIn, bool bForce2DTexcoords )
{
	CDmeVertexData *pVertexData = pDmeMeshOut->FindOrCreateBaseState( "bind" );
	pDmeMeshOut->SetCurrentBaseState( "bind" );
	//pDmeMeshOut->SetBindBaseState( pVertexData );
	pVertexData->AddVertexIndices( pMeshIn->m_nIndexCount );	
	pVertexData->FlipVCoordinate( false );

	// Add the vertices
	for ( int a=0; a<pMeshIn->m_nAttributeCount; ++a )
	{
		// translate CMesh attributes to dme mesh attributes
		CMeshVertexAttribute &attrib = pMeshIn->m_pAttributes[ a ];
		CDmeVertexDataBase::StandardFields_t field = g_AttribToField[ attrib.m_nType ];
		int nSize = GetVertexElementSize( attrib.m_nType, VERTEX_COMPRESSION_NONE );

		if ( bForce2DTexcoords && field == CDmeVertexData::FIELD_TEXCOORD )
		{
			nSize = 8;
		}

		int nFloats = nSize / sizeof( float );
		DmAttributeType_t at = g_atForSizeFloats[ nFloats - 1 ];

		// add a vertex field
		const FieldIndex_t pIndex( pVertexData->CreateField( field ) );
		pVertexData->AddVertexData( pIndex, pMeshIn->m_nVertexCount );

		float *pFieldData = new float[ pMeshIn->m_nVertexCount * nFloats ];
		float *pFieldStart = pFieldData;
		float *pVertStart = pMeshIn->m_pVerts + attrib.m_nOffsetFloats;
		for ( int v=0; v<pMeshIn->m_nVertexCount; ++v )
		{
			Q_memcpy( pFieldStart, pVertStart, nSize );

			pFieldStart += nFloats;
			pVertStart += pMeshIn->m_nVertexStrideFloats;
		}

		pVertexData->SetVertexData( pIndex, 0, pMeshIn->m_nVertexCount, at, pFieldData );
		delete []pFieldData;

		pVertexData->SetVertexIndices( pIndex, 0, pMeshIn->m_nIndexCount, (int*)pMeshIn->m_pIndices );
	}

	// Add a material
	CDmeMaterial *pMaterial = CreateElement< CDmeMaterial >( "material", pDmeMeshOut->GetFileId() );
	pMaterial->SetMaterial( pMeshIn->m_materialName );

	// Add a face set
	CDmeFaceSet *pFaceSet = CreateElement< CDmeFaceSet >( "faceSet", pDmeMeshOut->GetFileId() );
	int nFaceSetIndices = ( pMeshIn->m_nIndexCount / 3 ) * 4;
	pFaceSet->AddIndices( nFaceSetIndices );
	int nIndexCounter = 0;
	for ( int i=0; i<nFaceSetIndices; i+=4 )
	{
		pFaceSet->SetIndex( i,     nIndexCounter++ );
		pFaceSet->SetIndex( i + 1, nIndexCounter++ );
		pFaceSet->SetIndex( i + 2, nIndexCounter++ );
		pFaceSet->SetIndex( i + 3, -1 );
	}

	//pFaceSet->SetIndices( 0, faceSetIndices.Count(), faceSetIndices.Base() );
	pFaceSet->SetMaterial( pMaterial );
	pDmeMeshOut->AddFaceSet( pFaceSet );
}

bool SaveMeshesToDMX( CUtlVector<CMesh*> &inputMeshes, const char *pDMXFile, bool bForce2DTexcoords )
{
	CDisableUndoScopeGuard guard;

	DmFileId_t dmFileId = g_pDataModel->FindOrCreateFileId( pDMXFile );

	CDmElement *pRoot = CreateElement< CDmElement >( "root", dmFileId );
	CDmeModel *pModel = CreateElement< CDmeModel >( "model", dmFileId );

	pRoot->SetValue( "skeleton", pModel );
	pRoot->SetValue( "model", pModel );

	int nMeshes = inputMeshes.Count();
	for ( int m=0; m<nMeshes; ++m )
	{
		CDmeDag *pDmeDag = CreateElement< CDmeDag >( "obj", pRoot->GetFileId() );
		Assert( pDmeDag );
		CDmeMesh *pDmeMesh = CreateElement< CDmeMesh >( "obj", pRoot->GetFileId() );
		Assert( pDmeMesh );

		pDmeDag->SetShape( pDmeMesh );
		pModel->AddJoint( pDmeDag );
		pModel->AddChild( pDmeDag );

		ConvertMeshToDMX( pDmeMesh, inputMeshes[ m ], bForce2DTexcoords );
	}

	pModel->CaptureJointsToBaseState( "bind" );

	const char *pFileFormat = "model";
	if ( !g_pDataModel->SaveToFile( pDMXFile, NULL, NULL, pFileFormat, pRoot ) )
	{
		Warning( "SaveMeshesToDMX: SaveToFile \"%s\" failed!\n", pDMXFile );
		return false;
	}
	
	return true;
}
//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Loads mesh data from dmx files
//
// $NoKeywords: $
//
//===========================================================================//


// Valve includes
#include "bspflags.h"

#include "movieobjects/dmeattributereference.h"
#include "movieobjects/dmeconnectionoperator.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeattachment.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmerigconstraintoperators.h"
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
#include "mdlobjects/dmeeyelid.h"
#include "mdlobjects/dmeboneweight.h"
#include "mdlobjects/dmebonemask.h"
#include "mdlobjects/dmebonemasklist.h"
#include "mdlobjects/dmeik.h"
#include "mdlobjects/dmeanimcmd.h"
#include "mdlobjects/dmemotioncontrol.h"
#include "mdlobjects/dmeposeparameter.h"
#include "mdlobjects/dmeposeparameterlist.h"
#include "mdlobjects/dmeanimblocksize.h"
#include "mdlobjects/dmeboneflexdriver.h"
#include "mdlobjects/dmejigglebone.h"
#include "mdlobjects/dmemouth.h"
#include "fbxutils/dmfbxserializer.h"

#include "mdlobjects/mpp_utils.h"

// Local includes
#include "studiomdl.h"
#include "collisionmodel.h"

#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// The current model being loaded...
//-----------------------------------------------------------------------------
s_model_t *g_pCurrentModel = NULL;


void UnifyIndices( s_source_t *psource );


//-----------------------------------------------------------------------------
// Mapping of bone transforms
//-----------------------------------------------------------------------------
struct BoneTransformMap_t
{
	// Number of bones
	int m_nBoneCount;

	// The order in which transforms appear in this list specifies their bone indices
	CDmeTransform *m_ppTransforms[MAXSTUDIOSRCBONES];

	// m_pnDmeModelToMdl[bone index in DmeModel] == bone index in studiomdl
	int	m_pnDmeModelToMdl[MAXSTUDIOSRCBONES];

	// m_pnMdlToDmeModel[bone index in studiomdl] == bone index in DmeModel
	int	m_pnMdlToDmeModel[MAXSTUDIOSRCBONES];
};


//-----------------------------------------------------------------------------
// Index into an s_node_t array for the default root node
//-----------------------------------------------------------------------------
static int s_nDefaultRootNode;


//-----------------------------------------------------------------------------
// Balance/speed data
//-----------------------------------------------------------------------------
static CUtlVector<float> s_Balance;
static CUtlVector<float> s_Speed;


//-----------------------------------------------------------------------------
// List of unique vertices
//-----------------------------------------------------------------------------
struct VertIndices_t
{
	int v;
	int n;
	int t[MAXSTUDIOTEXCOORDS];
	int balance;
	int speed;
};

static CUtlVector< VertIndices_t > s_UniqueVertices;	// A list of the unique vertices in the mesh

// Given the non-unique vertex index, return the unique vertex index
// The indices are absolute indices into s_UniqueVertices
// But as both arrays contain information for all meshes in the DMX
// The proper offset for the desired mesh must be added to the lookup
// into the map but the value returned has the offset already built in
static CUtlVector< int > s_UniqueVerticesMap;


//-----------------------------------------------------------------------------
// Delta state intermediate data [used for positions, normals, etc.]
//-----------------------------------------------------------------------------
struct DeltaIndex_t
{
	DeltaIndex_t() : m_nPositionIndex(-1), m_nNormalIndex(-1), m_nNextDelta(-1), m_nWrinkleIndex(-1), m_bInList(false) {}
	int m_nPositionIndex;	// Index into DeltaState_t::m_PositionDeltas
	int m_nNormalIndex;		// Index into DeltaState_t::m_NormalDeltas
	int m_nWrinkleIndex;	// Index into DeltaState_t::m_WrinkleDeltas
	int m_nNextDelta;		// Index into DeltaState_t::m_DeltaIndices;
	bool m_bInList;
};

struct DeltaState_t
{
	DeltaState_t() : m_nDeltaCount( 0 ), m_nFirstDelta( -1 ) {}

	CUtlString m_Name;
	CUtlVector< Vector > m_PositionDeltas;
	CUtlVector< Vector > m_NormalDeltas;
	CUtlVector< float > m_WrinkleDeltas;
	CUtlVector< DeltaIndex_t > m_DeltaIndices;
	int m_nDeltaCount;
	int m_nFirstDelta;
};


// NOTE: This is a temporary which loses its state once Load_DMX is exited.
static CUtlVector<DeltaState_t> s_DeltaStates;


// Finds or adds delta states. These pointers are invalidated by calling FindOrAddDeltaState again
//-----------------------------------------------------------------------------
static DeltaState_t* FindOrAddDeltaState( const char *pDeltaStateName, int nBaseStateVertexCount )
{
	int nCount = s_DeltaStates.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( s_DeltaStates[i].m_Name, pDeltaStateName ) )
		{
			MdlWarning( "Unsupported duplicate delta state named \"%s\" in DMX file\n", pDeltaStateName );

			s_DeltaStates[i].m_DeltaIndices.EnsureCount( nBaseStateVertexCount );
			return &s_DeltaStates[i];
		}
	}

	int j = s_DeltaStates.AddToTail();
	s_DeltaStates[j].m_Name = pDeltaStateName;
	s_DeltaStates[j].m_DeltaIndices.SetCount( nBaseStateVertexCount );
	return &s_DeltaStates[j];
}


struct VertexLookup_t
{
	int v, n, t;
	int index;
};

static bool VertexLookup_CompareFunc( VertexLookup_t const &a, VertexLookup_t const &b )
{
	return ( ( a.v == b.v ) && ( a.n == b.n ) && ( a.t == b.t ) );
}

static unsigned int VertexLookup_KeyFunc( VertexLookup_t const &a )
{
	return Hash12( &a );
}

//-----------------------------------------------------------------------------
// Loads the vertices from the model
//-----------------------------------------------------------------------------
static bool DefineUniqueVertices( CDmeVertexData *pBindState )
{
	const CUtlVector<int> &positionIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );
	const CUtlVector<int> &normalIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_NORMAL );
	const CUtlVector<int> &texcoordIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_TEXCOORD );
	const CUtlVector<int> &balanceIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_BALANCE );
	const CUtlVector<int> &speedIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_MORPH_SPEED );

	int nPositionCount = positionIndices.Count();
	int nNormalCount = normalIndices.Count();
	int nTexcoordCount = texcoordIndices.Count();
	int nBalanceCount = balanceIndices.Count();
	int nSpeedCount = speedIndices.Count();

	int nExtraTexcoordCount[MAXSTUDIOTEXCOORDS-1];
	const CUtlVector<int> * extraTexcoordIndices[MAXSTUDIOTEXCOORDS - 1];

	for (int i = 1; i < MAXSTUDIOTEXCOORDS; ++i)
	{
		FieldIndex_t nExtra = pBindState->FindFieldIndex(CFmtStr("texcoord$%d", i).Get());
		if (nExtra != -1)
		{
			extraTexcoordIndices[i-1] = &pBindState->GetVertexIndexData(nExtra);
			nExtraTexcoordCount[i-1] = extraTexcoordIndices[i-1]->Count();
			if (nPositionCount != nExtraTexcoordCount[i-1])
			{
				MdlError("Encountered a mesh with invalid geometry (different number of indices for various data fields)\n");
				return false;
			}
		}
		else
		{
			extraTexcoordIndices[i-1] = NULL;
			nExtraTexcoordCount[i-1] = 0;
		}
	}

	if ( nNormalCount && nPositionCount != nNormalCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}
	if ( nTexcoordCount && nPositionCount != nTexcoordCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}
	if ( nBalanceCount && nPositionCount != nBalanceCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}
	if ( nSpeedCount && nPositionCount != nSpeedCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}

	// Make a hash table to speed up this de-duplication process:
	CUtlHash< VertexLookup_t > vertexLookupHash( nPositionCount, 0, 0, VertexLookup_CompareFunc, VertexLookup_KeyFunc );

	// Only add unique vertices to the list as in UnifyIndices
	for ( int i = 0; i < nPositionCount; ++i )
	{
		VertIndices_t vert;
		vert.v = g_numverts + positionIndices[i];
		vert.n = ( nNormalCount > 0 ) ? g_numnormals + normalIndices[i] : -1;
		vert.t[0] = ( nTexcoordCount > 0 ) ? g_numtexcoords[0] + texcoordIndices[i] : -1;
		vert.balance = s_Balance.Count() + ( ( nBalanceCount > 0 ) ? balanceIndices[i] : 0 );
		vert.speed = s_Speed.Count() + ( ( nSpeedCount > 0 ) ? speedIndices[i] : 0 );
		for (int j = 1; j < MAXSTUDIOTEXCOORDS; ++j)
		{
			vert.t[j] = (nExtraTexcoordCount[j-1] > 0) ? g_numtexcoords[j] + extraTexcoordIndices[j-1]->Element(i) : -1;
		}

		VertexLookup_t vertexLookup = { vert.v, vert.n, vert.t[0], -1 };
		UtlHashHandle_t vertexHandle = vertexLookupHash.Find( vertexLookup );
		if ( vertexHandle == vertexLookupHash.InvalidHandle() )
		{
			// Unique
			int k = s_UniqueVertices.AddToTail();
			s_UniqueVertices[k] = vert;
			s_UniqueVerticesMap.AddToTail( k );
			vertexLookup.index = k;
			vertexLookupHash.Insert( vertexLookup );
		}
		else
		{
			// Not unique
			VertexLookup_t &equivalentVertex = vertexLookupHash.Element( vertexHandle );
			s_UniqueVerticesMap.AddToTail( equivalentVertex.index );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the vertices from the model
//-----------------------------------------------------------------------------
static bool LoadVertices( CDmeDag *pDmeDag, CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale, int nBoneAssign, int *pBoneRemap, s_source_t *pSource )
{
	// nBoneAssign is only used if the mesh has no skinning information
	// It's the DMX bone index, but it might be < 0, in which case
	// no bone has been defined yet.  There are two options, use the
	// default root bone which is always defined and at the origin
	// or try and use the DmeDag's bone that was created when the mesh was
	// loaded
	if ( nBoneAssign < 0 )
	{
		nBoneAssign = s_nDefaultRootNode;
	}
	else
	{
		nBoneAssign = pBoneRemap[ nBoneAssign ];
		if ( nBoneAssign < 0 )
		{
			nBoneAssign = s_nDefaultRootNode;
		}
	}

	// Used by the morphing system to set up delta states
	DefineUniqueVertices( pBindState );

	matrix3x4_t normalMat;
	MatrixInverseTranspose( mat, normalMat );

	const CUtlVector<Vector> &positions = pBindState->GetPositionData( );
	const CUtlVector<Vector> &normals = pBindState->GetNormalData( );
	const CUtlVector<Vector2D> &texcoords = pBindState->GetTextureCoordData( );
	const CUtlVector<float> &balances = pBindState->GetBalanceData( );
	const CUtlVector<float> &speeds = pBindState->GetMorphSpeedData( );

	int nCount = positions.Count();

	int nJointCount = pBindState->HasSkinningData() ? pBindState->JointCount() : 0;
	if ( nJointCount > MAXSTUDIOBONEWEIGHTS )
	{
		MdlError( "Too many bone influences per vertex!\n" );
		return false;
	}

	if ( nJointCount <= 0 && nBoneAssign == s_nDefaultRootNode && pDmeDag )
	{
		// Use the bone created for the DmeDag node of the DmeMesh
		for ( int i = 0; i < pSource->numbones; ++i )
		{
			if ( !Q_strcmp( pDmeDag->GetName(), pSource->localBone[ i ].name ) )
			{
				nBoneAssign = i;
				break;
			}
		}
	}

	bool pbWarnmap[ MAXSTUDIOBONES ];
	Q_memset( pbWarnmap, 0, MAXSTUDIOBONES * sizeof( bool ) );

	float *pWeightBuf = (float *)malloc( nJointCount * sizeof( float ) );
	int   *pIndexBuf  = (int   *)malloc( nJointCount * sizeof( int   ) );

	// Copy positions + bone info
	for ( int i = 0; i < nCount; ++i )
	{
		// NOTE: The transform transforms the positions into the bind space
		VectorTransform( positions[i], mat, g_vertex[g_numverts] );
		g_vertex[g_numverts] *= flScale;
		if ( nJointCount == 0 )
		{
			g_bone[g_numverts].numbones = 1;
			g_bone[g_numverts].bone[0] = nBoneAssign;
			g_bone[g_numverts].weight[0] = 1.0;
		}
		else
		{
			const float *pJointWeights = pBindState->GetJointWeightData( i );
			const int *pJointIndices = pBindState->GetJointIndexData( i );

			memcpy( pWeightBuf, pJointWeights, nJointCount * sizeof(float) );
			memcpy( pIndexBuf, pJointIndices, nJointCount * sizeof(int) );

			int nBoneCount = SortAndBalanceBones( nJointCount, MAXSTUDIOBONEWEIGHTS, pIndexBuf, pWeightBuf );
			int nBoneIndex = -1;

			g_bone[g_numverts].numbones = nBoneCount;
			for ( int j = 0; j < nBoneCount; ++j )
			{
				nBoneIndex = pBoneRemap[ pIndexBuf[j] ];
				if ( nBoneIndex < 0 )
				{
					if ( pIndexBuf[j] < MAXSTUDIOBONES && !pbWarnmap[ pIndexBuf[j] ] )
					{
						MdlWarning( "DmeMesh[%s] Verts Assigned To DmeModel.jointList[%d] Which Isn't Mapped To The Dag Hierarchy\n",
							pDmeDag->GetName(), pIndexBuf[j] );
						pbWarnmap[ pIndexBuf[j] ] = true;
					}
					g_bone[g_numverts].bone[j] = nBoneAssign;
				}
				else
				{
					g_bone[g_numverts].bone[j] = nBoneIndex;
				}
				g_bone[g_numverts].weight[j] = pWeightBuf[j];
			}
		}
		++g_numverts;
	}

	free( pWeightBuf );
	free( pIndexBuf  );

	// Copy normals
	Vector vNormal;
	nCount = normals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		VectorCopy( normals[i], vNormal );
		VectorNormalize( vNormal );
		if ( fabs( VectorLength( vNormal ) - 1.0f ) > 0.01 )
		{
			MdlWarning( "Non-Unit Length Normal [%d] < %8.6f %8.6f %8.6f >\n", i, vNormal.x, vNormal.y, vNormal.z );
		}
		VectorRotate( vNormal, normalMat, g_normal[g_numnormals] );
		++g_numnormals;
	}

	// Copy texcoords
	nCount = texcoords.Count();
	bool bFlipVCoordinate = pBindState->IsVCoordinateFlipped();
	for ( int i = 0; i < nCount; ++i )
	{
		g_texcoord[0][g_numtexcoords[0]].x = texcoords[i].x;
		g_texcoord[0][g_numtexcoords[0]].y = bFlipVCoordinate ? 1.0f - texcoords[i].y : texcoords[i].y;
		++g_numtexcoords[0];
	}

	// Check for additional texcoords
	for (int i = 1; i < MAXSTUDIOTEXCOORDS; ++i)
	{
		g_numtexcoords[i] = 0;
		FieldIndex_t nFieldIndex = pBindState->FindFieldIndex(CFmtStr("texcoord$%d", i).Get());
		if (nFieldIndex > -1)
		{
			CDmrArrayConst<Vector2D> vertexData = pBindState->GetVertexData(nFieldIndex);
			const CUtlVector<Vector2D> &extraTexcoords = vertexData.Get();
			nCount = extraTexcoords.Count();
			for (int j = 0; j < nCount; ++j)
			{
				g_texcoord[i][g_numtexcoords[i]].x = extraTexcoords[j].x;
				g_texcoord[i][g_numtexcoords[i]].y = bFlipVCoordinate ? 1.0f - extraTexcoords[j].y : extraTexcoords[j].y;
				++g_numtexcoords[i];
			}
		}
		else
		{
			break;
		}
	}

	// In the event of no speed or balance map, use the same value of 1 for all vertices
	if ( balances.Count() )
	{
		s_Balance.AddMultipleToTail( balances.Count(), balances.Base() );
	}
	else
	{
		s_Balance.AddToTail( 1.0f );
	}

	if ( speeds.Count() )
	{
		s_Speed.AddMultipleToTail( speeds.Count(), speeds.Base() );
	}
	else
	{
		s_Speed.AddToTail( 1.0f );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Hook delta into delta list
//-----------------------------------------------------------------------------
static void AddToDeltaList( DeltaState_t *pDeltaStateData, int nUniqueVertex )
{
	DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertex ];
	if ( !index.m_bInList )
	{
		index.m_nNextDelta = pDeltaStateData->m_nFirstDelta;
		pDeltaStateData->m_nFirstDelta = nUniqueVertex;
		pDeltaStateData->m_nDeltaCount++;
		index.m_bInList = true;
	}
}


//-----------------------------------------------------------------------------
// Loads the vertices from the delta state
//-----------------------------------------------------------------------------
static bool LoadDeltaState(
						   CDmeVertexDeltaData *pDeltaState,
						   CDmeVertexData *pBindState,
						   const matrix3x4_t& mat,
						   float flScale,
						   int nStartingUniqueVertex,
						   int nStartingUniqueVertexMap )
{
	DeltaState_t *pDeltaStateData = FindOrAddDeltaState( pDeltaState->GetName(), nStartingUniqueVertex + pBindState->VertexCount() );

	matrix3x4_t normalMat;
	MatrixInverseTranspose( mat, normalMat );

	const CUtlVector<Vector> &positions = pDeltaState->GetPositionData( );
	const CUtlVector<int> &positionIndices = pDeltaState->GetVertexIndexData( CDmeVertexDataBase::FIELD_POSITION );
	const CUtlVector<Vector> &normals = pDeltaState->GetNormalData( );
	const CUtlVector<int> &normalIndices = pDeltaState->GetVertexIndexData( CDmeVertexDataBase::FIELD_NORMAL );
	const CUtlVector<float> &wrinkle = pDeltaState->GetWrinkleData( );
	const CUtlVector<int> &wrinkleIndices = pDeltaState->GetVertexIndexData( CDmeVertexDataBase::FIELD_WRINKLE );

	if ( positions.Count() != positionIndices.Count() )
	{
		MdlError( "DeltaState %s contains a different number of positions + position indices!\n", pDeltaState->GetName() );
		return false;
	}

	if ( normals.Count() != normalIndices.Count() )
	{
		MdlError( "DeltaState %s contains a different number of normals + normal indices!\n", pDeltaState->GetName() );
		return false;
	}

	if ( wrinkle.Count() != wrinkleIndices.Count() )
	{
		MdlError( "DeltaState %s contains a different number of wrinkles + wrinkle indices!\n", pDeltaState->GetName() );
		return false;
	}

	// Copy position delta
	int nCount = positions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		Vector vecDelta;

		// NOTE NOTE!!: This is VectorRotate, *not* VectorTransform. This is because
		// we're transforming a delta, which is basically a direction vector. To
		// move it into the new space, we must rotate it
		VectorRotate( positions[i], mat, vecDelta );
		vecDelta *= flScale;

		int nPositionIndex = pDeltaStateData->m_PositionDeltas.AddToTail( vecDelta );

		// Indices
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, positionIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int k = 0; k < nBaseVertCount; ++k )
		{
			int nUniqueVertexIndex = s_UniqueVerticesMap[ nStartingUniqueVertexMap + baseVerts[k] ];
			AddToDeltaList( pDeltaStateData, nUniqueVertexIndex );
			DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertexIndex ];
			index.m_nPositionIndex = nPositionIndex;
		}
	}

	// Copy normals
	nCount = normals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		Vector vecDelta;
		VectorRotate( normals[i], normalMat, vecDelta );
		int nNormalIndex = pDeltaStateData->m_NormalDeltas.AddToTail( vecDelta );

		// Indices
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_NORMAL, normalIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int k = 0; k < nBaseVertCount; ++k )
		{
			int nUniqueVertexIndex = s_UniqueVerticesMap[ nStartingUniqueVertexMap + baseVerts[k] ];
			AddToDeltaList( pDeltaStateData, nUniqueVertexIndex );
			DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertexIndex ];
			index.m_nNormalIndex = nNormalIndex;
		}
	}

	// Copy wrinkle
	nCount = wrinkle.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		int nWrinkleIndex = pDeltaStateData->m_WrinkleDeltas.AddToTail( wrinkle[i] );

		// Indices
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_WRINKLE, wrinkleIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int k = 0; k < nBaseVertCount; ++k )
		{
			int nUniqueVertexIndex = s_UniqueVerticesMap[ nStartingUniqueVertexMap + baseVerts[k] ];
			AddToDeltaList( pDeltaStateData, nUniqueVertexIndex );
			DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertexIndex ];
			index.m_nWrinkleIndex = nWrinkleIndex;
		}
	}
	return true;
}


static int GetExtraTexcoordIndex(CDmeVertexData *pVertexData, int nVertexIndex, int nChannel)
{
	FieldIndex_t nFieldIndex = pVertexData->FindFieldIndex(CFmtStr("texcoord$%d", nChannel).Get());
	if (nFieldIndex < 0)
		return -1;

	CDmrArrayConst<int> indices(pVertexData->GetIndexData(nFieldIndex));
	return indices[nVertexIndex];
}

// JasonM TODO: unify ParseQuadFaceData() and ParseFaceData()

//-----------------------------------------------------------------------------
// Reads the quad face data from the DMX data
//-----------------------------------------------------------------------------
static void ParseQuadFaceData( CDmeVertexData *pVertexData, int material, int *pIndices, int vi, int ni, int* ti )
{
	s_tmpface_t f;
	f.material = material;

	int p, n, t;
	p = pVertexData->GetPositionIndex(pIndices[0]); n = pVertexData->GetNormalIndex(pIndices[0]); t = pVertexData->GetTexCoordIndex(pIndices[0]);
	f.a = (p >= 0) ? vi + p : 0; f.na = (n >= 0) ? ni + n : 0; f.ta[0] = (t >= 0) ? ti[0] + t : 0;
	p = pVertexData->GetPositionIndex(pIndices[3]); n = pVertexData->GetNormalIndex(pIndices[3]); t = pVertexData->GetTexCoordIndex(pIndices[3]);
	f.b = (p >= 0) ? vi + p : 0; f.nb = (n >= 0) ? ni + n : 0; f.tb[0] = (t >= 0) ? ti[0] + t : 0;
	p = pVertexData->GetPositionIndex(pIndices[2]); n = pVertexData->GetNormalIndex(pIndices[2]); t = pVertexData->GetTexCoordIndex(pIndices[2]);
	f.c = (p >= 0) ? vi + p : 0; f.nc = (n >= 0) ? ni + n : 0; f.tc[0] = (t >= 0) ? ti[0] + t : 0;
	p = pVertexData->GetPositionIndex(pIndices[1]); n = pVertexData->GetNormalIndex(pIndices[1]); t = pVertexData->GetTexCoordIndex(pIndices[1]);
	f.d = (p >= 0) ? vi + p : 0; f.nd = (n >= 0) ? ni + n : 0; f.td[0] = (t >= 0) ? ti[0] + t : 0;

	Assert( f.a  <= (unsigned long)g_numverts     && f.b  <= (unsigned long)g_numverts     && f.c  <= (unsigned long)g_numverts     && f.d  <= (unsigned long)g_numverts     );
	Assert( f.na <= (unsigned long)g_numnormals   && f.nb <= (unsigned long)g_numnormals   && f.nc <= (unsigned long)g_numnormals   && f.nd <= (unsigned long)g_numnormals   );
	Assert(f.ta[0] <= (unsigned long)g_numtexcoords[0] && f.tb[0] <= (unsigned long)g_numtexcoords[0] && f.tc[0] <= (unsigned long)g_numtexcoords[0] && f.td[0] <= (unsigned long)g_numtexcoords[0]);

	for (int i = 1; i < (MAXSTUDIOTEXCOORDS); ++i)
	{
		t = GetExtraTexcoordIndex(pVertexData, pIndices[0], i);
		f.ta[i] = (t >= 0) ? ti[i] + t : 0;
		t = GetExtraTexcoordIndex(pVertexData, pIndices[3], i);
		f.tb[i] = (t >= 0) ? ti[i] + t : 0;
		t = GetExtraTexcoordIndex(pVertexData, pIndices[2], i);
		f.tc[i] = (t >= 0) ? ti[i] + t : 0;
		t = GetExtraTexcoordIndex(pVertexData, pIndices[1], i);
		f.td[i] = (t >= 0) ? ti[i] + t : 0;
		Assert(f.ta[i] <= (unsigned long)g_numtexcoords[i] && f.tb[i] <= (unsigned long)g_numtexcoords[i] && f.tc[i] <= (unsigned long)g_numtexcoords[i] && f.td[i] <= (unsigned long)g_numtexcoords[i]);
	}

	int i = g_numfaces++;
	g_face[i] = f;
}


//-----------------------------------------------------------------------------
// Reads the face data from the DMX data
//-----------------------------------------------------------------------------
static void ParseFaceData( CDmeVertexData *pVertexData, int material, int v1, int v2, int v3, int vi, int ni, int* ti )
{
	s_tmpface_t f;
	f.material = material;

	int p, n, t;
	p = pVertexData->GetPositionIndex(v1); n = pVertexData->GetNormalIndex(v1); t = pVertexData->GetTexCoordIndex(v1);
	f.a = (p >= 0) ? vi + p : 0; f.na = (n >= 0) ? ni + n : 0; f.ta[0] = (t >= 0) ? ti[0] + t : 0;
	p = pVertexData->GetPositionIndex(v2); n = pVertexData->GetNormalIndex(v2); t = pVertexData->GetTexCoordIndex(v2);
	f.b = (p >= 0) ? vi + p : 0; f.nb = (n >= 0) ? ni + n : 0; f.tb[0] = (t >= 0) ? ti[0] + t : 0;
	p = pVertexData->GetPositionIndex(v3); n = pVertexData->GetNormalIndex(v3); t = pVertexData->GetTexCoordIndex(v3);
	f.c = (p >= 0) ? vi + p : 0; f.nc = (n >= 0) ? ni + n : 0; f.tc[0] = (t >= 0) ? ti[0] + t : 0;

	Assert( f.a <= (unsigned long)g_numverts && f.b <= (unsigned long)g_numverts && f.c <= (unsigned long)g_numverts );
	Assert( f.na <= (unsigned long)g_numnormals && f.nb <= (unsigned long)g_numnormals && f.nc <= (unsigned long)g_numnormals );
	Assert(f.ta[0] <= (unsigned long)g_numtexcoords[0] && f.tb[0] <= (unsigned long)g_numtexcoords[0] && f.tc[0] <= (unsigned long)g_numtexcoords[0]);

	for (int i = 1; i < (MAXSTUDIOTEXCOORDS); ++i)
	{
		t = GetExtraTexcoordIndex(pVertexData, v1, i);
		f.ta[i] = (t >= 0) ? ti[i] + t : 0;
		t = GetExtraTexcoordIndex(pVertexData, v2, i);
		f.tb[i] = (t >= 0) ? ti[i] + t : 0;
		t = GetExtraTexcoordIndex(pVertexData, v3, i);
		f.tc[i] = (t >= 0) ? ti[i] + t : 0;
		Assert(f.ta[i] <= (unsigned long)g_numtexcoords[i] && f.tb[i] <= (unsigned long)g_numtexcoords[i] && f.tc[i] <= (unsigned long)g_numtexcoords[i]);
	}

	int i = g_numfaces++;
	g_face[i] = f;
}


//-----------------------------------------------------------------------------
// Reads the mesh data from the DMX data
//-----------------------------------------------------------------------------
static bool LoadMesh( CDmeDag *pDmeDag, CDmeMesh *pMesh, CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale,
					 int nBoneAssign, int *pBoneRemap, s_source_t *pSource )
{
	pMesh->CollapseRedundantNormals( normal_blend );

	// Load the vertices
	int nStartingVertex = g_numverts;
	int nStartingNormal = g_numnormals;
	int nStartingUniqueCount = s_UniqueVertices.Count();
	int nStartingUniqueMapCount = s_UniqueVerticesMap.Count();
	int nStartingTexCoord[MAXSTUDIOTEXCOORDS];
	for (int i = 0; i < MAXSTUDIOTEXCOORDS; ++i)
	{
		nStartingTexCoord[i] = g_numtexcoords[i];
	}

	// This defines s_UniqueVertices & s_UniqueVerticesMap
	LoadVertices( pDmeDag, pBindState, mat, flScale, nBoneAssign, pBoneRemap, pSource );

	// Load the deltas
	int nDeltaStateCount = pMesh->DeltaStateCount();
	for ( int i = 0; i < nDeltaStateCount; ++i )
	{
		CDmeVertexDeltaData *pDeltaState = pMesh->GetDeltaState( i );
		if ( !LoadDeltaState( pDeltaState, pBindState, mat, flScale, nStartingUniqueCount, nStartingUniqueMapCount ) )
			return false;
	}

	// load the base triangles
	int texture;
	int material;
	char pTextureName[MAX_PATH];

	int nFaceSetCount = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		CDmeMaterial *pMaterial = pFaceSet->GetMaterial();

		// Get the material name
		Q_strncpy( pTextureName, pMaterial->GetMaterialName(), sizeof(pTextureName) );

		// funky texture overrides (specified with the -t command-line argument)
		for ( int j = 0; j < numrep; j++ )
		{
			if ( sourcetexture[j][0] == '\0' )
			{
				Q_strncpy( pTextureName, defaulttexture[j], sizeof(pTextureName) );
				break;
			}
			if ( Q_stricmp( pTextureName, sourcetexture[j]) == 0 )
			{
				Q_strncpy( pTextureName, defaulttexture[j], sizeof(pTextureName) );
				break;
			}
		}

		// skip all faces with the null texture on them.
		char pPathNoExt[MAX_PATH];
		Q_StripExtension( pTextureName, pPathNoExt, sizeof(pPathNoExt) );
		if ( !Q_stricmp( pPathNoExt, "null" ) )
			continue;

		texture = LookupTexture( pTextureName, true );
		pSource->texmap[texture] = texture;	// hack, make it 1:1
		material = UseTextureAsMaterial( texture );

		// Is this a quad-only subd?
		bool bQuadSubd = ( gflags & STUDIOHDR_FLAGS_SUBDIVISION_SURFACE ) != 0;

		// prepare indices
		int nFirstIndex = 0;
		int nIndexCount = pFaceSet->NumIndices();
		while ( nFirstIndex < nIndexCount )
		{
			int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );

			// Quad subd face?
			if ( bQuadSubd && ( nVertexCount == 4 ) )
			{
				int quadIndices[4];

				for ( int j = 0; j < 4; j++ )
				{
					quadIndices[j] = pFaceSet->GetIndex( nFirstIndex + j );
				}

				ParseQuadFaceData( pBindState, material, quadIndices, nStartingVertex, nStartingNormal, nStartingTexCoord );

				nFirstIndex += 5; // -1 in list between face indices, so jump over 5 elements, not 4
			}
			else
			{
				if ( nVertexCount >= 3 )
				{
					int nOutCount = (nVertexCount-2) * 3;
					int *pIndices = (int*)malloc( nOutCount * sizeof(int) );
					pMesh->ComputeTriangulatedIndices( pBindState, pFaceSet, nFirstIndex, pIndices, nOutCount );
					for ( int ii = 0; ii < nOutCount; ii +=3 )
					{
						ParseFaceData( pBindState, material, pIndices[ii], pIndices[ii+2], pIndices[ii+1], nStartingVertex, nStartingNormal, nStartingTexCoord );
					}
					free( pIndices );
				}

				nFirstIndex += nVertexCount + 1; // -1 in list between face indices, so jump over nVertexCount + 1 elements, not nVertexCount elements
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Method used to add mesh data
//-----------------------------------------------------------------------------
struct LoadMeshInfo_t
{
	s_source_t *m_pSource;
	CDmeModel *m_pModel;
	float m_flScale;
	int *m_pBoneRemap;
	matrix3x4_t m_pBindPose[MAXSTUDIOSRCBONES];
};

static bool LoadMeshes( const LoadMeshInfo_t &info, CDmeDag *pDag, const matrix3x4_t &parentToBindPose, int nBoneAssign )
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

		if ( !LoadMesh( pDag, pMesh, pBindState, dagToBindPose, info.m_flScale, nBoneAssign, info.m_pBoneRemap, info.m_pSource ) )
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
static bool LoadMeshes( CDmeModel *pModel, float flScale, int *pBoneRemap, s_source_t *pSource )
{
	matrix3x4_t mat;
	SetIdentityMatrix( mat );

	LoadMeshInfo_t info;
	info.m_pModel = pModel;
	info.m_flScale = flScale;
	info.m_pBoneRemap = pBoneRemap;
	info.m_pSource = pSource;

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


//-----------------------------------------------------------------------------
// Builds s_vertanim_ts
//-----------------------------------------------------------------------------
static void BuildVertexAnimations( s_source_t *pSource )
{
	int nCount = s_DeltaStates.Count();
	if ( nCount == 0 )
		return;

	Assert( s_Speed.Count() > 0 );
	Assert( s_Balance.Count() > 0 );

	Assert( s_UniqueVertices.Count() == g_numvlist );
	s_vertanim_t *pVertAnim = (s_vertanim_t *)malloc( g_numvlist * sizeof( s_vertanim_t ) );
	for ( int i = 0; i < nCount; ++i )
	{
		DeltaState_t &state = s_DeltaStates[i];

		s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, state.m_Name );
		pSourceAnim->numframes = 1;
		pSourceAnim->startframe = 0;
		pSourceAnim->endframe = 0;
		pSourceAnim->newStyleVertexAnimations = true;

		// Traverse the linked list of unique vertex indices j that has a delta
		int nVertAnimCount = 0;
		for ( int j = state.m_nFirstDelta; j >= 0; j = state.m_DeltaIndices[j].m_nNextDelta )
		{
			// The Delta Indices array is a parallel array to s_UniqueVertices
			// j is used to index into both
			DeltaIndex_t &delta = state.m_DeltaIndices[j];
			Assert( delta.m_nPositionIndex >= 0 || delta.m_nNormalIndex >= 0 || delta.m_nWrinkleIndex >= 0 );

			VertIndices_t &uniqueVert = s_UniqueVertices[j];

			const v_unify_t *pList = v_list[uniqueVert.v];
			for( ; pList; pList = pList->next )
			{
				if ( pList->n != uniqueVert.n || pList->t[0] != uniqueVert.t[0] )
					continue;

				s_vertanim_t& vertanim = pVertAnim[nVertAnimCount++];
				vertanim.vertex = pList - v_listdata;
				vertanim.speed = s_Speed[ s_UniqueVertices[j].speed ];
				vertanim.side = s_Balance[ s_UniqueVertices[j].balance ];
				if ( delta.m_nPositionIndex >= 0 )
				{
					vertanim.pos = state.m_PositionDeltas[ delta.m_nPositionIndex ];
				}
				else
				{
					vertanim.pos = vec3_origin;
				}
				if ( delta.m_nNormalIndex >= 0 )
				{
					vertanim.normal = state.m_NormalDeltas[ delta.m_nNormalIndex ];
				}
				else
				{
					vertanim.normal = vec3_origin;
				}

				if ( delta.m_nWrinkleIndex >= 0 )
				{
					vertanim.wrinkle = state.m_WrinkleDeltas[ delta.m_nWrinkleIndex ];
				}
				else
				{
					vertanim.wrinkle = 0.0f;
				}
			}
		}
		pSourceAnim->numvanims[0] = nVertAnimCount;
		pSourceAnim->vanim[0] = (s_vertanim_t *)calloc( nVertAnimCount, sizeof( s_vertanim_t ) );
		memcpy( pSourceAnim->vanim[0], pVertAnim, nVertAnimCount * sizeof( s_vertanim_t ) );
	}
	free( pVertAnim );
}


//-----------------------------------------------------------------------------
// Handles DmeJiggleBones
//-----------------------------------------------------------------------------
static void HandleDmeJiggleBone( const CDmeDag *pDmeDag )
{
	const CDmeJiggleBone *pDmeJiggleBone = CastElementConst< CDmeJiggleBone >( pDmeDag );
	if ( !pDmeJiggleBone )
		return;

	const char *pName = pDmeJiggleBone->GetName();
	for ( int i = 0; i < g_numjigglebones; ++i )
	{
		if ( !Q_stricmp( pName, g_jigglebones[i].bonename ) )
		{
			MdlWarning( "2000: Jiggle Bone: %s already defined, ignoring additional declarations\n", pName );
			return;
		}
	}

	struct s_jigglebone_t *pJiggleBone = &g_jigglebones[ g_numjigglebones ];
	++g_numjigglebones;

	Q_strncpy( pJiggleBone->bonename, pName, sizeof( pJiggleBone->bonename ) );

	// default values
	memset( &pJiggleBone->data, 0, sizeof( mstudiojigglebone_t ) );
	pJiggleBone->data.length = 10.0f;
	pJiggleBone->data.yawStiffness = 100.0f;
	pJiggleBone->data.pitchStiffness = 100.0f;
	pJiggleBone->data.alongStiffness = 100.0f;
	pJiggleBone->data.baseStiffness = 100.0f;
	pJiggleBone->data.baseMinUp = -100.0f;
	pJiggleBone->data.baseMaxUp = 100.0f;
	pJiggleBone->data.baseMinLeft = -100.0f;
	pJiggleBone->data.baseMaxLeft = 100.0f;
	pJiggleBone->data.baseMinForward = -100.0f;
	pJiggleBone->data.baseMaxForward = 100.0f;

	// Common Parameters
	pJiggleBone->data.length = pDmeJiggleBone->m_flLength;
	pJiggleBone->data.tipMass = pDmeJiggleBone->m_flTipMass;
	pJiggleBone->data.flags |= ( pDmeJiggleBone->m_bLengthConstrained ? JIGGLE_HAS_LENGTH_CONSTRAINT : 0 );
	pJiggleBone->data.angleLimit = DEG2RAD( pDmeJiggleBone->m_flAngleLimit );

	pJiggleBone->data.flags |= ( pDmeJiggleBone->m_bYawConstrained ? JIGGLE_HAS_YAW_CONSTRAINT : 0 );
	pJiggleBone->data.minYaw = DEG2RAD( pDmeJiggleBone->m_flYawMin );
	pJiggleBone->data.maxYaw = DEG2RAD( pDmeJiggleBone->m_flYawMax );
	pJiggleBone->data.yawFriction = pDmeJiggleBone->m_flYawFriction;
	pJiggleBone->data.yawBounce = pDmeJiggleBone->m_flYawBounce;

	pJiggleBone->data.flags |= ( pDmeJiggleBone->m_bAngleConstrained ? JIGGLE_HAS_ANGLE_CONSTRAINT : 0 );

	pJiggleBone->data.minPitch = DEG2RAD( pDmeJiggleBone->m_flPitchMin );
	pJiggleBone->data.maxPitch = DEG2RAD( pDmeJiggleBone->m_flPitchMax );
	pJiggleBone->data.pitchFriction = pDmeJiggleBone->m_flPitchFriction;
	pJiggleBone->data.pitchBounce = pDmeJiggleBone->m_flPitchBounce;

	if ( pDmeJiggleBone->m_bFlexible )
	{
		if ( pDmeJiggleBone->m_bRigid )
		{
			MdlWarning( "2001: Jiggle Bone %s: Both flexible and rigid set, ignoring rigid\n", pName );
		}

		pJiggleBone->data.flags |= ( JIGGLE_IS_FLEXIBLE );
		pJiggleBone->data.flags |= ( pDmeJiggleBone->m_bPitchConstrained ? JIGGLE_HAS_PITCH_CONSTRAINT : 0 );

		// flexible parameters - I think damping should be clamped [0, 10] but code
		// in studiomdl looks incorrect and clamps 0, 1000.0f
		pJiggleBone->data.yawStiffness = clamp( pDmeJiggleBone->m_flYawStiffness.Get(), 0.0f, 1000.0f );
		pJiggleBone->data.yawDamping = clamp( pDmeJiggleBone->m_flYawDamping.Get(), 0.0f, 10.0f );

		pJiggleBone->data.pitchStiffness = clamp( pDmeJiggleBone->m_flPitchStiffness.Get(), 0.0f, 1000.0f );
		pJiggleBone->data.pitchDamping = clamp( pDmeJiggleBone->m_flPitchDamping.Get(), 0.0f, 10.0f );

		pJiggleBone->data.alongStiffness = clamp( pDmeJiggleBone->m_flAlongStiffness.Get(), 0.0f, 1000.0f );
		pJiggleBone->data.alongDamping = clamp( pDmeJiggleBone->m_flAlongDamping.Get(), 0.0f, 10.0f );
	}
	else if ( pDmeJiggleBone->m_bRigid )
	{
		pJiggleBone->data.flags |= ( JIGGLE_IS_FLEXIBLE | JIGGLE_HAS_LENGTH_CONSTRAINT );
	}
	else
	{
		// TODO: Is neither rigid or flexible an error?
	}

	if ( pDmeJiggleBone->m_bBaseSpring )
	{
		// flexible parameters - I think damping should be clamped [0, 10] but code
		// in studiomdl looks incorrect and clamps 0, 1000.0f
		pJiggleBone->data.baseMass = pDmeJiggleBone->m_flBaseMass;
		pJiggleBone->data.baseStiffness = clamp( pDmeJiggleBone->m_flBaseStiffness.Get(), 0.0f, 1000.0f );
		pJiggleBone->data.baseDamping = clamp( pDmeJiggleBone->m_flBaseStiffness.Get(), 0.0f, 10.0f );

		pJiggleBone->data.baseMinLeft = pDmeJiggleBone->m_flBaseYawMin;
		pJiggleBone->data.baseMaxLeft = pDmeJiggleBone->m_flBaseYawMax;
		pJiggleBone->data.baseLeftFriction = pDmeJiggleBone->m_flBaseYawFriction;

		pJiggleBone->data.baseMinUp = pDmeJiggleBone->m_flBasePitchMin;
		pJiggleBone->data.baseMaxUp = pDmeJiggleBone->m_flBasePitchMax;
		pJiggleBone->data.baseUpFriction = pDmeJiggleBone->m_flBasePitchFriction;

		pJiggleBone->data.baseMinForward = pDmeJiggleBone->m_flBaseAlongMin;
		pJiggleBone->data.baseMaxForward = pDmeJiggleBone->m_flBaseAlongMax;
		pJiggleBone->data.baseForwardFriction = pDmeJiggleBone->m_flBaseAlongFriction;
	}
}


//-----------------------------------------------------------------------------
// Loads the skeletal hierarchy from the game model, returns bone count
//-----------------------------------------------------------------------------
static bool AddDagJoint( CDmeModel *pModel, CDmeDag *pDag, s_node_t *pNodes, int nParentIndex, BoneTransformMap_t &boneMap )
{
	CDmeTransform *pDmeTransform = pDag->GetTransform();

	if ( !pDmeTransform )
		return true;

	// Need room for one implicit bone added
	if ( boneMap.m_nBoneCount >= ( MAXSTUDIOSRCBONES - 1 ) )
	{
		MdlWarning( "Ignoring Bone %s and children, too many bones [max can be %d]!\n", pDag->GetName(), MAXSTUDIOSRCBONES - 1 );
		return false;
	}

	const int nJointIndex = boneMap.m_nBoneCount++;

	boneMap.m_ppTransforms[ nJointIndex ] = pDmeTransform;

	if ( pModel )
	{
		const int nFoundIndex = pModel->GetJointIndex( pDag );
		if ( nFoundIndex >= 0 )
		{
			boneMap.m_pnDmeModelToMdl[ nFoundIndex ] = nJointIndex;
			boneMap.m_pnMdlToDmeModel[ nJointIndex ] = nFoundIndex;
		}
		else
		{
			MdlWarning( "Joint %s doesn't appear in DmeModel[%s].jointList\n", pDag->GetName(), pModel->GetName() );
		}
	}

	HandleDmeJiggleBone( pDag );

	Q_strncpy( pNodes[ nJointIndex ].name, pDag->GetName(), sizeof( pNodes[ nJointIndex ].name ) );
	pNodes[ nJointIndex ].parent = nParentIndex;

	// Now deal with children
	for ( int i = 0; i < pDag->GetChildCount(); ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !pChild )
			continue;

		if ( !AddDagJoint( pModel, pChild, pNodes, nJointIndex, boneMap ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for loading the skeleton
//-----------------------------------------------------------------------------
static int LoadSkeleton( CDmeDag *pRoot, CDmeModel *pModel, s_node_t *pNodes, BoneTransformMap_t &boneMap )
{
	// Initialize bone indices
	boneMap.m_nBoneCount = 0;
	for ( int i = 0; i < MAXSTUDIOSRCBONES; ++i )
	{
		pNodes[i].name[0] = 0;
		pNodes[i].parent = -1;
		boneMap.m_pnDmeModelToMdl[i] = -1;
		boneMap.m_pnMdlToDmeModel[i] = -1;
		boneMap.m_ppTransforms[i] = NULL;
	}

	// Don't create joints for the the root dag ever.. just deal with the children
	for ( int i = 0; i < pRoot->GetChildCount(); ++i )
	{
		CDmeDag *pChild = pRoot->GetChild( i );
		if ( !pChild )
			continue;

		if ( !AddDagJoint( pModel, pChild, pNodes, -1, boneMap ) )
			return 0;
	}

	// Add a default identity bone used for autoskinning if no joints are specified
	s_nDefaultRootNode = boneMap.m_nBoneCount;
	Q_strncpy( pNodes[s_nDefaultRootNode].name, "defaultRoot", sizeof( pNodes[ s_nDefaultRootNode ].name ) );
	pNodes[s_nDefaultRootNode].parent = -1;

	// +1 for the default identity bone just added
	return boneMap.m_nBoneCount + 1;
}


//-----------------------------------------------------------------------------
// Loads the attachments found in the file
//-----------------------------------------------------------------------------
static void LoadAttachments( CDmeDag *pRoot, CDmeDag *pDag, s_source_t *pSource, bool bStaticProp )
{
	CDmeAttachment *pAttachment = CastElement< CDmeAttachment >( pDag->GetShape() );
	if ( pAttachment && ( pDag != pRoot ) )
	{
		int i = pSource->m_Attachments.AddToTail();
		s_attachment_t &attachment = pSource->m_Attachments[i];
		memset( &attachment, 0, sizeof(s_attachment_t) );
		Q_strncpy( attachment.name, pAttachment->GetName(), sizeof( attachment.name ) );
		Q_strncpy( attachment.bonename, pDag->GetName(), sizeof( attachment.bonename ) );
		SetIdentityMatrix( attachment.local );

		if ( bStaticProp )
		{
			// Static prop will remove all bones so put the attachment transform
			// on the attachment rather than the bone. Also ignore all attachment
			// flags
			pDag->GetAbsTransform( attachment.local );
		}
		else
		{
			if ( pAttachment->m_bIsRigid )
			{
				attachment.type |= IS_RIGID;
			}
			if ( pAttachment->m_bIsWorldAligned )
			{
				attachment.flags |= ATTACHMENT_FLAG_WORLD_ALIGN;
			}
		}
	}

	// Don't create joints for the the root dag ever.. just deal with the children
	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !pChild )
			continue;

		LoadAttachments( pRoot, pChild, pSource, bStaticProp );
	}
}


//-----------------------------------------------------------------------------
// Loads the bind pose
//-----------------------------------------------------------------------------
static void LoadBindPose( CDmeModel *pModel, float flScale, const BoneTransformMap_t &boneMap, s_source_t *pSource )
{
	s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, "BindPose" );
	pSourceAnim->startframe = 0;
	pSourceAnim->endframe = 0;
	pSourceAnim->numframes = 1;

	// Default all transforms to identity
	pSourceAnim->rawanim[0] = (s_bone_t *)calloc( pSource->numbones, sizeof(s_bone_t) );
	for ( int i = 0; i < pSource->numbones; ++i )
	{
		pSourceAnim->rawanim[0][i].pos.Init();
		pSourceAnim->rawanim[0][i].rot.Init();
	}

	{
		matrix3x4_t jointTransform;

		CDmeTransformList *pBindPose = pModel->FindBaseState( "bind" );
		for ( int nMdlBoneIndex = 0; nMdlBoneIndex < boneMap.m_nBoneCount; ++nMdlBoneIndex )
		{
			CDmeTransform *pDmeTransform = NULL;

			const int nDmeModelBoneIndex = boneMap.m_pnMdlToDmeModel[ nMdlBoneIndex ];
			if ( nDmeModelBoneIndex < 0 )
			{
				// No bind pose stored for the specified joint, use the current
				// position of the joint from the skeleton instead
				pDmeTransform = boneMap.m_ppTransforms[ nMdlBoneIndex ];
			}
			else
			{
				pDmeTransform = pBindPose ? pBindPose->GetTransform( nDmeModelBoneIndex ) : pModel->GetJointTransform( nDmeModelBoneIndex );
			}

			if ( pDmeTransform )
			{
				pDmeTransform->GetTransform( jointTransform );

				s_bone_t &mdlBone = pSourceAnim->rawanim[0][ nMdlBoneIndex ];
				MatrixAngles( jointTransform, mdlBone.rot, mdlBone.pos );
				mdlBone.pos *= flScale;
			}
			else
			{
				MdlWarning( "Cannot find DmeTransform for MDL Bone %d\n", nMdlBoneIndex );
			}
		}
	}

	Build_Reference( pSource, "BindPose" );
}


//-----------------------------------------------------------------------------
// Does a search through connection operators for dependent DmeOperators
//-----------------------------------------------------------------------------
static void GetDependentOperators( CUtlVector< IDmeOperator * > &operatorList, CDmeOperator *pDmeOperator )
{
	if ( !pDmeOperator || !CastElement< CDmeOperator >( pDmeOperator ) )
		return;

	for ( int i = 0; i < operatorList.Count(); ++i )
	{
		CDmeOperator *pTmpDmeOperator = CastElement< CDmeOperator >( reinterpret_cast< CDmeOperator * >( operatorList[i] ) );
		if ( pTmpDmeOperator && pTmpDmeOperator == pDmeOperator )
			return;
	}

	operatorList.AddToTail( pDmeOperator );

	CUtlVector< CDmAttribute * > outAttrList;
	pDmeOperator->GetOutputAttributes( outAttrList );
	for ( int i = 0; i < outAttrList.Count(); ++i )
	{
		CDmElement *pDmElement = outAttrList[i]->GetOwner();
		if ( !pDmElement )
			continue;

		if ( pDmElement == pDmeOperator )
		{
			CUtlVector< CDmElement * > reList0;
			FindReferringElements( reList0, pDmElement, g_pDataModel->GetSymbol( "element" ) );
			for ( int j = 0; j < reList0.Count(); ++j )
			{
				CDmeAttributeReference *pRe0 = CastElement< CDmeAttributeReference >( reList0[j] );
				if ( !pRe0 )
					continue;

				CUtlVector< CDmElement * > reList1;
				FindReferringElements( reList1, pRe0, g_pDataModel->GetSymbol( "input" ) );
				for ( int k = 0; k < reList1.Count(); ++k )
				{
					CDmeConnectionOperator *pRe1 = CastElement< CDmeConnectionOperator >( reList1[k] );
					if ( !pRe1 )
						continue;

					GetDependentOperators( operatorList, pRe1 );
				}
			}
		}
		else
		{
			GetDependentOperators( operatorList, CastElement< CDmeOperator >( pDmElement ) );
		}
	}
}


//-----------------------------------------------------------------------------
// Main entry point for loading DMX files
//-----------------------------------------------------------------------------
static void PrepareChannels(
	CUtlVector< IDmeOperator * > &operatorList,
	CDmeChannelsClip *pAnimation )
{
	int nChannelsCount = pAnimation->m_Channels.Count();
	for ( int i = 0; i < nChannelsCount; ++i )
	{
		pAnimation->m_Channels[i]->SetMode( CM_PLAY );
		GetDependentOperators( operatorList, pAnimation->m_Channels[i] );
	}
}


//-----------------------------------------------------------------------------
// Update channels so they are in position for the next frame
//-----------------------------------------------------------------------------
static void UpdateChannels( CUtlVector< IDmeOperator * > &operators, CDmeChannelsClip *pAnimation, DmeTime_t clipTime )
{
	int nChannelsCount = pAnimation->m_Channels.Count();
	DmeTime_t channelTime = pAnimation->ToChildMediaTime( clipTime );
	for ( int i = 0; i < nChannelsCount; ++i )
	{
		pAnimation->m_Channels[i]->SetCurrentTime( channelTime );
	}

	// Recompute the position of the joints
	{
		CDisableUndoScopeGuard guard;
		g_pDmElementFramework->SetOperators( operators );
		g_pDmElementFramework->Operate( true );
	}
	g_pDmElementFramework->BeginEdit();
}


//-----------------------------------------------------------------------------
// Initialize the pose for this frame
//-----------------------------------------------------------------------------
static void ComputeFramePose( s_sourceanim_t *pSourceAnim, int nFrame, float flScale, BoneTransformMap_t& boneMap )
{
	pSourceAnim->rawanim[nFrame] = (s_bone_t *)calloc( boneMap.m_nBoneCount, sizeof( s_bone_t ) );

	for ( int i = 0; i < boneMap.m_nBoneCount; ++i )
	{
		matrix3x4_t jointTransform;
		boneMap.m_ppTransforms[i]->GetTransform( jointTransform );
		MatrixAngles( jointTransform, pSourceAnim->rawanim[nFrame][i].rot, pSourceAnim->rawanim[nFrame][i].pos );
		pSourceAnim->rawanim[nFrame][i].pos *= flScale;
	}
}


//-----------------------------------------------------------------------------
// Main entry point for loading animations
//-----------------------------------------------------------------------------
static void LoadAnimations( s_source_t *pSource, CDmeAnimationList *pAnimationList, float flScale, BoneTransformMap_t &boneMap )
{
	int nAnimationCount = pAnimationList->GetAnimationCount();
	for ( int i = 0; i < nAnimationCount; ++i )
	{
		CDmeChannelsClip *pAnimation = pAnimationList->GetAnimation( i );

		if ( !Q_stricmp( pAnimationList->GetName(), "BindPose" ) )
		{
			MdlError( "Error: Cannot use \"BindPose\" as an animation name!\n" );
			break;
		}

		s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, pAnimation->GetName() );
		DmeTime_t nStartTime = pAnimation->GetStartTime();
		DmeTime_t nEndTime = pAnimation->GetEndTime();
		int nFrameRateVal = pAnimation->GetValue<int>( "frameRate" );
		if ( nFrameRateVal <= 0 )
		{
			nFrameRateVal = 30;
		}
		DmeFramerate_t nFrameRate = nFrameRateVal;
		pSourceAnim->startframe = nStartTime.CurrentFrame( nFrameRate );
		pSourceAnim->endframe = nEndTime.CurrentFrame( nFrameRate );
		pSourceAnim->numframes = pSourceAnim->endframe - pSourceAnim->startframe + 1;
		CUtlVector< IDmeOperator * > operatorList;
		PrepareChannels( operatorList, pAnimation );
		float flOOFrameRate = 1.0f / (float)nFrameRateVal;
		int nFrame = 0;
		while ( nFrame < pSourceAnim->numframes )
		{
			int nSecond = nFrame / nFrameRateVal;
			int nFraction = nFrame - nSecond * nFrameRateVal;
			DmeTime_t t = nStartTime + DmeTime_t( nSecond * 10000 ) + DmeTime_t( (float)nFraction * flOOFrameRate );
			UpdateChannels( operatorList, pAnimation, t );
			ComputeFramePose( pSourceAnim, nFrame, flScale, boneMap );
			++nFrame;
		}
	}
}


//-----------------------------------------------------------------------------
// Loads the skeletal hierarchy from the game model, returns bone count
//-----------------------------------------------------------------------------
static void AddFlexKeys( CDmeDag *pRoot, CDmeDag *pDag, CDmeCombinationOperator *pComboOp, s_source_t *pSource )
{
	CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
	if ( pMesh && ( pDag != pRoot ) )
	{
		int nDeltaStateCount = pMesh->DeltaStateCount();
		for ( int i = 0; i < nDeltaStateCount; ++i )
		{
			CDmeVertexDeltaData *pDeltaState = pMesh->GetDeltaState( i );
			AddFlexKey( pSource, pComboOp, pDeltaState->GetName() );
		}
	}

	// Don't create joints for the the root dag ever.. just deal with the children
	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !pChild )
			continue;

		AddFlexKeys( pRoot, pChild, pComboOp, pSource );
	}
}

//-----------------------------------------------------------------------------
// Loads all auxilliary model info:
//
//	*	Determine original source files used to generate
//		the current DMX file and schedule them for processing.
//-----------------------------------------------------------------------------
void LoadModelInfo( CDmElement *pRoot, char const *pFullPath )
{
	// Determine original source files and schedule them for processing
	if ( CDmElement *pMakeFile = pRoot->GetValueElement< CDmElement >( "makefile" ) )
	{
		if ( CDmAttribute *pSources = pMakeFile->GetAttribute( "sources" ) )
		{
			CDmrElementArray< CDmElement > arrSources( pSources );
			for ( int kk = 0; kk < arrSources.Count(); ++ kk )
			{
				if ( CDmElement *pModelSource = arrSources.Element( kk ) )
				{
					if ( char const *szName = pModelSource->GetName() )
					{
						ProcessOriginalContentFile( pFullPath, szName );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool LoadTwistConstraint( CDmElement *pDmElement )
{
	CDmeRigTwistConstraintOperator *pDmeTwist = CastElement< CDmeRigTwistConstraintOperator >( pDmElement );
	if ( !pDmeTwist )
		return false;

	if ( g_twistbones.Count() == MAXSTUDIOBONES )
		return false;

	CDmeDag *pDmeDagParent = pDmeTwist->GetParentTarget();
	CDmeDag *pDmeDagChild = pDmeTwist->GetChildTarget();

	if ( !pDmeDagParent || !pDmeDagChild )
		return false;

	CTwistBone &twistBone = g_twistbones[ g_twistbones.AddToTail() ];

	twistBone.m_bInverse = pDmeTwist->GetInverse();
	twistBone.m_vUpVector = pDmeTwist->GetUpAxis();
	V_strncpy( twistBone.m_szParentBoneName, pDmeDagParent->GetName(), ARRAYSIZE( twistBone.m_szParentBoneName ) );
	twistBone.m_qBaseRotation = twistBone.m_bInverse ? pDmeTwist->GetParentBindRotation() : pDmeTwist->GetChildBindRotation();
	V_strncpy( twistBone.m_szChildBoneName, pDmeDagChild->GetName(), ARRAYSIZE( twistBone.m_szChildBoneName ) );

	for ( int i = 0; i < pDmeTwist->SlaveCount(); ++i )
	{
		CDmeDag *pDmeTwistDag = pDmeTwist->GetSlaveDag( i );
		if ( !pDmeTwistDag )
			continue;

		const char *pszTwistTargetName = pDmeTwistDag->GetName();
		if ( !pszTwistTargetName || *pszTwistTargetName == '\0' )
			continue;

		bool bFound = false;

		// Check to see if this target is already the target of a twist bone
		for ( int j = 0; !bFound && j < g_twistbones.Count(); ++j )
		{
			const CTwistBone &tmpTwistBone = g_twistbones[j];
			for ( int k = 0; k < tmpTwistBone.m_twistBoneTargets.Count(); ++k )
			{
				if ( !Q_stricmp( pszTwistTargetName, tmpTwistBone.m_twistBoneTargets[k].m_szBoneName ) )
				{
					bFound = true;
					break;
				}
			}
		}

		// A Twist bone is already driving this bone, don't make another
		if ( bFound )
			continue;

		s_constraintbonetarget_t &twistBoneTarget = twistBone.m_twistBoneTargets[ twistBone.m_twistBoneTargets.AddToTail() ];
		V_strncpy( twistBoneTarget.m_szBoneName, pDmeTwistDag->GetName(), ARRAYSIZE( twistBoneTarget.m_szBoneName ) );
		twistBoneTarget.m_nBone = -1;
		twistBoneTarget.m_flWeight = pDmeTwist->GetSlaveWeight( i );
		twistBoneTarget.m_vOffset = pDmeTwistDag->GetTransform()->GetPosition();
		twistBoneTarget.m_qOffset = pDmeTwist->GetSlaveBindOrientation( i );
	}

	// No targets, twist constraint is useless
	if ( twistBone.m_twistBoneTargets.Count() <= 0 )
	{
		g_twistbones.RemoveMultipleFromTail( 1 );
		return true;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool LoadBaseConstraintParams( CConstraintBoneBase *pConstraintBone, CDmeRigBaseConstraintOperator *pDmeBaseConstraint )
{
	const CDmaElementArray< CDmeConstraintTarget > &constraintTargets = pDmeBaseConstraint->GetTargets();

	if ( constraintTargets.Count() <= 0 )
		return false;

	const Quaternion qRot = Quaternion( g_defaultrotation );

	for ( int i = 0; i < constraintTargets.Count(); ++i )
	{
		CDmeConstraintTarget *pDmeConstraintTarget = constraintTargets[i];
		if ( !pDmeConstraintTarget )
			continue;

		CDmeDag *pDmeTargetDag = pDmeConstraintTarget->GetDag();
		if ( !pDmeTargetDag )
			continue;

		// Load targets
		s_constraintbonetarget_t &target = pConstraintBone->m_targets[ pConstraintBone->m_targets.AddToTail() ];
		V_strncpy( target.m_szBoneName, pDmeTargetDag->GetName(), ARRAYSIZE( target.m_szBoneName ) );
		target.m_nBone = -1;
		target.m_flWeight = pDmeConstraintTarget->GetWeight();
		target.m_qOffset = pDmeConstraintTarget->GetOrientationOffset();

		if ( pDmeBaseConstraint->IsA( CDmeRigPointConstraintOperator::GetStaticTypeSymbol() ) )
		{
			// Target offsets are in world space for Point constraint
			VectorRotate( pDmeConstraintTarget->GetPositionOfffset(), qRot, target.m_vOffset );
		}
		else
		{
			target.m_vOffset = pDmeConstraintTarget->GetPositionOfffset();
		}

//		QuaternionMult( pDmeConstraintTarget->GetOrientationOffset(), qRot, target.m_qOffset );
	}

	if ( pConstraintBone->m_targets.Count() <= 0 )
		return false;

	const CDmeConstraintSlave *pDmeConstraintSlave = pDmeBaseConstraint->GetConstraintSlave();
	if ( !pDmeConstraintSlave )
		return false;

	const CDmeDag *pDmeSlaveDag = pDmeBaseConstraint->GetSlave();
	if ( !pDmeSlaveDag )
		return false;

	s_constraintboneslave_t &slave = pConstraintBone->m_slave;

	V_strncpy( slave.m_szBoneName, pDmeSlaveDag->GetName(), ARRAYSIZE( slave.m_szBoneName ) );
	slave.m_nBone = -1;
	slave.m_vBaseTranslate = pDmeConstraintSlave->GetBasePosition();
	slave.m_qBaseRotation = pDmeConstraintSlave->GetBaseOrientation();

	return true;
}


//-----------------------------------------------------------------------------
// Most constraints don't have any specialized constraint parameters
//-----------------------------------------------------------------------------
template < class S, class T >
static bool LoadSpecializedConstraintParams( S *pDmeConstraint, T *pConstraint )
{
	return true;
}


//-----------------------------------------------------------------------------
// Load specialized Aim Constraint parameters
//-----------------------------------------------------------------------------
template <>
static bool LoadSpecializedConstraintParams( CDmeRigAimConstraintOperator *pDmeConstraint, CAimConstraint *pConstraint )
{
	const Quaternion qRot = Quaternion( g_defaultrotation );

	// Set Aim Constraint Parameters
	pConstraint->m_qAimOffset = pDmeConstraint->GetValue< Quaternion >( "aimOffset" );

	// Up vectors are specified in world space
	VectorRotate( pDmeConstraint->GetValue< Vector >( "upVector" ), qRot, pConstraint->m_vUpVector );

	CDmElement *pDmeUpSpaceTarget = pDmeConstraint->GetValueElement< CDmElement >( "upSpaceTarget" );
	if ( pDmeUpSpaceTarget )
	{
		V_strncpy( pConstraint->m_szUpSpaceTargetBone, pDmeUpSpaceTarget->GetName(), ARRAYSIZE( pConstraint->m_szUpSpaceTargetBone ) );
	}
	pConstraint->m_nUpSpaceTargetBone = -1;

	pConstraint->m_nUpType = pDmeConstraint->GetValue< int > ( "upType" );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool FindDuplicateConstraint( const CConstraintBoneBase *pConstraintA )
{
	for ( int i = 0; i < g_constraintBones.Count(); ++i )
	{
		const CConstraintBoneBase *pConstraintB = g_constraintBones[i];

		if ( *pConstraintA == *pConstraintB )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class S, class T >
static bool LoadConstraint( CDmElement *pDmElement )
{
	if ( g_constraintBones.Count() == MAXSTUDIOBONES )
	{
		MdlError( "Too Many Constraint Bones, Max: %d\n", MAXSTUDIOBONES );
		return false;
	}

	S *pDmeConstraint = CastElement< S >( pDmElement );
	if ( !pDmeConstraint )
		return false;

	T *pConstraint = new T();
	if ( !pConstraint )
		return false;

	if ( !LoadBaseConstraintParams( pConstraint, pDmeConstraint ) )
	{
		delete pConstraint;
		return false;
	}

	if ( !LoadSpecializedConstraintParams( pDmeConstraint, pConstraint ) )
	{
		delete pConstraint;
		return false;
	}

	if ( FindDuplicateConstraint( pConstraint ) )
	{
		// Delete this
		delete pConstraint;
	}
	else
	{
		g_constraintBones.AddToTail( pConstraint );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void LoadConstraints( CDmElement *pDmeRoot )
{
	if ( !pDmeRoot )
		return;

	CDmAttribute *pDmeConstraintsAttr = pDmeRoot->GetAttribute( "constraints", AT_ELEMENT_ARRAY );
	if ( !pDmeConstraintsAttr )
		return;

	CDmrElementArray< CDmElement > constraints( pDmeConstraintsAttr );
	for ( int i = 0; i < constraints.Count(); ++i )
	{
		if ( LoadTwistConstraint( constraints[i] ) )
			continue;

		if ( LoadConstraint< CDmeRigPointConstraintOperator, CPointConstraint >( constraints[i] ) )
			continue;

		if ( LoadConstraint< CDmeRigOrientConstraintOperator, COrientConstraint >( constraints[i] ) )
			continue;

		if ( LoadConstraint< CDmeRigAimConstraintOperator, CAimConstraint >( constraints[i] ) )
			continue;

		if ( LoadConstraint< CDmeRigParentConstraintOperator, CParentConstraint >( constraints[i] ) )
			continue;

		Error( "TODO: Support Constraint: %s\n", constraints[i]->GetName() );
	}
}


//-----------------------------------------------------------------------------
// Load model and skeleton
//-----------------------------------------------------------------------------
static bool LoadModelAndSkeleton( s_source_t *pSource, BoneTransformMap_t &boneMap, CDmeDag *pSkeleton, CDmeModel *pModel, CDmeCombinationOperator *pCombinationOperator, bool bStaticProp )
{
	s_DeltaStates.RemoveAll();
	s_Balance.RemoveAll();
	s_Speed.RemoveAll();
	s_UniqueVertices.RemoveAll();
	s_UniqueVerticesMap.RemoveAll();

	if ( !pSkeleton )
		return false;

	// BoneRemap[bone index in file] == bone index in studiomdl
	pSource->numbones = LoadSkeleton( pSkeleton, pModel, pSource->localBone, boneMap );
	if ( pSource->numbones == 0 )
		return false;

	g_numfaces = 0;
	if ( pModel )
	{
		if ( pCombinationOperator )
		{
			pCombinationOperator->GenerateWrinkleDeltas( false );
		}
		LoadBindPose( pModel, g_currentscale, boneMap, pSource );
		if ( !LoadMeshes( pModel, g_currentscale, boneMap.m_pnDmeModelToMdl, pSource ) )
			return false;

		UnifyIndices( pSource );
		BuildVertexAnimations( pSource );
		BuildIndividualMeshes( pSource );
	}

	if ( g_numfaces == 0 && pSource->numbones == 1 && !V_strcmp( pSource->localBone[0].name, "defaultRoot" ) )
	{
		MdlError( "Error - dmx has no contents: %s\n", pSource->filename );
	}

	if ( pCombinationOperator )
	{
		AddFlexKeys( pModel, pModel, pCombinationOperator, pSource );
		AddCombination( pSource, pCombinationOperator );
	}

	LoadAttachments( pSkeleton, pSkeleton, pSource, bStaticProp );

	return true;
}


//-----------------------------------------------------------------------------
// Given the s_model_t pointer, finds the index of it in g_model
// Returns -1 if it cannot be found
//-----------------------------------------------------------------------------
static int FindModelIndex( s_model_t *pModel )
{
	if ( pModel )
	{
		for ( int i = 0; i < g_nummodels; ++i )
		{
			if ( g_model[ i ] == pModel )
				return i;
		}

		MdlWarning( "Cannot Find s_model_t: \"%s\" in g_model\n", pModel->name );
	}

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool AddFlexKey(
	s_source_t *pSource, s_model_t *pModel,
	const char *pszAnimationName,
	int nFlexDesc,
	int nFlexPair,
	EyelidType_t nEyelidType,
	float flLowererHeight,
	float flNeutralHeight,
	float flRaiserHeight,
	float flSplit = 0.0f,
	float flDecay = 1.0f,
	int nFrame = 0 )			// DMX frame is always 0
{
	if ( g_numflexkeys >= ARRAYSIZE( g_flexkey ) )
	{
		MdlError( "Too Many Flex Keys, Cannot Add %s\n", "TODO: FlexKeyName" );
		return false;
	}

	const int nModelIndex = FindModelIndex( pModel );

	s_flexkey_t *pFlexKey = &g_flexkey[ g_numflexkeys ];
	pFlexKey->source = pSource;
	V_strncpy( pFlexKey->animationname, pszAnimationName, sizeof( pFlexKey->animationname ) );
	pFlexKey->imodel = nModelIndex;
	pFlexKey->flexdesc = nFlexDesc;
	pFlexKey->flexpair = nFlexPair;

	pFlexKey->split = flSplit;
	pFlexKey->decay = flDecay;
	pFlexKey->frame = nFrame;

	switch ( nEyelidType )
	{
	case kLowerer:
		pFlexKey->target0 = -11;
		pFlexKey->target1 = -10;
		pFlexKey->target2 = flLowererHeight;
		pFlexKey->target3 = flNeutralHeight;
		break;
	case kNeutral:
		pFlexKey->target0 = flLowererHeight;
		pFlexKey->target1 = flNeutralHeight;
		pFlexKey->target2 = flNeutralHeight;
		pFlexKey->target3 = flRaiserHeight;
		break;
	case kRaiser:
		pFlexKey->target0 = flNeutralHeight;
		pFlexKey->target1 = flRaiserHeight;
		pFlexKey->target2 = 10;
		pFlexKey->target3 = 11;
		break;
	}

	// Check for a duplicate
	for ( int i = 0; i < g_numflexkeys; ++i )
	{
		const s_flexkey_t *pTmpFlexKey = &( g_flexkey[ i ] );

		if ( !V_stricmp( pFlexKey->animationname, pTmpFlexKey->animationname ) &&
			pFlexKey->flexdesc == pTmpFlexKey->flexdesc &&
			pFlexKey->flexpair == pTmpFlexKey->flexpair &&
			pFlexKey->frame == pTmpFlexKey->frame &&
			pFlexKey->target0 == pTmpFlexKey->target0 &&
			pFlexKey->target1 == pTmpFlexKey->target1 &&
			pFlexKey->target2 == pTmpFlexKey->target2 &&
			pFlexKey->target3 == pTmpFlexKey->target3 )
		{
			// Duplicate, get rid of it
			V_memset( pFlexKey, 0, sizeof( s_flexkey_t ) );

			return false;
		}
	}

	// Not a duplicate, keep it
	++g_numflexkeys;

	return true;
}


//-----------------------------------------------------------------------------
// In studiomdl.cpp
//-----------------------------------------------------------------------------
const s_sourceanim_t *GetNewStyleSourceVertexAnim( s_source_t *pSource, const char *pszVertexAnimName );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool LoadEyelid( s_model_t *pModel, CDmeEyelid *pDmeEyelid )
{
	if ( !pModel || !pDmeEyelid )
		return false;

	s_source_t *pSource = pModel->source;
	if ( !pSource )
		return false;

	const bool bUpper = pDmeEyelid->m_bUpper.Get();

	enum RightLeftType_t
	{
		kLeft = 0,
		kRight = 1,
		kRightLeftTypeCount = 2
	};

	struct EyelidData_t
	{
		int m_nFlexDesc[ kRightLeftTypeCount ];
		const s_sourceanim_t *m_pSourceAnim;
		float m_flTarget;
		const char *m_pszSuffix;
	};

	EyelidData_t eyelidData[3] =
	{
		{ { -1, -1 },	NULL, 0.0f, "lowerer" },
		{ { -1, -1 },	NULL, 0.0f, "neutral" },
		{ { -1, -1 },	NULL, 0.0f, "raiser" }
	};

	eyelidData[kLowerer].m_pSourceAnim = GetNewStyleSourceVertexAnim( pSource, pDmeEyelid->m_sLowererFlex.Get() );
	eyelidData[kLowerer].m_flTarget = pDmeEyelid->m_flLowererHeight.Get();
	eyelidData[kNeutral].m_pSourceAnim = GetNewStyleSourceVertexAnim( pSource, pDmeEyelid->m_sNeutralFlex.Get() );
	eyelidData[kNeutral].m_flTarget = pDmeEyelid->m_flNeutralHeight.Get();
	eyelidData[kRaiser].m_pSourceAnim = GetNewStyleSourceVertexAnim( pSource, pDmeEyelid->m_sRaiserFlex.Get() );
	eyelidData[kRaiser].m_flTarget = pDmeEyelid->m_flRaiserHeight.Get();

	// Add a flexdesc for <type>_right & <type>_left
	// Where <type> is "upper" or "lower"
	int nRightLeftBaseDesc[kRightLeftTypeCount] = { -1, -1 };

	const char *sRightBaseDesc = bUpper ? "upper_right" : "lower_right";
	nRightLeftBaseDesc[kRight] = Add_Flexdesc( sRightBaseDesc );

	for ( int i = 0; i < kEyelidTypeCount; ++i )
	{
		CUtlString sRightLocalDesc = sRightBaseDesc;
		sRightLocalDesc += "_";
		sRightLocalDesc += eyelidData[i].m_pszSuffix;
		eyelidData[i].m_nFlexDesc[kRight] = Add_Flexdesc( sRightLocalDesc.Get() );
	}

	const char *sLeftBaseDesc = bUpper ? "upper_left" : "lower_left";
	nRightLeftBaseDesc[kLeft] = Add_Flexdesc( sLeftBaseDesc );

	for ( int i = 0; i < kEyelidTypeCount; ++i )
	{
		CUtlString sLeftLocalDesc = sLeftBaseDesc;
		sLeftLocalDesc += "_";
		sLeftLocalDesc += eyelidData[i].m_pszSuffix;
		eyelidData[i].m_nFlexDesc[kLeft] = Add_Flexdesc( sLeftLocalDesc.Get() );
	}

	for ( int i = 0; i < kEyelidTypeCount; ++i )
	{
		if ( !AddFlexKey( pSource, pModel,
			eyelidData[i].m_pSourceAnim->animationname,
			nRightLeftBaseDesc[kLeft],
			nRightLeftBaseDesc[kRight],
			EyelidType_t( i ),
			eyelidData[kLowerer].m_flTarget,
			eyelidData[kNeutral].m_flTarget,
			eyelidData[kRaiser].m_flTarget ) )
		{
			// Flex Keys Already Added - Eyelid is a duplicate
			return false;
		}
	}

	bool bRightOk = false;
	bool bLeftOk = false;

	for ( int i = 0; i < pModel->numeyeballs; ++i )
	{
		s_eyeball_t *pEyeball = &( pModel->eyeball[i] );
		if ( !pEyeball )
			continue;

		RightLeftType_t nRightLeftIndex = kRight;
		if ( !V_stricmp( pDmeEyelid->m_sRightEyeballName.Get(), pEyeball->name ) )
		{
			nRightLeftIndex = kRight;
			bRightOk = true;
		}
		else if ( !Q_stricmp( pDmeEyelid->m_sLeftEyeballName, pEyeball->name ) )
		{
			nRightLeftIndex = kLeft;
			bLeftOk = true;
		}
		else
		{
			MdlWarning( "Unknown Eyeball: %s\n", pEyeball->name );
			continue;
		}

		for ( int j = 0; j < kEyelidTypeCount; ++j )
		{
			if ( fabs( eyelidData[j].m_flTarget ) > pEyeball->radius )
			{
				MdlError( "Eyelid \"%s\" %s %.1f out of range (+-%.1f)\n", bUpper ? "upper" : "lower", eyelidData[j].m_pszSuffix, eyelidData[j].m_flTarget, pEyeball->radius );
			}
		}

		if ( bUpper )
		{
			pEyeball->upperlidflexdesc	= nRightLeftBaseDesc[nRightLeftIndex];
			for ( int j = 0; j < kEyelidTypeCount; ++j )
			{
				pEyeball->upperflexdesc[j]	= eyelidData[j].m_nFlexDesc[nRightLeftIndex]; 
				pEyeball->uppertarget[j]	= eyelidData[j].m_flTarget;
			}
		}
		else
		{
			pEyeball->lowerlidflexdesc	= nRightLeftBaseDesc[nRightLeftIndex];
			for ( int j = 0; j < kEyelidTypeCount; ++j )
			{
				pEyeball->lowerflexdesc[j]	= eyelidData[j].m_nFlexDesc[nRightLeftIndex]; 
				pEyeball->lowertarget[j]	= eyelidData[j].m_flTarget;
			}
		}
	}

	if ( !bRightOk )
	{
		MdlError( "Could not find right eye \"%s\"\n", pDmeEyelid->m_sRightEyeballName.Get() );
		return false;
	}

	if ( !bLeftOk )
	{
		MdlError( "Could not find left eye \"%s\"\n", pDmeEyelid->m_sLeftEyeballName.Get() );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool LoadMouth( CDmeMouth *pDmeMouth )
{
	if ( !pDmeMouth )
		return false;

	const int nMouthIndex = pDmeMouth->m_nMouthNumber.Get();

	// Check if mouth is already defined... not sure why people need to specify a mouth number
	// in the QC though.

	if ( g_nummouths > nMouthIndex )
		return false;

	g_nummouths = nMouthIndex + 1;

	s_mouth_t *pMouth = &( g_mouth[ nMouthIndex ] );
	pMouth->flexdesc = Add_Flexdesc( pDmeMouth->m_sFlexControllerName.Get() );
	V_strncpy( pMouth->bonename, pDmeMouth->m_sBoneName, sizeof( pMouth->bonename ) );
	pMouth->forward = pDmeMouth->m_vForward.Get();	// TODO: Adjust Y Up?

	return false;
}

//-----------------------------------------------------------------------------
// Loads Eyeballs
//-----------------------------------------------------------------------------
static void LoadEyeballs( s_source_t *pSource, s_model_t *pModel, CDmrElementArray< CDmElement > &elementArray )
{
	if ( !pSource || !pModel || elementArray.Count() <= 0 )
		return;

	Assert( pModel->source == NULL || pModel->source == pSource );

	matrix3x4_t mDefRot;
	AngleMatrix( g_defaultrotation, mDefRot );
	Vector vTmp;

	for ( int i = 0; i < elementArray.Count(); ++i )
	{
		CDmeEyeball *pDmeEyeball = CastElement< CDmeEyeball >( elementArray.Element( i ) );
		if ( !pDmeEyeball )
			continue;

		if ( pModel->numeyeballs >= ARRAYSIZE( pModel->eyeball ) )
		{
			MdlWarning( "1100: Max number of eyeballs reached for model %s, ignoring eyeball %s\n", pModel->name, pDmeEyeball->GetName() );
			continue;
		}

		int nFoundBoneIndex = -1;
		for ( int nSearchBoneIndex = 0; nSearchBoneIndex < pSource->numbones; ++nSearchBoneIndex )
		{
			if ( !Q_stricmp( pSource->localBone[ nSearchBoneIndex ].name, pDmeEyeball->m_sParentBoneName.Get() ) )
			{
				nFoundBoneIndex = nSearchBoneIndex;
				break;
			}
		}

		if ( nFoundBoneIndex < 0 )
		{
			MdlWarning( "1101: Couldn't find bone %s on model %s, ignoring eyeball %s\n", pDmeEyeball->m_sParentBoneName.Get(), pModel->name, pDmeEyeball->GetName() );
			continue;
		}

		const char *pszMaterialName = pDmeEyeball->m_sMaterialName.Get();
		const bool bRelative = ( strchr( pszMaterialName, '/' ) || strchr( pszMaterialName, '\\' ) ) ? true : false;

		const int nSearchMeshMatIndex = UseTextureAsMaterial( LookupTexture( pDmeEyeball->m_sMaterialName.Get(), bRelative ) );
		int nFoundMeshMatIndex = -1;
		for ( int i = 0; i < pSource->nummeshes; ++i )
		{
			const int nTmpMeshMatIndex = pSource->meshindex[ i ]; // meshes are internally stored by material index

			if ( nTmpMeshMatIndex == nSearchMeshMatIndex )
			{
				nFoundMeshMatIndex = i;
				break;
			}
		}

		if ( nFoundMeshMatIndex < 0 )
		{
			MdlWarning( "1102: Couldn't find eyeball material %s on model %s, ignoring eyeball %s\n", pDmeEyeball->m_sMaterialName.Get(), pModel->name, pDmeEyeball->GetName() );
			continue;
		}

		s_eyeball_t *ps_eyeball_t = &( pModel->eyeball[ pModel->numeyeballs ] );
		Q_strncpy( ps_eyeball_t->name, pDmeEyeball->GetName(), sizeof( ps_eyeball_t->name ) );
		ps_eyeball_t->bone = nFoundBoneIndex;
		ps_eyeball_t->mesh = nFoundMeshMatIndex;
		ps_eyeball_t->radius = pDmeEyeball->m_flRadius.Get();
		ps_eyeball_t->zoffset = tan( DEG2RAD( pDmeEyeball->m_flYawAngle.Get() ) );
		ps_eyeball_t->iris_scale = 1.0f / pDmeEyeball->m_flIrisScale;

		// translate eyeball into bone space
		VectorITransform( pDmeEyeball->m_vPosition.Get(), pSource->boneToPose[ ps_eyeball_t->bone ], ps_eyeball_t->org );

		VectorIRotate( Vector( 0, 0, 1 ), mDefRot, vTmp );
		VectorIRotate( vTmp, pSource->boneToPose[ ps_eyeball_t->bone ], ps_eyeball_t->up );

		VectorIRotate( Vector( 1, 0, 0 ), mDefRot, vTmp );
		VectorIRotate( vTmp, pSource->boneToPose[ ps_eyeball_t->bone ], ps_eyeball_t->forward );

		// Not applicable
		ps_eyeball_t->upperlidflexdesc = -1;
		ps_eyeball_t->lowerlidflexdesc = -1;

		bool bOk = true;

		// Check for a duplicate eyeball
		for ( int j = 0; j < pModel->numeyeballs; ++j )
		{
			s_eyeball_t *pTmp = &( pModel->eyeball[ j ] );
			if ( !V_stricmp( ps_eyeball_t->name, pTmp->name ) )
			{
				// TODO: Check and warn about duplicate eyeballs with mis-matched parameters
				bOk = false;
				break;
			}
		}

		if ( bOk )
		{
			// Keep eyeball
			pModel->numeyeballs += 1;
		}
		else
		{
			// Clear data
			V_memset( ps_eyeball_t, 0, sizeof( s_eyeball_t ) );
		}
	}

	// Make the standard flex controllers for eyes if required
	static const char *szEyesFlexControllers[] = {
		"eyes_updown",
		"eyes_rightleft"
	};

	for ( int nNewFlexIndex = 0; nNewFlexIndex < ARRAYSIZE( szEyesFlexControllers ); ++nNewFlexIndex )
	{
		bool bHasEyeFlexController = false;

		for ( int nFlexIndex = 0; nFlexIndex < g_numflexcontrollers; ++nFlexIndex )
		{
			if ( !Q_strcmp( szEyesFlexControllers[ nNewFlexIndex ], g_flexcontroller[ nFlexIndex ].name ) )
			{
				bHasEyeFlexController = true;
				break;
			}
		}

		// The flex controller range for eyes_updown & eyes_rightleft is default [-45, 45] because it's clamped by eyesMaxDeflection
		// and changing it based on max deflection would cause animatin changes since flex controller values are normalized [0, 1]
		// [-45, 45 ] gives a maxium range that's useful

		if ( !bHasEyeFlexController )
		{
			if ( g_numflexcontrollers >= MAXSTUDIOFLEXCTRL )
			{
				MdlWarning( "1103: Couldn't make eyes flexcontroller %s, too many flex controllers defined\n", szEyesFlexControllers[ nNewFlexIndex ] );
				continue;
			}

			Q_strncpy( g_flexcontroller[g_numflexcontrollers].name, szEyesFlexControllers[ nNewFlexIndex ], sizeof( g_flexcontroller[ g_numflexcontrollers ].name ) );
			Q_strncpy( g_flexcontroller[g_numflexcontrollers].type, "eyes", sizeof( g_flexcontroller[ g_numflexcontrollers ].name ) );
			g_flexcontroller[g_numflexcontrollers].min = -45.0f;
			g_flexcontroller[g_numflexcontrollers].max = 45.0f;
			++g_numflexcontrollers;
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void LoadQcModelElements( s_source_t *pSource, s_model_t *pModel, CDmeModel *pDmeModel )
{
	if ( !pModel || !pDmeModel )
		return;

	CDmAttribute *pQcModelElementsAttr = pDmeModel->GetAttribute( "qcModelElements", AT_ELEMENT_ARRAY );
	if ( !pQcModelElementsAttr )
		return;

	CDmrElementArray< CDmElement > qcModelElements( pQcModelElementsAttr );

	LoadEyeballs( pSource, pModel, qcModelElements );

	for ( int i = 0; i < qcModelElements.Count(); ++i )
	{
		LoadEyelid( pModel, CastElement< CDmeEyelid >( qcModelElements.Element( i ) ) );
	}

	for ( int i = 0; i < qcModelElements.Count(); ++i )
	{
		LoadMouth( CastElement< CDmeMouth >( qcModelElements.Element( i ) ) );
	}
}


#ifdef MDLCOMPILE
//-----------------------------------------------------------------------------
// Allocates a source
// Applies the .dmx extension because the source searching algorithm looks
// for .dmx
//-----------------------------------------------------------------------------
static s_source_t *AllocateDmxSource( const char *pSourceName )
{
	// Allocate a new source
	s_source_t *pSource = (s_source_t *)calloc( 1, sizeof( s_source_t ) );
	g_source[g_numsources++] = pSource;
	Q_strncpy( pSource->filename, pSourceName, sizeof( pSource->filename ) );
	Q_SetExtension( pSource->filename, "dmx", sizeof( pSource->filename ) );
	VectorCopy( g_defaultadjust, pSource->adjust );
	pSource->scale = 1.0f;
	pSource->rotation = g_defaultrotation;
	return pSource;
}


//-----------------------------------------------------------------------------
// Purpose: Creates an animation from the Dme skeleton specified in the
//          bone map.  NOTE: It doesn't look for 'bindPose', it uses the
//          current pose of the skeleton
//-----------------------------------------------------------------------------
static void CreateAnimFromSkeleton( s_source_t *pSource, const char *pSequenceName, const BoneTransformMap_t &boneMap )
{
	s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, pSequenceName );
	pSourceAnim->startframe = 0;
	pSourceAnim->endframe = 0;
	pSourceAnim->numframes = 1;

	// Default all transforms to identity
	pSourceAnim->rawanim[0] = (s_bone_t *)calloc( pSource->numbones, sizeof(s_bone_t) );
	for ( int i = 0; i < pSource->numbones; ++i )
	{
		pSourceAnim->rawanim[0][i].pos.Init();
		pSourceAnim->rawanim[0][i].rot.Init();
	}

	matrix3x4_t jointTransform;
	for ( int nBoneIndex = 0; nBoneIndex < boneMap.m_nBoneCount; ++nBoneIndex )
	{
		const CDmeTransform *pDmeTransform = boneMap.m_ppTransforms[ nBoneIndex ];
		s_bone_t &mdlBone = pSourceAnim->rawanim[0][ nBoneIndex ];
		VectorCopy( pDmeTransform->GetPosition() * g_currentscale, mdlBone.pos );
		QuaternionAngles( pDmeTransform->GetOrientation(), mdlBone.rot );
	}

	// This copies the named animation into the poseToBone array in the source
	Build_Reference( pSource, pSequenceName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadBoneMaskList( CDmeBoneMaskList *pDmeBoneMaskList )
{
	if ( !pDmeBoneMaskList )
		return;

	CDmeBoneMask *pDmeDefaultBoneMask = pDmeBoneMaskList->m_eDefaultBoneMask.GetElement();

	for ( int i = 0; i < pDmeBoneMaskList->m_BoneMaskList.Count(); ++i )
	{
		CDmeBoneMask *pDmeBoneMask = pDmeBoneMaskList->m_BoneMaskList.Element( i );
		if ( !pDmeBoneMask )
			return;

		int nBoneMaskIndex = g_numweightlist;

		if ( pDmeBoneMask == pDmeDefaultBoneMask )
		{
			nBoneMaskIndex = 0;
		}

		if ( nBoneMaskIndex >= MAXWEIGHTLISTS )
		{
			MdlWarning( "1300: Too many bone masks, max %d, ignoring %s\n", MAXWEIGHTLISTS, pDmeBoneMask->GetName() );
		}

		bool bDuplicate = false;
		for ( int j = 0; j < g_numweightlist; ++j )
		{
			if ( g_weightlist[ j ].name && !Q_stricmp( g_weightlist[ j ].name, pDmeBoneMask->GetName() ) )
			{
				bDuplicate = true;
				MdlWarning( "1301: Ignoring duplicate bone mask %s\n", pDmeBoneMask->GetName() );
				break;
			}
		}

		if ( bDuplicate )
			continue;

		s_weightlist_t *pWeightList = &( g_weightlist[ nBoneMaskIndex ] );

		Q_strncpy( pWeightList->name, pDmeBoneMask->GetName(), sizeof( pWeightList->name ) );
		pWeightList->numbones = 0;
		for ( int j = 0; j < pDmeBoneMask->m_BoneWeights.Count(); ++j )
		{
			CDmeBoneWeight *pDmeBoneWeight = pDmeBoneMask->m_BoneWeights.Element( j );
			if ( !pDmeBoneWeight )
				continue;

			int nBoneWeightIndex = pWeightList->numbones;
			if ( nBoneWeightIndex >= MAXWEIGHTSPERLIST )
			{
				MdlWarning( "1302: Too many bones in weightlist %s, ignoring weight for %s (%f)\n", pWeightList->name, pDmeBoneWeight->GetName(), pDmeBoneWeight->m_flWeight.Get() );
				continue;
			}

			pWeightList->bonename[ nBoneWeightIndex ] = strdup( pDmeBoneWeight->GetName() );

			// 'weight' attribute is normally used for both rotation and position weight
			pWeightList->boneweight[ nBoneWeightIndex ] = pDmeBoneWeight->m_flWeight.Get();
			pWeightList->boneposweight[ nBoneWeightIndex ] = pWeightList->boneweight[ nBoneWeightIndex ];

			// Look for optional 'positionWeight' attribute, use it separately for position if it exists
			CDmAttribute *pDmePositionWeightAttr = pDmeBoneWeight->GetAttribute( "positionWeight", AT_FLOAT );
			if ( pDmePositionWeightAttr )
			{
				pWeightList->boneposweight[ nBoneWeightIndex ] = pDmePositionWeightAttr->GetValue< float >();
				if ( pWeightList->boneweight[ nBoneWeightIndex ] == 0 && pWeightList->boneposweight[ nBoneWeightIndex ] > 0 )
				{
					MdlWarning( "1303: Non-zero position weight with zero rotation weight not allowed for bone weight list %s:%s P: %f R: %f, ignoring position weight\n",
						pWeightList->name, pWeightList->bonename[ nBoneWeightIndex ], pWeightList->boneposweight[ nBoneWeightIndex ], pWeightList->boneweight[ nBoneWeightIndex ] );
					pWeightList->boneposweight[ nBoneWeightIndex ] = pWeightList->boneweight[ nBoneWeightIndex ];
				}
			}

			++pWeightList->numbones;
		}

		if ( nBoneMaskIndex != 0 )
		{
			++g_numweightlist;
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadPoseParameterList( CDmePoseParameterList *pDmePoseParameterList )
{
	if ( !pDmePoseParameterList )
		return;

	const int nPoseParameterCount = pDmePoseParameterList->m_ePoseParameterList.Count();
	for ( int i = 0; i < nPoseParameterCount; ++i )
	{
		const CDmePoseParameter *pDmePoseParameter = pDmePoseParameterList->m_ePoseParameterList[ i ];

		if ( g_numposeparameters >= MAXSTUDIOPOSEPARAM )
		{
			MdlWarning( "1900: Too many pose parameters, ignoring from %s\n", pDmePoseParameter->GetName() );
		}

		// This is like FindOrCreatePoseParamater, name is copied into g_pose[].name
		const int nPoseParameterIndex = LookupPoseParameter( pDmePoseParameter->GetName() );

		s_poseparameter_t *pPoseParameter = &g_pose[ nPoseParameterIndex ];
		Q_strncpy( pPoseParameter->name, pDmePoseParameter->GetName(), sizeof( pPoseParameter->name ) );
		pPoseParameter->min = pDmePoseParameter->m_flMin;
		pPoseParameter->max = pDmePoseParameter->m_flMax;
		if ( pDmePoseParameter->m_bWrap )
		{
			pPoseParameter->flags |= STUDIO_LOOPING;
			pPoseParameter->loop = pPoseParameter->max - pPoseParameter->min;
		}
		else if ( pDmePoseParameter->m_bLoop )
		{
			pPoseParameter->flags |= STUDIO_LOOPING;
			pPoseParameter->loop = pDmePoseParameter->m_flLoopRange;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Load dmeSequence.ikChainList into the sequence
//-----------------------------------------------------------------------------
static void LoadIkChainList( CDmeSequenceList *pDmeSequenceList )
{
	if ( !pDmeSequenceList )
		return;

	const int nIkChainCount = pDmeSequenceList->m_eIkChainList.Count();
	for ( int i = 0; i < nIkChainCount; ++i )
	{
		CDmeIkChain *pDmeIkChain = pDmeSequenceList->m_eIkChainList[ i ];
		if ( !pDmeIkChain )
			continue;

		const char *pIkChainName = pDmeIkChain->GetName();

		// Check for duplicates
		int j;
		for ( j = 0; j < g_numikchains; ++j )
		{
			if ( !Q_stricmp( pIkChainName, g_ikchain[ j ].name ) )
				break;
		}

		if ( j < g_numikchains )
		{
			if ( !g_quiet )
			{
				MdlWarning( "1401: Duplicate IkChain: %s Ignored\n", pIkChainName );
			}
		}

		// Set defaults
		g_ikchain[g_numikchains].axis = STUDIO_Z;	// Not actually used
		g_ikchain[g_numikchains].value = 0.0;		// Not actually used

		g_ikchain[g_numikchains].height = 18.0;
		g_ikchain[g_numikchains].floor = 0.0;
		g_ikchain[g_numikchains].radius = 0.0;

		Q_strncpy( g_ikchain[ g_numikchains ].name, pIkChainName, sizeof( g_ikchain[ g_numikchains ].name ) );
		Q_strncpy( g_ikchain[ g_numikchains ].bonename, pDmeIkChain->m_sEndJoint, sizeof( g_ikchain[ g_numikchains ].bonename ) );
		g_ikchain[ g_numikchains ].height = pDmeIkChain->m_flHeight;
		g_ikchain[ g_numikchains ].floor = pDmeIkChain->m_flFloor;
		g_ikchain[ g_numikchains ].radius = pDmeIkChain->m_flPad / 2.0f;
		g_ikchain[ g_numikchains ].link[0].kneeDir = pDmeIkChain->m_vKnee;
		g_ikchain[ g_numikchains ].center = pDmeIkChain->m_vCenter;

		++g_numikchains;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void LoadAnimationOptions( CDmeSequence *pDmeSimpleSequence, s_animation_t *pAnimation )
{
	if ( !pAnimation || !pDmeSimpleSequence )
		return;

	pAnimation->fps = pDmeSimpleSequence->m_flFPS;
	pAnimation->adjust = pDmeSimpleSequence->m_vOrigin;
	// Right now don't include rotation, but if added, likely combine it with the
	// default rotation which is already in pAnimation->rotation
	//	QuaternionAngles( pDmeSequence->m_qRotation, pAnimation->rotation );
	pAnimation->scale = pDmeSimpleSequence->m_flScale;

	pAnimation->looprestart = pDmeSimpleSequence->m_nStartLoop;

	if ( pDmeSimpleSequence->m_bLoop )
	{
		pAnimation->flags |= STUDIO_LOOPING;
	}

	// This should be removed and handled by assemble, i.e. STUDIO_NOFORCELOOP should always be set
	if ( !pDmeSimpleSequence->m_bForceLoop )
	{
		pAnimation->flags |= STUDIO_NOFORCELOOP;
	}

	if ( pDmeSimpleSequence->m_bSnap )
	{
		pAnimation->flags |= STUDIO_SNAP;
	}

	if ( pDmeSimpleSequence->m_bPost )
	{
		pAnimation->flags |= STUDIO_POST;
	}

	if ( pDmeSimpleSequence->m_bAutoIk )
	{
		pAnimation->noAutoIK = false;
	}
	else
	{
		pAnimation->noAutoIK = true;
	}

	pAnimation->motionrollback = pDmeSimpleSequence->m_flMotionRollback;

	if ( pDmeSimpleSequence->m_bAnimBlocks )
	{
		pAnimation->disableAnimblocks = false;
	}
	else
	{
		pAnimation->disableAnimblocks = true;
	}

	if ( pDmeSimpleSequence->m_bAnimBlockStall )
	{
		pAnimation->isFirstSectionLocal = false;
	}
	else
	{
		pAnimation->isFirstSectionLocal = true;
	}

	pAnimation->motiontype = pDmeSimpleSequence->m_eMotionControl.GetElement()->GetStudioMotionControl();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int FindWeightList( const char *pWeightListName )
{
	for ( int i = 0; i < g_numweightlist; ++i )
	{
		if ( !Q_stricmp( pWeightListName, g_weightlist[ i ].name ) )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ValidateControlType( int nControlType, const char *pszTypeString, const char *pszNameString )
{
	switch ( nControlType )
	{
	case STUDIO_X:
	case STUDIO_Y:
	case STUDIO_Z:
	case STUDIO_XR:
	case STUDIO_YR:
	case STUDIO_ZR:
	case STUDIO_LX:
	case STUDIO_LY:
	case STUDIO_LZ:
	case STUDIO_LXR:
	case STUDIO_LYR:
	case STUDIO_LZR:
	case STUDIO_LINEAR:
	case STUDIO_QUADRATIC_MOTION:
		return true;
		// OK!
		break;
	default:
		MdlWarning( "1605: Unknown controlType \"0x%04x\" specified for %s:%s\n",
			nControlType,
			pszTypeString,
			pszNameString );
	}

	return false;
}


//-----------------------------------------------------------------------------
//
// Handles
//
// alignto
//   align
//   alignboneto
//     alignbone
//
//-----------------------------------------------------------------------------
bool HandleDmeAnimCmdAlign( s_animcmd_t *pAnimCmd, CDmeAnimCmd *pDmeAnimCmd )
{
	if ( !pDmeAnimCmd )
		return false;

	CDmeAnimCmdAlign *pDmeAnimCmdAlign = CastElement< CDmeAnimCmdAlign >( pDmeAnimCmd );
	if ( !pDmeAnimCmdAlign )
		return false;

	CDmeSequenceBase *pDmeSequenceBase = pDmeAnimCmdAlign->m_eAnimation.GetElement();
	if ( !pDmeSequenceBase )
	{
		MdlWarning( "1608: No DmeSequence specified for %s:%s\n",
			pDmeAnimCmdAlign->GetTypeString(),
			pDmeAnimCmd->GetName() );
		return false;
	}

	s_animation_t *pExtAnim = LookupAnimation( pDmeSequenceBase->GetName() );
	if ( !pExtAnim )
	{
		MdlWarning( "1604: Unknown animation \"%s\" specified for %s:%s\n",
			pDmeSequenceBase->GetName(),
			pDmeAnimCmdAlign->GetTypeString(),
			pDmeAnimCmd->GetName() );
		return false;
	}

	pAnimCmd->cmd = CMD_AO;
	pAnimCmd->u.ao.ref = pExtAnim;
	pAnimCmd->u.ao.pBonename = pDmeAnimCmdAlign->m_sBoneName.IsEmpty() ? NULL : strdup( pDmeAnimCmdAlign->m_sBoneName );
	pAnimCmd->u.ao.srcframe = pDmeAnimCmdAlign->m_nSourceFrame;
	pAnimCmd->u.ao.destframe = pDmeAnimCmdAlign->m_nDestinatonFrame;
	pAnimCmd->u.ao.motiontype = pDmeAnimCmdAlign->m_eMotionControl.GetElement()->GetStudioMotionControl();

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void LoadAnimationCommands( CDmeSequence *pDmeSimpleSequence, s_animation_t *pAnimation )
{
	if ( !pDmeSimpleSequence || !pAnimation )
		return;

	const int nAnimCmdCount = pDmeSimpleSequence->m_eAnimationCommandList.Count();
	for ( int i = 0; i < nAnimCmdCount; ++i )
	{
		CDmeAnimCmd *pDmeAnimCmd = pDmeSimpleSequence->m_eAnimationCommandList[ i ];
		if ( !pDmeAnimCmd )
			continue;

		if ( pAnimation->numcmds >= MAXSTUDIOCMDS )
		{
			MdlWarning( "1600: Too many animation commands for anim: %s, ignoring from %d:%s\n", pAnimation->name, i, pDmeAnimCmd->GetName() );
			return;
		}

		s_animcmd_t *pAnimCmd = &pAnimation->cmds[ pAnimation->numcmds ];

		if ( pDmeAnimCmd->IsA( CDmeAnimCmdFixupLoop::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdFixupLoop *pDmeAnimCmdFixupLoop = CastElement< CDmeAnimCmdFixupLoop >( pDmeAnimCmd );
			pAnimCmd->cmd = CMD_FIXUP;
			pAnimCmd->u.fixuploop.start = pDmeAnimCmdFixupLoop->m_nStartFrame;
			pAnimCmd->u.fixuploop.end = pDmeAnimCmdFixupLoop->m_nEndFrame;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdWeightList::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdWeightList *pDmeAnimCmdWeightList = CastElement< CDmeAnimCmdWeightList >( pDmeAnimCmd );

			const int nWeightListIndex = FindWeightList( pDmeAnimCmdWeightList->m_sWeightListName );
			if ( nWeightListIndex < 0 )
			{
				MdlWarning( "1602: Unknown weightList \"%s\" specified for %s animation command %s.%s[%d] %s\n",
					pDmeAnimCmdWeightList->m_sWeightListName,
					pDmeAnimCmdWeightList->GetTypeString(),
					pDmeSimpleSequence->GetName(), pDmeSimpleSequence->m_eAnimationCommandList.GetAttribute()->GetName(), i,
					pDmeAnimCmd->GetName() );
				continue;
			}
			else
			{
				pAnimCmd->cmd = CMD_WEIGHTS;
				pAnimCmd->u.weightlist.index = nWeightListIndex;
			}
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdSubtract::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdSubtract *pDmeAnimCmdSubtract = CastElement< CDmeAnimCmdSubtract >( pDmeAnimCmd );
			CDmeSequenceBase *pDmeSequenceBase = pDmeAnimCmdSubtract->m_eAnimation.GetElement();
			if ( pDmeSequenceBase )
			{
				s_animation_t *pExtAnim = LookupAnimation( pDmeSequenceBase->GetName() );
				if ( pExtAnim )
				{
					pAnimCmd->cmd = CMD_SUBTRACT;
					pAnimCmd->u.subtract.ref = pExtAnim;
					pAnimCmd->u.subtract.frame = pDmeAnimCmdSubtract->m_nFrame;
					if ( !pDmeAnimCmd->IsA( CDmeAnimCmdPreSubtract::GetStaticTypeSymbol() ) )
					{
						pAnimCmd->u.subtract.flags |= STUDIO_POST;
					}
				}
				else
				{
					MdlWarning( "1603: Unknown animation \"%s\" specified for %s %s.%s[%d] %s\n",
						pDmeSequenceBase->GetName(),
						pDmeAnimCmdSubtract->GetTypeString(),
						pDmeSimpleSequence->GetName(), pDmeSimpleSequence->m_eAnimationCommandList.GetAttribute()->GetName(), i,
						pDmeAnimCmd->GetName() );
					continue;
				}
			}
			else
			{
				MdlWarning( "1607: No DmeSequenceBase specified for %s %s.%s[%d] %s\n",
					pDmeAnimCmdSubtract->GetTypeString(),
					pDmeSimpleSequence->GetName(), pDmeSimpleSequence->m_eAnimationCommandList.GetAttribute()->GetName(), i,
					pDmeAnimCmd->GetName() );
				continue;
			}
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdAlign::GetStaticTypeSymbol() ) )
		{
			if ( !HandleDmeAnimCmdAlign( pAnimCmd, pDmeAnimCmd ) )
				continue;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdRotateTo::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdRotateTo *pDmeAnimCmdRotateTo = CastElement< CDmeAnimCmdRotateTo >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_ANGLE;
			pAnimCmd->u.angle.angle = pDmeAnimCmdRotateTo->m_flAngle;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdWalkFrame::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdWalkFrame *pDmeAnimCmdWalkFrame = CastElement< CDmeAnimCmdWalkFrame >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_MOTION;
			pAnimCmd->u.motion.iEndFrame = pDmeAnimCmdWalkFrame->m_nEndFrame;
			pAnimCmd->u.motion.motiontype = pDmeAnimCmdWalkFrame->m_eMotionControl.GetElement()->GetStudioMotionControl();
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdDerivative::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdDerivative *pDmeAnimCmdDerivative = CastElement< CDmeAnimCmdDerivative >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_DERIVATIVE;
			pAnimCmd->u.derivative.scale = pDmeAnimCmdDerivative->m_flScale;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdDerivative::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdDerivative *pDmeAnimCmdDerivative = CastElement< CDmeAnimCmdDerivative >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_DERIVATIVE;
			pAnimCmd->u.derivative.scale = pDmeAnimCmdDerivative->m_flScale;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdLinearDelta::GetStaticTypeSymbol() ) )
		{
			pAnimCmd->cmd = CMD_LINEARDELTA;
			pAnimCmd->u.linear.flags |= STUDIO_AL_POST;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdSplineDelta::GetStaticTypeSymbol() ) )
		{
			pAnimCmd->cmd = CMD_LINEARDELTA;
			pAnimCmd->u.linear.flags |= STUDIO_AL_POST;
			pAnimCmd->u.linear.flags |= STUDIO_AL_SPLINE;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdCompress::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdCompress *pDmeAnimCmdCompress = CastElement< CDmeAnimCmdCompress >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_COMPRESS;
			pAnimCmd->u.compress.frames = pDmeAnimCmdCompress->m_nSkipFrames;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdNumFrames::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdNumFrames *pDmeAnimCmdNumFrames = CastElement< CDmeAnimCmdNumFrames >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_NUMFRAMES;
			pAnimCmd->u.compress.frames = pDmeAnimCmdNumFrames->m_nFrames;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdLocalHierarchy::GetStaticTypeSymbol() ) )
		{
			CDmeAnimCmdLocalHierarchy *pDmeAnimCmdLocalHierarchy = CastElement< CDmeAnimCmdLocalHierarchy >( pDmeAnimCmd );

			pAnimCmd->cmd = CMD_LOCALHIERARCHY;
			pAnimCmd->u.localhierarchy.pBonename = strdup( pDmeAnimCmdLocalHierarchy->m_sBoneName.Get() );
			pAnimCmd->u.localhierarchy.pParentname = strdup( pDmeAnimCmdLocalHierarchy->m_sParentBoneName.Get() );
			pAnimCmd->u.localhierarchy.start = pDmeAnimCmdLocalHierarchy->m_flStartFrame;
			pAnimCmd->u.localhierarchy.peak = pDmeAnimCmdLocalHierarchy->m_flPeakFrame;
			pAnimCmd->u.localhierarchy.tail = pDmeAnimCmdLocalHierarchy->m_flTailFrame;
			pAnimCmd->u.localhierarchy.end = pDmeAnimCmdLocalHierarchy->m_flEndFrame;
		}
		else if ( pDmeAnimCmd->IsA( CDmeAnimCmdNoAnimation::GetStaticTypeSymbol() ) )
		{
			pAnimCmd->cmd = CMD_NOANIMATION;
		}
		else
		{
			MdlWarning( "1601: Unhandled DmeAnimCmd %s (%s)\n", pDmeAnimCmd->GetTypeString(), pDmeAnimCmd->GetName() );
			continue;
		}

		++pAnimation->numcmds;
	}
}


//-----------------------------------------------------------------------------
// TODO: Rework to handle all animation related commands
//-----------------------------------------------------------------------------
void LoadIkRuleList( CDmeSequence *pDmeSimpleSequence, s_animation_t *pAnimation )
{
	if ( !pDmeSimpleSequence )
		return;

	matrix3x4_t mDefRot;
	AngleMatrix( g_defaultrotation, mDefRot );

	// See if we have any IkRules to load
	const int nIkRuleCount = pDmeSimpleSequence->m_eIkRuleList.Count();
	if ( nIkRuleCount <= 0 )
		return;

	for ( int i = 0; i < nIkRuleCount; ++i )
	{
		CDmeIkRule *pDmeIkRule = pDmeSimpleSequence->m_eIkRuleList[ i ];
		if ( !pDmeIkRule )
			continue;

		if ( pAnimation->numcmds >= MAXSTUDIOCMDS )
		{
			// TODO: Issue a warning
			MdlWarning( "1503: Too many animation commands for anim: %s, ignoring from %s:%s\n",
				pAnimation->name, pDmeIkRule->GetTypeString(), pDmeIkRule->GetName() );
			return;
		}

		// Find the IkChain
		CDmeIkChain *pDmeIkChain = pDmeIkRule->m_eIkChain.GetElement();
		if ( !pDmeIkChain )
		{
			MdlError( "1501: No IkChain element assigned to %s:%s\n", pDmeIkRule->GetTypeString(), pDmeIkRule->GetName() );
			continue;
		}

		int nIkChainIndex = -1;
		for ( int j = 0; j < g_numikchains; ++j )
		{
			if ( !Q_stricmp( pDmeIkChain->GetName(), g_ikchain[ j ].name ) )
			{
				nIkChainIndex = j;
				break;
			}
		}

		if ( nIkChainIndex < 0 )
		{
			MdlWarning( "1504: Cannot find IkChain referenced by IkRule %s:%s, ignoring\n", pDmeIkRule->GetName(), pDmeIkRule->GetTypeString() );
			continue;
		}

		// calloc sets memory to 0
		s_ikrule_t *pIkRule = reinterpret_cast< s_ikrule_t * >( calloc( 1, sizeof( s_ikrule_t ) ) );
		if ( !pIkRule )
		{
			MdlWarning( "1502: Cannot allocate memory for IkRule %s:%s, ignoring\n", pDmeIkRule->GetName(), pDmeIkRule->GetTypeString() );
			return;
		}

		pIkRule->chain = nIkChainIndex;
		pIkRule->slot = nIkChainIndex;	// Default slot

		CDmeIkTouchRule *pDmeIkTouchRule = CastElement< CDmeIkTouchRule >( pDmeIkRule );
		CDmeIkFootstepRule *pDmeIkFootstepRule = CastElement< CDmeIkFootstepRule >( pDmeIkRule );
		CDmeIkAttachmentRule *pDmeIkAttachmentRule = CastElement< CDmeIkAttachmentRule >( pDmeIkRule );
		CDmeIkReleaseRule *pDmeIkReleaseRule = CastElement< CDmeIkReleaseRule >( pDmeIkRule );

		if ( pDmeIkTouchRule )
		{
			pIkRule->type = IK_SELF;
			Q_strncpy( pIkRule->bonename, pDmeIkTouchRule->m_sBoneName, sizeof( pIkRule->bonename ) );
		}
		else if ( pDmeIkFootstepRule )
		{
			pIkRule->type = IK_GROUND;
			pIkRule->height = pDmeIkFootstepRule->GetValue< float >( "height", g_ikchain[ pIkRule->chain ].height );
			pIkRule->floor = pDmeIkFootstepRule->GetValue< float >( "floor", g_ikchain[ pIkRule->chain ].floor );
			pIkRule->radius = pDmeIkFootstepRule->GetValue< float >( "pad", g_ikchain[ pIkRule->chain ].radius * 2.0f ) / 2.0f;
			if ( pDmeIkFootstepRule->HasAttribute( "contact", AT_INT ) )
			{
				pIkRule->contact = pDmeIkFootstepRule->GetValue< int >( "contact" );
			}
		}
		else if ( pDmeIkAttachmentRule )
		{
			pIkRule->type = IK_ATTACHMENT;
			Q_strncpy( pIkRule->attachment, pDmeIkAttachmentRule->m_sAttachmentName, sizeof( pIkRule->attachment ) );

			if ( pDmeIkAttachmentRule->HasAttribute( "fallbackBone", AT_STRING ) )
			{
				Q_strncpy( pIkRule->bonename, pDmeIkAttachmentRule->GetValueString( "fallbackBone" ), sizeof( pIkRule->bonename ) );
			}

			if ( pDmeIkAttachmentRule->HasAttribute( "fallbackPosition", AT_VECTOR3 ) )
			{
				VectorIRotate( pDmeIkAttachmentRule->GetValue< Vector >( "fallbackPosition" ), mDefRot, pIkRule->pos );
				pIkRule->bone = -1;
			}

			if ( pDmeIkAttachmentRule->HasAttribute( "fallbackRotation", AT_QUATERNION ) )
			{
				// TODO: Adjust rotation as above for position adjustment?
				pIkRule->q = pDmeIkAttachmentRule->GetValue< Quaternion >( "fallbackRotation" );
				pIkRule->bone = -1;
			}
		}
		else if ( pDmeIkReleaseRule )
		{
			pIkRule->type = IK_RELEASE;
		}
		else
		{
			MdlWarning( "1500: Unknown IkRuleType %s:%s, ignoring\n", pDmeIkRule->GetName(), pDmeIkRule->GetTypeString() );
			continue;
		}

		switch ( pDmeIkRule->m_nUseType )
		{
		case CDmeIkRule::USE_SEQUENCE:
			pIkRule->usesequence = true;
			pIkRule->usesource = false;
			break;
		case CDmeIkRule::USE_SOURCE:
			pIkRule->usesequence = false;
			pIkRule->usesource = true;
			break;
		case CDmeIkRule::USE_NONE:
		default:
			// Nothing
			break;
		}

		CDmeIkRange *pDmeIkRange = pDmeIkRule->m_eRange.GetElement();
		if ( pDmeIkRange )
		{
			pIkRule->start = pDmeIkRange->m_nStartFrame;
			pIkRule->peak = pDmeIkRange->m_nMaxStartFrame;
			pIkRule->tail = pDmeIkRange->m_nMaxEndFrame;
			pIkRule->end = pDmeIkRange->m_nEndFrame;
		}

		s_animcmd_t *pAnimCmd = &( pAnimation->cmds[ pAnimation->numcmds ] );

		pAnimCmd->cmd = CMD_IKRULE;
		pAnimCmd->u.ikrule.pRule = pIkRule;

		pAnimation->numcmds++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads all of the AddLayer/BlendLayer for the specified sequence
//-----------------------------------------------------------------------------
void LoadLayerList( CDmeSequenceBase *pDmeSequenceBase, s_sequence_t *pSequence )
{
	if ( !pDmeSequenceBase || !pSequence )
		return;

	const int nLayerCount = pDmeSequenceBase->m_eLayerList.Count();
	for ( int i = 0; i < nLayerCount; ++i )
	{
		CDmeSequenceLayerBase *pDmeLayerBase = pDmeSequenceBase->m_eLayerList[ i ];
		if ( !pDmeLayerBase )
			continue;

		CDmeSequenceBase *pDmeRefSeq = pDmeLayerBase->m_eAnimation.GetElement();
		if ( !pDmeRefSeq )
		{
			// TODO: Warning message - No sequence specified
			continue;
		}

		if ( pSequence->numautolayers >= ARRAYSIZE( pSequence->autolayer ) )
		{
			// TODO: Warning message - Too many layers
			break;
		}

		Q_strncpy( pSequence->autolayer[ pSequence->numautolayers ].name, pDmeRefSeq->GetName(), sizeof( pSequence->autolayer[ pSequence->numautolayers ].name ) );

		CDmeSequenceAddLayer *pDmeAddLayer = CastElement< CDmeSequenceAddLayer >( pDmeSequenceBase->m_eLayerList[ i ] );
		if ( pDmeAddLayer )
		{
			// Nothing to do
		}

		CDmeSequenceBlendLayer *pDmeBlendLayer = CastElement< CDmeSequenceBlendLayer >( pDmeSequenceBase->m_eLayerList[ i ] );
		if ( pDmeBlendLayer )
		{
			pSequence->autolayer[ pSequence->numautolayers ].start = pDmeBlendLayer->m_flStartFrame;
			pSequence->autolayer[ pSequence->numautolayers ].peak = pDmeBlendLayer->m_flPeakFrame;
			pSequence->autolayer[ pSequence->numautolayers ].tail = pDmeBlendLayer->m_flTailFrame;
			pSequence->autolayer[ pSequence->numautolayers ].end = pDmeBlendLayer->m_flEndFrame;
			pSequence->autolayer[ pSequence->numautolayers ].flags |= pDmeBlendLayer->m_bCrossfade ? STUDIO_AL_XFADE : 0;
			pSequence->autolayer[ pSequence->numautolayers ].flags |= pDmeBlendLayer->m_bSpline ? STUDIO_AL_SPLINE : 0;
			pSequence->autolayer[ pSequence->numautolayers ].flags |= pDmeBlendLayer->m_bNoBlend ? STUDIO_AL_NOBLEND : 0;
			pSequence->autolayer[ pSequence->numautolayers ].flags |= pDmeBlendLayer->m_bLocal ? STUDIO_AL_LOCAL : 0;
			pSequence->flags |= pDmeBlendLayer->m_bLocal ? STUDIO_AL_LOCAL : 0;
			if ( !pDmeBlendLayer->m_sPoseParameterName.IsEmpty() )
			{
				pSequence->autolayer[ pSequence->numautolayers ].flags |= STUDIO_AL_POSE;
				pSequence->autolayer[ pSequence->numautolayers ].pose = LookupPoseParameter( pDmeBlendLayer->m_sPoseParameterName.Get() );
			}
		}

		pSequence->numautolayers++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads all of the IkLocks for the specified Sequence
//-----------------------------------------------------------------------------
void LoadIkLockList( CDmeSequenceBase *pDmeSequenceBase, s_sequence_t *pSequence )
{
	if ( !pDmeSequenceBase || !pSequence )
		return;

	const int nIkLockCount = pDmeSequenceBase->m_eIkLockList.Count();
	for ( int i = 0; i < nIkLockCount; ++i )
	{
		CDmeIkLock *pDmeIkLock = pDmeSequenceBase->m_eIkLockList[ i ];
		if ( !pDmeIkLock )
			continue;

		// TODO: Check for duplicates?
		CDmeIkChain *pDmeIkChain = pDmeIkLock->m_eIkChain.GetElement();
		if ( pDmeIkChain )
		{
			Q_strncpy( pSequence->iklock[ pSequence->numiklocks ].name, pDmeIkChain->GetName(), sizeof( pSequence->iklock[ pSequence->numiklocks ].name ) );
		}
		else
		{
			MdlError( "1700: No IkChain element assigned to %s:%s\n", pDmeIkLock->GetTypeString(), pDmeIkLock->GetName() );
		}
		pSequence->iklock[ pSequence->numiklocks ].flPosWeight = clamp( pDmeIkLock->m_flLockPosition.Get(), 0.0f, 1.0f );
		pSequence->iklock[ pSequence->numiklocks ].flLocalQWeight = RemapValClamped( pDmeIkLock->m_flLockRotation, 0.0f, 1.0f, 1.0f, 0.0f );	// Invert it for historical reasons

		pSequence->numiklocks++;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void LoadAnimationEventList( CDmeSequenceBase *pDmeSequenceBase, s_sequence_t *pSequence )
{
	if ( !pDmeSequenceBase || !pSequence )
		return;

	CDmAttribute *pBlah = pDmeSequenceBase->GetAttribute( "animationEventList" );
	int nFoo = 0;
	if ( pBlah )
	{
		nFoo = CDmrGenericArrayConst( pBlah ).Count();
	}
	const int nAnimEventCount = pDmeSequenceBase->m_eAnimationEventList.Count();
	for ( int i = 0; i < nAnimEventCount; ++i )
	{
		CDmeAnimationEvent *pDmeAnimationEvent = pDmeSequenceBase->m_eAnimationEventList[ i ];
		if ( !pDmeAnimationEvent )
			continue;

		// Not sure why this is +1... always one special event??
		if ( pSequence->numevents + 1 >= MAXSTUDIOEVENTS )
		{
			MdlError( "1800: Too many %s's on %s:%s, ignoring from %s\n",
				pDmeAnimationEvent->GetTypeString(),
				pDmeSequenceBase->GetTypeString(),
				pDmeSequenceBase->GetName(),
				pDmeAnimationEvent->GetName() );
			return;
		}

		Q_strncpy( pSequence->event[ pSequence->numevents ].eventname, pDmeAnimationEvent->GetName(), sizeof( pSequence->event[ pSequence->numevents ].eventname ) );
		pSequence->event[ pSequence->numevents ].frame = pDmeAnimationEvent->m_nFrame;
		if ( pDmeAnimationEvent->m_sDataString.IsEmpty() )
		{
			pSequence->event[ pSequence->numevents ].options[0] = '\0';
		}
		else
		{
			Q_strncpy( pSequence->event[ pSequence->numevents ].options, pDmeAnimationEvent->m_sDataString, sizeof( pSequence->event[ pSequence->numevents ].options ) );
		}
		++pSequence->numevents;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadSequenceList( CDmeMultiSequence *pDmeMultiSequence, CUtlVector< s_animation_t * > &animations )
{
	if ( !pDmeMultiSequence )
		return;

	const int nSequenceCount = pDmeMultiSequence->m_eSequenceList.Count();
	for ( int i = 0; i < nSequenceCount; ++i )
	{
		CDmeSequence *pDmeSubSequence = pDmeMultiSequence->m_eSequenceList[ i ];
		if ( !pDmeSubSequence )
			continue;

		s_animation_t *pAnim = LookupAnimation( pDmeSubSequence->GetName() );
		if ( !pAnim )
		{
			MdlWarning( "1208: DmeSequence %s: Couldn't find referenced animation: %s\n", pDmeMultiSequence->GetName(), pDmeSubSequence->GetName() );
			continue;
		}

		// TODO: Limit to 64?
		animations.AddToTail( pAnim );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadSequenceBlends( CDmeMultiSequence *pDmeMultiSequence, s_sequence_t *pSequence )
{
	if ( !pSequence || !pDmeMultiSequence )
		return;

	const int nBlendCount = pDmeMultiSequence->m_eBlendList.Count();
	for ( int i = 0; i < nBlendCount; ++i )
	{
		CDmeSequenceBlendBase *pDmeSequenceBlendBase = pDmeMultiSequence->m_eBlendList[ i ];
		if ( !pDmeSequenceBlendBase )
			continue;

		if ( pDmeSequenceBlendBase->IsA( CDmeSequenceBlend::GetStaticTypeSymbol() ) )
		{
			CDmeSequenceBlend *pDmeSequenceBlend = CastElement< CDmeSequenceBlend >( pDmeSequenceBlendBase );

			const int i = ( pSequence->paramindex[0] != -1 ) ? 1 : 0;
			const int j = LookupPoseParameter( pDmeSequenceBlend->m_sPoseParameterName.Get() );
			pSequence->paramindex[i] = j;
			pSequence->paramattachment[i] = -1;
			pSequence->paramstart[i] = pDmeSequenceBlend->m_flParamStart;
			pSequence->paramend[i] = pDmeSequenceBlend->m_flParamEnd;

			g_pose[j].min  = MIN( g_pose[j].min, pSequence->paramstart[i] );
			g_pose[j].min  = MIN( g_pose[j].min, pSequence->paramend[i] );
			g_pose[j].max  = MAX( g_pose[j].max, pSequence->paramstart[i] );
			g_pose[j].max  = MAX( g_pose[j].max, pSequence->paramend[i] );
		}
		else if ( pDmeSequenceBlendBase->IsA( CDmeSequenceCalcBlend::GetStaticTypeSymbol() ) )
		{
			// TODO: This really isn't an animation command but the distinction might not be important?
			CDmeSequenceCalcBlend *pDmeSequenceCalcBlend = CastElement< CDmeSequenceCalcBlend >( pDmeSequenceBlendBase );

			const int i = ( pSequence->paramindex[0] != -1 ) ? 1 : 0;
			const int j = LookupPoseParameter( pDmeSequenceCalcBlend->m_sPoseParameterName.Get() );
			pSequence->paramindex[i] = j;
			pSequence->paramattachment[i] = LookupAttachment( pDmeSequenceCalcBlend->m_sAttachmentName.Get() );
			pSequence->paramcontrol[i] = pDmeSequenceCalcBlend->m_eMotionControl.GetElement()->GetStudioMotionControl();

			if ( pSequence->paramattachment[i] < 0 )
			{
				MdlWarning( "1606: Unknown Attachment For %s - %s.%s = %s\n",
					pDmeSequenceCalcBlend->GetTypeString(),
					pDmeSequenceCalcBlend->GetName(),
					pDmeSequenceCalcBlend->m_sAttachmentName.GetAttribute()->GetName(),
					pDmeSequenceCalcBlend->m_sAttachmentName.Get() );
			}

			continue;	// Don't add it as an animation command
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void LoadBlendRefCompCenter( CDmeMultiSequence *pDmeMultiSequence, s_sequence_t *pSequence )
{
	if ( !pDmeMultiSequence || !pSequence )
		return;

	CDmeSequence *pDmeBlendRefSequence = CastElement< CDmeSequence >( pDmeMultiSequence->m_eBlendRef.GetElement() );
	if ( pDmeBlendRefSequence )
	{
		const char *pszBlendRef = pDmeBlendRefSequence->GetName();
		if ( pszBlendRef && *pszBlendRef )
		{
			pSequence->paramanim = LookupAnimation( pszBlendRef );
			if ( pSequence->paramanim == NULL )
			{
				MdlWarning( "1202: DmeSequence %s: Unknown .blendRef animation: %s\n", pDmeMultiSequence->GetName(), pszBlendRef );
			}
		}
	}

	CDmeSequence *pDmeBlendRefComp = CastElement< CDmeSequence >( pDmeMultiSequence->m_eBlendComp.GetElement() );
	if ( pDmeBlendRefComp )
	{
		const char *pszBlendComp = pDmeBlendRefComp->GetName();
		if ( pszBlendComp && *pszBlendComp )
		{
			pSequence->paramcompanim = LookupAnimation( pszBlendComp );
			if ( pSequence->paramcompanim == NULL )
			{
				MdlWarning( "1203: DmeSequence %s: Unknown .blendComp animation: %s\n", pDmeMultiSequence->GetName(), pszBlendComp );
			}
		}
	}

	CDmeSequence *pDmeBlendRefCenter = CastElement< CDmeSequence >( pDmeMultiSequence->m_eBlendCenter.GetElement() );
	if ( pDmeBlendRefCenter )
	{
		const char *pszBlendCenter = pDmeBlendRefCenter->GetName();
		if ( pszBlendCenter && *pszBlendCenter )
		{
			pSequence->paramcenter = LookupAnimation( pszBlendCenter );
			if ( pSequence->paramcenter == NULL )
			{
				MdlWarning( "1204: DmeSequence %s: Unknown .blendCenter animation: %s\n", pDmeMultiSequence->GetName(), pszBlendCenter );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void CreateBindPoseSequence( s_source_t *pMainSource )
{
	s_sourceanim_t *pSourceAnim = FindSourceAnim( pMainSource, "BindPose" );
	if ( pSourceAnim )
	{
		// Create the first "BindPose" default sequence
		s_sequence_t *pSeq = ProcessCmdSequence( "BindPose" );
		s_animation_t *pAnim = ProcessImpliedAnimation( pSeq, pMainSource->filename );
		pSeq->panim[0][0] = pAnim;
		ProcessSequence( pSeq, 1, &pAnim, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Load the data from the CDmeAnimBlockSize element which is
//			specified by the .animBlockSize attribute on the root node
//			of the .mpp file.  If it's NULL, do nothing
//			Basically implements Cmd_AnimBlockSize
//-----------------------------------------------------------------------------
static void	LoadAnimBlockSize( CDmeAnimBlockSize *pDmeAnimBlockSize )
{
	if ( !pDmeAnimBlockSize )
		return;

	g_animblocksize = pDmeAnimBlockSize->m_nSize;
	if ( g_animblocksize < 1024 )
	{
		g_animblocksize *= 1024;
	}

	g_bNoAnimblockStall = !pDmeAnimBlockSize->m_bStall;
	switch ( pDmeAnimBlockSize->m_nStorageType )
	{
	case CDmeAnimBlockSize::ANIMBLOCKSTORAGETYPE_HIRES:
		g_bAnimblockLowRes = false;
		g_bAnimblockHighRes = true;
		break;
	case CDmeAnimBlockSize::ANIMBLOCKSTORAGETYPE_LOWRES:
		// Fallthrough deliberate
	default:
		g_bAnimblockLowRes = true;
		g_bAnimblockHighRes = false;
		break;
	}
}




//-----------------------------------------------------------------------------
// Purpose: Load all of the DmeSequence elements, if the passed
//          CDmeSequenceList is NULL, empty or doesn't contain any valid
//          sequences, a default "BindPose" sequence is created
// Returns: The number of sequences created
//-----------------------------------------------------------------------------
static int LoadAndCreateSequences( s_source_t *pMainSource, CDmeSequenceList *pSequenceList, bool &bSetUpAxis )
{
	CreateBindPoseSequence( pMainSource );

	LoadIkChainList( pSequenceList );

	int nSequenceAddedCount = 0;

	char szSeqName[ MAX_PATH ];
	const int nSequenceCount = pSequenceList ? pSequenceList->m_Sequences.Count() : 0;

	CUtlVector< CDmeSequenceBase * > sortedSequenceList;

	// Add all CDmeSequence elements to the list first
	for ( int nSequenceIndex = 0; nSequenceIndex < nSequenceCount; ++nSequenceIndex )
	{
		CDmeSequenceBase *pDmeSequenceBase = pSequenceList->m_Sequences[ nSequenceIndex ];
		if ( !pDmeSequenceBase )
		{
			MdlWarning( "1201: Empty DmeSequence %s[ %d ]\n", pSequenceList->GetName(), nSequenceIndex );
			continue;
		}

		const char *pszSeqName = pDmeSequenceBase->GetName();
		if ( pszSeqName == NULL || *pszSeqName == '\0' )
		{
			MdlWarning( "1200: Ignoring Unnamed Sequence On %s[%d]\n", pSequenceList->GetName(), nSequenceIndex );
			continue;
		}

		sortedSequenceList.AddToTail( pDmeSequenceBase );
	}

	qsort( sortedSequenceList.Base(), sortedSequenceList.Count(), sizeof( CDmeSequenceBase * ), CDmeSequenceBase::QSortFunction );

	// Process the reordered DmeSequenceBase list
	for ( int nSequenceIndex = 0; nSequenceIndex < sortedSequenceList.Count(); ++nSequenceIndex )
	{
		CUtlVector< s_animation_t * > animations;

		CDmeSequenceBase *pDmeSequenceBase = sortedSequenceList[ nSequenceIndex ];
		// Already checked for NULL & unnamed

		const char *pszSeqName = pDmeSequenceBase->GetName();
		Q_StripExtension( pszSeqName, szSeqName, sizeof( szSeqName ) );

		s_sequence_t *pSeq = ProcessCmdSequence( szSeqName );

		{

			CDmeSequence *pDmeSimpleSequence = CastElement< CDmeSequence >( pDmeSequenceBase );
			CDmeMultiSequence *pDmeMultiSequence = CastElement< CDmeMultiSequence >( pDmeSequenceBase );
			if ( !pDmeSimpleSequence && !pDmeMultiSequence )
			{
				MdlWarning( "1209: Invalid DmeSequence %s[ %d ], not Simple or Multi\n", pSequenceList->GetName(), nSequenceIndex );
				continue;
			}

			if ( pDmeSimpleSequence )
			{
				CDmeModel *pDmeModel = CastElement< CDmeModel >( pDmeSimpleSequence->m_eSkeleton.GetElement() );

				if ( bSetUpAxis && pDmeModel )
				{
					const char *pUpAxis = pDmeModel->GetValueString( "upAxis" );
					if ( pUpAxis )
					{
						if ( StringHasPrefix( pUpAxis, "Y" ) )
						{
							// rotate 90 degrees around x to move y into z
							g_defaultrotation = RadianEuler( M_PI / 2.0f, 0.0f, M_PI / 2.0f );
						}
					}
					bSetUpAxis = false;
				}

				CDmeDag *pDmeSkeleton = pDmeSimpleSequence->m_eSkeleton.GetElement();
				if ( !pDmeSkeleton )
				{
					MdlWarning( "1205: Ignoring Sequence %s, No Skeleton Specified\n", pszSeqName );
					continue;
				}

				s_source_t *pSource = AllocateDmxSource( szSeqName );

				BoneTransformMap_t boneMap;
				if ( !LoadModelAndSkeleton( pSource, boneMap, pDmeSkeleton, pDmeModel, NULL, false ) )
				{
					MdlWarning( "1206: Sequence %s: Ignoring Sequence, Couldn't Load Specified Skeleton: %s\n", pszSeqName, pDmeSkeleton->GetName() );
					continue;
				}

				CDmeAnimationList *pDmeAnimationList = pDmeSimpleSequence->m_eAnimationList.GetElement();
				s_animation_t *pAnim = NULL;

				if ( pDmeAnimationList )
				{
					// Simple sequence created from animation data
					LoadAnimations( pSource, pDmeAnimationList, g_currentscale, boneMap );

					// Check for various CDmeAnimCmd's which may or may not be flagged with bDelta
					bool bDmeAnimCmdDelta = false;
					for ( int i = 0; i < pDmeSimpleSequence->m_eAnimationCommandList.Count(); ++i )
					{
						if ( pDmeSimpleSequence->m_eAnimationCommandList[i]->IsA( CDmeAnimCmdSubtract::GetStaticTypeSymbol() ) ||
							pDmeSimpleSequence->m_eAnimationCommandList[i]->IsA( CDmeAnimCmdPreSubtract::GetStaticTypeSymbol() ) ||
							pDmeSimpleSequence->m_eAnimationCommandList[i]->IsA( CDmeAnimCmdLinearDelta::GetStaticTypeSymbol() ) ||
							pDmeSimpleSequence->m_eAnimationCommandList[i]->IsA( CDmeAnimCmdSplineDelta::GetStaticTypeSymbol() ) )
						{
							bDmeAnimCmdDelta = true;
							pSeq->flags |= STUDIO_DELTA;
						}
					}

					{
						// allocate animation entry
						g_panimation[g_numani] = (s_animation_t *)calloc( 1, sizeof( s_animation_t ) );
						g_panimation[g_numani]->index = g_numani;
						pAnim = g_panimation[g_numani];
						g_numani++;

						pAnim->isImplied = true;

						pAnim->startframe = 0;
						pAnim->endframe = MAXSTUDIOANIMFRAMES - 1;

						strcpy( pAnim->name, "@" );
						strcat( pAnim->name, pSeq->name );
						strcpyn( pAnim->filename, pSource->filename );

						VectorCopy( g_defaultadjust, pAnim->adjust );
						pAnim->scale = 1.0f;

						// Don't re-orient delta animations
						if ( bDmeAnimCmdDelta || !( pDmeSimpleSequence->m_bDelta || pDmeSimpleSequence->m_bPreDelta ) )
						{
							pAnim->rotation = g_defaultrotation;
						}

						pAnim->fps = 30;
						pAnim->motionrollback = g_flDefaultMotionRollback;

						pAnim->source = pSource;
						const int nSourceAnimCount = pAnim->source->m_Animations.Count();
						if ( nSourceAnimCount > 0 )
						{
							s_sourceanim_t *pSourceAnim = &pAnim->source->m_Animations[nSourceAnimCount-1];
							Q_strncpy( pAnim->animationname, pAnim->source->m_Animations[nSourceAnimCount-1].animationname, sizeof(pAnim->animationname) );
							if ( pAnim->startframe < pSourceAnim->startframe )
							{
								pAnim->startframe = pSourceAnim->startframe;
							}

							if ( pAnim->endframe > pSourceAnim->endframe )
							{
								pAnim->endframe = pSourceAnim->endframe;
							}
						}
						else
						{
							Q_strncpy( pAnim->animationname, "", sizeof( pAnim->animationname ) );
						}

						pAnim->numframes = pAnim->endframe - pAnim->startframe + 1;
					}
				}
				else
				{
					// Simple sequence created from a single pose
					CreateAnimFromSkeleton( pSource, szSeqName, boneMap );

					pAnim = ProcessImpliedAnimation( pSeq, pSource->filename );
				}

				Assert( pAnim );
				animations.AddToTail( pAnim );

				// Hack to allow animations commands to refer to same sequence in a simple DmeSequence
				pSeq->panim[0][0] = pAnim;

				// Animation Options
				LoadAnimationOptions( pDmeSimpleSequence, pAnim );

				// Animation Commands
				LoadAnimationCommands( pDmeSimpleSequence, pAnim );

				// IkRules
				LoadIkRuleList( pDmeSimpleSequence, pAnim );

				AddBodyAttachments( pSource );
			}

			// Only apply to multi-sequences
			if ( pDmeMultiSequence )
			{
				LoadSequenceList( pDmeMultiSequence, animations );

				LoadSequenceBlends( pDmeMultiSequence, pSeq );

				LoadBlendRefCompCenter( pDmeMultiSequence, pSeq );

				pSeq->groupsize[ 0 ] = pDmeMultiSequence->m_nBlendWidth.Get();
			}
		}

		LoadAnimationEventList( pDmeSequenceBase, pSeq );

		// Same As ParseSequence
		CDmeSequenceActivity *pDmeSequenceActivity = pDmeSequenceBase->m_eActivity.GetElement();
		if ( pDmeSequenceActivity )
		{
			const char *pszActivityName = pDmeSequenceActivity->GetName();
			if ( pszActivityName && Q_strlen( pszActivityName ) > 0 )
			{
				Q_strncpy( pSeq->activityname, pszActivityName, sizeof( pSeq->activityname ) );
				pSeq->actweight = pDmeSequenceActivity->m_nWeight.Get();

				for ( int nModifierIndex = 0; nModifierIndex < pDmeSequenceActivity->m_sModifierList.Count(); ++nModifierIndex )
				{
					strcpyn( pSeq->activitymodifier[ pSeq->numactivitymodifiers++ ].name, pDmeSequenceActivity->m_sModifierList.Get( nModifierIndex ) );

					if ( pSeq->numactivitymodifiers == MAXSTUDIOACTIVITYMODIFIERS )
						break;
				}

				if ( pDmeSequenceActivity->m_sModifierList.Count() > MAXSTUDIOACTIVITYMODIFIERS )
				{
					MdlWarning( "1210: Too many activity modifiers (%d) on DmeSequence %s, only using first %d\n",
						pDmeSequenceActivity->m_sModifierList.Count(),
						pSeq->name,
						MAXSTUDIOACTIVITYMODIFIERS );
				}
			}
		}

		if ( pDmeSequenceBase->m_bLoop )
		{
			pSeq->flags |= STUDIO_LOOPING;
		}

		if ( pDmeSequenceBase->m_bSnap )
		{
			pSeq->flags |= STUDIO_SNAP;
		}

		if ( pDmeSequenceBase->m_bPost )
		{
			pSeq->flags |= STUDIO_POST;
		}

		if ( pDmeSequenceBase->m_bHidden )
		{
			pSeq->flags |= STUDIO_HIDDEN;
		}

		if ( pDmeSequenceBase->m_bDelta )
		{
			pSeq->flags |= STUDIO_DELTA;
			pSeq->flags |= STUDIO_POST;
		}

		if ( pDmeSequenceBase->m_bWorldSpace )
		{
			pSeq->flags |= STUDIO_WORLD;
			pSeq->flags |= STUDIO_POST;
		}

		if ( pDmeSequenceBase->m_bPreDelta )
		{
			pSeq->flags |= STUDIO_DELTA;
		}

		if ( pDmeSequenceBase->m_bAutoPlay )
		{
			pSeq->flags |= STUDIO_AUTOPLAY;
		}

		if ( pDmeSequenceBase->m_bRealtime )
		{
			pSeq->flags |= STUDIO_REALTIME;
		}

		// AddLayer/BlendLayer
		LoadLayerList( pDmeSequenceBase, pSeq );

		// IkLock
		LoadIkLockList( pDmeSequenceBase, pSeq );

		pSeq->fadeintime = pDmeSequenceBase->m_flFadeIn.Get();
		pSeq->fadeouttime = pDmeSequenceBase->m_flFadeOut.Get();

		const CUtlString &entryNode = pDmeSequenceBase->m_sEntryNode.Get();
		const CUtlString &exitNode = pDmeSequenceBase->m_sExitNode.Get();
		if ( !entryNode.IsEmpty() )
		{
			if ( !exitNode.IsEmpty() )
			{
				pSeq->entrynode = LookupXNode( entryNode.Get() );
				pSeq->exitnode = LookupXNode( exitNode.Get() );
				if ( pDmeSequenceBase->m_bReverseNodeTransition )
				{
					pSeq->nodeflags |= 1;
				}
			}
			else
			{
				pSeq->entrynode = pSeq->exitnode = LookupXNode( entryNode.Get() );
			}
		}

		if ( animations.Count() > 0 )
		{
			ProcessSequence( pSeq, animations.Count(), animations.Base(), false );
			++nSequenceAddedCount;
		}
		else
		{
			MdlWarning( "1207: DmeSequence %s: No animations created or referenced, ignoring\n", pszSeqName );
		}

		const CUtlString &keyValues = pDmeSequenceBase->m_sKeyValues.Get();
		const int nKeyValuesLength = keyValues.Length();
		if ( nKeyValuesLength > 0 )
		{
			pSeq->KeyValue.AddMultipleToTail( nKeyValuesLength, keyValues.Get() );
		}
	}

	return nSequenceAddedCount;
}


//-----------------------------------------------------------------------------
// Purpose: Load all of the strings from the passed
//			CDmeIncludeModelList.includeModels array
//-----------------------------------------------------------------------------
static void LoadIncludeModelList( CDmeIncludeModelList *pIncludeModelList )
{
	if ( !pIncludeModelList )
		return;

	const int nIncludeModelsCount = pIncludeModelList->m_IncludeModels.Count();
	for ( int ni = 0; ni < nIncludeModelsCount; ++ni )
	{
		if ( g_numincludemodels >= ARRAYSIZE( g_includemodel ) )
		{
			MdlError( "Too Many Include Models while including: \"%s\", Max: %d\n", pIncludeModelList->m_IncludeModels[ ni ], ARRAYSIZE( g_includemodel ) );
		}
		Q_strncpy( g_includemodel[ g_numincludemodels ].name, pIncludeModelList->m_IncludeModels[ ni ], MAXSTUDIONAME );
		Q_FixSlashes( g_includemodel[ g_numincludemodels ].name, '/' );
		++g_numincludemodels;
	}
}

// return true if this QAngle is (0,0,0) within tolerance
static bool IsZero( const QAngle &qa, float tolerance = 0.01f )
{
	return (qa.x > -tolerance && qa.x < tolerance &&
		qa.y > -tolerance && qa.y < tolerance &&
		qa.z > -tolerance && qa.z < tolerance);
}


//-----------------------------------------------------------------------------
// Split the specified string into substrings separated by "_"'s and then
// sort the strings alphabetically
//-----------------------------------------------------------------------------
static void UnderscoreSplitAndSortStrings( const char *pszString, CUtlVector< CUtlString > &splitAndSortedString )
{
	splitAndSortedString.RemoveAll();

	const int nStrLen = Q_strlen( pszString );
	if ( nStrLen <= 0 )
		return;

	char *pszTmpString = reinterpret_cast< char * >( stackalloc( ( nStrLen + 1 ) * sizeof( char ) ) );
	if ( !pszTmpString )
		return;

	Q_memset( pszTmpString, 0, ( nStrLen + 1 ) * sizeof( char ) );
	Q_strncpy( pszTmpString, pszString, nStrLen + 1 );

	char *pszEnd = NULL;
	for ( char *pszName = pszTmpString; pszName && *pszName; pszName = pszEnd )
	{
		pszEnd = strchr( pszName, '_' );
		if ( !pszEnd )
		{
			splitAndSortedString.AddToTail( CUtlString( pszName ) );
		}
		else
		{
			*pszEnd = '\0';
			++pszEnd;
			splitAndSortedString.AddToTail( CUtlString( pszName ) );
		}
	}

	// Insertion sort
	for ( int i = 1; i < splitAndSortedString.Count(); ++i )
	{
		const CUtlString sValue = splitAndSortedString[i];
		int j = i - 1;
		while ( j >= 0 && Q_stricmp( splitAndSortedString[j].Get(), sValue.Get() ) > 0 )
		{
			splitAndSortedString[ j + 1 ] = splitAndSortedString[ j ];
			--j;
		}
		splitAndSortedString[j + 1] = sValue;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool GetContentsDescription( int &nContentsDescription, CDmElement *pDmElement )
{
	nContentsDescription = CONTENTS_SOLID;

	if ( !pDmElement )
		return false;

	CDmAttribute *pDmeContentsDescriptionAttr = pDmElement->GetAttribute( "contentsDescription" );
	if ( !pDmeContentsDescriptionAttr )
		return false;

	if ( pDmeContentsDescriptionAttr->GetType() != AT_STRING )
		return false;

	const char *pszContentsDesc = pDmeContentsDescriptionAttr->GetValueString();
	if ( !pszContentsDesc || Q_strlen( pszContentsDesc ) <= 0 )
		return false;

	struct ContentDesc_t
	{
		const char *m_pszName;
		int m_nContentDescriptionFlags;
	};

	ContentDesc_t contentDescs[] = {
		{ "notsolid",				CONTENTS_EMPTY },
		{ "monster",				CONTENTS_MONSTER },
		{ "ladder",					CONTENTS_LADDER },
		{ "solid",					CONTENTS_SOLID },
		{ "solid_monster",			CONTENTS_SOLID | CONTENTS_MONSTER },
		{ "solid_ladder",			CONTENTS_SOLID | CONTENTS_LADDER },
		{ "solid_monster_ladder",	CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_LADDER },
		{ "grate",					CONTENTS_GRATE },
		{ "grate_monster",			CONTENTS_GRATE | CONTENTS_MONSTER },
		{ "grate_ladder",			CONTENTS_GRATE | CONTENTS_LADDER },
		{ "grate_monster_ladder",	CONTENTS_GRATE | CONTENTS_MONSTER | CONTENTS_LADDER },
	};

	CUtlVector< CUtlString > userContentsDesc;
	UnderscoreSplitAndSortStrings( pszContentsDesc, userContentsDesc );

	CUtlVector< CUtlString > validContentsDesc;

	for ( int i = 0; i < ARRAYSIZE( contentDescs ); ++i )
	{
		UnderscoreSplitAndSortStrings( contentDescs[i].m_pszName, validContentsDesc );
		if ( userContentsDesc.Count() != validContentsDesc.Count() )
			continue;

		bool bFound = true;

		for ( int j = 0; j < userContentsDesc.Count(); ++j )
		{
			if ( Q_stricmp( userContentsDesc[j].Get(), validContentsDesc[j].Get() ) )
			{
				bFound = false;
				break;
			}
		}

		if ( bFound )
		{
			nContentsDescription = contentDescs[i].m_nContentDescriptionFlags;
			return true;
		}
	}

	MdlWarning( "2100: Invalid \"contentsDescription\" \"%s\" on \"%s\"\n",
		pszContentsDesc, pDmElement->GetName() );

	static bool bWarned = false;
	if ( !bWarned )
	{
		bWarned = true;

		CUtlString sWarn;

		const int nContentDescsCount = ARRAYSIZE( contentDescs );
		if ( nContentDescsCount > 0 )
		{
			sWarn = contentDescs[0].m_pszName;

			for ( int i = 1; i < nContentDescsCount; ++i )
			{
				sWarn += ", ";
				sWarn += contentDescs[i].m_pszName;
			}
		}

		MdlWarning( "      Valid ones are: %s\n", sWarn.Get() );
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadDefineBoneList( CDmeDefineBoneList *pDmeDefineBoneList )
{
	if ( !pDmeDefineBoneList )
		return;

	CDmeDefineBone *pDmeDefineBone = NULL;
	s_importbone_t *pImportBone = NULL;

	const int nDefineBoneCount = pDmeDefineBoneList->m_DefineBones.Count();
	for ( int ni = 0; ni < nDefineBoneCount; ++ni )
	{
		pDmeDefineBone = pDmeDefineBoneList->m_DefineBones[ ni ];
		if ( !pDmeDefineBone )
			continue;

		if ( g_numimportbones >= ARRAYSIZE( g_importbone ) )
		{
			MdlError( "Too Many Define Bones while processing: \"%s\", Max: %d\n", pDmeDefineBone->GetName(), ARRAYSIZE( g_importbone ) );
			break;
		}

		pImportBone = &g_importbone[ g_numimportbones++ ];

		Q_strncpy( pImportBone->name, pDmeDefineBone->GetName(), MAXSTUDIONAME );
		Q_strncpy( pImportBone->parent, pDmeDefineBone->m_Parent.Get(), MAXSTUDIONAME );

		AngleMatrix( pDmeDefineBone->m_Rotation.Get(), pDmeDefineBone->m_Translation.Get(), pImportBone->rawLocal );

		const Vector &rt = pDmeDefineBone->m_RealignTranslation.Get();
		const QAngle &rr = pDmeDefineBone->m_RealignRotation.Get();
		if ( !rt.IsZero() || !IsZero( rr ) )
		{
			pImportBone->bPreAligned = true;
			AngleMatrix( pDmeDefineBone->m_RealignRotation.Get(), pDmeDefineBone->m_RealignTranslation.Get(), pImportBone->srcRealign );
		}
		else
		{
			pImportBone->bPreAligned = false;
			SetIdentityMatrix( pImportBone->srcRealign );
		}

		int nContentsDescription = CONTENTS_SOLID;
		if ( GetContentsDescription( nContentsDescription, pDmeDefineBone ) )
		{
			ContentsName_t *pContentsName = NULL;

			for ( int j = 0; j < s_JointContents.Count(); ++j )
			{
				if ( !Q_stricmp( s_JointContents[j].m_pJointName, pDmeDefineBone->GetName() ) )
				{
					pContentsName = &s_JointContents[j];
					break;
				}
			}

			if ( !pContentsName )
			{
				pContentsName = &s_JointContents[ s_JointContents.AddToTail() ];
				Q_strncpy( pContentsName->m_pJointName, pDmeDefineBone->GetName(), ARRAYSIZE( pContentsName->m_pJointName ) );
			}

			pContentsName->m_nContents = nContentsDescription;
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadKeyValues( const char *pszKeyValues )
{
	if ( !pszKeyValues )
		return;

	const int nKeyValueCount = Q_strlen( pszKeyValues );
	if ( nKeyValueCount <= 0 )
		return;

	const char *pszHeader = "mdlkeyvalue\n{\n";
	const char *pszFooter = "}\n";

	g_KeyValueText.AddMultipleToTail( Q_strlen( pszHeader ), pszHeader );
	g_KeyValueText.AddMultipleToTail( nKeyValueCount, pszKeyValues );
	g_KeyValueText.AddMultipleToTail( Q_strlen( pszFooter ), pszFooter );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadGlobalFlags( CDmElement *pDmeRoot )
{
	// Get default rotation as a matrix
	matrix3x4_t mDefRot;
	AngleMatrix( g_defaultrotation, mDefRot );

	// QC $bbox
	CDmeBBox *pDmeBBox = pDmeRoot->GetValueElement< CDmeBBox >( "bbox" );
	if ( pDmeBBox )
	{
		ITransformAABB( mDefRot, pDmeBBox->m_vMinBounds, pDmeBBox->m_vMaxBounds, bbox[0], bbox[1] );
	}

	if ( pDmeRoot->HasAttribute( "opacity" ) )
	{
		switch ( pDmeRoot->GetValue< int >( "opacity" ) )
		{
		case 0:	// Auto, do nothing
			gflags &= ~STUDIOHDR_FLAGS_FORCE_OPAQUE;
			gflags &= ~STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS;
			break;
		case 1:	// opaque
			// Opaque has precedence over Mostly Opaque (TRANSLUCENT_TWOPASS)
			gflags |= STUDIOHDR_FLAGS_FORCE_OPAQUE;
			gflags &= ~STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS;
			break;
		case 2:	// mostly opaque
			// Opaque has precedence over Mostly Opaque (TRANSLUCENT_TWOPASS)
			if ( ( gflags & STUDIOHDR_FLAGS_FORCE_OPAQUE) == 0)
			{
				gflags |= STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS;
			}
			break;
		}
	}

	if ( pDmeRoot->HasAttribute( "illuminationPosition", AT_VECTOR3 ) )
	{
		VectorIRotate( pDmeRoot->GetValue< Vector >( "illuminationPosition"), mDefRot, illumposition );
	}

	if ( pDmeRoot->GetValue< bool >( "ambientBoost", false ) )
	{
		gflags |= STUDIOHDR_FLAGS_AMBIENT_BOOST;
	}

	if ( pDmeRoot->GetValue< bool >( "subdivisionSurface", false ) )
	{
		gflags |= STUDIOHDR_FLAGS_SUBDIVISION_SURFACE;
	}

	if ( pDmeRoot->GetValue< bool >( "doNotCastShadows", false ) )
	{
		gflags |= STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS;
	}

	if ( pDmeRoot->GetValue< bool >( "castTextureShadows", false ) )
	{
		gflags |= STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS;
	}

	if ( pDmeRoot->GetValue< bool >( "noForcedFade", false ) )
	{
		gflags |= STUDIOHDR_FLAGS_NO_FORCED_FADE;
	}

	int nContentsDescription = CONTENTS_SOLID;
	if ( GetContentsDescription( nContentsDescription, pDmeRoot ) )
	{
		s_nDefaultContents = nContentsDescription;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadEyeballGlobals( CDmeEyeballGlobals *pDmeEyeballGlobals )
{
	if ( !pDmeEyeballGlobals )
		return;

	g_flMaxEyeDeflection = cosf( DEG2RAD( pDmeEyeballGlobals->m_flMaxEyeDeflection.Get() ) );

	matrix3x4_t mDefRot;
	AngleMatrix( g_defaultrotation, mDefRot );
	VectorIRotate( pDmeEyeballGlobals->m_vEyePosition.Get(), mDefRot, eyeposition );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadMaterialGroups( const CDmeMaterialGroupList *pDmeMaterialGroupList )
{
	if ( !pDmeMaterialGroupList )
		return;

	if ( g_numskinref == 0 )
		g_numskinref = g_numtextures;

	// Studiomdl will read in multiple $texturegroup calls but only the first one
	// is ever used, so in mdlcompile, there will only ever be one
	Assert( g_numtexturegroups == 0 );

	CDmeMaterialGroup *pDmeMaterialGroup = NULL;

	const int nMaterialGroupCount = pDmeMaterialGroupList->m_MaterialGroups.Count();
	if ( nMaterialGroupCount >= ARRAYSIZE( g_texturegroup[ 0 ] ) )
	{
		MdlError( "Too Many Material Groups, Max: %d\n", ARRAYSIZE( g_texturegroup[ 0 ] ) );
		return;
	}

	const int nTextureCount = g_numtextures;

	for ( int nGroupIndex = 0; nGroupIndex < nMaterialGroupCount; ++nGroupIndex )
	{
		pDmeMaterialGroup = pDmeMaterialGroupList->m_MaterialGroups[ nGroupIndex ];
		if ( !pDmeMaterialGroup )
			continue;

		const int nMaterialCount = pDmeMaterialGroup->m_MaterialList.Count();
		if ( nGroupIndex > 0 && nMaterialCount < nTextureCount )
		{
			MdlWarning( "Only Setting %d of %d Textures via MaterialGroup %d, Skin %d will have bad materials\n",
				nMaterialCount, nTextureCount, nGroupIndex, nGroupIndex );
		}
		for ( int nMaterialIndex = 0; nMaterialIndex < nMaterialCount; ++nMaterialIndex )
		{
			const int nMdlMaterialIndex = UseTextureAsMaterial( LookupTexture( pDmeMaterialGroup->m_MaterialList[ nMaterialIndex ], true ) );
			g_texturegroup[ g_numtexturegroups ][ nGroupIndex ][ nMaterialIndex ] = nMdlMaterialIndex;
			if ( nGroupIndex != 0 )
			{
				g_texture[ nMdlMaterialIndex ].parent = g_texturegroup[ g_numtexturegroups ][ 0 ][ nMaterialIndex ];
			}
			g_numtexturelayers[ g_numtexturegroups ] = nGroupIndex + 1;
			g_numtexturereps[ g_numtexturegroups ] = nMaterialIndex + 1;
		}
	}

	++g_numtexturegroups;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static s_hitboxset *AllocateHitboxSet()
{
	s_hitboxset *pHitboxSet = &g_hitboxsets[ g_hitboxsets.AddToTail() ];
	memset( pHitboxSet, 0, sizeof( s_hitboxset ) );
	return pHitboxSet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static s_bbox_t *AllocateHitbox( s_hitboxset *pHitboxSet )
{
	if ( pHitboxSet->numhitboxes < ARRAYSIZE( pHitboxSet->hitbox ) )
		return &pHitboxSet->hitbox[ pHitboxSet->numhitboxes++ ];

	MdlWarning( "Too many hitboxes request for hitbox set \"%s\", max %d\n", pHitboxSet->hitboxsetname, ARRAYSIZE( pHitboxSet->hitbox ) );
	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadHitboxSetList( const CDmeHitboxSetList *pDmeHitboxSetList )
{
	if ( !pDmeHitboxSetList )
		return;

	const int nHitboxSetCount = pDmeHitboxSetList->m_HitboxSetList.Count();
	for ( int nHitBoxSetIndex = 0; nHitBoxSetIndex < nHitboxSetCount; ++nHitBoxSetIndex )
	{
		CDmeHitboxSet *pSrcHitboxSet = pDmeHitboxSetList->m_HitboxSetList[ nHitBoxSetIndex ];

		// Add a new hitboxset
		s_hitboxset *pHitboxSet = AllocateHitboxSet();
		Q_strncpy( pHitboxSet->hitboxsetname, pSrcHitboxSet->GetName(), sizeof(pHitboxSet->hitboxsetname) );

		int nHitboxCount = pSrcHitboxSet->m_HitboxList.Count();
		for ( int nHitboxIndex = 0; nHitboxIndex < nHitboxCount; ++nHitboxIndex )
		{
			CDmeHitbox *pSrcHitbox = pSrcHitboxSet->m_HitboxList[ nHitboxIndex ];

			// Find bone
			s_bonetable_t *pStudioBone = NULL;
			const int nBoneIndex = findGlobalBone( pSrcHitbox->m_sBoneName );
			if ( nBoneIndex >= 0 && nBoneIndex < g_numbones )
			{
				pStudioBone = &g_bonetable[ nBoneIndex ];
			}

			// Find bone
			for ( int nBoneIndex = 0; nBoneIndex < g_numbones; ++nBoneIndex )
			{
			}

			s_bbox_t *pHitbox = AllocateHitbox( pHitboxSet );
			Q_strncpy( pHitbox->name, pSrcHitbox->m_sBoneName, sizeof( pHitbox->name ) );
			Q_strncpy( pHitbox->hitboxname, pSrcHitbox->GetName(), sizeof( pHitbox->hitboxname ) );
			pHitbox->group = pSrcHitbox->m_nGroupId;
			pHitbox->bmin = pSrcHitbox->m_vMinBounds;
			pHitbox->bmax = pSrcHitbox->m_vMaxBounds;

			if ( !pSrcHitbox->m_sSurfaceProperty.IsEmpty() )
			{
				const char *pSurfaceProp = FindSurfaceProp( pHitbox->name );
				if ( pSurfaceProp && Q_stricmp( pSurfaceProp, pSrcHitbox->m_sSurfaceProperty ) )
				{
					MdlWarning( "Hitbox surface property \"%s\" for bone \"%s\" conflicts with existing \"%s\"",
						pSrcHitbox->m_sSurfaceProperty.Get(), pSrcHitbox->m_sBoneName.Get(), pSurfaceProp );
				}
				else
				{
					AddSurfaceProp( pHitbox->name, pSrcHitbox->m_sSurfaceProperty );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle the root.boneFlexDriverList
//-----------------------------------------------------------------------------
static void LoadBoneFlexDriverList( const CDmeBoneFlexDriverList *pDmeBoneFlexDriverList )
{
	if ( !pDmeBoneFlexDriverList )
		return;

	const CDmeBoneFlexDriverList *pDmeTmp = GetElement< CDmeBoneFlexDriverList >( g_hDmeBoneFlexDriverList );
	if ( pDmeTmp != NULL )
	{
		char szTmpBuf0[40];
		UniqueIdToString( pDmeTmp->GetId(), szTmpBuf0, sizeof( szTmpBuf0 ) );
		char szTmpBuf1[40];
		UniqueIdToString( pDmeBoneFlexDriverList->GetId(), szTmpBuf1, sizeof( szTmpBuf1 ) );

		MdlError( "DmeBoneFlexDriverList already defined (%s:%s), ignoring (%s:%s)\n",
			pDmeTmp->GetName(), szTmpBuf0,
			pDmeBoneFlexDriverList->GetName(), szTmpBuf1 );

		return;
	}

	CDmeBoneFlexDriverList *pDmeCopy = pDmeBoneFlexDriverList->Copy();
	pDmeCopy->SetFileId( DMFILEID_INVALID, TD_DEEP );

	g_hDmeBoneFlexDriverList = pDmeCopy->GetHandle();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void LoadBoneMergeList( CDmAttribute *pDmeBoneMergeListAttr )
{
	if ( !pDmeBoneMergeListAttr )
		return;

	CDmrStringArrayConst boneMergeList( pDmeBoneMergeListAttr );
	if ( !boneMergeList.IsValid() )
		return;

	const int nBoneMergeCount = boneMergeList.Count();
	for ( int nBoneMergeIndex = 0; nBoneMergeIndex < nBoneMergeCount; ++nBoneMergeIndex )
	{
		strcpyn( g_BoneMerge[ g_BoneMerge.AddToTail() ].bonename, boneMergeList[ nBoneMergeIndex ] );
	}
}


//-----------------------------------------------------------------------------
// Loads LODs from the preprocessed file
//-----------------------------------------------------------------------------
static int LodDistanceCompare( const void *elem1, const void *elem2 )
{
	const CDmeLOD *l1 = *(const CDmeLOD **)elem1;
	const CDmeLOD *l2 = *(const CDmeLOD **)elem2;
	if ( l1->m_bIsShadowLOD != l2->m_bIsShadowLOD )
		return l1->m_bIsShadowLOD ? 1 : -1;
	if ( l1->m_flSwitchMetric > l2->m_flSwitchMetric )
		return 1;
	return ( l1->m_flSwitchMetric == l2->m_flSwitchMetric ) ? 0 : -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool LoadLODs( s_source_t** ppRootLODSource, CDmeLODList *pLodList, bool &bSetUpAxis, bool bStaticProp )
{
	// NOTE: This is a little bit of a hack; it generates new s_source_ts
	// based on the LOD data, which is somewhat contrary to the existing
	// architecture, but is the easiest method of achieving the goal
	// of loading LODs from the mpp file without reloading the giant mpp
	// file over and over again.
	*ppRootLODSource = NULL;
	int nCount = pLodList->m_LODs.Count();
	if ( nCount == 0 )
		return true;

	// Sort LODs by switch distance
	CDmeLOD **ppLODs = (CDmeLOD**)stackalloc( nCount * sizeof(CDmeLOD*) );
	for ( int i = 0; i < nCount; ++i )
	{
		ppLODs[i] = pLodList->m_LODs[i];
	}
	qsort( ppLODs, nCount, sizeof( CDmeLOD* ), LodDistanceCompare );

	CDmeLOD *pRootLOD = pLodList->GetRootLOD();

	if ( pRootLOD && bSetUpAxis )
	{
		CDmeModel *pDmeModel = pRootLOD->GetValueElement< CDmeModel >( "model" );
		if ( pDmeModel )
		{
			const char *pUpAxis = pDmeModel->GetValueString( "upAxis" );
			if ( pUpAxis )
			{
				if ( StringHasPrefix( pUpAxis, "Y" ) )
				{
					// rotate 90 degrees around x to move y into z
					g_defaultrotation = RadianEuler( M_PI / 2.0f, 0.0f, M_PI / 2.0f );
				}
			}
			bSetUpAxis = false;
		}
	}

	char pSrcLODName[MAX_PATH];
	Q_StripExtension( pRootLOD->GetName(), pSrcLODName, sizeof(pSrcLODName) );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeLOD *pLOD = ppLODs[i];

		bool bIsShadowLOD = pLOD->m_bIsShadowLOD;
		if ( bIsShadowLOD )
		{
			if ( ( gflags & STUDIOHDR_FLAGS_HASSHADOWLOD ) != 0 )
			{
				MdlError( "Invalid LOD: \"%s\": Multiple Shadow LODs Defined\n", pLOD->GetName() );
				return false;
			}
			gflags |= STUDIOHDR_FLAGS_HASSHADOWLOD;
		}
		else if ( pLOD->m_flSwitchMetric.Get() < 0.0f )
		{
			MdlError( "Invalid LOD: \"%s\": Negative switch value\n", pLOD->GetName() );
			return false;
		}

		// Skip lower LODs if we're stripping them
		bool bIsRootLOD = ( pLOD == pRootLOD );
		if ( g_bStripLods && !bIsShadowLOD && ( !bIsRootLOD ) )
		{
			if ( !g_quiet )
			{
				Msg( "Stripped lod \"%s\" @ %.1f\n", pLOD->m_Model.GetElement() ? pLOD->m_Model->GetName() : "<none>", pLOD->m_flSwitchMetric );
			}
			continue;
		}

		// Now, fill her up! No model means LOD is valid, but has no geometry
		const bool bHasModel    = pLOD->m_Model.GetHandle() != DMELEMENT_HANDLE_INVALID;
		const bool bHasSkeleton = pLOD->m_Skeleton.GetHandle() != DMELEMENT_HANDLE_INVALID;

		// No model & no skeleton means LOD is invalid
		if ( !bHasModel && !bHasSkeleton )
		{
			MdlError( "Invalid LOD: \"%s\": No Model or Skeleton defined\n", pLOD->GetName() );
			return false;
		}

		if ( g_ScriptLODs.Count() == MAX_NUM_LODS )
		{
			MdlError( "Too many LODs (MAX_NUM_LODS==%d) while loading LOD: \"%s\"\n", static_cast< int >( MAX_NUM_LODS ), pLOD->GetName() );
			return false;
		}

		// Check for overflow
		if ( g_numsources >= MAXSTUDIOSEQUENCES )
		{
			MdlError( "Too many source models/animations (MAXSTUDIOSEQUENCES==%d) while loading LOD: \"%s\"\n", static_cast< int >( MAXSTUDIOSEQUENCES ), pLOD->GetName() );
			return false;
		}

		s_source_t *pLODSource = AllocateDmxSource( pLOD->m_Path.Get() );

		if ( !pLODSource )
		{
			MdlError( "Couldn't allocate new source while loading LOD: \"%s\"\n", pLOD->GetName() );
			return false;
		}

		if ( bIsRootLOD )
		{
			*ppRootLODSource = pLODSource;
			pLODSource->isActiveModel = true;
		}

		if ( bHasModel )
		{
			BoneTransformMap_t boneMap;
			if ( !LoadModelAndSkeleton( pLODSource, boneMap, pLOD->m_Skeleton, pLOD->m_Model, pLOD->m_CombinationOperator, bStaticProp ) )
			{
				MdlError( "Couldn't load skeleton and model while loading LOD: \"%s\"\n", pLOD->GetName() );
				return false;
			}

			LoadQcModelElements( pLODSource, g_pCurrentModel, pLOD->m_Model );
		}

		if ( bIsRootLOD )
			continue;

		// Create LOD information in terms of how the old system does it
		// Shadow lod reserves -1 as switch value which uniquely identifies a shadow lod
		int j = g_ScriptLODs.AddToTail();
		LodScriptData_t& lod = g_ScriptLODs[j];
		lod.switchValue = bIsShadowLOD ? -1.0f : pLOD->m_flSwitchMetric;
		lod.EnableFacialAnimation( bIsShadowLOD ? false : !pLOD->m_bNoFlex );
		lod.StripFromModel( false );

		// We only support simple model replacement here. Other processing
		// is expected to have occurred in the preprocessing phase
		j = lod.modelReplacements.AddToTail();
		CLodScriptReplacement_t& replacement = lod.modelReplacements[j];
		replacement.SetSrcName( pSrcLODName );
		replacement.SetDstName( pLODSource->filename );
		replacement.m_pSource = pLODSource;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
extern int Option_Blank();


//-----------------------------------------------------------------------------
// Loads Eyeballs
//-----------------------------------------------------------------------------
static void LoadEyeballs( s_model_t *ps_model_t, CDmeLODList *pDmeLODList )
{
	if ( !pDmeLODList )
		return;

	matrix3x4_t mDefRot;
	AngleMatrix( g_defaultrotation, mDefRot );
	Vector vTmp;

	const int nEyeballCount = pDmeLODList->m_EyeballList.Count();
	if ( nEyeballCount <= 0 )
		return;

	for ( int nEyeballIndex = 0; nEyeballIndex < nEyeballCount; ++nEyeballIndex )
	{
		CDmeEyeball *pDmeEyeball = pDmeLODList->m_EyeballList.Get( nEyeballIndex );
		if ( !pDmeEyeball )
			continue;

		if ( ps_model_t->numeyeballs >= ARRAYSIZE( ps_model_t->eyeball ) )
		{
			MdlWarning( "1100: Max number of eyeballs reached for model %s, ignoring eyeball %s\n", ps_model_t->name, pDmeEyeball->GetName() );
			continue;
		}

		int nFoundBoneIndex = -1;
		for ( int nSearchBoneIndex = 0; nSearchBoneIndex < ps_model_t->source->numbones; ++nSearchBoneIndex )
		{
			if ( !Q_stricmp( ps_model_t->source->localBone[ nSearchBoneIndex ].name, pDmeEyeball->m_sParentBoneName.Get() ) )
			{
				nFoundBoneIndex = nSearchBoneIndex;
				break;
			}
		}

		if ( nFoundBoneIndex < 0 )
		{
			MdlWarning( "1101: Couldn't find bone %s on model %s, ignoring eyeball %s\n", pDmeEyeball->m_sParentBoneName.Get(), ps_model_t->name, pDmeEyeball->GetName() );
			continue;
		}

		const char *pszMaterialName = pDmeEyeball->m_sMaterialName.Get();
		const bool bRelative = ( strchr( pszMaterialName, '/' ) || strchr( pszMaterialName, '\\' ) ) ? true : false;

		const int nSearchMeshMatIndex = UseTextureAsMaterial( LookupTexture( pDmeEyeball->m_sMaterialName.Get(), bRelative ) );
		int nFoundMeshMatIndex = -1;
		for ( int i = 0; i < ps_model_t->source->nummeshes; ++i )
		{
			const int nTmpMeshMatIndex = ps_model_t->source->meshindex[ i ]; // meshes are internally stored by material index

			if ( nTmpMeshMatIndex == nSearchMeshMatIndex )
			{
				nFoundMeshMatIndex = i;
				break;
			}
		}

		if ( nFoundMeshMatIndex < 0 )
		{
			MdlWarning( "1102: Couldn't find eyeball material %s on model %s, ignoring eyeball %s\n", pDmeEyeball->m_sMaterialName.Get(), ps_model_t->name, pDmeEyeball->GetName() );
			continue;
		}

		s_eyeball_t *ps_eyeball_t = &( ps_model_t->eyeball[ ps_model_t->numeyeballs++ ] );
		Q_strncpy( ps_eyeball_t->name, pDmeEyeball->GetName(), sizeof( ps_eyeball_t->name ) );
		ps_eyeball_t->bone = nFoundBoneIndex;
		ps_eyeball_t->mesh = nFoundMeshMatIndex;
		ps_eyeball_t->radius = pDmeEyeball->m_flRadius.Get();
		ps_eyeball_t->zoffset = tan( DEG2RAD( pDmeEyeball->m_flYawAngle.Get() ) );
		ps_eyeball_t->iris_scale = 1.0f / pDmeEyeball->m_flIrisScale;

		// translate eyeball into bone space
		VectorITransform( pDmeEyeball->m_vPosition.Get(), ps_model_t->source->boneToPose[ ps_eyeball_t->bone ], ps_eyeball_t->org );

		VectorIRotate( Vector( 0, 0, 1 ), mDefRot, vTmp );
		VectorIRotate( vTmp, ps_model_t->source->boneToPose[ ps_eyeball_t->bone ], ps_eyeball_t->up );

		VectorIRotate( Vector( 1, 0, 0 ), mDefRot, vTmp );
		VectorIRotate( vTmp, ps_model_t->source->boneToPose[ ps_eyeball_t->bone ], ps_eyeball_t->forward );

		// Not applicable
		ps_eyeball_t->upperlidflexdesc = -1;
		ps_eyeball_t->lowerlidflexdesc = -1;
	}

	// Make the standard flex controllers for eyes if required
	static const char *szEyesFlexControllers[] = {
		"eyes_updown",
		"eyes_rightleft"
	};

	for ( int nNewFlexIndex = 0; nNewFlexIndex < ARRAYSIZE( szEyesFlexControllers ); ++nNewFlexIndex )
	{
		bool bHasEyeFlexController = false;

		for ( int nFlexIndex = 0; nFlexIndex < g_numflexcontrollers; ++nFlexIndex )
		{
			if ( !Q_strcmp( szEyesFlexControllers[ nNewFlexIndex ], g_flexcontroller[ nFlexIndex ].name ) )
			{
				bHasEyeFlexController = true;
				break;
			}
		}

		// The flex controller range for eyes_updown & eyes_rightleft is default [-45, 45] because it's clamped by eyesMaxDeflection
		// and changing it based on max deflection would cause animatin changes since flex controller values are normalized [0, 1]
		// [-45, 45 ] gives a maxium range that's useful

		if ( !bHasEyeFlexController )
		{
			if ( g_numflexcontrollers >= MAXSTUDIOFLEXCTRL )
			{
				MdlWarning( "1103: Couldn't make eyes flexcontroller %s, too many flex controllers defined\n", szEyesFlexControllers[ nNewFlexIndex ] );
				continue;
			}

			Q_strncpy( g_flexcontroller[g_numflexcontrollers].name, szEyesFlexControllers[ nNewFlexIndex ], sizeof( g_flexcontroller[ g_numflexcontrollers ].name ) );
			Q_strncpy( g_flexcontroller[g_numflexcontrollers].type, "eyes", sizeof( g_flexcontroller[ g_numflexcontrollers ].name ) );
			g_flexcontroller[g_numflexcontrollers].min = -45.0f;
			g_flexcontroller[g_numflexcontrollers].max = 45.0f;
			++g_numflexcontrollers;
		}
	}
}


//-----------------------------------------------------------------------------
// Loads Body Group Lists from the preprocessed file
//-----------------------------------------------------------------------------
static bool LoadBodyGroupList( s_source_t **ppMainSource, CDmeBodyGroupList *pBodyGroupList, CDmeEyeballGlobals *pDmeEyeballGlobals, bool bStaticProp, bool &bSetUpAxis )
{
	*ppMainSource = NULL;

	CDmaElementArray< CDmeBodyGroup > &dmeBodyGroups = pBodyGroupList->m_BodyGroups;
	const int nDmeBodyGroupCount = dmeBodyGroups.Count();
	if ( nDmeBodyGroupCount == 0 )
		return true;

	// Eyeball globals needs to be loaded after g_defaultrotation is set, which is after the first LOD is loaded
	bool bLoadEyeballGlobals = true;

	// Compute the 'main' body part
	const CDmeLODList *pMainBodyPart = pBodyGroupList->GetMainBodyPart();
	for ( int i = 0; i < nDmeBodyGroupCount; ++i )
	{
		const CDmeBodyGroup *pDmeBodyGroup = dmeBodyGroups[i];

		s_bodypart_t *pBodyPart = &g_bodypart[ g_numbodyparts ];
		pBodyPart->nummodels = 0;
		if ( g_numbodyparts == 0 )
		{
			pBodyPart->base = 1;
		}
		else
		{
			pBodyPart->base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
		}
		Q_strncpy( pBodyPart->name, pDmeBodyGroup->GetName(), sizeof( pBodyPart->name ) );
		++g_numbodyparts;

		// Load all body parts for this body group
		const int nDmeBodyPartCount = pDmeBodyGroup->m_BodyParts.Count();
		for ( int j = 0; j < nDmeBodyPartCount; ++j )
		{
			s_model_t *pModel = (s_model_t *)calloc( 1, sizeof( s_model_t ) );
			const int nModel = g_nummodels++;
			g_model[nModel] = pModel;
			pBodyPart->pmodel[ pBodyPart->nummodels++ ] = pModel;
			pModel->scale = 1.0f;

			CDmeBodyPart *pDmeBodyPart = pDmeBodyGroup->m_BodyParts[j];
			CDmeLODList *pDmeLODList = CastElement< CDmeLODList >( pDmeBodyPart );

			if ( pDmeBodyPart->LODCount() == 0 || !pDmeLODList )
			{
				pModel->source = AllocateDmxSource( "blank" );
				Q_strncpy( pModel->name, "blank", sizeof(pModel->name) );
			}
			else
			{
				Q_strncpy( pModel->name, pDmeLODList->GetName(), sizeof(pModel->name) );

				g_pCurrentModel = pModel;

				if ( !LoadLODs( &pModel->source, pDmeLODList, bSetUpAxis, bStaticProp ) )
				{
					MdlError( "Bad LOD On BodyGroup \"%s\".bodyPartList[%d]\n", pDmeBodyGroup->GetName(), j );
					return false;
				}

				g_pCurrentModel = NULL;

				// This needs to be done after g_defaultRotation is set, which should be set after the first LOD is loaded
				if ( bLoadEyeballGlobals )
				{
					bLoadEyeballGlobals = false;
					LoadEyeballGlobals( pDmeEyeballGlobals );
				}

				LoadEyeballs( pModel, pDmeLODList );

				if ( pModel->source )
				{
					Q_strncpy( pModel->filename, pModel->source->filename, sizeof(pModel->filename) );
				}

				PostProcessSource( pModel->source, nModel );

				if ( pMainBodyPart == pDmeLODList )
				{
					*ppMainSource = pModel->source;
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the collision model from the preprocessed file
//-----------------------------------------------------------------------------
bool LoadCollisionModel( CDmeCollisionModel *pCollisionInfo, bool bStaticProp )
{
	if ( !pCollisionInfo )
		return false;

	// Check for overflow - TODO: Move to AllocateSource()
	if ( g_numsources >= MAXSTUDIOSEQUENCES )
	{
		MdlError( "Load_Source - overflowed g_numsources loading LODs." );
		return false;
	}

	s_source_t *pCollisionSource = AllocateDmxSource( pCollisionInfo->GetName() );
	if ( !pCollisionSource )
		return false;

	int nummaterials = g_nummaterials;
	int numtextures = g_numtextures;

	// BoneRemap[bone index in file] == bone index in studiomdl
	CDmeDag *pSkeleton = pCollisionInfo->GetValueElement< CDmeDag >( "skeleton" );
	if ( !pSkeleton )
	{
		MdlError( "%s(%s): No \"skeleton\" defined\n", pCollisionInfo->GetTypeString(), pCollisionInfo->GetName() );
		return false;
	}

	CDmeModel *pModel = pCollisionInfo->GetValueElement< CDmeModel >( "model" );
	if ( !pModel )
	{
		MdlError( "%s(%s): No \"model\" defined\n", pCollisionInfo->GetTypeString(), pCollisionInfo->GetName() );
		return false;
	}

	BoneTransformMap_t boneMap;
	if ( !LoadModelAndSkeleton( pCollisionSource, boneMap, pSkeleton, pModel, NULL, bStaticProp ) )
	{
		MdlError( "%s(%s): Couldn't Load Skeleton: %s(%s) & Model: %s(%s)\n",
			pCollisionInfo->GetTypeString(), pCollisionInfo->GetName(),
			pSkeleton->GetTypeString(), pSkeleton->GetName(),
			pModel->GetTypeString(), pModel->GetName() );
		return false;
	}

	// auto-remove any new materials/textures
	if ( nummaterials && numtextures && ( numtextures != g_numtextures || nummaterials != g_nummaterials ) )
	{
		g_numtextures = numtextures;
		g_nummaterials = nummaterials;
		pCollisionSource->texmap[0] = 0;
	}

	if ( DoCollisionModel( pCollisionSource, pCollisionInfo, bStaticProp ) == 0 )
		return false;

	const char *pSurfaceProperty = pCollisionInfo->GetValueString( "surfaceProperty" );
	if ( pSurfaceProperty && pSurfaceProperty[0] )
	{
		SetDefaultSurfaceProp( pSurfaceProperty );
	}

	return true;
}

#endif // MDLCOMPILE

//-----------------------------------------------------------------------------
// Sets up the DMX if it was a static prop
//-----------------------------------------------------------------------------
static void SetupStaticProp( s_source_t *pSource )
{
#ifdef MDLCOMPILE
	ProcessStaticProp();
	s_sequence_t *pSeq = ProcessCmdSequence( "BindPose" );
	s_animation_t *pAnim = ProcessImpliedAnimation( pSeq, pSource->filename );
	pSeq->panim[0][0] = pAnim;
	ProcessSequence( pSeq, 1, &pAnim, false );
#endif
}


//-----------------------------------------------------------------------------
// Main entry point for loading DMX files
//-----------------------------------------------------------------------------
int Load_DMX( s_source_t *pSource )
{
	DmFileId_t fileId;

	// use the full search tree, including mod hierarchy to find the file
	char pFullPath[MAX_PATH];
	if ( !GetGlobalFilePath( pSource->filename, pFullPath, sizeof(pFullPath) ) )
		return 0;

	// When reading, keep the CRLF; this will make ReadFile read it in binary format
	// and also append a couple 0s to the end of the buffer.
	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pFullPath, NULL, NULL, &pRoot ) == DMFILEID_INVALID )
		return 0;

	if ( !g_quiet )
	{
		Msg( "DMX Model %s\n", pFullPath );
	}

	// Load model info
	LoadModelInfo( pRoot, pFullPath );

	// Load constraints
	LoadConstraints( pRoot );

	// Extract out the skeleton
	// BoneRemap[bone index in file] == bone index in studiomdl
	CDmeDag *pSkeleton = pRoot->GetValueElement< CDmeDag >( "skeleton" );
	CDmeModel *pModel = pRoot->GetValueElement< CDmeModel >( "model" );
	CDmeCombinationOperator *pCombinationOperator = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	BoneTransformMap_t boneMap;
	if ( !LoadModelAndSkeleton( pSource, boneMap, pSkeleton, pModel, pCombinationOperator, false ) )
		goto dmxError;

	LoadQcModelElements( pSource, g_pCurrentModel, pModel );

	CDmeAnimationList *pAnimationList = pRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	if ( pAnimationList )
	{
		LoadAnimations( pSource, pAnimationList, g_currentscale, boneMap );
	}

	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return 1;

dmxError:
	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return 0;
}


//-----------------------------------------------------------------------------
// Main entry point for loading FBX files
//-----------------------------------------------------------------------------
int Load_FBX( s_source_t *pSource )
{
	// use the full search tree, including mod hierarchy to find the file
	char pFullPath[ MAX_PATH ];
	if ( !GetGlobalFilePath( pSource->filename, pFullPath, sizeof( pFullPath ) ) )
		return 0;
	
	CDmFbxSerializer dmFbxSerializer;
	dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_Y;
	dmFbxSerializer.m_eOptForwardParity = CDmeAxisSystem::AS_PARITY_ODD;
	CDmElement *pRoot = dmFbxSerializer.ReadFBX( pFullPath );
	if ( !pRoot )
		return 0;

	if ( !g_quiet )
	{
		Msg( "FBX Model %s\n", pFullPath );
	}
		
	// Load model info
	LoadModelInfo( pRoot, pFullPath );

	// Load constraints
	LoadConstraints( pRoot );

	// Extract out the skeleton
	// BoneRemap[bone index in file] == bone index in studiomdl
	CDmeDag *pSkeleton = pRoot->GetValueElement< CDmeDag >( "skeleton" );
	CDmeModel *pModel = pRoot->GetValueElement< CDmeModel >( "model" );
	CDmeCombinationOperator *pCombinationOperator = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	BoneTransformMap_t boneMap;
	int nReturn = 0;
	if ( LoadModelAndSkeleton( pSource, boneMap, pSkeleton, pModel, pCombinationOperator, false ) )
	{
		LoadQcModelElements( pSource, g_pCurrentModel, pModel );

		CDmeAnimationList *pAnimationList = pRoot->GetValueElement< CDmeAnimationList >( "animationList" );
		if ( pAnimationList )
		{
			LoadAnimations( pSource, pAnimationList, g_currentscale, boneMap );
		}

		if ( CommandLine()->FindParm( "-debugfbx2dmx" ) )
			g_pDataModel->SaveToFile( CUtlString( pFullPath ).StripExtension() + ".fbx2dmx.dmx", NULL, "keyvalues2", "model", pRoot );
		nReturn = 1; // loaded ok
	}
	g_pDataModel->UnloadFile/*RemoveFileId?*/( pRoot->GetFileId() );

	return nReturn;
}

//-----------------------------------------------------------------------------
// Declare it so we can call it, defined in studiomdl.cpp
//-----------------------------------------------------------------------------
extern void ProcessModelName( const char *pMdlName );


//-----------------------------------------------------------------------------
// Main entry point for loading preprocessed files
//-----------------------------------------------------------------------------
bool LoadPreprocessedFile( const char *pFileName, float flScale )
{
#ifndef MDLCOMPILE
	return false;
#else
	DmFileId_t fileId;

	// use the full search tree, including mod hierarchy to find the file
	char pFullPath[MAX_PATH];
	if ( !GetGlobalFilePath( pFileName, pFullPath, sizeof(pFullPath) ) )
	{
		MdlError( "Invalid MPP Filename: %s\n", pFileName );
		return false;
	}

	// When reading, keep the CRLF; this will make ReadFile read it in binary format
	// and also append a couple 0s to the end of the buffer.
	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pFullPath, NULL, NULL, &pRoot ) == DMFILEID_INVALID )
	{
		MdlError( "0001: Couldn't Load MPP File: %s\n", pFullPath );
		return false;
	}

	if ( !g_quiet )
	{
		Msg( "Loaded Preprocessed File %s\n", pFullPath );
	}

	const char *pMdlPath = pRoot->GetValueString( "mdlPath" );
	if ( pMdlPath && *pMdlPath )
	{
		// This is a hack... really the model path should be able to be anywhere relative to the game
		// directory but currently "models/" is prepended everywhere... would like to get rid of that
		// pattern but would also like to limit changes required for mdlcompile
		if ( !Q_strnicmp( pMdlPath, "models", 6 ) && pMdlPath[6] == '/' || pMdlPath[7] == '\\' )
		{
			ProcessModelName( pMdlPath + 7 );
		}
		else
		{
			ProcessModelName( pMdlPath );
		}
	}

	// Get whether we're doing skinned LODs from the pre-process file
	g_bSkinnedLODs = pRoot->GetValue< bool >( "skinnedLODs", false );

	// Load model info
	LoadModelInfo( pRoot, pFullPath );

	// Find out if it's marked as a static prop
	const bool bStaticProp = pRoot->GetValue< bool >( "staticProp" );

	bool bSetUpAxis = true;
	s_source_t *pMainSource = NULL;

	CDmeBodyGroupList *pBodyGroupList = pRoot->GetValueElement< CDmeBodyGroupList >( "bodyGroupList" );
	if ( pBodyGroupList )
	{
		// Loads all body groups
		if ( !LoadBodyGroupList( &pMainSource, pBodyGroupList, pRoot->GetValueElement< CDmeEyeballGlobals >( "eyeballGlobals" ), bStaticProp, bSetUpAxis ) )
			goto dmxError;

		CDmeCollisionModel *pCollisionModel = pRoot->GetValueElement< CDmeCollisionModel >( "collisionModel" );
		if ( pCollisionModel && !LoadCollisionModel( pCollisionModel, bStaticProp ) )
			goto dmxError;

		LoadCollisionText( pRoot->GetValueString( "collisionText" ) );

		// Deal with material groups.  Ok to pass NULL for CDmeMaterialGroupList
		LoadMaterialGroups( pRoot->GetValueElement< CDmeMaterialGroupList >( "materialGroupList" ) );

		// Deal with bone merge directives.  Ok to pass NULL DmAttribute for string array
		LoadBoneMergeList( pBodyGroupList->GetAttribute( "boneMergeList" ) );

		// Deal with bone merge directives.  Ok to pass NULL DmAttribute for string array
		// TODO: Bone keep list is a little funny, right now just treat the same as bone
		//       merge list which will ensure the bone is always present.  This will also
		//       increase the bone priority and make the bone available to the server
		LoadBoneMergeList( pBodyGroupList->GetAttribute( "boneKeepList" ) );
	}
	else
	{
		if ( bStaticProp )
		{
			MdlError( "0002: Static prop specified but no body groups present\n" );
			goto dmxError;
		}

		// This MPP has no body groups, so maybe it's an animation only MPP?
		pMainSource = AllocateDmxSource( "anim" );
	}

	// Deal with static props
	if ( bStaticProp && pBodyGroupList )
	{
		// FIXME: This source should come from the skeleton;
		// need to figure out if static props can deal with multiple sources
		SetupStaticProp( pMainSource );
		goto dmxSuccess;
	}

	// Nothing after here is applied if this is a static prop
	LoadBoneMaskList( pRoot->GetValueElement< CDmeBoneMaskList >( "boneMaskList" ) );

	// Deal with pose parameters.  Ok to pass NULL for CDmePoseParameterList
	LoadPoseParameterList( pRoot->GetValueElement< CDmePoseParameterList >( "poseParameterList" ) );

	// Deal with animBlockSize, Ok to pass NULL for CDmeAnimBlockSize
	LoadAnimBlockSize( pRoot->GetValueElement< CDmeAnimBlockSize >( "animBlockSize" ) );

	// Deal with sequences.  Ok to pass NULL for CDmeSequenceList
	const int nSequenceCount = LoadAndCreateSequences( pMainSource, pRoot->GetValueElement< CDmeSequenceList >( "sequenceList" ), bSetUpAxis );
	if ( nSequenceCount == 0 && !pBodyGroupList )
	{
		MdlError( "0003: MPP has no body groups and no animations\n" );
		goto dmxError;
	}

	// Deal with include models.  Ok to pass NULL for CDmeIncludeModelList
	LoadIncludeModelList( pRoot->GetValueElement< CDmeIncludeModelList >( "includeModelList" ) );

	// Deal with define bones.  Ok to pass NULL for CDmeDefineBoneList
	LoadDefineBoneList( pRoot->GetValueElement< CDmeDefineBoneList >( "defineBoneList" ) );

	// Deal with hitbox sets.  Ok to pass NULL for CDmeHitboxSetList
	LoadHitboxSetList( pRoot->GetValueElement< CDmeHitboxSetList >( "hitboxSetList" ) );

	// Deal with boneFlexDriver
	LoadBoneFlexDriverList( pRoot->GetValueElement< CDmeBoneFlexDriverList >( "boneFlexDriverList" ) );

	// At this point, reorienting of skeleton defined stuff will likely be required

	// Deal with KeyValues, Ok to pass NULL or empty string
	LoadKeyValues( pRoot->GetValueString( "keyValues" ) );

	LoadGlobalFlags( pRoot );

dmxSuccess:
	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return true;

dmxError:
	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return false;
#endif   // MDLCOMPILE
}

//========== Copyright (c) Valve Corporation. All Rights Reserved. ============
#include "mdlobjects/authphysfx.h"
#include "resourcefile/resourcestream.h"
#include "mathlib/femodeldesc.h"
#include "rubikon/param_types.h"
#include "tier1/utlstringtoken.h"
#include "tier1/keyvalues.h"
#include "tier2/fileutils.h"
#include "tier2/p4helpers.h"
#include "mathlib/disjoint_set_forest.h"
//#include "mdlobjects/physmodelsource.h"
#include "meshutils/mesh.h"
// #include "meshutils/meshdisjointsetpartition.h"
// #include "meshutils/meshconvexitydetector.h"
// #include "mdlobjects/vpropbreakablelist.h"
#include "mdlobjects/physmodelsource.h"
#include "mathlib/femodelbuilder.h"
#include "tier1/heapsort.h"
#include "tier1/fmtstr.h"
#include "mathlib/disjoint_set_forest.h"

#include "datamodel/dmelement.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/idatamodel.h"

#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmejoint.h"
#include "movieobjects/dmemodel.h"
#include "mdlobjects/clothproxymesh.h"


#include "dmeutils/dmmeshutils.h"


// #ifdef _DEBUG
// #define TNT_BOUNDS_CHECK
// #else
// //#define TNT_UNROLL_LOOPS
// #endif
// 
// #include "tnt.h"
// #include "tnt_linalg.h"

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_AUTH_PHYS, "AuthPhys" );

const char *g_pTokenSeparators[] = { " ", "\t", "\n", "\r", ",", ";", "|" };



//-----------------------------------------------------------------------------
static CDmeModel* LoadModelFromDMX( const char* pDMXFile )
{
	CDmElement* pRoot;
	if ( g_pDataModel->RestoreFromFile( pDMXFile, "CONTENT", NULL, &pRoot ) == DMFILEID_INVALID )
	{
		return false;
	}

	// If this isn't a DME Model
	CDmeModel* pModel = NULL;
	if ( pRoot->IsA< CDmeModel >() )
	{
		pModel = dynamic_cast<CDmeModel*>( pRoot );
	}

	// Try to find a CDmeModel
	if ( !pModel )
	{
		pModel = pRoot->GetValueElement< CDmeModel >( "model" );
	}

	if ( !pModel )
	{
		pModel = pRoot->GetValueElement< CDmeModel >( "skeleton" );
	}

	return pModel;
}



//-----------------------------------------------------------------------------
int CompareCaseInsensitive( const CUtlString *pLeft, const CUtlString *pRight )
{
	return V_stricmp( pLeft->Get(), pRight->Get() );
}


//-----------------------------------------------------------------------------
inline const Vector GetAxisX( const Quaternion &q )
{
	return q.GetForward( );
}


int CAuthPhysFx::FindNodeIndex( const char *pName )
{
	for ( int i = 0; i < m_Nodes.Count(); ++i )
	{
		if ( !V_stricmp( pName, m_Nodes[ i ].m_Name.Get() ) )
			return i;
	}
	return -1;
}


CAuthPhysFx::CBone *CAuthPhysFx::GetOrCreateBone( const char *pName )
{
	int nNode = FindNodeIndex( pName );
	if ( nNode >= 0 )
	{
		return &m_Nodes[ nNode ];
	}

	CBone *pNewBone = &m_Nodes[ m_Nodes.AddToTail() ];
	pNewBone->m_Name = pName;
	return pNewBone;
}





int DescSort( const int* a, const int* b )
{
	return *b - *a;
}


Vector Sqr( const Vector &v )
{
	return Vector( v.x * v.x, v.y * v.y, v.z * v.z );
}

Vector SafeSqrt( const Vector &v )
{
	return Vector( sqrtf( MAX( 0, v.x ) ), sqrtf( MAX( 0, v.y ) ), sqrtf( MAX( 0, v.z ) ) );
}


void CAuthPhysFx::AddRod( const CUtlVector< CBone > &nodes, uint nNode0, uint nNode1, float flRelaxationFactor )
{
	if ( nodes[ nNode0 ].m_bSimulated || nodes[ nNode1 ].m_bSimulated )
	{
		CRod &rod = m_Rods[ m_Rods.AddToTail( ) ];
		rod.m_nNodes[ 0 ] = nNode0;
		rod.m_nNodes[ 1 ] = nNode1;
		rod.m_flRelaxationFactor = flRelaxationFactor;
	}
}


static float GetClothFloat( KeyValues *pKeyValues, const char *keyName = NULL, float defaultValue = 0.0f )
{
	if ( keyName )
	{
		CUtlString altKeyName( "s2:" );
		altKeyName += keyName;
		if ( KeyValues *pKey = pKeyValues->FindKey( altKeyName ) )
		{
			return pKey->GetFloat( );
		}
	}
	return pKeyValues->GetFloat( keyName, defaultValue );
};


static bool FindKey( KeyValues *pParent, const char *pSubkey, float *pFloatOut )
{
	if ( KeyValues *pChild = pParent->FindKey( pSubkey ) )
	{
		*pFloatOut = pChild->GetFloat( ( const char * )NULL, *pFloatOut );
		return true;
	}
	return false;

}
 



static int GetClothInt( KeyValues *pKeyValues, const char *keyName = NULL, int defaultValue = 0 )
{
	if ( keyName )
	{
		CUtlString altKeyName( "s2:" );
		altKeyName += keyName;
		if ( KeyValues *pKey = pKeyValues->FindKey( altKeyName ) )
		{
			return pKey->GetInt( );
		}
	}
	return pKeyValues->GetInt( keyName, defaultValue );
};



static const char *GetClothString( KeyValues *pKeyValues, const char *keyName = NULL, const char *defaultValue = "" )
{
	if ( keyName )
	{
		CUtlString altKeyName( "s2:" );
		altKeyName += keyName;
		if ( KeyValues *pKey = pKeyValues->FindKey( altKeyName ) )
		{
			return pKey->GetString( );
		}
	}
	return pKeyValues->GetString( keyName, defaultValue );
};








void CreateGridNodeBases( const CNodeIdx &nodeIdx, int nRows, int nColumns, const CUtlVector< CAuthPhysFx::CBone > &nodes, CUtlVector< FeNodeBase_t > &nodeBases )
{
	//float sin45 = sqrtf( 0.5f );
	//const Quaternion qAdjust = Quaternion( 0, sin45, 0, sin45 ) * Quaternion( 0, 0, sin45, sin45 );
	const Quaternion qAdjust = Quaternion( .5, .5, .5, .5 ) * Quaternion( 1, 0, 0, 0 );
	matrix3x4a_t tmTest;
	QuaternionMatrix( qAdjust, tmTest );

	for ( int nRow = 0; nRow < nRows; ++nRow )
	{
		for ( int nColumn = 0; nColumn < nColumns; ++nColumn )
		{
			uint nNode = nodeIdx( nRow, nColumn );
			if ( nodes[ nNode ].m_bVirtual || !nodes[ nNode ].m_bSimulated )
			{
				continue; // don't care about virtual or static nodes
			}
			int nUp = Max( 0, nRow - 1 ), nDown = Min( nRows - 1, nRow + 1 );
			int nLeft = Max( 0, nColumn - 1 ), nRight = Min( nColumns - 1, nColumn + 1 );
			FeNodeBase_t &nb = nodeBases[ nodeBases.AddToTail( ) ];
			V_memset( &nb, 0, sizeof( nb ) );
			nb.nNode   = nNode;
			nb.nNodeX0 = nodeIdx( nRow, nLeft );
			nb.nNodeX1 = nodeIdx( nRow, nRight );
			nb.nNodeY0 = nodeIdx( nUp, nColumn );
			nb.nNodeY1 = nodeIdx( nDown, nColumn );

			// nodeX -> right     -> particle Z; this direction is gram-schmidt-orthogonalized (in Source1 using double-cross-product, in Source2 I just GS-orthogonalize it directly)
			// nodeY -> Down(-Up) -> particle X; this direction is unchanged
			nb.qAdjust = qAdjust;
		}
	}
}




void CAuthPhysFx::Load( const CFeModel *pFeModel )
{
	
#ifdef _DEBUG // recreate the CFeModel
	CResourceStreamVM stream;
	const PhysFeModelDesc_t *pFeDesc = Compile( &stream );
	CFeModel *pFeModel2 = stream.Allocate< CFeModel >(); 
	Clone( pFeDesc, 0, stream.Allocate< char * >( pFeModel2->m_nCtrlCount ), pFeModel2 );
	NOTE_UNUSED( pFeModel2 );
#endif
}


bool CAuthPhysFx::ImportDotaCloth( const char *pFileName, CPhysModelSource &physicsModel/*CUtlStringMap< int, CUtlSymbolTable > *pBoneToIndex*/ )
{
	const char *pContentPath = V_IsAbsolutePath( pFileName ) ? NULL : "CONTENT";
	if ( !g_pFullFileSystem->FileExists( pFileName, pContentPath ) )
	{
		return false;
	}
	
	KeyValues *kv = new KeyValues( "Cloth" );
	if ( !kv->LoadFromFile( g_pFullFileSystem, pFileName, pContentPath ) )
	{
		Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse %s\n", pFileName );
		kv->deleteThis();
		return false;
	}

	CAuthClothParser parser;
	parser.SetBones( physicsModel );

	bool bParsedOk = parser.Parse( kv );

	kv->deleteThis();

	if ( bParsedOk )
	{
		Swap( parser );
	}
	m_Constraints.Purge( );
	m_Capsules.Purge( );
	m_bFollowTheLead = true; // this is a bad default, necessary for compatibility with old Dota2 Source1 cloth

	for ( uint n = 0; n < ( uint )m_Nodes.Count(); ++n )
	{
		if ( m_Nodes[ n ].m_flLocalForce != m_flLocalForce || m_Nodes[ n ].m_flLocalRotation != m_flLocalRotation )
		{
			m_bUsePerNodeLocalForceAndRotation = true; // Dota2 Source1 uses different local rotation and force per cloth piece
			break;
		}
	}

	return true;
}


void CAuthClothParser::SetBones( CPhysModelSource &physicsModel )
{
	m_BoneToParent.SetCount( physicsModel.GetBoneCount( ) );

	for ( int nBone = 0; nBone < physicsModel.GetBoneCount( ); ++nBone )
	{
		m_BoneToIndex.Insert( physicsModel.GetBoneNameByIndex( nBone ), nBone );
		m_BoneToParent[ nBone ] = physicsModel.GetParentJoint( nBone );
	}

	m_ModelBoneToNode.SetCount( physicsModel.GetBoneCount( ) );
	m_ModelBoneToNode.FillWithValue( -1 );

	// we'll use the "bindpose" or similar animation, first frame, if we can
	/*
	const char * idleAnimations[] = { "bindpose", "idle", "run" };
	for ( int i = 0; i < ARRAYSIZE( idleAnimations ); ++i )
	{
		if ( physicsModel.GetAnimFrame( idleAnimations[ i ], 0, &transforms ) )
		{
			break; // we'll use this
		}
	}
	
	if( m_BoneTransforms.Count() < physicsModel.GetBoneCount() )
	{
		Log_Msg( LOG_AUTH_PHYS, FUNCTION_LINE_STRING "  m_BoneTransforms.Count() %d < physicsModel.GetBoneCount() %d\n", m_BoneTransforms.Count(), physicsModel.GetBoneCount() );
		Log_Msg( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Could not find bindpose animation, using actual bind pose which dota s1 does NOT use when building cloth. The resulting cloth will be different from Dota Source1 cloth\n" );
		// couldn't get the first frame of one of ethalon poses
		physicsModel.GetBindPoseWorldTransforms( m_BoneTransforms );
		AdjustLegacyDotaOrientation( m_BoneTransforms );
	} */

	physicsModel.GetBindPoseWorldTransforms( m_BoneTransforms );
	AdjustLegacyDotaOrientation( m_BoneTransforms );
}



int CAuthClothParser::FindNodeByName( const char *pName )
{
	// fill out the node map
	for ( ; m_nNodeNameMapNodes < m_Nodes.Count( ); m_nNodeNameMapNodes++ )
	{
		m_NodeNameMap[ m_Nodes[ m_nNodeNameMapNodes ].m_Name ] = m_nNodeNameMapNodes;
	}
	UtlSymId_t hFind = m_NodeNameMap.Find( pName );
	if ( hFind != UTL_INVAL_SYMBOL )
	{
		return m_NodeNameMap[ hFind ];
	}
	else
	{
		return -1;
	}
}


bool CAuthClothParser::Parse( KeyValues *kv )
{
	m_nDefaultCompatibilityMode = GetClothInt( kv, "compatibilityMode", 3 );
	m_bFollowTheLead = kv->GetBool( "followTheLead", true );
	m_nNodeNameMapNodes = 0;

	for ( KeyValues *pSubKey = kv->GetFirstSubKey( ); pSubKey != NULL; pSubKey = pSubKey->GetNextKey( ) )
	{
		const char *pSubkeyName = pSubKey->GetName( );
		if ( !V_stricmp( pSubkeyName, "Defaults" ) )
		{
			if ( !ParseDefaults( pSubKey ) )
				return false;
		}
		else if ( !V_stricmp( pSubKey->GetName( ), "Cloth" ) )
		{
			if ( !ParsePiece( pSubKey ) )
				return false;
		}
	}

	ReconstructHierarchy( );
	return true;
}



void CAuthClothParser::ReconstructHierarchy()
{
	// reconstruct the hierarchy. We could just assume that the hierarchy goes along the columns to previous row, but this pass
	// will allow any arbitrary topology of the cloth bones
	for ( int nModelBone = 0; nModelBone < m_ModelBoneToNode.Count( ); ++nModelBone )
	{
		int nChildNode = m_ModelBoneToNode[ nModelBone ];
		if ( nChildNode >= 0 )
		{
			int nModelBoneParent = m_BoneToParent[ nModelBone ];
			if ( nModelBoneParent >= 0 )
			{
				int nParentNode = m_ModelBoneToNode[ nModelBoneParent ];
				if ( nParentNode >= 0 )
				{
					int &refParent = m_Nodes[ nChildNode ].m_nParent;
					if ( refParent >= 0 && refParent != nParentNode )
					{
						Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Node %s has deducted parent %s, but skeleton parent is %s. Resolving conflict in favor of the skeleton hierarchy.\n", m_Nodes[ nChildNode ].m_Name.Get( ), m_Nodes[ refParent ].m_Name.Get( ), m_Nodes[ nParentNode ].m_Name.Get( ) );
					}
					refParent = nParentNode;
				}
			}
		}
	}
}




bool CAuthClothParser::ParseDefaults( KeyValues *pSubKey )
{
	m_flDefaultSurfaceStretch = GetClothFloat( pSubKey, "SurfaceStretch", 0.0f );
	m_flDefaultThreadStretch = GetClothFloat( pSubKey, "ThreadStretch", 0.0f );
	return true;
}


CBoneParseParams::CBoneParseParams( KeyValues *pSubKey, int nCompatibilityMode )
{
	m_pBonePrefix = GetClothString( pSubKey, "BonePrefix" );
	m_nNominalColumnCount = GetClothInt( pSubKey, "columns" );
	m_nRowCount = GetClothInt( pSubKey, "rows" );

	m_flWorldFriction = GetClothFloat( pSubKey, "WorldFriction", 0.35f );
	m_flAnimationForceAttraction = GetClothFloat( pSubKey, "AnimationForceAttraction" );
	m_flAnimationVertexAttraction = GetClothFloat( pSubKey, "AnimationVertexAttraction" );
	m_flDamping = GetClothFloat( pSubKey, "damping" ); // this is velocity-acceleration damping: ( damping force ) = -flDamping * ( velocity ); velocity *= 1 - flDamping * dt / m
	m_flFixedPointDamping = GetClothFloat( pSubKey, "FixedPointDamping", 0.0f );
	m_flFollowRootEnd = m_flFollowRootBegin = m_flFixedPointDamping;
	m_flSpringStretchiness = GetClothFloat( pSubKey, "SpringStretchiness", 0.0f );

	// <sergiy> stretchiness meant opposite things as constructor parameter and internal member variable. This is how Dota Source1 implementation goes. I'll rename the internal parameter to something else in S2 to distinguish.
	// Also: CClothSpring::m_flStretchiness = 1 - SpringStretchiness from keyvalue file... yeah..
	m_flRelaxationFactor = clamp( 1.0f - m_flSpringStretchiness, 0.0f, 1.0f );

	// the stretch force is computed after applying SpringStretchiness  (flRelaxationFactor) in Source1 cloth, so we'll follow the same pattern to be true to the original: we'll premultiply it when creating CString
	// in the new versions, we shouldn't support stretch force. We should use damping instead.
	m_flStretchForce = GetClothFloat( pSubKey, "StretchForce", nCompatibilityMode >= 2 ? 1.0f : 0.0f );
	m_flStructSpringConstant = GetClothFloat( pSubKey, "StructSpringConstant", 4.0f );
	m_flStructSpringDamping = GetClothFloat( pSubKey, "StructSpringDamping", 0.6f );

	float flDefaultGravity = nCompatibilityMode >= 1 ? 20 : 380;
	m_flGravityScale = GetClothFloat( pSubKey, "gravity_scale", 0.0f );
	if ( const char *pGravityString = GetClothString( pSubKey, "gravity", NULL ) )
	{
		Vector vGravityDirection = Vector( 0, 0, -flDefaultGravity );
		if ( 3 == sscanf( pGravityString, "%g %g %g", &vGravityDirection.x, &vGravityDirection.y, &vGravityDirection.z ) )
		{
			if ( m_flGravityScale == 0.0f )
			{
				m_flGravityScale = vGravityDirection.NormalizeInPlace();
			}
			else
			{
				m_flGravityScale *= vGravityDirection.NormalizeInPlace();
			}

			if ( vGravityDirection.z > 0 )
			{
				// Everything further assumes negative gravity direction (towards -Z)
				vGravityDirection = -vGravityDirection;
				m_flGravityScale = -m_flGravityScale;
			}
		}
	}
	else
	{
		m_flGravityScale = flDefaultGravity;
	}

	if ( const char *pStabilizeAnim = GetClothString( pSubKey, "stabilizeAnim", NULL ) )
	{
		switch ( sscanf( pStabilizeAnim, "%g %g", &m_flFollowRootBegin, &m_flFollowRootEnd ) )
		{
		case 2:
			break; // ok, we parsed both
		
		case 0:
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse stabilizeAnim extended keyword parameters, leaving stabilization at the default. Fixed point damping = %g\n", m_flFixedPointDamping );
			break;

		case 1:
			m_flFollowRootEnd = m_flFollowRootBegin;
			break;
		}
	}

	m_bIsRopeS1 = nCompatibilityMode >= 3 && m_nNominalColumnCount == 1;
	m_nVirtColumnCount = m_nNominalColumnCount;
	if ( m_bIsRopeS1 )
	{
		m_nVirtColumnCount++; // add an extra column, like in Source1 rope simulation
	}
}


void CBoneParseParams::ApplyDefaultParams( CBone &node  )
{
	// apply node settings that are not per node
	node.m_Integrator.flPointDamping = m_flDamping;
	// <sergiy> Important fix: the springs (particularly animation force attraction) in Source1 were tuned to the "mass" of cloth particles (see CClothParticleState::Integrate() , flDeltaTimeMass = flFrameTime * pClothParticle->GetInverseMass() )
	//          Source2 computes accelerations directly, so we need to pre-divide by mass here. I forgot to do that in CL 2322760, fixed in 2532503 in femodelbuilder.cpp:2220
	node.m_Integrator.flAnimationForceAttraction = m_flAnimationForceAttraction;
	node.m_Integrator.flAnimationVertexAttraction = m_flAnimationVertexAttraction;
	node.m_Integrator.flGravity = m_flGravityScale;
	node.m_flWorldFriction = m_flWorldFriction;
	node.m_flLegacyStretchForce = m_flStretchForce;
}





bool CAuthClothParser::ParsePiece( KeyValues *pSubKey )
{
	m_nCompatibilityMode = GetClothInt( pSubKey, "compatibilityMode", m_nDefaultCompatibilityMode );
	if ( m_nCompatibilityMode >= 2 )
	{
		m_bUninertialRods = true;
	}

	if ( m_nCompatibilityMode >= 1 )
	{
		m_bExplicitMasses = true;
		m_bUnitlessDamping = false; // in Dota2 Source1 damping was a coefficient for computing force, not acceleration. So the same damping would affect nodes with differing masses differently
	}

	CBoneParseParams parseParm( pSubKey, m_nCompatibilityMode );
	m_flLocalForce = GetClothFloat( pSubKey, "LocalForce", m_flLocalForce );
	m_flLocalRotation = 1.0f - GetClothFloat( pSubKey, "LocalRotation", 1.0f - m_flLocalRotation );

	bool bAnonymousPiece = !parseParm.m_pBonePrefix || !*parseParm.m_pBonePrefix || parseParm.m_nNominalColumnCount <= 0 || parseParm.m_nRowCount <= 0; NOTE_UNUSED( bAnonymousPiece );

	parseParm.m_pCollisionSpheres = &m_CollisionSpheres;
	parseParm.m_pCollisionPlanes = &m_CollisionPlanes;

	if ( !bAnonymousPiece )
	{
		int nNodeBaseIndex = m_Nodes.AddMultipleToTail( parseParm.m_nRowCount * parseParm.m_nVirtColumnCount );
		for ( int nNode = nNodeBaseIndex; nNode < m_Nodes.Count(); ++nNode )
		{
			m_Nodes[ nNode ].m_flLocalForce = m_flLocalForce;
			m_Nodes[ nNode ].m_flLocalRotation = m_flLocalRotation;
		}

		CNodeIdx nodeIdx( nNodeBaseIndex, parseParm.m_nRowCount, parseParm.m_nVirtColumnCount );

		if ( !ParseLegacyDotaNodeGrid( pSubKey, parseParm, nodeIdx ) )
			return false;

		if ( !CreateLegacyDotaRodGrid( parseParm, nodeIdx, m_nCompatibilityMode ) )
			return false;
	}

	if ( m_nCompatibilityMode <= 0 )
	{
		ParseExplicitDefinitions( pSubKey, parseParm );
	}

	return true;
}



bool Preparse( KeyValues *kv, CAuthPhysFx::CCollisionSphere &sphere )
{

	sphere.m_bInclusive = 0 == V_stricmp( kv->GetName(), "in_sphere" );
	if ( kv->GetBool( "inside" ) || kv->GetBool( "inclusive" ) )
		sphere.m_bInclusive = true;
	if ( kv->GetBool( "outside" ) || kv->GetBool( "exclusive" ) )
		sphere.m_bInclusive = false;

	FindKey( kv, "radius", &sphere.m_flRadius );

	const char *pCenter = kv->GetString( "center", NULL );
	if ( !pCenter )
	{
		pCenter = kv->GetString( "offset", NULL );
	}
	if ( pCenter )
	{
		if ( 3 != sscanf( pCenter, "%g %g %g", &sphere.m_vOrigin.x, &sphere.m_vOrigin.y, &sphere.m_vOrigin.z ) )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse sphere center %s\n", pCenter );
		}
	}

	if ( sphere.m_flRadius < 1e-5f && !sphere.m_bInclusive )
	{
		return false; // no collision with this sphere
	}
	return true;
}


bool Preparse( KeyValues *kv, CAuthPhysFx::CCollisionPlane &plane )
{
	FindKey( kv, "offset", &plane.m_Plane.m_flOffset );

	if ( const char *pNormal = kv->GetString( "normal", NULL ) )
	{
		if ( 3 != sscanf( pNormal, "%g %g %g", &plane.m_Plane.m_vNormal.x, &plane.m_Plane.m_vNormal.y, &plane.m_Plane.m_vNormal.z ) )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse plane normal %s\n", pNormal );
		}
		else
		{
			plane.m_Plane.m_vNormal.NormalizedSafe( Vector( 1, 0, 0 ) );
		}
	}

	return true;
}


template <typename T >
void CAuthClothParser::ParseExplicitColl( KeyValues *kv, CBoneParseParams &parseParm, CUtlVector< T > &collArray )
{
	T coll;

	if ( !Preparse( kv, coll ) )
		return;
	if ( const char *pParent = kv->GetString( "parent", NULL ) )
	{
		coll.m_nParentBone = FindNodeByName( pParent );
		if ( coll.m_nParentBone < 0 )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot find coll parent bone %s\n", pParent );
			return;
		}
	}
	if ( const char *pChild = kv->GetString( "child", NULL ) )
	{
		coll.m_nChildBone = FindNodeByName( pChild );
		if ( coll.m_nChildBone < 0 )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot find coll child bone %s\n", pChild );
		}
	}

	if ( coll.m_nChildBone < 0 )
		coll.m_nChildBone = coll.m_nParentBone;
	if ( coll.m_nParentBone < 0 )
		coll.m_nParentBone = coll.m_nChildBone;

	if ( coll.m_nParentBone < 0 )
	{
		Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Collision Sphere has no parent bone\n" );
		return;
	}


	if ( const char *pChildren = kv->GetString( "children", NULL ) )
	{
		CUtlStringList tokens( pChildren, g_pTokenSeparators, ARRAYSIZE( g_pTokenSeparators ) );
		for ( int i = 0; i < tokens.Count( ); ++i )
		{
			const char *pChildName = tokens[ i ];
			coll.m_nChildBone = FindNodeByName( pChildName );
			if ( coll.m_nChildBone >= 0 )
			{
				collArray.AddToTail( coll );
			}
			else
			{
				Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot find child %s\n", pChildName );
			}
		}
	}
	else
	{
		Assert( coll.m_nChildBone >= 0 );
		collArray.AddToTail( coll );
	}
}






void CAuthClothParser::ParseExplicitDefinitions( KeyValues *pCloth, CBoneParseParams &parseParm )
{
	for ( KeyValues *pSubKey = pCloth->GetFirstSubKey( ); pSubKey != NULL; pSubKey = pSubKey->GetNextKey( ) )
	{
		const char *pSubkeyName = pSubKey->GetName( );
		if ( !V_stricmp( pSubkeyName, "node" ) )
		{
			ParseExplicitNode( pSubKey, parseParm );
		}
		else if ( !V_stricmp( pSubkeyName, "tri" ) || !V_stricmp( pSubkeyName, "quad" ) || !V_stricmp( pSubkeyName, "elem" ) )
		{
			ParseExplicitElem( pSubKey, parseParm );
		}
		else if ( !V_stricmp( pSubkeyName, "sphere" ) || !V_stricmp( pSubkeyName, "in_sphere" ) || !V_stricmp( pSubkeyName, "ex_sphere" ) )
		{
			ParseExplicitColl( pSubKey, parseParm, m_CollisionSpheres );
		}
		else if ( !V_stricmp( pSubkeyName, "plane" ) )
		{
			ParseExplicitColl( pSubKey, parseParm, m_CollisionPlanes );
		}
	}
}




void CAuthClothParser::ParseExplicitNode( KeyValues *kv, CBoneParseParams &parseParm )
{
	CBone bone;
	bone.m_Name = kv->GetString( "name" );
	parseParm.ApplyDefaultParams( bone );
	
	FindKey( kv, "mass", &bone.m_flMass );
	FindKey( kv, "gravity", &bone.m_Integrator.flGravity );
	FindKey( kv, "damping", &bone.m_Integrator.flPointDamping );

	if ( const char *pParent = kv->GetString( "parent", NULL ) )
	{
		bone.m_nParent = FindNodeByName( pParent );
		if ( bone.m_nParent < 0 )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Explicit Node %s - cannot find parent %s\n", bone.m_Name.Get( ), pParent );
		}
	}

	if ( const char *pFollowParent = kv->GetString( "followParent", NULL ) )
	{
		bone.m_nFollowParent = FindNodeByName( pFollowParent );
		if ( bone.m_nFollowParent < 0 )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Explicit Node %s - cannot find followParent %s\n", bone.m_Name.Get( ), pFollowParent );
		}
	}

	if ( FindKey( kv, "followWeight", &bone.m_flFollowWeight ) && bone.m_flFollowWeight > 0 && bone.m_nParent >= 0 && bone.m_nFollowParent < 0 )
	{
		// if there's parent, followWeight, but no followParent, we can assume followParent	to be = parent
		bone.m_nFollowParent = bone.m_nParent;
	}

	if ( const char *pOffset = kv->GetString( "offset", NULL ) )
	{
		Vector vOffset;
		if ( sscanf( pOffset, "%g %g %g", &vOffset.x, &vOffset.y, &vOffset.z ) != 3 )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Explicit node %s - cannot parse offset %s\n", bone.m_Name.Get( ), pOffset );
		}
		else if ( bone.m_nParent < 0 )
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Explicit node %s has offset %s but no parent\n", bone.m_Name.Get( ), pOffset );
		}
		else
		{
			const CTransform &parentXform = m_Nodes[ bone.m_nParent ].m_Transform;
			bone.m_Transform.m_orientation = parentXform.m_orientation;
			bone.m_Transform.m_vPosition = TransformPoint( parentXform, vOffset );
		}
	}

	if ( const char *pFlags = kv->GetString( "flags", NULL ) )
	{
		parseParm.ApplyDotaFlags( bone, pFlags );
	}

	if ( kv->GetBool( "static" ) )
	{
		bone.m_bSimulated = false;
		bone.m_bFreeRotation = false;
	}

	if ( const char *pLock = kv->GetString( "lock", NULL ) )
	{
		CUtlStringList tokens( pLock, g_pTokenSeparators, ARRAYSIZE( g_pTokenSeparators ) );
		for ( int i = 0; i < tokens.Count( ); ++i )
		{
			if ( !V_stricmp( tokens[ i ], "position" ) )
			{
				bone.m_bSimulated = false;
			}
			else if ( !V_stricmp( tokens[ i ], "rotation" ) )
			{
				bone.m_bFreeRotation = false;
			}
			else
			{
				Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Invalid lock parameter %s in bone %s, ignoring\n", tokens[ i ], bone.m_Name.Get() );
			}
		}
	}


	if ( !bone.m_bFreeRotation && bone.m_bSimulated )
	{
		Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Bone %s rotation is locked, but the bone is simulated, which is not useful\n", bone.m_Name.Get( ) );
	}

	int nNewNode = m_Nodes.AddToTail( bone );
	TieModelToNewNode( nNewNode );
}




void CAuthClothParser::ParseExplicitElem( KeyValues *kv, CBoneParseParams &parseParm )
{
	const char *pNodes = kv->GetString( "nodes", NULL );
	if ( !pNodes )
	{
		pNodes = kv->GetString( );
	}
	if ( pNodes )
	{
		CUtlStringList tokens( pNodes, g_pTokenSeparators, ARRAYSIZE( g_pTokenSeparators ) );
		CQuad quad;
		int nNodeCount = 0;
		for ( int i = 0; i < tokens.Count( ); ++i )
		{
			int nNode = FindNodeByName( tokens[ i ] );
			if ( nNode >= 0 )
			{
				quad.m_nNodes[ nNodeCount ] = nNode;
				++nNodeCount;
			}
			else
			{
				Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cloth cannot find element node '%s', ignoring!\n", tokens[ i ] );
			}
		}

		if ( nNodeCount >= 3 )
		{
			for ( int i = nNodeCount; i < 4; ++i )
				quad.m_nNodes[ i ] = quad.m_nNodes[ nNodeCount - 1 ];


			m_Quads.AddToTail( quad );
		}
		else
		{
			Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cloth element must have 3 or  4 nodes: %s\n", pNodes );
		}
	}
}




// Return: true if the node was successfully tied to an existing bone
bool CAuthClothParser::TieModelToNewNode( int nNewNode )
{
	CBone &node = m_Nodes[ nNewNode ];
	UtlSymId_t nFindBone = m_BoneToIndex.Find( node.m_Name.Get( ) );

	if ( nFindBone != UTL_INVAL_SYMBOL )
	{
		int nBoneIndex = m_BoneToIndex[ nFindBone ];
		node.m_Transform = m_BoneTransforms[ nBoneIndex ];
		// this bone hasn't been mapped to an earlier node, it's original bone-node, map it now
		m_ModelBoneToNode[ nBoneIndex ] = nNewNode;
		return true;
	}
	else
	{
		// we'll figure the parent out later
		node.m_bVirtual = true; // this node is virtual. it has no bone name.
		return false;
	}
}


bool CAuthClothParser::ParseLegacyDotaNodeGrid( KeyValues *pSubKey, CBoneParseParams &parseParm, const CNodeIdx &nodeIdx )
{
	CVarBitVec nodesInitialized( m_Nodes.Count( ) );

	// add the grid (or one strand) of Nodes
	for ( int nColumn = 0; nColumn < parseParm.m_nVirtColumnCount; ++nColumn )
	{
		for ( int nRow = 0; nRow < parseParm.m_nRowCount; ++nRow )
		{
			const char *pRowFlags = GetClothString( pSubKey, CFmtStr( "r%d", nRow ).Get( ) );
			const char *pColumnFlags = GetClothString( pSubKey, CFmtStr( "c%d", nColumn ).Get( ) );
			const char *pNodeFlags = GetClothString( pSubKey, CFmtStr( "r%dc%d", nRow, nColumn ).Get( ) );

			int nNewNode = nodeIdx( nRow, nColumn );
			CBone &node = m_Nodes[ nNewNode ];
			// There is no possibility to create free-rotating static nodes in dota2 source1
			node.m_bFreeRotation = false;
			// then, apply sequentially settings: per column; per individual node
			parseParm.m_nBoneIndex = nNewNode;

			if ( parseParm.m_bIsRopeS1 && nColumn )
			{
				parseParm.m_bPrevColumnParent = true; // for the ropes, we parent virtual column #1 to the previous column #0
				parseParm.m_vOffset = Vector( 0, -20, 0 ); // NOTE: OS offset!
				node.m_bOsOffset = true;
				node.m_bVirtual = true;
			}
			else
			{
				parseParm.m_bPrevColumnParent = false;
				parseParm.m_vOffset = vec3_origin;
			}

			node.m_Name.Format( "%sr%dc%d", parseParm.m_pBonePrefix, nRow, nColumn );
			
			if ( nRow > 0 && node.m_bSimulated )
			{
				// note: following happensin Dota2 Source1 in CClothModelPiece::SetupBone() in cloth_system.cpp#36:3412, the whole column follows the static parent
				// this happens for ropes and cloths. 
				node.m_nFollowParent = nodeIdx( 0, parseParm.m_bIsRopeS1 ? 0 : nColumn );
				node.m_flFollowWeight = nRow * ( parseParm.m_flFollowRootEnd - parseParm.m_flFollowRootBegin ) / ( parseParm.m_nRowCount - 1 ) + parseParm.m_flFollowRootBegin;
			}

			if ( !TieModelToNewNode( nNewNode ) )
			{
				//node.m_Name += "(virtual)"; // the name doesn't matter, except for debugging, so marking it clearly as a virtual bone name
			}

			parseParm.ApplyDefaultParams( node );

			parseParm.ApplyDotaFlags( node, pRowFlags );
			parseParm.ApplyDotaFlags( node, pColumnFlags );
			parseParm.ApplyDotaFlags( node, pNodeFlags );

			if ( !node.m_bSimulated )
			{
				node.m_Integrator.flPointDamping = parseParm.m_flFixedPointDamping * 60; // Dota cloth didn't take variability of time into account, we will
			}

			// pre-divide by nominal mass
			if ( node.m_flMass > 0.0333f )
			{
				node.m_Integrator.flAnimationForceAttraction /= node.m_flMass;
				if ( node.m_bSimulated )
				{
					// NOTE: the damping gets divided by mass later in the FE model builder
					// node.m_Integrator.flPointDamping /= node.m_flMass;
				}
			}

			if ( parseParm.m_bPrevColumnParent )
			{
				if ( nColumn > 0 )
				{
					node.m_nParent = nodeIdx( nRow, nColumn - 1 );
				}
			}
			else
			{
				if ( nRow > 0 )
				{
					node.m_nParent = nodeIdx( nRow - 1, nColumn );
				}
			}

			if ( node.m_bVirtual )
			{
				if ( node.m_nParent < 0 )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Virtual particle %s has no parent to latch onto\n", node.m_Name.Get( ) );
					return false; // we tried to find a parent bone , but there was none. We can't have a virtual particle without a clear parent bone to at least define its relaxed position
				}
				if ( !nodesInitialized.IsBitSet( node.m_nParent ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Virtual particle %s has parent that hasn't been initialized yet, ordering problem\n", node.m_Name.Get( ) );
					return false;
				}
				if ( parseParm.m_vOffset.Length( ) < 1e-6f )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Virtual particle %s has no offset to its parent %s, it's useless, please just reuse %s\n", node.m_Name.Get( ), m_Nodes[ node.m_nParent ].m_Name.Get( ), node.m_Name.Get( ) );
				}
				const CTransform &parentXform = m_Nodes[ node.m_nParent ].m_Transform;
				node.m_Transform.m_orientation = parentXform.m_orientation;
				if ( node.m_bOsOffset )
				{
					node.m_Transform.m_vPosition = parentXform.m_vPosition + parseParm.m_vOffset;
				}
				else
				{
					node.m_Transform.m_vPosition = TransformPoint( parentXform, parseParm.m_vOffset );
				}
			}

			nodesInitialized.Set( nNewNode );
		}
	}
	return true;
}



void CAuthClothParser::AddDotaRod( uint nNode0, uint nNode1, CBoneParseParams &parseParm )
{
	AddRod( m_Nodes, nNode0, nNode1, parseParm.m_flRelaxationFactor );
	// springs do not evaluate in Source1: the only call to Evaluate is after a return;
	// springs do ResolveStretch in Source1, but the effect is simply applying velocity of vDir * ( ( flDistance - flRestLength ) * flStretchiness * flStretchForce) in the direction vDir of fixing up the rod.
	// This can be accounted for in the node damping: since all nodes in a cloth piece have the same flStretchiness * flStretchForce, and verlet integrator will by default cause the delta velocity = 60 * ( flDistance - flRestLength )
	//m_Springs.AddToTail( CSpring( nNode0, nNode1, parseParm.m_flStructSpringConstant, parseParm.m_flStructSpringDamping, parseParm.m_flStretchForce, parseParm.m_flRelaxationFactor ) );
}

bool CAuthClothParser::CreateLegacyDotaRodGrid( CBoneParseParams &parseParm, const CNodeIdx &nodeIdx, int nCompatibilityMode )
{
	// add finite elements (either rods or quads) that will simulate the cloth
	if ( parseParm.m_nVirtColumnCount == 1 )
	{
		// special case, single-strand cloth (rope)
		for ( int nRow = 1; nRow < parseParm.m_nRowCount; ++nRow )
		{
			uint nNode0 = nodeIdx( nRow - 1, 0 ), nNode1 = nodeIdx( nRow, 0 );
			AddDotaRod( nNode0, nNode1, parseParm );
			if ( nRow >= 2 && /*bUseBendSprings*/false )
			{
				uint nNodePrev = nodeIdx( nRow - 2, 0 );
				AddDotaRod( nNodePrev, nNode1, parseParm );
			}
		}
	}
	else
	{
		for ( int nRow = 1; nRow < parseParm.m_nRowCount; ++nRow )
		{
			for ( int nColumn = 1; nColumn < parseParm.m_nVirtColumnCount; ++nColumn )
			{
				uint nNode00 = nodeIdx( nRow - 1, nColumn - 1 );
				uint nNode01 = nodeIdx( nRow - 1, nColumn );
				uint nNode11 = nodeIdx( nRow, nColumn );
				uint nNode10 = nodeIdx( nRow, nColumn - 1 );

				if ( m_nCompatibilityMode == 0 || ( m_nCompatibilityMode == 1 && nRow >= 2 ) )
				{
					CQuad &quad = m_Quads[ m_Quads.AddToTail( ) ];
					quad.m_nNodes[ 0 ] = nNode00;
					quad.m_nNodes[ 1 ] = nNode01;
					quad.m_nNodes[ 2 ] = nNode11;
					quad.m_nNodes[ 3 ] = nNode10;
					// in compat 1, we skip the first row of quads; in compat 2+, we skip all quads
				}
				else
				{
					// for compatibility with previous version of the solver, simplify to rods
					//  
					// "Structural springs", in source1 cloth lingo
					//
					AddDotaRod( nNode00, nNode01, parseParm );
					AddDotaRod( nNode00, nNode10, parseParm );

					if ( nColumn + 1 == parseParm.m_nVirtColumnCount )
					{
						// last column
						AddDotaRod( nNode01, nNode11, parseParm );
					}

					if ( nRow + 1 == parseParm.m_nRowCount )
					{
						// last row
						AddDotaRod( nNode10, nNode11, parseParm );
					}
					//
					// "Shear springs"
					//
					if ( false )
					{
						AddDotaRod( nNode00, nNode11, parseParm );
						AddDotaRod( nNode01, nNode10, parseParm );
					}
				}
			}
		}

		CreateGridNodeBases( nodeIdx, parseParm.m_nRowCount, parseParm.m_nVirtColumnCount, m_Nodes, m_PresetNodeBases );
	}
	return true;
}


static const char	*s_pszTokenDelimiter = " ,|\t\r\n";

bool FloatToken( float &x )
{
	const char *pToken = strtok( NULL, s_pszTokenDelimiter );
	if ( !pToken )
		return false;
	x = atof( pToken );
	return true;
}

bool VectorToken( Vector &v )
{
	return FloatToken( v.x ) && FloatToken( v.y ) && FloatToken( v.z );
}

bool KeywordToken( const char *pKeyword )
{
	const char *pToken = strtok( NULL, s_pszTokenDelimiter );
	if ( !pToken )
		return false;
	return V_stricmp( pToken, pKeyword ) == 0;
}



bool CBoneParseParams::ApplyDotaFlags( CAuthPhysFx::CBone &bone, const char *pszParms )
{
	if ( pszParms[ 0 ] == 0 )
	{
		return true;
	}

	int nParamLength = V_strlen( pszParms );
	if ( nParamLength > 1024 )
	{
		Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Keyvalue parameter too long in %s\n", bone.m_Name.Get( ) );
		return false;
	}

	char *pszParmsCopy = ( char * ) stackalloc( nParamLength + 1 );
	if ( pszParmsCopy != NULL )
	{
		V_strcpy( pszParmsCopy, pszParms );

		char *pszToken = strtok( pszParmsCopy, s_pszTokenDelimiter );
		while ( pszToken != NULL )
		{
			if ( V_stricmp( pszToken, "fixed" ) == 0 )
			{
				bone.m_bSimulated = false;
			}
			else if ( V_stricmp( pszToken, "world" ) == 0 )
			{
				bone.m_bNeedsWorldCollision = true;
			}
			else if ( V_stricmp( pszToken, "mass" ) == 0 )
			{
				float flValue;
				if( FloatToken( flValue ) )
				{
					if ( flValue > 0.0f )
					{
						bone.m_flMass = flValue;
						bone.m_bHasMassOverride = true;
						bone.m_bSimulated = true;
					}
					else
					{
						bone.m_flMass = 1.0f; // take default mass;  todo: find out what "mass 0" is actually supposed to mean
						bone.m_bHasMassOverride = false;
						bone.m_bSimulated = true;
					}
				}
				else
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse mass in %s\n", bone.m_Name.Get() );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "damping" ) == 0 )
			{
				if ( !FloatToken( bone.m_Integrator.flPointDamping ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse damping in %s\n", bone.m_Name.Get( ) );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "gravity" ) == 0 )
			{
				if ( !FloatToken( bone.m_Integrator.flGravity ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse gravity in %s\n", bone.m_Name.Get( ) );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "offsetx" ) == 0 )
			{
				if ( !FloatToken( m_vOffset.x ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse offsetx in %s\n", bone.m_Name.Get( ) );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "offsety" ) == 0 )
			{
				if ( !FloatToken( m_vOffset.y ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse offsety in %s\n", bone.m_Name.Get( ) );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "offsetz" ) == 0 )
			{
				if ( !FloatToken( m_vOffset.z ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse offsetz in %s\n", bone.m_Name.Get( ) );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "prev_col_parent" ) == 0 )
			{
				m_bPrevColumnParent = true;
			}
			else if ( V_stricmp( pszToken, "prev_row_parent" ) == 0 )
			{
				m_bPrevColumnParent = false;
			}
			else if ( V_stricmp( pszToken, "stabilizeAnim" ) == 0 )
			{
				if ( !FloatToken( bone.m_flFollowWeight ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse stabilizeAnim in %s\n", bone.m_Name.Get( ) );
					return false;
				}
			}
			else if ( V_stricmp( pszToken, "in_sphere" ) == 0 || V_stricmp( pszToken, "ex_sphere" ) == 0 )
			{
				CCollisionSphere ce;
				if ( !VectorToken( ce.m_vOrigin ) || !KeywordToken( "r" ) || !FloatToken( ce.m_flRadius ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse %s in %s\n", pszToken, bone.m_Name.Get( ) );
					return false;
				}
				ce.m_bInclusive = V_stricmp( pszToken, "in_sphere" ) == 0;
				ce.m_nChildBone = ce.m_nParentBone = m_nBoneIndex;
				m_pCollisionSpheres->AddToTail( ce );
			}
			else if ( V_stricmp( pszToken, "plane" ) == 0 )
			{
				CCollisionPlane cp;
				if ( !VectorToken( cp.m_Plane.m_vNormal ) || !KeywordToken( "d" ) || !FloatToken( cp.m_Plane.m_flOffset ) )
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse %s in %s\n", pszToken, bone.m_Name.Get( ) );
					return false;
				}
				cp.m_nChildBone = cp.m_nParentBone = m_nBoneIndex;
				m_pCollisionPlanes->AddToTail( cp );
			}
			/*else if ( V_stricmp( pszToken, "0" ) == 0 )
			{
				bone.m_flMass = 1.0f; // take default mass;  it seems impossible now to find out what "mass 0" was actually supposed to mean originally
				bone.m_bHasMassOverride = false;
				bone.m_bSimulated = true;
			}*/
			else
			{
				float flValue = V_atof( pszToken );
				if ( flValue != 0.0f )
				{
					// <sergiy> just a number is ignored in source1; Some artists (e.g. JO) thought it should means mass, but it doesn't mean anything in source1 cloth, so for compatibility I'll ignore it here, too
					//bone.m_flMass = flValue;
					//bone.m_bHasMassOverride = true;
					//bone.m_bSimulated = true;
				}
				else
				{
					Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING " Cannot parse %s in %s\n", pszToken, bone.m_Name.Get( ) );
					return false;
				}
			}

			
			if ( pszToken )
			{
				pszToken = strtok( NULL, s_pszTokenDelimiter );
			}
		}
	}
	return true;
}



int CAuthPhysFx::GetSimParticleCount()const
{
	int nCount = 0;
	for( int nNode = 0; nNode < m_Nodes.Count(); ++nNode )
	{
		if( m_Nodes[nNode].m_bSimulated )
		{
			nCount++;
		}
	}
	return nCount;
}



float CAuthPhysFx::GetRodLength( int nRod )const
{
	const CRod & rod = m_Rods[ nRod ];
	if ( rod.m_bExplicitLength )
	{
		return rod.m_flLength;
	}
	else
	{
		return ( m_Nodes[ rod.m_nNodes[ 0 ] ].m_Transform.m_vPosition - m_Nodes[ rod.m_nNodes[ 1 ] ].m_Transform.m_vPosition ).Length(); // implicit length
	}
}


bool CAuthPhysFx::IsSimilarTo( const CFeModel *pFeModel )const
{
	if ( !pFeModel )
	{
		return IsEmpty();
	}

	if ( int( pFeModel->m_nNodeCount ) != m_Nodes.Count() )
		return false;
	if ( int( pFeModel->m_nQuadCount ) != m_Quads.Count() )
		return false;
	if ( int( pFeModel->m_nRodCount ) != m_Rods.Count() )
		return false;
	if ( int( pFeModel->m_nCtrlOffsets ) != m_CtrlOffsets.Count() )
		return false;
	for ( int i = 0; i < m_Nodes.Count(); ++i )
	{
		if ( m_Nodes[ i ].m_Name != pFeModel->GetNodeName( i ) )
			return false;
	}
	return true;
}





CFeModelBuilder::BuildNode_t CAuthPhysFx::CBone::AsBuildNode()const
{
	CFeModelBuilder::BuildNode_t node;

	const CBone &bone = *this;

	node.pName = bone.m_Name.Get();
	node.transform = bone.m_Transform;
	node.nParent = bone.m_nParent;
	node.nFollowParent = bone.m_nFollowParent;
	node.flFollowWeight = bone.m_flFollowWeight;
	if ( bone.m_bSimulated && bone.m_flMass > 0 )
	{
		node.flMassMultiplier = bone.m_flMass;
		node.invMass = 1.0f / bone.m_flMass;
		node.bSimulated = true;
		node.bForceSimulated = bone.m_bForceSimulated;
	}
	else
	{
		node.flMassMultiplier = 0;
		node.invMass = 0;
		node.bSimulated = false;
	}
	node.flMassBias = bone.m_flMassBias;
	node.bFreeRotation = bone.m_bFreeRotation;
	node.bAnimRotation = bone.m_bAnimRotation;
	node.bVirtual = bone.m_bVirtual;
	node.bNeedNodeBase = bone.m_bNeedNodeBase;
	node.bOsOffset = bone.m_bOsOffset;
	node.bMassMultiplierGlobal = bone.m_bHasMassOverride;
	node.flSlack = 0;
	node.integrator = bone.m_Integrator;
	node.flLegacyStretchForce = bone.m_flLegacyStretchForce;
	node.bWorldCollision = bone.m_bNeedsWorldCollision;
	node.flWorldFriction = bone.m_flWorldFriction;
	node.flGroundFriction = bone.m_flGroundFriction;
	node.flCollisionRadius = bone.m_flCollisionRadius;
	node.flLocalRotation = bone.m_flLocalRotation;
	node.flLocalForce = bone.m_flLocalForce;
	node.nCollisionMask = bone.m_nCollisionMask;

	return node;
}



CFeModelBuilder::BuildElem_t CAuthPhysFx::CQuad::AsBuildElem() const
{
	CFeModelBuilder::BuildElem_t elem;
	const CAuthPhysFx::CQuad &quad = *this;

	elem.nNode[ 0 ] = quad.m_nNodes[ 0 ];
	elem.nNode[ 1 ] = quad.m_nNodes[ 1 ];
	elem.nNode[ 2 ] = quad.m_nNodes[ 2 ];
	elem.nNode[ 3 ] = quad.m_nNodes[ 3 ];
	elem.nStaticNodes = 0;
	elem.nRank = 0;
	elem.flSlack = 0;

	return elem;
}





CLockedResource< PhysFeModelDesc_t > CAuthPhysFx::Compile( CResourceStream *pStream, const CVClothProxyMeshOptions *pOptions )const
{
	CFeModelBuilder builder;
	if ( pOptions )
	{
		builder.m_bNeedBacksolvedBasesOnly = pOptions->m_bDriveMeshesWithBacksolvedJointsOnly;
	}

	// Note: for now I'm skipping (pre-applying) the mapping from ctrl to node;
	// that mapping will be set to NULL. So, the indices and count of nodes in builder.m_Nodes and m_NOdes is different
	builder.EnableIdentityCtrlOrder( );

	const int nParentBodyCount = /*pParent->GetBodyCount()*/ 0;
	
	builder.m_Nodes.EnsureCapacity( m_Nodes.Count() + nParentBodyCount );
	builder.m_Nodes.SetCount( m_Nodes.Count() );
	builder.m_Elems.EnsureCapacity( m_Quads.Count( ) );
	builder.m_Springs.EnsureCapacity( m_Springs.Count( ) );
	builder.m_Rods.EnsureCapacity( m_Rods.Count( ) );
	builder.m_PresetNodeBases.CopyArray( m_PresetNodeBases.Base( ), m_PresetNodeBases.Count() );
	builder.m_TaperedCapsuleStretches.CopyArray( m_TaperedCapsuleStretches.Base(), m_TaperedCapsuleStretches.Count() );
	builder.m_TaperedCapsuleRigids.CopyArray( m_TaperedCapsuleRigids.Base(), m_TaperedCapsuleRigids.Count() );
	builder.m_SphereRigids.CopyArray( m_SphereRigids.Base(), m_SphereRigids.Count() );
	builder.m_CtrlOffsets.SetCount( m_CtrlOffsets.Count() );
	for ( int nCtrlOffset = 0; nCtrlOffset < m_CtrlOffsets.Count(); ++nCtrlOffset )
	{
		builder.m_CtrlOffsets[ nCtrlOffset ] = m_CtrlOffsets[ nCtrlOffset ];
	}
	builder.m_FitInfluences.CopyArray( m_FitInfluences.Base(), m_FitInfluences.Count() );

	for ( int nNode = 0; nNode < m_Nodes.Count(); ++nNode )
	{
		builder.m_Nodes[ nNode ] = m_Nodes[ nNode ].AsBuildNode();
	}

	for ( int nQuad = 0; nQuad < m_Quads.Count(); ++nQuad )
	{
		const CQuad &quad = m_Quads[ nQuad ];
		CFeModelBuilder::BuildElem_t elem = quad.AsBuildElem();
		uint numNodes = elem.NumNodes();
		BubbleSort( elem.nNode, numNodes, [&builder] ( uint left, uint right ){
			return builder.m_Nodes[ left ].invMass < builder.m_Nodes[ right ].invMass;
		} );
		elem.nNode[ 3 ] = elem.nNode[ numNodes - 1 ];

		for ( elem.nStaticNodes = 0; elem.nStaticNodes < numNodes && builder.m_Nodes[ elem.nNode[ elem.nStaticNodes ] ].invMass == 0; ++elem.nStaticNodes )
			continue;

		if ( quad.m_bUseRods )
		{
			// convert this to rods?
		}

		builder.m_Elems.AddToTail( elem ); // if elem has 3 or 4 static nodes, the builder will skip it; builder will also sort everything as needed
	}

	for ( int nRod = 0; nRod < m_Rods.Count( ); ++nRod )
	{
		const CRod &source = m_Rods[ nRod ];
		FeRodConstraint_t rod;
		rod.nNode[ 0 ] = source.m_nNodes[ 0 ];
		rod.nNode[ 1 ] = source.m_nNodes[ 1 ];
		rod.flRelaxationFactor = source.m_flRelaxationFactor;
		CFeModelBuilder::BuildNode_t &n0 = builder.m_Nodes[ rod.nNode[ 0 ] ], &n1 = builder.m_Nodes[ rod.nNode[ 1 ] ];
		rod.flMaxDist = source.m_bExplicitLength ? source.m_flLength : ( n0.transform.m_vPosition - n1.transform.m_vPosition ).Length( );
		rod.flMinDist = source.m_flContractionFactor * rod.flMaxDist; // Trying to match source1, where min rod distance is 0, but also improve on it a bit, we don't really want cloth to ever collapse
		float sumInvMass = source.m_flMotionBias[ 0 ] * n0.invMass + source.m_flMotionBias[ 1 ] * n1.invMass;
		Assert( rod.flRelaxationFactor >= 0 && rod.flRelaxationFactor <= 1.0f );
		if ( sumInvMass > 1e-6f && rod.flRelaxationFactor >= 0.0033f ) // we need to have non-zero relaxation factor for this rod to make any sense to compute; otherwise it has no effect. And we shouldn't have any relaxation facotrs outside (0,1) interval, ever
		{
			rod.flWeight0 = source.m_flMotionBias[ 0 ] * n0.invMass / sumInvMass;
			builder.m_Rods.AddToTail( rod );
		}
	}
	for ( int nSpring = 0; nSpring < m_Springs.Count( ); ++nSpring )
	{
		CFeModelBuilder::BuildSpring_t out;
		const CSpring &in = m_Springs[ nSpring ];
		out.nNode[ 0 ] = in.m_nNodes[ 0 ];
		out.nNode[ 1 ] = in.m_nNodes[ 1 ];
		out.flSpringConstant = in.m_flSpringConstant;
		out.flSpringDamping = in.m_flSpringDamping;
		// note: Stretchiness is not implemented!
		out.flStretchiness = in.m_flStretchiness;
		builder.m_Springs.AddToTail( out );
	}
	
	builder.m_CollisionSpheres.SetCount( m_CollisionSpheres.Count( ) );
	for ( int nCollSphere = 0; nCollSphere < m_CollisionSpheres.Count( ); ++nCollSphere )
	{
		const CCollisionSphere &source = m_CollisionSpheres[ nCollSphere ];
		CFeModelBuilder::BuildCollisionSphere_t &sphere = builder.m_CollisionSpheres[ nCollSphere ];
		sphere.m_bInclusive = source.m_bInclusive;
		sphere.m_nChild = source.m_nChildBone;
		sphere.m_nParent = source.m_nParentBone;
		sphere.m_vOrigin = source.m_vOrigin;
		sphere.m_flRadius = source.m_flRadius;
		sphere.m_flStickiness = source.m_flStickiness;
	}

	builder.m_CollisionPlanes.SetCount( m_CollisionPlanes.Count( ) );
	for ( int nCollisionPlane = 0; nCollisionPlane < m_CollisionPlanes.Count( ); ++nCollisionPlane )
	{
		const CCollisionPlane &source = m_CollisionPlanes[ nCollisionPlane ];
		CFeModelBuilder::BuildCollisionPlane_t &plane = builder.m_CollisionPlanes[ nCollisionPlane ];
		plane.m_nChild = source.m_nChildBone;
		plane.m_nParent = source.m_nParentBone;
		plane.m_Plane = source.m_Plane;
		plane.m_flStickiness = source.m_flStickiness;
		AssertDbg( uint( plane.m_nChild ) < uint( m_Nodes.Count() ) && uint( plane.m_nParent ) < uint( m_Nodes.Count() ) );
	}


	if ( m_bForceWorldCollisionOnAllNodes )
	{
		for ( CFeModelBuilder::BuildNode_t &node: builder.m_Nodes )
		{
			if ( !node.bWorldCollision )
			{
				node.bWorldCollision = true;
				node.flWorldFriction = m_flDefaultWorldCollisionPenetration;
				node.flGroundFriction = m_flDefaultGroundFriction;
			}
		}
	}

	builder.m_flDefaultSurfaceStretch = m_flDefaultSurfaceStretch;
	builder.m_flDefaultThreadStretch  = m_flDefaultThreadStretch;
	builder.m_flDefaultGravityScale = m_flDefaultGravityScale;
	builder.m_flDefaultVelAirDrag = m_flDefaultVelAirDrag;
	builder.m_flDefaultExpAirDrag = m_flDefaultExpAirDrag;
	builder.m_flDefaultVelQuadAirDrag = m_flDefaultVelQuadAirDrag;
	builder.m_flDefaultExpQuadAirDrag = m_flDefaultExpQuadAirDrag;
	builder.m_flDefaultVelRodAirDrag = m_flDefaultVelRodAirDrag;
	builder.m_flDefaultExpRodAirDrag = m_flDefaultExpRodAirDrag;
	builder.m_flQuadVelocitySmoothRate = m_flQuadVelocitySmoothRate;
	builder.m_flRodVelocitySmoothRate = m_flRodVelocitySmoothRate;
	builder.m_flWindage = m_flWindage;
	builder.m_flWindDrag = m_flWindDrag;
	builder.m_nQuadVelocitySmoothIterations = Max( 0, m_nQuadVelocitySmoothIterations );
	builder.m_nRodVelocitySmoothIterations = Max( 0, m_nRodVelocitySmoothIterations );
	builder.m_flAddWorldCollisionRadius = m_flAddWorldCollisionRadius;
	builder.m_bAddStiffnessRods = m_bAddStiffnessRods;
	builder.m_bRigidEdgeHinges = m_bRigidEdgeHinges;
	builder.m_bUsePerNodeLocalForceAndRotation = m_bUsePerNodeLocalForceAndRotation;
	builder.m_flLocalForce = m_flLocalForce;
	builder.m_flLocalRotation = m_flLocalRotation;
	builder.m_flDefaultVolumetricSolveAmount = m_flVolumetricSolveAmount;

	builder.SetQuadBendTolerance( m_flQuadBendTolerance );
	builder.EnableExplicitNodeMasses( m_bExplicitMasses );
	builder.EnableUnitlessDamping( m_bUnitlessDamping );
	if ( m_bFollowTheLead )
	{
		builder.m_nDynamicNodeFlags |= FE_FLAG_ENABLE_FTL;
	}
	if ( m_bCanCollideWithWorldCapsulesAndSpheres )
	{
		builder.m_nDynamicNodeFlags |= FE_FLAG_ENABLE_WORLD_SPHERE_COLLISION | FE_FLAG_ENABLE_WORLD_CAPSULE_COLLISION;
	}
	if( m_bCanCollideWithWorldMeshes )
	{
		builder.m_nDynamicNodeFlags |= FE_FLAG_ENABLE_WORLD_MESH_COLLISION;
	}
	if ( m_bCanCollideWithWorldHulls )
	{
		builder.m_nDynamicNodeFlags |= FE_FLAG_ENABLE_WORLD_HULL_COLLISION;
	}
	if ( m_bUninertialRods )
	{
		builder.m_nDynamicNodeFlags |= FE_FLAG_UNINERTIAL_CONSTRAINTS;
	}
	builder.Finish( false, m_flAddCurvature, 0 );
	if ( builder.m_pLegacyStretchForce )
	{
		Assert( m_bUninertialRods );
	}

	CLockedResource< PhysFeModelDesc_t > pDesc;
	if ( builder.m_nNodeCount > builder.m_nStaticNodes )
	{
		pDesc = Clone( &builder, pStream );
	}
	else if ( builder.m_nNodeCount )
	{
		Log_Warning( LOG_AUTH_PHYS, FUNCTION_LINE_STRING "Degenerate softbody with %u static nodes - ignoring, because there are no dynamic nodes to simulate\n", builder.m_nStaticNodes );
	}

	return pDesc;
}




struct VectorHash_t 
{
	uint operator()( const Vector &v ) const
	{
		uint32 *p = (uint32*)&v;
		return p[0] ^ RotateBitsLeft32( p[1], 5 ) ^ RotateBitsLeft32( p[2], 10 );
	}
};


struct VectorEqual_t
{
	bool operator() ( const Vector &a, const Vector &b )const
	{
		return a.DistToSqr( b ) < 1e-12f;
	}
};


class VertexDict
{
public:
	VertexDict()
	{
		m_nVerts = 0;
	}
	int Add( const Vector &v )
	{
		UtlHashHandle_t h = m_Dict.Find( v );
		if( h == m_Dict.InvalidHandle() )
		{
			m_Dict.Insert( v, m_nVerts );
			return m_nVerts++;
		}
		else
		{
			return m_Dict.Element( h );
		}
		
	}
	uint Count()
	{
		return m_nVerts;
	}
protected:
	CUtlHashtable< Vector, uint, VectorHash_t, VectorEqual_t > m_Dict;
	uint m_nVerts;
};







const Quaternion GetRelativeRotationFromTwistAngles( const Vector &vTwist )
{
	float tx = tanf( vTwist.x * 0.5f ), ty = tanf( vTwist.y * 0.5f ), tz = tanf( vTwist.z * 0.5f ), f = ( 1 + ty * ty ) * ( 1 + tz * tz );
	Quaternion q;
	q.w = 1 / sqrtf( ( 1 + tx * tx ) * f );  // there are 2 solutions: x,y,z,w and x,y,-z,-w where w>0
	q.x = tx * q.w;
	q.y = ty / sqrtf( 1 + ty * ty );
	q.z = tz / sqrtf( f );

	return q;
}






bool IsIn( const char *pName, const CUtlVector<CUtlString,CUtlMemory< CUtlString, int > > &choice )
{
	for( int i = 0; i < choice.Count(); ++i )
	{
		if( !V_stricmp( choice[i], pName ) )
		{
			return true;
		}
	}
	return false;
}






void AppendTo( CUtlVector< CUtlString> &appendTo, const CUtlVector< CUtlString > &appendFrom )
{
	for( int i = 0; i < appendFrom.Count(); ++i )
	{
		if( appendTo.Find( appendFrom[i] ) < 0 )
		{
			appendTo.AddToTail( appendFrom[i] );
		}
	}
}

void CAuthPhysCollisionAttributes::ApplyOverride( const CAuthPhysCollisionAttributesOverride &collOverride )
{
	switch( collOverride.m_nMode )
	{
	case AUTH_PHYS_COLL_ATTR_OVERRIDE:
		{
			if( !collOverride.m_CollisionGroup.IsEmpty() )
			{
				m_CollisionGroup = collOverride.m_CollisionGroup;
			}
			m_InteractAs = collOverride.m_InteractAs;
			m_InteractWith = collOverride.m_InteractWith;
		}
		break;

	case AUTH_PHYS_COLL_ATTR_APPEND:
		{
			if( !collOverride.m_CollisionGroup.IsEmpty() )
			{
				m_CollisionGroup = collOverride.m_CollisionGroup;
			}
			AppendTo( m_InteractAs, collOverride.m_InteractAs );
			AppendTo( m_InteractWith,  collOverride.m_InteractWith );
		}
		break;
	}
}


int CAuthPhysCompileContext::ResolveCollisionAttributesIndex()
{
	for( int i = 0; i < m_CollAttrPalette.Count(); ++i )
	{
		if( m_CollAttrPalette[i] == m_DefaultCollisionAttributes )
		{
			return i;
		}
	}
	int nAdded = m_CollAttrPalette.AddToTail() ;
	m_CollAttrPalette[nAdded] = m_DefaultCollisionAttributes;
	return nAdded;
}







CLockedResource<char> CAuthPhysCompileContext::WriteString( const char *pString, uint32 *pHashOut )
{
	if ( pHashOut )
	{
		uint32 nHash = MakeStringToken( pString ).GetHashCode( );
		*pHashOut = nHash;
	}

	return FindOrWrite( pString, V_strlen( pString ) + 1 );
}



int CAuthPhysCompileContext::ResolveSurfacePropertyIndex()
{
	for( int i = 0; i < m_SurfacePropPalette.Count(); ++i )
	{
		if( m_DefaultSurfaceProperty == m_SurfacePropPalette[i] )
			return i;
	}
	int nNewEntry = m_SurfacePropPalette.AddToTail();
	m_SurfacePropPalette[nNewEntry ] = m_DefaultSurfaceProperty;
	return nNewEntry;
}


static bool IsIn( const CUtlVector< CUtlString > &left, const CUtlVector< CUtlString > &right )
{
	for( int i = 0; i < left.Count(); ++i )
	{
		bool bFound = false;
		for( int j = 0; j < right.Count(); ++j )
		{
			if( !V_stricmp( left[i], right[j] ) )
			{
				bFound = true;
				break;
			}
		}
		if( !bFound )
		{
			return false;
		}
	}
	return true;
}


static bool IsIn( const CUtlHashtable< uint32 >& leftHashes, const CUtlHashtable< uint32 >& rightHashes )
{
	FOR_EACH_HASHTABLE( leftHashes, it )
	{
		if( rightHashes.Find( leftHashes.Key( it ) ) == rightHashes.InvalidHandle() )
		{
			return false;
		}
	}
	return true;
}


bool EqualCaseInsensitive( const CUtlVector< CUtlString > &left, const CUtlVector< CUtlString > &right )
{
	if( left.Count() * right.Count() < 50 )
	{
		return IsIn( left, right ) && IsIn( right, left );
	}
	else
	{
		// scalable version
		CUtlHashtable< uint32 >leftHashes, rightHashes;
		for( int nLeftIndex = 0; nLeftIndex < left.Count(); ++nLeftIndex )
		{
			leftHashes.Insert( MakeStringToken( left[nLeftIndex] ).GetHashCode() );
		}
		for( int nRightIndex = 0; nRightIndex < right.Count(); ++nRightIndex )
		{
			rightHashes.Insert( MakeStringToken( right[nRightIndex] ).GetHashCode() );
		}
		return IsIn( leftHashes, rightHashes ) && IsIn( rightHashes, leftHashes );
	}
}

bool CAuthPhysCollisionAttributes::operator == ( const CAuthPhysCollisionAttributes & other ) const
{
	return !V_stricmp( m_CollisionGroup, other.m_CollisionGroup ) && EqualCaseInsensitive( m_InteractAs, other.m_InteractAs ) && EqualCaseInsensitive( m_InteractWith, other.m_InteractWith );
}



bool CAuthPhysFx::IsNewSpringAllowed( int nBone0, int nBone1 )
{
	if( nBone0 == nBone1 )
	{
		// no constraining a bone to itself
		return false;
	}
	if( !m_Nodes[ nBone0 ].m_bSimulated && !m_Nodes[nBone1].m_bSimulated )
	{
		// no constraining non-simulating bones
		return false; 
	}
	// also, no constraining bones that are already constrained
	// note: linear search, will get slow if we get insanely complex softbodies, but we're not likely to get there anytime soon, if ever
	for( int i = 0; i < m_Constraints.Count(); ++i )
	{
		if( m_Constraints[i].Equals( nBone0, nBone1 ) )
		{
			return false;
		}
	}
	return true;
}



bool CAuthPhysFx::IsNewRodAllowed( int nBone0, int nBone1 )
{
	if ( nBone0 == nBone1 )
	{
		// no constraining a bone to itself
		return false;
	}
	if ( !m_Nodes[ nBone0 ].m_bSimulated && !m_Nodes[ nBone1 ].m_bSimulated )
	{
		// no constraining non-simulating bones
		return false;
	}
	// also, no constraining bones that are already constrained
	// note: linear search, will get slow if we get insanely complex softbodies, but we're not likely to get there anytime soon, if ever
	for ( int i = 0; i < m_Rods.Count(); ++i )
	{
		if ( m_Rods[ i ].Equals( nBone0, nBone1 ) )
		{
			return false;
		}
	}
	return true;
}



void CAuthPhysFx::SetBones( const CUtlVector< CBone > &bones )
{
	m_Nodes.CopyArray( bones.Base(), bones.Count() );
}


int CAuthPhysFx::AddConstraint( int nBone0, int nBone1 )
{
	Assert( uint( nBone0 ) < uint( m_Nodes.Count() ) && uint( nBone1 ) < uint( m_Nodes.Count() ) );
	return m_Constraints.AddToTail( CConstraint( nBone0, nBone1 ) );
}



bool CAuthPhysFx::IsConstraintSimulated( int nConstraint ) const
{
	const CConstraint &constraint = m_Constraints[ nConstraint ];
	const CBone &bone0 = m_Nodes[ constraint.m_nBones[0] ], &bone1 = m_Nodes[ constraint.m_nBones[1] ];
	return bone0.m_bSimulated || bone1.m_bSimulated;
}

bool CAuthPhysFx::IsSpringSimulated( int nSpring ) const
{
	const CSpring &Spring = m_Springs[ nSpring ];
	const CBone &bone0 = m_Nodes[ Spring.m_nNodes[ 0 ] ], &bone1 = m_Nodes[ Spring.m_nNodes[ 1 ] ];
	return bone0.m_bSimulated || bone1.m_bSimulated;
}


void CAuthPhysFx::SortAndRemoveDuplicates( )
{
	HeapSort( m_Quads, [] ( const CAuthPhysFx::CQuad& left, const CAuthPhysFx::CQuad& right ) {
		for ( int c = 0; c < 4; ++c )
		{
			if ( left.m_nNodes[ c ] != right.m_nNodes[ c ] )
			{
				return left.m_nNodes[ c ] < right.m_nNodes[ c ];
			}
		}
		return false;
	} );
	RemoveDuplicates( m_Quads );
	HeapSort( m_Rods, [] ( const CAuthPhysFx::CRod &left, const CAuthPhysFx::CRod &right ) {
		return left < right;
	} );
	RemoveDuplicates( m_Rods );
}


static bool IsIn( int nNode, const CVarBitVec &nodes )
{
	return ( nNode >= 0 && nNode < nodes.GetNumBits( ) && nodes.IsBitSet( nNode ) );
}

void CAuthPhysFx::RemoveRodsConnecting( const CVarBitVec &nodes )
{
	for ( int nRod = 0; nRod < m_Rods.Count( );  )
	{
		const CRod &rod = m_Rods[ nRod ];
		if ( IsIn( rod.m_nNodes[ 0 ], nodes ) && IsIn( rod.m_nNodes[ 1 ], nodes ) )
		{
			m_Rods.FastRemove( nRod );
		}
		else
		{
			++nRod;
		}
	}
	
}



void CAuthPhysFx::RemoveQuadsConnecting( const CVarBitVec &nodes )
{
	for ( int nQuad = 0; nQuad < m_Quads.Count( ); )
	{
		const CQuad &quad = m_Quads[ nQuad ];
		if ( IsIn( quad.m_nNodes[ 0 ], nodes ) && IsIn( quad.m_nNodes[ 1 ], nodes ) && IsIn( quad.m_nNodes[ 2 ], nodes ) && IsIn( quad.m_nNodes[ 3 ], nodes ) )
		{
			m_Quads.FastRemove( nQuad );
		}
		else
		{
			++nQuad;
		}
	}
}



// Find connected nodes, assign each island an index, assign each node an island 
int CAuthPhysFx::BuildIslandMap( CUtlVector< int > &nodeToIsland ) const
{
	int nNodeCount = m_Nodes.Count();
	CDisjointSetForest forest( nNodeCount );
	for ( const CQuad &quad : m_Quads )
	{
		forest.Union( quad.m_nNodes[ 0 ], quad.m_nNodes[ 1 ] );
		forest.Union( quad.m_nNodes[ 0 ], quad.m_nNodes[ 2 ] );
		forest.Union( quad.m_nNodes[ 1 ], quad.m_nNodes[ 2 ] );
		if ( quad.m_nNodes[ 3 ] != quad.m_nNodes[ 2 ] )
		{
			forest.Union( quad.m_nNodes[ 0 ], quad.m_nNodes[ 3 ] );
			forest.Union( quad.m_nNodes[ 1 ], quad.m_nNodes[ 3 ] );
			forest.Union( quad.m_nNodes[ 2 ], quad.m_nNodes[ 3 ] );
		}
	}
	for ( const CRod &rod : m_Rods )
	{
		forest.Union( rod.m_nNodes[ 0 ], rod.m_nNodes[ 1 ] );
	}

	nodeToIsland.SetCount( nNodeCount );
	nodeToIsland.FillWithValue( -1 );

	int nIslandCount = 0;
	for ( int n = 0; n < nNodeCount; ++n )
	{
		int nIslandRootNode = forest.Find( n );
		int &refIsland = nodeToIsland[ nIslandRootNode ];
		if ( refIsland < 0 )
		{
			refIsland = nIslandCount++;
		}
		nodeToIsland[ n ] = refIsland;
	}
	return nIslandCount;
}



template <typename Array>
static int Cleanup( Array &arr, int nNodeCount )
{
	int nRemoved = 0;
	for ( int i = arr.Count(); i-- > 0; )
	{
		if ( !arr[ i ].IsValid( nNodeCount ) )
		{
			arr.FastRemove( i );
			++nRemoved;
		}
	}
	return nRemoved;
}


int CAuthPhysFx::Cleanup()
{
	int nRemoved = ::Cleanup( m_SphereRigids, m_Nodes.Count() );
	nRemoved += ::Cleanup( m_TaperedCapsuleRigids, m_Nodes.Count() );
	nRemoved += ::Cleanup( m_TaperedCapsuleStretches, m_Nodes.Count() );
	nRemoved += ::Cleanup( m_CtrlOffsets, m_Nodes.Count() );
	nRemoved += ::Cleanup( m_Quads, m_Nodes.Count() );
	nRemoved += ::Cleanup( m_Rods, m_Nodes.Count() );
	return nRemoved;
}



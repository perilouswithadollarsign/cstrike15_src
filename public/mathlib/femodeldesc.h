//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef FE_MODEL_DESC
#define FE_MODEL_DESC

#include "resourcefile/resourcestream.h"
#include "tier1/utlvector.h"

class CTransform;
class CFeModel;
struct FeNodeBase_t;
struct FeSimdNodeBase_t;
struct FeQuad_t;
struct FeSimdQuad_t ;
struct FeSimdTri_t ;
struct FeSimdRodConstraint_t ;
struct FeRodConstraint_t ;
struct FeAxialEdgeBend_t ;
struct FeCtrlOffset_t ;
struct FeCtrlOsOffset_t ;
struct FeFollowNode_t ;
struct FeCollisionSphere_t ;
struct FeCollisionPlane_t ;
struct FeNodeIntegrator_t;
struct FeSpringIntegrator_t ;
struct FeSimdSpringIntegrator_t ;
struct FeWorldCollisionParams_t ;
struct FeTaperedCapsuleStretch_t ;
struct FeTaperedCapsuleRigid_t ;
struct FeSphereRigid_t ;
struct FeTreeChildren_t ;
struct FeFitMatrix_t ;
struct FeSimdFitMatrices_t ;
struct FeFitWeight_t ;
struct FeNodeReverseOffset_t;

//
// On-disk structure holding Finite Element Model data
//
// When making changes to this structure, also change:
//   CFeModel - runtime reflection of this same structure, used for easy runtime changes
//   CLockedResource< PhysFeModelDesc_t > Clone( CFeModel *pFeModel, CResourceStream *pStream ) in physicsresourcehelpers.cpp
//   CVPhysics2Interface::CreateAggregateDataFromDiskData in vphysics.cpp
//   CFeModelBuilder (::Finish() ) in mathlib\femodelbuilder.cpp
//   perhaps CAuthPhysFx::Compile in mdlobjects\authphysmodel.cpp
//
struct PhysFeModelDesc_t
{
	CResourceArray< uint32 > m_CtrlHash;
	CResourceArray< CResourceString > m_CtrlName;

	uint32 m_nStaticNodeFlags;
	uint32 m_nDynamicNodeFlags;
	float32 m_flLocalForce;
	float32 m_flLocalRotation;
	uint16 m_nNodeCount;
	uint16 m_nStaticNodes;
	uint16 m_nRotLockStaticNodes;
	uint16 m_nSimdTriCount1;
	uint16 m_nSimdTriCount2;
	uint16 m_nSimdQuadCount1;
	uint16 m_nSimdQuadCount2;
	uint16 m_nQuadCount1;
	uint16 m_nQuadCount2;
	uint16 m_nCollisionSphereInclusiveCount;
	uint16 m_nTreeDepth;
	uint16 m_nFitMatrixCount1;
	uint16 m_nFitMatrixCount2;
	uint16 m_nSimdFitMatrixCount1;
	uint16 m_nSimdFitMatrixCount2;

	uint16 m_nRopeCount;
	CResourceArray< uint16 > m_Ropes; // first, there's the "end" indices of each rope (1st rope "begin" is assumed to be == m_nRopeCount ). Then, there are ropes: with indices from the parent/anchor (ground truth w.r.t. orientation ) to child
	CResourceArray< FeNodeBase_t > m_NodeBases;
	CResourceArray< FeSimdNodeBase_t > m_SimdNodeBases;
	
	CResourceArray< FeQuad_t > m_Quads;
	CResourceArray< FeSimdQuad_t > m_SimdQuads;
	CResourceArray< FeSimdTri_t > m_SimdTris;
	CResourceArray< FeSimdRodConstraint_t > m_SimdRods;
	CResourceArray< CTransform > m_InitPose;
	CResourceArray< FeRodConstraint_t > m_Rods;
	CResourceArray< FeAxialEdgeBend_t > m_AxialEdges;
	CResourceArray< float32 > m_NodeInvMasses;
	CResourceArray< FeCtrlOffset_t > m_CtrlOffsets;
	CResourceArray< FeCtrlOsOffset_t > m_CtrlOsOffsets;
	CResourceArray< FeFollowNode_t > m_FollowNodes;
	CResourceArray< FeCollisionSphere_t > m_CollisionSpheres;
	CResourceArray< FeCollisionPlane_t > m_CollisionPlanes;

	// either 0 elements (implying 0 damping for all nodes), or m_nNodeCount elements;
	// static nodes are damped as they come in from animation,
	// dynamic nodes as they simulate; 
	// damping is multiplie	d by the time step (in Dota, they are not)
	CResourceArray< FeNodeIntegrator_t > m_NodeIntegrator; 

	// this is to simulate spring forces (acceleration level) with the verlet integrator: it gets applied as a separte step, just adding a*t^2 to the corresponding nodes
	// if nodes have different damping, it needs to be figured out in the weight here. If damping is not 1.0, it needs to be premultiplied in both the constant and damping
	CResourceArray< FeSpringIntegrator_t > m_SpringIntegrator;
	CResourceArray< FeSimdSpringIntegrator_t > m_SimdSpringIntegrator;
	
	CResourceArray< FeWorldCollisionParams_t > m_WorldCollisionParams;
	CResourceArray< float > m_LegacyStretchForce;
	CResourceArray< float > m_NodeCollisionRadii;
	CResourceArray< float > m_LocalRotation;
	CResourceArray< float > m_LocalForce;
	CResourceArray< FeTaperedCapsuleStretch_t > m_TaperedCapsuleStretches;
	CResourceArray< FeTaperedCapsuleRigid_t > m_TaperedCapsuleRigids;
	CResourceArray< FeSphereRigid_t > m_SphereRigids;
	CResourceArray< uint16 > m_WorldCollisionNodes;
	CResourceArray< uint16 > m_TreeParents;
	CResourceArray< uint16 > m_TreeCollisionMasks;
	CResourceArray< FeTreeChildren_t > m_TreeChildren;
	CResourceArray< uint16 > m_FreeNodes;
	CResourceArray< FeFitMatrix_t > m_FitMatrices;
	CResourceArray< FeSimdFitMatrices_t > m_SimdFitMatrices;
	CResourceArray< FeFitWeight_t > m_FitWeights;
	CResourceArray< FeNodeReverseOffset_t > m_ReverseOffsets;
	uint32 m_nReserved[ 30 ];

	float32 m_flWindage;
	float32 m_flWindDrag;
	float32 m_flDefaultSurfaceStretch;
	float32 m_flDefaultThreadStretch;
	float32 m_flDefaultGravityScale;
	float32 m_flDefaultVelAirDrag;
	float32 m_flDefaultExpAirDrag;
	float32 m_flDefaultVelQuadAirDrag;
	float32 m_flDefaultExpQuadAirDrag;
	float32 m_flDefaultVelRodAirDrag;
	float32 m_flDefaultExpRodAirDrag;
	float32 m_flRodVelocitySmoothRate;
	float32 m_flQuadVelocitySmoothRate;
	float32 m_flAddWorldCollisionRadius;
	float32 m_flDefaultVolumetricSolveAmount;
	uint16 m_nRodVelocitySmoothIterations;
	uint16 m_nQuadVelocitySmoothIterations;

	uint GetNodeCount()const { return m_InitPose.Count(); }
	uint GetDynamicNodeCount() const { return m_nNodeCount - m_nStaticNodes; }
};

CLockedResource< PhysFeModelDesc_t > Clone( CFeModel *pFeModel, CResourceStream *pStream );
void Clone( const PhysFeModelDesc_t *pFeDesc, intp nOffsetBytes, char **pCtrlNames, CFeModel *pFeModel );


class CFeModel;
class CFeModelReplaceContext
{
public:
	CFeModelReplaceContext( const CFeModel *pOld, const CFeModel *pNew );
	const CFeModel *GetOld() { return m_pOld; }
	const CFeModel *GetNew() { return m_pNew; }
	int OldToNewNode( int i ) { return m_OldToNewNode[ i ]; }
	int NewToOldNode( int i ){ return m_NewToOldNode[ i ]; }
	int OldToNewCtrl( int i ) { return m_OldToNewCtrl[ i ]; }
	int NewToOldCtrl( int i ){ return m_NewToOldCtrl[ i ]; }
protected:
	const CFeModel *m_pOld;
	const CFeModel *m_pNew;
	CUtlVector< int > m_OldToNewNode, m_NewToOldNode;
	CUtlVector< int > m_OldToNewCtrl, m_NewToOldCtrl;
};


#endif

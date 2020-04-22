//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "mathlib/femodeldesc.h"
#include "mathlib/femodel.h"
#include "tier1/heapsort.h"
#include "tier1/fmtstr.h"

template < typename T >
inline CLockedResource< T > CloneArrayWithMarkers( CResourceStream *pStream, const T *pArray, uint nCount, const char *pName )
{
	// create the marker for the start of the array
	CFmtStr beginMsg( "Begin of %s, %d byte aligned, here: <", pName, VALIGNOF( T ) );
	// align the marker so that it ends aligned, right before the array data
	int nBeginMsgLength = beginMsg.Length( );
	int nPreAlign = ( -int( pStream->GetTotalSize() + nBeginMsgLength ) ) & ( VALIGNOF( T ) - 1 );
	V_memset( pStream->AllocateBytes( nPreAlign ), '-', nPreAlign );
	void *pPrefixData = pStream->AllocateBytes( nBeginMsgLength );
	Assert( !( ( uintp( pPrefixData ) + nBeginMsgLength ) & ( VALIGNOF( T ) - 1 ) ) );
	V_memcpy( pPrefixData, beginMsg.Get(), nBeginMsgLength );
	// write out the array
	CLockedResource< T > result = CloneArray( pStream, pArray, nCount );
	// add the end marker
	pStream->WriteString( CFmtStr( "> End of %s, %d bytes total.", pName, nCount * sizeof( T ) ) );
	return result;
}

#if 0//def _DEBUG
#define CloneArray( STREAM, ARRAY, COUNT ) CloneArrayWithMarkers( (STREAM), (ARRAY), (COUNT), #ARRAY ); 
#endif



CLockedResource< PhysFeModelDesc_t > Clone( CFeModel *pFeModel, CResourceStream *pStream )
{
	CLockedResource< PhysFeModelDesc_t > pFx = pStream->Allocate< PhysFeModelDesc_t >( );
	uint nDynamicNodes = pFeModel->m_nNodeCount - pFeModel->m_nStaticNodes;

	pFx->m_flLocalForce = pFeModel->m_flLocalForce;
	pFx->m_flLocalRotation   = pFeModel->m_flLocalRotation;

	pFx->m_nStaticNodeFlags  = pFeModel->m_nStaticNodeFlags;
	pFx->m_nDynamicNodeFlags = pFeModel->m_nDynamicNodeFlags;
	pFx->m_nNodeCount      = pFeModel->m_nNodeCount;
	pFx->m_nStaticNodes    = pFeModel->m_nStaticNodes;
	pFx->m_nRotLockStaticNodes = pFeModel->m_nRotLockStaticNodes;
	pFx->m_nSimdTriCount1  = pFeModel->m_nSimdTriCount[ 1 ];
	pFx->m_nSimdTriCount2  = pFeModel->m_nSimdTriCount[ 2 ];
	pFx->m_nSimdQuadCount1 = pFeModel->m_nSimdQuadCount[ 1 ];
	pFx->m_nSimdQuadCount2 = pFeModel->m_nSimdQuadCount[ 2 ];
	pFx->m_nQuadCount1     = pFeModel->m_nQuadCount[ 1 ];
	pFx->m_nQuadCount2     = pFeModel->m_nQuadCount[ 2 ];
	pFx->m_nFitMatrixCount1 = pFeModel->m_nFitMatrixCount[ 1 ];
	pFx->m_nFitMatrixCount2 = pFeModel->m_nFitMatrixCount[ 2 ];
	pFx->m_nSimdFitMatrixCount1 = pFeModel->m_nSimdFitMatrixCount[ 1 ];
	pFx->m_nSimdFitMatrixCount2 = pFeModel->m_nSimdFitMatrixCount[ 2 ];
	pFx->m_nRopeCount = pFeModel->m_nRopeCount;
	pFx->m_nTreeDepth      = pFeModel->m_nTreeDepth;
	pFx->m_flDefaultSurfaceStretch = pFeModel->m_flDefaultSurfaceStretch;
	pFx->m_flDefaultThreadStretch  = pFeModel->m_flDefaultThreadStretch;

	pFx->m_flDefaultGravityScale = pFeModel->m_flDefaultGravityScale;
	pFx->m_flDefaultVelAirDrag = pFeModel->m_flDefaultVelAirDrag;
	pFx->m_flDefaultExpAirDrag = pFeModel->m_flDefaultExpAirDrag;
	pFx->m_flDefaultVelQuadAirDrag = pFeModel->m_flDefaultVelQuadAirDrag;
	pFx->m_flDefaultExpQuadAirDrag = pFeModel->m_flDefaultExpQuadAirDrag;
	pFx->m_flDefaultVelRodAirDrag = pFeModel->m_flDefaultVelRodAirDrag;
	pFx->m_flDefaultExpRodAirDrag = pFeModel->m_flDefaultExpRodAirDrag;
	pFx->m_flQuadVelocitySmoothRate = pFeModel->m_flQuadVelocitySmoothRate;
	pFx->m_flRodVelocitySmoothRate = pFeModel->m_flRodVelocitySmoothRate;
	pFx->m_flAddWorldCollisionRadius = pFeModel->m_flAddWorldCollisionRadius;
	pFx->m_nQuadVelocitySmoothIterations = pFeModel->m_nQuadVelocitySmoothIterations;
	pFx->m_nRodVelocitySmoothIterations = pFeModel->m_nRodVelocitySmoothIterations;
	pFx->m_flDefaultVolumetricSolveAmount = pFeModel->m_flDefaultVolumetricSolveAmount;
	pFx->m_flWindage = pFeModel->m_flWindage;
	pFx->m_flWindDrag = pFeModel->m_flWindDrag;

	pFx->m_SimdQuads     = CloneArray( pStream, pFeModel->m_pSimdQuads, pFeModel->m_nSimdQuadCount[ 0 ] );
	pFx->m_SimdTris      = CloneArray( pStream, pFeModel->m_pSimdTris, pFeModel->m_nSimdTriCount[ 0 ] );
	pFx->m_SimdRods      = CloneArray( pStream, pFeModel->m_pSimdRods, pFeModel->m_nSimdRodCount );
	pFx->m_SimdNodeBases = CloneArray( pStream, pFeModel->m_pSimdNodeBases, pFeModel->m_nSimdNodeBaseCount );
	pFx->m_SimdFitMatrices = CloneArray( pStream, pFeModel->m_pSimdFitMatrices, pFeModel->m_nSimdFitMatrixCount[ 0 ] );
	pFx->m_FitMatrices = CloneArray( pStream, pFeModel->m_pFitMatrices, pFeModel->m_nFitMatrixCount[ 0 ] );
	pFx->m_Quads = CloneArray( pStream, pFeModel->m_pQuads, pFeModel->m_nQuadCount[ 0 ] );
	pFx->m_CtrlOffsets   = CloneArray( pStream, pFeModel->m_pCtrlOffsets, pFeModel->m_nCtrlOffsets );
	pFx->m_CtrlOsOffsets = CloneArray( pStream, pFeModel->m_pCtrlOsOffsets, pFeModel->m_nCtrlOsOffsets );
	pFx->m_Rods          = CloneArray( pStream, pFeModel->m_pRods, pFeModel->m_nRodCount );
	pFx->m_AxialEdges	 = CloneArray( pStream, pFeModel->m_pAxialEdges, pFeModel->m_nAxialEdgeCount );
	pFx->m_Ropes         = CloneArray( pStream, pFeModel->m_pRopes, pFeModel->m_nRopeIndexCount );
	pFx->m_NodeBases     = CloneArray( pStream, pFeModel->m_pNodeBases, pFeModel->m_nNodeBaseCount );
	pFx->m_SpringIntegrator      = CloneArray( pStream, pFeModel->m_pSpringIntegrator, pFeModel->m_nSpringIntegratorCount );
	pFx->m_SimdSpringIntegrator  = CloneArray( pStream, pFeModel->m_pSimdSpringIntegrator, pFeModel->m_nSimdSpringIntegratorCount );
	pFx->m_InitPose		 = CloneArray( pStream, pFeModel->m_pInitPose, pFeModel->m_nCtrlCount );
	pFx->m_FollowNodes   = CloneArray( pStream, pFeModel->m_pFollowNodes, pFeModel->m_nFollowNodeCount ); 
	pFx->m_CollisionSpheres = CloneArray( pStream, pFeModel->m_pCollisionSpheres, pFeModel->m_nCollisionSpheres[ 0 ] );
	pFx->m_CollisionPlanes = CloneArray( pStream, pFeModel->m_pCollisionPlanes, pFeModel->m_nCollisionPlanes );
	pFx->m_NodeCollisionRadii = CloneArray( pStream, pFeModel->m_pNodeCollisionRadii, nDynamicNodes );
	pFx->m_LocalRotation = CloneArray( pStream, pFeModel->m_pLocalRotation, nDynamicNodes );
	pFx->m_LocalForce = CloneArray( pStream, pFeModel->m_pLocalForce, nDynamicNodes );
	pFx->m_FitWeights = CloneArray( pStream, pFeModel->m_pFitWeights, pFeModel->m_nFitWeightCount );
	pFx->m_nCollisionSphereInclusiveCount = pFeModel->m_nCollisionSpheres[ 1 ];
	pFx->m_WorldCollisionParams = CloneArray( pStream, pFeModel->m_pWorldCollisionParams, pFeModel->m_nWorldCollisionParamCount );
	pFx->m_TaperedCapsuleStretches = CloneArray( pStream, pFeModel->m_pTaperedCapsuleStretches, pFeModel->m_nTaperedCapsuleStretchCount );
	pFx->m_TaperedCapsuleRigids = CloneArray( pStream, pFeModel->m_pTaperedCapsuleRigids, pFeModel->m_nTaperedCapsuleRigidCount );
	pFx->m_SphereRigids = CloneArray( pStream, pFeModel->m_pSphereRigids, pFeModel->m_nSphereRigidCount );
	pFx->m_TreeChildren = CloneArray( pStream, pFeModel->m_pTreeChildren, nDynamicNodes - 1 );
	pFx->m_TreeParents = CloneArray( pStream, pFeModel->m_pTreeParents, nDynamicNodes + nDynamicNodes - 1 );
	pFx->m_TreeCollisionMasks = CloneArray( pStream, pFeModel->m_pTreeCollisionMasks, nDynamicNodes + nDynamicNodes - 1 );
	pFx->m_WorldCollisionNodes = CloneArray( pStream, pFeModel->m_pWorldCollisionNodes, pFeModel->m_nWorldCollisionNodeCount );
	pFx->m_FreeNodes = CloneArray( pStream, pFeModel->m_pFreeNodes, pFeModel->m_nFreeNodeCount );
	pFx->m_ReverseOffsets = CloneArray( pStream, pFeModel->m_pReverseOffsets, pFeModel->m_nReverseOffsetCount );

	if ( pFeModel->m_pLegacyStretchForce )
	{
		pFx->m_LegacyStretchForce = CloneArray( pStream, pFeModel->m_pLegacyStretchForce, pFeModel->m_nNodeCount );
	}
	if ( pFeModel->m_pNodeIntegrator )
	{
		pFx->m_NodeIntegrator = CloneArray( pStream, pFeModel->m_pNodeIntegrator, pFeModel->m_nNodeCount );
	}
	pFx->m_NodeInvMasses = CloneArray( pStream, pFeModel->m_pNodeInvMasses, pFeModel->m_nNodeCount );

	if ( pFeModel->m_pCtrlHash )
	{
		pFx->m_CtrlHash = CloneArray( pStream, pFeModel->m_pCtrlHash, pFeModel->m_nCtrlCount );
	}
	if ( pFeModel->m_pCtrlName )
	{
		pFx->m_CtrlName = pStream->Allocate< CResourceString >( pFeModel->m_nCtrlCount );
		for ( uint i = 0; i < pFeModel->m_nCtrlCount; ++i )
		{
			pFx->m_CtrlName[ i ] = pStream->WriteString( pFeModel->m_pCtrlName[ i ] );
		}
	}

	return pFx;
}




void Clone( const PhysFeModelDesc_t *pFeDesc, intp nOffsetBytes, char **pCtrlNames, CFeModel *pFeModel )
{
	pFeModel->m_nDynamicNodeFlags = pFeDesc->m_nDynamicNodeFlags;
	pFeModel->m_nStaticNodeFlags = pFeDesc->m_nStaticNodeFlags;
	pFeModel->m_flLocalForce = pFeDesc->m_flLocalForce;
	pFeModel->m_flLocalRotation = pFeDesc->m_flLocalRotation;

	pFeModel->m_nAxialEdgeCount = pFeDesc->m_AxialEdges.Count();
	pFeModel->m_nCtrlCount = pFeDesc->m_CtrlHash.Count();
	pFeModel->m_nNodeCount = pFeDesc->m_nNodeCount;
	pFeModel->m_nStaticNodes = pFeDesc->m_nStaticNodes;
	pFeModel->m_nRotLockStaticNodes = pFeDesc->m_nRotLockStaticNodes;
	AssertDbg( pFeModel->m_nRotLockStaticNodes <= pFeModel->m_nStaticNodes );
	pFeModel->m_nTreeDepth = pFeDesc->m_nTreeDepth;
	// no scalar data
	pFeModel->m_nQuadCount[ 0 ] = 0;
	pFeModel->m_nQuadCount[ 1 ] = 0;
	pFeModel->m_nQuadCount[ 2 ] = 0;
	pFeModel->m_nTriCount[ 0 ] = 0;
	pFeModel->m_nTriCount[ 1 ] = 0;
	pFeModel->m_nTriCount[ 2 ] = 0;
	pFeModel->m_nSimdQuadCount[ 0 ] = pFeDesc->m_SimdQuads.Count();
	pFeModel->m_nSimdQuadCount[ 1 ] = pFeDesc->m_nSimdQuadCount1;
	pFeModel->m_nSimdQuadCount[ 2 ] = pFeDesc->m_nSimdQuadCount2;
	pFeModel->m_nSimdTriCount[ 0 ] = pFeDesc->m_SimdTris.Count();
	pFeModel->m_nSimdTriCount[ 1 ] = pFeDesc->m_nSimdTriCount1;
	pFeModel->m_nSimdTriCount[ 2 ] = pFeDesc->m_nSimdTriCount2;
	pFeModel->m_nQuadCount[ 0 ] = pFeDesc->m_Quads.Count();
	pFeModel->m_nQuadCount[ 1 ] = pFeDesc->m_nQuadCount1;
	pFeModel->m_nQuadCount[ 2 ] = pFeDesc->m_nQuadCount2;
	pFeModel->m_nFitMatrixCount[ 0 ] = pFeDesc->m_FitMatrices.Count();
	pFeModel->m_nFitMatrixCount[ 1 ] = pFeDesc->m_nFitMatrixCount1;
	pFeModel->m_nFitMatrixCount[ 2 ] = pFeDesc->m_nFitMatrixCount2;
	pFeModel->m_nSimdFitMatrixCount[ 0 ] = pFeDesc->m_SimdFitMatrices.Count();
	pFeModel->m_nSimdFitMatrixCount[ 1 ] = pFeDesc->m_nSimdFitMatrixCount1;
	pFeModel->m_nSimdFitMatrixCount[ 2 ] = pFeDesc->m_nSimdFitMatrixCount2;
	pFeModel->m_flDefaultSurfaceStretch = pFeDesc->m_flDefaultSurfaceStretch;
	pFeModel->m_flDefaultThreadStretch = pFeDesc->m_flDefaultThreadStretch;
	pFeModel->m_flDefaultGravityScale = pFeDesc->m_flDefaultGravityScale;
	pFeModel->m_flDefaultVelAirDrag = pFeDesc->m_flDefaultVelAirDrag;
	pFeModel->m_flDefaultExpAirDrag = pFeDesc->m_flDefaultExpAirDrag;
	pFeModel->m_flDefaultVelQuadAirDrag = pFeDesc->m_flDefaultVelQuadAirDrag;
	pFeModel->m_flDefaultExpQuadAirDrag = pFeDesc->m_flDefaultExpQuadAirDrag;
	pFeModel->m_flDefaultVelRodAirDrag = pFeDesc->m_flDefaultVelRodAirDrag;
	pFeModel->m_flDefaultExpRodAirDrag = pFeDesc->m_flDefaultExpRodAirDrag;
	pFeModel->m_flQuadVelocitySmoothRate = pFeDesc->m_flQuadVelocitySmoothRate;
	pFeModel->m_flRodVelocitySmoothRate = pFeDesc->m_flRodVelocitySmoothRate;
	pFeModel->m_nQuadVelocitySmoothIterations = pFeDesc->m_nQuadVelocitySmoothIterations;
	pFeModel->m_nRodVelocitySmoothIterations = pFeDesc->m_nRodVelocitySmoothIterations;
	pFeModel->m_flAddWorldCollisionRadius = pFeDesc->m_flAddWorldCollisionRadius;
	pFeModel->m_flDefaultVolumetricSolveAmount = pFeDesc->m_flDefaultVolumetricSolveAmount;
	pFeModel->m_nFitWeightCount = pFeDesc->m_FitWeights.Count();
	pFeModel->m_nReverseOffsetCount = pFeDesc->m_ReverseOffsets.Count();
	pFeModel->m_flWindage = pFeDesc->m_flWindage;
	pFeModel->m_flWindDrag = pFeDesc->m_flWindDrag;

	pFeModel->m_nRodCount = pFeDesc->m_Rods.Count();
	pFeModel->m_nSimdRodCount = pFeDesc->m_SimdRods.Count();
	pFeModel->m_nFollowNodeCount = pFeDesc->m_FollowNodes.Count();
	pFeModel->m_nCtrlOffsets = pFeDesc->m_CtrlOffsets.Count();
	pFeModel->m_nCtrlOsOffsets = pFeDesc->m_CtrlOsOffsets.Count();
	pFeModel->m_nSpringIntegratorCount = pFeDesc->m_SpringIntegrator.Count();
	pFeModel->m_nSimdSpringIntegratorCount = pFeDesc->m_SimdSpringIntegrator.Count();
	pFeModel->m_nWorldCollisionParamCount = pFeDesc->m_WorldCollisionParams.Count();
	pFeModel->m_nWorldCollisionNodeCount = pFeDesc->m_WorldCollisionNodes.Count();
	pFeModel->m_nFreeNodeCount = pFeDesc->m_FreeNodes.Count();
	pFeModel->m_nTaperedCapsuleStretchCount = pFeDesc->m_TaperedCapsuleStretches.Count();
	pFeModel->m_nTaperedCapsuleRigidCount = pFeDesc->m_TaperedCapsuleRigids.Count();
	pFeModel->m_nSphereRigidCount = pFeDesc->m_SphereRigids.Count();
	pFeModel->m_pSimdQuads = ConstCastOffsetPointer( pFeDesc->m_SimdQuads.Base(), nOffsetBytes );
	pFeModel->m_pQuads = ConstCastOffsetPointer( pFeDesc->m_Quads.Base(), nOffsetBytes );
	pFeModel->m_pSimdTris = ConstCastOffsetPointer( pFeDesc->m_SimdTris.Base(), nOffsetBytes );
	pFeModel->m_pTris = NULL;
	pFeModel->m_pRods = ConstCastOffsetPointer( pFeDesc->m_Rods.Base(), nOffsetBytes );;
	pFeModel->m_pSimdRods = ConstCastOffsetPointer( pFeDesc->m_SimdRods.Base(), nOffsetBytes );
	pFeModel->m_pAxialEdges = ConstCastOffsetPointer( pFeDesc->m_AxialEdges.Base(), nOffsetBytes );
	pFeModel->m_pNodeToCtrl = NULL;
	pFeModel->m_pCtrlToNode = NULL;
	pFeModel->m_pCtrlHash = ConstCastOffsetPointer( pFeDesc->m_CtrlHash.Base(), nOffsetBytes );
	pFeModel->m_pRopes = ConstCastOffsetPointer( pFeDesc->m_Ropes.Base(), nOffsetBytes );
	pFeModel->m_pNodeBases = ConstCastOffsetPointer( pFeDesc->m_NodeBases.Base(), nOffsetBytes );
	pFeModel->m_pSimdNodeBases = ConstCastOffsetPointer( pFeDesc->m_SimdNodeBases.Base(), nOffsetBytes );
	pFeModel->m_pNodeIntegrator = ConstCastOffsetPointer( pFeDesc->m_NodeIntegrator.Base(), nOffsetBytes );
	pFeModel->m_pSpringIntegrator = ConstCastOffsetPointer( pFeDesc->m_SpringIntegrator.Base(), nOffsetBytes );
	pFeModel->m_pSimdSpringIntegrator = ConstCastOffsetPointer( pFeDesc->m_SimdSpringIntegrator.Base(), nOffsetBytes );
	pFeModel->m_pCtrlOffsets = ConstCastOffsetPointer( pFeDesc->m_CtrlOffsets.Base(), nOffsetBytes );
	pFeModel->m_pCtrlOsOffsets = ConstCastOffsetPointer( pFeDesc->m_CtrlOsOffsets.Base(), nOffsetBytes );
	pFeModel->m_pFollowNodes = ConstCastOffsetPointer( pFeDesc->m_FollowNodes.Base(), nOffsetBytes );
	pFeModel->m_pNodeCollisionRadii = ConstCastOffsetPointer( pFeDesc->m_NodeCollisionRadii.Base(), nOffsetBytes );
	pFeModel->m_pLocalRotation = ConstCastOffsetPointer( pFeDesc->m_LocalRotation.Base(), nOffsetBytes );
	pFeModel->m_pLocalForce = ConstCastOffsetPointer( pFeDesc->m_LocalForce.Base(), nOffsetBytes );
	pFeModel->m_pCollisionSpheres = ConstCastOffsetPointer( pFeDesc->m_CollisionSpheres.Base(), nOffsetBytes );
	pFeModel->m_pCollisionPlanes = ConstCastOffsetPointer( pFeDesc->m_CollisionPlanes.Base(), nOffsetBytes );
	pFeModel->m_pWorldCollisionNodes = ConstCastOffsetPointer( pFeDesc->m_WorldCollisionNodes.Base(), nOffsetBytes );
	pFeModel->m_pWorldCollisionParams = ConstCastOffsetPointer( pFeDesc->m_WorldCollisionParams.Base(), nOffsetBytes );
	pFeModel->m_pLegacyStretchForce = ConstCastOffsetPointer( pFeDesc->m_LegacyStretchForce.Base(), nOffsetBytes );
	pFeModel->m_pTaperedCapsuleStretches = ConstCastOffsetPointer( pFeDesc->m_TaperedCapsuleStretches.Base(), nOffsetBytes );
	pFeModel->m_pTaperedCapsuleRigids = ConstCastOffsetPointer( pFeDesc->m_TaperedCapsuleRigids.Base(), nOffsetBytes );
	pFeModel->m_pSphereRigids = ConstCastOffsetPointer( pFeDesc->m_SphereRigids.Base(), nOffsetBytes );
	pFeModel->m_pFreeNodes = ConstCastOffsetPointer( pFeDesc->m_FreeNodes.Base(), nOffsetBytes );
	pFeModel->m_pFitMatrices = ConstCastOffsetPointer( pFeDesc->m_FitMatrices.Base(), nOffsetBytes );
	pFeModel->m_pSimdFitMatrices = ConstCastOffsetPointer( pFeDesc->m_SimdFitMatrices.Base(), nOffsetBytes );
	pFeModel->m_pFitWeights = ConstCastOffsetPointer( pFeDesc->m_FitWeights.Base(), nOffsetBytes );
	pFeModel->m_pReverseOffsets = ConstCastOffsetPointer( pFeDesc->m_ReverseOffsets.Base(), nOffsetBytes );
	AssertDbg( pFeModel->m_pWorldCollisionParams ? pFeModel->m_pWorldCollisionParams[ pFeModel->m_nWorldCollisionParamCount - 1 ].nListEnd == pFeModel->m_nWorldCollisionNodeCount : !pFeModel->m_pWorldCollisionNodes && !pFeModel->m_nWorldCollisionParamCount && !pFeModel->m_nWorldCollisionNodeCount );
	pFeModel->m_nRopeCount = pFeDesc->m_nRopeCount;
	pFeModel->m_nRopeIndexCount = pFeDesc->m_Ropes.Count();
	pFeModel->m_nNodeBaseCount = pFeDesc->m_NodeBases.Count();
	pFeModel->m_nSimdNodeBaseCount = pFeDesc->m_SimdNodeBases.Count();
	pFeModel->m_nCollisionSpheres[ 0 ] = pFeDesc->m_CollisionSpheres.Count();
	pFeModel->m_nCollisionSpheres[ 1 ] = pFeDesc->m_nCollisionSphereInclusiveCount;
	pFeModel->m_nCollisionPlanes = pFeDesc->m_CollisionPlanes.Count();
	Assert( pFeDesc->m_TreeChildren.Count() == 0 || pFeDesc->m_TreeChildren.Count() == ( int )pFeDesc->GetDynamicNodeCount() - 1 );
	pFeModel->m_pTreeChildren = ConstCastOffsetPointer( pFeDesc->m_TreeChildren.Base(), nOffsetBytes );
	Assert( pFeDesc->m_TreeParents.Count() == 0 || pFeDesc->m_TreeParents.Count() == 2 * ( int )pFeDesc->GetDynamicNodeCount() - 1 );
	pFeModel->m_pTreeParents = ConstCastOffsetPointer( pFeDesc->m_TreeParents.Base(), nOffsetBytes );
	Assert( pFeDesc->m_TreeParents.Count() == pFeDesc->m_TreeCollisionMasks.Count() );
	pFeModel->m_pTreeCollisionMasks = ConstCastOffsetPointer( pFeDesc->m_TreeCollisionMasks.Base(), nOffsetBytes );
	Assert( pFeDesc->m_NodeInvMasses.Count() == 0 || pFeDesc->m_NodeInvMasses.Count() == pFeDesc->m_nNodeCount );
	pFeModel->m_pNodeInvMasses = pFeDesc->m_NodeInvMasses.Count() ? ConstCastOffsetPointer( pFeDesc->m_NodeInvMasses.Base(), nOffsetBytes ) : NULL;
	if ( pFeDesc->m_CtrlName.IsEmpty() || !pCtrlNames )
	{
		pFeModel->m_pCtrlName = NULL;
	}
	else
	{
		pFeModel->m_pCtrlName = const_cast< const char ** >( pCtrlNames ); //ConstCastOffsetPointer( pFeDesc->m_CtrlName.Base( ), nOffsetBytes );
		for ( int i = 0; i < pFeModel->m_nCtrlCount; ++i )
		{
			const CResourceString &name = pFeDesc->m_CtrlName[ i ];
			if ( name.IsNull() )
			{
				pFeModel->m_pCtrlName[ i ] = "";
			}
			else
			{
				pFeModel->m_pCtrlName[ i ] = ConstCastOffsetPointer( name.GetPtr(), nOffsetBytes );
			}
		}
	}
	AssertDbg( pFeDesc->m_InitPose.Count() == pFeModel->m_nCtrlCount );
	pFeModel->m_pInitPose = ConstCastOffsetPointer( pFeDesc->m_InitPose.Base(), nOffsetBytes );
}

void SetIdentityPerm( CUtlVector< uint > &perm, uint nCount )
{
	perm.SetCount( nCount );
	for ( uint i = 0; i < nCount; ++i )
		perm[ i ] = i;
}


struct CtrlHashFunctor_t
{
	const CFeModel *m_pFeModel;
	CtrlHashFunctor_t( const CFeModel *pFeModel ) : m_pFeModel( pFeModel ){}
	bool operator( )( int nLeft, int nRight ) const
	{
		return m_pFeModel->m_pCtrlHash[ nLeft ] < m_pFeModel->m_pCtrlHash[ nRight ];
	}
};


CFeModelReplaceContext::CFeModelReplaceContext( const CFeModel *pOld, const CFeModel *pNew )
{
	m_pOld = pOld;
	m_pNew = pNew;
	m_OldToNewNode.SetCount( pOld->m_nNodeCount ); m_OldToNewNode.FillWithValue( -1 );
	m_OldToNewCtrl.SetCount( pOld->m_nCtrlCount ); m_OldToNewCtrl.FillWithValue( -1 );
	m_NewToOldNode.SetCount( pNew->m_nNodeCount ); m_NewToOldNode.FillWithValue( -1 );
	m_NewToOldCtrl.SetCount( pNew->m_nCtrlCount ); m_NewToOldCtrl.FillWithValue( -1 );

	CUtlVector< uint > oldCtrlIndex, newCtrlIndex;
	SetIdentityPerm( oldCtrlIndex, pOld->m_nCtrlCount );
	SetIdentityPerm( newCtrlIndex, pNew->m_nCtrlCount );
	HeapSort( oldCtrlIndex, CtrlHashFunctor_t( pOld ) );
	HeapSort( newCtrlIndex, CtrlHashFunctor_t( pNew ) );

	for ( uint nOld = 0, nNew = 0; nOld < pOld->m_nCtrlCount && nNew < pNew->m_nCtrlCount; )
	{
		uint nOldCtrl = oldCtrlIndex[ nOld ], nNewCtrl = newCtrlIndex[ nNew ];
		uint nOldCtrlHash = pOld->m_pCtrlHash[ nOldCtrl ], nNewCtrlHash = pNew->m_pCtrlHash[ nNewCtrl ];
		if ( nOldCtrlHash == nNewCtrlHash )
		{
			// we found a match!
			m_OldToNewCtrl[ nOldCtrl ] = nNewCtrl;
			m_NewToOldCtrl[ nNewCtrl ] = nOldCtrl;
			uint nOldNode = pOld->CtrlToNode( nOldCtrl );
			uint nNewNode = pNew->CtrlToNode( nNewCtrl );
			if ( nOldNode < pOld->m_nNodeCount && nNewNode < pNew->m_nNodeCount )
			{
				// there's a match in nodes
				m_NewToOldNode[ nNewNode ] = nOldNode;
				m_OldToNewNode[ nOldNode ] = nNewNode;
			}
			nOld++;
			nNew++;
		}
		else if ( nOldCtrlHash < nNewCtrlHash )
		{
			AssertDbg( m_OldToNewCtrl[ nOldCtrl ] == -1 );
			nOld++;
		}
		else
		{
			AssertDbg( m_NewToOldCtrl[ nNewCtrl ] == -1 );
			nNew++;
		}
	}
}



//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Routines used for various prodcedural bones but meant to be called from
// datamodel or maya as well
//
// In a separate source file so linking bonesetup.lib doesn't get more than
// needed
//
//===============================================================================


// Valve includes
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier1/strtools.h"
#include "bone_setup.h"
#include "bone_constraints.h"
#include "bone_accessor.h"
#include "studio.h"

#include "tier0/tslist.h"
#include "tier0/miniprofiler.h"

#include "bone_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
//=============================================================================
//
// CConstraintBones
//
//=============================================================================
//=============================================================================

//-----------------------------------------------------------------------------
// Compute the aggregate target position and orientation from the weighted target list and return
// the resulting position and orientation in addition to updating the target dag.
// All passed arrays must be nTargetCount in length
//-----------------------------------------------------------------------------
float CConstraintBones::ComputeTargetPosition(
	Vector &vTargetPosition,
	int nTargetCount,
	float *flTargetWeights,
	Vector *vTargetPositions,
	Vector *vTargetOffsets )
{
	vTargetPosition = vec3_origin;
	float flWeightSum = 0.0;

	for ( int i = 0; i < nTargetCount; ++i )
	{
		vTargetPosition += ( flTargetWeights[i] * ( vTargetPositions[i] + vTargetOffsets[i] ) );
		flWeightSum += flTargetWeights[i];
	}

	if ( flWeightSum > 0.0f )
	{
		vTargetPosition *= 1.0f / flWeightSum;
	}

	return MIN( 1.0f, flWeightSum );
}


//-----------------------------------------------------------------------------
// Compute the aggregate target orientation from the weighted target list and 
// return the total weight
// All passed arrays must be nTargetCount in length
//-----------------------------------------------------------------------------
float CConstraintBones::ComputeTargetOrientation(
	Quaternion &qTargetOrientation,
	int nTargetCount,
	float *pflTargetWeights,
	Quaternion *pqTargetOrientations,
	Quaternion *pqTargetOffsets )
{
	// If there is only one target, for efficiency don't bother with the weighting
	if ( nTargetCount == 1 )
	{
		QuaternionMult( pqTargetOrientations[0], pqTargetOffsets[0], qTargetOrientation );

		return MIN( 1.0f, pflTargetWeights[0] );
	}

	qTargetOrientation = quat_identity;

	// If no targets, return identity quaternion and weight of 0
	if ( nTargetCount <= 0 )
		return 0.0f;

	Quaternion *pQuats = reinterpret_cast< Quaternion * >( stackalloc( nTargetCount * sizeof( Quaternion ) ) );

	float flWeightSum = 0.0f;

	for ( int i = 0; i < nTargetCount; ++i )
	{
		QuaternionMult( pqTargetOrientations[i], pqTargetOffsets[i], pQuats[i] );
		flWeightSum += pflTargetWeights[i];
	}

	QuaternionAverageExponential( qTargetOrientation, nTargetCount, pQuats, pflTargetWeights );

	return MIN( 1.0f, flWeightSum );
}


//-----------------------------------------------------------------------------
// Compute the aggregate target position and orientation from the weighted
// target list and return the total weight
// All passed arrays must be nTargetCount in length
//-----------------------------------------------------------------------------
float CConstraintBones::ComputeTargetPositionOrientation(
	Vector &vTargetPosition,
	Quaternion &qTargetOrientation,
	int nTargetCount,
	float *pflTargetWeights,
	Vector *pvTargetPositions,
	Vector *pvTargetOffsets,
	Quaternion *pqTargetOrientations,
	Quaternion *pqTargetOffsets )
{
	Quaternion *pQuats = reinterpret_cast< Quaternion *>( stackalloc( nTargetCount * sizeof( Quaternion ) ) );

	float flWeightSum = 0.0f;

	vTargetPosition = vec3_origin;
	qTargetOrientation = quat_identity;

	for ( int i = 0; i < nTargetCount; ++i )
	{
		float flWeight = pflTargetWeights[i];

		matrix3x4a_t mTarget;
		AngleMatrix( RadianEuler(pqTargetOrientations[i]), pvTargetPositions[i], mTarget );

		matrix3x4a_t mOffset;
		AngleMatrix( RadianEuler(pqTargetOffsets[i]), pvTargetOffsets[i], mOffset );

		matrix3x4a_t mAbs;
		ConcatTransforms( mTarget, mOffset, mAbs );

		Vector vPos;
		MatrixAngles( mAbs, pQuats[i], vPos );

		vTargetPosition += ( flWeight * vPos );

		// For normalization
		flWeightSum += flWeight;
	}

	if ( flWeightSum > 0.0f )
	{
		vTargetPosition *= 1.0f / flWeightSum;
	}

	QuaternionAverageExponential( qTargetOrientation, nTargetCount, pQuats, pflTargetWeights );

	return MIN( 1.0f, flWeightSum );
}


//-----------------------------------------------------------------------------
// Compute the aggregate target position and orientation from the weighted
// target list and return the total weight
// All passed arrays must be nTargetCount in length
//-----------------------------------------------------------------------------
float CConstraintBones::ComputeTargetPositionOrientation(
	Vector &vTargetPosition,
	Quaternion &qTargetOrientation,
	int nTargetCount,
	float *pflTargetWeights,
	matrix3x4a_t *pmTargets,
	matrix3x4a_t *pmOffsets )
{
	Quaternion *pQuats = reinterpret_cast< Quaternion *>( stackalloc( nTargetCount * sizeof( Quaternion ) ) );

	float flWeightSum = 0.0f;

	vTargetPosition = vec3_origin;
	qTargetOrientation = quat_identity;

	matrix3x4a_t mAbs;
	Vector vPos;

	for ( int i = 0; i < nTargetCount; ++i )
	{
		float flWeight = pflTargetWeights[i];

		ConcatTransforms( pmTargets[i], pmOffsets[i], mAbs );

		MatrixAngles( mAbs, pQuats[i], vPos );

		vTargetPosition += ( flWeight * vPos );

		// For normalization
		flWeightSum += flWeight;
	}

	if ( flWeightSum > 0.0f )
	{
		vTargetPosition *= 1.0f / flWeightSum;
	}

	QuaternionAverageExponential( qTargetOrientation, nTargetCount, pQuats, pflTargetWeights );

	return MIN( 1.0f, flWeightSum );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CConstraintBones::ComputeAimConstraintOffset(
	Quaternion &qAimOffset,
	bool bPreserveOffset,
	const Vector &vTargetWorldPos,
	const matrix3x4_t &mSlaveParentToWorld,
	const Vector &vUp,
	const Vector &vSlaveLocalPos,
	const Quaternion &qSlaveLocal,
	matrix3x4_t *pmUpToWorld,
	AimConstraintUpType_t eUpType )
{
	if ( !bPreserveOffset )
	{
		qAimOffset = quat_identity;
		return;
	}

	// Calculate the desired orientation based the target position
	Quaternion qAim;
	ComputeAimConstraint( qAim, vTargetWorldPos, mSlaveParentToWorld, vUp, vSlaveLocalPos, pmUpToWorld, eUpType );

	// Compute the difference between the slave's current orientation and the target orientation
	Quaternion qAimInv;
	QuaternionInvert( qAim, qAimInv );

	QuaternionMult( qAimInv, qSlaveLocal, qAimOffset );

	RadianEuler eAim(qAim);
	RadianEuler eSlaveLocal(qSlaveLocal);
	RadianEuler eAimOffset(qAimOffset);
}


//-----------------------------------------------------------------------------
// Calculate the orientation needed to make a transform where the y 
// vector of the transform matches the forward vector and the z vector matches
// the up reference vector as closely as possible. The x vector will be in the 
// plane defined by using the forward vector as the normal. 
//-----------------------------------------------------------------------------
void CConstraintBones::ComputeAimConstraintAimAt(
	Quaternion &qAim,
	const Vector &vForward,
	const Vector &vReferenceUp )
{ 
	Vector vFwd = vForward;
	vFwd.NormalizeInPlace();
	const float flRatio = DotProduct( vFwd, vReferenceUp );
	Vector vUp = vReferenceUp - ( vFwd * flRatio );
	vUp.NormalizeInPlace();

	Vector vRight = vFwd.Cross( vUp );
	vRight.NormalizeInPlace();

	const Vector &vX = vRight;
	const Vector &vY = vFwd;
	const Vector &vZ = vUp; 

	const float flTr = vX.x + vY.y + vZ.z; 
	qAim.Init( vY.z - vZ.y , vZ.x - vX.z, vX.y - vY.x, flTr + 1.0f ); 
	const float flRadius = qAim[0] * qAim[0] + qAim[1] * qAim[1] + qAim[2] * qAim[2] + qAim[3] * qAim[3];
	if ( flRadius > FLT_EPSILON )
	{
		QuaternionNormalize( qAim ); 
	}
	else
	{
		matrix3x4_t mRot;
		MatrixSetColumn( vX, 0, mRot );
		MatrixSetColumn( vY, 1, mRot );
		MatrixSetColumn( vZ, 2, mRot );
		MatrixQuaternion( mRot, qAim );
	}
}


//-----------------------------------------------------------------------------
// Given the various parameters, computes the local vForward & vReferenceUp
// and calls ComputeAimConstraintAimAt
//-----------------------------------------------------------------------------
void CConstraintBones::ComputeAimConstraint(
	Quaternion &qAim,
	const Vector &vTargetWorldPos,
	const matrix3x4_t &mParentToWorld,
	const Vector &vUp,
	const Vector &vSlaveLocalPos,
	const matrix3x4_t *pmUpToWorld,
	AimConstraintUpType_t eUpType )
{
	matrix3x4_t mWorldToParent;
	MatrixInvert( mParentToWorld, mWorldToParent );

	// If the up vector is in world space, convert it into local space
	Vector vWorldUp;

	ComputeWorldUpVector( &vWorldUp, mParentToWorld, vUp, vSlaveLocalPos, pmUpToWorld, eUpType );

	Vector vLocalUp;
	VectorRotate( vWorldUp, mWorldToParent, vLocalUp );

	// Convert the target's world space position into the local space of the slave.
	Vector vTargetLocalPos;
	VectorTransform( vTargetWorldPos, mWorldToParent, vTargetLocalPos );

	// Compute the local space forward vector
	Vector vLocalForward = vTargetLocalPos - vSlaveLocalPos;
	vLocalForward.NormalizeInPlace();

	// Compute the orientation 
	CConstraintBones::ComputeAimConstraintAimAt( qAim, vLocalForward, vLocalUp );	
	RadianEuler e(qAim);
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CConstraintBones::ComputeWorldUpVector(
	Vector *pvWorldUp,
	const matrix3x4_t & mParentToWorld,
	const Vector &vUp,
	const Vector &vSlaveLocalPos,
	const matrix3x4_t *pmUpToWorld,
	AimConstraintUpType_t eUpType )
{
	switch ( eUpType )
	{
	case AC_UP_TYPE_VECTOR:
		VectorCopy( vUp, *pvWorldUp );
		break;
	case AC_UP_TYPE_OBJECT:
		if ( pmUpToWorld )
		{
			Vector vUpObjectWorldPos;
			MatrixPosition( *pmUpToWorld, vUpObjectWorldPos );
			Vector vSlaveWorldPos;
			VectorTransform( vSlaveLocalPos, mParentToWorld, vSlaveWorldPos );
			VectorSubtract( vUpObjectWorldPos, vSlaveWorldPos, *pvWorldUp );
			VectorNormalize( *pvWorldUp );
		}
		else
		{
			VectorCopy( vUp, *pvWorldUp );
		}
		break;
	case AC_UP_TYPE_PARENT_ROTATION:
		VectorRotate( vUp, mParentToWorld, *pvWorldUp );
		break;
	default:
	case AC_UP_TYPE_OBJECT_ROTATION:
		if ( pmUpToWorld )
		{
			VectorRotate( vUp, *pmUpToWorld, *pvWorldUp );
		}
		else
		{
			VectorCopy( vUp, *pvWorldUp );
		}
		break;
	}
}


//=============================================================================
//=============================================================================
//
// CStudioConstraintBones
//
//=============================================================================
//=============================================================================


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CStudioConstraintBones::ComputeTargetPosition(
	Vector &vTargetPosition,
	mstudioconstrainttarget_t *pTargets,
	int	nTargetCount,
	CBoneAccessor &boneToWorld )
{
	float *pflTargetWeights = reinterpret_cast< float * >( stackalloc( nTargetCount * sizeof( float ) ) );
	Vector *pvTargetPositions = reinterpret_cast< Vector * >( stackalloc( nTargetCount * sizeof( Vector ) ) );
	Vector *pvTargetOffsets = reinterpret_cast< Vector * >( stackalloc( nTargetCount * sizeof( Vector ) ) );

	mstudioconstrainttarget_t *pTarget;

	for ( int i = 0; i < nTargetCount; ++i )
	{
		pTarget = pTargets + i;
		pflTargetWeights[i] = pTarget->m_flWeight;
		pvTargetOffsets[i] = pTarget->m_vOffset;
		MatrixPosition( boneToWorld.GetBone( pTarget->m_nBone ), pvTargetPositions[i] );
	}

	return CConstraintBones::ComputeTargetPosition( vTargetPosition, nTargetCount, pflTargetWeights, pvTargetPositions, pvTargetOffsets );
}


//=============================================================================
// CStudioConstraintBones : Studio Interface Functions
//=============================================================================


//-----------------------------------------------------------------------------
// studio interface
//-----------------------------------------------------------------------------
float CStudioConstraintBones::ComputeTargetOrientation(
	Quaternion &qTargetOrientation,
	mstudioconstrainttarget_t *pTargets,
	int	nTargetCount,
	CBoneAccessor &boneToWorld )
{
	float *pflTargetWeights = reinterpret_cast< float * >( stackalloc( nTargetCount * sizeof( float ) ) );
	Quaternion *pqTargetOrientations = reinterpret_cast< Quaternion * >( stackalloc( nTargetCount * sizeof( Quaternion ) ) );
	Quaternion *pqTargetOffsets = reinterpret_cast< Quaternion * >( stackalloc( nTargetCount * sizeof( Quaternion ) ) );

	mstudioconstrainttarget_t *pTarget;

	for ( int i = 0; i < nTargetCount; ++i )
	{
		pTarget = pTargets + i;
		pflTargetWeights[i] = pTarget->m_flWeight;
		pqTargetOffsets[i] = pTarget->m_qOffset;
		MatrixQuaternion( boneToWorld.GetBone( pTarget->m_nBone ), pqTargetOrientations[i] );
	}

	return CConstraintBones::ComputeTargetOrientation( qTargetOrientation, nTargetCount, pflTargetWeights, pqTargetOrientations, pqTargetOffsets );
}


//-----------------------------------------------------------------------------
// studio interface
//-----------------------------------------------------------------------------
float CStudioConstraintBones::ComputeTargetPositionOrientation(
	Vector &vTargetPosition,
	Quaternion &qTargetOrientation,
	mstudioconstrainttarget_t *pTargets,
	int	nTargetCount,
	CBoneAccessor &boneToWorld )
{
	float *pflTargetWeights = reinterpret_cast< float * >( stackalloc( nTargetCount * sizeof( float ) ) );
	matrix3x4a_t *pmTargets = reinterpret_cast< matrix3x4a_t * >( stackalloc( nTargetCount * sizeof( matrix3x4a_t ) ) );
	matrix3x4a_t *pmOffsets = reinterpret_cast< matrix3x4a_t * >( stackalloc( nTargetCount * sizeof( matrix3x4a_t ) ) );

	mstudioconstrainttarget_t *pTarget = pTargets;

	for ( int i = 0; i < nTargetCount; ++i, ++pTarget )
	{
		pflTargetWeights[i] = pTarget->m_flWeight;
		QuaternionMatrix( pTarget->m_qOffset, pTarget->m_vOffset, pmOffsets[i] );
		pmTargets[i] = boneToWorld.GetBone( pTarget->m_nBone );
	}

	return CConstraintBones::ComputeTargetPositionOrientation( vTargetPosition, qTargetOrientation, nTargetCount, pflTargetWeights, pmTargets, pmOffsets );
}


//-----------------------------------------------------------------------------
// studio interface
//-----------------------------------------------------------------------------
void CStudioConstraintBones::ComputeBaseWorldMatrix(
	matrix3x4a_t &mBaseWorldMatrix,
	mstudioconstraintslave_t *pSlave,
	CBoneAccessor &boneToWorld,
	const CStudioHdr *pStudioHdr,
	const matrix3x4_t *pmViewTransform /* = NULL */ )
{
	// studiomdl shouldn't create mstudioconstraintslave_t's with invalid bone indices
	Assert( pSlave->m_nBone >= 0 && pSlave->m_nBone < MAXSTUDIOBONES );

	const int nBoneParent = pStudioHdr->boneParent( pSlave->m_nBone );
	if ( nBoneParent < 0 )
	{
		if ( pmViewTransform )
		{
			matrix3x4a_t mTmp;
			QuaternionMatrix( pSlave->m_qBaseOrientation, pSlave->m_vBasePosition, mTmp );
			ConcatTransforms( *pmViewTransform, mTmp, mBaseWorldMatrix );
		}
		else
		{
			QuaternionMatrix( pSlave->m_qBaseOrientation, pSlave->m_vBasePosition, mBaseWorldMatrix );
		}
	}
	else
	{
		matrix3x4a_t mTmp;
		QuaternionMatrix( pSlave->m_qBaseOrientation, pSlave->m_vBasePosition, mTmp );
		ConcatTransforms( boneToWorld.GetBone( nBoneParent ), mTmp, mBaseWorldMatrix );
	}
}


//-----------------------------------------------------------------------------
// studio interface
//-----------------------------------------------------------------------------
void CStudioConstraintBones::ComputePointConstraint(
	const mstudiobone_t *pBones,
	int	nBone,
	CBoneAccessor &boneToWorld,
	const CStudioHdr *pStudioHdr )
{
	BONE_PROFILE_FUNC();

	mstudiopointconstraint_t *pProc = ( mstudiopointconstraint_t * )pBones[nBone].pProcedure();

	// Calculate the current target position and the total weight 
	// of the the targets contributing to the target position.
	Vector vTargetPosition;
	const float flWeight = CStudioConstraintBones::ComputeTargetPosition( vTargetPosition, pProc->pTarget( 0 ), pProc->m_nTargetCount, boneToWorld );

	Vector vFinalPosition;

	matrix3x4a_t &mBaseWorldMatrix = boneToWorld.GetBoneForWrite( nBone );
	CStudioConstraintBones::ComputeBaseWorldMatrix( mBaseWorldMatrix, &( pProc->m_slave ), boneToWorld, pStudioHdr );

	// Blend between the target position and the base position using the target weight
	if ( flWeight < 1.0f )
	{
		Vector vBasePosition;
		MatrixPosition( mBaseWorldMatrix, vBasePosition );

		vFinalPosition = Lerp( flWeight, vBasePosition, vTargetPosition );
	}
	else
	{
		vFinalPosition = vTargetPosition;
	}

	// Update the bone the new position.
	PositionMatrix( vFinalPosition, mBaseWorldMatrix );
}


//-----------------------------------------------------------------------------
// studio interface
//-----------------------------------------------------------------------------
void CStudioConstraintBones::ComputeOrientConstraint(
	const mstudiobone_t *pBones,
	int	nBone,
	CBoneAccessor &boneToWorld,
	const CStudioHdr *pStudioHdr,
	const matrix3x4_t *pmViewTransform )
{
	BONE_PROFILE_FUNC();

	mstudioorientconstraint_t *pProc = ( mstudioorientconstraint_t * )pBones[nBone].pProcedure();

	// Calculate the current target position and the total weight 
	// of the the targets contributing to the target position.
	Quaternion qTargetOrientation;
	const float flWeight = CStudioConstraintBones::ComputeTargetOrientation( qTargetOrientation, pProc->pTarget( 0 ), pProc->m_nTargetCount, boneToWorld );

	// Blend between the target orientation and the base orientation using the target weight
	Quaternion qFinalOrientation;

	matrix3x4a_t &mBaseWorldMatrix = boneToWorld.GetBoneForWrite( nBone );
	CStudioConstraintBones::ComputeBaseWorldMatrix( mBaseWorldMatrix, &( pProc->m_slave ), boneToWorld, pStudioHdr, pmViewTransform );

	if ( flWeight < 1.0f )
	{
		Quaternion qBaseOrientation;
		MatrixQuaternion( mBaseWorldMatrix, qBaseOrientation );

		QuaternionSlerp( qBaseOrientation, qTargetOrientation, flWeight, qFinalOrientation );
	}
	else
	{
		qFinalOrientation = qTargetOrientation;
	}

	// Quaternion matrix wipes out the translate component
	Vector vTmpPosition;
	MatrixPosition( mBaseWorldMatrix, vTmpPosition );
	QuaternionMatrix( qFinalOrientation, vTmpPosition, mBaseWorldMatrix );
}


//-----------------------------------------------------------------------------
// studio interface
//
// pmViewTransform is for hlmv which modifies the actual parentless bones
// to position things in the viewer
//
// TODO: Split into hlmv and "normal" versions, i.e. hlmv needs ViewTransform
//-----------------------------------------------------------------------------
void CStudioConstraintBones::ComputeAimConstraint(
	const mstudiobone_t *pBones,
	int	nBone,
	CBoneAccessor &boneToWorld,
	const CStudioHdr *pStudioHdr,
	const matrix3x4_t *pmViewTransform,
	AimConstraintUpType_t eType )
{
	BONE_PROFILE_FUNC();

	mstudioaimconstraint_t *pProc = ( mstudioaimconstraint_t * )pBones[nBone].pProcedure();

	// Calculate the current target position and the total weight 
	// of the the targets contributing to the target position.
	Vector vTargetPos;
	const float flWeight = CStudioConstraintBones::ComputeTargetPosition( vTargetPos, pProc->pTarget( 0 ), pProc->m_nTargetCount, boneToWorld );
	Vector vTargetWorldPos;

	matrix3x4a_t mSlaveParentToWorld;
	const int nParentBone = pBones[nBone].parent;

	if ( pmViewTransform )
	{
		matrix3x4_t mInv;
		MatrixInvert( *pmViewTransform, mInv );

		VectorTransform( vTargetPos, mInv, vTargetWorldPos );

		if ( nParentBone >= 0 )
		{
			ConcatTransforms( mInv, boneToWorld[nParentBone], mSlaveParentToWorld );
		}
		else
		{
			SetIdentityMatrix( mSlaveParentToWorld );
		}
	}
	else
	{
		VectorCopy( vTargetPos, vTargetWorldPos );

		if ( nParentBone >= 0 )
		{
			MatrixCopy( boneToWorld[nParentBone], mSlaveParentToWorld );
		}
		else
		{
			SetIdentityMatrix( mSlaveParentToWorld );
		}
	}

	Quaternion qTargetOrientation;
	CConstraintBones::ComputeAimConstraint(
		qTargetOrientation,
		vTargetWorldPos,
		mSlaveParentToWorld,
		pProc->m_vUp,
		pProc->m_slave.m_vBasePosition,
		pProc->m_nUpSpaceTarget >= 0 ? &boneToWorld[ pProc->m_nUpSpaceTarget ] : NULL,
		eType );

	// Add in initial offset
	Quaternion qOffsetOrientation;
	QuaternionMult( qTargetOrientation, pProc->m_qAimOffset, qOffsetOrientation );

	// Add in parent matrix
	Quaternion qParentToWorld;
	MatrixQuaternion( mSlaveParentToWorld, qParentToWorld );
	Quaternion qTmp;
	QuaternionMult( qParentToWorld, qOffsetOrientation, qTmp );
	qOffsetOrientation = qTmp;

	// Blend between the target orientation and the base orientation using the target weight
	Quaternion qFinalOrientation;

	matrix3x4a_t &mBaseWorldMatrix = boneToWorld.GetBoneForWrite( nBone );
	CStudioConstraintBones::ComputeBaseWorldMatrix( mBaseWorldMatrix, &( pProc->m_slave ), boneToWorld, pStudioHdr, pmViewTransform );

	if ( flWeight < 1.0f )
	{
		Quaternion qBaseOrientation;
		MatrixQuaternion( mBaseWorldMatrix, qBaseOrientation );

		QuaternionSlerp( qBaseOrientation, qOffsetOrientation, flWeight, qFinalOrientation );
	}
	else
	{
		qFinalOrientation = qOffsetOrientation;
	}

	if ( pmViewTransform )
	{
		Quaternion qTmp0;
		Quaternion qTmp1;
		MatrixQuaternion( *pmViewTransform, qTmp0 );
		QuaternionMult( qTmp0, qFinalOrientation, qTmp1 );
		qFinalOrientation = qTmp1;
	}

	// Quaternion matrix wipes out the translate component
	Vector vTmpPosition;
	MatrixPosition( mBaseWorldMatrix, vTmpPosition );
	QuaternionMatrix( qFinalOrientation, vTmpPosition, mBaseWorldMatrix );
}


//-----------------------------------------------------------------------------
// studio interface
//-----------------------------------------------------------------------------
void CStudioConstraintBones::ComputeParentConstraint(
	const mstudiobone_t *pBones,
	int	nBone,
	CBoneAccessor &boneToWorld,
	const CStudioHdr *pStudioHdr )
{
	BONE_PROFILE_FUNC();

	mstudioorientconstraint_t *pProc = ( mstudioorientconstraint_t * )pBones[nBone].pProcedure();

	// Calculate the current target position and the total weight 
	// of the the targets contributing to the target position.
	Vector vTargetPosition;
	Quaternion qTargetOrientation;
	const float flWeight = CStudioConstraintBones::ComputeTargetPositionOrientation( vTargetPosition, qTargetOrientation, pProc->pTarget( 0 ), pProc->m_nTargetCount, boneToWorld );

	// Blend between the target orientation and the base orientation using the target weight
	Quaternion qFinalOrientation;
	Vector vFinalPosition;

	matrix3x4a_t &mBaseWorldMatrix = boneToWorld.GetBoneForWrite( nBone );
	CStudioConstraintBones::ComputeBaseWorldMatrix( mBaseWorldMatrix, &( pProc->m_slave ), boneToWorld, pStudioHdr );

	if ( flWeight < 1.0f )
	{
		Vector vBasePosition;
		Quaternion qBaseOrientation;
		MatrixAngles( mBaseWorldMatrix, qBaseOrientation, vBasePosition );

		QuaternionSlerp( qBaseOrientation, qTargetOrientation, flWeight, qFinalOrientation );
		VectorLerp( vBasePosition, vTargetPosition, flWeight, vFinalPosition );
	}
	else
	{
		qFinalOrientation = qTargetOrientation;
		vFinalPosition = vTargetPosition;
	}

	QuaternionMatrix( qFinalOrientation, vFinalPosition, mBaseWorldMatrix );
}


//-----------------------------------------------------------------------------
//
// Twist bones are bones which take a portion of the rotation around a specified
// axis.
//
// The axis is defined as the vector between a parent and child bone
// The twist bones must also be children of the parent
//
// + parent
// | 
// +--+ twist 0.25
// |
// +--+ twist 0.5
// |
// +--+ twist 0.75
// |
// +--+ child
//
// If inverse is false each twist takes a portion of the child rotation around
// the specified axis
//
// If inverse is true each twist takes a portion of the parent rotation around
// the specified axis from a specified reference orientation
//
// All specified matrices & Quaternions are local to the bone, they are not
// worldToBone transformations
//
// pqTwists, pflWeights, pqTwistBinds are all pointers to arrays which must be
// at least nCount in size
// 
// This code is called directly from:
//   maya, datamodel & CalcProceduralBone/DoTwistBones
//-----------------------------------------------------------------------------
void ComputeTwistBones(
	Quaternion *pqTwists,
	int nCount,
	bool bInverse,
	const Vector &vUp,
	const Quaternion &qParent,
	const matrix3x4_t &mChild,
	const Quaternion &qBaseInv,
	const float *pflWeights,
	const Quaternion *pqTwistBinds )
{
	const float flEps = FLT_EPSILON * 10.0f;
	const float flEpsSq = flEps * flEps;

	Vector vUpRotate;
	Vector vLocalTranslation;
	Vector vRotatedTranslation;
	Quaternion qTmp0;
	Quaternion qTmp1;

	{
		Quaternion qChild;
		MatrixAngles( mChild, qChild, vLocalTranslation );

		// Check for 0 length translation - perhaps use Vector::IsZero?
		if ( vLocalTranslation.LengthSqr() < flEpsSq )
		{
			// No translation, can't compute rotation axis, do nothing
			V_memcpy( pqTwists, pqTwistBinds, nCount * sizeof( Quaternion ) );
			return;
		}

		VectorNormalize( vLocalTranslation );

		if ( bInverse )
		{
			QuaternionMult( qBaseInv, qParent, qTmp0 );
			VectorRotate( vUp, qTmp0, vUpRotate );
			VectorRotate( vLocalTranslation, qTmp0, vRotatedTranslation );
		}
		else
		{
			QuaternionMult( qBaseInv, qChild, qTmp0 );
			VectorRotate( vUp, qTmp0, vUpRotate );
			VectorRotate( vLocalTranslation, qBaseInv, vRotatedTranslation );
		}
	}

	// If the specified up axis and the rotated translation vector are parallel then quit
	if ( 1.0f - FloatMakePositive( DotProduct( vRotatedTranslation, vUp ) ) < flEps )
	{
		V_memcpy( pqTwists, pqTwistBinds, nCount * sizeof( Quaternion ) );
		return;
	}

	// If the rotated up axis and the rotated translation vector are parallel then quit
	if ( 1.0f - FloatMakePositive( DotProduct( vRotatedTranslation, vUpRotate ) ) < flEps )
	{
		V_memcpy( pqTwists, pqTwistBinds, nCount * sizeof( Quaternion ) );
		return;
	}

	// Project Up (V) & Rotated Up (V) into the plane defined by the
	// rotated up vector (N)
	//
	// U = V - ( V dot N ) N;
	//
	// U is V projected into plane with normal N

	Vector vTmp0;
	
	vTmp0 = vRotatedTranslation;
	Vector vUpProject;
	vTmp0 *= DotProduct( vUp, vRotatedTranslation );
	VectorSubtract( vUp, vTmp0, vUpProject );
	VectorNormalize( vUpProject );

	vTmp0 = vRotatedTranslation;
	Vector vUpRotateProject;
	vTmp0 *= DotProduct( vUpRotate, vRotatedTranslation );
	VectorSubtract( vUpRotate, vTmp0, vUpRotateProject );
	VectorNormalize( vUpRotateProject );

	if ( VectorsAreEqual( vUpProject, vUpRotateProject, 0.001 ) )
	{
		V_memcpy( pqTwists, pqTwistBinds, nCount * sizeof( Quaternion ) );
	}
	else
	{
		CrossProduct( vUpProject, vUpRotateProject, vTmp0 );
		VectorNormalize( vTmp0 );
		const float flDot = DotProduct( vUpProject, vUpRotateProject );
		const float flAngle = DotProduct( vTmp0, vRotatedTranslation ) < 0.0f ? -acos( flDot ) : acos( flDot );

		AxisAngleQuaternion( vLocalTranslation, RAD2DEG( flAngle ), qTmp0 );

		if ( bInverse )
		{
			for ( int i = 0; i < nCount; ++i )
			{
				QuaternionScale( qTmp0, pflWeights[i] - 1.0f, qTmp1 );
				QuaternionMult( qTmp1, pqTwistBinds[i], pqTwists[i] );
			}
		}
		else
		{
			for ( int i = 0; i < nCount; ++i )
			{
				QuaternionScale( qTmp0, pflWeights[i], qTmp1 );
				QuaternionMult( qTmp1, pqTwistBinds[i], pqTwists[i] );
			}
		}
	}
}
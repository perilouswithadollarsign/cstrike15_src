//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Generic constraint functions to compute bone constraints
// Used by studio/engine, SFM & Maya
//
// Studio interface functions to the generic constraint functions
//
//===============================================================================


#ifndef BONE_CONSTRAINTS_H
#define BONE_CONSTRAINTS_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct mstudiobone_t;
class CBoneAccessor;
class CStudioHdr;
struct mstudioconstrainttarget_t;
struct mstudioconstraintslave_t;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CConstraintBones
{
public:
	enum AimConstraintUpType_t
	{
		AC_UP_TYPE_OBJECT_ROTATION,		// Use supplied vector rotated by the specified object as up vector (Maya: Object Rotation Up), if no dag is supplied, reverts to Vector
		AC_UP_TYPE_VECTOR,				// Use supplied vector as up vector	(Maya: Vector)
		AC_UP_TYPE_OBJECT,				// Use vector from slave to specified object in world space as up vector( Maya: Object Up), if no dag is supplied, reverts to Vector
		AC_UP_TYPE_PARENT_ROTATION,		// Use supplied vector rotated by the parent object as up vector (Maya: None)

		AC_UP_TYPE_FIRST = AC_UP_TYPE_OBJECT_ROTATION,	// The smallest possible value
		AC_UP_TYPE_LAST = AC_UP_TYPE_PARENT_ROTATION,	// The largest possible value
	};


	// Compute the aggregate target position and orientation from the weighted target list and return
	// the resulting position and orientation in addition to updating the target dag.
	// All passed arrays must be nTargetCount in length
	static float ComputeTargetPosition(
		Vector &vTargetPosition,
		int nTargetCount,
		float *flTargetWeights,
		Vector *vTargetPositions,
		Vector *vTargetOffsets );

	// Compute the aggregate target orientation from the weighted target list and 
	// return the total weight
	// All passed arrays must be nTargetCount in length
	static float ComputeTargetOrientation(
		Quaternion &qTargetOrientation,
		int nTargetCount,
		float *pflTargetWeights,
		Quaternion *pqTargetOrientations,
		Quaternion *pqTargetOffsets );

	// Compute the aggregate target position and orientation from the weighted
	// target list and return the total weight
	// All passed arrays must be nTargetCount in length
	static float ComputeTargetPositionOrientation(
		Vector &vTargetPosition,
		Quaternion &qTargetOrientation,
		int nTargetCount,
		float *pflTargetWeights,
		Vector *vTargetPositions,
		Vector *vTargetOffsets,
		Quaternion *pqTargetOrientations,
		Quaternion *pqTargetOffsets );

	// Compute the aggregate target position and orientation from the weighted
	// target list and return the total weight
	// All passed arrays must be nTargetCount in length
	static float ComputeTargetPositionOrientation(
		Vector &vTargetPosition,
		Quaternion &qTargetOrientation,
		int nTargetCount,
		float *pflTargetWeights,
		matrix3x4a_t *pmTargets,
		matrix3x4a_t *pmOffsets );

	static void ComputeAimConstraintOffset(
		Quaternion &qAimOffset,
		bool bPreserveOffset,
		const Vector &vTargetWorldPos,
		const matrix3x4_t &mSlaveParentToWorld,
		const Vector &vUp,
		const Vector &vSlaveLocalPos,
		const Quaternion &qSlaveLocal,
		matrix3x4_t *pmUpToWorld,
		AimConstraintUpType_t eUpType );

	// Calculate the orientation needed to make a transform where the y 
	// vector of the transform matches the forward vector and the z vector matches
	// the up reference vector as closely as possible. The x vector will be in the 
	// plane defined by using the forward vector as the normal. 
	static void ComputeAimConstraintAimAt(
		Quaternion &qAim,
		const Vector &vForward,
		const Vector &vReferenceUp );

	// Given the various parameters, computes the local vForward & vReferenceUp
	// and calls ComputeAimConstraintAimAt
	static void ComputeAimConstraint(
		Quaternion &qAim,
		const Vector &vTargetWorldPos,
		const matrix3x4_t &mParentToWorld,
		const Vector &vUp,
		const Vector &vSlaveLocalPos,
		const matrix3x4_t *pmUpToWorld,
		AimConstraintUpType_t eUpType );


	//-----------------------------------------------------------------------------
	//
	//-----------------------------------------------------------------------------
	static void ComputeWorldUpVector(
		Vector *pvWorldUp,
		const matrix3x4_t & mParentToWorld,
		const Vector &vUp,
		const Vector &vSlaveLocalPos,
		const matrix3x4_t *pmUpToWorld,
		AimConstraintUpType_t eUpType );

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CStudioConstraintBones : public CConstraintBones
{
public:
	// Utilities
	static float ComputeTargetPosition(
		Vector &vTargetPosition,
		mstudioconstrainttarget_t *pTargets,
		int	nTargetCount,
		CBoneAccessor &boneToWorld );

	static float ComputeTargetOrientation(
		Quaternion &qTargetOrientation,
		mstudioconstrainttarget_t *pTargets,
		int	nTargetCount,
		CBoneAccessor &boneToWorld );

	static float ComputeTargetPositionOrientation(
		Vector &vTargetPosition,
		Quaternion &qTargetOrientation,
		mstudioconstrainttarget_t *pTargets,
		int	nTargetCount,
		CBoneAccessor &boneToWorld );

	static void ComputeBaseWorldMatrix(
		matrix3x4a_t &mBaseWorldMatrix,
		mstudioconstraintslave_t *pSlave,
		CBoneAccessor &boneToWorld,
		const CStudioHdr *pStudioHdr,
		const matrix3x4_t *pmViewTransform = NULL );

	// constraints
	static void ComputePointConstraint(
		const mstudiobone_t *pBones,
		int	nBone,
		CBoneAccessor &boneToWorld,
		const CStudioHdr *pStudioHdr );

	static void ComputeOrientConstraint(
		const mstudiobone_t *pBones,
		int	nBone,
		CBoneAccessor &boneToWorld,
		const CStudioHdr *pStudioHdr,
		const matrix3x4_t *pmViewTransform );

	static void ComputeAimConstraint(
		const mstudiobone_t *pBones,
		int	nBone,
		CBoneAccessor &boneToWorld,
		const CStudioHdr *pStudioHdr,
		const matrix3x4_t *pmViewTransform,
		AimConstraintUpType_t eType );

	static void ComputeParentConstraint(
		const mstudiobone_t *pBones,
		int	nBone,
		CBoneAccessor &boneToWorld,
		const CStudioHdr *pStudioHdr );

};

#endif // BONE_CONSTRAINTS_H
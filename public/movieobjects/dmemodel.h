//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a skeletal model (gets compiled into a MDL)
//
//===========================================================================//

#ifndef DMEMODEL_H
#define DMEMODEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstack.h"
#include "tier1/utlstringtoken.h"
#include "movieobjects/dmejoint.h"
#include "movieobjects/dmetransformlist.h"
#include "movieobjects/dmeaxissystem.h"


class CDmeDrawSettings;
class CDmeMesh;

//-----------------------------------------------------------------------------
// A class representing a skeletal model
//-----------------------------------------------------------------------------
class CDmeModel : public CDmeDag
{
	DEFINE_ELEMENT( CDmeModel, CDmeDag );

public:
	// Add joint
	CDmeJoint *AddJoint( const char *pJointName, CDmeDag *pParent = NULL );
	int AddJoint( CDmeDag *pJoint );

	// Returns the number of joint transforms we know about
	int GetJointCount() const;

	// Returns the DmeDag for the specified joint index
	CDmeDag *GetJoint( int nIndex );
	const CDmeDag *GetJoint( int nIndex ) const;

	// Returns the DmeTransform for the specified joint index
	CDmeTransform *GetJointTransform( int nIndex );
	const CDmeTransform *GetJointTransform( int nIndex ) const;

	// Determines joint index of a given a joint
	int GetJointIndex( CDmeDag *pJoint ) const;
	int GetJointIndex( const char *pJointName ) const;
	int GetJointIndex( CUtlStringToken nJointNameHash ) const;

	// Captures the current joint transforms into a base state
	void CaptureJointsToBaseState( const char *pBaseStateName );

	// Sets the joint transforms to the values in the specified base state, if it exists
	void PushBaseStateToJoints( const char *pBaseStateName );

	// Finds a base state by name, returns NULL if not found
	CDmeTransformList *FindBaseState( const char *pBaseStateName );

	// Recursively render the Dag hierarchy
	virtual void Draw( CDmeDrawSettings *pDrawSettings = NULL );

	// NOTE: See comment for m_upAxis attribute below

	// Set if Z is the up axis of the model
	// Set to true if the data under the DmeModel is AS_VALVE_ENGINE space( Up: Z, Fwd: X, Left: Y )
	// Set to false if the data under the DmeModel is AS_MAYA_YUP space ( Up: Y, Fwd: Z, Left: X )
	// See SetAxisSystem() for anything other than AS_VALVE_ENGINE or AS_MAYA_YUP
	void ZUp( bool bZUp );

	// Returns true if the DmeModel is Z Up.
	bool IsZUp() const;

	// Returns the matrix that moves DmeModel data to engine space
	void GetModelToEngineMat( matrix3x4_t &modelToEngineMat );

	// Returns the matrix that moves engine data to DmeModel space
	void GetEngineToModelMat( matrix3x4_t &engineToModelMat );

	// Replace all instances of a material with a different material
	// Using a NULL material name will replace *all* materials with the new material
	void ReplaceMaterial( const char *pOldMaterialName, const char *pNewMaterialName );

	// Collapses all joints below the specified joint name, reskinning any meshes
	// referring to collapsed joints to use the specified joint instead
	void CollapseJoints( const char *pJointName );

	// Gets the joint with the specified name
	CDmeDag *GetJoint( const char *pJointName );

	// Reskin all meshes under the DmeModel
	void ReskinMeshes( const int *pJointTransformIndexRemap );

	// Sets the DmeAxisSystem, no reorientation is done, see ConvertToAxisSystem
	bool SetAxisSystem( CDmeAxisSystem::PredefinedAxisSystem ePredefAxisSystem );
	bool SetAxisSystem( CDmeAxisSystem::Axis_t eUpAxis, CDmeAxisSystem::ForwardParity_t eForwardParity, CDmeAxisSystem::CoordSys_t eCoordSys = CDmeAxisSystem::AS_RIGHT_HANDED );

	// Sets the DmeAxisSystem and reorients all of the data under the DmeModel
	// as necessary to change from the current DmeAxisSystem to the specified DmeAxisSystem
	// Currently private, will be made public soon, upAxis is still authoritative
	bool ConvertToAxisSystem( CDmeAxisSystem::PredefinedAxisSystem ePredefAxisSystem );
	bool ConvertToAxisSystem( CDmeAxisSystem::Axis_t eUpAxis, CDmeAxisSystem::ForwardParity_t eForwardParity, CDmeAxisSystem::CoordSys_t eCoordSys = CDmeAxisSystem::AS_RIGHT_HANDED );

	// Returns the matrix to convert data from the current axisSystem to the specified axisSystem
	bool GetConversionMatrix( matrix3x4a_t &mConversion, CDmeAxisSystem::PredefinedAxisSystem eToPredefinedAxisSystem );

	// Freeze child meshes, pushes transform into vertices of mesh, used to be part of Reorient
	void FreezeChildMeshes();

	// TransformScene
	void TransformScene( const Vector &vScale = Vector( 1.0f, 1.0f, 1.0f ), const Vector &vTranslate = vec3_origin, const DegreeEuler &eRotation = DegreeEuler( 0.0f, 0.0f, 0.0f ), float flEps = 1.0e-4 );

	void ScaleScene( const Vector &vScale );

	// Updates all base states by adding missing joints
	void UpdateBaseStates();

private:
	// Reskin meshes based on bone collapse
	void ReskinMeshes( CDmeDag *pDag, const int *pJointTransformIndexRemap );

	// Remove joints
	void RemoveJoints( int nNewJointCount, const int *pInvJointRemap );

	// Removes all children from this joint, moving shapes to be 
	void RemoveAllChildren( CDmeDag *pDag, CDmeDag *pSubtreeRoot, const matrix3x4_t &jointToSubtreeRoot );
	void RemoveAllChildren( CDmeDag *pSubtreeRoot );

	// Helper functions for ReorientToEngineSpace() and ReorientToDCCToolSpace()
	static void GetReorientData( matrix3x4_t &m, Quaternion &q, bool bMakeZUp );
	static void ReorientDmeAnimation( CDmeDag *pDmeDag, const matrix3x4_t &mOrient, const Quaternion &qOrient );
	static void ReorientDmeTransform( CDmeTransform *pDmeTransform, const matrix3x4_t &mOrient, const Quaternion &qOrient );
	static void ReorientDmeModelChildren( CDmeModel *pDmeModel, const matrix3x4_t &mOrient, const Quaternion &qOrient );
	static void ReorientChildDmeMeshes_R( CDmeDag *pDmeDag );
	static void ReorientDmeMesh( CDmeMesh *pDmeMesh, matrix3x4_t absMat );

protected:
	// The order in which the joint transform names appear in this list
	// indicates the joint index for each dag
	CDmaElementArray< CDmeDag > m_JointList;

	// Stores a list of base poses for all the joint transforms
	CDmaElementArray<CDmeTransformList> m_BaseStates;

	// Stores the up axis of the model
	// One of Y or Z
	//
	// NOTE: Y up is Maya's typical coordinate system.  Setting the up axis to Z
	//       won't quite give you the engine's coordinate system (even though it
	//       has an Up Axis of Z as well).  The difference is the forward axis.
	//
	//       A DmeModel with m_UpAxis = Y gives the data in a coordinate system
	//       that is Maya's typical coordinate system.  Y Up, Z Foward, X Right.
	//
	//       A DmeModel with m_UpAxis = Z gives the data in a coordinate system
	//       that is compatible with typical SMD data.  This is data that can
	//       be compiled in studiomdl without any special options.
	//
	//       Neither Y up or Z up is engine space.  Z up is closer but still
	//       requires another 90 degree rotation to align things.  This can be
	//       changed in the future if required but Z up was added for legacy
	//       SMD compatibility.
	//
	//       In this table, missing entries mean it's not required or isn't
	//       supported
	//
	// m_UpAxis Desc          Up      Forward Right   studiomdl
	// -------- ------------- ------- ------- ------- ---------
	//    Y     Maya Y Up        Y       Z       X    $upaxis Y
	//    Z     Maya Z Up/XSI    Z      -Y       X
	//          Engine           Z       X       Y    $origin 0 0 0 0
	//-----------------------------------------------------------------------------
	CDmaString m_UpAxis;
	
	CDmaElement< CDmeAxisSystem > m_eAxisSystem;

	CDmaElement< CDmeModel > m_eModel;
	CDmaElement< CDmeModel > m_eSkeleton;

private:
	enum SetupBoneRetval_t
	{
		NO_SKIN_DATA = 0,
		TOO_MANY_BONES,
		BONES_SET_UP
	};

	// Sets up the render state for the model
	SetupBoneRetval_t SetupBoneMatrixState( const matrix3x4_t& shapeToWorld, bool bForceSoftwareSkin );

	// Loads up joint transforms for this model
	void LoadJointTransform( CDmeDag *pJoint, CDmeTransformList *pBindPose, const matrix3x4_t &parentToWorld, const matrix3x4_t &parentToBindPose, bool bSetHardwareState );

	// Implementation of ReplaceMaterial
	void ReplaceMaterial( CDmeDag *pDag, const char *pOldMaterialName, const char *pNewMaterialName );

	// Sets up the render state for the model
	static matrix3x4_t *SetupModelRenderState( const matrix3x4_t& shapeToWorld, bool bHasSkinningData, bool bForceSoftwareSkin );
	static void CleanupModelRenderState();

	// Stack of DmeModels currently being rendered. Used to set up render state
	static CUtlStack< CDmeModel * > s_ModelStack;

	friend class CDmeMesh;
	friend bool SetMeshFromSkeleton( CDmeMesh *pDmeMesh );	// dmx edit function - to do skin weights

};


#endif // DMEMODEL_H

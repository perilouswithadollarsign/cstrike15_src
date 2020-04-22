//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Declaration of CDmAnimUtils a set of animation related utilities 
// which work on DmeDag and other DmElement derived objects.
//
//=============================================================================


#ifndef DMANIMUTILS_H
#define DMANIMUTILS_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmechannel.h"


class CDmeGameModel;


enum TransformSpace_t
{
	TS_WORLD_SPACE,
	TS_LOCAL_SPACE,
	TS_OBJECT_SPACE,
	TS_REFERENCE_SPACE,
};

enum ReParentLogMode_t
{
	REPARENT_LOGS_NONE,				// Do not touch logs
	REPARENT_LOGS_OVERWRITE,		// Overwrite the logs with new local transform of the dag
	REPARENT_LOGS_OFFSET_LOCAL,		// Apply the transform required to maintain the world space transform of the dag to all log samples
	REPARENT_LOGS_MAINTAIN_WORLD,	// Modify the logs so that the world space position and orientation animation is maintained
};

//-----------------------------------------------------------------------------
// CDmeDagPtr - For swig generation this is an actual class with a single 
// member which is a pointer to dag, for normal c++ code it is just a typedef
// of a pointer to a CDmeDag* , this is required because swig has issues 
// generating code for CUtlVector< CDmeDag * > as there are functions within
// CUtlVector which use the paremter of T & wich with T = CDmeDag * results in
// CDmeDag *&, and when swig generates code to parse this type coming from python
// it considers a pointer to a pointer, when infact it is still just a pointer.
// So we tell swig that CDmeDagPtr is a class to avoid this issue, but treat it
// as typedef in code. This works since both are exactly 4 bytes containing the
// address of the CDmeDag instance. 
//-----------------------------------------------------------------------------
#ifdef SWIG

class CDmeDagPtr
{
public:
	CDmeDagPtr()				{ m_pDag = NULL; }
	CDmeDagPtr( CDmeDag *pDag ) { m_pDag = pDag; }
private:
	CDmeDag	*m_pDag;
};

#else

typedef CDmeDag* CDmeDagPtr;

#endif


//-----------------------------------------------------------------------------
// CDmeAnimUtils - A collection of utility functions which perform animation 
// related operations on data model elements. This class is wrapped by swig so
// that the functions are accessible from python, but the functions are 
// designed to be used directly as well.
//-----------------------------------------------------------------------------
class CDmAnimUtils
{

public:


	// Create an infinite time selection
	static CDmeTimeSelection *CreateInfiniteTimeSelection();

	// Create an absolute time selection with the specified key times
	static CDmeTimeSelection *CreateTimeSelection( DmeTime_t leftFalloff, DmeTime_t leftHold, DmeTime_t rightHold, DmeTime_t rightFalloff );

	// Create a dag with the specified name, position and orientation
	static CDmeDag *CreateDag( const char *pchName, const Vector &position, const Quaternion &orientation, CDmeDag *pParent = NULL );
	
	// Get the average position of the provided dag nodes in the specified space
	static Vector GetDagPosition( const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReference = NULL );
	
	// Get the average Euler rotation of the provided dag nodes in the specified space
	static Vector GetDagRotation( const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReference = NULL );

	// Get the average orientation (quaternion) of the provided dag nodes in the specified space
	static Quaternion GetDagOrientation( const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReference = NULL );
	
	// Get the average position and orientation of the provided dag nodes in the specified space
	static void GetDagPositionOrienation( Vector &position, Quaternion &orienation, const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReferenceDag = NULL );

	// Move the provided dag nodes in the specified space
	static void MoveDagNodes( const Vector &offset, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, TransformSpace_t space, CDmeDag *pReference = NULL );
	
	// Move the provided dag nodes in the specified space and apply the operation to the logs associated with the dag
	static void MoveDagNodes( const Vector &offset, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, const CDmeTimeSelection *pTimeSelection, bool bOffsetOverTime, TransformSpace_t space, CDmeDag *pReference = NULL );
	
	// Rotate the provided dag nodes in the specified space
	static void RotateDagNodes( const Vector &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, TransformSpace_t space, CDmeDag *pReference = NULL );
	
	// Rotate the provided dag nodes in the specified space and apply the operation to the logs associated with the dag
	static void RotateDagNodes( const Vector &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, const CDmeTimeSelection *pTimeSelection, bool bOffsetOverTime, TransformSpace_t space, CDmeDag* pReference = NULL );
	
	// Perform both a translation and a rotation of the specified dag nodes
	static void TransformDagNodes( const Vector &offset, const Quaternion &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, TransformSpace_t space, CDmeDag* pReferenceDag = NULL, bool bPosition = true, bool bRotation = true );

	// Perform both a translation and a rotation of the specified dag nodes and apply the operation to the logs associated with the dag
	static void TransformDagNodes( const Vector &offset, const Quaternion &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, const CDmeTimeSelection *pTimeSelection, bool bOffsetOverTime, TransformSpace_t space, CDmeDag* pReferenceDag = NULL, bool bPosition = true, bool bRotation = true );

	// Set the current position and orientation as the defaults for the specified dag nodes.
	static void SetDagTransformDefaults( const CUtlVector< CDmeDagPtr > &dagList, bool bPosition, bool bOrientation );

	// Set the controls associated with the specified dag nodes to the reference pose position and orientation
	static void SetReferencePose( CDmeGameModel *pGameModel, const CUtlVector< CDmeDag* > &dagList );

	// Re-parent the specified dag node from its current parent to the specified dag node.
	static void ReParentDagNode( CDmeDag *pDagNode, CDmeDag *pNewParent, bool bMaintainWorldPos, ReParentLogMode_t logMode );

	// Set the temporary override parent of a dag, maintaining its world space position and orientation animation
	static void SetOverrideParent( CDmeDag *pDagNode, const CDmeDag *pOverrideParent, bool bPosition, bool bRotation );

	// Enable or disable the override parent functionality on a dag, if different than the current state the logs will be udpdated such the dag world space position and orientation are maintained.
	static void ToggleOverrideParent( CDmeDag *pDagNode, bool bEnable );

	// Determine if the specified dag node has any constraints
	static bool DagHasConstraints( CDmeDag *pDag );

	// Remove all of the constraints from the specified dag 
	static void RemoveConstraints( CDmeDag *pDag );
	
	// Update the logs of the dag so the the current local transform is the only value in the log
	static void SetLogsToCurrentTransform( CDmeDag *pDag );

	// Generate log samples for the specified dag node, if a parent is provided, generate the samples in the space of that parent, otherwise generate the samples in world space.
	static void GenerateLogSamples( CDmeDag* pDag, CDmeDag *pParent, bool bPosition, bool bOrientation, const DmeLog_TimeSelection_t *pTimeSelection = NULL );
	
	// Find and operate all of the channels driving the specified dag nodes
	static void OperateDagChannels( const CUtlVector< CDmeDag* > &dagList, ChannelMode_t mode, const DmeLog_TimeSelection_t &timeSelection, CDmeClip *pShot = NULL, CDmeClip *pMovie = NULL );

	// Create a constraint of the specified type
	static CDmeRigBaseConstraintOperator *CreateConstraint( const char *pchName, EConstraintType constraintType, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight, bool bOperate );

	// Create a Point constraint which will control the position of the specified dag such that it matches the weighted target position
	static CDmeRigPointConstraintOperator* CreatePointConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight );

	// Create a Orient constraint which will control the orientation of the specified dag such that it matches the weighted target orientation
	static CDmeRigOrientConstraintOperator* CreateOrientConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight );

	// Create a Parent constraint which will control the position and orientation of the specified dag such the dag behaves as if it is a child of the transform defined by the weighted target list
	static CDmeRigParentConstraintOperator* CreateParentConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight );

	// Create an Aim constraint which will control the orientation of the specified dag such that it points toward the weighted target position
	static CDmeRigAimConstraintOperator* CreateAimConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight, const Vector &upVector, TransformSpace_t upSpace, const CDmeDag* pReferenceDag = NULL );
	
	// Create an IK constraint controlling the 2 bone chain from the specified root dag to the specified end dag
	static CDmeRigIKConstraintOperator* CreateIKConstraint( const char *pchName, CDmeDag *pChainRootDag, CDmeDag *pChainEndDag, CDmeDag *pTargetDag, bool bPreserveOffset, const Vector &poleVector, CDmeDag *pPoleVectorTarget = NULL );

	// Print the position and orientation of the dag over time for debugging purposes
	static void PrintDagTransformOverTime( CDmeDag* pDag, CDmeClip *pShot = NULL, CDmeClip *pMovie = NULL );

private:



	// Find the constraint of the specified type controlling the specified dag node
	static CDmeRigBaseConstraintOperator *FindConstraintOnDag( CDmeDag* pDag, EConstraintType constraintType );

	// Allocate an constraint of the specified type.
	static CDmeRigBaseConstraintOperator *InstanceConstraint( char const *pchName, EConstraintType eType, const DmFileId_t &fileId );

	// Find the position or orientation channel for the specified dag node
	static CDmeChannel *FindDagTransformChannel( CDmeDag *pDag, const char *pchAttributeName );

	// Create the position and orientation channels for the specified dag node if they do not already exist.
	static void CreateTransformChannelsForDag( CDmeDag *pDag, CDmeChannelsClip *pChannelsClip, bool bPosition, bool bOrientation, CDmeChannel *&pPositionChannel, CDmeChannel *&pOrientationChannel );

	// Generate a list of all of the world space transform of the specified dag node at each time in the provided list
	static void GenerateDagWorldTransformList( CDmeDag *pDag, const CUtlVector< DmeTime_t > &times, CUtlVector< matrix3x4_t > &transformList, CDmeClip *pShot, CDmeClip *pMovie );
	
	// Update the position and orientation logs of the specified dag node so that the dag node's world space transform matches the provided list
	static void SetDagWorldSpaceTransforms( CDmeDag* pDag, CUtlVector< DmeTime_t > &times, const CUtlVector< matrix3x4_t > &transformList, CDmeClip *pShot, CDmeClip *pMovie );

	// Create a list of all of the key times in the provided list of channels
	static void CompileKeyTimeList( const CUtlVector< CDmeOperator* > &channelList, CUtlVector< DmeTime_t > &times, const DmeLog_TimeSelection_t *pTimeSelection, CDmeClip *pShot, CDmeClip *pMovie );

	// Find the position and orientation controls for the specified dag node
	static CDmeTransformControl *GetTransformControl( const CDmeDag *pDag );

	// Get the local space default transform for the specified dag node
	static void GetDefaultTransform( CDmeDag *pDagNode, matrix3x4_t &defaultTransform );

	// Get the world space default transform for the specified dag node
	static void GetDefaultAbsTransform( CDmeDag *pDagNode, matrix3x4_t &absDefaultTransform );

	// Update the default values for the position and orientation controls of the specified dag so they maintain their world space position 
	static void UpdateDefaultsForNewParent( CDmeDag *pDagNode, CDmeDag *pParentDag );

	// Get all of the channels directly driving the specified dag nodes
	static void GetChannelsForDags( const CUtlVector< CDmeDag* > &dagList, CUtlVector< CDmeChannel* > &channelList );

	// Operate all of the provided channels.
	static void OperateChannels( const CUtlVector< CDmeChannel* > &channelList, const DmeLog_TimeSelection_t &timeSelection, CDmeClip *pShot, CDmeClip *pMovie );

	// Operate all of the provided channels in the specified mode.
	static void RecordChannels( const CUtlVector< CDmeChannel* > &channelList, const DmeLog_TimeSelection_t &timeSelection, CDmeClip *pShot, CDmeClip *pMovie );

	// Find the shot and the movie to which the specified dag node belongs
	static void FindShotAndMovieForDag( const CDmeDag *pDag, CDmeClip *&pShot, CDmeClip *&pMovie );


};

#endif // DMANIMUTILS_H

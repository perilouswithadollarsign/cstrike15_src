//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Implementation of CDmAnimUtils, a set of animation related utilities 
// which work on DmeDag and other DmElement derived objects.
//
//=============================================================================


#include "dmeutils/dmanimutils.h"
#include "movieobjects/dmetransformcontrol.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmetimeselection.h"
#include "movieobjects/dmetrack.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//-------------------------------------------------------------------------------------------------
// Create an infinite time selection
//-------------------------------------------------------------------------------------------------
CDmeTimeSelection *CDmAnimUtils::CreateInfiniteTimeSelection()
{
	CDmeTimeSelection *pTimeSelection = CreateElement< CDmeTimeSelection >( "AnimUtilsTimeSelection", DMFILEID_INVALID );
	pTimeSelection->SetInfinite( 0 );
	pTimeSelection->SetInfinite( 1 );
	return pTimeSelection;
}


//-------------------------------------------------------------------------------------------------
// Create an absolute time selection with the specified key times
//-------------------------------------------------------------------------------------------------
CDmeTimeSelection *CDmAnimUtils::CreateTimeSelection( DmeTime_t leftFalloff, DmeTime_t leftHold, DmeTime_t rightHold, DmeTime_t rightFalloff )
{
	CDmeTimeSelection *pTimeSelection = CreateElement< CDmeTimeSelection >( "AnimUtilsTimeSelection", DMFILEID_INVALID );
	pTimeSelection->SetAbsTime( DMETIME_ZERO, TS_LEFT_FALLOFF,	leftFalloff );
	pTimeSelection->SetAbsTime( DMETIME_ZERO, TS_LEFT_HOLD,		leftHold );
	pTimeSelection->SetAbsTime( DMETIME_ZERO, TS_RIGHT_HOLD,	rightHold );
	pTimeSelection->SetAbsTime( DMETIME_ZERO, TS_RIGHT_FALLOFF, rightFalloff );
	return pTimeSelection;
}


//-------------------------------------------------------------------------------------------------
// Create a CDmeDag instance with the specified name with the specified position and orientation
//-------------------------------------------------------------------------------------------------
CDmeDag *CDmAnimUtils::CreateDag( char const *pchHandleName, const Vector &position, const Quaternion &orientation, CDmeDag *pParent )
{	
	DmFileId_t fileId = DMFILEID_INVALID;

	if ( pParent )
	{
		fileId = pParent->GetFileId();
	}

	// Create the dag element
	CDmeDag *pDag = CreateElement< CDmeDag >( pchHandleName, fileId );
	
	if ( pDag )
	{
		// Add the handle to the scene
		if ( pParent )
		{
			pParent->AddChild( pDag );
		}

		// Position the handle
		pDag->SetAbsPosition( position );
		pDag->SetAbsOrientation( orientation );	
	}

	return pDag;
}


//-------------------------------------------------------------------------------------------------
// Get the average position of the provided dag nodes in the specified space
//-------------------------------------------------------------------------------------------------
Vector CDmAnimUtils::GetDagPosition( const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReferenceDag )
{
	Vector position = vec3_origin;
	Quaternion orientation = quat_identity;
	GetDagPositionOrienation( position, orientation, dagList, space, pReferenceDag );
	return position;
}


//-------------------------------------------------------------------------------------------------
// Get the average Euler rotation of the provided dag nodes in the specified space
//-------------------------------------------------------------------------------------------------
Vector CDmAnimUtils::GetDagRotation( const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReferenceDag )
{
	Vector position = vec3_origin;
	Quaternion orientation = quat_identity;
	GetDagPositionOrienation( position, orientation, dagList, space, pReferenceDag );

	// Convert the quaternion to Euler rotation  
	RadianEuler eulerRotation( orientation );
	Vector rotation = vec3_origin;
	rotation.x =  RAD2DEG( eulerRotation.x );
	rotation.y =  RAD2DEG( eulerRotation.y );
	rotation.z =  RAD2DEG( eulerRotation.z );
	return rotation;
}


//-------------------------------------------------------------------------------------------------
// Get the average orientation (quaternion) of the provided dag nodes in the specified space
//-------------------------------------------------------------------------------------------------
Quaternion CDmAnimUtils::GetDagOrientation( const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReferenceDag )
{
	Vector position = vec3_origin;
	Quaternion orientation = quat_identity;
	GetDagPositionOrienation( position, orientation, dagList, space, pReferenceDag );
	return orientation;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the average position and orientation of the provided dag nodes in the specified 
// space
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::GetDagPositionOrienation( Vector &position, Quaternion &orienation, const CUtlVector< CDmeDagPtr > &dagList, TransformSpace_t space, const CDmeDag *pReference )
{
	int nDagNodes = dagList.Count();
	position = vec3_origin;
	orienation = quat_identity;

	// Return the origin if there are no nodes provided
	if ( nDagNodes < 1 )
		return;

	// Sum the positions of all of the provided dag nodes 
	// and add the orientation of each node to the list.
	CUtlVector< Quaternion > orientationList;
	orientationList.EnsureCapacity( nDagNodes );

	float flDagCount = 0;
	const CDmeDag *pPrimaryDag = NULL;
	Vector positionSum = vec3_origin;
	for ( int iNode = 0; iNode < nDagNodes; ++iNode )
	{	
		const CDmeDag *pDagNode = dagList[ iNode ];
		if ( pDagNode == NULL )
			continue;
 
		// Get the world space position of the dag 
		matrix3x4_t absTransform;
		Vector worldPos;
		Quaternion worldOrientation;
		pDagNode->GetAbsTransform( absTransform );
		MatrixAngles( absTransform, worldOrientation, worldPos );

		positionSum += worldPos;
		orientationList.AddToTail( worldOrientation );
		flDagCount += 1.0f;
		pPrimaryDag = pDagNode;
	}

	// Stop if there were no valid dag nodes
	if ( pPrimaryDag == NULL )
		return;

	// Calculate the average position in world space
	Vector positionAvg = positionSum / flDagCount;
	
	// Calculate the average orienation in world space
	Quaternion orienationAvg;
	QuaternionAverageExponential( orienationAvg, orientationList.Count(), orientationList.Base() );

	// Find the dag node whose space the position is to be returned in
	const CDmeDag *pSpaceDag = NULL;

	switch ( space )
	{
		case TS_LOCAL_SPACE:
			pSpaceDag = pPrimaryDag->GetParent();
			break;
		case TS_OBJECT_SPACE:
			pSpaceDag = pPrimaryDag;
			break;
		case TS_REFERENCE_SPACE:
			pSpaceDag = pReference;
			break;
		default:
			break;
	}

	// Convert the average world space position to the space of the specified dag node
	if ( pSpaceDag )		 
	{
		matrix3x4_t worldToLocal;
		matrix3x4_t localToWorld;
		pSpaceDag->GetAbsTransform( localToWorld );
		MatrixInvert( localToWorld, worldToLocal );
		VectorTransform( positionAvg, worldToLocal, position );

		float rotationAngle;
		Vector wsRotationAxis;
		Vector lsRotationAxis;
		QuaternionAxisAngle( orienationAvg, wsRotationAxis, rotationAngle );
		VectorRotate( wsRotationAxis, worldToLocal, lsRotationAxis );
		AxisAngleQuaternion( lsRotationAxis, rotationAngle, orienation );
	}
	else
	{
		position = positionAvg;
		orienation = orienationAvg;
	}
}


//-------------------------------------------------------------------------------------------------	
// Move the provided dag nodes in the specified space
//-------------------------------------------------------------------------------------------------	
void CDmAnimUtils::MoveDagNodes( const Vector &offset, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, TransformSpace_t space, CDmeDag *pReferenceDag )
{
	TransformDagNodes( offset, quat_identity, dagList, bRelative, space, pReferenceDag, true, false );
}


//-------------------------------------------------------------------------------------------------	
// Move the provided dag nodes in the specified space and apply the operation to the logs 
// associated with the dag
//-------------------------------------------------------------------------------------------------	
void CDmAnimUtils::MoveDagNodes( const Vector &offset, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, const CDmeTimeSelection *pTimeSelection, bool bOffsetOverTime, TransformSpace_t space, CDmeDag *pReferenceDag )
{
	TransformDagNodes( offset, quat_identity, dagList, bRelative, pTimeSelection, bOffsetOverTime, space, pReferenceDag, true, false );
}


//-------------------------------------------------------------------------------------------------	
// Rotate the provided dag nodes in the specified space
//-------------------------------------------------------------------------------------------------	
void CDmAnimUtils::RotateDagNodes( const Vector &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, TransformSpace_t space, CDmeDag* pReferenceDag )
{
	// Rotate the selected dag nodes in the specified space	
	RadianEuler eulerRotation( DEG2RAD( rotation.x ), DEG2RAD( rotation.y ), DEG2RAD( rotation.z ) );
	Quaternion qRotation( eulerRotation );
	TransformDagNodes( vec3_origin, qRotation, dagList, bRelative, space, pReferenceDag, false, true );
}


//-------------------------------------------------------------------------------------------------	
// Rotate the provided dag nodes in the specified space and apply the operation to the logs 
// associated with the dag
//-------------------------------------------------------------------------------------------------	
void CDmAnimUtils::RotateDagNodes( const Vector &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, const CDmeTimeSelection *pTimeSelection, bool bOffsetOverTime, TransformSpace_t space, CDmeDag* pReferenceDag )
{
	// Rotate the selected dag nodes in the specified space	
	RadianEuler eulerRotation( DEG2RAD( rotation.x ), DEG2RAD( rotation.y ), DEG2RAD( rotation.z ) );
	Quaternion qRotation( eulerRotation );
	TransformDagNodes( vec3_origin, qRotation, dagList, bRelative, pTimeSelection, bOffsetOverTime, space, pReferenceDag, false, true );
}


//-------------------------------------------------------------------------------------------------
// Translate an rotate the provided dag nodes in the specified space
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::TransformDagNodes( const Vector &offset, const Quaternion &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, TransformSpace_t space, CDmeDag* pReferenceDag, bool bPosition, bool bRotation )
{
	float rotationAngle;
	Vector rotationAxis;
	matrix3x4_t orienationMat;
	QuaternionAxisAngle( rotation, rotationAxis, rotationAngle );
	QuaternionMatrix( rotation, orienationMat );

	int nDagNodes = dagList.Count();
	for ( int iNode = 0; iNode < nDagNodes; ++iNode )
	{
		const CDmeDag *pDagNode = dagList[ iNode ];
		if ( pDagNode == NULL )
			continue;

		// The value stored in the dag node's transform is in local space ( space of the parent ), 
		// we must convert the offset from the space it is provided in to a value in local space.
		Vector lsOffset;
		Vector lsRotationAxis;
		matrix3x4_t lsOrientationMat;
		matrix3x4_t objectToLocal;
		pDagNode->GetLocalMatrix( objectToLocal );

		if ( space == TS_LOCAL_SPACE )
		{
			// The offset and rotation are provided in local space, nothing needs to be done
			lsOffset = offset;
			lsRotationAxis = rotationAxis;
			lsOrientationMat = orienationMat;
		}
		else if ( space == TS_OBJECT_SPACE )
		{
			// The offset and rotation are provided in object space, it is simply transformed
			// by the object's transform to get it into space of the object's parent.
			VectorTransform( offset, objectToLocal, lsOffset );
			VectorRotate( rotationAxis, objectToLocal, lsRotationAxis );
			ConcatTransforms( orienationMat, objectToLocal, lsOrientationMat);
		}
		else
		{	
			// Calculate the offset to be applied in world space
			Vector wsOffset = offset;
			Vector wsRotationAxis = rotationAxis;
			matrix3x4_t wsOrientationMat = orienationMat;
			if ( pReferenceDag && ( space == TS_REFERENCE_SPACE ) )	 
			{
				matrix3x4_t referenceToWorld;
				pReferenceDag->GetAbsTransform( referenceToWorld );
				VectorTransform( offset, referenceToWorld, wsOffset );
				VectorRotate( rotationAxis, referenceToWorld, wsRotationAxis );
				ConcatTransforms( orienationMat, referenceToWorld, wsOrientationMat );
			}

			// Calculate the offset to be applied to the dag's local space 
			CDmeDag *pParent = pDagNode->GetParent();
			if ( pParent )
			{
				matrix3x4_t localToWorld;
				matrix3x4_t worldToLocal;
				pParent->GetAbsTransform( localToWorld );
				MatrixInvert( localToWorld, worldToLocal );
				VectorRotate( wsRotationAxis, worldToLocal, lsRotationAxis );
				MatrixMultiply( wsOrientationMat, worldToLocal, lsOrientationMat );
				if ( bRelative )
				{
					VectorRotate( wsOffset, worldToLocal, lsOffset );
				}
				else
				{
					VectorTransform( wsOffset, worldToLocal, lsOffset );
				}
			}
			else
			{
				lsOffset = wsOffset;
				lsRotationAxis = wsRotationAxis;
				lsOrientationMat = wsOrientationMat;
			}
		}

		// Now that we have the offset and rotation in local space, they may be applied to the 
		// local transform. They may be applied as an absolute position and orientation, or a 
		// relative offset and rotation.
		Quaternion lsRotation;
		AxisAngleQuaternion( lsRotationAxis, rotationAngle, lsRotation );

		Vector lsPosition = lsOffset;
		Quaternion lsOrientation = quat_identity;

		if ( bRelative )
		{
			Vector lsOrigin;
			MatrixAngles( objectToLocal, lsOrientation, lsOrigin );
			QuaternionMult( lsRotation, lsOrientation, lsOrientation );
			lsPosition = lsOrigin + lsOffset;
		}
		else
		{
			MatrixQuaternion( lsOrientationMat, lsOrientation );
		}

		
		CDmeTransform *pTransform = pDagNode->GetTransform();
		if ( pTransform )
		{
			if ( bPosition )
			{
				pTransform->SetPosition( lsPosition );
			}

			if ( bRotation )
			{
				pTransform->SetOrientation( lsOrientation );
			}
		}

		// Find the position and orientation controls for the transform of the dag node.
		CDmeTransformControl *pTransformControl = GetTransformControl( pDagNode );

		// Assign the new local space position and orienation to the control
		if ( pTransformControl )
		{		
			if ( bPosition )
			{
				pTransformControl->SetPosition( lsPosition );
			}
			if ( bRotation)
			{
				pTransformControl->SetOrientation( lsOrientation );
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Perform both a translation and a rotation of the specified dag nodes and apply the operation to 
// the logs associated with the dag
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::TransformDagNodes( const Vector &offset, const Quaternion &rotation, const CUtlVector< CDmeDagPtr > &dagList, bool bRelative, const CDmeTimeSelection *pTimeSelection, bool bOffsetOverTime, TransformSpace_t space, CDmeDag* pReferenceDag, bool bPosition, bool bRotation )
{
	TransformDagNodes( offset, rotation, dagList, bRelative, space, pReferenceDag, bPosition, bRotation );

	if ( pTimeSelection )
	{
		// Now apply the operation through time
		DmeLog_TimeSelection_t logTimeSelection;
		logTimeSelection.m_bAttachedMode					= pTimeSelection->IsRelative();
		logTimeSelection.m_flThreshold						= pTimeSelection->GetThreshold();
		logTimeSelection.m_nFalloffInterpolatorTypes[ 0 ]	= pTimeSelection->GetFalloffInterpolatorType( 0 );
		logTimeSelection.m_nFalloffInterpolatorTypes[ 1 ]	= pTimeSelection->GetFalloffInterpolatorType( 1 );
		logTimeSelection.m_nResampleInterval				= pTimeSelection->GetResampleInterval();
		logTimeSelection.m_bInfinite[ 0 ]					= pTimeSelection->IsInfinite( 0 );
		logTimeSelection.m_bInfinite[ 1 ]					= pTimeSelection->IsInfinite( 1 );


		if ( logTimeSelection.m_bInfinite[ 0 ] )
		{
			logTimeSelection.m_nTimes[ TS_LEFT_FALLOFF ] = DMETIME_MAXTIME;
			logTimeSelection.m_nTimes[ TS_LEFT_HOLD ] = DMETIME_MAXTIME;
		}
		else
		{
			logTimeSelection.m_nTimes[ TS_LEFT_FALLOFF ] = pTimeSelection->GetAbsTime( DMETIME_ZERO, TS_LEFT_FALLOFF );
			logTimeSelection.m_nTimes[ TS_LEFT_HOLD ] = pTimeSelection->GetAbsTime( DMETIME_ZERO, TS_LEFT_HOLD );
		}

		if ( logTimeSelection.m_bInfinite[ 1 ] )
		{
			logTimeSelection.m_nTimes[ TS_RIGHT_FALLOFF ] = DMETIME_MINTIME;
			logTimeSelection.m_nTimes[ TS_RIGHT_HOLD ] = DMETIME_MINTIME;
		}
		else
		{
			logTimeSelection.m_nTimes[ TS_RIGHT_HOLD ] = pTimeSelection->GetAbsTime( DMETIME_ZERO, TS_RIGHT_HOLD );
			logTimeSelection.m_nTimes[ TS_RIGHT_FALLOFF ] = pTimeSelection->GetAbsTime( DMETIME_ZERO, TS_RIGHT_FALLOFF );
		}
		
		// Apply the results to the channels of the selected dag nodes
		if ( bOffsetOverTime )
		{
			logTimeSelection.m_TransformWriteMode = TRANSFORM_WRITE_MODE_OFFSET;
			logTimeSelection.SetRecordingMode( RECORD_ATTRIBUTESLIDER );
		}
		OperateDagChannels( dagList, CM_RECORD, logTimeSelection, NULL, NULL );
	}
}


//-------------------------------------------------------------------------------------------------
// Set the current position and orientation as the defaults for the specified dag nodes.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::SetDagTransformDefaults( const CUtlVector< CDmeDagPtr > &dagList, bool bPosition, bool bOrientation )
{
	if ( ( bPosition == false ) && ( bOrientation == false ) )
		return;

	int nDagNodes = dagList.Count();
	for ( int iDagNode = 0; iDagNode < nDagNodes; ++iDagNode )
	{
		const CDmeDag *pDagNode = dagList[ iDagNode ];
		if ( pDagNode == NULL )
			continue;

		CDmeTransform *pTransform = pDagNode->GetTransform();
		if ( pTransform == NULL )
			continue;
		

		CDmeTransformControl *pTransformControl = GetTransformControl( pDagNode );
		if ( pTransformControl )
		{
			if ( bPosition )
			{
				const Vector &position = pTransform->GetPosition();
				pTransformControl->SetDefaultPosition( position );
			}
			if ( bOrientation )
			{
				const Quaternion &orientation = pTransform->GetOrientation();
				pTransformControl->SetDefaultOrientation( orientation );
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Set all of the controls targeting bones in the animation set to the reference pose 
// position and orientation
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::SetReferencePose( CDmeGameModel *pGameModel, const CUtlVector< CDmeDag* > &dagList )
{	
	if ( pGameModel == NULL )
		return;

	int nDagNodes = dagList.Count();
	for ( int iDagNode = 0; iDagNode < nDagNodes; ++iDagNode )
	{
		const CDmeDag *pDagNode = dagList[ iDagNode ];
		if ( pDagNode == NULL )
			continue;

		CDmeTransform *pTransform = pDagNode->GetTransform();
		if ( pTransform == NULL )
			continue;
			
		// Find the bone corresponding to the control
		int boneIndex = pGameModel->FindBone( pTransform );

		// Skip the dag if there is no corresponding bone unless it is the root
		if ( ( boneIndex < 0 ) && ( pDagNode != pGameModel ) )
			continue;


		CDmeTransformControl *pTransformControl = GetTransformControl( pDagNode );
		
		if ( pTransformControl )
		{
			Vector position = vec3_origin;
			pGameModel->GetBoneDefaultPosition( boneIndex, position );
			pTransformControl->SetPosition( position );
		
			Quaternion orientation = quat_identity;
			pGameModel->GetBoneDefaultOrientation( boneIndex, orientation );
			pTransformControl->SetOrientation( orientation );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Update the default values for the position and orientation controls of the specified dag so they
// maintain their world space position 
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::UpdateDefaultsForNewParent( CDmeDag *pDagNode, CDmeDag *pParentDag )
{
	// Calculate the transform needed to convert from the 
	// space of the current parent to the new parent.
	matrix3x4_t worldToNewParent;
	matrix3x4_t defaultWorldToNewParent;
	if ( pParentDag )
	{
		matrix3x4_t parentToWorld;
		pParentDag->GetAbsTransform( parentToWorld );
		MatrixInvert( parentToWorld, worldToNewParent );

		matrix3x4_t defaultParentToWorld;
		GetDefaultAbsTransform( pParentDag, defaultParentToWorld );
		MatrixInvert( defaultParentToWorld, defaultWorldToNewParent );
	}
	else
	{
		SetIdentityMatrix( worldToNewParent );
		SetIdentityMatrix( defaultWorldToNewParent );
	}

	matrix3x4_t oldToNewParentMat;
	matrix3x4_t defaultOldToNewParentMat;
	CDmeDag *pOldParent = pDagNode->GetParent();
	if ( pOldParent )
	{
		matrix3x4_t oldParentToWorld;
		pOldParent->GetAbsTransform( oldParentToWorld );
		ConcatTransforms( worldToNewParent, oldParentToWorld, oldToNewParentMat );

		matrix3x4_t defaultOldParentToWorld;
		GetDefaultAbsTransform( pOldParent, defaultOldParentToWorld);
		ConcatTransforms( defaultWorldToNewParent, defaultOldParentToWorld, defaultOldToNewParentMat );
	}
	else
	{
		oldToNewParentMat = worldToNewParent;
		defaultOldToNewParentMat = defaultWorldToNewParent;
	}

	CDmeTransformControl *pTransformControl = pDagNode->FindTransformControl();
	if ( pTransformControl )
	{	
		// Move the position value into the space of the new parent
		Vector position = pTransformControl->GetPosition();
		Vector newPosition;
		VectorTransform( position, oldToNewParentMat, newPosition );
		pTransformControl->SetPosition( newPosition );

		// Move the default position into the space of new parent
		if ( pTransformControl->HasDefaultPosition() )
		{			
			Vector newPosition;	
			Vector position = pTransformControl->GetDefaultPosition();
			VectorTransform( position, defaultOldToNewParentMat, newPosition );
			pTransformControl->SetDefaultPosition( newPosition );
		}
	
		// Move the orientation value into the space of the new parent
		Quaternion orientation = pTransformControl->GetOrientation();
		Quaternion newOrientation;
		matrix3x4_t oldOrientMat, newOrientMat;
		QuaternionMatrix( orientation, oldOrientMat );
		ConcatTransforms( oldToNewParentMat, oldOrientMat, newOrientMat );
		MatrixQuaternion( newOrientMat, newOrientation );
		pTransformControl->SetOrientation( newOrientation );
		
		// Move the default orientation into the space of the new parent
		if ( pTransformControl->HasDefaultOrientation() )
		{
			Quaternion orientation = pTransformControl->GetDefaultOrientation();
			Quaternion newOrientation;
			matrix3x4_t oldOrientMat, newOrientMat;
			QuaternionMatrix( orientation, oldOrientMat );
			ConcatTransforms( defaultOldToNewParentMat, oldOrientMat, newOrientMat );
			MatrixQuaternion( newOrientMat, newOrientation );
			pTransformControl->SetDefaultOrientation( newOrientation );
		}
	}
	
}


//-------------------------------------------------------------------------------------------------
// Purpose: Re-parent the specified dag node from its current parent to the specified dag node.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::ReParentDagNode( CDmeDag *pDagNode, CDmeDag *pParentDag, bool bMaintainWorldPos, ReParentLogMode_t logMode )
{
	if ( pDagNode == NULL ) 
		return;

	CDmeTransform *pTransform = pDagNode->GetTransform();
	
	if ( bMaintainWorldPos && pTransform )
	{		
		// Modify the default values of the controls on the dag so that they maintain
		// their world space position when the specified dag node is made the new parent.
		UpdateDefaultsForNewParent( pDagNode, pParentDag );

		// Get the current world space matrix of the dag
		matrix3x4_t currentWorldMat;
		pDagNode->GetAbsTransform( currentWorldMat );

		// Get the current world space matrix of the new parent 
		matrix3x4_t parentWorldMat;
		if ( pParentDag )
		{
			pParentDag->GetAbsTransform( parentWorldMat );
		}
		else
		{
			SetIdentityMatrix( parentWorldMat );
		}

		matrix3x4_t invParentMat;
		MatrixInvert( parentWorldMat, invParentMat );

		// If an update of the logs is requested re-generate the local logs of the dag so that the same
		// world space location is maintained once the dag is a child of the specified parent.
		if ( logMode == REPARENT_LOGS_MAINTAIN_WORLD )
		{
			// Determine if the dag has existing position and orientation samples
			CDmeChannel *pPosChannel = FindDagTransformChannel( pDagNode, TRANSFORM_POSITION );
			CDmeChannel *pRotChannel = FindDagTransformChannel( pDagNode, TRANSFORM_ORIENTATION );

			// Generate the log samples in the space of the new parent
			GenerateLogSamples( pDagNode, pParentDag, pPosChannel != NULL, pRotChannel != NULL, NULL );
		}
		else if ( logMode == REPARENT_LOGS_OFFSET_LOCAL )
		{
			// Compute the transform from the space defined by the 
			// current parent to the space defined by the new parent.
			matrix3x4_t currentParentMat;
			pDagNode->GetParentWorldMatrix( currentParentMat );
			matrix3x4_t oldToNewParentMat;
			ConcatTransforms( invParentMat, currentParentMat, oldToNewParentMat );

			// Apply the transform to all the samples of the position channel of the dag
			CDmeChannel *pPosChannel = FindDagTransformChannel( pDagNode, TRANSFORM_POSITION );
			if ( pPosChannel )
			{
				CDmeTypedLogLayer< Vector > *pPositionLog = CastElement< CDmeTypedLogLayer< Vector > >( pPosChannel->GetLog() );
				RotatePositionLog( pPositionLog, oldToNewParentMat );
			}

			// Apply the the transform to all the samples of the orientation channel of the dag
			CDmeChannel *pRotChannel = FindDagTransformChannel( pDagNode, TRANSFORM_ORIENTATION );
			if ( pRotChannel )
			{
				CDmeTypedLogLayer< Quaternion > *pRotationLog = CastElement< CDmeTypedLogLayer< Quaternion > >( pRotChannel->GetLog() );
				RotateOrientationLog( pRotationLog, oldToNewParentMat, true );
			}
		}
	
		// Compute the new local matrix of the transform which will maintain the world space position of the dag
		matrix3x4_t newLocalMat;
		ConcatTransforms( invParentMat, currentWorldMat, newLocalMat );
		pTransform->SetTransform( newLocalMat );
	}

	// Remove the child from all parents
	CUtlVector< CDmeDag* > parents;
	FindAncestorsReferencingElement( pDagNode, parents );
	int nParents = parents.Count();
	for ( int i = 0; i < nParents; ++i )
	{
		parents[ i ]->RemoveChild( pDagNode );
	}

	// Add the child to the specified parent
	if ( pParentDag )
	{
		pParentDag->AddChild( pDagNode );
	}

	if ( logMode == REPARENT_LOGS_OVERWRITE )
	{
		SetLogsToCurrentTransform( pDagNode );
	}
}


//-------------------------------------------------------------------------------------------------
// Set the temporary override parent of a dag, maintaining its world space position and orientation
// animation
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::SetOverrideParent( CDmeDag *pDagNode, const CDmeDag *pOverrideParent, bool bPosition, bool bRotation )
{
	if ( pDagNode == NULL )
		return;

	CDmeTransform *pTransform = pDagNode->GetTransform();
	if ( pTransform == NULL )
		return;

	// Do not allow the override parent to be set to a dag that is a child of the specified dag.
	if ( pDagNode->IsAncestorOfDag( pOverrideParent ) || ( pDagNode == pOverrideParent ) )
		return;

	// If both position and rotation are false treat it the same as if pOverrideParent is NULL.
	if ( !bPosition && !bRotation )
	{
		pOverrideParent = NULL;
	}

	// Get the shot and movie that the specified dag node belongs to for use in time conversions
	CDmeClip *pShot = NULL;
	CDmeClip *pMovie = NULL;
	FindShotAndMovieForDag( pDagNode, pShot, pMovie );

	// Find the channels which will be needed to evaluate
	// the position of the child and the parent dag.
	CUtlVector< CDmeOperator* > operatorList;
	pDagNode->FindRelevantOperators( operatorList );
	if ( pOverrideParent )
	{
		pOverrideParent->FindRelevantOperators( operatorList );
	}

	// Build a list of all of the key times for the operators
	CUtlVector< DmeTime_t > keyTimes;
	CompileKeyTimeList( operatorList, keyTimes, NULL, pShot, pMovie );

	// Get the world space transform of the dag at all the key times
	CUtlVector< matrix3x4_t > worldSpaceTransforms;
	GenerateDagWorldTransformList( pDagNode, keyTimes, worldSpaceTransforms, pShot, pMovie );
	
	// Get the current world space matrix of the dag
	matrix3x4_t currentWorldMat;
	pDagNode->GetAbsTransform( currentWorldMat );

	// Now set the temporary override parent, note if NULL 
	// is specified the override will simply be removed.
	pDagNode->SetOverrideParent( pOverrideParent, bPosition, bRotation );

	// Update the logs of the dag node so that it matches 
	// its previous world space transform at all times
	SetDagWorldSpaceTransforms( pDagNode, keyTimes, worldSpaceTransforms, pShot, pMovie );

	// Compute the new local matrix of the transform which 
	// will maintain the world space position of the dag
	pDagNode->SetAbsTransform( currentWorldMat );
}


//-------------------------------------------------------------------------------------------------
// Enable or disable the override parent functionality on a dag, if different than the current
// state the logs will be updated such the dag world space position and orientation are maintained.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::ToggleOverrideParent( CDmeDag *pDagNode, bool bEnable )
{
	if ( pDagNode == NULL )
		return;
	
	CDmeTransform *pTransform = pDagNode->GetTransform();
	if ( pTransform == NULL )
		return;

	// If the enable state matches the requested state nothing needs to be done.
	if ( pDagNode->IsOverrideParentEnabled() == bEnable )
		return;

	// Get the current override parent regardless of the override state
	const CDmeDag *pOverrideParent = pDagNode->GetOverrideParent( true );
	if ( pOverrideParent == NULL )
		return;

	// Get the shot and movie that the specified dag node belongs to for use in time conversions
	CDmeClip *pShot = NULL;
	CDmeClip *pMovie = NULL;
	FindShotAndMovieForDag( pDagNode, pShot, pMovie );

	// Find the channels which will be needed to evaluate
	// the position of the child and the parent dag.
	CUtlVector< CDmeOperator* > operatorList;
	pDagNode->FindRelevantOperators( operatorList );
	if ( pOverrideParent )
	{
		pOverrideParent->FindRelevantOperators( operatorList );
	}

	// Build a list of all of the key times for the operators
	CUtlVector< DmeTime_t > keyTimes;
	CompileKeyTimeList( operatorList, keyTimes, NULL, pShot, pMovie );

	// Get the world space transform of the dag at all the key times
	CUtlVector< matrix3x4_t > worldSpaceTransforms;
	GenerateDagWorldTransformList( pDagNode, keyTimes, worldSpaceTransforms, pShot, pMovie );
	
	// Get the current world space matrix of the dag
	matrix3x4_t currentWorldMat;
	pDagNode->GetAbsTransform( currentWorldMat );

	// Now change the override parent enable state and update 
	// the local transforms to match the world transforms
	pDagNode->EnableOverrideParent( bEnable );

	// Update the logs of the dag node so that it matches 
	// its previous world space transform at all times
	SetDagWorldSpaceTransforms( pDagNode, keyTimes, worldSpaceTransforms, pShot, pMovie );

	// Compute the new local matrix of the transform which 
	// will maintain the world space position of the dag
	pDagNode->SetAbsTransform( currentWorldMat );
}


//-------------------------------------------------------------------------------------------------
// Determine if the specified dag node has any constraints
//-------------------------------------------------------------------------------------------------
bool CDmAnimUtils::DagHasConstraints( CDmeDag *pDag )
{
	if ( pDag != NULL )
	{
		// Find the constraint slaves referencing this dag
		CUtlVector< CDmeConstraintSlave* > constraintSlaves( 0, 8 );
		FindAncestorsReferencingElement( pDag, constraintSlaves );

		int nSlaves = constraintSlaves.Count();
		for ( int iSlave = 0; iSlave < nSlaves; ++iSlave )
		{
			CDmeRigBaseConstraintOperator *pConstraint = FindAncestorReferencingElement< CDmeRigBaseConstraintOperator >( constraintSlaves[ iSlave ] );
			if ( pConstraint )
				return true;
		}
	}
	return false;
}
	

//-------------------------------------------------------------------------------------------------
//  Remove all constraints from the currently selected dag nodes.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::RemoveConstraints( CDmeDag *pDag )
{
	CDmeRigBaseConstraintOperator::RemoveConstraintsFromDag( pDag );		
}


//-------------------------------------------------------------------------------------------------
// Create a constraint of the specified type
//-------------------------------------------------------------------------------------------------
CDmeRigBaseConstraintOperator *CDmAnimUtils::CreateConstraint( const char *pchName, EConstraintType constraintType, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetList, bool bPreserveOffset, float flTargetWeight, bool bOperate )
{
	// Verify the input parameters are valid.
	if ( ( pConstrainedDag == NULL ) || ( targetList.Count() <= 0 ) )
		return NULL;

	// IK constraints should not be created through this path 
	// since their setup is significantly different
	if ( constraintType == CT_IK )
		return NULL;
	

	// First see if there is already an existing constraint of the specified type on the dag node.
	CDmeRigBaseConstraintOperator *pConstraint = FindConstraintOnDag( pConstrainedDag, constraintType );

	// Create a constraint if one of the specified type did not already exist on the dag node.
	if ( pConstraint == NULL )
	{
		pConstraint = InstanceConstraint( pchName, constraintType, pConstrainedDag->GetFileId() );
		if ( pConstraint == NULL )
			return NULL;

		// Set the slave dag which will be controlled by the constraint
		pConstraint->SetSlave( pConstrainedDag );

		// Disconnect the channels from the attributes of slave transform that are written by the constraint.
		pConstraint->DisconnectTransformChannels();
	}

	// Initialize the weights
	int nTargets = targetList.Count();
	CUtlVector< float > weights;
	weights.SetCount( nTargets );
	for ( int i = 0; i < nTargets; ++i )
	{
		weights[ i ] = flTargetWeight;
	}

	// Add the handles to the constraint or update their weights if they already exist.
	pConstraint->AddHandles( nTargets, targetList.Base(), weights.Base(), bPreserveOffset, NULL );

	// If specified operate the constraint so that its result is available immediately
	if ( bOperate )
	{
		pConstraint->Operate();
	}
		
	return pConstraint;	
}


//-------------------------------------------------------------------------------------------------
// Create a Point constraint which will control the position of the specified dag such that it 
// matches the weighted target position
//-------------------------------------------------------------------------------------------------
CDmeRigPointConstraintOperator* CDmAnimUtils::CreatePointConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight )
{
	CDmeRigBaseConstraintOperator *pConstraint = CreateConstraint( pchName, CT_POINT, pConstrainedDag, targetDagList, bPreserveOffset, flWeight, true );
	return CastElement< CDmeRigPointConstraintOperator >( pConstraint );
}


//-------------------------------------------------------------------------------------------------
// Create a Orient constraint which will control the orientation of the specified dag such that it
// matches the weighted target orientation
//-------------------------------------------------------------------------------------------------
CDmeRigOrientConstraintOperator* CDmAnimUtils::CreateOrientConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight )
{
	CDmeRigBaseConstraintOperator *pConstraint = CreateConstraint( pchName, CT_ORIENT, pConstrainedDag, targetDagList, bPreserveOffset, flWeight, true );
	return CastElement< CDmeRigOrientConstraintOperator >( pConstraint );
}


//-------------------------------------------------------------------------------------------------
// Create a Parent constraint which will control the position and orientation of the specified dag 
// such the dag behaves as if it is a child of the transform defined by the weighted target list
//-------------------------------------------------------------------------------------------------
CDmeRigParentConstraintOperator* CDmAnimUtils::CreateParentConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight )
{
	CDmeRigBaseConstraintOperator *pConstraint = CreateConstraint( pchName, CT_PARENT, pConstrainedDag, targetDagList, bPreserveOffset, flWeight, true );
	return CastElement< CDmeRigParentConstraintOperator >( pConstraint );
}


//-------------------------------------------------------------------------------------------------
// Create an Aim constraint which will control the orientation of the specified dag such the it 
// points toward the weighted target position
//-------------------------------------------------------------------------------------------------
CDmeRigAimConstraintOperator* CDmAnimUtils::CreateAimConstraint( char const *pchName, CDmeDag *pConstrainedDag, const CUtlVector< CDmeDagPtr > &targetDagList, bool bPreserveOffset, float flWeight, const Vector &upVector, TransformSpace_t upSpace, const CDmeDag* pReferenceDag )
{
	CDmeRigAimConstraintOperator *pAimConstraint = NULL;

	CDmeRigBaseConstraintOperator *pConstraint = CreateConstraint( pchName, CT_AIM, pConstrainedDag, targetDagList, bPreserveOffset, flWeight, false );

	if ( pConstraint )
	{
		pAimConstraint = CastElement< CDmeRigAimConstraintOperator >( pConstraint );
		if ( pAimConstraint )
		{
			// Translate all spaces into a dag node by which the space is defined
			if ( upSpace != TS_REFERENCE_SPACE )
			{
				pReferenceDag = NULL;
				const CDmeDag *pSlave = pAimConstraint->GetSlave();
				if ( pSlave && ( upSpace == TS_OBJECT_SPACE ) )
				{
					pReferenceDag = pSlave->GetParent();
				}
			}

			pAimConstraint->SetUpVector( upVector, bPreserveOffset, pReferenceDag );
			pAimConstraint->Operate();
		}
	}

	return pAimConstraint;
}


//-------------------------------------------------------------------------------------------------
// Create an ik constraint controlling the 2 bone chain from the specified root dag to the 
// specified end dag.
//-------------------------------------------------------------------------------------------------
CDmeRigIKConstraintOperator* CDmAnimUtils::CreateIKConstraint( const char *pchName, CDmeDag *pStartNode, CDmeDag *pEndNode, CDmeDag *pTargetDag, bool bPreserveOffset, const Vector &poleVector, CDmeDag *pPoleVectorTarget )
{		
	// All of the input dag nodes must be specified
	if ( pStartNode == NULL || pEndNode == NULL || pTargetDag == NULL )
		return NULL;
	
	// Find the middle node and make sure the chain is valid.
	CDmeDag *pMiddleNode = pEndNode->GetParent();
	if ( pMiddleNode == NULL )
		return NULL;
	if ( pMiddleNode->GetParent() != pStartNode )
		return NULL;

	// Make sure there are no existing ik constraints on the nodes
	if ( FindConstraintOnDag( pStartNode, CT_IK ) != NULL )
		return NULL;
	if ( FindConstraintOnDag( pMiddleNode, CT_IK ) != NULL ) 
		return NULL;
	if ( FindConstraintOnDag( pEndNode, CT_IK ) != NULL ) 
		return NULL;

	// Create the ik constraint
	CDmeRigIKConstraintOperator *pIKConstraint = CreateElement< CDmeRigIKConstraintOperator >( pchName, pStartNode->GetFileId() );
	if ( pIKConstraint == NULL )
		return NULL;

	// Set the joints that are controlled by the constraint
	pIKConstraint->SetJoints( pStartNode, pMiddleNode, pEndNode );

	// Disconnect the channels from the attributes of slave transform that are written by the constraint.
	pIKConstraint->DisconnectTransformChannels();

	// Add the handles to the constraint or update their weights if they already exist.
	pIKConstraint->AddHandles( 1, &pTargetDag, NULL, bPreserveOffset, NULL );

	// Initialize the pole vector, if a pole vector target dag is specified
	// it takes precedence over the pole vector direction
	pIKConstraint->SetPoleVector( poleVector );
	pIKConstraint->SetPoleVectorTarget( pPoleVectorTarget );

	// Initialize and run the constraint 
	pIKConstraint->Setup( bPreserveOffset );
	pIKConstraint->Operate();
	
	return pIKConstraint;
}


//-------------------------------------------------------------------------------------------------
// Allocate a constraint of the specified type.
//-------------------------------------------------------------------------------------------------
CDmeRigBaseConstraintOperator *CDmAnimUtils::InstanceConstraint( char const *pchName, EConstraintType eType, const DmFileId_t &fileId )
{
	CDmeRigBaseConstraintOperator *pOperator = NULL;

	switch ( eType )
	{
		case CT_POINT:
			{
				pOperator = CreateElement< CDmeRigPointConstraintOperator >( pchName, fileId );
			}
			break;
		case CT_ORIENT:
			{
				pOperator = CreateElement< CDmeRigOrientConstraintOperator >( pchName, fileId );
			}
			break;
		case CT_AIM:
			{
				pOperator = CreateElement< CDmeRigAimConstraintOperator >( pchName, fileId );
			}
			break;
		case CT_IK:
			{
				pOperator = CreateElement< CDmeRigIKConstraintOperator >( pchName, fileId );
			}
			break;
		case CT_PARENT:
			{
				pOperator = CreateElement< CDmeRigParentConstraintOperator>( pchName, fileId );
			}
			break;
		default:
			return NULL;
	}

	return pOperator;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find the position or orientation channel for the specified dag node
//-------------------------------------------------------------------------------------------------
CDmeChannel *CDmAnimUtils::FindDagTransformChannel( CDmeDag* pDag, const char *pchAttributeName )
{
	if ( pDag == NULL )
		return NULL;

	CDmeChannel *pChannel = FindChannelTargetingElement( pDag->GetTransform(), pchAttributeName );

	// If no channel was attach directly to the dag, check for a 
	// channel attached to constraint that is driving the dag node.
	if ( pChannel == NULL )
	{
		// Find the constraints associated with the specified dag node.
		CUtlVector< CDmeConstraintSlave* > constraintSlaves;
		FindAncestorsReferencingElement( pDag, constraintSlaves );

		int nSlaves = constraintSlaves.Count();
		for ( int i = 0; i < nSlaves; ++i )
		{
			CDmeConstraintSlave *pSlave = constraintSlaves[ i ];
			if ( pSlave == NULL )
				continue;

			// Find the channel targeting the constraint slave
			pChannel = FindChannelTargetingElement( pSlave, pchAttributeName );
			if ( pChannel != NULL )
				break;
		}
	}
	
	return pChannel;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find the constraint of the specified type controlling the specified dag node
//-------------------------------------------------------------------------------------------------
CDmeRigBaseConstraintOperator *CDmAnimUtils::FindConstraintOnDag( CDmeDag* pDag, EConstraintType constraintType )
{
	if ( pDag == NULL )
		return NULL;

	// Find the constraints associated with the specified dag node.
	CUtlVector< CDmeConstraintSlave* > constraintSlaves;
	FindAncestorsReferencingElement( pDag, constraintSlaves );

	int nSlaves = constraintSlaves.Count();
	for ( int i = 0; i < nSlaves; ++i )
	{
		CDmeConstraintSlave *pSlave = constraintSlaves[ i ];
		if ( pSlave == NULL )
			continue;

		CDmeRigBaseConstraintOperator *pConstraint = pSlave->GetConstraint();
		if ( pConstraint )
		{
			if ( pConstraint->GetConstraintType() == constraintType )
			{
				return pConstraint;
			}
		}
	}

	return NULL;
}


//-------------------------------------------------------------------------------------------------
// Create the position and orientation channels for the specified dag node if they do not already 
// exist.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::CreateTransformChannelsForDag( CDmeDag *pDag, CDmeChannelsClip *pChannelsClip, bool bPosition, bool bOrientation, CDmeChannel *&pPosChannel, CDmeChannel *&pRotChannel )
{
	pPosChannel = NULL;
	pRotChannel = NULL;

	if ( pDag == NULL )
		return;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return;
	
	CDmeTransformControl *pTransformControl = NULL;

	// Create the position channel if it does not exist
	if ( bPosition )
	{
		pPosChannel = FindDagTransformChannel( pDag, TRANSFORM_POSITION );
		if ( pPosChannel )
		{
			pTransformControl = CastElement< CDmeTransformControl >( pPosChannel->GetFromElement() );
		}
		else
		{
			char name[ 256 ];
			V_snprintf( name, sizeof( name ), "%s_p", pDag->GetName() );
			pPosChannel = CreateElement< CDmeChannel >( name, pDag->GetFileId() );
			pPosChannel->SetMode( CM_PLAY );
			pPosChannel->CreateLog( AT_VECTOR3 );
			pPosChannel->SetOutput( pTransform, "position" );
			if ( pChannelsClip )
			{
				pChannelsClip->m_Channels.AddToTail( pPosChannel );
			}
		}
	}

	// Create the orientation channel if it does not exist
	if ( bOrientation )
	{
		pRotChannel = FindDagTransformChannel( pDag, TRANSFORM_ORIENTATION );
		if ( pRotChannel )
		{
			pTransformControl = CastElement< CDmeTransformControl >( pRotChannel->GetFromElement() );
		}
		else
		{
			char name[ 256 ];
			V_snprintf( name, sizeof( name ), "%s_o", pDag->GetName() );
			pRotChannel = CreateElement< CDmeChannel >( name, pDag->GetFileId() );
			pRotChannel->SetMode( CM_PLAY );
			pRotChannel->CreateLog( AT_QUATERNION );
			pRotChannel->SetOutput( pTransform, "orientation" );
			if ( pChannelsClip )
			{
				pChannelsClip->m_Channels.AddToTail( pRotChannel );
			}
		}
	}

	// If either channel is valid make sure a transform control is properly attached
	if ( pPosChannel || pRotChannel )
	{			
		if ( pTransformControl == NULL )
		{
			pTransformControl = CreateElement< CDmeTransformControl >( pDag->GetName(), pDag->GetFileId() );
		}
		
		if ( pPosChannel )
		{
			pTransformControl->SetPosition( pTransform->GetPosition() );
			pTransformControl->SetPositionChannel( pPosChannel );
			pPosChannel->SetInput( pTransformControl->GetPositionAttr() );
		}

		if ( pRotChannel )
		{
			pTransformControl->SetOrientation( pTransform->GetOrientation() );
			pTransformControl->SetOrientationChannel( pRotChannel );
			pRotChannel->SetInput( pTransformControl->GetOrientationAttr() );
		}
	}

}


//-------------------------------------------------------------------------------------------------
// Print the position and orientation of the dag over time for debugging purposes
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::PrintDagTransformOverTime( CDmeDag* pDag, CDmeClip *pShot, CDmeClip *pMovie )
{
	
	// Find the channels which will be needed to evaluate
	// the position of the child and the parent dag.
	CUtlVector< CDmeOperator* > operatorList;
	pDag->FindRelevantOperators( operatorList );
		
	CUtlVector< DmeTime_t > keyTimes;
	CompileKeyTimeList( operatorList, keyTimes, NULL, pShot, pMovie );

	CUtlVector< matrix3x4_t > transformList;
	GenerateDagWorldTransformList( pDag, keyTimes, transformList, pShot, pMovie );

	int nNumTransforms = transformList.Count();
	for ( int i = 0; i < nNumTransforms; ++i )
	{
		Vector pos;
		QAngle rot;
		MatrixAngles( transformList[ i ], rot, pos );
		Msg( "%i : pos < %f, %f, %f >, rot< %f, %f, %f >\n", keyTimes[ i ].GetTenthsOfMS(), pos.x, pos.y, pos.z, rot.x, rot.y, rot.z );
		
	}
}


//-------------------------------------------------------------------------------------------------
// Update the logs of the dag so the the current local transform is the only value in the log
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::SetLogsToCurrentTransform( CDmeDag *pDag )
{
	if ( pDag == NULL )
		return;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return;
		
	matrix3x4_t mTransform;
	pTransform->GetTransform( mTransform );

	Vector pos;
	Quaternion rot;
	MatrixAngles( mTransform, rot, pos );

	CDmeChannel *pPosChannel = FindDagTransformChannel( pDag, TRANSFORM_POSITION );
	if ( pPosChannel )
	{
		CDmeTypedLog< Vector > *pPosLog = CastElement< CDmeTypedLog< Vector > >( pPosChannel->GetLog() );
		if ( pPosLog )
		{
			pPosLog->ClearKeys();
			pPosLog->SetKey( pPosChannel->GetCurrentTime(), pos );
		}
	}

	CDmeChannel *pRotChannel = FindDagTransformChannel( pDag, TRANSFORM_ORIENTATION );
	if ( pRotChannel )
	{	
		CDmeTypedLog< Quaternion > *pRotLog = CastElement< CDmeTypedLog< Quaternion > >( pRotChannel->GetLog() );
		if ( pRotLog )
		{
			pRotLog->ClearKeys();
			pRotLog->SetKey( pRotChannel->GetCurrentTime(), rot );
		}
	}

}


//-------------------------------------------------------------------------------------------------
// Purpose: Generate log samples for the specified dag node, if a parent is provided, generate the 
// samples in the space of that parent, otherwise generate the samples in world space.
// 
// Input : pDag - Pointer to the dag node for which log samples are to be generated.
// Input : pParent - Pointer to the dag node whose space samples are to be generated in
// Input : bPosition - Flag indicating if the position log of the transform will be modified
// Input : bRotation - Flag indicating if the rotation log of the transform will be modified
// Input : pTimeSelection - Pointer to a time selection over which the samples are to be generated,
//			if NULL samples will be generated at all key points for the log.
//
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::GenerateLogSamples( CDmeDag* pDag, CDmeDag *pParent, bool bPosition, bool bRotation, const DmeLog_TimeSelection_t *pTimeSelection )
{
	if ( pDag == NULL )
		return;

	CDmeClip *pShot = NULL;
	CDmeClip *pMovie = NULL;
	FindShotAndMovieForDag( pDag, pShot, pMovie );

	// If position and rotation are both false then there is nothing to do.
	if ( !bPosition && !bRotation )
		return;


	// If a time selection is provided use the re-sample interval and perform samples through the 
	// time selection, otherwise build a list of all of the key times for the relevant channels
	CUtlVector< DmeTime_t > keyTimes;

	// Find the channels which will be needed to evaluate
	// the position of the child and the parent dag.
	CUtlVector< CDmeOperator* > operatorList;
	pDag->FindRelevantOperators( operatorList );
	if ( pParent )
	{
		pParent->FindRelevantOperators( operatorList );
	}

	CompileKeyTimeList( operatorList, keyTimes, pTimeSelection, pShot, pMovie );
	

	// Get the world space transform of the parent and the child at all of the key times
	CUtlVector< matrix3x4_t > parentTransformList;
	CUtlVector< matrix3x4_t > childTransformList;
	GenerateDagWorldTransformList( pDag, keyTimes, childTransformList, pShot, pMovie );
	if ( pParent )
	{
		GenerateDagWorldTransformList( pParent, keyTimes, parentTransformList, pShot, pMovie );
	}

	// Find the existing channels associated with the position and orientation of the dag transform
	CDmeChannel *pPosChannel = FindDagTransformChannel( pDag, TRANSFORM_POSITION );
	CDmeChannel *pRotChannel = FindDagTransformChannel( pDag, TRANSFORM_ORIENTATION );

	// Find the channels clip containing the position and rotation channel. They should both be in the 
	// same channels clip, so we pick the channels clip from the position unless position is not used 
	// and the rotation channels clip is valid, or if the position channels clip is not valid.
	CDmeChannelsClip *pChannelsClipPos = FindAncestorReferencingElement< CDmeChannelsClip >( pPosChannel );
	CDmeChannelsClip *pChannelsClipRot = FindAncestorReferencingElement< CDmeChannelsClip >( pRotChannel );
	CDmeChannelsClip *pChannelsClip = ( ( !bPosition && ( pChannelsClipRot != NULL ) ) || ( pChannelsClipPos == NULL ) ) ? pChannelsClipRot : pChannelsClipPos;
	
	// Ensure that the dag has channels for position and rotation
	CreateTransformChannelsForDag( pDag, pChannelsClip, bPosition, bRotation, pPosChannel, pRotChannel );
	
	// Find the position and rotation channels for the child dag and create a new layer in each channel
	CDmeLog *pPosLog = ( ( pPosChannel != NULL ) && bPosition ) ? pPosChannel->GetLog() : NULL;
	CDmeLog *pRotLog = ( ( pRotChannel != NULL ) && bRotation ) ? pRotChannel->GetLog() : NULL;
	CDmeTypedLogLayer< Vector > *pPosLayer = ( pPosLog != NULL ) ? CastElement< CDmeTypedLogLayer< Vector > >( pPosLog->AddNewLayer() ) : NULL;
	CDmeTypedLogLayer< Quaternion > *pRotLayer = ( pRotLog != NULL ) ? CastElement< CDmeTypedLogLayer< Quaternion > >( pRotLog->AddNewLayer() ) : NULL;
	Assert( pPosLayer || !bPosition );
	Assert( pRotLayer || !bRotation );

	// Build the clip stack to convert the global time into a local time
	DmeClipStack_t clipStack;
	if ( pChannelsClip )
	{
		pChannelsClip->BuildClipStack( &clipStack, pMovie, pShot );
	}
	else if ( pMovie && pShot )
	{
		clipStack.AddClipToTail( pMovie );
		clipStack.AddClipToTail( pShot );
	}
	
	
	// Disable the undo, the only thing that is actually 
	// needed is the add new layer and the flatten layers.
	CDisableUndoScopeGuard undosg;
	Vector position = vec3_origin;
	Quaternion rotation = quat_identity;
	DmeTime_t logTime = DMETIME_ZERO;

	int nTimes = keyTimes.Count();
	for ( int iTime = 0; iTime < nTimes; ++iTime )
	{
		// Calculate the time relative to the log
		logTime = clipStack.ToChildMediaTime( keyTimes[ iTime ], false );

		if ( pParent )
		{
			// Calculate the transform of the child relative to the parent
			// needed to keep the child in its original location in world space.
			const matrix3x4_t &parentMatrix = parentTransformList[ iTime ];
			const matrix3x4_t &worldChildMatrix = childTransformList[ iTime ];
			matrix3x4_t invParentMatrix;
			matrix3x4_t localChildMatrix;
			MatrixInvert( parentMatrix, invParentMatrix );
			ConcatTransforms( invParentMatrix, worldChildMatrix, localChildMatrix );
			MatrixAngles( localChildMatrix, rotation, position );
		}
		else
		{
			const matrix3x4_t &worldMatrix = childTransformList[ iTime ];
			MatrixAngles( worldMatrix, rotation, position );
		}

		// Set a key storing the new position and rotation of the child at the current time
		if ( pPosLayer )
		{
			pPosLayer->SetKey( logTime, position, SEGMENT_INTERPOLATE, CURVE_DEFAULT, false );
		}
		if ( pRotLayer )
		{
			pRotLayer->SetKey( logTime, rotation, SEGMENT_INTERPOLATE, CURVE_DEFAULT, false );
		}
	}


	// If a time selection is specified blend the top level log layers containing the new 
	// position and orientation with the base log according to the time selection.
	float flattenThreshold = DMELOG_DEFAULT_THRESHHOLD;

	if ( pTimeSelection )
	{
		// Convert the time selection into the local time space of the log.
		DmeLog_TimeSelection_t logTimeSelection = *pTimeSelection;
		clipStack.ToChildMediaTime( logTimeSelection.m_nTimes );
		flattenThreshold = pTimeSelection->m_flThreshold;

		if ( pPosLog )
		{
			pPosLog->BlendLayersUsingTimeSelection( logTimeSelection );
		}
		if ( pRotLog )
		{
			pRotLog->BlendLayersUsingTimeSelection( logTimeSelection );
		}

		// Now flatten the all the layers in the log to finalize the new layer
		{
			// Re-enable the undo so the the flatten can be undone
			CEnableUndoScopeGuard undosg;
			if ( pPosLog )
			{
				pPosLog->FlattenLayers( flattenThreshold, 0 );
			}
			if ( pRotLog )
			{
				pRotLog->FlattenLayers( flattenThreshold, 0 );
			}
		}
	}
	else
	{
		CEnableUndoScopeGuard undosg;

		// If not using a time selection then we only want data from the new log layer so just
		// copy it to the base layer without blending and then remove all other layers.
		if ( pPosLog )
		{			
			CDmeLogLayer *pPosBaseLayer = pPosLog->GetLayer( 0 );
			if ( pPosBaseLayer != pPosLayer )
			{
				pPosBaseLayer->CopyLayer( pPosLayer );
				while ( pPosLog->GetNumLayers() > 1 )
				{
					pPosLog->RemoveLayerFromTail();
					pPosLog->RemoveRedundantKeys( false );
				}
			}
		}

		if ( pRotLog )
		{
			CDmeLogLayer *pRotBaseLayer = pRotLog->GetLayer( 0 );
			if ( pRotBaseLayer != pRotLayer )
			{
				pRotBaseLayer->CopyLayer( pRotLayer );
				if ( pRotLog->GetNumLayers() > 1 )
				{
					pRotLog->RemoveLayerFromTail();
					pRotLog->RemoveRedundantKeys( false );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Purpose: Generate a list of all of the world space transform of the specified dag node at each 
// time in the provided list
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::GenerateDagWorldTransformList( CDmeDag *pDag, const CUtlVector< DmeTime_t > &times, CUtlVector< matrix3x4_t > &transformList, CDmeClip *pShot, CDmeClip *pMovie ) 
{
	// Make sure nothing here gets add to the undo queue.
	CDisableUndoScopeGuard undosg;

	// Find the channels and operators which must be updated in order to evaluate transform of the dag.
	CUtlVector< CDmeChannel* > channelList;
	CUtlVector< CDmeOperator* > operatorList;
	pDag->FindRelevantOperators( channelList, operatorList );
		
	// Build a clip stack for each channel in the list and save the current time of each of the channels.
	CUtlVector< DmeClipStack_t > channelClipStackList;
	CUtlVector< DmeTime_t > channelOriginalTimeList;
	BuildClipStackList( channelList, channelClipStackList, channelOriginalTimeList, pMovie, pShot );

	// Get the number of times to be sampled and resize the transform list to hold one matrix for each time
	int nTimes = times.Count();
	transformList.SetCount( nTimes );

	for ( int iTime = 0; iTime < nTimes; ++iTime )
	{
		DmeTime_t time = times[ iTime ];

		// Evaluate all of the channels relevant to the dag at the specified time
		// so that the transform evaluations will correspond to the correct time.
		PlayChannelsAtTime( time, channelList, operatorList, channelClipStackList );
		
		// Get the world space transform of the target
		matrix3x4_t &targetTransform = transformList[ iTime ];
		pDag->GetAbsTransform( targetTransform );
	}

	// Restore all of the channels to the original times so that this operation does not have side effects.
	PlayChannelsAtLocalTimes( channelOriginalTimeList, channelList, operatorList, false );
}


//-------------------------------------------------------------------------------------------------
// Update the position and orientation logs of the specified dag node so that the dag node's world 
// space transform matches the provided list
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::SetDagWorldSpaceTransforms( CDmeDag* pDag, CUtlVector< DmeTime_t > &times, const CUtlVector< matrix3x4_t > &transformList, CDmeClip *pShot, CDmeClip *pMovie )
{
	if ( pDag == NULL )
		return;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return;

	// Make sure nothing here gets add to the undo queue.
	CDisableUndoScopeGuard undosg;

	// Find the existing channels associated with the position and orientation of the dag transform
	CDmeChannel *pPosChannel = FindDagTransformChannel( pDag, TRANSFORM_POSITION );
	CDmeChannel *pRotChannel = FindDagTransformChannel( pDag, TRANSFORM_ORIENTATION );

	// Find the channels clip containing the position and rotation channel. They should both be in the 
	// same channels clip, so we pick the channels clip from the position unless position is not used 
	// and the rotation channels clip is valid, or if the position channels clip is not valid.
	CDmeChannelsClip *pChannelsClipPos = FindAncestorReferencingElement< CDmeChannelsClip >( pPosChannel );
	CDmeChannelsClip *pChannelsClipRot = FindAncestorReferencingElement< CDmeChannelsClip >( pRotChannel );
	CDmeChannelsClip *pChannelsClip = ( pChannelsClipPos != NULL ) ? pChannelsClipPos : pChannelsClipRot;
	
	// Build the clip stack to convert the global time into a local time
	DmeClipStack_t clipStack;
	if ( pChannelsClip )
	{
		pChannelsClip->BuildClipStack( &clipStack, pMovie, pShot );
	}
	else if ( pMovie && pShot )
	{
		clipStack.AddClipToTail( pMovie );
		clipStack.AddClipToTail( pShot );
	}

	// Find the channels and operators which must be updated in order to evaluate transform of the dag.
	CUtlVector< CDmeChannel* > channelList;
	CUtlVector< CDmeOperator* > operatorList;
	pDag->FindRelevantOperators( channelList, operatorList );
		
	// Build a clip stack for each channel in the list and save the current time of each of the channels.
	CUtlVector< DmeClipStack_t > channelClipStackList;
	CUtlVector< DmeTime_t > channelOriginalTimeList;
	BuildClipStackList( channelList, channelClipStackList, channelOriginalTimeList, pMovie, pShot );

	// Get the number of times to be sampled and resize the transform list to hold one matrix for each time
	int nTimes = times.Count();
	
	CUtlVector< DmeTime_t > localTimes;
	CUtlVector< Vector > localPositions;
	CUtlVector< Quaternion > localOrientations;
	localTimes.SetCount( nTimes );
	localPositions.SetCount( nTimes );
	localOrientations.SetCount( nTimes );

	for ( int iTime = 0; iTime < nTimes; ++iTime )
	{
		DmeTime_t time = times[ iTime ];

		// Convert the global time to the local time of the logs
		localTimes[ iTime ] = clipStack.ToChildMediaTime( time, false );

		// Evaluate all of the channels relevant to the dag at the specified time
		// so that the transform evaluations will correspond to the correct time.
		PlayChannelsAtTime( time, channelList, operatorList, channelClipStackList );
		
		// Get the world space transform of the target
		const matrix3x4_t &targetTransform = transformList[ iTime ];
		pDag->SetAbsTransform( targetTransform );
		localPositions[ iTime ] = pTransform->GetPosition();
		localOrientations[ iTime ] = pTransform->GetOrientation();
	}

	// Restore all of the channels to the original times so that this operation does not have side effects.
	PlayChannelsAtLocalTimes( channelOriginalTimeList, channelList, operatorList, false );

	// Write the positions to the log
	if ( pPosChannel )
	{
		CDmeLog *pPosLog = pPosChannel->GetLog();
		if ( pPosLog )
		{				
			CDmeTypedLogLayer< Vector > *pPosLayer = CastElement< CDmeTypedLogLayer< Vector > >( pPosLog->GetLayer( 0 ) );
			if ( pPosLayer )
			{
				CEnableUndoScopeGuard undosg;
				pPosLayer->SetAllKeys( localTimes, localPositions );
				pPosLog->RemoveRedundantKeys( false );
			}
		}
	}

	// Write the orientations to the log 
	if ( pRotChannel )
	{
		CDmeLog *pRotLog = pRotChannel->GetLog();
		if ( pRotLog )
		{
			CDmeTypedLogLayer< Quaternion > *pRotLayer = CastElement< CDmeTypedLogLayer< Quaternion > >( pRotLog->GetLayer( 0 ) );
			if ( pRotLayer )
			{
				CEnableUndoScopeGuard undosg;
				pRotLayer->SetAllKeys( localTimes, localOrientations );
				pRotLog->RemoveRedundantKeys( false );
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Create a list of all of the key times in the provided list of channels
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::CompileKeyTimeList( const CUtlVector< CDmeOperator* > &channelList, CUtlVector< DmeTime_t > &keyTimes, const DmeLog_TimeSelection_t *pTimeSelection, CDmeClip *pShot, CDmeClip *pMovie )
{
	// Make sure nothing here gets add to the undo queue.
	CDisableUndoScopeGuard undosg;


	// Iterate through all of the provided channels adding 
	// the time of all of the keys in each channel to list.
	CUtlRBTree< DmeTime_t > logTimes( 0, 0, DefLessFunc( DmeTime_t ) );
	int nChannels = channelList.Count();
	for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
	{
		CDmeChannel *pChannel = CastElement< CDmeChannel >( channelList[ iChannel ] );
		if ( pChannel == NULL )
			continue;

		CDmeLog *pLog = pChannel->GetLog();
		if ( pLog == NULL )
			continue;

		// Build the clip stack for the channel
		CDmeChannelsClip *pChannelsClip = FindAncestorReferencingElement< CDmeChannelsClip >( pChannel );
		if ( pChannelsClip == NULL )
			continue;

		DmeClipStack_t clipStack;
		pChannelsClip->BuildClipStack( &clipStack, pMovie, pShot );


		// Iterate through all of the keys in the log and
		// make sure there is an entry for the time of the key.
		int nKeys = pLog->GetKeyCount();

		// If the log only has one key, we can ignore the time 
		// since the value will be the same throughout time.
		if ( nKeys < 2 )
			continue;

		for ( int iKey = 0; iKey < nKeys; ++iKey )
		{
			// Get the time of the key and convert it to a global time
			DmeTime_t keyTime = pLog->GetKeyTime( iKey );
			if ( ( keyTime <= DMETIME_MINTIME ) || ( keyTime >= DMETIME_MAXTIME ) )
				continue;

			DmeTime_t time = clipStack.FromChildMediaTime( keyTime, false );

			// If a time selection was specified, add only times within it.
			if ( pTimeSelection )
			{
				if ( ( time < pTimeSelection->m_nTimes[ TS_LEFT_FALLOFF ] ) ||
					 ( time > pTimeSelection->m_nTimes[ TS_RIGHT_FALLOFF ] ) )
					continue;
			}

			// Add the key time if it is valid and has not already been added.
			if ( ( time <= DMETIME_MINTIME ) || ( time >=  DMETIME_MAXTIME ) )
				continue;

			logTimes.InsertIfNotFound( time );
		}
	}
	

	DmeTime_t firstTime = DMETIME_ZERO;
	DmeTime_t lastTime = DMETIME_ZERO;

	if ( pTimeSelection )
	{
		firstTime = pTimeSelection->m_nTimes[ TS_LEFT_FALLOFF ];
		lastTime = pTimeSelection->m_nTimes[ TS_RIGHT_FALLOFF ];
	}
	else if ( logTimes.Count() > 0 )
	{
		firstTime = logTimes[ logTimes.FirstInorder() ];
		lastTime = logTimes[ logTimes.LastInorder() ];
	}


	// Use a re-sample time of 50ms unless otherwise specified by the time selection
	DmeTime_t resampleInterval = DmeTime_t( 500 );
	if ( pTimeSelection )
	{
		resampleInterval = MAX( pTimeSelection->m_nResampleInterval, DMETIME_MINDELTA );
	}


	// Calculate the starting time for the re-sampling. In order to try to keep the sampling 
	// consistent, make sure the starting time is on a multiple of the re-sample time.
	int tmsResampleInterval = resampleInterval.GetTenthsOfMS();
	int tmsFirstTime = firstTime.GetTenthsOfMS();
	
	DmeTime_t sampleStartTime;
	if ( tmsFirstTime < 0 )
	{
		int resampleStartBase = ( ( tmsFirstTime + 1 ) / tmsResampleInterval );
		sampleStartTime = DmeTime_t( tmsResampleInterval * resampleStartBase );
	}
	else
	{
		int resampleStartBase = ( tmsFirstTime / tmsResampleInterval );
		sampleStartTime = DmeTime_t( tmsResampleInterval * ( resampleStartBase + 1 ) );
	}
	Assert( sampleStartTime > firstTime );
	Assert( sampleStartTime <= ( firstTime + resampleInterval ) );


	// Calculate the approximate number of samples so that the key times array can be preallocated.
	int nTimes = logTimes.Count() + ( lastTime - firstTime ).GetTenthsOfMS() / tmsResampleInterval;
	keyTimes.RemoveAll();
	keyTimes.EnsureCapacity( nTimes + 8 );

	// Set the log time to either the first entry in the set of log times, 
	// or to the last time if there are no entries in the set of log times.
	DmeTime_t logTime = lastTime;
	int iLogTime = logTimes.FirstInorder();
	if ( iLogTime != logTimes.InvalidIndex() )
	{	
		logTime = logTimes[ iLogTime ];
	}

	// If the first log time is not the first time, with the first time, as there
	// is nothing else that will since the sample times start after the first time.
	if ( logTime != firstTime )
	{
		keyTimes.AddToTail( firstTime );
	}

	// Iterate through the sample times and log times, adding sample times until the next log
	// time is reached and then adding log times until the next sample time is reached.
	DmeTime_t sampleTime = sampleStartTime;
	do
	{
		// Add sample time until the next log time is reached
		while ( sampleTime < logTime )
		{
			keyTimes.AddToTail( sampleTime );
			sampleTime += resampleInterval;
		}

		// If the next sample time is equal to the next log time, move the 
		// sample time up one more step since the log time will be added to 
		// the list. This guarantees sampleTime is greater than logTime.
		if ( sampleTime == logTime )
		{
			sampleTime += resampleInterval;
		}

		// Add log times until the next sample time is reached or the last log time is 
		// hit. If the last log time is added, make the next log time the last time, 
		// this ensures the last time will be added even if it is not a sample time.
		while ( logTime < sampleTime )
		{		
			Assert( keyTimes.Find( logTime ) == keyTimes.InvalidIndex() );
			keyTimes.AddToTail( logTime );
		
			if ( iLogTime == logTimes.InvalidIndex() )
				break;
			
			iLogTime = logTimes.NextInorder( iLogTime );
			if ( iLogTime == logTimes.InvalidIndex() )
			{
				if ( lastTime == logTime )
					break;
				logTime = lastTime;
			}
			else
			{
				logTime = logTimes[ iLogTime ];
			}
		}
	}
	while ( sampleTime <= lastTime );

	Assert( logTime == lastTime );
	Assert( keyTimes.Count() > 0 );
	Assert( keyTimes[ keyTimes.Count() - 1 ] == lastTime );

	for ( int i = 1; i < keyTimes.Count(); ++i )
	{
		Assert( keyTimes[ i - 1 ] != keyTimes[ i ] );
		Assert( keyTimes[ i - 1 ] < keyTimes [ i ] );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find the position and orientation controls for the specified transform.
//-------------------------------------------------------------------------------------------------
CDmeTransformControl *CDmAnimUtils::GetTransformControl( const CDmeDag *pDag )
{
	if ( pDag == NULL )
		return NULL;

	CUtlVector< CDmeChannel* > channelList( 0, 8 );

	if ( pDag->GetTransform() )
	{
		FindAncestorsReferencingElement( pDag->GetTransform(), channelList );
	}

	CUtlVector< CDmeConstraintSlave* > slaveList( 0, 4 );
	FindAncestorsReferencingElement( pDag, slaveList );
	
	int nSlaves = slaveList.Count();
	for ( int iSlave = 0; iSlave < nSlaves; ++iSlave )
	{
		CDmeConstraintSlave *pSlave = slaveList[ iSlave ];
		if ( pSlave )
		{
			FindAncestorsReferencingElement( pSlave, channelList );
		}
	}

	int nChannels = channelList.Count();
	for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
	{
		CDmeChannel *pChannel = channelList[ iChannel ];
		if ( pChannel == NULL )
			continue;

		CDmElement *pControl = pChannel->GetFromElement();
		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pControl );
		if ( pTransformControl )
		{
			return pTransformControl;
		}
	}

	return NULL;
}

//-------------------------------------------------------------------------------------------------
// Purpose: Get the local space default transform for the specified dag node
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::GetDefaultTransform( CDmeDag *pDagNode, matrix3x4_t &defaultTransform )
{
	CDmeTransformControl *pTransformControl = GetTransformControl( pDagNode );
	
	Vector defaultPosition = vec3_origin;
	Quaternion defaultOrientation = quat_identity;

	if ( pTransformControl )
	{
		defaultPosition = pTransformControl->GetDefaultPosition();
		defaultOrientation = pTransformControl->GetDefaultOrientation();
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the world space default transform for the specified dag node
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::GetDefaultAbsTransform( CDmeDag *pDagNode, matrix3x4_t &absDefaultTransform )
{
	matrix3x4_t parentToWorld;
	matrix3x4_t localMatrix;

	if ( pDagNode == NULL )
	{
		SetIdentityMatrix( absDefaultTransform );
		return;
	}

	GetDefaultTransform( pDagNode, localMatrix );
	GetDefaultAbsTransform( pDagNode->GetParent(), parentToWorld );

	ConcatTransforms( parentToWorld, localMatrix, absDefaultTransform );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find and operate all of the channels driving the specified dag nodes
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::OperateDagChannels( const CUtlVector< CDmeDag* > &dagList, ChannelMode_t mode, const DmeLog_TimeSelection_t &timeSelection, CDmeClip *pShot, CDmeClip *pMovie )
{
	// Get all of the channels driving the selected dag nodes
	CUtlVector< CDmeChannel* > channelList;

	GetChannelsForDags( dagList, channelList );

	// Operate all of the channels in the currently selected mode in order 
	// to apply the values from the controls to the dag node transforms.
	if ( mode == CM_RECORD )
	{
		RecordChannels( channelList, timeSelection, pShot, pMovie );
	}
	else
	{
		OperateChannels( channelList, timeSelection, pShot, pMovie );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get all of the channels directly driving the specified dag nodes
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::GetChannelsForDags( const CUtlVector< CDmeDag* > &dagList, CUtlVector< CDmeChannel* > &channelList )
{
	int nDagNodes = dagList.Count();
	channelList.EnsureCapacity( channelList.Count() + ( nDagNodes * 2 )	);

	for ( int iNode = 0; iNode < nDagNodes; ++iNode )
	{
		const CDmeDag *pDagNode = dagList[ iNode ];
		if ( pDagNode )
		{			
			FindAncestorsReferencingElement( pDagNode->GetTransform(), channelList );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Operate all of the provided channels in the specified mode.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::OperateChannels( const CUtlVector< CDmeChannel* > &channelList, const DmeLog_TimeSelection_t &timeSelection, CDmeClip *pShot, CDmeClip *pMovie )
{
	int nChannels = channelList.Count();
	for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
	{
		CDmeChannel *pChannel = channelList[ iChannel ];
		if ( pChannel )
		{		
			// Newly created channels may not have the current time, so 
			// make sure the time on the channel is correct before operating.
			DmeClipStack_t clipStack;
			pChannel->BuildClipStack( &clipStack, pMovie, pShot );
			DmeTime_t localtime = clipStack.ToChildMediaTime( timeSelection.m_tHeadPosition );
			pChannel->SetCurrentTime( localtime );

			pChannel->Operate();
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Operate all of the provided channels in the specified mode.
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::RecordChannels( const CUtlVector< CDmeChannel* > &channelList, const DmeLog_TimeSelection_t &timeSelection, CDmeClip *pShot, CDmeClip *pMovie )
{
	int nChannels = channelList.Count();
	if ( nChannels == 0  )
		return;

	g_pChannelRecordingMgr->StartModificationLayer( &timeSelection, false );
	g_pChannelRecordingMgr->StartLayerRecording( "Script channel record" );

	for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
	{
		CDmeChannel *pChannel = channelList[ iChannel ];
		if ( pChannel )
		{
			// Newly created channels may not have the current time, so 
			// make sure the time on the channel is correct before operating.
			DmeClipStack_t clipStack;
			pChannel->BuildClipStack( &clipStack, pMovie, pShot );
			DmeTime_t localtime = clipStack.ToChildMediaTime( timeSelection.m_tHeadPosition );
			pChannel->SetCurrentTime( localtime );
			
			TransformWriteMode_t transformWriteMode = g_pChannelRecordingMgr->GetTransformWriteMode();
			g_pChannelRecordingMgr->SetTransformWriteMode( timeSelection.m_TransformWriteMode );
			g_pChannelRecordingMgr->AddChannelToRecordingLayer( pChannel, LOG_COMPONENTS_ALL, pMovie, pShot );
			g_pChannelRecordingMgr->SetTransformWriteMode( transformWriteMode );
			pChannel->Operate();
		}
	}

	g_pChannelRecordingMgr->FinishLayerRecording( timeSelection.m_flThreshold );
	g_pChannelRecordingMgr->FinishModificationLayer();
}


//-------------------------------------------------------------------------------------------------
// Find the shot and the movie to which the specified dag node belongs
//-------------------------------------------------------------------------------------------------
void CDmAnimUtils::FindShotAndMovieForDag( const CDmeDag *pDag, CDmeClip *&pShot, CDmeClip *&pMovie )
{
	pShot = NULL;
	pMovie = NULL;

	if ( pDag == NULL )
		return;

	// First find the root of the dag hierarchy, this should be the scene.
	const CDmeDag *pCurrentDag = pDag;
	const CDmeDag *pParent = pCurrentDag->GetParent();

	while ( pParent )
	{
		pCurrentDag = pParent;
		pParent = pCurrentDag->GetParent();
	}

	// Now find the film clips referencing the scene, take the first one that 
	// the scene attribute is the scene dag containing the dag node in question.
	const CDmeDag *pRoot = pCurrentDag;
	
	CUtlVector< CDmeFilmClip* > filmClipList( 0, 4 );
	FindAncestorsReferencingElement( pRoot, filmClipList );

	int nNumClips = filmClipList.Count();
	for ( int iClip = 0; iClip < nNumClips; ++iClip )
	{
		CDmeFilmClip *pClip = filmClipList[ iClip ];
		if ( pClip == NULL )
			continue;

		if ( pClip->GetScene() == pRoot )
		{
			pShot = pClip;
			break;
		}

		// The active camera does not have to be in the scene
		if ( pRoot == ( const CDmeDag* )( pClip->GetCamera() ) )
		{	
			pShot = pClip;
			break;
		}
	}

	// If the shot was not found don't bother with the movie.
	if ( pShot == NULL )
		return;

	// Find all of the tracks that reference the shot. There may be multiple if the shot has been imported into 
	// the active session from a session in the misc bin. We want the track that belongs to the active session.
	CUtlVector< CDmeTrack* > trackList;
	FindAncestorsReferencingElement( pShot, trackList );

	int nNumTracks = trackList.Count();
	for ( int iTrack = 0; iTrack < nNumTracks; ++iTrack )
	{
		CDmeTrack *pTrack = trackList[ iTrack ];
		if ( pTrack == NULL )
			continue;

		// The session should be root file element, so we can
		// find the session using the file id of the shot
		CDmElement *pSession = g_pDataModel->GetElement( g_pDataModel->GetFileRoot( pTrack->GetFileId() ) );
		if ( pSession )
		{
			CDmeFilmClip *pClip = pSession->GetValueElement< CDmeFilmClip >( "activeClip" );
			if ( pClip )
			{			
				if ( pClip->FindTrackForClip( pShot ) == pTrack )
				{
					pMovie = pClip;
					break;
				}
			}
		}
	}
}




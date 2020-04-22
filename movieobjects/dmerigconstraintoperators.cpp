//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: Implementation of the constraint operator classes. The constraint 
// operators control the position and / or orientation of a specified slave dag
// node based on the position and orientation of a set of weighted target dag 
// nodes. The relationship between the target dag node and the slave is
// determined by the type of the constraint. The Operate() function of each 
// constraint class is responsible for enforcing its specific relationship.
//
//=============================================================================
#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmerig.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier1/fmtstr.h"
#include "bone_constraints.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-------------------------------------------------------------------------------------------------
//
// CDmeConstraintTarget
//
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Expose the CDmeConstraintTarget class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeConstraintTarget, CDmeConstraintTarget );


//-----------------------------------------------------------------------------
// Purpose: Perform construction tasks, initializes attributes
//-----------------------------------------------------------------------------
void CDmeConstraintTarget::OnConstruction()
{
	m_Handle.Init( this, "target" );
	m_flWeight.Init( this, "targetWeight" );
	m_vecOffset.InitAndSet( this, "vecOffset", vec3_origin );
	m_qOffset.InitAndSet( this, "oOffset", quat_identity );
}


//-----------------------------------------------------------------------------
// Purpose: Perform destruction tasks
//-----------------------------------------------------------------------------
void CDmeConstraintTarget::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Purpose: Reset the position and orientation offset values
//-----------------------------------------------------------------------------
void CDmeConstraintTarget::ClearOffset()
{
	m_vecOffset = vec3_origin;
	m_qOffset = quat_identity;
}


//-----------------------------------------------------------------------------
// Purpose: Find the channel driving the weight value of the constraint
//-----------------------------------------------------------------------------
CDmeChannel *CDmeConstraintTarget::FindWeightChannel() const
{
	for ( DmAttributeReferenceIterator_t it = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( it );
		CDmElement *pElement = pAttr->GetOwner();
		if ( pElement == NULL )
			continue;

		CDmeChannel *pChannel = CastElement< CDmeChannel >( pElement );
		if ( pChannel )
		{		
			if ( pChannel->GetToAttribute() == m_flWeight.GetAttribute() )
				return pChannel;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Get the constraint that target belongs to.
//-----------------------------------------------------------------------------
CDmeRigBaseConstraintOperator *CDmeConstraintTarget::GetConstraint()
{
	return FindAncestorReferencingElement< CDmeRigBaseConstraintOperator >( this );
}


//-----------------------------------------------------------------------------
// Get the control attached to constraint
//-----------------------------------------------------------------------------
CDmElement *CDmeConstraintTarget::FindWeightControl() const
{
	CDmElement *pWeightChannel = FindWeightChannel();
	CDmElement *pControl = FindReferringElement< CDmElement >( pWeightChannel, "channel" );
	return pControl;
}



//-------------------------------------------------------------------------------------------------
//
// CDmeConstraintSlave
//
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Expose the CDmeConstraintSlave class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeConstraintSlave, CDmeConstraintSlave );


//-----------------------------------------------------------------------------
// Purpose: Perform construction tasks, initializes attributes
//-----------------------------------------------------------------------------
void CDmeConstraintSlave::OnConstruction()
{
	m_Dag.Init( this, "target" );
	m_BasePosition.Init( this, TRANSFORM_POSITION );
	m_BaseOrientation.Init( this, TRANSFORM_ORIENTATION );
}


//-----------------------------------------------------------------------------
// Purpose: Perform destruction tasks
//-----------------------------------------------------------------------------
void CDmeConstraintSlave::OnDestruction()
{

}


//-----------------------------------------------------------------------------
// Get the constraint to which the slave belongs.
//-----------------------------------------------------------------------------
CDmeRigBaseConstraintOperator *CDmeConstraintSlave::GetConstraint() const
{
	CDmeRigBaseConstraintOperator *pConstraint = FindAncestorReferencingElement< CDmeRigBaseConstraintOperator >( this );
	return pConstraint;
}


//-----------------------------------------------------------------------------
// Purpose:  Compute the base world space matrix 
//-----------------------------------------------------------------------------
void CDmeConstraintSlave::GetBaseWorldTransform( matrix3x4_t &worldMatrix ) const
{
	Vector lsBasePosition = m_BasePosition;
	Quaternion lsBaseOrientation = m_BaseOrientation;
	CDmeDag *pDag = m_Dag;
	if ( pDag )
	{
		matrix3x4_t parentToWorld, localMatrix;
		pDag->GetParentWorldMatrix( parentToWorld );
		QuaternionMatrix( lsBaseOrientation, lsBasePosition, localMatrix );
		ConcatTransforms( parentToWorld, localMatrix, worldMatrix );
	}
	else
	{
		SetIdentityMatrix( worldMatrix );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Compute the base position and orientation in world space
//-----------------------------------------------------------------------------
void CDmeConstraintSlave::ComputeBaseWorldValues( Vector &wsPosition, Quaternion &wsOrientation ) const
{
	matrix3x4_t worldMatrix;
	GetBaseWorldTransform( worldMatrix );
	MatrixAngles( worldMatrix, wsOrientation, wsPosition );
}



//-------------------------------------------------------------------------------------------------
//
// CDmeRigBaseConstraintOperator
//
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigBaseConstraintOperator, CDmeRigBaseConstraintOperator );

//-----------------------------------------------------------------------------
// Purpose: Constructor, initializes attributes, create the embedded target
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::OnConstruction()
{
	m_Targets.Init( this, "targets" );
	m_Slave.InitAndCreate( this, "slave" );
	m_mode.InitAndSet( this, "mode", (int)RM_FORWARD );
}


//-----------------------------------------------------------------------------
// Purpose: Perform destruction tasks, destroy the internal elements of the 
// constraint.
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::OnDestruction()
{
	g_pDataModel->DestroyElement( m_Slave.GetHandle() );

	for ( int i = 0 ;i < m_Targets.Count(); ++i )
	{
		if ( m_Targets[ i ] )
		{
			g_pDataModel->DestroyElement( m_Targets[ i ]->GetHandle() );
		}
	}

	m_Targets.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose:  Determine if the the constraint has slave with the specified name
//-----------------------------------------------------------------------------
bool CDmeRigBaseConstraintOperator::IsSlaveObject( char const *pchName ) const
{
	if ( !m_Slave->m_Dag )
		return false;

	return !Q_stricmp( m_Slave->m_Dag->GetName(), pchName );
}


//-----------------------------------------------------------------------------
// Purpose: Get the attributes that the constraint reads data from, Inputs are 
// CDmeDags (handles usually)
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( int i = 0; i < m_Targets.Count(); ++i )
	{
		if ( !m_Targets[ i ])
			continue;

		CDmeDag* pDagNode = m_Targets[ i ]->m_Handle;
		if ( pDagNode == NULL )
			continue;

		AddAttribute( attrs, GetInputAttributeType(), pDagNode->GetTransform() );
		AddAncestorAttributes( attrs, pDagNode );

		CDmAttribute *pWeightAttr = m_Targets[ i ]->GetWeightAttribute();
		if ( pWeightAttr )
		{
			attrs.AddToTail( pWeightAttr );
		}
	}

	// If the slave dag node is the child of another node the operation of the constraint
	// is dependent upon the evaluation of the parent because result of the constraint 
	// operation must be converted into the space of the parent of the slave.
	AddAncestorAttributes( attrs, m_Slave->m_Dag );

	CDmAttribute *pSlavePosAttr = m_Slave->m_BasePosition.GetAttribute();
	if ( pSlavePosAttr )
	{
		attrs.AddToTail( pSlavePosAttr );
	}

	CDmAttribute *pSlaveRotAttr = m_Slave->m_BaseOrientation.GetAttribute();
	if ( pSlaveRotAttr )
	{
		attrs.AddToTail( pSlaveRotAttr );
	}

}


//-----------------------------------------------------------------------------
// Purpose: Get the attributes to which the attribute writes, Outputs are
// CDmeDags (bones or other CDmeRigHandles usually)
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	if ( !m_Slave->m_Dag )
		return;

	AddAttribute( attrs, GetOutputAttributeType(), m_Slave->m_Dag->GetTransform() );
}


//-----------------------------------------------------------------------------
// Purpose: Add the specified type of attributes from the provided transform
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::AddAttribute( CUtlVector< CDmAttribute * > &attrs, enum EAddType type, CDmeTransform *pTxForm )
{
	if ( !pTxForm )
		return;

	// Get the transform attribute
	if ( type & AA_TYPE_POSITION )
	{
		CDmAttribute *pAttrib = pTxForm->GetPositionAttribute();
		if ( pAttrib )
			attrs.AddToTail( pAttrib );
	}
	if ( type & AA_TYPE_ORIENTATION )
	{
		CDmAttribute *pAttrib = pTxForm->GetOrientationAttribute();
		if ( pAttrib )
			attrs.AddToTail( pAttrib );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add the position and orientation attributes the entire ancestry of 
// the dag node.
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::AddAncestorAttributes(  CUtlVector< CDmAttribute * > &attrs, CDmeDag *pDagNode )
{
	if ( pDagNode == NULL )
		return;

	CDmeDag* pParent = pDagNode->GetParent();
	while ( pParent )
	{
		AddAttribute( attrs, (EAddType)( AA_TYPE_POSITION | AA_TYPE_ORIENTATION ), pParent->GetTransform() );
		pParent = pParent->GetParent();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add target handles to the the constraint with the specified 
// weights. Returns the index of the first target added.
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::AddHandles( int nCount, CDmeDag *const pTargetDags[], const float *pflWeights, bool bPreserveOffset, CUtlVector< CDmeConstraintTarget* > *pTargetList )
{
	// If a target list is provided, allocate the space to hold all
	// of the constraint targets for the list of input dag nodes.
	if ( pTargetList )
	{
		pTargetList->EnsureCapacity( pTargetList->Count() + nCount );
	}

	for ( int i = 0; i < nCount; ++i )
	{
		Assert( pTargetDags[ i ] );
		if ( !pTargetDags[ i ] )
			continue;
		
		// Check to see if the constraint already has a target with the specified dag node
		CDmeConstraintTarget *pTarget = FindConstraintTargetForDag( pTargetDags[ i ] );

		if ( pTarget == NULL )
		{
			pTarget = CreateElement< CDmeConstraintTarget >( CFmtStr( "%s", pTargetDags[ i ]->GetName() ), GetFileId() );
			m_Targets.AddToTail( pTarget );		
		}

		if ( pTarget != NULL )
		{
			pTarget->m_Handle = pTargetDags[ i ];
			pTarget->m_flWeight = ( pflWeights != NULL ) ? pflWeights[ i ] : 1.0f;

			Vector vOffset = vec3_origin;
			Quaternion qOffset = quat_identity;
			ComputeOffset( vOffset, qOffset, pTarget, bPreserveOffset );
			pTarget->m_vecOffset = vOffset;
			pTarget->m_qOffset = qOffset;

			if ( pTargetList )
			{
				pTargetList->AddToTail( pTarget );
			}
		}
	}

	PostHandlesAdded( bPreserveOffset );
}


//-----------------------------------------------------------------------------
// Purpose: Remove all target handles from the constraint
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::ClearHandles()
{
	m_Targets.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Set the dag node which the constraint is controlling
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::SetSlave( CDmeDag *pSlave )
{
	m_Slave->m_Dag = pSlave;

	CDmeTransform *pTransform = pSlave->GetTransform();
	if ( pTransform )
	{
		m_Slave->SetBasePosition( pTransform->GetPosition() );
		m_Slave->SetBaseOrientation( pTransform->GetOrientation() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the dag node the constraint is controlling
//-----------------------------------------------------------------------------
const CDmeDag *CDmeRigBaseConstraintOperator::GetSlave() const
{
	return m_Slave->m_Dag;
}


//-----------------------------------------------------------------------------
// Purpose: Set the operating mode of the constraint
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::SetMode( RigOperatorMode_t mode )
{
	m_mode = mode;
}


//-----------------------------------------------------------------------------
// Purpose: Compute the position and orientation offset of the specified target
// based on the relative transforms of the target and the slave.
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::ComputeOffset( Vector &vOffset, Quaternion &qOffset, CDmeConstraintTarget *pTarget, bool bPreserveOffset )
{
	vOffset = vec3_origin;
	qOffset = quat_identity;

	Assert( m_Slave->m_Dag );
	if ( !bPreserveOffset || !m_Slave->m_Dag || !pTarget->m_Handle )		
		return;

	matrix3x4_t mSlave, mTarget;
	m_Slave->m_Dag->GetAbsTransform( mSlave );
	pTarget->m_Handle->GetAbsTransform( mTarget );
	
	Vector slavePosition, targetPosition;
	Quaternion slaveOrientation, targetOrientation;
	MatrixAngles( mSlave, slaveOrientation, slavePosition );
	MatrixAngles( mTarget, targetOrientation, targetPosition );
	vOffset	= slavePosition - targetPosition;

	Quaternion invTargetOrientation;
	QuaternionInvert( targetOrientation, invTargetOrientation );
	QuaternionMult( invTargetOrientation, slaveOrientation, qOffset );
}


//-----------------------------------------------------------------------------
// Compute the aggregate target position from the weighted target list and 
// return the total weight
//-----------------------------------------------------------------------------
float CDmeRigBaseConstraintOperator::ComputeTargetPosition( Vector &wsTargetPosition )
{
	wsTargetPosition = vec3_origin;

	float flWeightSum = 0.0f;
	int nTargets = m_Targets.Count();
	for ( int i = 0; i < nTargets; ++i )
	{
		CDmeConstraintTarget *pTarget = m_Targets[ i ];
		Assert( pTarget );

		CDmeDag *pTargetDag = pTarget->m_Handle;
		if ( pTargetDag )
		{	
			Vector targetPos;
			pTargetDag->GetAbsPosition( targetPos );

			wsTargetPosition += ( pTarget->m_flWeight * ( targetPos + pTarget->m_vecOffset ) );
			flWeightSum += pTarget->m_flWeight;
		}
	}

	if ( flWeightSum > 0.0f )
	{
		wsTargetPosition *= 1.0f / flWeightSum;
	}

	return MIN( 1.0f, flWeightSum );
}


static const int MAX_RIG_TARGETS = 4;

//-----------------------------------------------------------------------------
// Compute the aggregate target orientation from the weighted target list and 
// return the total weight
//-----------------------------------------------------------------------------
float CDmeRigBaseConstraintOperator::ComputeTargetOrientation( Quaternion &wsTargetOrientation )
{
	int nTargets = m_Targets.Count();


	// If there is only one target, for efficiency don't bother with the weighting
	if ( nTargets == 1 )
	{
		CDmeConstraintTarget *pTarget = m_Targets[ 0 ];
		CDmeDag *pTargetDag = pTarget->m_Handle;
		if ( !pTargetDag )
			return 0;

		Quaternion targetOrientation;
		Quaternion offsetOrientation;
		pTargetDag->GetAbsOrientation( targetOrientation );
		QuaternionMult( targetOrientation, pTarget->m_qOffset, wsTargetOrientation );

		return MIN( 1.0f, pTarget->m_flWeight.Get() );
	}

	int nAllocCount = MAX( nTargets, 1 );

	Quaternion *pQuats = ( Quaternion* )stackalloc( nAllocCount * sizeof( Quaternion ) );
	float *flQuatWeights = ( float* )stackalloc( nAllocCount * sizeof( float ) );

	wsTargetOrientation = quat_identity;

	float flWeightSum = 0.0f;
	int nQuatCount = 0;

	for ( int i = 0; i < nTargets; ++i )
	{
		CDmeConstraintTarget *pTarget = m_Targets[ i ];
		Assert( pTarget );

		CDmeDag *pTargetDag = pTarget->m_Handle;
		if ( !pTargetDag )
			continue;

		Quaternion targetOrientation;
		Quaternion offsetOrientation;
		pTargetDag->GetAbsOrientation( targetOrientation );
		QuaternionMult( targetOrientation, pTarget->m_qOffset, offsetOrientation );

		pQuats[ nQuatCount ] = offsetOrientation;
		flQuatWeights[ nQuatCount ] =  pTarget->m_flWeight;		
		flWeightSum += pTarget->m_flWeight;
		++nQuatCount;
	}

	QuaternionAverageExponential( wsTargetOrientation, nQuatCount, pQuats, flQuatWeights );
	return MIN( 1.0f, flWeightSum );
}


//-----------------------------------------------------------------------------
// Purpose: Get a list of all of the operators that the operation of the 
// constraint depends on.
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::GatherInputOperators( CUtlVector< CDmeOperator * > &operatorList )
{
	// Iterate through the targets and add the operator 
	// dependencies for the associated dag nodes.
	int nTargets = m_Targets.Count();
	for ( int i = 0; i < nTargets; ++i )
	{
		CDmeConstraintTarget *pTarget = m_Targets[ i ];
		Assert( pTarget );

		CDmeDag *pHandle = pTarget->m_Handle;
		if ( !pHandle )
			continue;

		pHandle->FindRelevantOperators( operatorList );
	}

	BaseClass::GatherInputOperators( operatorList );
}


//-----------------------------------------------------------------------------
// Purpose: Disconnect the channels driving the slave dag nodes from the dag 
// transforms and connect them to the constraint
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::DisconnectTransformChannels()
{
	DisconnectSlaveChannels( m_Slave.GetElement(), GetOutputAttributeType() );
}


//-----------------------------------------------------------------------------
// Purpose: Reconnect the base channels of each slave directly to the dag
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::ReconnectTransformChannels()
{
	ReconnectSlaveChannels( m_Slave.GetElement(), GetOutputAttributeType()  );
}


//-----------------------------------------------------------------------------
// Purpose: Disconnect the channels driving the specified slave from the dag 
// and connect them to the constraint
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::DisconnectSlaveChannels( CDmeConstraintSlave *pSlave, int attributeTypeFlags )
{
	// Verify that the specified slave is valid and has a dag node connected.
	if ( pSlave == NULL )
		return;
	
	if ( pSlave->m_Dag.GetElement() == NULL )
		return;

	CDmeTransform *pTransform = pSlave->m_Dag->GetTransform();
	if ( pTransform == NULL )
		return;

	// Make the base position and orientation match the 
	// current position and orientation of the transform.
	pSlave->SetBasePosition( pTransform->GetPosition() );
	pSlave->SetBaseOrientation( pTransform->GetOrientation() );
	
	// Find the channel targeting the position attribute of the transform and
	// re-connect it to the position attribute of the slave within the constraint.
	if ( attributeTypeFlags & AA_TYPE_POSITION )
	{
		CDmeChannel *pPosChannel = FindChannelTargetingElement( pTransform, TRANSFORM_POSITION );
		if ( pPosChannel )
		{
			pPosChannel->SetOutput( pSlave, TRANSFORM_POSITION );
		}
	}

	// Find the channel targeting the orientation attribute of the transform and
	// re-connect it to the orientation attribute of the slave within the constraint.
	if ( attributeTypeFlags & AA_TYPE_ORIENTATION )
	{
		CDmeChannel *pRotChannel = FindChannelTargetingElement( pTransform, TRANSFORM_ORIENTATION );
		if ( pRotChannel )
		{
			pRotChannel->SetOutput( pSlave, TRANSFORM_ORIENTATION );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Reconnect the transform channels associated with the specified 
// slave directly to the dag
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::ReconnectSlaveChannels( CDmeConstraintSlave *pSlave, int attributeTypeFlags )
{

	// Verify that the specified slave is valid and has a dag node connected.
	if ( pSlave == NULL )
		return;

	if ( pSlave->m_Dag.GetElement() == NULL )
		return;

	CDmeTransform *pTransform = pSlave->m_Dag->GetTransform();
	if ( pTransform == NULL )
		return;

	// Find the channel targeting the position attribute of the slave and
	// re-connect it to the position attribute of the transform.
	if ( attributeTypeFlags & AA_TYPE_POSITION )
	{
		CDmeChannel *pPosChannel = FindChannelTargetingElement( pSlave, TRANSFORM_POSITION );
		if ( pPosChannel )
		{
			pPosChannel->SetOutput( pTransform, TRANSFORM_POSITION );

			// If the channel is in pass through mode, update the control value with the 
			// transform value so that the result of the constraint will be maintained.
			if ( pPosChannel->GetMode() == CM_PASS )
			{				
				CDmElement *pControl = pPosChannel->GetFromElement();
				CDmeTransformControl *pTranformControl = CastElement< CDmeTransformControl >( pControl );
				if ( pTranformControl )
				{
					pTranformControl->SetPosition( pTransform->GetPosition() );
				}
			}
		}
	}

	// Find the channel targeting the orientation attribute of the slave and
	// re-connect it to the orientation attribute of the transform.
	if ( attributeTypeFlags & AA_TYPE_ORIENTATION )
	{
		CDmeChannel *pRotChannel = FindChannelTargetingElement( pSlave, TRANSFORM_ORIENTATION );
		if ( pRotChannel )
		{
			pRotChannel->SetOutput( pTransform, TRANSFORM_ORIENTATION );

			// If the channel is in pass through mode, update the control value with the 
			// transform value so that the result of the constraint will be maintained.
			if ( pRotChannel->GetMode() == CM_PASS )
			{			
				CDmElement *pControl = pRotChannel->GetFromElement();
				CDmeTransformControl *pTranformControl = CastElement< CDmeTransformControl >( pControl );
				if ( pTranformControl )
				{
					pTranformControl->SetOrientation( pTransform->GetOrientation() );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find the constraint target for the specified dag node
//-----------------------------------------------------------------------------
CDmeConstraintTarget *CDmeRigBaseConstraintOperator::FindConstraintTargetForDag( CDmeDag* pDagNode ) const
{
	if ( pDagNode == NULL )
		return NULL;

	int nTargets = m_Targets.Count();
	for ( int iTarget = 0; iTarget < nTargets; ++iTarget )
	{
		CDmeConstraintTarget *pTarget = m_Targets[ iTarget ];
		if ( pTarget )
		{
			if ( pTarget->GetDag() == pDagNode )
			{
				return pTarget;
			}
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find all of the constraints that target the specified dag node
//-----------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::FindDagConstraints( const CDmeDag *pDagNode, CUtlVector< CDmeRigBaseConstraintOperator* > &constraintList )
{
	if ( pDagNode == NULL )
		return;

	// Find the slave instances targeting the specified node
	CUtlVector< CDmeConstraintSlave* > slaveList;
	FindAncestorsReferencingElement( pDagNode, slaveList );

	// Find the constraints for each slave instance
	for ( int i = 0; i < slaveList.Count(); ++i ) 
	{
		FindAncestorsReferencingElement( slaveList[ i ], constraintList );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find the constraint on the dag of the specified type
//-----------------------------------------------------------------------------
CDmeRigBaseConstraintOperator *CDmeRigBaseConstraintOperator::FindDagConstraint( CDmeDag *pDag, EConstraintType constraintType )
{
	CUtlVector< CDmeRigBaseConstraintOperator* > constraintList;
	FindDagConstraints( pDag, constraintList );

	int nConstraints = constraintList.Count();
	for ( int iConstraint = 0; iConstraint < nConstraints; ++iConstraint )
	{
		CDmeRigBaseConstraintOperator *pConstraint = constraintList[ iConstraint ];
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
// Purpose: Remove all of the constraints from the specified dag node.
//-------------------------------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::RemoveConstraintsFromDag( CDmeDag *pDag  )
{
	if ( pDag == NULL )
		return;

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
			DestroyConstraint( pConstraint );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Destroy the specified constraint and remove it from the animation set.
//-------------------------------------------------------------------------------------------------
void CDmeRigBaseConstraintOperator::DestroyConstraint( CDmeRigBaseConstraintOperator *pConstraint )
{
	if ( pConstraint == NULL )
		return;

	CDmeAnimationSet *pAnimSet = FindAncestorReferencingElement< CDmeAnimationSet >( pConstraint );

	// Find the weight channels associated with the constraint targets.	
	const CDmaElementArray< CDmeConstraintTarget > &targetList = pConstraint->GetTargets();

	int nTargets = targetList.Count();
	for ( int iTarget = 0; iTarget < nTargets; ++iTarget )
	{
		CDmeConstraintTarget *pTarget = targetList[ iTarget ];
		if ( pTarget == NULL )
			continue;

		CDmeChannel *pWeightChannel = pTarget->FindWeightChannel();
		if ( pWeightChannel == NULL )
			continue;

		// Destroy the control associated with the channel
		CDmElement* pControl = pWeightChannel->GetFromElement();
		if ( pControl )
		{
			if ( pAnimSet )
			{
				pAnimSet->RemoveControl( pControl );
			}
			CDmeRig::RemoveElementFromRig( pControl );
			DestroyElement( pControl );
		}

		// Remove the channel from the channels clip it belongs to
		CDmeChannelsClip *pChannelsClip = FindAncestorReferencingElement< CDmeChannelsClip >( pWeightChannel );
		if ( pChannelsClip )
		{
			pChannelsClip->RemoveChannel( pWeightChannel );
		}

		// Remove the channel from the rig.
		CDmeRig::RemoveElementFromRig( pWeightChannel );

		// Destroy the channel
		DestroyElement( pWeightChannel );
	}

	// Reconnect the original channels of the constrained dag back to the transform.
	pConstraint->ReconnectTransformChannels();

	// Remove the constraint from the animation set's list of operators or 
	// from the shot if the constraint did not belong to an animation set.
	if ( pAnimSet )
	{
		pAnimSet->RemoveOperator( pConstraint );
	}

	CDmeFilmClip *pClip = FindAncestorReferencingElement< CDmeFilmClip >( pConstraint );
	if ( pClip )
	{
		pClip->RemoveOperator( pConstraint );
	}

	// Remove the constraint from the rig it belongs to, if any
	CDmeRig::RemoveElementFromRig( pConstraint );

	// Destroy the constraint element
	DestroyElement( pConstraint );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the string name associated with the specified constraint type.
//-------------------------------------------------------------------------------------------------
char const *CDmeRigBaseConstraintOperator::ConstraintTypeName( EConstraintType eType )
{
	switch ( eType )
	{
		case CT_POINT:
			return( "pointConstraint" );
		case CT_ORIENT:
			return( "orientConstraint" );
		case CT_AIM:
			return( "aimConstraint" );
		case CT_IK:
			return( "ikConstraint" );
		case CT_PARENT:
			return( "parentConstraint" );
		case CT_ROTATION:
			return( "rotationConstraint" );
		default:
			break;
	}
	Assert( 0 );
	return "unknown";
}




//-------------------------------------------------------------------------------------------------
//
// CDmeRigPointConstraintOperator
//
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Point Constraint
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigPointConstraintOperator, CDmeRigPointConstraintOperator );

void CDmeRigPointConstraintOperator::OnConstruction()
{
}

void CDmeRigPointConstraintOperator::OnDestruction()
{
}

void CDmeRigPointConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigPointConstraintOperator::Operate", "SFM" );

	CDmeDag *pSlaveDag = m_Slave->GetDag();
	if ( !pSlaveDag )
		return;

	// Calculate the current target position and the total weight 
	// of the the targets contributing to the target position.
	Vector targetPos;
	float weight = ComputeTargetPosition( targetPos );

	// Blend between the target position and the base position using the target weight
	Vector finalPos = targetPos;
	if ( weight < 1.0f )
	{
		Vector basePosition;
		Quaternion baseOrientation;
		m_Slave->ComputeBaseWorldValues( basePosition, baseOrientation );
		finalPos = ( targetPos * weight ) + ( basePosition * ( 1.0f - weight ) );	
	}
	
	// Update the transform of the dag with the new position.
	pSlaveDag->SetAbsPosition( finalPos );
}



//-------------------------------------------------------------------------------------------------
//
// CDmeRigOrientConstraintOperator
//
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigOrientConstraintOperator, CDmeRigOrientConstraintOperator );

void CDmeRigOrientConstraintOperator::OnConstruction()
{
}

void CDmeRigOrientConstraintOperator::OnDestruction()
{
}

void CDmeRigOrientConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigOrientConstraintOperator::Operate", "SFM" );

	CDmeDag *pSlaveDag = m_Slave->GetDag();
	if ( !pSlaveDag )
		return;

	// Compute the target orientation and weight
	Quaternion qTarget;
	float weight = ComputeTargetOrientation( qTarget );

	// Blend between the target orientation and the base orientation using the target weight
	Quaternion qFinalWorld = qTarget;
	if ( weight < 1.0f )
	{
		Vector basePosition;
		Quaternion baseOrientation;
		m_Slave->ComputeBaseWorldValues( basePosition, baseOrientation );
		QuaternionSlerp( baseOrientation, qTarget, weight, qFinalWorld );
	}

	matrix3x4_t parentToWorld;
	pSlaveDag->GetParentWorldMatrix( parentToWorld );

	Quaternion qParentToWorld;
	MatrixQuaternion( parentToWorld, qParentToWorld );
	Quaternion qParentToWorldInv;
	QuaternionInvert( qParentToWorld, qParentToWorldInv );
	Quaternion qFinalLocal;
	QuaternionMult( qParentToWorldInv, qFinalWorld, qFinalLocal );
	pSlaveDag->GetTransform()->SetOrientation( qFinalLocal );
	

	/*
	CDmeDag *pSlaveDag = m_Slave->GetDag();
	if ( !pSlaveDag )
		return;

	// Compute the target orientation and weight
	Quaternion qTarget;
	float weight = ComputeTargetOrientation( qTarget );

	// Blend between the target orientation and the base orientation using the target weight
	Vector basePosition;
	Quaternion baseOrientation;
	m_Slave->ComputeBaseWorldValues( basePosition, baseOrientation );
	Quaternion qFinal;
	QuaternionSlerp( baseOrientation, qTarget, weight, qFinal );

	// Update the orientation of the slave, but maintain the position.
	matrix3x4_t absTxForm;
	pSlaveDag->GetAbsTransform( absTxForm );
	Vector worldPos;
	MatrixGetColumn( absTxForm, 3, worldPos );
	AngleMatrix( qFinal, worldPos, absTxForm );
	pSlaveDag->SetAbsTransform( absTxForm );
	*/
	

}



//-------------------------------------------------------------------------------------------------
//
// CDmeRigAimConstraintOperator
//
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigAimConstraintOperator, CDmeRigAimConstraintOperator );

void CDmeRigAimConstraintOperator::OnConstruction()
{
	m_AimOffset.InitAndSet( this, "aimOffset", quat_identity );
	m_UpVector.InitAndSet( this, "upVector", Vector( 0, 0, 1 ) );
	m_UpSpaceTarget.Init( this, "upSpaceTarget" );
	m_UpType.InitAndSet( this, "upType", CConstraintBones::AC_UP_TYPE_OBJECT_ROTATION );
}

void CDmeRigAimConstraintOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Purpose: Perform an update after targets are added to the constraint. Offset
// means something slightly different for aim constraint, it's the rotation 
// needed to orient the aimVector back at the original slave object. So all 
// target offsets are simply cleared out.
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::PostHandlesAdded( bool bPreserveOffset )
{
	for ( int i = 0; i < m_Targets.Count(); ++i )
	{
		m_Targets[ i ]->ClearOffset();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the up vector to be used when calculating the orientation from 
// the target position. The up vector may be in world space or in the space of
// a specified dag node.
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::SetUpVector( const Vector &upVector, bool bPreserveOffset, const CDmeDag *pUpSpaceTarget )
{
	CDmeDag *pDag = m_Slave->GetDag();
	if ( pDag == NULL )
		return;

	m_UpVector = upVector.Normalized();
	m_UpSpaceTarget = pUpSpaceTarget;
	UpdateOffset( bPreserveOffset );
}


//-----------------------------------------------------------------------------
// Purpose: Set the up vector to be used when calculating the orientation from 
// the target position. The up vector may be in world space or in the space of
// a specified dag node.
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::SetUpType( int nUpType )
{
	if ( nUpType < CConstraintBones::AC_UP_TYPE_FIRST || nUpType > CConstraintBones::AC_UP_TYPE_LAST )
	{
		nUpType = CConstraintBones::AC_UP_TYPE_OBJECT_ROTATION;
	}

	m_UpType = CConstraintBones::AC_UP_TYPE_OBJECT_ROTATION;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the orientation needed to make a transform where the y 
// vector of the transform matches the forward vector and the z vector matches
// the up reference vector as closely as possible. The x vector will be in the 
// plane defined by using the forward vector as the normal. 
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::AimAt( const Vector &vecForward, const Vector &referenceUp, Quaternion &q ) 
{ 
	Vector forward = vecForward;
	forward.NormalizeInPlace();
	float ratio = DotProduct( forward, referenceUp );
	Vector up = referenceUp - ( forward * ratio );
	up.NormalizeInPlace();

	Vector right = forward.Cross( up );
	right.NormalizeInPlace();

	const Vector &x = right;
	const Vector &y = forward;
	const Vector &z = up; 

	float tr = x.x + y.y + z.z; 
	q.Init( y.z - z.y , z.x - x.z, x.y - y.x, tr + 1.0f ); 
	float radius = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
	if ( radius > FLT_EPSILON )
	{
		QuaternionNormalize( q ); 
	}
	else
	{
		matrix3x4_t rotMat;
		MatrixSetColumn( x, 0, rotMat );
		MatrixSetColumn( y, 1, rotMat );
		MatrixSetColumn( z, 2, rotMat );
		MatrixQuaternion( rotMat, q );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the target orientation of the aim constraint based on the
// current slave position and the target position, using the up vector as a 
// reference to try and maintain.
//-----------------------------------------------------------------------------
float CDmeRigAimConstraintOperator::CalculateOrientation( Quaternion &targetOrientation )
{
	CDmeDag *pDag = m_Slave->GetDag();
	if ( pDag == NULL )
		return 0.0f;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return 0.0f;

	// Compute the world space position of the target
	Vector wsTargetPos;
	float weight = ComputeTargetPosition( wsTargetPos );

	// Construct the matrix to convert from world space to the parent space of the slave 
	matrix3x4_t parentToWorld;
	matrix3x4_t worldToParent;
	pDag->GetParentWorldMatrix( parentToWorld );
	MatrixInvert( parentToWorld, worldToParent );

	// If the up vector is in world space, convert it into local space
	Vector worldUp;
	Vector localUp;

	const CDmeDag *pUpSpaceDag = m_UpSpaceTarget;

	switch ( m_UpType )
	{
	case CConstraintBones::AC_UP_TYPE_VECTOR:
		VectorCopy( m_UpVector, worldUp );
		break;
	case CConstraintBones::AC_UP_TYPE_OBJECT:
		if ( pUpSpaceDag )
		{
			matrix3x4_t localToWorld;
			pUpSpaceDag->GetAbsTransform( localToWorld );

			Vector vUpObjectWorldPos;
			MatrixPosition( localToWorld, vUpObjectWorldPos );
			Vector vSlaveWorldPos;
			VectorTransform( pTransform->GetPosition(), parentToWorld, vSlaveWorldPos );
			VectorSubtract( vUpObjectWorldPos, vSlaveWorldPos, worldUp );
			VectorNormalize( worldUp );
		}
		else
		{
			VectorCopy( m_UpVector, worldUp );
		}
		break;
	case CConstraintBones::AC_UP_TYPE_PARENT_ROTATION:
		VectorRotate( m_UpVector, parentToWorld, worldUp );
		break;
	default:
	case CConstraintBones::AC_UP_TYPE_OBJECT_ROTATION:
		if ( pUpSpaceDag )
		{
			matrix3x4_t localToWorld;
			pUpSpaceDag->GetAbsTransform( localToWorld );
			VectorRotate( m_UpVector, localToWorld, worldUp );
		}
		else
		{
			VectorCopy( m_UpVector, worldUp );
		}
		break;
	}

	VectorRotate( worldUp, worldToParent, localUp );
	
	// Convert the target's world space position into the local space of the slave.
	Vector lsTargetPos;
	VectorTransform( wsTargetPos, worldToParent, lsTargetPos );

	// Compute the local space forward vector
	Vector slavePos = pTransform->GetPosition();
	Vector forward = lsTargetPos - slavePos;
	forward.NormalizeInPlace();

	// Compute the orientation 
	AimAt( forward, localUp, targetOrientation );	

	{
		Quaternion qAim;
		matrix3x4_t mUpToWorld;
		if ( pUpSpaceDag )
		{
			pUpSpaceDag->GetAbsTransform( mUpToWorld );
		}
		else
		{
			SetIdentityMatrix( mUpToWorld );
		}
		CConstraintBones::ComputeAimConstraint( qAim, wsTargetPos, parentToWorld, m_UpVector.Get(), slavePos, &mUpToWorld, static_cast< CConstraintBones::AimConstraintUpType_t >( m_UpType.Get() ) );
		Assert( QuaternionsAreEqual( qAim, targetOrientation, 1.0e-5 ) );
	}

	
	return weight;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the rotational offset which will be applied after aiming
// the transform at the target location. The rotational offset essentially 
// allows the selection of which direction within the slave's space will be
// considered the forward direction and will point towards the target.
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::UpdateOffset( bool bPreserveOffset )
{
	if ( !bPreserveOffset )
	{
		m_AimOffset = quat_identity;
		return;
	}

	CDmeDag *pDag = m_Slave->GetDag();
	if ( pDag == NULL )
		return;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return;

	// Calculate the desired orientation based the target position
	Quaternion targetOrientation;
	CalculateOrientation( targetOrientation );

	// Compute the difference between the slave's current orientation and the target orientation
	Quaternion slaveOrientation = pTransform->GetOrientation();
	Quaternion qInv, aimOffset;
	QuaternionInvert( targetOrientation, qInv );
	QuaternionMult( qInv, slaveOrientation, aimOffset );
	m_AimOffset = aimOffset;
}


//-----------------------------------------------------------------------------
// Purpose: Operate the aim constraint, modifying the orientation of the slave 
// so that it looks at the target position.
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigAimConstraintOperator::Operate", "SFM" );

	CDmeDag *pDag = m_Slave->GetDag();
	if ( pDag == NULL )
		return;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return;

	// Calculate the desired orientation based the target position
	Quaternion targetOrientation;
	float targetWeight = CalculateOrientation( targetOrientation );

	// Add in initial offset
	Quaternion offsetOrientation;
	QuaternionMult( targetOrientation, m_AimOffset, offsetOrientation );

	// Blend between the target orientation and the base orientation using the target weight
	Quaternion finalOrientation;
	Quaternion baseOrientation = m_Slave->GetBaseOrientation();
	QuaternionSlerp( baseOrientation, offsetOrientation, targetWeight, finalOrientation );

	// Update the orientation of the slave
	pTransform->SetOrientation( finalOrientation );
}


//-----------------------------------------------------------------------------
// Purpose: Get the attributes that the constraint reads data from, Inputs are 
// CDmeDags (handles usually)
//-----------------------------------------------------------------------------
void CDmeRigAimConstraintOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	CDmeRigBaseConstraintOperator::GetInputAttributes( attrs );

	// The position of the slave must be evaluated be
	CDmeDag *pSlaveDag = m_Slave->GetDag();
	if ( pSlaveDag )
	{
		AddAttribute( attrs, AA_TYPE_POSITION, pSlaveDag->GetTransform() );
	}
}

//-------------------------------------------------------------------------------------------------
//
// CDmeRigRotationConstraintOperator
//
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigRotationConstraintOperator, CDmeRigRotationConstraintOperator );



//-----------------------------------------------------------------------------
// Purpose: Perform post construction operations, including initializing member
// attributes.
//-----------------------------------------------------------------------------
void CDmeRigRotationConstraintOperator::OnConstruction()
{
	m_Rotations.Init( this, "rotations" );
	m_Axies.Init( this, "axies" );
}


//-----------------------------------------------------------------------------
// Purpose: Perform shutdown and cleanup operations.
//-----------------------------------------------------------------------------
void CDmeRigRotationConstraintOperator::OnDestruction()
{


}


//-----------------------------------------------------------------------------
// Purpose: Run the operator, this calculates the resulting quaternion 
// orientation and stores it in result.
//-----------------------------------------------------------------------------
void CDmeRigRotationConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigRotationConstraintOperator::Operate", "SFM" );

	CDmeDag *pDag = m_Slave->GetDag();
	if ( pDag == NULL )
		return;

	CDmeTransform *pTransform = pDag->GetTransform();
	if ( pTransform == NULL )
		return;

	Quaternion finalOrientation = quat_identity;

	int nAxies = MIN( m_Axies.Count(), m_Rotations.Count() );
	for ( int iAxis = 0; iAxis < nAxies; ++iAxis )
	{
		Vector axis = m_Axies[ iAxis ];
		float rotation = m_Rotations[ iAxis ];
		Quaternion orientation;
		AxisAngleQuaternion( axis, rotation, orientation );
		QuaternionNormalize( orientation );
		QuaternionMult( orientation, finalOrientation, finalOrientation );
		QuaternionNormalize( finalOrientation );
	}

	// Update the orientation of the slave
	pTransform->SetOrientation( finalOrientation );
}


//-----------------------------------------------------------------------------
// Purpose: Add the input attribute used by the operator to the provided list 
// of attributes, This is generally used by the evaluation process to find the 
// attributes an operator is dependent on.
//-----------------------------------------------------------------------------
void CDmeRigRotationConstraintOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_Axies.GetAttribute() );
	attrs.AddToTail( m_Rotations.GetAttribute() );
}


//-----------------------------------------------------------------------------
// Purpose: Add a rotation axis
//-----------------------------------------------------------------------------
int CDmeRigRotationConstraintOperator::AddAxis( const Vector &axis )
{
	int index = m_Axies.AddToTail( axis );
	m_Rotations.AddToTail( 0.0f );
	Assert( m_Rotations.Count() == m_Axies.Count() );
	return index;
}


//-----------------------------------------------------------------------------
// Purpose: Set the axis around which the rotation is to occur
//-----------------------------------------------------------------------------
void CDmeRigRotationConstraintOperator::SetAxis( const Vector &axis, int index )
{
	if ( index < m_Axies.Count() )
	{
		m_Axies.Set( index, axis );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set current rotation value
//-----------------------------------------------------------------------------
void CDmeRigRotationConstraintOperator::SetRotation( float rotation, int index )
{
	if ( index < m_Rotations.Count() )
	{
		m_Rotations.Set( index, rotation );
	}
}



//-------------------------------------------------------------------------------------------------
//
// Parent Constraint
//
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigParentConstraintOperator, CDmeRigParentConstraintOperator );

void CDmeRigParentConstraintOperator::OnConstruction()
{

}

void CDmeRigParentConstraintOperator::OnDestruction()
{

}


//-------------------------------------------------------------------------------------------------
// Compute the offsets of the specified target based on the relative transforms of the target to
// the slave
//-------------------------------------------------------------------------------------------------
void CDmeRigParentConstraintOperator::ComputeOffset( Vector &vOffset, Quaternion &qOffset, CDmeConstraintTarget *pTarget, bool bPreserveOffset )
{
	CDmeDag *pSlaveDag = m_Slave->GetDag();
	CDmeDag *pTargetDag = pTarget->GetDag();
	Assert( pSlaveDag );
	if ( !bPreserveOffset || !pSlaveDag|| !pTargetDag )
	{
		vOffset = vec3_origin;
		qOffset = quat_identity;
		return;
	}

	// Get slave abs info
	matrix3x4_t mS;
	m_Slave->GetBaseWorldTransform( mS );

	matrix3x4_t mT;
	pTargetDag->GetAbsTransform( mT );

	matrix3x4_t invT;
	MatrixInvert( mT, invT );

	// Compute slave offset local to target transform
	matrix3x4_t offset;
	ConcatTransforms( invT, mS, offset );
	MatrixAngles( offset, qOffset, vOffset );
}


//-------------------------------------------------------------------------------------------------
// Compute the aggregate target position and orientation from the weighted target list and return
// the resulting position and orientation in addition to updating the target dag.
//-------------------------------------------------------------------------------------------------
float CDmeRigParentConstraintOperator::ComputeTargetPositionOrientation( Vector &wsTargetPos, Quaternion &wsTargetOrientation )
{
	static Quaternion s_Quats[ MAX_RIG_TARGETS ];
	static float s_flQuatWeights[ MAX_RIG_TARGETS ];

	Quaternion *pQuats = s_Quats;
	float *flQuatWeights = s_flQuatWeights;

	int nTargets = m_Targets.Count();
	if ( nTargets > MAX_RIG_TARGETS )
	{
		pQuats = (Quaternion *)stackalloc( nTargets * sizeof( Quaternion ) );
		flQuatWeights = (float *)stackalloc( nTargets * sizeof( float ) );
	}

	float weightSum = 0.0f;

	wsTargetPos = vec3_origin;
	wsTargetOrientation = quat_identity;

	int nQuatCount = 0;

	for ( int i = 0; i < nTargets; ++i )
	{
		CDmeConstraintTarget *pTarget = m_Targets[ i ];
		Assert( pTarget );

		CDmeDag *pTargetDag = pTarget->GetDag();
		if ( !pTargetDag )
			continue;

		float flWeight = pTarget->GetWeight();

		matrix3x4_t handleM;
		pTargetDag->GetAbsTransform( handleM );

		matrix3x4_t offset;
		Vector vOffset = pTarget->GetPositionOfffset();
		Quaternion qOffset = pTarget->GetOrientationOffset();
		AngleMatrix( RadianEuler( qOffset ), vOffset, offset );

		matrix3x4_t absTxForm;
		ConcatTransforms( handleM, offset, absTxForm );

		Vector pos;
		Quaternion q;
		MatrixAngles( absTxForm, q, pos );

		wsTargetPos += ( flWeight * pos );

		pQuats[ nQuatCount ] = q;
		flQuatWeights[ nQuatCount ] = flWeight;
		++nQuatCount;

		// For normalization
		weightSum += flWeight;
	}

	if ( weightSum > 0.0f )
	{
		wsTargetPos *= 1.0f / weightSum;
	}

	QuaternionAverageExponential( wsTargetOrientation, nQuatCount, pQuats, flQuatWeights );

	return MIN( 1.0f, weightSum );
}


void CDmeRigParentConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigParentConstraintOperator::Operate", "SFM" );

	CDmeDag *pSlaveDag = m_Slave->GetDag();
	if ( !pSlaveDag )
		return;

	// Compute the target orientation and weight
	Vector targetPosition;
	Quaternion targetOrientation;
	float weight = ComputeTargetPositionOrientation( targetPosition, targetOrientation );
	
	Vector finalPosition = targetPosition;
	Quaternion finalOrientation = targetOrientation;

	// Blend between the target orientation and the base orientation using the target weight
	if ( weight < 1.0f )
	{		
		Vector basePosition;
		Quaternion baseOrientation;
		m_Slave->ComputeBaseWorldValues( basePosition, baseOrientation );
		VectorLerp( basePosition, targetPosition, weight, finalPosition );
		QuaternionSlerp( baseOrientation, targetOrientation, weight, finalOrientation );
	}

	matrix3x4_t finalTransform;
	AngleMatrix( RadianEuler( finalOrientation ), finalPosition, finalTransform );
	pSlaveDag->SetAbsTransform( finalTransform );
}


//-----------------------------------------------------------------------------
// 3 joint (2 bone) IK Constraint
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigIKConstraintOperator, CDmeRigIKConstraintOperator );


//-----------------------------------------------------------------------------
// Purpose: Perform initialization operations.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::OnConstruction()
{
	m_StartOffsetRotation.InitAndSet( this, "startOffset", quat_identity );
	m_MidOffsetRotation.InitAndSet( this, "midOffset", quat_identity );
	m_PoleVector.InitAndSet( this, "poleVector", Vector( 0, 0, 1 ) );
	m_PoleVectorTarget.Init( this, "pvTarget" );
	m_StartJoint.InitAndCreate( this, "startJoint" );
	m_MidJoint.InitAndCreate( this, "midJoint" );
	m_EndJoint.InitAndCreate( this, "endJoint" );
}


//-----------------------------------------------------------------------------
// Purpose: Perform shutdown operations
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::OnDestruction()
{
	g_pDataModel->DestroyElement( m_StartJoint );
	g_pDataModel->DestroyElement( m_MidJoint );
	g_pDataModel->DestroyElement( m_EndJoint );
}


//-----------------------------------------------------------------------------
// Purpose: Validate that the start and end effector are set up and that there 
// is only one intermediate bone between them forming a 2 bone ik chain. If 
// specified calculate the offset of the current position of the bones from the
// target position calculated by the ik solution and store as an offset to be 
// applied upon operation.
//-----------------------------------------------------------------------------
bool CDmeRigIKConstraintOperator::Setup( bool bPreserveOffset )
{
	CDmeDag *pStartJoint = m_StartJoint->GetDag();
	CDmeDag *pMidJoint = m_MidJoint->GetDag();
	CDmeDag *pEndJoint = m_EndJoint->GetDag();
	CDmeTransform *pStartTransform = pStartJoint->GetTransform();
	CDmeTransform *pMidTransform = pMidJoint->GetTransform();

	// Make sure all the joints have been specified.
	if ( !pStartJoint || !pMidJoint || !pEndJoint )
		return false;

	// Verify the chain
	if ( ( pEndJoint->GetParent() != pMidJoint ) || ( pMidJoint->GetParent() != pStartJoint ) )
		return false;

	// Make sure there is one and only one target and that it is valid
	if ( m_Targets.Count() != 1 )
		return false;
	
	CDmeDag *pTargetDag = m_Targets[ 0 ]->GetDag();
	if ( pTargetDag == NULL )
		return false;

	// Calculate the orientation that the start and end joints would be set to if the ik were run,
	// the calculate the difference between the result and the current orientation.
	if ( bPreserveOffset )
	{
		Quaternion qStartIK = quat_identity;
		Quaternion qMidIK = quat_identity;
		CalculateOrientations( qStartIK, qMidIK, quat_identity, quat_identity );

		// Calculate the offset of the start joint
		Quaternion qStartCurrent = pStartTransform->GetOrientation();
		Quaternion qStartInv, qStartOffset;
		QuaternionInvert( qStartIK, qStartInv );
		QuaternionMult( qStartInv, qStartCurrent, qStartOffset );
		m_StartOffsetRotation = qStartOffset;
		
		// Calculate the offset of the mid joint, note the ik solve has to be re-run 
		// with the offset of the start joint to get the proper offset for the mid joint.
		CalculateOrientations( qStartIK, qMidIK, m_StartOffsetRotation, quat_identity );
		Quaternion qMidCurrent = pMidTransform->GetOrientation();
		Quaternion qMidInv, qMidOffset;
		QuaternionInvert( qMidIK, qMidInv );
		QuaternionMult( qMidInv, qMidCurrent, qMidOffset );
		m_MidOffsetRotation = qMidOffset;		
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the orientation needed to make a transform where the x 
// vector of the transform matches the forward vector and the z vector matches
// the up reference vector as closely as possible. The y vector will be in the 
// plane defined by using the forward vector as the normal. 
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::AimAt( const Vector &vecForward, const Vector &referenceUp, Quaternion &q ) 
{ 
	Vector forward = vecForward;
	forward.NormalizeInPlace();
	float ratio = DotProduct( forward, referenceUp );
	Vector up = referenceUp - ( forward * ratio );
	up.NormalizeInPlace();

	Vector left = up.Cross( forward );
	left.NormalizeInPlace();

	const Vector &x = forward;
	const Vector &y = left;
	const Vector &z = up;

	float tr = x.x + y.y + z.z; 
	q.Init( y.z - z.y, z.x - x.z, x.y - y.x, tr + 1.0f ); 
	float radius = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];

	if ( radius > FLT_EPSILON )
	{
		QuaternionNormalize( q ); 
	}
	else
	{
		matrix3x4_t rotMat;
		MatrixSetColumn( x, 0, rotMat );
		MatrixSetColumn( y, 1, rotMat );
		MatrixSetColumn( z, 2, rotMat );
		MatrixQuaternion( rotMat, q );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Perform the 2 bone ik solve to get target orientation for the 
// start and mid bones and apply the specified offset rotations to get the 
// final target orientation for each bone.
//-----------------------------------------------------------------------------
float CDmeRigIKConstraintOperator::CalculateOrientations( Quaternion &startBoneOrientation, Quaternion &midBoneOrientation, const Quaternion &startOffset, const Quaternion &midOffset )
{
	// Get the pointers to the dag nodes and transforms that participate in the calculations.
	CDmeDag *pStartJoint = m_StartJoint->GetDag();
	CDmeDag *pMidJoint = m_MidJoint->GetDag();
	CDmeDag *pEndJoint = m_EndJoint->GetDag();
	CDmeTransform *pStartTransform = pStartJoint->GetTransform();
	CDmeTransform *pMidTransform = pMidJoint->GetTransform();
	CDmeTransform *pEndTransform = pEndJoint->GetTransform();

	// Get the transform matrix of the parent of the start joint, this transform defines the 
	// local space of the start joint, which is the space in which the calculations will be done.
	matrix3x4_t mLocalToWorld;
	matrix3x4_t mWorldToLocal;
	pStartJoint->GetParentWorldMatrix( mLocalToWorld );
	MatrixInvert( mLocalToWorld, mWorldToLocal );

	// Update the target position and get the result in world space
	Vector wsTargetPos;
	float weight = ComputeTargetPosition( wsTargetPos );

	// Convert the world space target position into the local space 
	Vector lsTargetPos;
	VectorTransform( wsTargetPos, mWorldToLocal, lsTargetPos );

	// Find the axis, which is the vector from the start joint to the end joint and then calculate 
	// the distance of the axis and each of the bones. Additionally calculate vDir, the unit vector
	// along the direction of the axis.
	Vector startPos = pStartTransform->GetPosition();
	Vector vAxis = lsTargetPos - startPos;
	Vector vDir = vAxis;
	float axisLen = VectorNormalize( vDir );
	float boneLenA = pMidTransform->GetPosition().Length();
	float boneLenB = pEndTransform->GetPosition().Length();
	axisLen = MIN( axisLen, boneLenA + boneLenB );
	lsTargetPos = startPos + ( vDir * axisLen );

	// Find the up vector, this is the vector in plane perpendicular to the axis along which the 
	// mid point will move as a function of the length of the axis.
	Vector vPole;
	CDmeDag *pPoleVectorTarget = m_PoleVectorTarget;
	if ( pPoleVectorTarget )
	{
		Vector wsPVTargetPos;
		pPoleVectorTarget->GetAbsPosition( wsPVTargetPos );
		Vector lsPVTargetPos;
		VectorTransform( wsPVTargetPos, mWorldToLocal, lsPVTargetPos );
		vPole = lsPVTargetPos - startPos;
	}
	else
	{
		VectorRotate( m_PoleVector, mWorldToLocal, vPole );
	}
	Vector vUp = vPole - ( vDir * DotProduct( vDir, vPole ) );
	VectorNormalize( vUp );

	// Calculate the distance from the start point to the axis mid point. The axis mid point is the
	// location on the axis that is the projection of the final mid point position onto the axis.
	float midAxisLen = ( ( boneLenA * boneLenA ) - ( boneLenB * boneLenB ) + ( axisLen * axisLen ) ) / ( 2.0f * axisLen );

	// Calculate the distance of the mid point from the axis
	float midDist = sqrt( MAX( 0, ( boneLenA * boneLenA ) - ( midAxisLen * midAxisLen ) ) );

	// Calculate the vector to the new mid point position, and then calculate the new mid point position.
	Vector vMid = ( vDir * midAxisLen ) + ( vUp * midDist );
	Vector newMidPos = startPos + vMid;

	// Calculate the orientation of the first bone
	Quaternion qStartTarget;
	AimAt( vMid, vUp, qStartTarget );
	QuaternionMult( qStartTarget, startOffset, startBoneOrientation );

	// Calculate the orientation of the second bone
	Vector vEnd = lsTargetPos - newMidPos;
	Quaternion qParentBoneOrientB;
	Quaternion qInvBoneOrientA;
	Quaternion qMidTarget;
	AimAt( vEnd, vUp, qParentBoneOrientB ); 
	QuaternionInvert( startBoneOrientation, qInvBoneOrientA );
	QuaternionMult( qInvBoneOrientA, qParentBoneOrientB, qMidTarget );
	QuaternionMult( qMidTarget, midOffset, midBoneOrientation );

	return weight;
}


//-----------------------------------------------------------------------------
// Purpose: Run the ik solve and apply the results to the constrained bones.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigIKConstraintOperator::Operate", "SFM" );

	CDmeDag *pStartJoint = m_StartJoint->GetDag();
	CDmeDag *pMidJoint = m_MidJoint->GetDag();
	CDmeDag *pEndJoint = m_EndJoint->GetDag();

	// Make sure all the joints have been specified.
	if ( !pStartJoint || !pMidJoint || !pEndJoint )
		return;

	// Calculate the new orientation of the start and mid joints
	Quaternion qStartTarget = quat_identity;
	Quaternion qMidTarget = quat_identity;
	float targetWeight = CalculateOrientations( qStartTarget, qMidTarget, m_StartOffsetRotation, m_MidOffsetRotation );
	
	// Update the orientations of the start and mid point joints
	CDmeTransform *pStartTransform = m_StartJoint->GetDag()->GetTransform();
	CDmeTransform *pMidTransform = m_MidJoint->GetDag()->GetTransform();
	Quaternion qStartBase = m_StartJoint->GetBaseOrientation();
	Quaternion qMidBase = m_MidJoint->GetBaseOrientation();
	Quaternion qStartFinal, qMidFinal;
	QuaternionSlerp( qStartBase, qStartTarget, targetWeight, qStartFinal );
	QuaternionSlerp( qMidBase, qMidTarget, targetWeight, qMidFinal );
	pStartTransform->SetOrientation( qStartFinal );
	pMidTransform->SetOrientation( qMidFinal );
}


//-----------------------------------------------------------------------------
// Purpose: Assign the dag nodes that to be controlled by the ik constraint.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::SetJoints( CDmeDag *pStartJoint, CDmeDag *pMidJoint, CDmeDag *pEndJoint )
{
	m_StartJoint->SetDag( pStartJoint );
	m_MidJoint->SetDag( pMidJoint );
	m_EndJoint->SetDag( pEndJoint );
}


//-----------------------------------------------------------------------------
// Purpose: Set the world space pole vector for the ik constraint
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::SetPoleVector( const Vector &poleVector )
{
	m_PoleVector = poleVector;
}


//-----------------------------------------------------------------------------
// Purpose: Set the pole vector target, a dag node toward which the pole vector
// will point, overrides the standard pole vector.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::SetPoleVectorTarget( CDmeDag *pPoleVectorTarget )
{
	m_PoleVectorTarget = pPoleVectorTarget;
}


//-----------------------------------------------------------------------------
// Purpose: Get the attributes which are written to by the constraint.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	AddAttribute( attrs, AA_TYPE_ORIENTATION, m_StartJoint->GetTransform() );
	AddAttribute( attrs, AA_TYPE_ORIENTATION, m_MidJoint->GetTransform() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the attributes upon which the constraint is dependent.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	BaseClass::GetInputAttributes( attrs );
	
	AddAncestorAttributes( attrs, m_StartJoint->GetDag() );

	AddAttribute( attrs, AA_TYPE_POSITION, m_StartJoint->GetTransform() );
	AddAttribute( attrs, AA_TYPE_POSITION, m_MidJoint->GetTransform() );
	AddAttribute( attrs, AA_TYPE_POSITION, m_EndJoint->GetTransform() );
}


//-----------------------------------------------------------------------------
// Purpose: Find all of the channels relevant to all of the target handles 
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::GatherInputOperators( CUtlVector< CDmeOperator * > &operatorList )
{	
	if ( m_PoleVectorTarget.GetElement() )
	{
		m_PoleVectorTarget->FindRelevantOperators( operatorList );
	}

	BaseClass::GatherInputOperators( operatorList );
}


//-----------------------------------------------------------------------------
// Purpose: Compute the offset for the specified target. The ik constraint does
// not use target offsets, so this function simply clears the offset for the
// specified target.
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::ComputeOffset( Vector &vOffset, Quaternion &qOffset, CDmeConstraintTarget *pTarget, bool bPreserveOffset )
{
	// No target offset allowed
	vOffset = vec3_origin;
	qOffset = quat_identity;
}


//-----------------------------------------------------------------------------
// Purpose: Disconnect the channels driving the slave dag nodes from the dag 
// transforms and connect them to the constraint
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::DisconnectTransformChannels()
{
	DisconnectSlaveChannels( m_StartJoint.GetElement(), AA_TYPE_ORIENTATION );
	DisconnectSlaveChannels( m_MidJoint.GetElement(), AA_TYPE_ORIENTATION );
}

//-----------------------------------------------------------------------------
// Purpose: Reconnect the base channels of each slave directly to the dag
//-----------------------------------------------------------------------------
void CDmeRigIKConstraintOperator::ReconnectTransformChannels()
{
	ReconnectSlaveChannels( m_StartJoint.GetElement(), AA_TYPE_ORIENTATION );
	ReconnectSlaveChannels( m_MidJoint.GetElement(), AA_TYPE_ORIENTATION  );
}


//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the dag node the constraint is controlling. For 
// the ik constraint this is the start joint, but both the start and the mid
// joint are actually 'slaves' of the constraint.
//-----------------------------------------------------------------------------
const CDmeDag *CDmeRigIKConstraintOperator::GetSlave() const
{
	return m_StartJoint->GetDag();
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the the constraint has slave with the specified name,
// for the ik constraint this is always false.
//-----------------------------------------------------------------------------
bool CDmeRigIKConstraintOperator::IsSlaveObject( char const *pchName ) const
{
	// IK Handle doesn't have targets that can be added
	return false;
}


//-------------------------------------------------------------------------------------------------
//
// CDmeRigTwistSlave
//
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Expose the CDmeConstraintTarget class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigTwistSlave, CDmeRigTwistSlave );


//-----------------------------------------------------------------------------
// Purpose: Perform initialization operations.
//-----------------------------------------------------------------------------
void CDmeRigTwistSlave::OnConstruction()
{
	m_flWeight.Init( this, "weight" );
}


//-----------------------------------------------------------------------------
// Purpose: Perform shutdown operations
//-----------------------------------------------------------------------------
void CDmeRigTwistSlave::OnDestruction()
{
}


//-------------------------------------------------------------------------------------------------
//
// CDmeRigTwistConstraintOperator
//
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Expose the CDmeConstraintTarget class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRigTwistConstraintOperator, CDmeRigTwistConstraintOperator );


//-----------------------------------------------------------------------------
// Purpose: Perform initialization operations.
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::OnConstruction()
{
	m_bInverse.Init( this, "inverse" );
	m_vUpAxis.InitAndSet( this, "upVector", Vector( 0.0f, 1.0f, 0.0f ) );
	m_flWeights.Init( this, "weights" );
	m_eSlaves.Init( this, "twistJoints" );
	m_qParentBindRotation.InitAndSet( this, "parentBindRotation", quat_identity );
	m_qChildBindRotation.InitAndSet( this, "childBindRotation", quat_identity );
}


//-----------------------------------------------------------------------------
// Purpose: Perform shutdown operations
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::OnDestruction()
{
	ClearSlaves();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeRigTwistConstraintOperator::SetTargets( CDmeDag *pDmeDagParent, CDmeDag *pDmeDagChild )
{
	ClearHandles();

	if ( !pDmeDagParent || !pDmeDagChild )
		return false;

	CDmeDag *dmeDags[] = { pDmeDagParent, pDmeDagChild };
	float flWeights[] = { 1.0f, 1.0f };

	AddHandles( 2, dmeDags, flWeights, false, NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return the DmeDag parent target or NULL
// m_Targets[0] is the parent target
//-----------------------------------------------------------------------------
CDmeDag *CDmeRigTwistConstraintOperator::GetParentTarget() const
{
	return m_Targets.Count() >= 1 ? m_Targets[0]->GetDag() : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Return the DmeDag child target or NULL
// m_Targets[0] is the parent target
//-----------------------------------------------------------------------------
CDmeDag *CDmeRigTwistConstraintOperator::GetChildTarget() const
{
	return m_Targets.Count() >= 2 ? m_Targets[1]->GetDag() : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Remove all slaves
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::ClearSlaves()
{
	for ( int i = 0; i < m_eSlaves.Count(); ++i )
	{
		CDmeConstraintSlave *pDmeConstraintSlave = m_eSlaves[i];
		if ( !pDmeConstraintSlave )
			continue;

		DestroyElement( pDmeConstraintSlave );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add the specified DmeDag as a slave with the specified weight
// Returns: The index of the slave, -1 on failure
//-----------------------------------------------------------------------------
int CDmeRigTwistConstraintOperator::AddSlave( CDmeDag *pDmeDagSlave, float flWeight )
{
	CDmeRigTwistSlave *pDmeRigTwistSlave = CreateElement< CDmeRigTwistSlave >( pDmeDagSlave->GetName(), GetFileId() );
	if ( !pDmeRigTwistSlave )
		return -1;

	// This assume the current rotation of the specified dag is the base/bind rotation
	matrix3x4_t mBind;
	pDmeDagSlave->GetLocalMatrix( mBind );
	Quaternion qBind;
	MatrixQuaternion( mBind, qBind );

	pDmeRigTwistSlave->SetDag( pDmeDagSlave );
	pDmeRigTwistSlave->SetWeight( flWeight );
	pDmeRigTwistSlave->SetBaseOrientation( qBind );

	return m_eSlaves.AddToTail( pDmeRigTwistSlave );
}


//-----------------------------------------------------------------------------
// Purpose: Return the DmeDag of the slave at the specified index
//-----------------------------------------------------------------------------
CDmeDag *CDmeRigTwistConstraintOperator::GetSlaveDag( int i ) const
{
	return m_eSlaves.Count() > i ? m_eSlaves[i]->GetDag() : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Return the weight of the slave at the specified index
//-----------------------------------------------------------------------------
float CDmeRigTwistConstraintOperator::GetSlaveWeight( int i ) const
{
	return m_eSlaves.Count() > i ? m_eSlaves[i]->GetWeight() : 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Set the bind orientation of the parent
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::SetParentBindRotation( const Quaternion &qBindRotation )
{
	m_qParentBindRotation = qBindRotation;
}


//-----------------------------------------------------------------------------
// Purpose: Set the bind orientation of the child
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::SetChildBindRotation( const Quaternion &qBindRotation )
{
	m_qChildBindRotation = qBindRotation;
}


//-----------------------------------------------------------------------------
// Purpose: Return the bind orientation of the slave at the specified index
//-----------------------------------------------------------------------------
const Quaternion &CDmeRigTwistConstraintOperator::GetSlaveBindOrientation( int i ) const
{
	if ( m_eSlaves.Count() > i )
	{
		return m_eSlaves[i]->GetBaseOrientation();
	}
	else
	{
		return quat_identity;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the bind orientation of the slave at the specified index
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::SetSlaveBindOrientation( const Quaternion &qBindRotation, int i )
{
	if ( m_eSlaves.Count() > i )
	{
		m_eSlaves[i]->SetBaseOrientation( qBindRotation );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Compute the twist
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::Operate()
{
	VPROF_BUDGET( "CDmeRigTwistConstraintOperator::Operate", "SFM" );

	/*
	 * TODO: Call the Twist constraint code
	 *       But it needs to be moved into bonesetup first
	 *       Right now this is just a data container
	 */

}


//-----------------------------------------------------------------------------
// Purpose: Get the attributes which are written to by the constraint.
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( int i = 0; i < m_eSlaves.Count(); ++i )
	{
		CDmeConstraintSlave *pDmeConstraintSlave = m_eSlaves[i];
		if ( !pDmeConstraintSlave )
			continue;

		AddAttribute( attrs, AA_TYPE_ORIENTATION, pDmeConstraintSlave->GetTransform() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Compute the offset for the specified target. The twist constraint
// does not use target offsets, so this function simply clears the offset for the
// specified target.
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::ComputeOffset( Vector &vOffset, Quaternion &qOffset, CDmeConstraintTarget *pTarget, bool bPreserveOffset )
{
	// No target offset allowed
	vOffset = vec3_origin;
	qOffset = quat_identity;
}


//-----------------------------------------------------------------------------
// Purpose: Disconnect the channels driving the slave dag nodes from the dag 
// transforms and connect them to the constraint
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::DisconnectTransformChannels()
{
	for ( int i = 0; i < m_eSlaves.Count(); ++i )
	{
		CDmeConstraintSlave *pDmeConstraintSlave = m_eSlaves[i];
		if ( !pDmeConstraintSlave )
			continue;

		DisconnectSlaveChannels( pDmeConstraintSlave, AA_TYPE_ORIENTATION );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Reconnect the base channels of each slave directly to the dag
//-----------------------------------------------------------------------------
void CDmeRigTwistConstraintOperator::ReconnectTransformChannels()
{
	for ( int i = 0; i < m_eSlaves.Count(); ++i )
	{
		CDmeConstraintSlave *pDmeConstraintSlave = m_eSlaves[i];
		if ( !pDmeConstraintSlave )
			continue;

		ReconnectSlaveChannels( pDmeConstraintSlave, AA_TYPE_ORIENTATION );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the dag node the constraint is controlling.
// Return the first 
//-----------------------------------------------------------------------------
const CDmeDag *CDmeRigTwistConstraintOperator::GetSlave() const
{
	return m_eSlaves.Count() ? m_eSlaves[0]->GetDag() : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the the constraint has slave with the specified name
//-----------------------------------------------------------------------------
bool CDmeRigTwistConstraintOperator::IsSlaveObject( char const *pchName ) const
{
	for ( int i = 0; i < m_eSlaves.Count(); ++i )
	{
		CDmeConstraintSlave *pDmeConstraintSlave = m_eSlaves[i];
		if ( !pDmeConstraintSlave )
			continue;

		if ( !V_stricmp( pchName, pDmeConstraintSlave->GetName() ) )
			return true;
	}

	return false;
}
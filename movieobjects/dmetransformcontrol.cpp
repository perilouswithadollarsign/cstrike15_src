//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: Implementation of CDmeTransformControl class, a helper for 
// modifying a transform. The CDmeTransformControl class implementation
// contains functions for setting and retrieving parameters used in transform
// manipulations, these parameters are allocated when the first value is set.
// If the manipulation parameters have not been allocated a default value will
// be returned. Additionally the CDmeTransformControl provides access to the 
// rotation channel associated with the transform, even if the transform 
// control is not directly attached to the rotation channel.
//
//=============================================================================
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmetransformcontrol.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects/dmerigconstraintoperators.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTransformControl, CDmeTransformControl );


//-----------------------------------------------------------------------------
// Purpose: Provide post construction processing.
//-----------------------------------------------------------------------------
void CDmeTransformControl::OnConstruction()
{
	m_PivotOffset = vec3_origin;
	m_pManipulationParams = NULL;

	m_PositionValue.InitAndSet( this, "valuePosition", vec3_origin );
	m_OrientationValue.InitAndSet( this, "valueOrientation", quat_identity );
	m_PositionChannel.Init( this, "positionChannel" );
	m_OrientationChannel.Init( this, "orientationChannel" );
}


//-----------------------------------------------------------------------------
// Purpose: Provide processing and cleanup before shutdown
//-----------------------------------------------------------------------------
void CDmeTransformControl::OnDestruction()
{
	// Destroy the manipulation parameters if they have been created.
	delete m_pManipulationParams;
	m_pManipulationParams = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Allocate and initialize the manipulation parameters
//
// Output : Returns true if the manipulation parameters were successfully 
//			initialized, false if the initialization failed.
//-----------------------------------------------------------------------------
bool CDmeTransformControl::InitManipulationParams()
{
	if ( m_pManipulationParams == NULL )
	{
		m_pManipulationParams = new ManipulationParams_t;

		if ( m_pManipulationParams )
		{
			m_pManipulationParams->Pivot = vec3_origin;
			m_pManipulationParams->RotationLocal = quat_identity;
			m_pManipulationParams->RotationParent = quat_identity;
			SetIdentityMatrix( m_pManipulationParams->Transform );
		}
	}

	return ( m_pManipulationParams != NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Get the manipulation transform matrix
//
// Output : transform - The transform that will be applied by the current 
//			manipulation.
//-----------------------------------------------------------------------------
void CDmeTransformControl::GetManipulationTransform( matrix3x4_t &transform ) const
{
	if ( m_pManipulationParams )
	{
		transform = m_pManipulationParams->Transform;
	}
	else
	{
		SetIdentityMatrix( transform );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the manipulation transform matrix
//
// Input  : transform - Matrix specifying the transform to be performed by the
//			manipulation.
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetManipulationTransform( const matrix3x4_t &transform )
{
	if ( InitManipulationParams() )
	{
		m_pManipulationParams->Transform = transform;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the manipulation rotation amount in the transform local space 
//
// Output : deltaRotation - Current rotation that will be applied in the 
//			transform manipulation
//-----------------------------------------------------------------------------
void CDmeTransformControl::GetManipulationRotationLocal( Quaternion &rotation ) const
{
	if ( m_pManipulationParams )
	{
		rotation = m_pManipulationParams->RotationLocal;
	}
	else
	{
		rotation = quat_identity;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the manipulation rotation amount in the transform local space
//
// Input  : deltaRotation - Rotation to be applied in the transform manipulation
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetManipulationRotationLocal( const Quaternion &rotation )
{
	if ( InitManipulationParams() )
	{
		m_pManipulationParams->RotationLocal = rotation;
	}
}


//-----------------------------------------------------------------------------
// Get the manipulation rotation amount in the parent space of the transform
//
// Output : localRotation - Current local rotation that will be applied in the 
//			transform manipulation 
//-----------------------------------------------------------------------------
void CDmeTransformControl::GetManipulationRotationParent( Quaternion &rotation ) const
{
	if ( m_pManipulationParams )
	{
		rotation = m_pManipulationParams->RotationParent;
	}
	else
	{
		rotation = quat_identity;
	}
}


//-----------------------------------------------------------------------------
// Set the manipulation rotation amount in the parent space of the transform
//
// Input  : localRotation - Local rotation to be applied in the transform
//			manipulation.
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetManipulationRotationParent( const Quaternion &rotation )
{
	if ( InitManipulationParams() )
	{
		m_pManipulationParams->RotationParent = rotation;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the manipulation pivot position, this may differ from the pivot 
// offset
//
// Output : pivotPosition - Pivot position being used for the current transform 
//			manipulation
//-----------------------------------------------------------------------------
void CDmeTransformControl::GetManipulationPivot( Vector &pivotPosition ) const
{
	if ( m_pManipulationParams )
	{
		pivotPosition = m_pManipulationParams->Pivot;
	}
	else
	{
		pivotPosition = vec3_origin;
	}

}


//-----------------------------------------------------------------------------
// Purpose: Set the manipulation pivot position
//
// Input  : Position to be used as the pivot location for manipulations
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetManipulationPivot( const Vector &pivotPosition )
{
	if ( InitManipulationParams() )
	{
		m_pManipulationParams->Pivot = pivotPosition;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the current local space pivot offset
//
// Output : Return the current local space pivot offset
//-----------------------------------------------------------------------------
const Vector &CDmeTransformControl::GetPivotOffset() const
{
	return m_PivotOffset;
}


//-----------------------------------------------------------------------------
// Purpose: Set the current local space pivot offset
//
// Input  : localOffset - The new local offset that is to be assigned to the 
//			pivot.
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetPivotOffset( const Vector &localOffset )
{
	m_PivotOffset = localOffset;
}


//-----------------------------------------------------------------------------
// Purpose: Get the transform associated with the transform control
//
// Output : Returns the pointer to the transform element
//-----------------------------------------------------------------------------
CDmeTransform *CDmeTransformControl::GetTransform() const
{
	CDmeTransform *pTransform = NULL;

	CDmeChannel *pChannel = GetPositionChannel();
	if ( pChannel == NULL )
	{
		pChannel = GetOrientationChannel();
	}
	
	if ( pChannel )
	{
		CDmElement *pToElement = pChannel->GetToElement();
		pTransform = CastElement< CDmeTransform>( pToElement );

		// If the element targeted by the channel is not a transform, 
		// see if it is a constraint slave, and get the transform from that.
		if ( pTransform == NULL )
		{	
			CDmeConstraintSlave *pConstraintSlave = CastElement< CDmeConstraintSlave >( pToElement );
			if ( pConstraintSlave )
			{
				CDmeDag *pDag = pConstraintSlave->GetDag();
				if ( pDag )
				{
					pTransform = pDag->GetTransform();
				}
			}
		}
	} 

	return pTransform;
}


//-----------------------------------------------------------------------------
// Get the dag node associated with the transform control
// 
// Output: Returns the pointer to the dag node targeted by the transform 
// control.
//-----------------------------------------------------------------------------
CDmeDag *CDmeTransformControl::GetDag() const
{
	CDmeDag *pDagNode = NULL;
	CDmeTransform *pTransform = GetTransform();

	if ( pTransform  )
	{
		pDagNode = pTransform->GetDag();
	}

	return pDagNode;
}


//-----------------------------------------------------------------------------
// Get the position attribute of the control
//-----------------------------------------------------------------------------
CDmAttribute *CDmeTransformControl::GetPositionAttr()
{
	return m_PositionValue.GetAttribute();
}


//-----------------------------------------------------------------------------
// Get the orientation attribute of the control
//-----------------------------------------------------------------------------
CDmAttribute *CDmeTransformControl::GetOrientationAttr()
{
	return m_OrientationValue.GetAttribute();
}


//-----------------------------------------------------------------------------
// Get the position value of the control
//-----------------------------------------------------------------------------
const Vector &CDmeTransformControl::GetPosition() const
{
	return m_PositionValue;
}


//-----------------------------------------------------------------------------
// Get the orientation value of the control
//-----------------------------------------------------------------------------
const Quaternion &CDmeTransformControl::GetOrientation() const
{
	return m_OrientationValue;
}


//-----------------------------------------------------------------------------
// Determine if the control currently has a default position set
//-----------------------------------------------------------------------------
bool CDmeTransformControl::HasDefaultPosition() const
{
	return HasAttribute( DEFAULT_POSITION_ATTR, AT_VECTOR3 );
}


//-----------------------------------------------------------------------------
// Determine if the control currently has a default orientation set
//-----------------------------------------------------------------------------
bool CDmeTransformControl::HasDefaultOrientation() const
{
	return HasAttribute( DEFAULT_ORIENTATION_ATTR, AT_QUATERNION );
}


//-----------------------------------------------------------------------------
// Get the default position of the control
//-----------------------------------------------------------------------------
const Vector &CDmeTransformControl::GetDefaultPosition() const
{
	return GetValue< Vector >( DEFAULT_POSITION_ATTR, vec3_origin );
}


//-----------------------------------------------------------------------------	
// Get the default orientation of the control
//-----------------------------------------------------------------------------
const Quaternion &CDmeTransformControl::GetDefaultOrientation() const
{
	return GetValue< Quaternion >( DEFAULT_ORIENTATION_ATTR, quat_identity );
}


//-----------------------------------------------------------------------------
// Get the default position attribute
//-----------------------------------------------------------------------------
const CDmAttribute *CDmeTransformControl::GetDefaultPositionAttr() const
{
	return GetAttribute( DEFAULT_POSITION_ATTR, AT_VECTOR3 );
}


//-----------------------------------------------------------------------------	
// Get the default position attribute
//-----------------------------------------------------------------------------
const CDmAttribute *CDmeTransformControl::GetDefaultOrientationAttr() const
{
	return GetAttribute( DEFAULT_ORIENTATION_ATTR, AT_QUATERNION );
}


//-----------------------------------------------------------------------------
// Get the position channel targeting the control
//-----------------------------------------------------------------------------
CDmeChannel *CDmeTransformControl::GetPositionChannel() const
{
	return m_PositionChannel;
}


//-----------------------------------------------------------------------------
// Get the orientation channel targeting the control 
//-----------------------------------------------------------------------------
CDmeChannel *CDmeTransformControl::GetOrientationChannel() const
{
	return m_OrientationChannel;
}


//-----------------------------------------------------------------------------
// Set the position value of the control
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetPosition( const Vector &position )
{
	m_PositionValue = position;
}


//-----------------------------------------------------------------------------
// Set the orientation value of the control
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetOrientation( const Quaternion &orientation )
{
	m_OrientationValue = orientation;
}


//-----------------------------------------------------------------------------
// Set the default position of the control
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetDefaultPosition( const Vector &position )
{
	CDmAttribute *pAttr = SetValue< Vector >( DEFAULT_POSITION_ATTR, position );
	if ( pAttr )
	{
		pAttr->AddFlag( FATTRIB_DONTSAVE );
	}
}


//-----------------------------------------------------------------------------
// Set the default orientation of the control 
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetDefaultOrientation( const Quaternion &orientation )
{
	CDmAttribute *pAttr = SetValue< Quaternion >( DEFAULT_ORIENTATION_ATTR, orientation );
	if ( pAttr )
	{
		pAttr->AddFlag( FATTRIB_DONTSAVE );
	}
}
	

//-----------------------------------------------------------------------------
// Set the position channel that is targeting the control
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetPositionChannel( CDmeChannel *pChannel )
{
	m_PositionChannel = pChannel;
}


//-----------------------------------------------------------------------------
// Get the orientation channel that is targeting the control
//-----------------------------------------------------------------------------
void CDmeTransformControl::SetOrientationChannel( CDmeChannel *pChannel )
{
	m_OrientationChannel = pChannel;
}


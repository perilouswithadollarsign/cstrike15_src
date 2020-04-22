//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Declaration of the CDmeTransform control class, a helper for modifying a
// transform.
//
//=============================================================================
#ifndef DMETRANSFORMCONTROL_H
#define DMETRANSFORMCONTROL_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeChannel;
class CDmeTransform;
class CDmeDag;


//-----------------------------------------------------------------------------
// The CDmeTransformControl class is a helper for modifying a transform, it is
// attached to a control CDmElement which has the transform bool set to true. 
// It stores information about how the a transform is to be modified, such as
// the relative pivot offset, manipulation transform, and rotation delta 
// information. The CDmeTransformControl class may be 
//-----------------------------------------------------------------------------
class CDmeTransformControl : public CDmElement
{
	DEFINE_ELEMENT( CDmeTransformControl, CDmElement );


public:

	// Get the manipulation transform matrix
	void GetManipulationTransform( matrix3x4_t &transform ) const;

	// Set the manipulation transform matrix
	void SetManipulationTransform( const matrix3x4_t &transform );
	
	// Get the manipulation rotation amount in the local space of the transform
	void GetManipulationRotationLocal( Quaternion &rotation ) const;

	// Set the manipulation rotation amount in the local space of the transform
	void SetManipulationRotationLocal( const Quaternion &rotation );

	// Get the manipulation rotation amount in the parent space of the transform
	void GetManipulationRotationParent( Quaternion &rotation ) const;

	// Set the manipulation rotation amount in the parent space of the transform
	void SetManipulationRotationParent( const Quaternion &rotation );

	// Get the manipulation pivot position, this may differ from the pivot offset
	void GetManipulationPivot( Vector &pivotPosition ) const;

	// Set the manipulation pivot position
	void SetManipulationPivot( const Vector &pivotPosition );
	
	// Get the current local space pivot offset
	const Vector &GetPivotOffset() const;
	
	// Set the current local space pivot offset
	void SetPivotOffset( const Vector &localOffset );

	// Get the transform associated with the transform control
	CDmeTransform *GetTransform() const;

	// Get the dag node associated with the transform control
	CDmeDag *GetDag() const;

	// Get the position attribute of the control
	CDmAttribute *GetPositionAttr();

	// Get the orientation attribute of the control
	CDmAttribute *GetOrientationAttr();

	// Get the position value of the control
	const Vector &GetPosition() const;

	// Get the orientation value of the control
	const Quaternion &GetOrientation() const;

	// Determine if the control currently has a default position set
	bool HasDefaultPosition() const;

	// Determine if the control currently has a default orientation set
	bool HasDefaultOrientation() const;

	// Get the default position of the control
	const Vector &GetDefaultPosition() const;
	
	// Get the default orientation of the control
	const Quaternion &GetDefaultOrientation() const;

	// Get the default position attribute
	const CDmAttribute *GetDefaultPositionAttr() const;
	
	// Get the default position attribute
	const CDmAttribute *GetDefaultOrientationAttr() const;

	// Get the position channel targeting the control
	CDmeChannel *GetPositionChannel() const;
	
	// Get the orientation channel targeting the control 
	CDmeChannel *GetOrientationChannel() const;

	// Set the position value of the control
	void SetPosition( const Vector &position );

	// Set the orientation value of the control
	void SetOrientation( const Quaternion &orientation );
		
	// Set the default position of the control
	void SetDefaultPosition( const Vector &position );

	// Set the default orientation of the control 
	void SetDefaultOrientation( const Quaternion &orientation );
	
	// Set the position channel that is targeting the control
	void SetPositionChannel( CDmeChannel *pChannel );

	// Get the orientation channel that is targeting the control
	void SetOrientationChannel( CDmeChannel *pChannel );






private:

	// Allocate and initialize the manipulation parameters
	bool InitManipulationParams();


	struct ManipulationParams_t
	{
		matrix3x4_t Transform;		// Transform used to apply the manipulation
		Quaternion	RotationLocal;	// Rotation applied in manipulation ( Local space )
		Quaternion  RotationParent;	// Rotation applied in manipulation ( Parent space )
		Vector		Pivot;			// Pivot used in manipulation
	};

	Vector						m_PivotOffset;			// Current pivot offset to be used when manipulating the transform
	ManipulationParams_t		*m_pManipulationParams;	// Parameters describing the last manipulation performed on the transform

	CDmaVar< Vector >			m_PositionValue;		// "valuePosition": current position value of the control
	CDmaVar< Quaternion	>		m_OrientationValue;		// "valueOrientation": current orientation value of the control
	CDmaElement< CDmeChannel >	m_PositionChannel;		// "positionChannel":  channel which connects the position value to the transform
	CDmaElement< CDmeChannel >	m_OrientationChannel;	// "orientationChannel": channel which connects the orientation value to the transform
	

};


#endif // DMETRANSFORMCONTROL_H
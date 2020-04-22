//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Purpose: Contains the declaration of the CDmeRig, a class which groups a set
// of associated constraints and operators together, allowing operations to be
// performed on the group of elements. Also contains the declaration of 
// CDmeRigAnimSetElements, a helper class used to store a list of elements which
// are all associated with a single animation set.
//
//=============================================================================

#ifndef DMERIG_H
#define DMERIG_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattributevar.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmecontrolgroup.h"
#include "movieobjects/dmeanimationset.h"

// Forward declarations
class CDmeAnimationSet;
class CDmeFilmClip;


//-------------------------------------------------------------------------------------------------
// CDmeRigAnimSetElements: A helper class used by CDmeRig to store a list of elements associated 
// with a particular animation set.
//-------------------------------------------------------------------------------------------------
class CDmeRigAnimSetElements : public CDmElement
{
	DEFINE_ELEMENT( CDmeRigAnimSetElements, CDmElement );

public:
	
	// Set the animation set elements in the list are to be associated with, only allowed when empty.
	void SetAnimationSet( CDmeAnimationSet* pAnimationSet );

	// Add an element to the list
	void AddElement( CDmElement *pElement );

	// Remove the specified element from the list
	bool RemoveElement( CDmElement *pElement );

	// Remove all of the elements from the list
	void RemoveAll();

	// Add all of the elements to the provided array
	void GetElements( CUtlVector< CDmElement* > &elementList ) const;
	
	// Add a control group to the list of hidden control groups
	void AddHiddenControlGroup( CDmeControlGroup *pControlGroup );

	// Accessors
	CDmeAnimationSet *AnimationSet() const						{ return m_AnimationSet; }
	int NumElements() const										{ return m_ElementList.Count(); }
	const CDmaElementArray< CDmElement > &Elements() const		{ return m_ElementList; }
	const CDmaStringArray &HiddenControlGroups() const			{ return m_HiddenGroups; }

private:

	CDmaElement< CDmeAnimationSet >	m_AnimationSet;	// "animationSet" : Animation set to which the elements belong
	CDmaElementArray< CDmElement >	m_ElementList;	// "elementList"  : List of elements assigned to the group
	CDmaStringArray					m_HiddenGroups;	// "hiddenGroups" : List of names of the groups for which the rig disabled visibility 
};



//-------------------------------------------------------------------------------------------------
// CDmeRig: The CDmeRig class represents a grouping of rig constraints, operators, controls and 
// other elements which are conceptually a single rig, operating on one or more animation sets.
// By maintaining this grouping it is possible to perform operations such as detach on all of the
// elements associated with rig, even if the rig has elements in multiple animation sets and those
// animation sets have elements from more than one rig. The CDmeRig is no actually required for 
// operation of the elements composing the rig, it is merely utility for managing the elements 
// associated with a single rig.
//-------------------------------------------------------------------------------------------------
class CDmeRig : public CDmeDag
{
	DEFINE_ELEMENT( CDmeRig, CDmeDag );

public:

	// Add an element to the rig
	void AddElement( CDmElement* pElement, CDmeAnimationSet *pAnimationSet );

	// Set the state of the specified control group and add it to list of control group modified by the rig
	void HideControlGroup( CDmeControlGroup *pGroup );

	// Remove an element from the rig
	void RemoveElement( CDmElement *pElement, CDmeAnimationSet *pAnimationSet );

	// Remove an animation set and all associated elements from the rig
	void RemoveAnimationSet( CDmeAnimationSet *pAnimationSet );

	// Determine if the rig has any animation sets associated with it
	bool HasAnyAnimationSets() const;

	// Get the list of animation sets in the rig
	void GetAnimationSets( CUtlVector< CDmeAnimationSet* > &animationSetList ) const;

	// Get the list of elements for the specified animation set
	void GetAnimationSetElements( const CDmeAnimationSet *pAnimationSet, CUtlVector< CDmElement* > &elementList ) const;

	// Determine if the rig has any elements from the specified animation set
	bool HasAnimationSet( const CDmeAnimationSet *pAnimationSet ) const;

	// Build a list of all of the dag nodes which are influenced by rig
	void FindInfluencedDags( CUtlVector< CDmeDag* > &dagList ) const;

	// Remove all of elements in the rig from scene
	void RemoveElementsFromShot( CDmeFilmClip *pShot );

	// Hide all of the control groups in the rig's list of hidden control groups
	void HideHiddenControlGroups( CDmeAnimationSet *pAnimationSet );

	// Remove the specified element from any rig which it may be associated with.
	static void RemoveElementFromRig( CDmElement *pElement );


private:

	// Find the element list for the specified animation set
	int FindAnimSetElementList( const CDmeAnimationSet *pAnimationSet ) const;

	// Find the element list for the specified animation set or create one
	CDmeRigAnimSetElements *FindOrCreateAnimSetElementList( CDmeAnimationSet *pAnimationSet );

	// Set the visibility of the control groups in the hidden list
	void SetHiddenControlGroupVisibility( CDmeRigAnimSetElements *pAnimSetElement, bool bHidden );

	CDmaElementArray< CDmeRigAnimSetElements >	m_AnimSetList;			// "animSetList" : Array of animation set / element groupings

};

void CollectRigsOnAnimationSet( CDmeAnimationSet *pAnimSet, CUtlVector< CDmeRig* > &rigList );

#endif // DMERIGELEMENTGROUP_H


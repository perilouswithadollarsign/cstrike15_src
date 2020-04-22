//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: Contains the declaration of the CDmeControlGroup class. The
// CDmeControlGroup class is a grouping of animation controls that is used for
// selection.
//
//=============================================================================

#ifndef DMECONTROLGROUP_H
#define DMECONTROLGROUP_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattributevar.h"

class CDmeTransform;
class CDmeAnimationSet;

//-----------------------------------------------------------------------------
// CDmeControlGroup: A class representing a grouping of controls used for 
// selection. Contains a list of controls as well as a list of child groups
// which are used to for a group hierarchy. 
//-----------------------------------------------------------------------------
class CDmeControlGroup : public CDmElement
{
	DEFINE_ELEMENT( CDmeControlGroup, CDmElement );

public:


	//---------------------------------
	// Control functions 
	//---------------------------------

	// Add a control to the group
	void AddControl( CDmElement *pControl, const CDmElement *pInsertBeforeControl = NULL );

	// Remove a control from the group
	bool RemoveControl( const CDmElement *pControl );
	
	// Get a flat list of all of the controls in the group 
	void GetControlsInGroup( CUtlVector< CDmElement* > &controlList, bool recursive ) const;

	// Find a control with the specified name within the group
	CDmElement *FindControlByName( const char *pchName, bool recursive, CDmeControlGroup **pParentGroup = NULL );

	// Find the group to which the specified control belongs, if any.
	static CDmeControlGroup *FindGroupContainingControl( const CDmElement* pControl );


	//---------------------------------
	// Group functions 
	//---------------------------------
	
	// Make the specified group a child of this group 
	void AddChild( CDmeControlGroup *pGroup, const CDmeControlGroup *pInsertBeforeGroup = NULL );

	// Remove the specified child group 
	bool RemoveChild( const CDmeControlGroup *pGroup );

	// Move the specified child group to the top of the list
	void MoveChildToTop( const CDmeControlGroup *pGroup );

	// Move the specified child group to the bottom of the list
	void MoveChildToBottom( const CDmeControlGroup *pGroup );

	// Sore the children by name
	void SortChildrenByName( bool bAscending );

	// Determine if the group has child of the specified name
	bool HasChildGroup( const char *pchName, bool recursive );

	// Find the child group with the specified name
	CDmeControlGroup *FindChildByName( const char *pchName, bool recursive, CDmeControlGroup **pParentGroup = NULL );

	// Find the parent of the group
	CDmeControlGroup *FindParent() const;

	// Determine if this group is an ancestor of the specified group
	bool IsAncestorOfGroup( const CDmeControlGroup *pGroup ) const;

	// Create a control group with the provided name and make it a child of this group
	CDmeControlGroup *CreateControlGroup( const char *pchName );

	// Get a flat list of all of the groups in sub-tree of the group
	void GetAllChildren( CUtlVector< DmElementHandle_t > &childGroupList ) const;

	// Destroy all of the empty children of the group, will not destroy this group even it is empty.
	void DestroyEmptyChildren();

	// Destroy the control group, moving all of its children and controls into this node
	static void DestroyGroup( CDmeControlGroup *pGroup, CDmeControlGroup *pRecipient, bool recursive );

	// Remove all of the children and controls from the group
	void RemoveAllChildrenAndControls();

	// Set the color of the group
	void SetGroupColor( const Color &groupColor, bool recursive );

	// Set the color to be used on the controls of the group
	void SetControlColor( const Color &controlColor, bool recursive );

	// Set the visible state of the group
	void SetVisible( bool bVisible );

	// Enable or disable selection of the controls
	void SetSelectable( bool bEnable );

	// Enable or disable control snapping
	void SetSnappable( bool bEnable );
		
	// Determine if there are any controls or children in the group
	bool IsEmpty() const;

	// Is the group visible, will return false if any parent of the group is not visible
	bool IsVisible() const;

	// Can controls in the group be selected in the viewport, check includes parents
	bool IsSelectable() const;

	// Can controls in the group be snapped to in the viewport, check includes parents
	bool IsSnappable() const;

	// Find the animation set associated with the control group
	CDmeAnimationSet *FindAnimationSet( bool bSearchAncestors ) const;

	// Find the shared ancestor between this control group and the specified control group
	CDmeControlGroup *FindCommonAncestor( CDmeControlGroup *pControlGroup );

	// Find the root control group which this control group is in the sub tree of.
	CDmeControlGroup *FindRootControlGroup();

	// Compare the two groups by name for an ascending sort
	static int CDmeControlGroup::CompareByNameAscending( CDmeControlGroup * const *pGroupA, CDmeControlGroup * const *pGroupB );

	// Compare the two groups by name for a descending sort
	static int CDmeControlGroup::CompareByNameDecending( CDmeControlGroup * const *pGroupA, CDmeControlGroup * const *pGroupB );

	// Accessors
	const CDmaElementArray< CDmeControlGroup > &Children() const	{ return m_Children;		}
	const CDmaElementArray< CDmElement > &Controls() const			{ return m_Controls;		}
	const Color &GroupColor() const									{ return m_GroupColor;		}
	const Color &ControlColor() const								{ return m_ControlColor;	}
	bool Visible() const											{ return m_Visible;			}
	bool Selectable() const											{ return m_Selectable;		}
	bool Snappable() const											{ return m_Snappable;		}

private:
	
	// Recursively destroy the children of the specified group which have no controls or sub groups
	static bool DestroyEmptyChildren_R( CDmeControlGroup *pGroup );

	// Recursively destroy the child groups of the specified group and and the controls to the specified recipient group
	static void DestroyGroup_R( CDmeControlGroup *pGroup, CDmeControlGroup *pRecipient, bool recursive );

	// Build a list of the control group that form the path to the root of the tree to which the control group belongs
	void BuildPathFromRoot( CUtlVector< CDmeControlGroup * > &path );

	CDmaElementArray< CDmeControlGroup >	m_Children;		// "children"		: Child groups of this control group
	CDmaElementArray< CDmElement >			m_Controls;		// "controls"		: Controls within the group
	CDmaVar< Color >						m_GroupColor;	// "groupColor"		: Color which the group is to be displayed with
	CDmaVar< Color >						m_ControlColor;	// "controlColor"	: Color in which the controls of the group are to be displayed 
	CDmaVar< bool >							m_Visible;		// "visible"		: Is the group visible in animation set editor
	CDmaVar< bool >							m_Selectable;	// "selectable"		: Can the group be selected
	CDmaVar< bool >							m_Snappable;	// "snapping"		: Can controls in the group be snapped to 

};


#endif // DMECONTROLGROUP_H


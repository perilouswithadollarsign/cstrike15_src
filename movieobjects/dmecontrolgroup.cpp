//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: Implementation of the CDmeControlGroup class. The CDmeControlGroup 
// class provides hierarchical grouping of animation controls and used for 
// selection of the animation set controls.
//
//=============================================================================
#include "movieobjects/dmecontrolgroup.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects/dmetransformcontrol.h"
#include "movieobjects/dmeanimationset.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-------------------------------------------------------------------------------------------------
// Expose this class to the scene database 
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeControlGroup, CDmeControlGroup );


//-------------------------------------------------------------------------------------------------
// Purpose: Provide post construction processing.
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::OnConstruction()
{
	m_Children.Init( this, "children" );
	m_Controls.Init( this, "controls" );
	m_GroupColor.InitAndSet( this, "groupColor", Color( 200, 200, 200, 255 ) );
	m_ControlColor.InitAndSet( this, "controlColor", Color( 200, 200, 200, 255 ) );
	m_Visible.InitAndSet( this, "visible", true );
	m_Selectable.InitAndSet( this, "selectable", true );
	m_Snappable.InitAndSet( this, "snappable", true );	
}


//-------------------------------------------------------------------------------------------------
// Purpose: Provide processing and cleanup before shutdown
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::OnDestruction()
{

}


//-------------------------------------------------------------------------------------------------
// Purpose: Add a the provided control to the group, if the control is currently in another group 
// it will be removed from the other group first.
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::AddControl( CDmElement *pControl, const CDmElement *pInsertBeforeControl )
{
	if ( pControl == NULL )
		return;

	// Remove the control from any group it is currently in.
	CDmeControlGroup *pCurrentGroup = FindGroupContainingControl( pControl );
	if ( pCurrentGroup )
	{
		pCurrentGroup->RemoveControl( pControl );
	}


	// If a insert location control was specified find it in the list of controls
	int nInsertLocation = m_Controls.InvalidIndex();
	if ( pInsertBeforeControl )
	{
		nInsertLocation = m_Controls.Find( pInsertBeforeControl );
	}

	// Add the control to the group
	if ( nInsertLocation != m_Controls.InvalidIndex() )
	{
		m_Controls.InsertBefore( nInsertLocation, pControl );
	}
	else
	{
		m_Controls.AddToTail( pControl );
	}

}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove a control from the group. This will only search the immediate group for the 
// specified control and remove it. It will not remove the control if it is in a child of this 
// group. Returns false if the control was not found. 
//-------------------------------------------------------------------------------------------------
bool CDmeControlGroup::RemoveControl( const CDmElement *pControl )
{
	if ( pControl == NULL )
		return false;

	int nControls = m_Controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		if ( pControl == m_Controls[ iControl ] )
		{
			m_Controls.Remove( iControl );
			return true;
		}
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get a flat list of all of the controls in the group. If the recursive flag is true 
// a flat list of all of the controls in the entire sub-tree of the group will be returned. If
// the recursive flag is false on 
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::GetControlsInGroup( CUtlVector< CDmElement* > &controlList, bool recursive ) const
{
	// If the recursive flag is set add all of the controls
	// of the entire tree of each child group within the group.
	if ( recursive )
	{
		int nChildren = m_Children.Count();
		for ( int iChild = 0; iChild < nChildren; ++iChild )
		{
			CDmeControlGroup *pChild = m_Children[ iChild ];
			if ( pChild )
			{
				pChild->GetControlsInGroup( controlList, true );
			}
		}
	}	

	// Add the controls from this group.
	int nControls = m_Controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		CDmElement *pControl = m_Controls[ iControl ];
		if ( pControl )
		{
			controlList.AddToTail( pControl );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find a control with the specified name within the group. If the recursive flag is true
// the entire sub-tree of the group will be searched, otherwise only the immediate control will 
// be searched for the group. If the parent group pointer is provided it will be returned with the 
// group to which the control belongs directly.
//-------------------------------------------------------------------------------------------------
CDmElement *CDmeControlGroup::FindControlByName( const char *pchName, bool recursive, CDmeControlGroup **pParentGroup )
{
	// Search the controls contained directly by the group for one with the specified name.
	int nControls = m_Controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		CDmElement *pControl = m_Controls[ iControl ];
		if ( pControl )
		{
			if ( V_stricmp( pControl->GetName(), pchName )	== 0 )
			{
				if ( pParentGroup )
				{
					*pParentGroup = this;
				}

				return pControl;
			}
		}
	}

	// If the control was not found in the controls contained directly by the group
	// search the children and their sub-trees if the recursive flag is true.
	if ( recursive )
	{
		int nChildren = m_Children.Count();
		for ( int iChild = 0; iChild < nChildren; ++iChild )
		{
			CDmeControlGroup *pChild = m_Children[ iChild ];
			if ( pChild )
			{
				CDmElement *pControl = pChild->FindControlByName( pchName, true, pParentGroup );
				if ( pControl )
					return pControl;
			}
		}
	}

	return NULL;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find the group to which the specified control belongs, if any. This function searches
// for any control groups which reference the specified control. It simply returns the first one
// it finds, as a control should only every belong to a single control group.
//-------------------------------------------------------------------------------------------------
CDmeControlGroup *CDmeControlGroup::FindGroupContainingControl( const CDmElement* pControl )
{
	return FindReferringElement< CDmeControlGroup >( pControl, "controls" );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Make the specified group a child of this group. The group will be removed from the 
// child list of any other group to which it may currently belong.
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::AddChild( CDmeControlGroup *pGroup, const CDmeControlGroup *pInsertBeforeGroup )
{
	// Can't make a group its own child
	Assert( pGroup != this );
	if ( pGroup == this )
		return;

	// Remove the group from its current control group if it belongs one.
	CDmeControlGroup *pParentGroup = pGroup->FindParent();
	if ( pParentGroup )
	{
		pParentGroup->RemoveChild( pGroup );
	}

	// If a insert location group was specified find it in the list of children
	int nInsertLocation = m_Children.InvalidIndex();
	if ( pInsertBeforeGroup )
	{
		nInsertLocation = m_Children.Find( pInsertBeforeGroup );
	}

	// Add the specified group as child of this group.
	if ( nInsertLocation != m_Children.InvalidIndex() )
	{
		m_Children.InsertBefore( nInsertLocation, pGroup );
	}
	else
	{
		m_Children.AddToTail( pGroup );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove the specified child group. Searches the immediate children of the node for the 
// specified group and removes it from the child list if the group is found. Returns true if the 
// group is found, false if the group is not found.
//-------------------------------------------------------------------------------------------------
bool CDmeControlGroup::RemoveChild( const CDmeControlGroup *pGroup )
{	
	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		if ( m_Children[ iChild ] == pGroup )
		{
			m_Children.Remove( iChild );
			return true;
		}
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Move the specified child group to the top of the list
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::MoveChildToTop( const CDmeControlGroup *pGroup )
{
	// Make sure the group is actually a child, and move it
	// to the top of the list if it is not already there.
	int nChildren = m_Children.Count();
	for ( int iChild = 1; iChild < nChildren; ++iChild )
	{
		if ( m_Children[ iChild ] == pGroup )
		{			
			CDmeControlGroup *pChild = m_Children[ iChild ];
			m_Children.Remove( iChild );
			m_Children.InsertBefore( 0, pChild );
			break;
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Move the specified child group to the bottom of the list
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::MoveChildToBottom( const CDmeControlGroup *pGroup )
{
	// Make sure the group is actually a child, and move it
	// to the bottom of the list if it is not already there.
	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < (nChildren - 1); ++iChild )
	{
		if ( m_Children[ iChild ] == pGroup )
		{		
			CDmeControlGroup *pChild = m_Children[ iChild ];
			m_Children.Remove( iChild );
			m_Children.AddToTail( pChild );
			break;	
		}
	}
}


//-----------------------------------------------------------------------------
// Compare the two groups by name for an ascending sort
//-----------------------------------------------------------------------------
int CDmeControlGroup::CompareByNameAscending( CDmeControlGroup * const *pGroupA, CDmeControlGroup * const *pGroupB )
{
	return V_stricmp( (*pGroupA)->GetName(), (*pGroupB)->GetName() );
}


//-----------------------------------------------------------------------------
// Compare the two groups by name for a descending sort
//-----------------------------------------------------------------------------
int CDmeControlGroup::CompareByNameDecending( CDmeControlGroup * const *pGroupA, CDmeControlGroup * const *pGroupB )
{
	return V_stricmp( (*pGroupB)->GetName(), (*pGroupA)->GetName() );
}


//-------------------------------------------------------------------------------------------------
// Sore the children by name
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::SortChildrenByName( bool bAscending )
{
	// Copy the children into a temporary array to be sorted.
	int nNumChildren = m_Children.Count();
	CUtlVector< CDmeControlGroup * > sortedList( 0, nNumChildren );
	
	for ( int iChild = 0; iChild < nNumChildren; ++iChild )
	{
		CDmeControlGroup *pGroup = m_Children[ iChild ];
		if ( pGroup )
		{
			sortedList.AddToTail( pGroup );
		}
	}

	// Sort the temporary array in ascending or descending order
	if ( bAscending )
	{
		sortedList.Sort( CompareByNameAscending );
	}
	else
	{
		sortedList.Sort( CompareByNameDecending );
	}

	// Remove all of the children from the original list and then add them back in sorted order
	m_Children.RemoveAll();
	int nNumSorted = sortedList.Count();
	for ( int iChild = 0; iChild < nNumSorted; ++iChild )
	{
		CDmeControlGroup *pGroup = sortedList[ iChild ];
		if ( pGroup )
		{
			m_Children.AddToTail( pGroup );
		}
	}

}


//-------------------------------------------------------------------------------------------------
// Determine if the group has child of the specified name
//-------------------------------------------------------------------------------------------------
bool CDmeControlGroup::HasChildGroup( const char *pchName, bool recursive )
{
	if ( FindChildByName( pchName, recursive ) == NULL )
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------------
// Purpose: Find the child group with the specified name. If the recursive flag is true the entire
// sub-tree of the group will be searched, otherwise only the immediate children of the group will
// be searched for the specified child. If a parent group pointer is provided it will be returned
// with the immediate parent in which the child was located.
//-------------------------------------------------------------------------------------------------
CDmeControlGroup *CDmeControlGroup::FindChildByName( const char *pchName, bool recursive, CDmeControlGroup **pParentGroup )
{	
	// Search the immediate children for a group with the specified name.
	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		CDmeControlGroup *pChild = m_Children[ iChild ];
		if ( pChild )
		{
			if ( V_stricmp( pChild->GetName(), pchName ) == 0 )
			{
				if ( pParentGroup )
				{
					*pParentGroup = this;
				}
				return pChild;
			}
		}
	}

	// If the group was not found in the immediate children of the current group and the recursive
	// flag is set, search the sub-trees of all the children for the specified group.
	if ( recursive )
	{
		for ( int iChild = 0; iChild < nChildren; ++iChild )
		{
			CDmeControlGroup *pChild = m_Children[ iChild ];
			if ( pChild )
			{
				CDmeControlGroup *pGroup = pChild->FindChildByName( pchName, true, pParentGroup );
				if ( pGroup )
					return pGroup;
			}
		}
	}

	return NULL;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find the parent of the group. Searches for groups which reference this group as a 
// child. Each group is allowed to be the child of only one group, so the first group found is 
// returned.
//-------------------------------------------------------------------------------------------------
CDmeControlGroup *CDmeControlGroup::FindParent() const
{
	const static CUtlSymbolLarge symChildren = g_pDataModel->GetSymbol( "children" );
	CDmeControlGroup *pParent = FindReferringElement< CDmeControlGroup >( this, symChildren );
	return pParent;
}


//-------------------------------------------------------------------------------------------------
// Determine if this group is an ancestor of the specified group
//-------------------------------------------------------------------------------------------------
bool CDmeControlGroup::IsAncestorOfGroup( const CDmeControlGroup *pGroup ) const
{
	if ( pGroup == NULL )
		return false;

	const CDmeControlGroup *pCurrentGroup = pGroup;
	const CDmeControlGroup *pParent = pGroup->FindParent();

	while ( pParent )
	{
		if ( pParent == this )
			return true;

		pCurrentGroup = pParent;
		pParent = pParent->FindParent();
		Assert( pCurrentGroup != pParent );
		if ( pCurrentGroup == pParent )
			break;
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
// Create a control group with the provided name and add it to the specified parent. If a child of 
// the specified name already exists it will be returned and no new group will be created.
//-------------------------------------------------------------------------------------------------
CDmeControlGroup *CDmeControlGroup::CreateControlGroup( const char *pchName )
{
	CDmeControlGroup *pExistingGroup = FindChildByName( pchName, false );
	if ( pExistingGroup )
		return pExistingGroup;

	// Create the new control group with the specified name
	CDmeControlGroup *pNewGroup = CreateElement< CDmeControlGroup >( pchName, GetFileId() );

	// Add the group to as a child of this group
	AddChild( pNewGroup );

	return pNewGroup;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get a flat list of all of the groups in sub-tree of the group
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::GetAllChildren( CUtlVector< DmElementHandle_t > &childGroupList ) const
{
	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		CDmeControlGroup *pChild = m_Children[ iChild ];
		if ( pChild )
		{
			childGroupList.AddToTail( pChild->GetHandle() );
			pChild->GetAllChildren( childGroupList );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Recursively destroy the children of the specified group which have no controls or sub groups
//-------------------------------------------------------------------------------------------------
bool CDmeControlGroup::DestroyEmptyChildren_R( CDmeControlGroup *pGroup )
{
	int nNumChildren = pGroup->m_Children.Count();
	
	// Build a list of the children which are empty and should be destroyed. This 
	// process will recursively remove empty children of the children so that if 
	// a child has only empty sub-children then it will still be removed.
	CUtlVector< CDmeControlGroup * > childrenToDestroy( 0, nNumChildren );
	for ( int iChild = 0; iChild < nNumChildren; ++iChild )
	{
		CDmeControlGroup *pChild = pGroup->m_Children[ iChild ]; 
		if ( pChild )
		{
			if ( DestroyEmptyChildren_R( pChild ) )
			{	
				childrenToDestroy.AddToTail( pChild );
			}
		}
	}

	// Destroy the empty children
	int nNumToDestroy = childrenToDestroy.Count();
	for ( int iChild = 0; iChild < nNumToDestroy; ++iChild )
	{
		CDmeControlGroup *pChild = childrenToDestroy[ iChild ];
		pGroup->RemoveChild( pChild );
	}

	// If this node is now empty return true indicating that it may be destroyed
	return ( ( pGroup->m_Children.Count() == 0 ) && ( pGroup->m_Controls.Count() == 0 ) );
}


//-------------------------------------------------------------------------------------------------
// Destroy all of the empty children of the group, will not destroy this group even it is empty.
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::DestroyEmptyChildren()
{
	DestroyEmptyChildren_R( this );
}


//-------------------------------------------------------------------------------------------------
// Purpose:  Destroy the control group, moving all of its children and controls into this node
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::DestroyGroup( CDmeControlGroup *pGroup, CDmeControlGroup *pRecipient, bool recursive )
{
	if ( pGroup == NULL  )
		return;

	// Remove the group from its parent
	CDmeControlGroup *pParent = pGroup->FindParent();
	if ( pParent )
	{
		pParent->RemoveChild( pGroup );
		if ( pRecipient == NULL )
		{
			pRecipient = pParent;
		}
	}

	// Destroy the group and all of its children if specified
	DestroyGroup_R( pGroup, pRecipient, recursive );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Recursively destroy the child groups of the specified group and and the controls to the 
// specified recipient group
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::DestroyGroup_R( CDmeControlGroup *pGroup, CDmeControlGroup *pRecipient, bool recursive )
{	
	if ( pGroup == NULL )
		return;

	// If the group is not empty there must be a recipient to receive its controls and groups
	if ( pRecipient == NULL && !pGroup->IsEmpty() )
	{
		Assert( pGroup->IsEmpty() || pRecipient );
		return;
	}

	// Iterate through the children, if recursive destroy the
	// children otherwise copy the children to the recipient.
	int nChildren = pGroup->m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		CDmeControlGroup *pChild = pGroup->m_Children[ iChild ];
		if ( pChild )
		{
			if ( recursive )
			{
				DestroyGroup_R( pChild, pRecipient, true );
			}
			else
			{
				pRecipient->m_Children.AddToTail( pChild );
			}
		}
	}
	
	// Copy all the controls of the node into the recipient
	int nControls = pGroup->m_Controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		CDmElement *pControl = pGroup->m_Controls[ iControl ];
		pRecipient->m_Controls.AddToTail( pControl );
	}

	// Destroy the group
	DestroyElement( pGroup );
}


//-------------------------------------------------------------------------------------------------
// Remove all of the children and controls from the group
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::RemoveAllChildrenAndControls()
{
	m_Children.RemoveAll();
	m_Controls.RemoveAll();
}


//-------------------------------------------------------------------------------------------------
// Purpose: Set the color of the group, this is the color that is used when displaying the group in
// the user interface.
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::SetGroupColor( const Color &groupColor, bool bRecursive )
{
	m_GroupColor = groupColor;
	
	if ( !bRecursive )
		return;
	
	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		if ( m_Children[ iChild ] )
		{
			m_Children[ iChild ]->SetGroupColor( groupColor, true );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Set the color to be used on the controls of the group
//-------------------------------------------------------------------------------------------------
void CDmeControlGroup::SetControlColor( const Color &controlColor, bool bRecursive )
{
	m_ControlColor = controlColor;

	if ( !bRecursive )
		return;

	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		if ( m_Children[ iChild ] )
		{
			m_Children[ iChild ]->SetControlColor( controlColor, true );
		}
	}
}


//-----------------------------------------------------------------------------
// Set the visible state of the group
//-----------------------------------------------------------------------------
void CDmeControlGroup::SetVisible( bool bVisible )
{
	m_Visible = bVisible;
}


//-----------------------------------------------------------------------------
// Enable or disable selection of the controls
//-----------------------------------------------------------------------------
void CDmeControlGroup::SetSelectable( bool bSelectable )
{
	m_Selectable = bSelectable;
}


//-----------------------------------------------------------------------------
// Enable or disable control snapping
//-----------------------------------------------------------------------------
void CDmeControlGroup::SetSnappable( bool bSnappable )
{
	m_Snappable = bSnappable;
}

		
//-----------------------------------------------------------------------------
// Purpose: Determine if there are any controls or children in the group
//-----------------------------------------------------------------------------
bool CDmeControlGroup::IsEmpty() const
{
	if ( m_Controls.Count() > 0 ) return false;
	if ( m_Children.Count() > 0 ) return false;
	return true;
}


//-----------------------------------------------------------------------------
// Is the group visible 
//-----------------------------------------------------------------------------
bool CDmeControlGroup::IsVisible() const
{
	CDmeControlGroup *pParent = FindParent();
	if ( pParent && !pParent->IsVisible() )
		return false;
	
	return m_Visible;
}


//-----------------------------------------------------------------------------
// Can controls in the group be selected in the viewport
//-----------------------------------------------------------------------------
bool CDmeControlGroup::IsSelectable() const
{
	CDmeControlGroup *pParent = FindParent();
	if ( pParent && !pParent->IsSelectable() )
		return false;

	return m_Selectable;
}


//-----------------------------------------------------------------------------
// Can controls in the group be snapped to in the viewport
//-----------------------------------------------------------------------------
bool CDmeControlGroup::IsSnappable() const
{
	CDmeControlGroup *pParent = FindParent();
	if ( pParent && !pParent->IsSnappable() )
		return false;

	return m_Snappable;
}


//-----------------------------------------------------------------------------
// Find the shared ancestor between this control group and the specified control 
// group. Will return NULL if groups are not in the same tree and do not share a 
// common ancestor. If one group is an ancestor of the other group then that 
// group will be returned, so result may be one of the nodes which is not 
// technically an ancestor of that node.
//-----------------------------------------------------------------------------
CDmeControlGroup *CDmeControlGroup::FindCommonAncestor( CDmeControlGroup *pControlGroupB )
{	
	CDmeControlGroup *pControlGroupA = this;

	// If the specified group is this group then 
	// the common ancestor is the group itself.
	if ( pControlGroupA == pControlGroupB )
		return pControlGroupA;

	// Build the path from each group to the root
	CUtlVector< CDmeControlGroup * > pathToGroupA;
	CUtlVector< CDmeControlGroup * > pathToGroupB;
	pControlGroupA->BuildPathFromRoot( pathToGroupA );
	pControlGroupB->BuildPathFromRoot( pathToGroupB );

	// Now walk each of the the paths until they diverge
	CDmeControlGroup *pCommonGroup = NULL;
	int nNumSteps = MIN( pathToGroupA.Count(), pathToGroupB.Count() );

	int iStep = 0;
	while ( iStep < nNumSteps )
	{
		if ( pathToGroupA[ iStep ] != pathToGroupB[ iStep ] )
			break;

		pCommonGroup = pathToGroupA[ iStep ];
		++iStep;
	}	

	return pCommonGroup;
}


//-----------------------------------------------------------------------------
// Find the root control group which this control group is in the sub tree of.
//-----------------------------------------------------------------------------
CDmeControlGroup *CDmeControlGroup::FindRootControlGroup()
{
	CDmeControlGroup *pCurrent = this;
	CDmeControlGroup *pParent = pCurrent->FindParent();

	while ( pParent )
	{		
		pCurrent = pParent;
		pParent = pParent->FindParent();
	}

	return pCurrent;
}


//-----------------------------------------------------------------------------
// Build a list of the control group that form the path to the root of the tree
// to which the control group belongs
//-----------------------------------------------------------------------------
void CDmeControlGroup::BuildPathFromRoot( CUtlVector< CDmeControlGroup * > &pathToGroup )
{
	CUtlVector< CDmeControlGroup * > pathToRoot( 0, 16 );

	CDmeControlGroup *pCurrent = this;

	while ( pCurrent )
	{
		pathToRoot.AddToTail( pCurrent );
		pCurrent = pCurrent->FindParent();
	}

	int nNumGroups = pathToRoot.Count();
	pathToGroup.SetCount( nNumGroups );

	for ( int iGroup = 0; iGroup < nNumGroups; ++iGroup )
	{
		pathToGroup[ iGroup ] = pathToRoot[ nNumGroups - 1 - iGroup ];
	}
}


//-----------------------------------------------------------------------------
// Find the animation set associated with the control group
//-----------------------------------------------------------------------------
CDmeAnimationSet *CDmeControlGroup::FindAnimationSet( bool bSearchAncestors ) const 
{
	const static CUtlSymbolLarge symRootControlGroup = g_pDataModel->GetSymbol( "rootControlGroup" );
	const CDmeControlGroup *pCurrent = this;

	while ( pCurrent )
	{
		CDmeAnimationSet *pAnimationSet = FindReferringElement< CDmeAnimationSet >( pCurrent, symRootControlGroup );
		if ( pAnimationSet != NULL )
			return pAnimationSet;

		if ( bSearchAncestors == false )
			break;

		const CDmeControlGroup *pParent = pCurrent->FindParent();
		if ( pCurrent == pParent )
			break;

		pCurrent = pParent;
	}

	return NULL;
}


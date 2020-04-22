//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMSETCONTROLGROUPPANEL_H
#define BASEANIMSETCONTROLGROUPPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAnimationSetEditorController.h"
#include "vgui_controls/EditablePanel.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlntree.h"
#include "tier1/utldict.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimationSetEditor;
class CBaseAnimationSetControl;
class CDmeAnimationSet;
class CDmeChannel;
class CDmeControlGroup;
class CDmeDag;
class CDmElement;
class CAnimGroupTree;

namespace vgui
{
	class TreeView;
	class IScheme;
	class Menu;
};


//-----------------------------------------------------------------------------
// Animation set editor control/group tree item types. These types represent 
// the different types of items which can be added to the m_hGroupsTree. They 
// are used in determining the action to be taken when an item is selected and 
// to determine the content of the right click context menu for an item.
//-----------------------------------------------------------------------------
enum AnimTreeItemType_t
{
	ANIMTREE_ITEM_ANIMSET,
	ANIMTREE_ITEM_GROUP,
	ANIMTREE_ITEM_CONTROL,
	ANIMTREE_ITEM_COMPONENT,
};


//-----------------------------------------------------------------------------
// Panel which shows a tree of controls
//-----------------------------------------------------------------------------
class CBaseAnimSetControlGroupPanel : public vgui::EditablePanel, public IAnimationSetControlSelectionChangedListener
{
	DECLARE_CLASS_SIMPLE( CBaseAnimSetControlGroupPanel, EditablePanel );
public:
	CBaseAnimSetControlGroupPanel( vgui::Panel *parent, char const *className, CBaseAnimationSetEditor *editor, bool bControlStateInterface );
	virtual ~CBaseAnimSetControlGroupPanel();

	CBaseAnimationSetEditor *GetEditor() { return m_hEditor; }

	void ChangeAnimationSetClip( CDmeFilmClip *pFilmClip );
	void OnControlsAddedOrRemoved();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	CDmeDag *GetWorkCameraParent();

	// Rebuild the tree view from the current control selection hierarchy
	void RebuildTree( bool bRestoreExpansion );
	void UpdateSelection();

	// inherited from IAnimationSetControlSelectionChangedListener
	virtual void OnControlSelectionChanged();
	virtual void ExpandTreeToControl( const CDmElement *pSelection, TransformComponent_t nComponentFlags );

	// Create a new control group containing the selected controls
	void CreateGroupFromSelectedControls();


protected:

	MESSAGE_FUNC_INT_INT( OnTreeViewItemSelected, "TreeViewItemSelected", itemIndex, replaceSelection );
	MESSAGE_FUNC_INT( OnTreeViewItemDeselected, "TreeViewItemDeselected", itemIndex );
	MESSAGE_FUNC( OnTreeViewStartRangeSelection, "TreeViewStartRangeSelection" );
	MESSAGE_FUNC( OnTreeViewFinishRangeSelection, "TreeViewFinishRangeSelection" );
	MESSAGE_FUNC( OnTreeViewItemSelectionCleared, "TreeViewItemSelectionCleared" );
	MESSAGE_FUNC_INT( OnTreeViewOpenContextMenu, "TreeViewOpenContextMenu", itemID );


protected:

	void SelectAnimTreeItem( int itemIndex, ESelectionMode selectionMode );

	struct ElementExpansion_t
	{
		CDmElement				*m_pElement;
		TransformComponent_t	m_ComponentFlags;
	};

	// pre-order traversal so that we can ExpandItems linearly
	void CollectExpandedItems( CUtlVector< ElementExpansion_t > &expandedNodes, int nParentIndex );
	// assumes expandedNodes have parents before children (ie expandedNodes is a pre-order traversal)
	void ExpandItems( const CUtlVector< ElementExpansion_t > &expandedNodes );

	SelectionState_t UpdateSelection_R( int nParentIndex );

	vgui::DHANDLE< CBaseAnimationSetEditor >	m_hEditor;

	vgui::DHANDLE< CAnimGroupTree >		m_hGroups;

	CBaseAnimationSetControl			*m_pController;

	Color								m_FullSelectionColor;
	Color								m_PartialSelectionColor;
	Color								m_ContextMenuHighlightColor;

	friend class CAnimGroupTree;
};

#endif // BASEANIMSETCONTROLGROUPPANEL_H

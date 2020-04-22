//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "dme_controls/BaseAnimSetControlGroupPanel.h"
#include "vgui_controls/TreeView.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImageList.h"
#include "vgui_controls/Tooltip.h"
#include "tier1/KeyValues.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmerig.h"
#include "movieobjects/dmetransformcontrol.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "dme_controls/dmecontrols_utils.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

enum StateIcons_t
{
	STATE_ICON_WORK_CAMERA_PARENT_ACTIVE,
	STATE_ICON_WORK_CAMERA_PARENT_HIDDEN,
	STATE_ICON_DAG_LOCKED,
	STATE_ICON_DAG_LOCKED_PARTIAL,
	STATE_ICON_DAG_LOCKED_WORLD,
	STATE_ICON_DAG_LOCKED_WORLD_PARTIAL,
	STATE_ICON_COUNT
};

enum StateIconSetType_t
{
	STATE_ICON_SET_INVALID,
	STATE_ICON_SET_GROUP,
	STATE_ICON_SET_CONTROL_POSITION,
	STATE_ICON_SET_CONTROL_ROTATION,
};


//-----------------------------------------------------------------------------
// A panel containing a set of icons providing information about the state of
// an animation group tree item.
//-----------------------------------------------------------------------------
class CAnimGroupStateIconSet : public Panel
{	
	DECLARE_CLASS_SIMPLE( CAnimGroupStateIconSet, Panel );


	// The sole purpose of the icon button is to allow right clicks
	// to be handled and used for context menu creation.
	class IconButton : public Button
	{
		DECLARE_CLASS_SIMPLE( IconButton, Button );

	public:
		IconButton( CAnimGroupStateIconSet *pIconSet, const char *pName ) 
		: Button( pIconSet, pName, "" )
		, m_pIconSet( pIconSet )
		{}

		void OnMousePressed( MouseCode code )
		{
			if ( code == MOUSE_RIGHT )
			{
				m_pIconSet->IconButtonRightClick();
				return;
			}
			BaseClass::OnMousePressed( code );
		}

		CAnimGroupStateIconSet *const m_pIconSet;
	};


public:
	
	CAnimGroupStateIconSet( Panel *pParent, const char *pchName, StateIconSetType_t stateType, CDmeDag *pDag, ImageList &imageList, const int *pImageIndexMap );

	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PerformLayout();
	virtual bool IsDroppable( CUtlVector< KeyValues * > &msglist );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msglist );

	void IconButtonRightClick();

	void UpdateState();

private:

	MESSAGE_FUNC( OnLockDagButton, "LockDagButton" );

	static const CDmeDag *GetDagFromDragElement( CDmElement *pElement );

	static const int LOCKED_TOOLTIP_DELAY = 750;
	static const int UNLOCKED_TOOLTIP_DELAY = 1500;
	
	ImageList		&m_ImageList;
	const int *const m_pImageIndexMap;
	const int		m_StateType;
	CDmeDag			*m_pDag;
	IconButton		*m_pLockButton;
};


CAnimGroupStateIconSet::CAnimGroupStateIconSet( Panel *pParent, const char *pchName, StateIconSetType_t itemType, CDmeDag *pDag, ImageList &imageList, const int *pImageIndexMap )
: BaseClass( pParent, "AnimGroupStateIconSet" )
, m_ImageList( imageList )
, m_pImageIndexMap( pImageIndexMap )
, m_StateType( itemType )
, m_pDag( pDag )
, m_pLockButton( NULL )
{
	m_pLockButton = new IconButton( this, "LockButton" );
	m_pLockButton->SetVisible( true );
	m_pLockButton->AddActionSignalTarget( this );
	m_pLockButton->SetCommand( new KeyValues( "LockDagButton" ) );
	m_pLockButton->SetKeyBoardInputEnabled( false );
	
	vgui::BaseTooltip *pTooltip = m_pLockButton->GetTooltip();
	if ( pTooltip )
	{
		pTooltip->SetTooltipDelay( UNLOCKED_TOOLTIP_DELAY );
		pTooltip->SetText( "#LockButtonTip" );
		pTooltip->SetTooltipFormatToSingleLine();
	}

	SetDropEnabled( true );
}


void CAnimGroupStateIconSet::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	Color buttonColor = pScheme->GetColor( "Frame.BgColor", Color( 0, 0, 0, 0 ) );
	SetBgColor( buttonColor );

	if ( m_pLockButton  )
	{
		m_pLockButton->SetDefaultBorder( pScheme->GetBorder( "DepressedBorder" ) );


	}
}

void CAnimGroupStateIconSet::PerformLayout()
{
	int nHeight = GetTall();

	if ( m_pLockButton )
	{
		m_pLockButton->SetBounds( 1, 1, nHeight - 1, nHeight - 1 );
	}
}


const CDmeDag *CAnimGroupStateIconSet::GetDagFromDragElement( CDmElement *pElement )
{
	if ( pElement == NULL )
		return NULL;

	const CDmeDag *pDag = NULL;
	
	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pElement );
	if ( pTransformControl )
	{
		pDag = pTransformControl->GetDag();
	}
	else
	{
		pDag = CastElement< CDmeDag >( pElement );
	}

	return pDag;
}


bool CAnimGroupStateIconSet::IsDroppable( CUtlVector< KeyValues * > &msglist )
{
	int nCount = msglist.Count();
	if ( nCount != 1 )
		return false;

	KeyValues *pData = msglist[ 0 ];
	CDmElement *pElement = GetElementKeyValue< CDmElement >( pData, "dmeelement" );
	
	const CDmeDag *pParentDag = GetDagFromDragElement( pElement );
	if ( pParentDag == NULL )
		return false;

	// Can't parent a dag to is child
	if ( m_pDag->IsAncestorOfDag( pParentDag ) )
		return false;

	// Can't parent a dag to itself.
	if ( m_pDag == pParentDag )
		return false;

	return true;
}

void CAnimGroupStateIconSet::OnPanelDropped( CUtlVector< KeyValues * >& msglist )
{
	int nCount = msglist.Count();
	if ( nCount == 1 )
	{	
		KeyValues *pData = msglist[ 0 ];
		CDmElement *pElement = GetElementKeyValue< CDmElement >( pData, "dmeelement" );
		const CDmElement *pParentDag = GetDagFromDragElement( pElement );

		if ( pParentDag )
		{
			bool bPosition = ( ( m_StateType == STATE_ICON_SET_GROUP ) || ( m_StateType == STATE_ICON_SET_CONTROL_POSITION ) ); 
			bool bRotation = ( ( m_StateType == STATE_ICON_SET_GROUP ) || ( m_StateType == STATE_ICON_SET_CONTROL_ROTATION ) );

			KeyValues *pMsgKv = new KeyValues( "SetOverrideParent" );
			pMsgKv->SetInt( "targetDag", m_pDag->GetHandle() );
			pMsgKv->SetInt( "parentDag", pParentDag->GetHandle()  );
			pMsgKv->SetBool( "position", bPosition );
			pMsgKv->SetBool( "rotation", bRotation );
			PostMessage( GetParent(), pMsgKv, 0.0f );
		}
	}
}

void CAnimGroupStateIconSet::IconButtonRightClick()
{
	bool bPosition = ( ( m_StateType == STATE_ICON_SET_GROUP ) || ( m_StateType == STATE_ICON_SET_CONTROL_POSITION ) ); 
	bool bRotation = ( ( m_StateType == STATE_ICON_SET_GROUP ) || ( m_StateType == STATE_ICON_SET_CONTROL_ROTATION ) );

	KeyValues *pMsgKv = new KeyValues( "OpenLockContextMenu" );
	pMsgKv->SetInt( "targetDag", m_pDag->GetHandle() );
	pMsgKv->SetBool( "position", bPosition );
	pMsgKv->SetBool( "rotation", bRotation );

	PostMessage( GetParent(), pMsgKv, 0.0f );
}


void CAnimGroupStateIconSet::UpdateState()
{
	if ( m_pDag == NULL )
		return;
	
	m_pLockButton->ClearImages();
	
	vgui::BaseTooltip *pTooltip = m_pLockButton->GetTooltip();
	
	bool bPos = false;
	bool bRot = false;
	const CDmeDag *pOverrideParent = m_pDag->GetOverrideParent( bPos, bRot, true );

	if ( pOverrideParent != NULL )
	{
		bool bLockedToWorld = ( pOverrideParent->GetParent() == NULL );
		int nFullLockedIcon = bLockedToWorld ? STATE_ICON_DAG_LOCKED_WORLD : STATE_ICON_DAG_LOCKED;
		int nPartialLockedIcon = bLockedToWorld ? STATE_ICON_DAG_LOCKED_WORLD_PARTIAL : STATE_ICON_DAG_LOCKED_PARTIAL;
		
		// Change the image to the grayed out version if both position and rotation are not overridden
		int nImageIndex = 0;
		
		if ( m_StateType == STATE_ICON_SET_GROUP )
		{
			nImageIndex = ( bPos && bRot ) ? m_pImageIndexMap[ nFullLockedIcon ] : m_pImageIndexMap[ nPartialLockedIcon ];
		}
		else if ( ( ( m_StateType == STATE_ICON_SET_CONTROL_POSITION ) && bPos ) ||
			      ( ( m_StateType == STATE_ICON_SET_CONTROL_ROTATION ) && bRot ) )
		{
			nImageIndex = m_pImageIndexMap[ nFullLockedIcon ];
		}

		// Update the image of on the button
		vgui::IImage *pLockImage = m_ImageList.GetImage( nImageIndex );
		m_pLockButton->AddImage( pLockImage, 0 );

		// Update the tool tip text describing what dag is new parent
		if ( pTooltip )
		{				
			if ( bLockedToWorld )
			{
				pTooltip->SetText( "#LockedToWorld" );
			}
			else
			{
				const wchar_t *pLabel = g_pVGuiLocalize->Find( "#LockedTo" );
				if ( pLabel )
				{
					char itemText[ 32 ];
					char tipText[ 64 ];
					g_pVGuiLocalize->ConvertUnicodeToANSI( pLabel, itemText, sizeof( itemText ) );
					V_snprintf( tipText, sizeof( tipText ), "%s %s", itemText, pOverrideParent->GetName() );
					pTooltip->SetText( tipText );
				}
			}
			
			pTooltip->SetTooltipDelay( LOCKED_TOOLTIP_DELAY );
		}
	}
	else
	{		
		pTooltip->SetTooltipDelay( UNLOCKED_TOOLTIP_DELAY );
		pTooltip->SetText( "#LockButtonTip" );
	}
}


void CAnimGroupStateIconSet::OnLockDagButton()
{
	if ( m_pDag )
	{
		bool bPosition = ( ( m_StateType == STATE_ICON_SET_GROUP ) || ( m_StateType == STATE_ICON_SET_CONTROL_POSITION ) ); 
		bool bRotation = ( ( m_StateType == STATE_ICON_SET_GROUP ) || ( m_StateType == STATE_ICON_SET_CONTROL_ROTATION ) );

		KeyValues *pMsgKv = new KeyValues( "ToggleDagLock" );
		pMsgKv->SetInt( "targetDag", m_pDag->GetHandle() );
		pMsgKv->SetBool( "position", bPosition );
		pMsgKv->SetBool( "rotation", bRotation );
		PostMessage( GetParent(), pMsgKv, 0.0f );
	}
}


//-----------------------------------------------------------------------------
// Shows the tree view of the animation groups
//-----------------------------------------------------------------------------
class CAnimGroupTree : public TreeView
{
	DECLARE_CLASS_SIMPLE( CAnimGroupTree, TreeView );
public:
	CAnimGroupTree( Panel *parent, const char *panelName, CBaseAnimSetControlGroupPanel *groupPanel, bool bStateInterface );
	virtual ~CAnimGroupTree();

	virtual bool IsItemDroppable( int itemIndex, bool bInsertBefore, CUtlVector< KeyValues * >& msglist );
	virtual void OnItemDropped( int itemIndex, bool bInsertBefore, CUtlVector< KeyValues * >& msglist );
	virtual bool CanCurrentlyEditLabel( int nItemIndex ) const;
	virtual void OnLabelChanged( int nItemIndex, char const *pOldString, char const *pNewString );
	virtual void GetSelectedItemsForDrag( int nPrimaryDragItem, CUtlVector< int >& list );
	virtual void GenerateDragDataForItem( int itemIndex, KeyValues *msg );
	virtual void GenerateContextMenu( int itemIndex, int x, int y );
	virtual void GenerateChildrenOfNode(int itemIndex);

    virtual void RemoveItem(int itemIndex, bool bPromoteChildren, bool bRecursivelyRemove = false );
	virtual void RemoveAll();

	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PaintBackground();
	virtual void PerformLayout();
	
	virtual void OnTick();

	virtual void OnMousePressed( MouseCode code );

	// Item add helpers
	int AddAnimationSetToTree( CDmeAnimationSet *pAnimSet );
	int AddControlGroupToTree( int parentIndex, CDmeControlGroup *pControlGroup, CDmeControlGroup *pParentGroup, CDmeAnimationSet *pAnimSet );
	int AddControlToTree( int parentIndex, CDmElement *pControl, CDmeControlGroup *pControlGroup, CDmeAnimationSet *pAnimSet );
	void AddTransformComponentsToTree( int parentIndex, CDmeTransformControl *pTransformControl, CDmeControlGroup *pControlGroup, CDmeAnimationSet *pAnimSet, TransformComponent_t nComponentFlags );

	CDmElement *GetTreeItemData( int nTreeIndex, AnimTreeItemType_t *pItemType = NULL,
								CDmeAnimationSet **ppParentAnimationSet = NULL,
								CDmeControlGroup **ppParentControlGroup = NULL ) const;

	// Get the component flags associated with the specified item
	TransformComponent_t GetItemComponentFlags( int nTreeIndex ) const;

	// Get the control group associated with the specified tree item
	CDmeControlGroup *GetControlGroupForTreeItem( int nTreeItemIndex ) const;

	// Get the state icon set panel associated with the specified tree item
	CAnimGroupStateIconSet *GetTreeItemStateIconSet( int nTreeItemIndex );

	// Get the dag node associated with the specified tree item
	CDmeDag *GetDagForTreeItem( int nTreeItemIndex ) const;

	// Find the child of the specified item which has the specified element as its "handle" value
	int FindChildItemForElement( int nParentIndex, const CDmElement *pElement, TransformComponent_t nComponentFlags = TRANSFORM_COMPONENT_NONE );

	// Find the index of the the tree view item that has the specified element as its "handle" value
	int FindItemForElement( const CDmElement *pElement, TransformComponent_t nComponentFlags = TRANSFORM_COMPONENT_NONE );

	// Build the tree view items from the root down to the specified animation set
	int BuildTreeToAnimationSet( CDmeAnimationSet *pAnimationSet );

	// Build the tree view items from the root down to the specified control group
	int BuildTreeToGroup( CDmeControlGroup *pGroup, CDmeAnimationSet *pAnimationSet );

	// Build the tree view items from the root down to the specified control
	void BuildTreeToControl( const CDmElement *pElement, TransformComponent_t nComponentFlags );

	// Set the selection state on the specified item
	void SetItemSelectionState( int nItemIndex, SelectionState_t selectionState );

	// Get the selection state of the specified item
	SelectionState_t GetItemSelectionState( int nItemIndex ) const;

	// Get a list of the controls which are fully selected and whose parents are not fully selected
	void GetSelectionRootItems( CUtlVector< int > &selectedItems ) const;

private:

	MESSAGE_FUNC( OnClearWorkCameraParent, "ClearWorkCameraParent" );
	MESSAGE_FUNC_INT( OnResetTransformPivot, "OnResetTransformPivot", viewCenter );
	MESSAGE_FUNC_PARAMS( OnToggleDagLock, "ToggleDagLock", params );
	MESSAGE_FUNC_PARAMS( OnSetOverrideParent, "SetOverrideParent", params );
	MESSAGE_FUNC_PARAMS( OnOpenLockContextMenu, "OpenLockContextMenu", params );


	void CleanupContextMenu();

	virtual void OnContextMenuSelection( int itemIndex );

	int AddItemToTree(
		AnimTreeItemType_t itemType, 
		const char *label, 
		int parentIndex, 
		const Color& fg, 
		CDmElement *pElement,
		CDmeAnimationSet *pAnimSet,
		CDmeControlGroup *pControlGroup,
		bool bExpandable,
		SelectionState_t selection,
		TransformComponent_t nComponentFlags );

	Color ModifyColorByGroupState( const Color &baseColor, CDmeControlGroup *pControlGroup );
	void AddDmeControlGroup( int nParentItemIndex, CDmeAnimationSet *pAnimationSet, CDmeControlGroup *pGroup );
	bool VisibleControlsBelow_R( CDmeControlGroup *pGroup );
	
	static bool CanAddDragIntoGroup( const CDmeControlGroup *pTargetGroup, const CDmElement *pTargetElement, const CDmElement *pDragElement, bool bInsertBefore );

	static SelectionState_t GetSelectionStateForFlags( int nBaseFlags, int nSelectionFlags );

	vgui::DHANDLE< vgui::Menu >		m_hContextMenu;
	CBaseAnimSetControlGroupPanel	*m_pGroupPanel;

	Button							*m_pWorkCameraParentButton;
	ImageList						m_Images;
	int								m_StateIconIndices[ STATE_ICON_COUNT ];

	Color							m_RootColor;
	Color							m_StateColumnColor;
	int								m_nStateColumnWidth;
	bool							m_bStateInterface;
};

CAnimGroupTree::CAnimGroupTree( Panel *parent, const char *panelName, CBaseAnimSetControlGroupPanel *groupPanel, bool bStateInterface ) : 
	BaseClass( parent, panelName ),
	m_pGroupPanel( groupPanel ),
	m_Images( false ),
	m_RootColor( 128, 128, 128, 255 ),
	m_StateColumnColor( 0, 0, 0, 0 ),
	m_nStateColumnWidth( 0 ),
	m_bStateInterface( bStateInterface )
{
	if ( m_bStateInterface )
	{
		m_nStateColumnWidth = 20;
		SetTreeIndent( m_nStateColumnWidth - 2 );
	}

	SetShowRootNode( false );
	SetDragEnabledItems( true );
	SetAllowLabelEditing( true );
	SetEnableInsertDropLocation( true );

	m_pWorkCameraParentButton = new Button( this, "workCameraParent", "" );
	m_pWorkCameraParentButton->SetVisible( false );
	m_pWorkCameraParentButton->AddActionSignalTarget( this );
	m_pWorkCameraParentButton->SetCommand( new KeyValues( "ClearWorkCameraParent" ) );
	m_pWorkCameraParentButton->SetKeyBoardInputEnabled( false );

	m_StateIconIndices[ STATE_ICON_WORK_CAMERA_PARENT_ACTIVE ]	= m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_referenceframe_active", false ) );
	m_StateIconIndices[ STATE_ICON_WORK_CAMERA_PARENT_HIDDEN ]	= m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_referenceframe_active_hidden", false ) );
	m_StateIconIndices[ STATE_ICON_DAG_LOCKED ]					= m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_dag_locked", false ) );
	m_StateIconIndices[ STATE_ICON_DAG_LOCKED_PARTIAL ]			= m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_dag_locked_grey", false ) );
	m_StateIconIndices[ STATE_ICON_DAG_LOCKED_WORLD ]			= m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_dag_locked_world", false ) );
	m_StateIconIndices[ STATE_ICON_DAG_LOCKED_WORLD_PARTIAL ]	= m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_dag_locked_world_grey", false ) );

	ivgui()->AddTickSignal( GetVPanel(), 0 );
}

CAnimGroupTree::~CAnimGroupTree()
{
	CleanupContextMenu();
}

void CAnimGroupTree::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_RootColor = pScheme->GetColor( "AnimSet.RootColor", Color( 128, 128, 128, 255 ) );
	m_StateColumnColor = pScheme->GetColor( "Frame.BgColor", Color( 0, 0, 0, 0 ) );
	SetFont( pScheme->GetFont( "DefaultBold", IsProportional() ) );
}

int CAnimGroupTree::AddAnimationSetToTree( CDmeAnimationSet *pAnimSet )
{
	// Don't add the animation set if the root control group is not visible
	CDmeControlGroup *pRootGroup = pAnimSet->GetRootControlGroup();
	if ( !m_pGroupPanel->m_pController->IsControlGroupVisible( pRootGroup ) )
		return -1;

	int parentIndex = GetRootItemIndex();
	SelectionState_t selection = m_pGroupPanel->m_pController->GetSelectionState( pAnimSet );
	
	Color groupColor = ModifyColorByGroupState( m_RootColor, pRootGroup );

	return AddItemToTree( ANIMTREE_ITEM_ANIMSET, pAnimSet->GetName(), parentIndex, groupColor, pAnimSet, pAnimSet, NULL, true, selection, TRANSFORM_COMPONENT_NONE );
}

int CAnimGroupTree::AddControlGroupToTree( int parentIndex, CDmeControlGroup *pControlGroup, CDmeControlGroup *pParentGroup, CDmeAnimationSet *pAnimSet )
{
	SelectionState_t selection = m_pGroupPanel->m_pController->GetSelectionState( pControlGroup );
	
	Color groupColor = ModifyColorByGroupState( pControlGroup->GroupColor(), pControlGroup );

	return AddItemToTree( ANIMTREE_ITEM_GROUP, pControlGroup->GetName(), parentIndex, groupColor, pControlGroup, pAnimSet, pParentGroup, true, selection, TRANSFORM_COMPONENT_NONE );
}

int CAnimGroupTree::AddControlToTree( int parentIndex, CDmElement *pControl, CDmeControlGroup *pControlGroup, CDmeAnimationSet *pAnimSet )
{
	SelectionState_t selection = m_pGroupPanel->m_pController->GetSelectionState( pControl );
	bool bTransformControl = pControl->IsA( CDmeTransformControl::GetStaticTypeSymbol() );
	TransformComponent_t nComponentFlags = bTransformControl ? TRANSFORM_COMPONENT_ALL : TRANSFORM_COMPONENT_NONE;

	Color controlColor = ModifyColorByGroupState( pControlGroup->ControlColor(), pControlGroup );

	return AddItemToTree( ANIMTREE_ITEM_CONTROL, pControl->GetName(), parentIndex, controlColor, pControl, pAnimSet, pControlGroup, bTransformControl, selection, nComponentFlags );
}


SelectionState_t CAnimGroupTree::GetSelectionStateForFlags( int nBaseFlags, int nSelectionFlags )
{
	int nMaskedFlags = nBaseFlags & nSelectionFlags;

	if ( nMaskedFlags == nBaseFlags )
		return SEL_ALL;

	return ( ( nMaskedFlags > 0 ) ? SEL_SOME : SEL_NONE );
}


void CAnimGroupTree::AddTransformComponentsToTree( int nParentIndex, CDmeTransformControl *pControl, CDmeControlGroup *pControlGroup, CDmeAnimationSet *pAnimSet, TransformComponent_t nParentComponentFlags )
{
	int nSelectionFlags = m_pGroupPanel->m_pController->GetSelectionComponentFlags( pControl );
	const char *pName = pControl->GetName();

	Color color = ModifyColorByGroupState( pControlGroup->ControlColor(), pControlGroup );

	if ( nParentComponentFlags == TRANSFORM_COMPONENT_ALL )
	{
		SelectionState_t posSelection = GetSelectionStateForFlags( TRANSFORM_COMPONENT_POSITION, nSelectionFlags );
		SelectionState_t rotSelection = GetSelectionStateForFlags( TRANSFORM_COMPONENT_ROTATION, nSelectionFlags );
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - pos", pName ), nParentIndex, color, pControl, pAnimSet, pControlGroup, true, posSelection, TRANSFORM_COMPONENT_POSITION );
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - rot", pName ), nParentIndex, color, pControl, pAnimSet, pControlGroup, true, rotSelection, TRANSFORM_COMPONENT_ROTATION );
	}
	else if ( nParentComponentFlags == TRANSFORM_COMPONENT_POSITION )
	{
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - pos.x", pName ), nParentIndex, color, pControl, pAnimSet, NULL, false, GetSelectionStateForFlags( TRANSFORM_COMPONENT_POSITION_X, nSelectionFlags ), TRANSFORM_COMPONENT_POSITION_X );
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - pos.y", pName ), nParentIndex, color, pControl, pAnimSet, NULL, false, GetSelectionStateForFlags( TRANSFORM_COMPONENT_POSITION_Y, nSelectionFlags ), TRANSFORM_COMPONENT_POSITION_Y );
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - pos.z", pName ), nParentIndex, color, pControl, pAnimSet, NULL, false, GetSelectionStateForFlags( TRANSFORM_COMPONENT_POSITION_Z, nSelectionFlags ), TRANSFORM_COMPONENT_POSITION_Z );
	}
	else if ( nParentComponentFlags == TRANSFORM_COMPONENT_ROTATION )
	{
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - rot.x", pName ), nParentIndex, color, pControl, pAnimSet, NULL, false, GetSelectionStateForFlags( TRANSFORM_COMPONENT_ROTATION_X, nSelectionFlags ), TRANSFORM_COMPONENT_ROTATION_X );
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - rot.y", pName ), nParentIndex, color, pControl, pAnimSet, NULL, false, GetSelectionStateForFlags( TRANSFORM_COMPONENT_ROTATION_Y, nSelectionFlags ), TRANSFORM_COMPONENT_ROTATION_Y );
		AddItemToTree( ANIMTREE_ITEM_COMPONENT, CFmtStr( "%s - rot.z", pName ), nParentIndex, color, pControl, pAnimSet, NULL, false, GetSelectionStateForFlags( TRANSFORM_COMPONENT_ROTATION_Z, nSelectionFlags ), TRANSFORM_COMPONENT_ROTATION_Z );
	}
}


Color CAnimGroupTree::ModifyColorByGroupState( const Color &baseColor, CDmeControlGroup *pControlGroup )
{
	Color groupColor = baseColor;
	int nAlpha = ( pControlGroup->IsSelectable() && pControlGroup->IsVisible() ) ? 255 : 64;
	groupColor.SetColor( groupColor.r(), groupColor.g(), groupColor.b(), nAlpha );
	return groupColor;
}


int CAnimGroupTree::AddItemToTree( AnimTreeItemType_t itemType, const char *label, int parentIndex, const Color& fg, CDmElement *pElement, CDmeAnimationSet *pAnimSet, CDmeControlGroup *pControlGroup, bool bExpandable, SelectionState_t selection, TransformComponent_t nComponentFlags )
{
	DmElementHandle_t hElement = pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID;
	DmElementHandle_t hAnimSet = pAnimSet ? pAnimSet->GetHandle() : DMELEMENT_HANDLE_INVALID;
	DmElementHandle_t hControlGroup = pControlGroup ? pControlGroup->GetHandle() : DMELEMENT_HANDLE_INVALID;

	KeyValues *kv = new KeyValues( "item", "text", label );
	kv->SetInt( "droppable", 1 );
	kv->SetInt( "itemType", ( int )itemType );
	kv->SetInt( "handle", ( int )hElement );
	kv->SetInt( "animset", ( int )hAnimSet );
	kv->SetInt( "controlgroup", ( int )hControlGroup );
	kv->SetInt( "selection", ( int )selection );
	kv->SetInt( "componentFlags", ( int )nComponentFlags );
	kv->SetInt( "Expand", bExpandable ? 1 : 0 );
	
	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pElement );
	CDmeTransform *pTransform = ( pTransformControl != NULL ) ? pTransformControl->GetTransform() : NULL;

	CDmeDag *pDag = ( pTransform != NULL ) ? pTransform->GetDag() : NULL;

	if ( m_bStateInterface )
	{			
		StateIconSetType_t stateIconSetType = STATE_ICON_SET_INVALID;

		if ( itemType == ANIMTREE_ITEM_COMPONENT )
		{
			if ( nComponentFlags == TRANSFORM_COMPONENT_POSITION )
			{
				stateIconSetType = STATE_ICON_SET_CONTROL_POSITION;
			}
			else if ( nComponentFlags == TRANSFORM_COMPONENT_ROTATION )
			{
				stateIconSetType = STATE_ICON_SET_CONTROL_ROTATION;
			}
		}
		else if ( itemType == ANIMTREE_ITEM_CONTROL )
		{
			stateIconSetType = STATE_ICON_SET_GROUP;
		}

		if ( pDag && ( stateIconSetType != STATE_ICON_SET_INVALID ) )
		{
			CAnimGroupStateIconSet *pIconSet = new CAnimGroupStateIconSet( this, label, stateIconSetType, pDag, m_Images, m_StateIconIndices );
			kv->SetPtr( "stateIconSet", pIconSet );
		}
	}

	int idx = AddItem( kv, parentIndex );
	SetItemFgColor( idx, fg );
	SetItemSelectionTextColor( idx, fg );
	

	SetSilentMode( true );

	if ( selection == SEL_ALL || selection == SEL_SOME )
	{
		Color color = ( selection == SEL_ALL ) ? Color( 128, 128, 128, 128 ) : Color( 128, 128, 64, 64 );
		SetItemSelectionBgColor( idx, color );
		SetItemSelectionUnfocusedBgColor( idx, color );
		AddSelectedItem( idx, false, false, true );
	}
	else
	{
		Color color( 0, 0, 0, 128 );
		SetItemSelectionBgColor( idx, color );
		SetItemSelectionUnfocusedBgColor( idx, color );
		RemoveSelectedItem( idx );
	}


	if ( ( itemType == ANIMTREE_ITEM_GROUP ) || ( itemType == ANIMTREE_ITEM_ANIMSET ) )
	{	
		SetLabelEditingAllowed( idx, true );
	}

	SetSilentMode( false );

	ExpandItem( idx, false );

	kv->deleteThis();

	return idx;
}

CDmElement *CAnimGroupTree::GetTreeItemData( int nTreeIndex, AnimTreeItemType_t *pItemType /*= NULL */,
											CDmeAnimationSet **ppParentAnimationSet /*= NULL*/,
											CDmeControlGroup **ppControlGroup /*= NULL*/ ) const
{
	KeyValues *kv = GetItemData( nTreeIndex );
	if ( !kv )
		return NULL;

	if ( pItemType )
	{
		*pItemType = static_cast< AnimTreeItemType_t >( kv->GetInt( "itemType" ) );
	}

	if ( ppParentAnimationSet )
	{
		*ppParentAnimationSet = GetElementKeyValue< CDmeAnimationSet >( kv, "animset" );
	}

	if ( ppControlGroup )
	{
		*ppControlGroup = GetElementKeyValue< CDmeControlGroup >( kv, "controlgroup" );
	}

	return GetElementKeyValue< CDmElement >( kv, "handle" );
}


TransformComponent_t CAnimGroupTree::GetItemComponentFlags( int nTreeIndex ) const
{
	KeyValues *kv = GetItemData( nTreeIndex );
	return static_cast< TransformComponent_t >( kv->GetInt( "componentFlags" ) );
}


CDmeControlGroup *CAnimGroupTree::GetControlGroupForTreeItem( int nItemIndex ) const
{
	CDmElement *pElement = GetTreeItemData( nItemIndex, NULL );
	
	CDmeControlGroup *pControlGroup = NULL;
	const CDmeAnimationSet *pAnimationSet = CastElement< CDmeAnimationSet >( pElement );

	if ( pAnimationSet )
	{
		pControlGroup = pAnimationSet->GetRootControlGroup();
	}
	else
	{
		pControlGroup = CastElement< CDmeControlGroup >( pElement );
	}

	return pControlGroup;
}


CAnimGroupStateIconSet *CAnimGroupTree::GetTreeItemStateIconSet( int nTreeIndex )
{
	KeyValues *kv = GetItemData( nTreeIndex );
	if ( !kv )
		return NULL;

	return static_cast< CAnimGroupStateIconSet * >( kv->GetPtr( "stateIconSet" ) );
}


CDmeDag *CAnimGroupTree::GetDagForTreeItem( int nTreeItemIndex ) const
{
	AnimTreeItemType_t itemType;
	CDmElement *pElement = GetTreeItemData( nTreeItemIndex, &itemType );

	if ( !pElement && itemType != ANIMTREE_ITEM_GROUP )
		return NULL;

	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pElement );
	if ( pTransformControl == NULL )
		return NULL;

	CDmeTransform *pTransform = pTransformControl->GetTransform();

	if ( !pTransform )
		return NULL;	

	return pTransform->GetDag();
}


void CAnimGroupTree::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}
}

bool CAnimGroupTree::CanAddDragIntoGroup( const CDmeControlGroup *pTargetGroup, const CDmElement *pTargetElement, const CDmElement *pDragElement, bool bInsertBefore )
{
	const static CUtlSymbolLarge symControls = g_pDataModel->GetSymbol( "controls" );

	if ( ( pTargetGroup == NULL ) || ( pDragElement == NULL ) || ( pTargetElement == NULL ) )
		return false;

	// Cannot drag a group into itself
	if ( pDragElement == pTargetGroup ) 
		return false;

	CDmeAnimationSet *pTargetGroupAnimSet = pTargetGroup->FindAnimationSet( true );
	CDmeAnimationSet *pDragGroupAnimSet = NULL;
	
	const CDmeControlGroup *pDragGroup = CastElement< CDmeControlGroup >( pDragElement );
	
	bool bTargetElenentIsGroup = pTargetElement->IsA( CDmeControlGroup::GetStaticTypeSymbol() );

	if ( pDragGroup )
	{
		// Cannot drag a group into a group that already exists in its sub-tree
		if ( pDragGroup->IsAncestorOfGroup( pTargetGroup ) )
			return false;

		// Cannot insert a group before a control
		if ( bInsertBefore && !bTargetElenentIsGroup )
			return false;
				
		CDmeControlGroup *pDragGroupParent = pDragGroup->FindParent();
		pDragGroupAnimSet = ( pDragGroupParent != NULL ) ? pDragGroupParent->FindAnimationSet( true ) : NULL;
	}
	else 
	{
		// Cannot insert a control before a group
		if ( bInsertBefore && bTargetElenentIsGroup )
			return false;

		// Find the animation set to which the control belongs
		pDragGroupAnimSet = FindReferringElement< CDmeAnimationSet >( pDragElement, symControls );
	}

	// If a control group is in the sub-tree of an animation set it must remain in the sub-tree of that animation set,
	// it is not in the sub-tree of an animation set it must not be added to the sub-tree of any other animation set.	
	if ( pDragGroupAnimSet != pTargetGroupAnimSet )
		return false;

	return true;
}

bool CAnimGroupTree::IsItemDroppable( int nItemIndex, bool bInsertBefore, CUtlVector< KeyValues * >& msglist )
{
	if ( msglist.Count() == 0 )
		return false;
		
	CDmeControlGroup *pParentControlGroup = NULL;
	CDmElement *pTargetElement = GetTreeItemData( nItemIndex, NULL, NULL, &pParentControlGroup );
	if ( pTargetElement == NULL )
		return false;

	CDmeControlGroup *pTargetControlGroup = GetControlGroupForTreeItem( nItemIndex );
	const CDmeControlGroup *pNewParentGroup = bInsertBefore ? pParentControlGroup : pTargetControlGroup;

	// See if there are any messages in the list that will apply to the control group
	int nMsgCount = msglist.Count();
	for ( int iMsg = 0; iMsg < nMsgCount; ++iMsg )
	{		
		KeyValues *pData = msglist[ iMsg ];
		if ( pData == NULL )
			continue;

		if ( pData->FindKey( "color" ) )
		{	
			if ( pTargetControlGroup != NULL )
				return true;
		}

		const CDmElement *pDragElement = GetElementKeyValue< CDmElement >( pData, "dmeelement" );
		if ( CanAddDragIntoGroup( pNewParentGroup, pTargetElement, pDragElement, bInsertBefore ) )
			return true;		
	}

	return false;
}

void CAnimGroupTree::OnItemDropped( int nItemIndex, bool bInsertBefore, CUtlVector< KeyValues * >& msglist )
{
	if ( !IsItemDroppable( nItemIndex, bInsertBefore, msglist ) )
		return;

	CDmeControlGroup *pParentControlGroup = NULL;
	CDmeAnimationSet *pTargetAnimSet = NULL;
	CDmElement *pTargetElement = GetTreeItemData( nItemIndex, NULL, &pTargetAnimSet, &pParentControlGroup );
	if ( pTargetElement == NULL )
		return;

	CDmeControlGroup *pTargetControlGroup = GetControlGroupForTreeItem( nItemIndex );
	CDmeControlGroup *pNewParentGroup = bInsertBefore ? pParentControlGroup : pTargetControlGroup;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Drop onto control group" );

	int nMsgCount = msglist.Count();
	for ( int iMsg = 0; iMsg < nMsgCount; ++iMsg )
	{		
		KeyValues *pData = msglist[ iMsg ];
		if ( pData == NULL )
			continue;

		if ( pData->FindKey( "color" ) )
		{	
			Color clr = pData->GetColor( "color" );
			SetItemFgColor( nItemIndex, clr );
			SetItemSelectionTextColor( nItemIndex, clr );
			pTargetControlGroup->SetGroupColor( clr, false );
		}


		CDmElement *pDragElement = GetElementKeyValue< CDmElement >( pData, "dmeelement" );
		if ( CanAddDragIntoGroup( pNewParentGroup, pTargetElement, pDragElement, bInsertBefore ) )
		{	
			CDmeControlGroup *pDragGroup = CastElement< CDmeControlGroup >( pDragElement );
			if ( pDragGroup )
			{
				pNewParentGroup->AddChild( pDragGroup, ( bInsertBefore ? pTargetControlGroup : NULL ) );
			}
			else
			{
				pNewParentGroup->AddControl( pDragElement, ( bInsertBefore ? pTargetElement : NULL ) );
			}

			if ( pTargetAnimSet )
			{
				CDmeControlGroup *pRootControlGroup = pTargetAnimSet->GetRootControlGroup();
				if ( pRootControlGroup )
				{
					pRootControlGroup->DestroyEmptyChildren();
				}
			}
		}
		
	}
}


void CAnimGroupTree::GetSelectedItemsForDrag( int nPrimaryDragItem, CUtlVector< int >& list )
{	
	// The item actually being dragged will be the first item in the selection list, 
	// since ancestors of this primary item may also be in the selection we can't
	// drag the whole selection, but we do want to drag the siblings of the item.
	// So iterate the selection and add any siblings of the primary selected item.

	CUtlVector< int > selectedItems;
	GetSelectedItems( selectedItems );

	int nNumSelected = selectedItems.Count();
	if ( nNumSelected <= 0 )
		return;

	AnimTreeItemType_t itemType;
	CDmeControlGroup *pPrimaryParentGroup = NULL;
	GetTreeItemData( nPrimaryDragItem, &itemType, NULL, &pPrimaryParentGroup );

	if ( itemType == ANIMTREE_ITEM_COMPONENT )
		return;

	for ( int i = 0 ; i < nNumSelected; ++i )
	{
		CDmeControlGroup *pParentGroup = NULL;
		GetTreeItemData( selectedItems[ i ], &itemType, NULL, &pParentGroup );
		
		if ( ( pPrimaryParentGroup == pParentGroup ) && ( itemType != ANIMTREE_ITEM_COMPONENT ) )
		{
			list.AddToTail( selectedItems[ i ] );
		}
	}
}

void CAnimGroupTree::GenerateDragDataForItem( int nItemIndex, KeyValues *msg )
{
	AnimTreeItemType_t itemType;
	CDmElement *pElement = GetTreeItemData( nItemIndex, &itemType );
	
	if ( ( pElement ) && ( itemType != ANIMTREE_ITEM_COMPONENT ) )
	{
		msg->SetInt( "dmeelement", pElement->GetHandle() );	
	}
}

bool CAnimGroupTree::CanCurrentlyEditLabel( int nItemIndex ) const
{
	// The item must still be in the selection
	if ( IsItemSelected( nItemIndex ) == false )
		return false;

	// Parents or children of the item may be selected, but siblings must not be selected.
	int nParentIndex = GetItemParent( nItemIndex );
	
	CUtlVector< int > selectedItems;
	GetSelectedItems( selectedItems );

	int nSelectedItems = selectedItems.Count();
	for ( int iItem = 0; iItem < nSelectedItems; ++iItem )
	{
		int nSelectedItem = selectedItems[ iItem ];
		if ( nItemIndex == nSelectedItem )
			continue;

		if ( GetItemParent( nSelectedItem) == nParentIndex )
			return false;
	}

	return true;
}


void CAnimGroupTree::OnLabelChanged( int nItemIndex, char const *pOldString, char const *pNewString )
{
	CUndoScopeGuard undoSg( "Change group label" );

	CDmElement *pElement = GetTreeItemData( nItemIndex );
	CDmeControlGroup *pControlGroup = CastElement< CDmeControlGroup >( pElement );
	CDmeAnimationSet *pAnimationSet = CastElement< CDmeAnimationSet >( pElement );

	if ( pControlGroup )
	{
		CDmeControlGroup *pRootGroup = pControlGroup->FindRootControlGroup();
		if ( pRootGroup && pRootGroup->FindChildByName( pNewString, true ) )
		{
			CUtlVector< DmElementHandle_t > childList;
			pRootGroup->GetAllChildren( childList );
			
			int nIndex = GenerateUniqueNameIndex( pNewString, childList, 0 );
			CFmtStr newName( "%s%d", pNewString, nIndex );
			pControlGroup->SetName( newName.Access() );

			// Force the tree display to update to reflect the modified name
			m_pGroupPanel->RebuildTree( true );
		}
		else
		{
			pControlGroup->SetName( pNewString );
		}
	} 
	else if ( pAnimationSet )
	{
		KeyValues *pMsgKV = new KeyValues( "SetAnimationSetName", "text", pNewString );
		SetElementKeyValue( pMsgKV, "animset", pAnimationSet );
		PostMessage( m_pGroupPanel->GetEditor(), pMsgKV, 0.0f );
	}
}

// override to open a custom context menu on a node being selected and right-clicked
void CAnimGroupTree::GenerateContextMenu( int itemIndex, int x, int y )
{
	PostMessage( GetParent(), new KeyValues( "TreeViewOpenContextMenu", "itemID", itemIndex ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Override the default right click selection behavior so that the 
// clicked item becomes the first selected item.
//-----------------------------------------------------------------------------
void CAnimGroupTree::OnContextMenuSelection( int itemIndex )
{
	// Select the item, if it was already selected 
	// this should make it the first selected item.
	AddSelectedItem( itemIndex, !IsItemSelected( itemIndex ) );
	Assert( GetFirstSelectedItem() == itemIndex );
}


void CAnimGroupTree::OnClearWorkCameraParent()
{
	m_pGroupPanel->m_pController->SetWorkCameraParent( NULL );
}


void CAnimGroupTree::OnResetTransformPivot( int viewCenter )
{
	PostMessage( m_pGroupPanel->GetEditor(), new KeyValues( "ResetTransformPivot", "viewCenter", viewCenter ), 0.0f );
}

void CAnimGroupTree::OnToggleDagLock( KeyValues *pParams )
{
	PostMessage( m_pGroupPanel->GetEditor(), pParams->MakeCopy(), 0.0f );

	// Cause perform layout to be run so that the lock icons 
	// are updated after the message has been processed.
	InvalidateLayout();
}


void CAnimGroupTree::OnSetOverrideParent( KeyValues *pParams )
{	
	PostMessage( m_pGroupPanel->GetEditor(), pParams->MakeCopy(), 0.0f );
	InvalidateLayout();
}


void CAnimGroupTree::OnOpenLockContextMenu( KeyValues *pParams )
{
	PostMessage( m_pGroupPanel->GetEditor(), pParams->MakeCopy(), 0.0f );
}


void CAnimGroupTree::OnMousePressed( MouseCode code )
{
	int mx, my;
	input()->GetCursorPos( mx, my );
	ScreenToLocal( mx, my );
	int idx = FindItemUnderMouse( mx, my );

	// The default tree behavior ignores the width of the item when testing against the mouse. For 
	// the animation set editor only the actual are of the item should be considered under the mouse.
	if ( IsItemIDValid( idx ) )
	{
		int xPos, yPos, width, height;
		GetItemBounds( idx, xPos, yPos, width, height );

		if ( ( mx < xPos ) || ( my < yPos ) || ( mx > ( xPos + width ) ) || ( my > ( yPos + height) ) )
		{
			idx = -1;
		}
	}

	bool ctrl = (input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL));
	if ( !ctrl )
	{
		if ( code == MOUSE_RIGHT )
		{
			PostMessage( GetParent(), new KeyValues( "TreeViewOpenContextMenu", "itemID", idx ), 0.0f );
		}
		else
		{
			BaseClass::OnMousePressed( code );
		}
		return;
	}

	if ( !IsItemIDValid( idx ) || ( mx >= ( 20 + m_nStateColumnWidth ) ) )
	{
		BaseClass::OnMousePressed( code );
		return;
	}

	
	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( GetTreeItemData( idx ) );
	if ( pTransformControl )
	{
		CDmeDag *pDag = pTransformControl->GetDag();
		m_pGroupPanel->m_pController->SetWorkCameraParent( pDag );
	}

}


void CAnimGroupTree::PaintBackground()
{
	BaseClass::PaintBackground();

	if ( m_bStateInterface )
	{
		int nHeight = GetTall();
		vgui::surface()->DrawSetColor( m_StateColumnColor );
		vgui::surface()->DrawFilledRect( 0, 0, m_nStateColumnWidth, nHeight );
	}
}


void CAnimGroupTree::PerformLayout()
{
	BaseClass::PerformLayout();

	for ( int itemID = FirstItem(); itemID != InvalidItemID(); itemID = NextItem( itemID ) )
	{
		CAnimGroupStateIconSet *pIconSet = GetTreeItemStateIconSet( itemID );
		if ( pIconSet == NULL )
			continue;

		int nPosX, nPosY, nWidth, nHeight; 
		if ( GetItemBounds( itemID, nPosX, nPosY, nWidth, nHeight ) )
		{
			pIconSet->UpdateState();
			pIconSet->SetBounds( 0, nPosY, nHeight, nHeight );
			pIconSet->SetVisible( true );
		}
		else
		{
			pIconSet->SetVisible( false );
		}
	}


}

void CAnimGroupTree::OnTick()
{
	BaseClass::OnTick();

	int nWorkCameraParentItem = -1;

	if ( CDmeDag *pWorkCameraParent = m_pGroupPanel->GetWorkCameraParent() )
	{
		for ( int itemID = FirstItem(); itemID != InvalidItemID(); itemID = NextItem( itemID ) )
		{
			CDmeDag *pDag = GetDagForTreeItem( itemID );
			if ( pDag == pWorkCameraParent )
			{
				nWorkCameraParentItem = itemID;
				break;
			}
		}
	}

	m_pWorkCameraParentButton->SetVisible( nWorkCameraParentItem != -1 );
	if ( nWorkCameraParentItem != -1 )
	{
		int x = 0, y = 0, w = 0, h = 0;
		GetItemBounds( nWorkCameraParentItem, x, y, w, h );
		m_pWorkCameraParentButton->SetBounds( 1 + m_nStateColumnWidth, y + 1, h - 2, h - 2 );
		m_pWorkCameraParentButton->ClearImages();
		m_pWorkCameraParentButton->AddImage( m_Images.GetImage( m_StateIconIndices[ STATE_ICON_WORK_CAMERA_PARENT_ACTIVE ] ), 0 );
	}
}



void CAnimGroupTree::RemoveItem( int itemIndex, bool bPromoteChildren, bool bRecursivelyRemove )
{
	// The tree view implementation uses negative item indices to indicate
	// recursion, here we don't care, we just want the current item being removed.
	int nActualIndex = ( itemIndex < 0 ) ? -itemIndex : itemIndex;
	
	CAnimGroupStateIconSet *pIconSet = GetTreeItemStateIconSet( nActualIndex );

	if ( pIconSet )
	{
		delete pIconSet;
	}

	BaseClass::RemoveItem( itemIndex, bPromoteChildren, bRecursivelyRemove );
}

void CAnimGroupTree::RemoveAll()
{
	for ( int itemID = FirstItem(); itemID != InvalidItemID(); itemID = NextItem( itemID ) )
	{
		CAnimGroupStateIconSet *pIconSet = GetTreeItemStateIconSet( itemID );
		if ( pIconSet )
		{
			delete pIconSet;
		}
	}

	BaseClass::RemoveAll();
}


bool CAnimGroupTree::VisibleControlsBelow_R( CDmeControlGroup *pGroup )
{
	// If the group is not visible itself then it 
	// cannot have any visible controls below it.
	if ( !m_pGroupPanel->m_pController->IsControlGroupVisible( pGroup ) )
		return false;

	// The group is visible and has controls itself then it has
	// visible controls below it, no need to go farther.
	if ( pGroup->Controls().Count() > 0 )
		return true;

	// If the group did not have any controls itself, check all of its children recursively, 
	// if any are visible and have controls then there are visible controls below this group.
	const CDmaElementArray< CDmeControlGroup > &children = pGroup->Children();
	int nNumChildren = children.Count();
	for ( int iChild = 0; iChild < nNumChildren; ++iChild )
	{
		CDmeControlGroup *pChild =  children[ iChild ];
		if ( pChild == NULL )
			continue;

		if ( VisibleControlsBelow_R( pChild ) )
			return true;
	}

	return false;
}

void CAnimGroupTree::AddDmeControlGroup( int nParentItemIndex, CDmeAnimationSet *pAnimationSet, CDmeControlGroup *pGroup )
{
	Assert( pGroup );
	Assert( pAnimationSet );

	// Add the sub-groups 
	const CDmaElementArray< CDmeControlGroup > &subGroups = pGroup->Children();
	int nGroups = subGroups.Count();
	for ( int iGroup = 0; iGroup < nGroups; ++iGroup )
	{
		CDmeControlGroup *pSubGroup = subGroups[ iGroup ];
		if ( pSubGroup == NULL )
			continue;

		if ( !VisibleControlsBelow_R( pSubGroup ) )
			continue;

		AddControlGroupToTree( nParentItemIndex, pSubGroup, pGroup, pAnimationSet );
	}

	// Add the controls
	const CDmaElementArray< CDmElement > &controls = pGroup->Controls();
	int nControls = controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		CDmElement *pControl = controls[ iControl ];
		if ( pControl )
		{
			AddControlToTree( nParentItemIndex, pControl, pGroup, pAnimationSet );
		}
	}
}

void CAnimGroupTree::GenerateChildrenOfNode(int itemIndex)
{
	BaseClass::GenerateChildrenOfNode( itemIndex );

	// Make sure the children are only generated once.
	if ( GetNumChildren( itemIndex ) > 0 )
		return;

	if ( itemIndex == GetRootItemIndex() )
	{
		CDmeFilmClip *pFilmClip = m_pGroupPanel->m_pController->GetAnimationSetClip();
		CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
		while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
		{
			AddAnimationSetToTree( pAnimSet );
		}
		return;
	}

	// Get type for item
	KeyValues *kv = GetItemData( itemIndex );
	if ( kv )
	{
		AnimTreeItemType_t itemType = static_cast< AnimTreeItemType_t >( kv->GetInt( "itemType" ) );

		CDmElement *pElement = GetElementKeyValue< CDmElement >( kv, "handle" );
		CDmeAnimationSet *pAnimationSet = CastElement< CDmeAnimationSet >( GetElementKeyValue< CDmElement >( kv, "animset" ) );
		CDmeControlGroup *pControlGroup = CastElement< CDmeControlGroup >( GetElementKeyValue< CDmElement >( kv, "controlgroup" ) );
		TransformComponent_t nComponentFlags = static_cast< TransformComponent_t >( kv->GetInt( "componentFlags" ) );

		switch ( itemType )
		{
		default:
			break;
		case ANIMTREE_ITEM_ANIMSET:
			// Under anim set we get 1-N groups, expandable if they have children or controls
			{
				Assert( pAnimationSet );

				CDmeControlGroup *pRootGroup = pAnimationSet->GetRootControlGroup();
				AddDmeControlGroup( itemIndex, pAnimationSet, pRootGroup );
			}
			break;
		case ANIMTREE_ITEM_GROUP:
			// Under group we have subgroups and controls
			{
				Assert( pAnimationSet );
				Assert( pElement );
				CDmeControlGroup *pGroup = CastElement< CDmeControlGroup >( pElement );
				Assert( pGroup );
				AddDmeControlGroup( itemIndex, pAnimationSet, pGroup );
			}
			break;
		case ANIMTREE_ITEM_CONTROL:
		case ANIMTREE_ITEM_COMPONENT:
			// Under transform controls are the position and rotation components
			{
				Assert( pControlGroup );
				Assert( pAnimationSet );
				Assert( pElement );
				CDmeTransformControl *pTranformControl = CastElement< CDmeTransformControl >( pElement );
				if ( pTranformControl )
				{
					AddTransformComponentsToTree( itemIndex, pTranformControl, pControlGroup, pAnimationSet, nComponentFlags );
				}
			}
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Find the child of the specified item which has the specified element as its
// "handle" value
//-----------------------------------------------------------------------------
int CAnimGroupTree::FindChildItemForElement( int nParentIndex, const CDmElement *pElement, TransformComponent_t nComponentFlags )
{
	int nChildren = GetNumChildren( nParentIndex );
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		int nChildIndex = GetChild( nParentIndex, iChild );

		KeyValues *pItemData = GetItemData( nChildIndex );
		if ( !pItemData )
			continue;

		CDmElement *pItemElement = GetElementKeyValue< CDmElement >( pItemData, "handle" );
		if ( pItemElement == pElement )
		{
			if ( nComponentFlags == 0 )
				return nChildIndex;

			TransformComponent_t nItemComponentFlags = static_cast< TransformComponent_t >( pItemData->GetInt( "componentFlags" ) );
			if ( nItemComponentFlags == nComponentFlags )
				return nChildIndex;

			if ( ( nItemComponentFlags & nComponentFlags ) == nComponentFlags )
			{
				GenerateChildrenOfNode( nChildIndex );
				int nSubChildIndex = FindChildItemForElement( nChildIndex, pElement, nComponentFlags );
				if ( nSubChildIndex >= 0 )
					return nChildIndex;
			}
		
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Find the index of the the tree view item that has the specified element as
// its "handle" value
//-----------------------------------------------------------------------------
int CAnimGroupTree::FindItemForElement( const CDmElement *pElement, TransformComponent_t nComponentFlags )
{
	int highest = GetHighestItemID();
	for ( int i = 0; i < highest; ++i )
	{
		if ( !IsItemIDValid( i ) )
			continue;

		KeyValues *pItemData = GetItemData( i );
		if ( !pItemData )
			continue;

		CDmElement *pItemElement = GetElementKeyValue< CDmElement >( pItemData, "handle" );
		if ( pItemElement == pElement )
		{	
			TransformComponent_t nItemComponentFlags = static_cast< TransformComponent_t >( pItemData->GetInt( "componentFlags" ) );
			if ( ( nItemComponentFlags == nComponentFlags ) || ( nComponentFlags == 0 ) )
				return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Build the tree view items from the root down to the specified animation set 
//-----------------------------------------------------------------------------
int CAnimGroupTree::BuildTreeToAnimationSet( CDmeAnimationSet *pAnimationSet )
{
	if ( pAnimationSet == NULL )
		return -1;

	// Check to see if the animation set is already has an item, if so just return that.
	int nItemIndex = FindItemForElement( pAnimationSet );

	// If the item was not found find or create the item for the animation set group containing
	// the animation set and then create all its children, including the animation set being looked for.
	if ( nItemIndex < 0 )
	{	
		int nParentItemIndex = GetRootItemIndex();
		GenerateChildrenOfNode( nParentItemIndex );
		nItemIndex = FindChildItemForElement( nParentItemIndex, pAnimationSet );	
	}
	
	return nItemIndex;
}


//-----------------------------------------------------------------------------
// Build the tree view items from the root down to the specified control group
//-----------------------------------------------------------------------------
int CAnimGroupTree::BuildTreeToGroup( CDmeControlGroup *pGroup, CDmeAnimationSet *pAnimationSet )
{	
	if ( ( pGroup == NULL ) || ( pAnimationSet == NULL ) )
		return -1;

	// Check to see if the group is already has an item, if so just return that.
	int nItemIndex = FindItemForElement( pGroup );

	if ( nItemIndex < 0 )
	{
		int nParentItemIndex = -1;

		if ( pAnimationSet->GetRootControlGroup() == pGroup )
		{
			nParentItemIndex = BuildTreeToAnimationSet( pAnimationSet );
			return nParentItemIndex;
		}
		else
		{
			CDmeControlGroup* pParentGroup = pGroup->FindParent();
			nParentItemIndex = BuildTreeToGroup( pParentGroup, pAnimationSet );
		}
	
		if ( nParentItemIndex >= 0 )
		{	
			GenerateChildrenOfNode( nParentItemIndex );
			nItemIndex = FindChildItemForElement( nParentItemIndex, pGroup );
		}
	}

	return nItemIndex;
}


//-----------------------------------------------------------------------------
// Build the tree view items from the root down to the specified control
//-----------------------------------------------------------------------------
void CAnimGroupTree::BuildTreeToControl( const CDmElement *pControl, TransformComponent_t nComponentFlags )
{
	if ( pControl == NULL )
		return;

	// Find the animation set to which the control belongs
	CDmeAnimationSet *pAnimationSet = FindAncestorReferencingElement< CDmeAnimationSet >( pControl );
	if ( pAnimationSet == NULL )
		return;

	// Check to see if the control is already has an item, if so just return that.
	int nItemIndex = FindItemForElement( pControl, nComponentFlags );

	// If the item for the control was not found, find the group containing the control
	// and make sure it has an item in the tree and the generate the children for that
	// item which will include the item for the control being looked for.
	if ( nItemIndex < 0 )
	{
		int nParentItemIndex = -1;
		CDmeControlGroup *pGroup = CDmeControlGroup::FindGroupContainingControl( pControl );
		nParentItemIndex = BuildTreeToGroup( pGroup, pAnimationSet );
		if ( nParentItemIndex >= 0 )
		{
			GenerateChildrenOfNode( nParentItemIndex );
			nItemIndex = FindChildItemForElement( nParentItemIndex, pControl, nComponentFlags );
		}
	}

	if ( nItemIndex >= 0 )
	{
		MakeItemVisible( nItemIndex );
	}
}


//-----------------------------------------------------------------------------
// Set the selection state on the specified item
//-----------------------------------------------------------------------------
void CAnimGroupTree::SetItemSelectionState( int nItemIndex,  SelectionState_t selectionState )
{
	KeyValues *pItemData = GetItemData( nItemIndex );

	if ( pItemData )
	{
		pItemData->SetInt( "selection", ( int )selectionState );
	}
}

//-----------------------------------------------------------------------------
// Get the selection state of the specified item
//-----------------------------------------------------------------------------
SelectionState_t CAnimGroupTree::GetItemSelectionState( int nItemIndex ) const
{
	KeyValues *pItemData = GetItemData( nItemIndex );
	if ( pItemData == NULL) 
		return SEL_EMPTY;
	
	return ( SelectionState_t )( pItemData->GetInt( "selection" ) );
}


//-----------------------------------------------------------------------------
// Get a list of the controls which are fully selected and whose parents are
// not fully selected
//-----------------------------------------------------------------------------
void CAnimGroupTree::GetSelectionRootItems( CUtlVector< int > &rootSelectedItems ) const
{
	CUtlVector< int > selectedItems;
	GetSelectedItems( selectedItems );

	int nNumSelectedItems = selectedItems.Count();
	rootSelectedItems.EnsureCapacity( nNumSelectedItems );

	for ( int iItem = 0; iItem < nNumSelectedItems; ++iItem )
	{		
		int nItemIndex = selectedItems[ iItem ];

		// Skip any items which are not fully selected
		if ( GetItemSelectionState( nItemIndex ) != SEL_ALL )
			continue;
		
		// Skip any items whose parent is fully selected
		int nParentIndex = GetItemParent( nItemIndex );
		if ( GetItemSelectionState( nParentIndex ) == SEL_ALL )
			continue;
		
		rootSelectedItems.AddToTail( nItemIndex );
	}
}


CBaseAnimSetControlGroupPanel::CBaseAnimSetControlGroupPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor, bool bControlStateInterface ) :
	BaseClass( parent, className ),
	m_pController( NULL )
{
	m_hEditor = editor;
	m_pController = editor->GetController();
	m_pController->AddControlSelectionChangedListener( this );

	m_hGroups = SETUP_PANEL( new CAnimGroupTree( this, "AnimSetGroups", this, bControlStateInterface ) );
	m_hGroups->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	m_hGroups->SetAllowMultipleSelections( true );
	m_hGroups->AddItem( new KeyValues( "root" ), -1 ); // add (invisible) root
}

CBaseAnimSetControlGroupPanel::~CBaseAnimSetControlGroupPanel()
{
}

void CBaseAnimSetControlGroupPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_FullSelectionColor		= pScheme->GetColor( "AnimSet.FullSelectionColor"		, Color( 128, 128, 128, 128 ) );
	m_PartialSelectionColor		= pScheme->GetColor( "AnimSet.PartialSelectionColor"	, Color( 128, 128, 64, 64 ) );
	m_ContextMenuHighlightColor = pScheme->GetColor( "AnimSet.ContextMenuSelectionColor", Color( 255, 153, 0, 255 ) );

	// Normally we shouldn't have to call this but ApplySchemeSettings is called directly 
	// by the animation set editor, so normal solve traverse isn't performed in that case.
	m_hGroups->ApplySchemeSettings( pScheme );
}

void CBaseAnimSetControlGroupPanel::SelectAnimTreeItem( int itemIndex, ESelectionMode selectionMode )
{
	AnimTreeItemType_t itemType;
	CDmElement *pElement = m_hGroups->GetTreeItemData( itemIndex, &itemType );
	if ( !pElement )
		return;

	switch ( itemType )
	{
	case ANIMTREE_ITEM_ANIMSET:
		if ( CDmeAnimationSet *pAnimSet = CastElement< CDmeAnimationSet >( pElement ) )
		{
			m_pController->SelectAnimationSet( pAnimSet, selectionMode );
		}
		break;
	case ANIMTREE_ITEM_GROUP:
		if ( CDmeControlGroup *pGroup = CastElement< CDmeControlGroup >( pElement ) )
		{
			m_pController->SelectControlGroup( pGroup, selectionMode );
		}
		break;
	case ANIMTREE_ITEM_CONTROL:
		m_pController->SelectControl( pElement, selectionMode );
		break;
	case ANIMTREE_ITEM_COMPONENT:
		TransformComponent_t nComponentFlags = m_hGroups->GetItemComponentFlags( itemIndex );
		m_pController->SelectControl( pElement, selectionMode, nComponentFlags );
		break;
	}
}

void CBaseAnimSetControlGroupPanel::OnControlSelectionChanged()
{
	UpdateSelection();
}

void CBaseAnimSetControlGroupPanel::ExpandTreeToControl( const CDmElement *pSelection, TransformComponent_t nComponentFlags )
{
	m_hGroups->BuildTreeToControl( pSelection, nComponentFlags );	
}

void CBaseAnimSetControlGroupPanel::OnTreeViewStartRangeSelection()
{
	m_pController->SetRangeSelectionState( true );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewFinishRangeSelection()
{
	m_pController->SetRangeSelectionState( false );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewItemSelected( int itemIndex, int replaceSelection )
{
	SelectAnimTreeItem( itemIndex, replaceSelection ? SELECTION_SET : SELECTION_ADD );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewItemDeselected( int itemIndex )
{
	SelectAnimTreeItem( itemIndex, SELECTION_REMOVE );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewItemSelectionCleared()
{
	m_pController->ClearSelection();
}

void CBaseAnimSetControlGroupPanel::ChangeAnimationSetClip( CDmeFilmClip *pFilmClip )
{
	RebuildTree( false );
}

void CBaseAnimSetControlGroupPanel::OnControlsAddedOrRemoved()
{
	RebuildTree( true );
}

//-----------------------------------------------------------------------------
// Purpose: Rebuild the tree view from the current control selection hierarchy
//-----------------------------------------------------------------------------
void CBaseAnimSetControlGroupPanel::RebuildTree( bool bRestoreExpansion )
{
	if ( bRestoreExpansion )
	{
		CUtlVector< ElementExpansion_t > expandedNodes( 0, m_hGroups->GetItemCount() );
		CollectExpandedItems( expandedNodes, m_hGroups->GetRootItemIndex() );

		m_hGroups->SetSilentMode( true );
		m_hGroups->RemoveAll();
		m_hGroups->SetSilentMode( false );
		m_hGroups->AddItem( new KeyValues( "root" ), -1 ); // add (invisible) root
		m_hGroups->ExpandItem( m_hGroups->GetRootItemIndex(), true );

		ExpandItems( expandedNodes );

		UpdateSelection();
	}
	else
	{
		m_hGroups->SetSilentMode( true );
		m_hGroups->RemoveAll();
		m_hGroups->SetSilentMode( false );
		m_hGroups->AddItem( new KeyValues( "root" ), -1 ); // add (invisible) root

		if ( CDmeFilmClip *pFilmClip = m_pController->GetAnimationSetClip() )
		{
			CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
			while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
			{
				m_hGroups->AddAnimationSetToTree( pAnimSet );
			}
			m_hGroups->ExpandItem( m_hGroups->GetRootItemIndex(), true );
		}
	}
}

// pre-order traversal so that we can ExpandItems linearly
void CBaseAnimSetControlGroupPanel::CollectExpandedItems( CUtlVector< ElementExpansion_t > &expandedNodes, int nItemIndex )
{
	if ( nItemIndex == m_hGroups->InvalidItemID() )
		return;

	AnimTreeItemType_t itemType;
	CDmElement *pElement = m_hGroups->GetTreeItemData( nItemIndex, &itemType );

	if ( !m_hGroups->IsItemExpanded( nItemIndex ) )
		return;

	ElementExpansion_t *pExpansionInfo = NULL;
	if ( pElement )
	{
		int nIndex = expandedNodes.AddToTail();
		pExpansionInfo = &expandedNodes[ nIndex ];
		pExpansionInfo->m_pElement = pElement;
		pExpansionInfo->m_ComponentFlags = TRANSFORM_COMPONENT_NONE;
	}

	int nChildren = m_hGroups->GetNumChildren( nItemIndex );
	for ( int i = 0; i < nChildren; ++i )
	{
		int nChildIndex = m_hGroups->GetChild( nItemIndex, i );
		
		AnimTreeItemType_t childItemType;
		m_hGroups->GetTreeItemData( nChildIndex, &childItemType );
		
		if ( childItemType == ANIMTREE_ITEM_COMPONENT )
		{
			if ( m_hGroups->IsItemExpanded( nChildIndex ) && pExpansionInfo )
			{
				pExpansionInfo->m_ComponentFlags |= m_hGroups->GetItemComponentFlags( nChildIndex );
			}
		}
		else
		{
			CollectExpandedItems( expandedNodes, nChildIndex );
		}

	}
}

// assumes expandedNodes have parents before children (ie expandedNodes is a pre-order traversal)
void CBaseAnimSetControlGroupPanel::ExpandItems( const CUtlVector< ElementExpansion_t > &expandedNodes )
{
	int nExpandedNodes = expandedNodes.Count();
	for ( int i = 0; i < nExpandedNodes; ++i )
	{
		CDmElement *pElement = expandedNodes[ i ].m_pElement;
		int nItemIndex = m_hGroups->FindItemForElement( pElement );
		if ( nItemIndex != m_hGroups->InvalidItemID() )
		{
			m_hGroups->ExpandItem( nItemIndex, true );

			TransformComponent_t expandedComponents = expandedNodes[ i ].m_ComponentFlags;
			if ( expandedComponents == TRANSFORM_COMPONENT_NONE )
				continue;

			int nChildren = m_hGroups->GetNumChildren( nItemIndex );
			for ( int i = 0; i < nChildren; ++i )
			{
				int nChildIndex = m_hGroups->GetChild( nItemIndex, i );

				AnimTreeItemType_t childItemType;
				m_hGroups->GetTreeItemData( nChildIndex, &childItemType );
				TransformComponent_t childComponents = m_hGroups->GetItemComponentFlags( nChildIndex );
				if ( ( childItemType == ANIMTREE_ITEM_COMPONENT ) && ( childComponents & expandedComponents ) )
				{
					m_hGroups->ExpandItem( nChildIndex, true );
				}
			}
		}
	}
}

void CBaseAnimSetControlGroupPanel::UpdateSelection()
{
	if ( !m_hGroups )
		return;

	m_hGroups->SetSilentMode( true );
	m_hGroups->ClearSelection();
	m_hGroups->SetSilentMode( false );
	UpdateSelection_R( m_hGroups->GetRootItemIndex() );
}

SelectionState_t CBaseAnimSetControlGroupPanel::UpdateSelection_R( int nParentIndex )
{
	if ( nParentIndex == m_hGroups->InvalidItemID() )
		return SEL_EMPTY;

	SelectionState_t selection = SEL_EMPTY;

	int nChildren = m_hGroups->GetNumChildren( nParentIndex );
	if ( nChildren > 0 )
	{
		for ( int i = 0; i < nChildren; ++i )
		{
			int nChildIndex = m_hGroups->GetChild( nParentIndex, i );
			selection += UpdateSelection_R( nChildIndex );
		}
	}
	else
	{
		// check actual controls
		AnimTreeItemType_t itemType;
		CDmElement *pElement = m_hGroups->GetTreeItemData( nParentIndex, &itemType );
		if ( !pElement )
			return SEL_EMPTY;

		switch ( itemType )
		{
		case ANIMTREE_ITEM_ANIMSET:
			if ( CDmeAnimationSet *pAnimSet = CastElement< CDmeAnimationSet >( pElement ) )
			{
				selection = m_pController->GetSelectionState( pAnimSet );
			}
			break;
		case ANIMTREE_ITEM_GROUP:
			if ( CDmeControlGroup *pGroup = CastElement< CDmeControlGroup >( pElement ) )
			{
				selection = m_pController->GetSelectionState( pGroup );
			}
			break;
		case ANIMTREE_ITEM_CONTROL:
			selection = m_pController->GetSelectionState( pElement );
			break;
		case ANIMTREE_ITEM_COMPONENT:
			TransformComponent_t nComponentFlags = m_hGroups->GetItemComponentFlags( nParentIndex );
			selection = m_pController->GetSelectionState( pElement, nComponentFlags );
			break;
		}
	}

	m_hGroups->SetItemSelectionState( nParentIndex, selection );

	if ( selection == SEL_SOME || selection == SEL_ALL )
	{
		Color color = ( selection == SEL_ALL ) ? m_FullSelectionColor : m_PartialSelectionColor;
		m_hGroups->SetItemSelectionBgColor( nParentIndex, color );
		m_hGroups->SetItemSelectionUnfocusedBgColor( nParentIndex, color );

		m_hGroups->SetSilentMode( true );
		m_hGroups->AddSelectedItem( nParentIndex, false, false, false );
		m_hGroups->SetSilentMode( false );
	}

	return selection;
}


CDmeDag *CBaseAnimSetControlGroupPanel::GetWorkCameraParent()
{
	return m_pController->GetWorkCameraParent();
}




//-----------------------------------------------------------------------------
// Purpose: Handle the request of the tree view to create a context menu
//-----------------------------------------------------------------------------
void CBaseAnimSetControlGroupPanel::OnTreeViewOpenContextMenu( int itemID )
{
	if ( itemID >= 0 )
	{
		m_hGroups->SetItemSelectionUnfocusedBgColor( itemID, m_ContextMenuHighlightColor );
	}

	KeyValues *pItemData = m_hGroups->GetItemData( itemID );
	m_hEditor->OpenTreeViewContextMenu( pItemData );
}


//-----------------------------------------------------------------------------
// Create a new control group containing the selected controls
//-----------------------------------------------------------------------------
void CBaseAnimSetControlGroupPanel::CreateGroupFromSelectedControls()
{
	CUtlVector< int > selectedItems;
	m_hGroups->GetSelectionRootItems( selectedItems );

	int nNumSelectedItems = selectedItems.Count();
	if ( nNumSelectedItems < 0 )
		return;

	CDmeControlGroup *pCommonAncestor = NULL;

	for ( int iItem = 0; iItem < nNumSelectedItems; ++iItem )
	{		
		int nItemIndex = selectedItems[ iItem ];

		AnimTreeItemType_t itemType;
		CDmeControlGroup *pParentControlGroup = NULL;
		m_hGroups->GetTreeItemData( nItemIndex, &itemType, NULL, &pParentControlGroup );

		// Currently not allowed to group animation sets
		if ( itemType == ANIMTREE_ITEM_ANIMSET )
			return; 

		if ( pCommonAncestor )
		{
			pCommonAncestor = pParentControlGroup->FindCommonAncestor( pCommonAncestor );
		}
		else
		{
			pCommonAncestor = pParentControlGroup;
		}

		// If any of the selected items do not have a common
		// ancestor, do not allow the group to be created.
		if ( pCommonAncestor == NULL )
			return;
	}

	// If the selected items did not share a common ancestor a new group cannot be created
	if ( pCommonAncestor == NULL )
		return;

	// Generate a name for the new group which is unique among the children of the group it will belong to.
	const CDmeControlGroup *pRootGroup = pCommonAncestor->FindRootControlGroup();
	if ( pRootGroup == NULL)
		return;

	CUtlVector< DmElementHandle_t > childList;
	pRootGroup->GetAllChildren( childList );
	
	int nIndex = GenerateUniqueNameIndex( "group", childList, 1 );
	CFmtStr groupName( "group%d", nIndex );

	// Create the new group and make it a child of the common ancestor
	CDmeControlGroup *pNewGroup = pCommonAncestor->CreateControlGroup( groupName );

	// Add the selected items to the new group
	if ( pNewGroup )
	{			
		for ( int iItem = 0; iItem < nNumSelectedItems; ++iItem )
		{		
			int nItemIndex = selectedItems[ iItem ];
			
			AnimTreeItemType_t itemType;
			CDmElement *pElement = m_hGroups->GetTreeItemData( nItemIndex, &itemType );

			if ( itemType == ANIMTREE_ITEM_GROUP )
			{					
				pNewGroup->AddChild( CastElement< CDmeControlGroup >( pElement ) );
			}
			else if ( itemType == ANIMTREE_ITEM_CONTROL )
			{
				pNewGroup->AddControl( pElement );
			}
		}
	
		// Clean up any empty children which may have been left in the tree.
		pCommonAncestor->DestroyEmptyChildren();
	
		// Rebuild the tree so that it includes the new group
		RebuildTree( true );

		// Find the item corresponding to the new group and set it label editing mode.
		int nNewItemIndex = m_hGroups->FindItemForElement( pNewGroup );
		m_hGroups->MakeItemVisible( nNewItemIndex );
		m_hGroups->StartEditingLabel( nNewItemIndex );
	}		
}

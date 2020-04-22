//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "FilterControl.h"
#include "ControlBarIDs.h"
#include "MapWorld.h"
#include "GlobalFunctions.h"
#include "EditGroups.h"
#include "CustomMessages.h"
#include "VisGroup.h"
#include "Selection.h"
#include "strdlg.h"
#include "toolmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


typedef struct
{
	CVisGroup *pGroup;
	CMapDoc *pDoc;
	SelectMode_t eSelectMode;
} MARKMEMBERSINFO;


static const unsigned int g_uToggleStateMsg = ::RegisterWindowMessage(TREELIST_MSG_TOGGLE_STATE);
static const unsigned int g_uLeftDragDropMsg = ::RegisterWindowMessage(TREELIST_MSG_LEFT_DRAG_DROP);
static const unsigned int g_uRightDragDropMsg = ::RegisterWindowMessage(TREELIST_MSG_RIGHT_DRAG_DROP);
static const unsigned int g_uSelChangeMsg = ::RegisterWindowMessage(TREELIST_MSG_SEL_CHANGE);
static const unsigned int g_uKeyDownMsg = ::RegisterWindowMessage(TREELIST_MSG_KEY_DOWN);


BEGIN_MESSAGE_MAP(CFilterControl, CHammerBar)
	//{{AFX_MSG_MAP(CFilterControl)
	ON_BN_CLICKED(IDC_EDITGROUPS, OnEditGroups)
	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_MARKMEMBERS, OnMarkMembers)
	ON_BN_CLICKED(IDC_SHOW_ALL, OnShowAllGroups)
	ON_COMMAND_EX(IDC_VISGROUP_MOVEUP, OnMoveUpDown)
	ON_COMMAND_EX(IDC_VISGROUP_MOVEDOWN, OnMoveUpDown)
	ON_UPDATE_COMMAND_UI(IDC_GROUPS, UpdateControlGroups)
	ON_UPDATE_COMMAND_UI(IDC_CORDONS, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_NEW, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_EDITGROUPS, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_MARKMEMBERS, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_SHOW_ALL, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_VISGROUP_MOVEUP, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_VISGROUP_MOVEDOWN, UpdateControl)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelChangeTab)
	ON_WM_ACTIVATE()
	ON_WM_SHOWWINDOW()
	ON_WM_SIZE()
	ON_WM_WINDOWPOSCHANGED()
	ON_REGISTERED_MESSAGE(g_uToggleStateMsg, OnListToggleState)
	ON_REGISTERED_MESSAGE(g_uLeftDragDropMsg, OnListLeftDragDrop)
	ON_REGISTERED_MESSAGE(g_uRightDragDropMsg, OnListRightDragDrop)	
	ON_REGISTERED_MESSAGE(g_uSelChangeMsg, OnListSelChange)	
	ON_REGISTERED_MESSAGE(g_uKeyDownMsg, OnListKeyDown)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParentWnd - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFilterControl::Create(CWnd *pParentWnd)
{
	if (!CHammerBar::Create(pParentWnd, IDD_FILTERCONTROL, CBRS_RIGHT | CBRS_SIZE_DYNAMIC, IDCB_FILTERCONTROL, "Filter Control"))
		return FALSE;

	m_cGroupBox.SubclassDlgItem(IDC_GROUPS, this);	
	m_cGroupBox.EnableChecks();

	m_cCordonBox.SubclassDlgItem(IDC_CORDONS, this);
	m_cCordonBox.EnableChecks();

	m_cTabControl.SubclassDlgItem(IDC_TAB1, this);
	m_cTabControl.InsertItem(0, "User");
	m_cTabControl.InsertItem(1, "Auto");
	m_cTabControl.InsertItem(2, "Cordon");

	//
	// Set up button icons.
	//
	CWinApp *pApp = AfxGetApp();
	HICON hIcon = pApp->LoadIcon(IDI_MOVE_UP);
	((CButton *)GetDlgItem(IDC_VISGROUP_MOVEUP))->SetIcon(hIcon);

	hIcon = pApp->LoadIcon(IDI_MOVE_DOWN);
	((CButton *)GetDlgItem(IDC_VISGROUP_MOVEDOWN))->SetIcon(hIcon);

    AddControl( IDC_GROUPS, GROUP_BOX );
    AddControl( IDC_CORDONS, GROUP_BOX );
	AddControl( IDC_VISGROUP_MOVEUP, BOTTOM_JUSTIFY );
	AddControl( IDC_VISGROUP_MOVEDOWN, BOTTOM_JUSTIFY );
	AddControl( IDC_SHOW_ALL, BOTTOM_JUSTIFY );
	AddControl( IDC_EDITGROUPS, BOTTOM_JUSTIFY );
	AddControl( IDC_NEW, BOTTOM_JUSTIFY );
	AddControl( IDC_DELETE, BOTTOM_JUSTIFY );
	AddControl( IDC_MARKMEMBERS, BOTTOM_JUSTIFY );
	AddControl( IDC_TAB1, GROUP_BOX );

	// Add all the VisGroups to the list.
	UpdateGroupList();

	m_bInitialized = true;

	ChangeMode( FILTER_DIALOG_NONE,  FILTER_DIALOG_USER_VISGROUPS );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nLength - 
//			dwMode - 
// Output : CSize
//-----------------------------------------------------------------------------
CSize CFilterControl::CalcDynamicLayout(int nLength, DWORD dwMode)
{
	// TODO: make larger / resizable when floating
	//if (IsFloating())
	//{
	//	return CSize(200, 300);
	//}

	return CHammerBar::CalcDynamicLayout(nLength, dwMode);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CFilterControl::OnSize(UINT nType, int cx, int cy)
{
	// TODO: make larger / resizable when floating
	//if (IsFloating())
	//{
	//	CWnd *pwnd = GetDlgItem(IDC_GROUPS);
	//	if (pwnd && IsWindow(pwnd->GetSafeHwnd()))
	//	{
	//		pwnd->MoveWindow(2, 10, cx - 2, cy - 2, TRUE);
	//	}
	//}

	CHammerBar::OnSize(nType, cx, cy);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::UpdateGroupList()
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc == NULL)
	{
		m_cGroupBox.DeleteAllItems();
		return;
	}

	m_cGroupBox.SaveTreeListExpandStates();

	CVisGroup *pVisGroup = m_cGroupBox.GetSelectedVisGroup();

	m_cGroupBox.SetRedraw(false);
	m_cGroupBox.DeleteAllItems();

	int nCount = pDoc->VisGroups_GetRootCount();

	for (int i = 0; i < nCount; i++)
	{
		CVisGroup *pGroup = pDoc->VisGroups_GetRootVisGroup(i);
		bool bIsAutoGroup = ( strcmp( pGroup->GetName(), "Auto" ) == 0 );
		if ( bIsAutoGroup == ( m_mode == FILTER_DIALOG_AUTO_VISGROUPS ) )
		{
			m_cGroupBox.AddVisGroup(pGroup);
		}
	}

	UpdateGroupListChecks();		

	if (pVisGroup)
	{
		m_cGroupBox.EnsureVisible(pVisGroup);
		m_cGroupBox.SelectItem(pVisGroup);
	}

	m_cGroupBox.RestoreTreeListExpandStates();
	m_cGroupBox.SetRedraw(true);
	m_cGroupBox.Invalidate();
}


//-----------------------------------------------------------------------------
// Refreshes the list and selects the given cordon or cordon box, if specified.
// If NULL is passed in for those, it tries to keep the selection as close to
// what it was before refreshing the list as it can.
//-----------------------------------------------------------------------------
void CFilterControl::UpdateCordonList( Cordon_t *pSelectCordon, BoundBox *pSelectBox )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc == NULL )
	{
		m_cCordonBox.DeleteAllItems();
		return;
	}

	m_cCordonBox.SetRedraw( false );

	// CORDON TODO: need a new scheme for saving/loading expand states because we free and reallocate the
	// list items every time, which breaks our pointer-based save scheme
	//m_cCordonBox.SaveTreeListExpandStates();

	int nSelectedItem = -1;
	if ( pSelectCordon == NULL )
	{
		nSelectedItem = m_cCordonBox.GetSelectedIndex();
	}
	
	// Free up the current list contents.
	for ( int i = 0; i < m_cCordonBox.GetItemCount(); i++ )
	{
		CordonListItem_t *item = m_cCordonBox.GetCordon( i );
		delete item;
	}
	m_cCordonBox.DeleteAllItems();

	CordonListItem_t *pSelectItem = NULL;
	int nCount = pDoc->Cordon_GetCount();
	for ( int i = 0; i < nCount; i++ )
	{
		Cordon_t *pCordon = pDoc->Cordon_GetCordon( i );

		// Add the cordon to the list
		CordonListItem_t *pItem = new CordonListItem_t;
		pItem->m_pCordon = pCordon;
		pItem->m_pBox = NULL;
		m_cCordonBox.AddCordon( pItem, NULL );

		if ( pSelectCordon && ( pSelectCordon == pCordon ) && !pSelectBox )
		{
			pSelectItem = pItem;
		}

		// Add its boxes to the list
		for ( int j = 0; j < pCordon->m_Boxes.Count(); j++ )
		{
			CordonListItem_t *pBoxItem = new CordonListItem_t;
			pBoxItem->m_pCordon = pCordon;
			pBoxItem->m_pBox = &pCordon->m_Boxes.Element( j );
			m_cCordonBox.AddCordon( pBoxItem, pItem ); 

			if ( pSelectCordon && ( pSelectCordon == pCordon ) && pSelectBox && ( pSelectBox == pBoxItem->m_pBox ) )
			{
				pSelectItem = pBoxItem;
			}
		}
	}

	UpdateCordonListChecks();

	// The selected item might be gone, so select by index to get the next closest item.
	if ( nSelectedItem != -1 )
	{
		m_cCordonBox.EnsureVisible( nSelectedItem );
		m_cCordonBox.SelectNearestItem( nSelectedItem );
	}
	else if ( pSelectItem )
	{
		// The caller passed in a cordon or cordon + box to select.
		m_cCordonBox.EnsureVisible( pSelectItem );
		m_cCordonBox.SelectItem( pSelectItem );
	}

	//m_cCordonBox.RestoreTreeListExpandStates();
	m_cCordonBox.ExpandAll();
	
	m_cCordonBox.SetRedraw( true );
	m_cCordonBox.Invalidate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::SelectCordon( Cordon_t *pSelectCordon, BoundBox *pSelectBox )
{
	for ( int i = 0; i < m_cCordonBox.GetItemCount(); i++ )
	{
		CordonListItem_t *pItem = m_cCordonBox.GetCordon( i );
		if ( ( pItem->m_pCordon == pSelectCordon ) && ( pItem->m_pBox == pSelectBox ) )
		{
			m_cCordonBox.EnsureVisible( pItem );
			m_cCordonBox.SelectItem( pItem );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::UpdateList()
{
	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		UpdateCordonList();
	}
	else
	{
		UpdateGroupList();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::UpdateControl(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetActiveWorld() ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Disables the group list when there's no active world or when the
//			visgroups are currently overridden by the "Show All" button.
//-----------------------------------------------------------------------------
void CFilterControl::UpdateControlGroups(CCmdUI *pCmdUI)
{
	pCmdUI->Enable((GetActiveWorld() != NULL) && !CVisGroup::IsShowAllActive());
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTarget - 
//			bDisableIfNoHndler - 
//-----------------------------------------------------------------------------
void CFilterControl::OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler)
{
	UpdateDialogControls(pTarget, FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::OnShowAllGroups(void)
{
	CButton *pButton = (CButton *)GetDlgItem(IDC_SHOW_ALL);
	if (pButton != NULL)
	{
		UINT uCheck = pButton->GetCheck();
		CVisGroup::ShowAllVisGroups(uCheck == 1);

		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		pDoc->UpdateVisibilityAll();

		UpdateGroupListChecks();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFilterControl::OnMoveUpDown(UINT uCmd)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
	{
		return TRUE;
	}

	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		CordonListItem_t *cordon = m_cCordonBox.GetSelectedCordon();
		if ( cordon == NULL )
		{
			return TRUE;
		}

		if ( uCmd == IDC_VISGROUP_MOVEUP )
		{
			pDoc->Cordon_MoveUp( cordon->m_pCordon );
		}
		else
		{
			pDoc->Cordon_MoveDown( cordon->m_pCordon );
		}

		UpdateCordonList();

		m_cCordonBox.EnsureVisible( cordon );
		m_cCordonBox.SelectItem( cordon );
	}
	else
	{	
		CVisGroup *pVisGroup = m_cGroupBox.GetSelectedVisGroup();
		if (pVisGroup == NULL)
		{
			return TRUE;
		}

		if (uCmd == IDC_VISGROUP_MOVEUP)
		{
			pDoc->VisGroups_MoveUp(pVisGroup);
		}
		else
		{
			pDoc->VisGroups_MoveDown(pVisGroup);
		}

		UpdateGroupList();

		m_cGroupBox.EnsureVisible(pVisGroup);
		m_cGroupBox.SelectItem(pVisGroup);
	}
	
	pDoc->SetModifiedFlag();

	return TRUE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::OnEditGroups()
{
	Assert( m_mode != FILTER_DIALOG_CORDONS );
	if ( m_mode == FILTER_DIALOG_CORDONS )
		return;

	CEditGroups dlg;
	dlg.DoModal();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->SetModifiedFlag();
	}

	UpdateGroupList();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::OnNew()
{
	if ( m_mode != FILTER_DIALOG_CORDONS )
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
		return;
		
	pDoc->OnNewCordon();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFilterControl::OnDelete()
{
	if ( m_mode != FILTER_DIALOG_CORDONS )
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
		return;

	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		CordonListItem_t *pItem = m_cCordonBox.GetSelectedCordon();
		if ( pItem )
		{
			DeleteCordonListItem( pItem, true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			pInfo - 
// Output : Returns TRUE to continue enumerating, FALSE to stop.
//-----------------------------------------------------------------------------
static BOOL MarkMembersOfGroup(CMapClass *pObject, MARKMEMBERSINFO *pInfo)
{
	if (pObject->IsInVisGroup(pInfo->pGroup))
	{
		if (!pObject->IsVisible())
		{
			return TRUE;
		}

		CMapClass *pSelectObject = pObject->PrepareSelection(pInfo->eSelectMode);
		if (pSelectObject)
		{
			pInfo->pDoc->SelectObject(pSelectObject, scSelect);
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Selects all objects that belong to the currently selected visgroup.
//-----------------------------------------------------------------------------
void CFilterControl::OnMarkMembers(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	CMapWorld *pWorld = pDoc->GetMapWorld();
	
	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		CordonListItem_t *cordon = m_cCordonBox.GetSelectedCordon();
		if ( cordon )
		{
			pDoc->GetSelection()->SetMode( selectObjects );

			// Clear the selection.
			pDoc->SelectObject( NULL, scClear | scSaveChanges );

			//
			// Select all objects that belong to the visgroup.
			//
			EnumChildrenPos_t pos;
			CMapClass *pChild = pWorld->GetFirstDescendent( pos );
			while ( pChild )
			{
				for ( int i = 0; i < cordon->m_pCordon->m_Boxes.Count(); i++ )
				{
					if ( pChild->IsVisible() &&
						 pChild->IsIntersectingCordon( cordon->m_pCordon->m_Boxes.Element( i ).bmins, cordon->m_pCordon->m_Boxes.Element( i ).bmaxs ) )
					{
						CMapClass *pSelectObject = pChild->PrepareSelection(pDoc->GetSelection()->GetMode() );
						if ( pSelectObject )
						{
							pDoc->SelectObject( pSelectObject, scSelect );
						}
					}
				}
				
				pChild = pWorld->GetNextDescendent( pos );
			}
		}
	}
	else
	{
		CVisGroup *pVisGroup = m_cGroupBox.GetSelectedVisGroup();
		if (pVisGroup)
		{
			pDoc->GetSelection()->SetMode(selectObjects);

			// Clear the selection.
			pDoc->SelectObject(NULL, scClear|scSaveChanges);

			//
			// Select all objects that belong to the visgroup.
			//
			EnumChildrenPos_t pos;
			CMapClass *pChild = pWorld->GetFirstDescendent(pos);
			while (pChild)
			{
				if ( pChild->IsVisible() && pChild->IsInVisGroup(pVisGroup) )
				{
					CMapClass *pSelectObject = pChild->PrepareSelection(pDoc->GetSelection()->GetMode());
					if (pSelectObject)
					{
						pDoc->SelectObject(pSelectObject, scSelect);
					}
				}
				
				pChild = pWorld->GetNextDescendent(pos);
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::OnWindowPosChanged(WINDOWPOS *pPos)
{
	if (m_bInitialized && pPos->flags & SWP_SHOWWINDOW)
	{
		UpdateGroupList();
	}

	CHammerBar::OnWindowPosChanged(pPos);
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (bShow)
	{
		UpdateGroupList();
	}

	CHammerBar::OnShowWindow(bShow, nStatus);
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::OnActivate(UINT nState, CWnd* pWnd, BOOL bMinimized)
{
	if (nState == WA_ACTIVE)
	{
		UpdateGroupList();
	}

	CHammerBar::OnActivate(nState, pWnd, bMinimized);
}


//--------------------------------------------------------------------------------------------------
// Called when the checkbox of a group or cordon is toggled.
//	wParam - Pointer to the item in the list that was toggled, either a CVisGroup * or CordonListItem_t *.
//--------------------------------------------------------------------------------------------------
LRESULT CFilterControl::OnListToggleState(WPARAM wParam, LPARAM lParam)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc == NULL)
		return 0;
		
	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		CordonListItem_t *pItem = (CordonListItem_t *)wParam;
		if ( pItem )
		{
			if ( !pItem->m_pBox )
			{
				pItem->m_pCordon->m_bActive = !pItem->m_pCordon->m_bActive;
			}

			pDoc->UpdateVisibilityAll();
			UpdateCordonListChecks();
		}
	} 
	else	
	{
		//
		// Update the visibility of the group.
		//
		CVisGroup *pVisGroup = (CVisGroup *)wParam;
		if (pVisGroup != NULL)
		{
			pDoc->VisGroups_ShowVisGroup(pVisGroup, pVisGroup->GetVisible() == VISGROUP_HIDDEN);
		}

		UpdateGroupListChecks();
	}

	return 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::DeleteCordonListItem(CordonListItem_t *pDelete, bool bConfirm )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc == NULL)
		return;

	// If removing the last box from a cordon, just nuke the whole cordon
	bool bRemoveCordon = ( !pDelete->m_pBox ) || ( pDelete->m_pCordon->m_Boxes.Count() <= 1 );

	CUtlString str;
	if ( bRemoveCordon )
	{
		// Always confirm when deleting a whole cordon
		str.Format( "Delete cordon '%s'?", pDelete->m_pCordon->m_szName.Get() );
	}
	else if ( bConfirm )
	{
		str.Format( "Delete box from cordon '%s'?", pDelete->m_pCordon->m_szName.Get() );
	}

	if ( bRemoveCordon || bConfirm )
	{
		if ( AfxMessageBox( str, MB_YESNO | MB_ICONQUESTION ) == IDNO )
			return;
	}
	
	if ( bRemoveCordon )
	{
		pDoc->Cordon_RemoveCordon( pDelete->m_pCordon );
	}
	else
	{
		pDoc->Cordon_RemoveBox( pDelete->m_pCordon, pDelete->m_pBox );
	}
	
	UpdateCordonList();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::OnCordonListDragDrop(CordonListItem_t *pDrag, CordonListItem_t *pDrop )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc == NULL)
		return;

	if ( !pDrag || ( pDrag == pDrop ) )
		return;

	if ( pDrop )
	{
		CString str;
		if ( pDrag->m_pBox && ( pDrag->m_pCordon->m_Boxes.Count() > 1 ) )
		{
			str.Format("Merge box from cordon '%s' into cordon '%s'?", pDrag->m_pCordon->m_szName.Get(), pDrop->m_pCordon->m_szName.Get() );
		}
		else
		{
			str.Format("Merge cordon '%s' into cordon '%s'?", pDrag->m_pCordon->m_szName.Get(), pDrop->m_pCordon->m_szName.Get() );
		}
		
		if ( AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION ) == IDNO )
		{
			//UpdateCordonList();
			return;
		}

		pDoc->Cordon_CombineCordons( pDrag->m_pCordon, pDrag->m_pBox, pDrop->m_pCordon );
		UpdateCordonList();
	}
	else
	{
		DeleteCordonListItem( pDrag, true );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::OnVisGroupListDragDrop(CVisGroup *pDragGroup, CVisGroup *pDropGroup )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc == NULL)
		return;

	if (pDropGroup != NULL)
	{
		if (pDragGroup->FindDescendent(pDropGroup))
		{
			CString str;
			str.Format("Cannot combine the groups because '%s' is a sub-group of '%s'.", pDropGroup->GetName(), pDragGroup->GetName());
			AfxMessageBox(str);
			UpdateGroupList();
			return;
		}

		CString str;
		str.Format("Combine group '%s' into group '%s'?", pDragGroup->GetName(), pDropGroup->GetName());
		if (AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO)
		{
			UpdateGroupList();
			return;
		}

		pDoc->VisGroups_CombineGroups(pDragGroup, pDropGroup);
	}
	else
	{
		CString str;
		str.Format("Delete group '%s'?", pDragGroup->GetName());
		if (AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO)
		{
			UpdateGroupList();
			return;
		}

		// Show the visgroup that's being deleted so that member objects are shown.
		pDoc->VisGroups_CheckMemberVisibility(pDragGroup);
		pDoc->VisGroups_RemoveGroup(pDragGroup);
	}

	UpdateGroupList();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
LRESULT CFilterControl::OnListLeftDragDrop(WPARAM wParam, LPARAM lParam)
{
	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		OnCordonListDragDrop( (CordonListItem_t *)wParam, (CordonListItem_t *)lParam );
		return 0;
	}
	
	if ( m_mode == FILTER_DIALOG_AUTO_VISGROUPS )
	{
		UpdateGroupList();
		return 0;
	}

	OnVisGroupListDragDrop( (CVisGroup *)wParam, (CVisGroup *)lParam );
	return 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
LRESULT CFilterControl::OnListRightDragDrop(WPARAM wParam, LPARAM lParam)
{
	// Nothing meaningful to do here for cordons.
	if ( m_mode == FILTER_DIALOG_CORDONS )
		return 0;
	
	if ( m_mode == FILTER_DIALOG_AUTO_VISGROUPS )
	{
		UpdateGroupList();
		return 0;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		CVisGroup *pDragGroup = (CVisGroup *)wParam;
		CVisGroup *pDropGroup = (CVisGroup *)lParam;

		if (pDragGroup->FindDescendent(pDropGroup))
		{
			CString str;
			str.Format("Cannot move the group because '%s' is a sub-group of '%s'.", pDropGroup->GetName(), pDragGroup->GetName());
			AfxMessageBox(str);
			return 0;
		}

		pDoc->VisGroups_SetParent(pDragGroup, pDropGroup);
		UpdateGroupList();
	}

	return 0;
}


//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------
LRESULT CFilterControl::OnListSelChange( WPARAM wParam, LPARAM lParam )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc == NULL )
		return 0;

	if ( wParam == IDC_GROUPS )
	{
		if ( m_mode == FILTER_DIALOG_CORDONS )
			return 0;
 
		// VISGROUPS TODO: this is broken by a couple of things:
		//   1) VisGroups_CanMoveUp returns true because the root Auto visgroup occupies index 0
		//	 2) UpdateControl goes ahead and enables the button right after we disable it
		//
		//CVisGroup *pGroup = m_cGroupBox.GetSelectedVisGroup();
		//if ( pGroup )
		//{
		//	bool bCanMoveUp = pDoc->VisGroups_CanMoveUp( pGroup );
		//	bool bCanMoveDown = pDoc->VisGroups_CanMoveDown( pGroup );
		//	GetDlgItem( IDC_VISGROUP_MOVEUP )->EnableWindow( bCanMoveUp ? TRUE : FALSE );
		//	GetDlgItem( IDC_VISGROUP_MOVEDOWN )->EnableWindow( bCanMoveDown ? TRUE : FALSE );
		//}

		return 0;
	}

	if ( wParam == IDC_CORDONS )
	{
		if ( m_mode != FILTER_DIALOG_CORDONS )
			return 0;

		CordonListItem_t *item = m_cCordonBox.GetSelectedCordon();
		BoundBox *box = item->m_pBox;
		if ( !box && ( item->m_pCordon->m_Boxes.Count() > 0 ) )
		{
			box = &item->m_pCordon->m_Boxes.Element( 0 );
		}
		
		if ( box )
		{
			pDoc->Cordon_SelectCordonForEditing( item->m_pCordon, box, SELECT_CORDON_FROM_DIALOG );
		}
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
LRESULT CFilterControl::OnListKeyDown(WPARAM wParam, LPARAM lParam)
{
	if ( m_mode != FILTER_DIALOG_CORDONS )
		return 0;

	if ( wParam == VK_DELETE )
	{
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if ( pDoc == NULL )
			return 0;

		CordonListItem_t *pSel = m_cCordonBox.GetSelectedCordon();
		if ( pSel )
		{
			DeleteCordonListItem( pSel, false );

			// Popping up the confirmation dialog killed focus from the list
			m_cCordonBox.SetFocus();
		}
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFilterControl::OnInitDialog(void)
{
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::UpdateGroupListChecks(void)
{
	int nCount = m_cGroupBox.GetVisGroupCount();
	for (int i = 0; i < nCount; i++)
	{
		CVisGroup *pVisGroup = m_cGroupBox.GetVisGroup(i);
		if (pVisGroup->GetVisible() == VISGROUP_HIDDEN)
		{
			m_cGroupBox.SetCheck(pVisGroup, 0);
		}
		else if (pVisGroup->GetVisible() == VISGROUP_SHOWN)
		{
			m_cGroupBox.SetCheck(pVisGroup, 1);
		}
		else
		{
			m_cGroupBox.SetCheck(pVisGroup, -1);
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::UpdateCordonListChecks()
{
	int nCount = m_cCordonBox.GetCordonCount();
	for ( int i = 0; i < nCount; i++ )
	{
		CordonListItem_t *item = m_cCordonBox.GetCordon( i );
		if ( !item->m_pBox )
		{
			m_cCordonBox.SetCheck( item, item->m_pCordon->m_bActive ? 1 : 0 );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::OnSelChangeTab(NMHDR *header, LRESULT *result) 
{
	FilterDialogMode_t oldMode = m_mode;
	FilterDialogMode_t newMode;

	int sel = m_cTabControl.GetCurSel(); 
	if ( sel == 0 ) 
	{
		newMode = FILTER_DIALOG_USER_VISGROUPS;
	} 
	else if ( sel == 1 )
	{
		newMode = FILTER_DIALOG_AUTO_VISGROUPS;
	}
	else
	{
		newMode = FILTER_DIALOG_CORDONS;
	}

	ChangeMode( oldMode, newMode );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CFilterControl::ChangeMode( FilterDialogMode_t oldMode,  FilterDialogMode_t newMode )
{
	m_mode = newMode;

	if ( m_mode == FILTER_DIALOG_CORDONS )
	{
		// Cordon controls
		m_cCordonBox.ShowWindow( SW_SHOW );
		GetDlgItem( IDC_NEW )->ShowWindow( SW_SHOW );
		GetDlgItem( IDC_DELETE )->ShowWindow( SW_SHOW );

		// Visgroup controls
		m_cGroupBox.ShowWindow( SW_HIDE );
		GetDlgItem( IDC_EDITGROUPS )->ShowWindow( SW_HIDE );
		GetDlgItem( IDC_SHOW_ALL )->ShowWindow( SW_HIDE );
		
		UpdateCordonList();
	}
	else
	{
		// Cordon controls
		m_cCordonBox.ShowWindow( SW_HIDE );
		GetDlgItem( IDC_NEW )->ShowWindow( SW_HIDE );
		GetDlgItem( IDC_DELETE )->ShowWindow( SW_HIDE );

		// Visgroup controls
		m_cGroupBox.ShowWindow( SW_SHOW );
		GetDlgItem( IDC_EDITGROUPS )->ShowWindow( SW_SHOW );
		GetDlgItem( IDC_SHOW_ALL )->ShowWindow( SW_SHOW );

		UpdateGroupList();
	}
}



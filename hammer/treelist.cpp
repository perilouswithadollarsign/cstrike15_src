//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ====
//
// A tree of checkable items. Can have multiple root-level items. Supports drag and drop
// and posts a registered Windows message to the tree's parent window when items
// are checked, unchecked, dragged & dropped, and when selection changes.
//
//=============================================================================

#include "stdafx.h"
#include "treelist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//
// Timer IDs.
//
enum
{
	TIMER_GROUP_DRAG_SCROLL = 1,
};


// TODO: Make these messages unique per instance so a window can contain more than one of these controls
static const unsigned int g_uToggleStateMsg = ::RegisterWindowMessage(TREELIST_MSG_TOGGLE_STATE);
static const unsigned int g_uLeftDragDropMsg = ::RegisterWindowMessage(TREELIST_MSG_LEFT_DRAG_DROP);
static const unsigned int g_uRightDragDropMsg = ::RegisterWindowMessage(TREELIST_MSG_RIGHT_DRAG_DROP);
static const unsigned int g_uSelChangeMsg = ::RegisterWindowMessage(TREELIST_MSG_SEL_CHANGE);
static const unsigned int g_uKeyDownMsg = ::RegisterWindowMessage(TREELIST_MSG_KEY_DOWN);


BEGIN_MESSAGE_MAP(CTreeList, CTreeCtrl)
	//{{AFX_MSG_MAP(CGroupList)
	ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBegindrag)
	ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, OnEndLabelEdit)
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChange)
	ON_NOTIFY_REFLECT(TVN_KEYDOWN, OnKeyDown)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	ON_WM_CONTEXTMENU()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CTreeList::CTreeList()
{
	m_pDragImageList = NULL;
	m_hDragItem = NULL;
	m_bRButtonDown = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CTreeList::~CTreeList()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::EnableChecks()
{
	if (!m_cNormalImageList.GetSafeHandle())
	{
		// TODO: pass the image list in?
		#define IDB_TREELISTCHECKS 223
		m_cNormalImageList.Create(IDB_TREELISTCHECKS, 16, 1, RGB(255, 255, 255));
		m_cNormalImageList.SetOverlayImage(1, 1);
		m_cNormalImageList.SetOverlayImage(2, 2);
	}

	CTreeCtrl::SetImageList(&m_cNormalImageList, TVSIL_STATE);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::AddItem(void *pItem, void *pParent, const char *pText, bool bHasCheckBox )
{
	HTREEITEM hParent = TVI_ROOT;
	if (pParent)
	{
		// FIXME: recursive lookup for every add is sucky
		hParent = FindHTreeItem(pParent);
	}
	
	HTREEITEM hItem = InsertItem(pText, hParent, TVI_LAST);
	if (hItem != NULL)
	{
		SetItemData(hItem, (DWORD)pItem);
		m_Items.AddToTail(pItem);

		if ( bHasCheckBox )
		{
			SetItemState( hItem, INDEXTOSTATEIMAGEMASK( 1 ), TVIS_STATEIMAGEMASK );
		}
		else
		{
			SetItemState( hItem, INDEXTOSTATEIMAGEMASK( 0 ), TVIS_STATEIMAGEMASK );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void UnsetItemData_R( CTreeCtrl *pCtrl, HTREEITEM hItem )
{
	pCtrl->SetItemData( hItem, 0 );
	
	HTREEITEM hChildItem = pCtrl->GetChildItem( hItem );

	while( hChildItem != NULL )
	{
		UnsetItemData_R( pCtrl, hChildItem );
		hChildItem = pCtrl->GetNextItem(hChildItem, TVGN_NEXT);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::DeleteAllItems()
{
	// Un-set all item data because sometimes during a delete it'll trigger selection change notifications
	// which might crash things later.
	if ( GetSafeHwnd() && m_Items.Count() > 0 )
	{
		UnsetItemData_R( this, TVI_ROOT );
	}
	
	DeleteItem(TVI_ROOT);
	m_Items.RemoveAll();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::EnsureVisible(void *pItem)
{
	//DBG("EnsureVisible: %s\n", pVisGroup->GetName());
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		CTreeCtrl::EnsureVisible(hItem);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::ExpandRecursive(HTREEITEM hItem)
{
	if (hItem)
	{
		Expand(hItem, TVE_EXPAND);

		if (ItemHasChildren(hItem))
		{
			HTREEITEM hChildItem = GetChildItem(hItem);
			while (hChildItem != NULL)
			{
				ExpandRecursive(hChildItem);
				hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::ExpandAll()
{
	HTREEITEM hItem = GetRootItem();
	while (hItem)
	{
		ExpandRecursive(hItem);
		hItem = GetNextItem(hItem, TVGN_NEXT);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::ExpandItem(void *pItem)
{
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		Expand(hItem, TVE_EXPAND);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::CollapseItem(void *pItem)
{
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		Expand(hItem, TVE_COLLAPSE);
	}
}


//-----------------------------------------------------------------------------
// Returns the HTREEITEM in the given subtree associated with the given
// item pointer, NULL if none.
//-----------------------------------------------------------------------------
HTREEITEM CTreeList::FindHTreeItemRecursive(HTREEITEM hItem, void *pItem)
{
	if (hItem)
	{
		void *pItemCheck = (void *)GetItemData(hItem);
		if (pItemCheck == pItem)
		{
			return hItem;
		}

		if (ItemHasChildren(hItem))
		{
			HTREEITEM hChildItem = GetChildItem(hItem);
			while (hChildItem != NULL)
			{
				HTREEITEM hFoundItem = FindHTreeItemRecursive(hChildItem, pItem);
				if (hFoundItem)
				{
					return hFoundItem;
				}

				hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
			}
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Returns the HTREEITEM associated with the given item pointer, NULL if none.
//-----------------------------------------------------------------------------
HTREEITEM CTreeList::FindHTreeItem(void *pItem)
{
	HTREEITEM hItem = GetRootItem();
	while (hItem)
	{
		HTREEITEM hFound = FindHTreeItemRecursive(hItem, pItem);
		if (hFound)
		{
			return hFound;
		}

		hItem = GetNextItem(hItem, TVGN_NEXT);
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void *CTreeList::GetSelectedItem()
{
	HTREEITEM hItem = CTreeCtrl::GetSelectedItem();
	if (hItem)
	{
		return (void *)GetItemData(hItem);
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CTreeList::GetSelectedIndex()
{
	int nItem = -1;

	void *pItem = GetSelectedItem();
	if ( pItem )
	{
		for ( int i = 0; i < m_Items.Count(); i++ )
		{
			if ( m_Items[i] == pItem )
			{
				nItem = i;
				break;
			}
		}
	}

	return nItem;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnLButtonDown(UINT nFlags, CPoint point) 
{
	unsigned int uFlags;
	HTREEITEM hItemHit = HitTest(point, &uFlags);
	if (hItemHit != NULL)
	{
		if (uFlags & TVHT_ONITEMSTATEICON)
		{
			// Don't forward to the base if they clicked on the check box.
			// This prevents undesired expansion/collapse of tree.
			return;
		}
	}

	CTreeCtrl::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnLButtonUp(UINT nFlags, CPoint point) 
{
	KillTimer(TIMER_GROUP_DRAG_SCROLL);
	ReleaseCapture();

	if (!m_hDragItem)
	{
		unsigned int uFlags;
		HTREEITEM hItemHit = HitTest(point, &uFlags);
		if (hItemHit != NULL)
		{
			if (uFlags & TVHT_ONITEMSTATEICON)
			{
				//
				// Notify our parent window that this item's state has changed.
				//
				CWnd *pwndParent = GetParent();
				if (pwndParent != NULL)
				{
					// TODO: might need a way to cycle through three states: on, off, grey
					int nCheckState = GetCheck(hItemHit);
					if (!nCheckState)
					{
						nCheckState = 1;
					}
					else
					{
						nCheckState = 0;
					}

					void *pItem = (void *)GetItemData(hItemHit);
					pwndParent->PostMessage(g_uToggleStateMsg, (WPARAM)pItem, nCheckState);
				}

				// Don't forward to the base if they clicked on the check box.
				// This prevents undesired expansion/collapse of tree.
				return;
			}
		}

		CTreeCtrl::OnLButtonUp(nFlags, point);
		return;
	}

	Drop(DROP_LEFT, nFlags, point);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	unsigned int uFlags;
	HTREEITEM hItemHit = HitTest(point, &uFlags);
	if (hItemHit != NULL)
	{
		if (uFlags & TVHT_ONITEMICON)
		{
			// Don't forward to the base if they clicked on the check box.
			// This prevents undesired expansion/collapse of tree.
			return;
		}
	}

	CTreeCtrl::OnLButtonDblClk(nFlags, point);
}


//-----------------------------------------------------------------------------
// Forwards selection change notifications to our parent window.
//		pNMHDR - 
//		pResult - 
//-----------------------------------------------------------------------------
void CTreeList::OnSelChange(NMHDR *pNMHDR, LRESULT *pResult)
{
	CWnd *pwndParent = GetParent();
	if (pwndParent != NULL)
	{
		pwndParent->PostMessage( g_uSelChangeMsg, (WPARAM)GetDlgCtrlID(), 0 );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnEndLabelEdit(NMHDR *pNMHDR, LRESULT *pResult) 
{
	NMTVDISPINFO *pInfo = (NMTVDISPINFO *)pNMHDR;
	if (!pInfo->item.pszText)
		return;

	void *pItem = (void *)GetItemData(pInfo->item.hItem);
	Assert(pItem);
	if (!pItem)
		return;

	OnRenameItem(pItem, pInfo->item.pszText);

	pResult[0] = TRUE;
}


//-----------------------------------------------------------------------------
// Begins dragging an item in the tree list. The drag image is
// created and anchored relative to the mouse cursor.
//		pNMHDR - 
//		pResult - 
//-----------------------------------------------------------------------------
void CTreeList::OnBegindrag(NMHDR *pNMHDR, LRESULT *pResult) 
{
	NMTREEVIEW *ptv = (NMTREEVIEW *)pNMHDR;
	BeginDrag(ptv->ptDrag, ptv->itemNew.hItem);
	*pResult = 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnKeyDown( NMHDR *pNMHDR, LRESULT *pResult )
{
	NMTVKEYDOWN *pKeyDown = (NMTVKEYDOWN *)pNMHDR;
	CWnd *pwndParent = GetParent();
	if (pwndParent != NULL)
	{
		pwndParent->PostMessage(g_uKeyDownMsg, (WPARAM)pKeyDown->wVKey, (LPARAM)pKeyDown->flags);
	}
	*pResult = 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::BeginDrag(CPoint point, HTREEITEM hItem) 
{
	m_hDragItem = hItem;
	if (m_hDragItem)
	{
		m_pDragImageList = CreateDragImage(m_hDragItem);
		if (m_pDragImageList)
		{
			CPoint ptHotSpot(0, 0);
			m_pDragImageList->BeginDrag(0, ptHotSpot);
			m_pDragImageList->DragEnter(this, point);
			SelectDropTarget(NULL);
		}

		// Timer handles scrolling the list control when dragging outside the window bounds.
		SetTimer(TIMER_GROUP_DRAG_SCROLL, 300, NULL);

		SetCapture();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_bRButtonDown = true;
	m_ptRButtonDown = point;
	m_hDragItem = NULL;
	SetCapture();

	// Chaining to the base class causes us never to receive the button up message
	// for a right click without drag, so we don't do that.
	//CTreeCtrl::OnRButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::OnContextMenu(CWnd *pWnd, CPoint point)
{
	KillTimer(TIMER_GROUP_DRAG_SCROLL);
	ReleaseCapture();

	m_bRButtonDown = false;

	if (!m_hDragItem)
	{
		// TODO: Need to invoke the correct context menu for this tree list. Currently no one uses this.
		CTreeCtrl::OnContextMenu(pWnd, point);
		return;
	}

	Drop(DROP_RIGHT, 0, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CTreeList::OnRButtonUp(UINT nFlags, CPoint point)
{
	KillTimer(TIMER_GROUP_DRAG_SCROLL);
	ReleaseCapture();

	m_bRButtonDown = false;

	if (!m_hDragItem)
	{
		CTreeCtrl::OnRButtonUp(nFlags, point);
		return;
	}

	Drop(DROP_RIGHT, nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eDropType - 
//			nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CTreeList::Drop(DropType_t eDropType, UINT nFlags, CPoint point)
{
	SelectDropTarget(NULL);

	HTREEITEM hDragItem = m_hDragItem;
	m_hDragItem = NULL;

	//
	// We are dragging. Drop!
	//
	if (m_pDragImageList)
	{
		m_pDragImageList->DragLeave(this);
		m_pDragImageList->EndDrag();
		delete m_pDragImageList;
		m_pDragImageList = NULL;
	}

	//
	// Get the group that we were dragging.
	//
	void *pDragItem = (void *)GetItemData(hDragItem);

	//
	// Determine what group was dropped onto.
	//
	HTREEITEM hDropItem = HitTest(point);
	if (hDropItem == hDragItem)
	{
		return;
	}

	void *pDropItem = NULL;
	if (hDropItem)
	{
		pDropItem = (void *)GetItemData(hDropItem);
	}

	if (pDragItem == pDropItem)
	{
		// Shouldn't happen, but just in case.
		return;
	}

	CWnd *pwndParent = GetParent();
	if (pwndParent != NULL)
	{
		if (eDropType == DROP_LEFT)
		{
			pwndParent->PostMessage(g_uLeftDragDropMsg, (WPARAM)pDragItem, (LPARAM)pDropItem);
		}
		else
		{
			pwndParent->PostMessage(g_uRightDragDropMsg, (WPARAM)pDragItem, (LPARAM)pDropItem);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIDEvent - 
//-----------------------------------------------------------------------------
void CTreeList::OnTimer(UINT nIDEvent) 
{
	//DBG("OnTimer\n");
	switch (nIDEvent)
	{
		case TIMER_GROUP_DRAG_SCROLL:
		{
			CPoint point;
			GetCursorPos(&point);

			CRect rect;
			GetWindowRect(&rect);

			if (!rect.PtInRect(point))
			{
				if (point.y > rect.bottom)
				{
					// scroll down
					int nCount = GetVisibleCount();
					HTREEITEM hItem = GetFirstVisibleItem();
					for (int i = 1; i < nCount; i++)
					{
						hItem = GetNextVisibleItem(hItem);
					}

					hItem = GetNextVisibleItem(hItem);

					if (hItem)
					{
						CTreeCtrl::EnsureVisible(hItem);
					}
				}
				else if (point.y < rect.top)
				{
					HTREEITEM hItem = GetFirstVisibleItem();
					HTREEITEM hPrevVisible = this->GetPrevVisibleItem(hItem);
					if (hPrevVisible)
					{
						// scroll up
						CTreeCtrl::EnsureVisible(hPrevVisible);
					}
				}
			}

			break;
		}
	
		default:
		{
			CTreeCtrl::OnTimer(nIDEvent);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CTreeList::OnMouseMove(UINT nFlags, CPoint point) 
{
	CTreeCtrl::OnMouseMove(nFlags, point);

	if (m_bRButtonDown && !m_hDragItem && (point.x != m_ptRButtonDown.x) && (point.y != m_ptRButtonDown.y))
	{
		// First mouse move since a right button down. Start dragging.
		HTREEITEM hItem = HitTest(m_ptRButtonDown);
		BeginDrag(point, hItem);
	}

	if (!m_hDragItem)
	{
		return;
	}

	if (m_pDragImageList)
	{
		m_pDragImageList->DragMove(point);
	}

	//
	// Highlight the item we hit.
	//
	HTREEITEM hItem = HitTest(point);
	if (hItem == GetDropHilightItem())
	{
		return;
	}

	// hide image first
	if (m_pDragImageList)
	{
		m_pDragImageList->DragLeave(this);
		m_pDragImageList->DragShowNolock(FALSE);
	}

	SelectDropTarget(hItem);

	if (m_pDragImageList)
	{
		m_pDragImageList->DragEnter(this, point);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTreeList::SelectItem(void *pItem)
{
	//DBG("SelectTreeListItem: %s\n", pVisGroup->GetName());
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		Select(hItem, TVGN_CARET);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::SelectNearestItem( int nItem )
{
	if ( ( m_Items.Count() > 0 ) && ( m_Items.Count() <= nItem ) )
	{
		nItem = m_Items.Count() - 1;
	}
	
	if ( m_Items.IsValidIndex( nItem ) )
	{
		SelectItem( m_Items[nItem] );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::EnsureVisible( int nItem )
{
	if ( ( m_Items.Count() > 0 ) && ( m_Items.Count() <= nItem ) )
	{
		nItem = m_Items.Count() - 1;
	}
	
	if ( m_Items.IsValidIndex( nItem ) )
	{
		EnsureVisible( m_Items[nItem] );
	}
}


//-----------------------------------------------------------------------------
// Sets the check status for the given item.
//		pItem - 
//		nCheckState - 0=not checked, 1=checked, -1=gray check (undefined)
//-----------------------------------------------------------------------------
void CTreeList::SetCheck(void *pItem, int nCheckState)
{
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		UINT uState = INDEXTOSTATEIMAGEMASK(1);
		if (nCheckState == 1)
		{
			uState = INDEXTOSTATEIMAGEMASK(2);
		}
		else if (nCheckState != 0)
		{
			uState = INDEXTOSTATEIMAGEMASK(3);
		}

		SetItemState(hItem, uState, TVIS_STATEIMAGEMASK);
	}
}


//-----------------------------------------------------------------------------
// Returns the check state for the given item.
//-----------------------------------------------------------------------------
int CTreeList::GetCheck(void *pItem)
{
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		return GetCheck(hItem);
	}

	return 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CTreeList::GetCheck(HTREEITEM hItem)
{
	UINT uState = (GetItemState(hItem, TVIS_STATEIMAGEMASK) & TVIS_STATEIMAGEMASK);
	if (uState == INDEXTOSTATEIMAGEMASK(2))
	{
		return 1;
	}
	else if (uState == INDEXTOSTATEIMAGEMASK(1))
	{
		return 0;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of visgroups in the whole tree.
//-----------------------------------------------------------------------------
int CTreeList::GetItemCount()
{
	return m_Items.Count();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void *CTreeList::GetItem(int nIndex)
{
	return m_Items.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Updates the tree control item text with the new group name.
//-----------------------------------------------------------------------------
void CTreeList::UpdateItem(void *pItem, const char *pszText)
{
	HTREEITEM hItem = FindHTreeItem(pItem);
	if (hItem)
	{
		SetItemText(hItem, pszText);
	}
}


//-----------------------------------------------------------------------------
// Stores the expanded/collapsed state into a data member for retrieval later.
//-----------------------------------------------------------------------------
void CTreeList::SaveTreeListExpandStates()
{
	for ( int i = 0; i < GetItemCount(); i++ )
	{
		void *pItem = GetItem(i);
		TreeListItemState_t newState;
		for ( int j = 0; j < m_ItemState.Count(); j++ )
		{
			TreeListItemState_t thisPair = m_ItemState.Element( j );
			if ( pItem == thisPair.pItem )
			{
				m_ItemState.Remove( j );
				break;
			}
		}

		HTREEITEM hItem = FindHTreeItem( pItem );
		newState.pItem = pItem;
		newState.bExpanded = false;
		if ( hItem && (GetItemState( hItem, TVIS_EXPANDED) & TVIS_EXPANDED) )
		{
			newState.bExpanded = true;
		}
		m_ItemState.AddToTail( newState );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::RestoreTreeListExpandStates()
{
	ExpandAll();
	for ( int i = 0; i < m_ItemState.Count(); i++ )
	{
		TreeListItemState_t thisPair = m_ItemState.Element( i );
		HTREEITEM thisItem = FindHTreeItem( thisPair.pItem );
		if ( thisItem )
		{
			if ( thisPair.bExpanded )
			{
				Expand(thisItem, TVE_EXPAND);
			}
			else
			{
				Expand( thisItem, TVE_COLLAPSE );				
			}
		}
	}
}

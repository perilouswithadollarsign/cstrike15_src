//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ====
//
//=============================================================================

#ifndef TREELIST_H
#define TREELIST_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlVector.h"


#define TREELIST_MSG_TOGGLE_STATE		"TreeList_ToggleState"
#define TREELIST_MSG_LEFT_DRAG_DROP		"TreeList_LeftDragDrop"
#define TREELIST_MSG_RIGHT_DRAG_DROP	"TreeList_RightDragDrop"
#define TREELIST_MSG_SEL_CHANGE			"TreeList_SelChange"
#define TREELIST_MSG_KEY_DOWN			"TreeList_KeyDown"


struct TreeListItemState_t
{
	void *pItem;
	bool bExpanded;
};


class CTreeList : public CTreeCtrl
{
public:

	CTreeList();
	virtual ~CTreeList();

	void DeleteAllItems();
	void AddItem(void *pItem, void *pParent, const char *pszText, bool bHasCheckBox );

	inline bool SubclassDlgItem(int nCtrlID, CWnd *pwndParent);
	inline void SetRedraw(bool bRedraw);
	inline void Invalidate(bool bErase = true);

	void SelectItem(void *pItem);
	void EnsureVisible(void *pItem);

	void SelectNearestItem( int nItem );
	void EnsureVisible( int nItem );

	void ExpandAll();
	void EnableChecks();

	void UpdateItem(void *pItem, const char *pszText);
	
	void *GetSelectedItem();
	int GetSelectedIndex();
	
	int GetItemCount();
	void *GetItem(int nIndex);
	void SetCheck(void *pItem, int nCheckState);
	int GetCheck(void *pItem);
	
	void ExpandItem(void *pItem);
	void CollapseItem(void *pItem);

	void SaveTreeListExpandStates();
	void RestoreTreeListExpandStates();

	// Virtuals that can be overridden by the derived class
	virtual void OnRenameItem(void *pItem, const char *pszText) {}

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGroupList)
	//}}AFX_VIRTUAL

protected:

	// Generated message map functions
	//{{AFX_MSG(CGroupList)
	afx_msg void OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnContextMenu(CWnd *, CPoint);
	afx_msg void OnSelChange(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnKeyDown( NMHDR *pNMHDR, LRESULT *pResult );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

	enum DropType_t
	{
		DROP_LEFT = 0,
		DROP_RIGHT,
	};

	void ExpandRecursive(HTREEITEM hItem);

	int GetCheck(HTREEITEM hItem);

	HTREEITEM FindHTreeItem(void *pItem);
	HTREEITEM FindHTreeItemRecursive(HTREEITEM hItem, void *pItem);

	void BeginDrag(CPoint pt, HTREEITEM hItem);
	void Drop(DropType_t eDropType, UINT nFlags, CPoint point);

	bool m_bRButtonDown;

	CPoint m_ptRButtonDown;

	CPoint m_ptLDown;

	CImageList m_cNormalImageList;
	CImageList *m_pDragImageList;

	HTREEITEM m_hDragItem;

	CUtlVector<void *> m_Items;
	CUtlVector<TreeListItemState_t> m_ItemState;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTreeList::Invalidate(bool bErase)
{
	CTreeCtrl::Invalidate(bErase ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Enables or disables updates. Useful for populating the list and only
// updating at the end.
//-----------------------------------------------------------------------------
void CTreeList::SetRedraw(bool bRedraw)
{
	CTreeCtrl::SetRedraw(bRedraw ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTreeList::SubclassDlgItem(int nCtrlID, CWnd *pwndParent)
{
	return (CTreeCtrl::SubclassDlgItem(nCtrlID, pwndParent) == TRUE);
}


#endif // TREELIST_H

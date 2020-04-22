//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FILTERCONTROL_H
#define FILTERCONTROL_H
#pragma once


#include "resource.h"
#include "cordonlist.h"
#include "GroupList.h"
#include "HammerBar.h"


enum FilterDialogMode_t
{
	FILTER_DIALOG_NONE = -1,
	FILTER_DIALOG_USER_VISGROUPS,
	FILTER_DIALOG_AUTO_VISGROUPS,
	FILTER_DIALOG_CORDONS,
};


class CFilterControl : public CHammerBar
{
public:

	CFilterControl() : CHammerBar()
	{
		m_bInitialized = false;
	}

	BOOL Create(CWnd *pParentWnd);

	void UpdateList();
	void UpdateCordonList( Cordon_t *pSelectCordon = NULL, BoundBox *pSelectBox = NULL );
	void SelectCordon( Cordon_t *pSelectCordon, BoundBox *pSelectBox );
	void UpdateGroupList();
	void UpdateGroupListChecks();
	void UpdateCordonListChecks();

	virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler);
	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);

private:
	//{{AFX_DATA(CFilterControl)
	enum { IDD = IDD_MAPVIEWBAR };
	//}}AFX_DATA

	CBitmapButton m_cMoveUpButton;
	CBitmapButton m_cMoveDownButton;
	CGroupList m_cGroupBox;
	CCordonList m_cCordonBox;
	CTabCtrl m_cTabControl;
	
	bool m_bInitialized;

protected:

	virtual BOOL OnInitDialog(void);

	void OnSelChangeTab(NMHDR *header, LRESULT *result); 
	void ChangeMode( FilterDialogMode_t oldMode,  FilterDialogMode_t newMode );

	void OnCordonListDragDrop(CordonListItem_t *drag, CordonListItem_t *drop );
	void OnVisGroupListDragDrop(CVisGroup *pDragGroup, CVisGroup *pDropGroup );

	void DeleteCordonListItem(CordonListItem_t *pDelete, bool bConfirm );

	//{{AFX_MSG(CFilterControl)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnEditGroups();
	afx_msg void OnNew();
	afx_msg void OnDelete();
	afx_msg void OnMarkMembers();
	afx_msg BOOL OnMoveUpDown(UINT uCmd);
	afx_msg void UpdateControl(CCmdUI *);
	afx_msg void UpdateControlGroups(CCmdUI *pCmdUI);
	afx_msg void OnActivate(UINT nState, CWnd*, BOOL);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnWindowPosChanged(WINDOWPOS *pPos);
	afx_msg void OnEndlabeleditGrouplist(NMHDR*, LRESULT*);
	afx_msg void OnBegindragGrouplist(NMHDR*, LRESULT*);
	afx_msg void OnMousemoveGrouplist(NMHDR*, LRESULT*);
	afx_msg void OnEnddragGrouplist(NMHDR*, LRESULT*);
	afx_msg LRESULT OnListToggleState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListLeftDragDrop(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListRightDragDrop(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListSelChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListKeyDown(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowAllGroups(void);
	//}}AFX_MSG

	CImageList *m_pDragImageList;
	FilterDialogMode_t m_mode;		// Whether we're showing user visgroups, auto visgroups, or cordons

	DECLARE_MESSAGE_MAP()
};


#endif // FILTERCONTROL_H

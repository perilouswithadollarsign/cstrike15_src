//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ====
//
//=============================================================================

#ifndef GROUPLIST_H
#define GROUPLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "treelist.h"


#define GROUPLIST_MSG_TOGGLE_STATE			"TreeList_ToggleState"
#define GROUPLIST_MSG_LEFT_DRAG_DROP		"TreeList_LeftDragDrop"
#define GROUPLIST_MSG_RIGHT_DRAG_DROP		"TreeList_RightDragDrop"
#define GROUPLIST_MSG_SEL_CHANGE			"TreeList_SelChange"


class CVisGroup;


class CGroupList : public CTreeList
{
public:

	CGroupList();
	virtual ~CGroupList();

	void AddVisGroup(CVisGroup *pVisGroup);
	void UpdateVisGroup(CVisGroup *pVisGroup);

	inline int GetVisGroupCount();
	inline CVisGroup *GetVisGroup(int nIndex);
	inline CVisGroup *GetSelectedVisGroup();

	void OnRenameItem(void *pVisGroup, const char *pszText);
};


//-----------------------------------------------------------------------------
// Helper functions for avoiding casts in client code.
//-----------------------------------------------------------------------------
int CGroupList::GetVisGroupCount()
{
	return GetItemCount();
}


CVisGroup *CGroupList::GetVisGroup(int nIndex)
{
	return (CVisGroup *)GetItem(nIndex);
}


CVisGroup *CGroupList::GetSelectedVisGroup()
{
	return (CVisGroup *)CTreeList::GetSelectedItem();
}

#endif // GROUPLIST_H

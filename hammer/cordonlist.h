//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ====
//
//=============================================================================

#ifndef CORDONLIST_H
#define CORDONLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "treelist.h"


#define CORDONLIST_MSG_TOGGLE_STATE			"TreeList_ToggleState"
#define CORDONLIST_MSG_LEFT_DRAG_DROP		"TreeList_LeftDragDrop"
#define CORDONLIST_MSG_RIGHT_DRAG_DROP		"TreeList_RightDragDrop"
#define CORDONLIST_MSG_SEL_CHANGE			"TreeList_SelChange"


class BoundBox;
struct Cordon_t;


struct CordonListItem_t
{
	Cordon_t *m_pCordon;
	BoundBox *m_pBox;
};


class CCordonList : public CTreeList
{
public:

	CCordonList();
	virtual ~CCordonList();

	void AddCordon( CordonListItem_t *pCordon, CordonListItem_t *pParent );
	void UpdateCordon(CordonListItem_t *item);

	inline int GetCordonCount();
	inline CordonListItem_t *GetCordon(int nIndex);
	inline CordonListItem_t *GetSelectedCordon();

	void OnRenameItem(void *item, const char *pszText);
};


//-----------------------------------------------------------------------------
// Helper functions for avoiding casts in client code.
//-----------------------------------------------------------------------------
int CCordonList::GetCordonCount()
{
	return GetItemCount();
}


CordonListItem_t *CCordonList::GetCordon(int nIndex)
{
	return (CordonListItem_t *)GetItem(nIndex);
}


CordonListItem_t *CCordonList::GetSelectedCordon()
{
	return (CordonListItem_t *)CTreeList::GetSelectedItem();
}

#endif // CORDONLIST_H

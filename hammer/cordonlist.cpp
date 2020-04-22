//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ====
//
// A tree list control for cordons. Supports drag and drop, and posts a
// registered windows message to the list view's parent window when cordons
// are hidden or shown.
//
//=============================================================================

#include "stdafx.h"
#include "GroupList.h"
#include "cordon.h"
#include "cordonlist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCordonList::CCordonList()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCordonList::~CCordonList()
{
}


//-----------------------------------------------------------------------------
// Called when the user finishes editing the label of a tree item.
//-----------------------------------------------------------------------------
void CCordonList::OnRenameItem(void *pItem, const char *pszText) 
{
	Assert(pItem);
	Assert(pszText);

	if (!pItem || !pszText)
		return;

	CordonListItem_t *pCordon = (CordonListItem_t *)pItem;
	
	// Can't rename cordon boxes
	if ( !pCordon->m_pBox )
	{
		pCordon->m_pCordon->m_szName.Set( pszText );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCordonList::AddCordon( CordonListItem_t *pCordon, CordonListItem_t *pParent )
{
	if ( !pCordon->m_pBox )
	{
		AddItem( pCordon, pParent, pCordon->m_pCordon->m_szName, true );
	}
	else
	{
		AddItem( pCordon, pParent, "box", false );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCordonList::UpdateCordon( CordonListItem_t *pCordon )
{
	UpdateItem( pCordon, pCordon->m_pCordon->m_szName );
}



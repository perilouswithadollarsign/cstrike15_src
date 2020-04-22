//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SCROLLABLEEDITABLEPANEL_H
#define SCROLLABLEEDITABLEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class ScrollBar;
}

namespace vgui
{

//-----------------------------------------------------------------------------
// An editable panel that has a scrollbar
//-----------------------------------------------------------------------------
class ScrollableEditablePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( ScrollableEditablePanel, vgui::EditablePanel );

public:
	ScrollableEditablePanel( vgui::Panel *pParent, vgui::EditablePanel *pChild, const char *pName );
	virtual ~ScrollableEditablePanel() {}
	virtual void PerformLayout();

	vgui::ScrollBar	*GetScrollbar( void ) { return m_pScrollBar; }

	virtual void OnMouseWheeled(int delta);

	MESSAGE_FUNC( OnScrollBarSliderMoved, "ScrollBarSliderMoved" );

private:
	vgui::ScrollBar *m_pScrollBar;
	vgui::EditablePanel *m_pChild;
};


} // end namespace vgui

#endif // SCROLLABLEEDITABLEPANEL_H
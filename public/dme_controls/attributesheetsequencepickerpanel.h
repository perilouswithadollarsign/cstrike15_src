//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// CAttributeSheetSequencePickerPanel - Panel for editing int attributes that select a sprite sheet sequence
//
//===============================================================================

#ifndef ATTRIBUTESHEETSEQUENCEPICKERPANEL_H
#define ATTRIBUTESHEETSEQUENCEPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeTextPanel.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CSheetSequencePanel;

namespace vgui
{
	class MenuButton;
}

//-----------------------------------------------------------------------------
// CAttributeSheetSequencePickerPanel
//-----------------------------------------------------------------------------
class CAttributeSheetSequencePickerPanel : public CAttributeTextPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeSheetSequencePickerPanel, CAttributeTextPanel );

public:
	CAttributeSheetSequencePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeSheetSequencePickerPanel();

	virtual void PerformLayout();

	void UpdateSheetPanel();

private:
	MESSAGE_FUNC_INT( OnSheetSequenceSelected, "SheetSequenceSelected", nSequenceNumber );

	vgui::MenuButton *m_pSequenceSelection;
	CSheetSequencePanel *m_pSheetPanel;
	bool m_bIsSecondView;
};

// Like the picker panel, except shows the color channel for dual-sequence VMTs
class CAttributeSheetSequencePickerPanel_Secondary : public CAttributeSheetSequencePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeSheetSequencePickerPanel_Secondary, CAttributeSheetSequencePickerPanel );
	CAttributeSheetSequencePickerPanel_Secondary( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

};


#endif // ATTRIBUTEMDLPICKERPANEL_H

//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// CSheetEditorPanel - Tool panel for editing sprite sheet information
//
//===============================================================================

#ifndef SHEETEDITORPANEL_H
#define SHEETEDITORPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeParticleSystemDefinition;
class CSheet;
class CVMTPicker;
class CVMTPreviewPanel;

namespace vgui
{
	class IScheme;
	class Label;
	class TextEntry;
	class IScheme;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CSheetEditorPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CSheetEditorPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CSheetEditorPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CSheetEditorPanel();

	void SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem );
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	vgui::Label *m_pTitleLabel;
	vgui::ListPanel *m_pTestList;
	CVMTPreviewPanel *m_pVMTPreview;
	CVMTPicker* m_pVMTPicker;
	CSheet *m_pSheetInfo;
};


#endif // SHEETEDITORPANEL_H

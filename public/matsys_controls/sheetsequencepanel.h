//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// CSheetSequencePanel - Panel for selecting one sequence from a sprite sheet
//
//===============================================================================

#ifndef SHEETSEQUENCEPANEL_H
#define SHEETSEQUENCEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui/MouseCode.h"
#include "vgui_controls/Menu.h"
#include "materialsystem/MaterialSystemUtil.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CSheetExtended;

namespace vgui
{
	class Menu;
}

//-----------------------------------------------------------------------------

class CSheetSequencePanel : public vgui::Menu
{
	DECLARE_CLASS_SIMPLE( CSheetSequencePanel, vgui::Menu );
public:
	CSheetSequencePanel( vgui::Panel *parent, const char *panelName );
	virtual ~CSheetSequencePanel();

	virtual void Paint();
	virtual void PerformLayout();
	virtual void OnCursorMoved(int x, int y);
	virtual void OnCursorExited();
	virtual void OnMouseReleased( vgui::MouseCode mouseCode );

	void SetFromMaterial( IMaterial* pMaterial );
	void SetFromMaterialName( const char* pMaterialName );

	void SetSecondSequenceView( bool bIsSecondSequenceView );

private:
	int SequenceGridCount();
	int SequenceGridRows();
	int SequenceGridSquareSize();

	void PrepareMaterials();

	CSheetExtended* m_pSheet;
	CMaterialReference m_Material;
	int m_nHighlightedSequence;
	bool m_bSeparateAlphaColorMaterial;
	bool m_bIsSecondSequenceView;

	static void EnsureMaterialsExist();
	static bool m_sMaterialsInitialized;
	static CMaterialReference m_sColorMat;
	static CMaterialReference m_sAlphaMat;
};

#endif // VMTPICKER_H

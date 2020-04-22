//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VMTPICKER_H
#define VMTPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "matsys_controls/baseassetpicker.h"
#include "materialsystem/MaterialSystemUtil.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CVMTPreviewPanel;
class CSheetExtended;
class CVMTSequenceMenuButton;
class CVMTPicker;
class CVMTPreviewToolbar;
class CSheetSequencePanel;

namespace vgui
{
	class Splitter;
	class CheckButton;
	class Slider;
	class Panel;
}

//-----------------------------------------------------------------------------

class CVMTPreviewToolbar : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CVMTPreviewToolbar, EditablePanel );
public:
	CVMTPreviewToolbar( vgui::Panel *parent, const char *panelName, CVMTPicker *parentpicker );

	void PopulateSequenceMenu( vgui::Menu *menu );
	int GetSequenceMenuItemCount( );
	void UpdateToolbarGUI();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", pData );
	MESSAGE_FUNC_INT( OnSelectSequence, "OnSelectSequence", nSequenceNumber );

	MESSAGE_FUNC( OnNextSequence, "OnNextSequence" );
	MESSAGE_FUNC( OnPrevSequence, "OnPrevSequence" );

	MESSAGE_FUNC_PARAMS( OnSheetSequenceSelected, "SheetSequenceSelected", pData );

private:
	vgui::Slider *m_pSheetPreviewSpeed;
	CVMTPicker *m_pParentPicker;
	vgui::Button *m_pNextSeqButton;
	vgui::Button *m_pPrevSeqButton;
	vgui::MenuButton *m_pSequenceSelection;
	vgui::MenuButton *m_pSequenceSelection_Second;
	CSheetSequencePanel *m_pSheetPanel;
	CSheetSequencePanel *m_pSheetPanel_Second;
};

//-----------------------------------------------------------------------------
// Purpose: Base class for choosing raw assets
//-----------------------------------------------------------------------------
class CVMTPicker : public CBaseAssetPicker
{
	DECLARE_CLASS_SIMPLE( CVMTPicker, CBaseAssetPicker );

public:
	CVMTPicker( vgui::Panel *pParent, bool bAllowMultiselect = false );
	virtual ~CVMTPicker();

	virtual void CustomizeSelectionMessage( KeyValues *pKeyValues );

	void SetSheetPreviewSpeed( float flPreviewSpeed );
	void SetSelectedSequence( int nSequence );
	void SetSelectedSecondarySequence( int nSequence );

	int GetSheetSequenceCount();
	int GetCurrentSequence();
	int GetCurrentSecondarySequence();
	int GetRealSequenceNumber();

	CSheetExtended* GetSheet();
	IMaterial* GetMaterial();

private:
	// Derived classes have this called when the previewed asset changes
	virtual void OnSelectedAssetPicked( const char *pAssetName );

	CVMTPreviewPanel *m_pVMTPreview2D;
	CVMTPreviewPanel *m_pVMTPreview3D;
	vgui::Splitter *m_p2D3DSplitter;
	vgui::Splitter *m_pPreviewSplitter;
	CVMTPreviewToolbar *m_pVMTPreviewToolbar;
};


//-----------------------------------------------------------------------------
// Purpose: Modal dialog for asset picker
//-----------------------------------------------------------------------------
class CVMTPickerFrame : public CBaseAssetPickerFrame
{
	DECLARE_CLASS_SIMPLE( CVMTPickerFrame, CBaseAssetPickerFrame );

public:
	CVMTPickerFrame( vgui::Panel *pParent, const char *pTitle, bool bAllowMultiselect = false );
};


#endif // VMTPICKER_H

//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// CAttributeSheetSequencePickerPanel - Panel for editing int attributes that select a sprite sheet sequence
//
//===============================================================================

#include "dme_controls/attributesheetsequencepickerpanel.h"
#include "FileSystem.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/FileOpenDialog.h"
#include "dme_controls/AttributeTextEntry.h"
#include "matsys_controls/sheetsequencepanel.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "tier1/keyvalues.h"
#include "dme_controls/AttributeWidgetFactory.h"
#include "movieobjects/dmeeditortypedictionary.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
//
// CNotifyMenuButton - MenuButton with a notify before the menu is shown
//
//-----------------------------------------------------------------------------

class CNotifyMenuButton: public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE( CNotifyMenuButton, vgui::MenuButton );

public:
	CNotifyMenuButton(CAttributeSheetSequencePickerPanel *parent, const char *panelName, const char *text);
	virtual void OnShowMenu(Menu *menu);

private:
	CAttributeSheetSequencePickerPanel* m_pParent;
};

CNotifyMenuButton::CNotifyMenuButton(CAttributeSheetSequencePickerPanel *parent, const char *panelName, const char *text):
	BaseClass(parent,panelName,text)
{
	m_pParent = parent;
}

void CNotifyMenuButton::OnShowMenu(Menu *menu)
{
	if ( m_pParent )
	{
		m_pParent->UpdateSheetPanel();
	}

	BaseClass::OnShowMenu(menu);
}

//-----------------------------------------------------------------------------
//
// CAttributeSheetSequencePickerPanel
//
//-----------------------------------------------------------------------------

CAttributeSheetSequencePickerPanel::CAttributeSheetSequencePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
	if ( info.m_pEditorInfo )
	{
		m_bIsSecondView = !V_stricmp( info.m_pEditorInfo->GetWidgetName(), "sheetsequencepicker_second" );
	}
	else
	{
		m_bIsSecondView = false;
	}

	m_pSheetPanel = new CSheetSequencePanel(this, "sheetsequencepanel");
	m_pSheetPanel->AddActionSignalTarget( this );

	m_pSequenceSelection = new CNotifyMenuButton( this, "SequenceSelection", "seq" );
	m_pSequenceSelection->SetMenu( m_pSheetPanel );
	m_pSheetPanel->AddActionSignalTarget( this );

	UpdateSheetPanel();
}

void CAttributeSheetSequencePickerPanel::UpdateSheetPanel()
{
	CDmElement* pElement = GetPanelElement();
	CDmAttribute* pMaterialAttr = pElement->GetAttribute("material");

	if ( m_bIsSecondView )
	{
		m_pSheetPanel->SetSecondSequenceView( true );
		m_pSequenceSelection->SetText( "sq2" );
	}

	if ( pMaterialAttr == NULL )
	{
		pElement = FindReferringElement< CDmeParticleSystemDefinition >( pElement, "initializers" );

		if ( pElement )
		{
			pMaterialAttr = pElement->GetAttribute("material");
		}
	}

	if ( pMaterialAttr )
	{
		m_pSheetPanel->SetFromMaterialName( pMaterialAttr->GetValueString() );
	}
}

CAttributeSheetSequencePickerPanel::~CAttributeSheetSequencePickerPanel()
{
}

void CAttributeSheetSequencePickerPanel::OnSheetSequenceSelected( int nSequenceNumber )
{
	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, GetNotify(), "Select Sheet Sequence" );
	SetAttributeValue(nSequenceNumber);
}

// MOC_TODO: factor this somewhere else - shared with other attribute controls
void CAttributeSheetSequencePickerPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	m_pType->GetBounds( x, y, w, h );

	int inset = 25;
	m_pType->SetWide( w - inset );

	x += w;
	x -= inset;

	h -= 2;

	m_pSequenceSelection->SetBounds( x, y, inset, h );
}


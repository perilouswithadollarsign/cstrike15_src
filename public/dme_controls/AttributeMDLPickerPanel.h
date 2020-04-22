//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEMDLPICKERPANEL_H
#define ATTRIBUTEMDLPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"
#include "vgui_controls/phandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMDLPickerFrame;


//-----------------------------------------------------------------------------
// CAttributeMDLPickerPanel
//-----------------------------------------------------------------------------
class CAttributeMDLPickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeMDLPickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeMDLPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeMDLPickerPanel();

private:
	MESSAGE_FUNC_PARAMS( OnMDLSelected, "AssetSelected", kv );
	virtual void ShowPickerDialog();
};


#endif // ATTRIBUTEMDLPICKERPANEL_H

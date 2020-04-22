//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef ATTRIBUTEBOOLEANPANEL_H
#define ATTRIBUTEBOOLEANPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeTextPanel.h"
//-----------------------------------------------------------------------------

class CAttributeBooleanPanel : public CAttributeTextPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeBooleanPanel, CAttributeTextPanel );

public:
	CAttributeBooleanPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	
	MESSAGE_FUNC_INT( OnCheckButtonChecked, "CheckButtonChecked", state );
	virtual void	PerformLayout();
	virtual void	Refresh();
	virtual void	ApplySchemeSettings(IScheme *pScheme);
	
protected:
	vgui::CheckButton *m_pValueButton;
};


#endif // ATTRIBUTEBOOLEANPANEL_H

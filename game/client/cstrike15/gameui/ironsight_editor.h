//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef IRONSIGHTDIALOG_H
#define IRONSIGHTDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "cs_shareddefs.h"

#ifdef IRONSIGHT

//-----------------------------------------------------------------------------
// Purpose: Handles selection of IronSight mode on/off
//-----------------------------------------------------------------------------
class CIronSightDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CIronSightDialog, vgui::Frame );

public:
	CIronSightDialog(vgui::Panel *parent);
	~CIronSightDialog();

	virtual void OnClose( void );
	//virtual void OnCommand( const char *command );
	virtual void OnMessage( const KeyValues *pParams,  vgui::VPANEL fromPanel );

	vgui::Slider *m_pSliderPosX;
	vgui::Slider *m_pSliderPosY;
	vgui::Slider *m_pSliderPosZ;
	Vector m_vecPosBackup;

	vgui::Slider *m_pSliderRotX;
	vgui::Slider *m_pSliderRotY;
	vgui::Slider *m_pSliderRotZ;
	Vector m_vecRotBackup;

	vgui::Slider *m_pSliderFOV;

	vgui::Slider *m_pSliderPivotForward;
	vgui::Slider *m_pSliderLooseness;

	vgui::TextEntry *m_pSchemaText;
};
#endif //IRONSIGHT

#endif //IRONSIGHTDIALOG_H
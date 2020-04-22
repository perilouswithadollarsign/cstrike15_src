//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VEXTRASDIALOG_H__
#define __VEXTRASDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class GenericPanelList;
class CInfoLabel;

struct ExtraInfo_t
{
	ExtraInfo_t()
	{
		m_nImageId = -1;
	}

	int			m_nImageId;
	CUtlString	m_TitleString;
	CUtlString	m_SubtitleString;
	CUtlString	m_MapName;
	CUtlString	m_VideoName;
	CUtlString	m_URLName;
	CUtlString	m_Command;
};

class CExtrasDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CExtrasDialog, CBaseModFrame );

public:
	CExtrasDialog( vgui::Panel *pParent, const char *pPanelName );
	~CExtrasDialog();

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );

	ExtraInfo_t *GetExtraInfo( int nInfoIndex );

protected:
	virtual void OnCommand( char const *pCommand );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();
	virtual void PaintBackground();
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void UpdateFooter();
	void SetInfoImage( int nInfoIndex );
	void DrawInfoImage();
	void PopulateFromScript();

	CUtlVector< ExtraInfo_t >	m_ExtraInfos;
	int							m_nVignetteImageId;
	int							m_nInfoImageId;

	GenericPanelList			*m_pInfoList;
	vgui::ImagePanel			*m_pInfoImage;
	CInfoLabel					*m_pInfoLabel;
};

};

#endif

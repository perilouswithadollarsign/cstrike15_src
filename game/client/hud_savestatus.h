//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_SAVESTATUS_H
#define HUD_SAVESTATUS_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

namespace vgui
{
	class IScheme;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudSaveStatus : public CHudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudSaveStatus, vgui::EditablePanel );

public:
	explicit CHudSaveStatus( const char *pElementName );

	void			SaveStarted();

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool	ShouldDraw();
	virtual void	OnThink();

private:
	void			SetSavingLabels( bool bIsGameSave );

	vgui::ImagePanel	*m_pSavingIcon;
	vgui::Label			*m_pSavingLabel;
	vgui::Label			*m_pSavedLabel;

	float				m_flLastAnimTime;
	float				m_flFadeOutTime;
	float				m_flSaveStartedTime;

	bool				m_bNeedsDraw;
	bool				m_bIsSteamProfileSave;
};

#endif // HUD_SAVESTATUS_H

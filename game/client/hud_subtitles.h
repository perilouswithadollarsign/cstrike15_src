//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
//===================================================================================//

#ifndef HUD_SUBTITLES_H
#define HUD_SUBTITLES_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "subtitlepanel.h"

namespace vgui
{
	class IScheme;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudSubtitles : public CHudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudSubtitles, vgui::EditablePanel );

	void StartCaptions( const char *pFilename );
	void StopCaptions();

public:
	explicit CHudSubtitles( const char *pElementName );
	~CHudSubtitles();

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool	ShouldDraw();
	virtual void	LevelShutdown();
	virtual void	Reset();

private:
	CSubtitlePanel	*m_pSubtitlePanel;
	bool			m_bIsPaused;
};

#endif // HUD_SUBTITLES_H

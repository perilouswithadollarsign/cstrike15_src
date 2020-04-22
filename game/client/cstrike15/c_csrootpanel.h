//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_CSRootPanel_H
#define C_CSRootPanel_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "utlvector.h"

class CPanelEffect;
class ITFHintItem;

// Serial under of effect, for safe lookup
typedef unsigned int EFFECT_HANDLE;

//-----------------------------------------------------------------------------
// Purpose: Sits between engine and client .dll panels
//  Responsible for drawing screen overlays
//-----------------------------------------------------------------------------
class C_CSRootPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
						C_CSRootPanel( vgui::VPANEL parent, int slot, const char *panelName = "CounterStrike Root Panel" );
	virtual				~C_CSRootPanel( void );

	virtual void		PaintTraverse( bool Repaint, bool allowForce = true );
	virtual void		OnThink();

	// Draw Panel effects here
	virtual void		PostChildPaint();

	// Clear list of Panel Effects
	virtual void		LevelInit( void );
	virtual void		LevelShutdown( void );

	// Run effects and let them decide whether to remove themselves
	void				OnTick( void );

private:

	// Render all panel effects
	void		RenderPanelEffects( void );

	// List of current panel effects
	CUtlVector< CPanelEffect *> m_Effects;
	int			m_nSplitSlot;
};

#endif // C_CSRootPanel_H

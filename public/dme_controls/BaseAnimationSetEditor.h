//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMATIONSETEDITOR_H
#define BASEANIMATIONSETEDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ImageList.h"
#include "dme_controls/RecordingState.h"
#include "dme_controls/BaseAnimationSetEditorController.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct LogPreview_t;
class CDmeAnimationSet;
class CDmeAnimationList;
class CDmeChannelsClip;
class CBaseAnimSetControlGroupPanel;
class CBaseAnimSetPresetFaderPanel;
class CBaseAnimSetAttributeSliderPanel;


//-----------------------------------------------------------------------------
// Base class for the panel for editing animation sets
//-----------------------------------------------------------------------------
class CBaseAnimationSetEditor : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAnimationSetEditor, vgui::EditablePanel );

public:
	enum EAnimSetLayout_t
	{
		LAYOUT_SPLIT = 0,
		LAYOUT_VERTICAL,
		LAYOUT_HORIZONTAL,
	};

	CBaseAnimationSetEditor( vgui::Panel *parent, char const *panelName, CBaseAnimationSetControl *pAnimationSetController );
	virtual ~CBaseAnimationSetEditor();

	virtual void						CreateToolsSubPanels();
	virtual void						ChangeLayout( EAnimSetLayout_t newLayout );
	virtual void						OpenTreeViewContextMenu( KeyValues *pItemData );

	CBaseAnimationSetControl			*GetController();

	CBaseAnimSetControlGroupPanel		*GetControlGroup();
	CBaseAnimSetPresetFaderPanel		*GetPresetFader();
	CBaseAnimSetAttributeSliderPanel	*GetAttributeSlider();

	void								ChangeAnimationSetClip( CDmeFilmClip *pFilmClip );
	void								OnControlsAddedOrRemoved();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", params );
	MESSAGE_FUNC_INT( OnChangeLayout, "OnChangeLayout", value );

protected:
	EAnimSetLayout_t					m_Layout;
	vgui::DHANDLE< vgui::Splitter >		m_Splitter;

	vgui::DHANDLE< CBaseAnimSetControlGroupPanel >		m_hControlGroup;
	vgui::DHANDLE< CBaseAnimSetPresetFaderPanel >		m_hPresetFader;
	vgui::DHANDLE< CBaseAnimSetAttributeSliderPanel >	m_hAttributeSlider;

	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;

	CBaseAnimationSetControl			*m_pController;
};

#endif // BASEANIMATIONSETEDITOR_H

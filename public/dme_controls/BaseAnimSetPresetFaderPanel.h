//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMSETPRESETFADERPANEL_H
#define BASEANIMSETPRESETFADERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/animsetattributevalue.h"
#include "dme_controls/dmecontrols_utils.h"
#include "datamodel/dmehandle.h"
#include "movieobjects/proceduralpresets.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/Slider.h"

	
//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimationSetControl;
class CPresetSlider;
class CBaseAnimationSetEditor;
class CSliderListPanel;
class CAddPresetDialog;
class CDmePreset;
class CDmePresetGroup;
class CDmePresetGroupEditorFrame;

namespace vgui
{
	class InputDialog;
}

struct FaderPreview_t
{
	FaderPreview_t() :
		name( 0 ),
		amount( 0 ),
		isbeingdragged( false ),
		holdingPreviewKey( false ),
		values( 0 ),
		nProceduralType( PROCEDURAL_PRESET_NOT )
	{
	}
	const char		*name;
	float			amount;
	bool			isbeingdragged;
	bool			holdingPreviewKey;
	AttributeDict_t *values;
	int				nProceduralType;
};

CDmePresetGroup *FindAnyPresetGroup( CDmeFilmClip *pFilmClip, const char *pPresetGroupName );
CDmePreset      *FindAnyPreset     ( CDmeFilmClip *pFilmClip, const char *pPresetGroupName, const char *pPresetName );


//-----------------------------------------------------------------------------
// Base class for the preset fader panel
//-----------------------------------------------------------------------------
class CBaseAnimSetPresetFaderPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAnimSetPresetFaderPanel, vgui::EditablePanel );
public:
	CBaseAnimSetPresetFaderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor );

	void	GetPreviewFader( FaderPreview_t& fader );

	void	UpdateProceduralPresetSlider( AttributeDict_t *values );

	void	UpdateControlValues( bool bVisibleOnly = true );

	CBaseAnimationSetControl *GetController() { return m_pController; }

	void	OnDeletePreset( const char *pPresetName );

	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	virtual void DispatchCurve( int nCurveType );

	CPresetSlider *FindPresetSlider( const char *pName );

	void SetActivePresetSlider( CPresetSlider *pSlider );
	CPresetSlider *GetActivePresetSlider();

protected:
	MESSAGE_FUNC( OnShowAddPresetDialog, "ShowAddPresetDialog" );
	MESSAGE_FUNC( OnPresetsChanged, "PresetsChanged" );
	MESSAGE_FUNC( OnManagePresets, "ManagePresets" );
	MESSAGE_FUNC_PARAMS( OnPresetNameSelected, "PresetNameSelected", params );
	MESSAGE_FUNC( OnPageChanged, "PageChanged" );

protected:
	CPresetSlider *GetSliderForRow( int nSlot );
	void UpdateOrCreatePresetSlider( int nSlot, const char *pPresetGroupName, const char *pPresetName );
	void RebuildPresetSliders( const char *pPresetGroupName, const CUtlVector< CUtlSymbolLarge > &presetNames );
	void AddPreset( const char *pPresetGroupName, const char *pPresetName, bool bAnimated );
	void PopulatePresetList( bool bChanged );

	CBaseAnimationSetControl				*m_pController;
	vgui::PropertySheet						*m_pSheet;
	CUtlVector< vgui::PropertyPage* >		m_presetGroupPages;
	CSliderListPanel						*m_pSliders;
	CUtlVector< CPresetSlider* >			m_presetSliders;
	vgui::DHANDLE< CDmePresetGroupEditorFrame > m_hPresetEditor;

	vgui::DHANDLE< CPresetSlider >			m_hActivePresetSlider;

	friend CBaseAnimationSetControl;
	friend CPresetSlider;
};


//-----------------------------------------------------------------------------
//
// CPresetSlider: The actual preset slider itself!
//
//-----------------------------------------------------------------------------
class CPresetSlider : public vgui::Slider
{
	DECLARE_CLASS_SIMPLE( CPresetSlider, vgui::Slider );

public:

	CPresetSlider( vgui::Panel *parent, CBaseAnimSetPresetFaderPanel *pFaderPanel );
	~CPresetSlider();

	void		Init( const char *pPresetGroupName, const char *pPresetName );
	void		Clear();

	void		SetControlValues( );

	float		GetCurrent();
	void		SetPos( float frac );

	AttributeDict_t *GetAttributeDict();

	bool			IsDragging();

	void			IgnoreCursorMovedEvents( bool bIgnore );
	void			Deactivate();

	const char *GetPresetName();
	const char *GetPresetGroupName();
	int GetProceduralPresetType() { return m_nProceduralType; }

protected:

	virtual void Paint();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void GetTrackRect( int &x, int &y, int &w, int &h );
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnCursorMoved( int x, int y );
 	virtual void OnCursorEntered();
 	virtual void OnCursorExited();

	MESSAGE_FUNC( OnShowContextMenu, "OnShowContextMenu" );
	MESSAGE_FUNC( OnRename, "OnRename" );
	MESSAGE_FUNC( OnDelete, "OnDelete" );

	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", params );

	MESSAGE_FUNC( OnDeleteConfirmed, "OnDeleteConfirmed" );

protected:

	KEYBINDING_FUNC( ts_curve_1,		KEY_1, 0,								OnCurve1,			"#ts_curve1_help", 0 );
	KEYBINDING_FUNC( ts_curve_2,		KEY_2, 0,								OnCurve2,			"#ts_curve2_help", 0 );
	KEYBINDING_FUNC( ts_curve_3,		KEY_3, 0,								OnCurve3,			"#ts_curve3_help", 0 );
	KEYBINDING_FUNC( ts_curve_4,		KEY_4, 0,								OnCurve4,			"#ts_curve4_help", 0 );

private:
	void OnRenameCompleted( const char *pText, KeyValues *pContextKeyValues );

	void UpdateTickPos( int x, int y );

	CBaseAnimSetPresetFaderPanel	*m_pPresetFaderPanel;

	Color			m_GradientColor;
	Color			m_ZeroColor;
	Color			m_TextColor;
	Color			m_TextColorFocus;
	vgui::TextImage	*m_pName;

	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;

	AttributeDict_t		m_AttributeLookup;

	CUtlSymbolLarge	m_presetGroupName;
	CUtlSymbolLarge	m_presetName;
	int			m_nProceduralType;
	bool		m_bReadOnly;

	bool		m_bIgnoreCursorMovedEvents;

	static bool s_bResetMousePosOnMouseUp;
	static int s_nMousePosX;
	static int s_nMousePosY;
};

#endif // BASEANIMSETPRESETFADERPANEL_H

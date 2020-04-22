//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMSETATTRIBUTESLIDERPANEL_H
#define BASEANIMSETATTRIBUTESLIDERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmehandle.h"
#include "dme_controls/BaseAnimationSetEditorController.h"
#include "vgui_controls/EditablePanel.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimationSetEditor;
class CBaseAnimationSetControl;
class CAttributeSlider;
class CDmElement;
class CDmeChannel;
class CDmeFilmClip;
class CDmeTimeSelection;
enum RecordingMode_t;
class DmeLog_TimeSelection_t;
class CPresetSideFilterSlider;
struct FaderPreview_t;
struct AttributeValue_t;

enum AnimationControlType_t;

enum
{
	FADER_DRAG_CHANGED			= ( 1<<0 ),
	FADER_PREVIEW_KEY_CHANGED	= ( 1<<1 ),
	FADER_AMOUNT_CHANGED		= ( 1<<2 ),
	FADER_PRESET_CHANGED		= ( 1<<3 ),
};


//-----------------------------------------------------------------------------
// CBaseAnimSetAttributeSliderPanel
//-----------------------------------------------------------------------------
class CBaseAnimSetAttributeSliderPanel : public vgui::EditablePanel, public IAnimationSetControlSelectionChangedListener
{
	DECLARE_CLASS_SIMPLE( CBaseAnimSetAttributeSliderPanel, vgui::EditablePanel );
public:
	CBaseAnimSetAttributeSliderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor );

public:

	virtual void						ChangeAnimationSetClip( CDmeFilmClip *pFilmClip );
	virtual void						OnControlsAddedOrRemoved();

	CBaseAnimationSetEditor*			GetEditor();
	virtual CBaseAnimationSetControl*	GetController() { return m_pController; }

	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	// These funcs only meaningful in derived/outer classes (SFM)
	virtual void						StampValueIntoLogs( CDmElement *control, AnimationControlType_t type, const float &flValue ) {} 
	virtual void						StampValueIntoLogs( CDmElement *control, AnimationControlType_t type, const Vector &vecValue ) {}
	virtual void						StampValueIntoLogs( CDmElement *control, AnimationControlType_t type, const Quaternion &qValue ) {}

	virtual void						GetTypeInValueForControl( CDmElement *pControl, bool bOrientation, AttributeValue_t &controlValue, const AttributeValue_t &sliderValue );

	virtual void						UpdatePreview( char const *pchFormat, ... );

	virtual void						DispatchCurve( int nCurveType );

	CAttributeSlider					*FindSliderForControl( const CDmElement *control );

	// Returns true if slider is visible
	bool								GetSliderValues( AttributeValue_t *pValue, int nIndex );

	virtual void SetupForPreset( FaderPreview_t &fader );

	float GetBalanceSliderValue();

	// inherited from IAnimationSetControlSelectionChangedListener
	virtual void OnControlSelectionChanged();

protected:

	virtual void OnThink();
	virtual void OnTick();
	virtual void OnCommand( const char *pCommand );
	virtual bool ApplySliderValues( bool force );
	virtual void UpdateControlSetMode( bool changingvalues, bool previewing, CAttributeSlider *dragSlider ) {}

	virtual void PerformLayout();

protected:
	int										FindSliderIndexForControl( const CDmElement *control );

	void									UpdateSliderDependencyFlags() const;

	void									RebuildSliderLists();

	// these are just temporary accessors for the CBaseAnimationSetControl until more code is moved over
	friend CBaseAnimationSetControl;
	int									GetSliderCount() const { return m_SliderList.Count(); }
	CAttributeSlider					*GetSlider( int i ) { return m_SliderList[ i ]; }

	CAttributeSlider *AllocateSlider();
	void FreeSlider( CAttributeSlider *slider ); 
	void InitFreeSliderList( int nCount );

	vgui::DHANDLE< CBaseAnimationSetEditor >	m_hEditor;
	// Visible slider list
	vgui::DHANDLE< vgui::PanelListPanel >	m_Sliders;
	// All sliders
	CUtlVector< CAttributeSlider * >		m_SliderList;
	vgui::Button							*m_pLeftRightBoth[ 2 ];
	CPresetSideFilterSlider				*m_pPresetSideFilter;

	CBaseAnimationSetControl			*m_pController;

	CUtlVector< CAttributeSlider * >	m_FreeSliderList;


};

inline CBaseAnimationSetEditor*	CBaseAnimSetAttributeSliderPanel::GetEditor()
{
	return m_hEditor;
}

#endif // BASEANIMSETATTRIBUTESLIDERPANEL_H

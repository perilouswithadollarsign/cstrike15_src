//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef SEQUENCEPICKER_H
#define SEQUENCEPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ImageList.h"
#include "datacache/imdlcache.h"
#include "matsys_controls/mdlpanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Splitter;
}


//-----------------------------------------------------------------------------
// Purpose: Sequence picker panel
//-----------------------------------------------------------------------------
class CSequencePicker : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CSequencePicker, vgui::EditablePanel );

public:
	enum PickType_t
	{
		PICK_NONE					= 0,
		PICK_SEQUENCES				= 0x1,
		PICK_ACTIVITIES 			= 0x2,
		PICK_ALL					= PICK_SEQUENCES | PICK_ACTIVITIES,
		PICK_SEQUENCE_PARAMETERS	= 0x10,
	};

	static const int NUM_POSE_CONTROLS = 6;
	static const int NUM_SEQUENCE_LAYERS = 4;

	// Flags come from PickType_t
	CSequencePicker( vgui::Panel *pParent, int nFlags = PICK_ALL );
	~CSequencePicker();

	// overridden frame functions
	virtual void PerformLayout();

	// Process command messages
	virtual void OnCommand( const char *pCommand );

	// Sets the MDL to preview sequences for
	void SetMDL( const char *pMDLName );

	// Gets the selected activity/sequence
	PickType_t GetSelectedSequenceType();
	const char *GetSelectedSequenceName( );

	// Get the array of pose parameter values
	void GetPoseParameters( CUtlVector< float > &poseParameters ) const;

	// Get the array of sequence layers 
	void GetSeqenceLayers( CUtlVector< MDLSquenceLayer_t > &sequenceLayers ) const;

	// Get the root motion generation state
	bool GetGenerateRootMotion() const;


private:

	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", pData );
	MESSAGE_FUNC_PARAMS( OnTextKillFocus, "TextKillFocus", pData );
	MESSAGE_FUNC_PARAMS( OnTextNewLine, "TextNewLine", pData );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", pData );	
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );
	MESSAGE_FUNC( OnPageChanged, "PageChanged" );

	void RefreshActivitiesAndSequencesList();

	// Plays the selected activity
	void PlayActivity( const char *pActivityName );

	// Update the controls and plays the selected sequence
	void UpdateActiveSequence( const char *pSequenceName );

	// Find the index of the sequence with the specified name
	int FindSequence( const char *pSequenceName ) const;

	// Update the set of available pose parameters 
	void UpdateAvailablePoseParmeters();

	// Update the value of the specified control control group
	void SetPoseParameterValue( float flPoseParameterValue, int nParameterIndex );

	// Set all pose parameters to their default values
	void ResetPoseParametersToDefault();

	// Update the pose parameter controls using the stored pose parameter values
	void UpdatePoseControlsFromParameters();

	// Update the pose parameter controls to match the set of pose parameters for the current mdl
	void UpdatePoseParameterControlsForMdl();

	// Update the sequence and weighting of the specified layer
	void SetSequenceLayer( int nLayerIndex, int nSequenceIndex, float flWeight );

	// Update the sequence layer controls to match the current layer data set
	void UpdateLayerControls();

	// Clear all the layer information and update the controls
	void ResetLayers();




	CMDLPanel			*m_pMDLPreview;
	vgui::Splitter		*m_pPreviewSplitter;
	vgui::PropertySheet *m_pViewsSheet;
	vgui::PropertyPage	*m_pSequencesPage;
	vgui::PropertyPage	*m_pActivitiesPage;
	vgui::ListPanel		*m_pSequencesList;
	vgui::ListPanel		*m_pActivitiesList;

	vgui::ComboBox		*m_pLayerSequenceSelectors[ NUM_SEQUENCE_LAYERS ];
	vgui::Slider		*m_pLayerSequenceSliders[ NUM_SEQUENCE_LAYERS ];

	vgui::CheckButton	*m_pRootMotionCheckBox;
	vgui::Button		*m_pPoseDefaultButton;
	vgui::Slider		*m_pPoseValueSliders[ NUM_POSE_CONTROLS ];
	vgui::TextEntry		*m_pPoseValueEntries[ NUM_POSE_CONTROLS ];
	vgui::ComboBox		*m_pPoseParameterName[ NUM_POSE_CONTROLS ];

	MDLSquenceLayer_t	m_SequenceLayers[ NUM_SEQUENCE_LAYERS ];	
	int					m_PoseControlMap[ NUM_POSE_CONTROLS ];		// Provides index of pose parameter driven by each control
	float				m_PoseParameters[ MAXSTUDIOPOSEPARAM ];		// Array of pose parameter values to be used when rendering the mdl

	bool		m_bSequenceParams;
	MDLHandle_t m_hSelectedMDL;
	CUtlString	m_Filter;

	friend class CSequencePickerFrame;
};


//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CSequencePickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CSequencePickerFrame, vgui::Frame );

public:
	CSequencePickerFrame( vgui::Panel *pParent, int nFlags );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

	// Purpose: Activate the dialog
	void DoModal( const char *pMDLName );

private:
	MESSAGE_FUNC_PARAMS( OnSequencePreviewChanged, "SequencePreviewChanged", kv );

	CSequencePicker *m_pPicker;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
};


#endif // SEQUENCEPICKER_H

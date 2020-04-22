//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ATTRIBUTESLIDER_H
#define ATTRIBUTESLIDER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "movieobjects/animsetattributevalue.h"
#include "movieobjects/dmelog.h"
#include "datamodel/dmehandle.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimSetAttributeSliderPanel;
class CDmElement;
class CAttributeSliderTextEntry;
class CSubRectImage;


//-----------------------------------------------------------------------------
// CAttributeSlider
//-----------------------------------------------------------------------------

// THIS CODE IS KIND OF A MESS WRT THE VARIOUS STATES WE CAN BE IN:
// we can be driven by the preset pane or by dragging on any individual control
// we can also be driven by ctrl hovering over the preset pane or an individual control
// if we move from control to control in the preset or here, we need to be able to decay into/out of the various individual sliders
class CAttributeSlider : public EditablePanel 
{
	DECLARE_CLASS_SIMPLE( CAttributeSlider, EditablePanel );

	// Overridden methods of EditablePanel
public:
	virtual void Paint();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnCursorMoved(int x, int y);
	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);
	virtual void OnMouseReleased(MouseCode code);
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void OnKeyCodeTyped( KeyCode code );

	// Other public methods
public:
	CAttributeSlider( CBaseAnimSetAttributeSliderPanel *parent );
	virtual ~CAttributeSlider();

	void Init( CDmElement *control, bool bOrientation );

// Things that rely on a specific type
	void SetValue( AnimationControlType_t type, float flValue );
	void SetValue( AnimationControlType_t type, const Vector &vec );
	void SetValue( AnimationControlType_t type, const Quaternion &quat );
	float GetValue( AnimationControlType_t type ) const;
	void GetValue( AnimationControlType_t type, Vector &vec ) const;
	void GetValue( AnimationControlType_t type, Quaternion &quat ) const;

	float		GetPreview( AnimationControlType_t type ) const;
	void		GetPreview( AnimationControlType_t type, Vector &vec ) const;
	void		GetPreview( AnimationControlType_t type, Quaternion &quat ) const;

	// Estimates the value of the control given a local coordinate
	float EstimateValueAtPos( int nLocalX, int nLocalY ) const;

	// Returns the control we're modifying
	CDmElement *GetControl();
	const CDmElement *GetControl() const;

	// sets internal state to dme control's value
	void InitControls();

	// Gets/sets the slider value. 
	// NOTE: This may not match the value pushed into the control because of fading
	void SetValue( const AttributeValue_t& value );
	const AttributeValue_t& GetValue() const;

	// Is this slider manipulating a transform control? 
	// [NOTE: This is a utility method; the control contains these states]
	bool IsTransform() const;
	bool IsOrientation() const;
	bool IsStereo() const;

	// Are we dragging?
	bool IsDragging() const;

	// Are we in text entry mode?
	bool IsInTextEntry() const;


	void		SetPreview( const AttributeValue_t &value, const AttributeValue_t &full );
	const AttributeValue_t &GetPreview() const;
	const AttributeValue_t &GetPreviewFull() const;

	void		UpdateFaderAmount( float amount );


	void		 SetVisibleComponents( LogComponents_t componentFlags );
	LogComponents_t VisibleComponents() const;

	// Slider dependency functions, provide information about which other sliders on which the function of this slider depends
	void ClearDependencies();
	bool AddDependency( const CAttributeSlider* pSlider );
	bool IsDependent( const CAttributeSlider* pSlider ) const;
	void SetDependent( bool dependent );

	MESSAGE_FUNC( OnSetToDefault, "SetToDefault" );
	MESSAGE_FUNC( OnEditMinMaxDefault, "EditMinMaxDefault" );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", params );

protected:

	KEYBINDING_FUNC( ts_curve_1,		KEY_1, 0,								OnCurve1,			"#ts_curve1_help", 0 );
	KEYBINDING_FUNC( ts_curve_2,		KEY_2, 0,								OnCurve2,			"#ts_curve2_help", 0 );
	KEYBINDING_FUNC( ts_curve_3,		KEY_3, 0,								OnCurve3,			"#ts_curve3_help", 0 );
	KEYBINDING_FUNC( ts_curve_4,		KEY_4, 0,								OnCurve4,			"#ts_curve4_help", 0 );

private:
	// Various slider modes
	enum SliderMode_t
	{
		SLIDER_MODE_NONE,
		SLIDER_MODE_TEXT,
		SLIDER_MODE_DRAG_VALUE, // scalar, position or orientation
	};

private:
// Things that rely on type
	// Called by the text entry code to enter the value into the logs
	void StampValueIntoLogs( AnimationControlType_t type, float flValue );
	void StampValueIntoLogs( AnimationControlType_t type, const Vector &vecValue );
	void StampValueIntoLogs( AnimationControlType_t type, const Quaternion &qValue );
	void DrawValueLabel();
// Other stuff

	void ActivateControl( AnimationControlType_t type, bool bActive );

	// Returns the location of a particular control
	void GetControlRect( Rect_t *pRect ) const;

	// Methods related to rendering
	void DrawMidpoint( int x, int ty, int ttall );
	void DrawTick( const Color& clr, const AttributeValue_t &value, int width, int inset );
	void DrawNameLabel();
	void PaintSliderBackGround();

	float GetPreviewAlphaScale() const;

	// Methods related to text entry mode
	void EnterTextEntryMode( bool bRelatchValues );
	void AcceptTextEntryValue();
	void DiscardTextEntryValue();
	void SetupTextFieldForTextEntryMode( CAttributeSliderTextEntry *&pTextField, const char *pText, bool bRequestFocus );

	void SetToDefault();

private:
	CBaseAnimSetAttributeSliderPanel *m_pParent;
	TextImage *m_pName;
	TextImage *m_pValues[ 4 ];

	// This is the control we're modifying
	CDmeHandle< CDmElement > m_hControl;

	// The current mode of the slider
	SliderMode_t m_SliderMode;

	// The slider value; it may not match the control attribute value due to blending
	AttributeValue_t m_Control;

	// Info used when in text entry mode
	AttributeValue_t m_InitialTextEntryValue;
	CAttributeSliderTextEntry *m_pTextField; // if this is a stereo control, then this will be the left text field
	CAttributeSliderTextEntry *m_pRightTextField;

	AttributeValue_t m_PreviewCurrent;
	AttributeValue_t m_PreviewFull;
	float			m_flFaderAmount;

	// Fields used to help with drag
	int				m_nDragStartPosition[2];	// Where was the mouse clicked?
	int				m_nAccum[2];				// What's the total mouse movement during the drag?
	AttributeValue_t m_dragStartValues;			// What was the value of the slider before the drag started?

	LogComponents_t	m_nVisibleComponents;		// Flags used to specify specific components that are visible

	bool			m_bTransform : 1;
	bool			m_bOrientation : 1;
	bool			m_bStereo : 1;
	bool			m_bDependent : 1;

	CUtlVector< const CAttributeSlider* > m_Dependenices;	// List of sliders that the operation of this slider depends on

	vgui::DHANDLE< vgui::Menu > m_hContextMenu;


	friend class CAttributeSliderTextEntry;
};

/*
class CAttributeSliderData
{
//	CDmeHandle< CDmElement > m_hControl;
	int					m_nIndex; // its index, the control's index, and the slider's index

	AttributeValue_t	m_Control;
	AttributeValue_t	m_InitialTextEntryValue;
	AttributeValue_t	m_dragStartValues;
	AttributeValue_t	m_PreviewCurrent;
	AttributeValue_t	m_PreviewFull;

	AttributeValue_t	m_CurrentValue;
	AttributeValue_t	m_SavedValue;
	AttributeValue_t	m_FullPresetValue;

	float				m_flFaderAmount;
	unsigned int		m_nVisibleComponents;
	CUtlVector< const CAttributeSliderData* > m_Dependenices;
};
*/


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Returns the control
//-----------------------------------------------------------------------------
inline CDmElement *CAttributeSlider::GetControl()
{
	return m_hControl;
}

inline const CDmElement *CAttributeSlider::GetControl() const
{
	return m_hControl;
}



//-----------------------------------------------------------------------------
// Returns information about the control
//-----------------------------------------------------------------------------
inline bool CAttributeSlider::IsTransform() const
{
	return m_bTransform;
}

inline bool CAttributeSlider::IsOrientation() const
{
	return m_bOrientation;
}

inline bool CAttributeSlider::IsStereo() const
{
	return m_bStereo;
}

//-----------------------------------------------------------------------------
// Are we dragging?
//-----------------------------------------------------------------------------
inline bool CAttributeSlider::IsDragging() const
{
	return m_SliderMode == SLIDER_MODE_DRAG_VALUE;
}


//-----------------------------------------------------------------------------
// Are we in text entry mode?
//-----------------------------------------------------------------------------
inline bool CAttributeSlider::IsInTextEntry() const
{
	return m_SliderMode == SLIDER_MODE_TEXT;
}

#endif // ATTRIBUTESLIDER_H

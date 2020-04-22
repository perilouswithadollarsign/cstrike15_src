//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMATIONSETEDITORCONTROLLER_H
#define BASEANIMATIONSETEDITORCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmehandle.h"
#include "movieobjects/animsetattributevalue.h"
#include "dme_controls/dmecontrols_utils.h"
#include "dme_controls/RecordingState.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct SelectionInfo_t;
class CDmeAnimationSet;
class CDmeAnimationList;
class CDmeChannelsClip;
class CDmeChannel;
class CBaseAnimationSetEditor;
class CAttributeSlider;
class CNotifyAnimationSetControlSelectionChangedScopeGuard;
struct AttributeValue_t;


enum ESelectionMode
{
	SELECTION_SET,
	SELECTION_ADD,
	SELECTION_REMOVE,
	SELECTION_TOGGLE,
};	

// NOTE - these values were chosen specifically to allow bitwise or (|) to combine selection states together properly
enum SelectionState_t
{
	SEL_EMPTY = 0,
	SEL_NONE = 1,
	SEL_ALL = 2,
	SEL_SOME = 3,
};
inline SelectionState_t operator+( SelectionState_t a, SelectionState_t b )
{
	return SelectionState_t( int( a ) | int( b ) );
}
inline SelectionState_t operator+=( SelectionState_t &a, SelectionState_t b )
{
	return a = a + b;
}


enum TransformComponent_t
{
	TRANSFORM_COMPONENT_NONE = 0,
	TRANSFORM_COMPONENT_POSITION_X = ( 1 << 0 ),
	TRANSFORM_COMPONENT_POSITION_Y = ( 1 << 1 ),
	TRANSFORM_COMPONENT_POSITION_Z = ( 1 << 2 ),
	TRANSFORM_COMPONENT_ROTATION_X = ( 1 << 3 ),
	TRANSFORM_COMPONENT_ROTATION_Y = ( 1 << 4 ),
	TRANSFORM_COMPONENT_ROTATION_Z = ( 1 << 5 ),
	TRANSFORM_COMPONENT_POSITION = TRANSFORM_COMPONENT_POSITION_X | TRANSFORM_COMPONENT_POSITION_Y | TRANSFORM_COMPONENT_POSITION_Z,
	TRANSFORM_COMPONENT_ROTATION = TRANSFORM_COMPONENT_ROTATION_X | TRANSFORM_COMPONENT_ROTATION_Y | TRANSFORM_COMPONENT_ROTATION_Z,
	TRANSFORM_COMPONENT_ALL = TRANSFORM_COMPONENT_POSITION | TRANSFORM_COMPONENT_ROTATION
};
DEFINE_ENUM_BITWISE_OPERATORS( TransformComponent_t )


class IOverrideParentChangedListener
{
public:
	virtual void OnOverrideParentChanged( CDmeDag *pChildDag ) = 0;
};

class IAnimationSetControlSelectionChangedListener
{
public:
	virtual void OnControlSelectionChanged() {}
	virtual void OnRebuildControlHierarchy() {}
	virtual void ExpandTreeToControl( const CDmElement *pSelection, TransformComponent_t nComponentFlags ) {}
};


struct SelectionInfo_t
{
	DECLARE_FIXEDSIZE_ALLOCATOR( SelectionInfo_t );

public:
	SelectionInfo_t() : m_nComponentFlags( TRANSFORM_COMPONENT_NONE ) {}
	SelectionInfo_t( CDmeAnimationSet *pAnimSet, CDmElement *pControl, TransformComponent_t nComponentFlags = TRANSFORM_COMPONENT_NONE )
		: m_hAnimSet( pAnimSet ), m_hControl( pControl ), m_nComponentFlags( nComponentFlags )
	{
	}

	bool IsPositionFullySelected() const
	{
		return ( ( m_nComponentFlags & TRANSFORM_COMPONENT_POSITION ) == TRANSFORM_COMPONENT_POSITION );
	}

	bool IsRotationFullySelected() const
	{
		return ( ( m_nComponentFlags & TRANSFORM_COMPONENT_ROTATION ) == TRANSFORM_COMPONENT_ROTATION );
	}

	bool AreAnyPositionComponentsSelected()
	{
		return ( ( m_nComponentFlags & TRANSFORM_COMPONENT_POSITION ) > 0 );
	}

	bool AreAnyOrientationComponentsSelected()
	{
		return ( ( m_nComponentFlags & TRANSFORM_COMPONENT_ROTATION ) > 0 );
	}

	static LogComponents_t ConvertTransformFlagsToLogFlags( TransformComponent_t nTransformFlags, bool bOrientation )
	{
		LogComponents_t nFlags = LOG_COMPONENTS_NONE;
		if ( bOrientation )
		{
			nFlags |= ( ( nTransformFlags & TRANSFORM_COMPONENT_ROTATION_X ) > 0 ) ? LOG_COMPONENTS_X : LOG_COMPONENTS_NONE;
			nFlags |= ( ( nTransformFlags & TRANSFORM_COMPONENT_ROTATION_Y ) > 0 ) ? LOG_COMPONENTS_Y : LOG_COMPONENTS_NONE;
			nFlags |= ( ( nTransformFlags & TRANSFORM_COMPONENT_ROTATION_Z ) > 0 ) ? LOG_COMPONENTS_Z : LOG_COMPONENTS_NONE;
		}
		else
		{
			nFlags |= ( ( nTransformFlags & TRANSFORM_COMPONENT_POSITION_X ) > 0 ) ? LOG_COMPONENTS_X : LOG_COMPONENTS_NONE;
			nFlags |= ( ( nTransformFlags & TRANSFORM_COMPONENT_POSITION_Y ) > 0 ) ? LOG_COMPONENTS_Y : LOG_COMPONENTS_NONE;
			nFlags |= ( ( nTransformFlags & TRANSFORM_COMPONENT_POSITION_Z ) > 0 ) ? LOG_COMPONENTS_Z : LOG_COMPONENTS_NONE;
		}
		return nFlags;	
	}

	static TransformComponent_t ConvertLogFlagsToTransformFlags( LogComponents_t nLogFlags, bool bOrientation )
	{
		TransformComponent_t nFlags = TRANSFORM_COMPONENT_NONE;		
		if ( bOrientation )
		{
			nFlags |= ( ( nLogFlags & LOG_COMPONENTS_X ) > 0 ) ? TRANSFORM_COMPONENT_ROTATION_X : TRANSFORM_COMPONENT_NONE;
			nFlags |= ( ( nLogFlags & LOG_COMPONENTS_Y ) > 0 ) ? TRANSFORM_COMPONENT_ROTATION_Y : TRANSFORM_COMPONENT_NONE;
			nFlags |= ( ( nLogFlags & LOG_COMPONENTS_Z ) > 0 ) ? TRANSFORM_COMPONENT_ROTATION_Z : TRANSFORM_COMPONENT_NONE;
		}
		else
		{
			nFlags |= ( ( nLogFlags & LOG_COMPONENTS_X ) > 0 ) ? TRANSFORM_COMPONENT_POSITION_X : TRANSFORM_COMPONENT_NONE;
			nFlags |= ( ( nLogFlags & LOG_COMPONENTS_Y ) > 0 ) ? TRANSFORM_COMPONENT_POSITION_Y : TRANSFORM_COMPONENT_NONE;
			nFlags |= ( ( nLogFlags & LOG_COMPONENTS_Z ) > 0 ) ? TRANSFORM_COMPONENT_POSITION_Z : TRANSFORM_COMPONENT_NONE;
		}
		return nFlags;
	}

	CDmeHandle< CDmeAnimationSet >	m_hAnimSet;
	CDmeHandle< CDmElement       >	m_hControl;
	TransformComponent_t			m_nComponentFlags;
};



template < class T >
void RebuildControlList( CUtlVector< T* > &controlList, CDmeFilmClip *pFilmClip )
{
	int nControls = controlList.Count();
	for ( int i = 0; i < nControls; ++i )
	{
		delete controlList[ i ];
	}

	controlList.RemoveAll();

	CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmaElementArray< CDmElement > &controls = pAnimSet->GetControls();
		int nControls = controls.Count();
		for ( int i = 0; i < nControls; ++i )
		{
			CDmElement *pControl = controls[ i ];
			if ( !pControl )
				continue;

			controlList.AddToTail( new T( pAnimSet, pControl, TRANSFORM_COMPONENT_NONE ) );
		}
	}
}

template < class T >
T *FindSelectionInfoForControl( CUtlVector< T* > &controlList, const CDmElement *pControl )
{
	if ( !pControl )
		return NULL;

	int nControls = controlList.Count();
	for ( int i = 0; i < nControls; ++i )
	{
		T *pT = controlList[ i ];
		if ( pT->m_hControl.Get() == pControl )
			return pT;
	}

	return NULL;
}

template < class T >
void RemoveNullControls( CUtlVector< T* > &controlList )
{
	int nControls = controlList.Count();
	for ( int i = nControls-1; i >= 0; --i )
	{
		if ( controlList[ i ]->m_hControl )
			continue; // TODO - are there other conditions that could cause a control to exist, but not want to be part of the full list?

		delete controlList[ i ];
		controlList.Remove( i );
	}
}

template < class T >
void AddMissingControls( CUtlVector< T* > &controlList, CDmeFilmClip *pFilmClip )
{
	CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmaElementArray< CDmElement > &controls = pAnimSet->GetControls();
		int nControls = controls.Count();
		for ( int i = 0; i < nControls; ++i )
		{
			CDmElement *pControl = controls[ i ];
			if ( !pControl )
				continue;

			if ( FindSelectionInfoForControl( controlList, pControl ) )
				continue;

			controlList.AddToTail( new T( pAnimSet, pControl, TRANSFORM_COMPONENT_NONE ) );
		}
	}
}




//-----------------------------------------------------------------------------
// Base class for the panel for editing animation sets
//-----------------------------------------------------------------------------
class CBaseAnimationSetControl
{
public:
	CBaseAnimationSetControl();
	~CBaseAnimationSetControl();

	void SetAnimationSetEditorPanel( CBaseAnimationSetEditor *pEditor ) { m_pEditor = pEditor; }

	virtual void						ChangeAnimationSetClip( CDmeFilmClip *pFilmClip );
	virtual void						OnControlsAddedOrRemoved();

	virtual RecordingState_t			GetRecordingState() const { return AS_PREVIEW; }
	CDmeFilmClip						*GetAnimationSetClip();

	virtual void						OnSliderRangeRemapped() {}

	void AddOverrideParentChangedListener( IOverrideParentChangedListener *pListener );
	void RemoveOverrideParentChangedListener( IOverrideParentChangedListener *pListener );

	void AddControlSelectionChangedListener   ( IAnimationSetControlSelectionChangedListener *listener );
	void RemoveControlSelectionChangedListener( IAnimationSetControlSelectionChangedListener *listener );

	void ClearSelection();
	SelectionState_t GetSelectionState( CDmeAnimationSet *pAnimSet ) const;
	SelectionState_t GetSelectionState( CDmeControlGroup *pControlGroup ) const;
	SelectionState_t GetSelectionState( CDmElement *pControl, TransformComponent_t componentFlags = TRANSFORM_COMPONENT_ALL ) const;
	void SetRangeSelectionState( bool bInRangeSelection );
	void SelectAnimationSet( CDmeAnimationSet *pAnimSet, ESelectionMode selectionMode = SELECTION_ADD );
	void SelectControlGroup( CDmeControlGroup *pControlGroup, ESelectionMode selectionMode = SELECTION_ADD );
	void SelectControlForDag( const CDmeDag *pDag, ESelectionMode selectionMode = SELECTION_ADD );
	bool SelectControl( const CDmElement *pControl, ESelectionMode selectionMode = SELECTION_ADD, TransformComponent_t componentFlags = TRANSFORM_COMPONENT_ALL, bool bExpandTree = false );
	void SaveSelection( CUtlVector< SelectionInfo_t > &selection ) const;
	void RestoreSelection( const CUtlVector< SelectionInfo_t > &selection );
	void DeselectHiddenControls();

	CDmElement *GetMostRecentlySelectedControl();

	bool IsControlSelected( CDmElement *pControl ) const;
	TransformComponent_t GetSelectionComponentFlags( CDmElement *pControl ) const;

	virtual void ProceduralPreset_UpdateCrossfade( AttributeDict_t *values, int nPresetType );

	void SetWorkCameraParent( CDmeDag *pWorkCameraParent );
	CDmeDag *GetWorkCameraParent();

	void SetPresetFromControls( const char *pPresetGroupName, const char *pPresetName );
	void AddPreset( const char *pPresetGroupName, const char *pPresetName, bool bAnimated );

	void UpdatePreviewSliderValues();
	void UpdatePreviewSliderTimes();
	bool IsPresetFaderBeingDragged() const;
	void UpdateDominantSliderStartValues( bool restoreSliderValues );
	void GetDominantSliderValues( float &flDomStart, float &flDomValue );
	void ApplyPreset( float flScale, AttributeDict_t& values );

	bool ApplySliderValues( bool force );

	void SetActiveAttributeSlider( CAttributeSlider *pSlider );
	CAttributeSlider *GetActiveAttributeSlider() { return m_ActiveAttributeSlider; }

	// HACK - these should be removed after creating the CAnimationSetControl,
	// and CAnimSetAttributeSliderPanel::ApplySliderValues is moved to there
	float GetPreviousPresetAmount() const { return m_flPreviousPresetAmount; }
	bool WasPreviouslyHoldingPresetPreviewKey() const { return m_bPreviouslyHoldingPresetPreviewKey; }
	bool HasPresetSliderChanged() const { return m_bPresetSliderChanged; }
	float GetDominantSliderStartValue( int nSliderIndex, AnimationControlType_t type );
	int GetDominantSliderIndex() const { return m_nDominantSlider; }


	// Can the control be snapped to
	bool IsControlSnapTarget( const CDmElement *pControl ) const;
	
	// Is the control selectable in the viewport
	bool IsControlSelectable( const CDmElement *pControl ) const;

	// Determine if the specified control is visible
	bool IsControlVisible( const CDmElement *pControl ) const;

	// Determine if the specified control group is visible
	bool IsControlGroupVisible( const CDmeControlGroup *pGroup ) const;

	// Return the state indicating if hidden controls are being displayed
	bool IsShowingHiddenControls() const { return m_bShowHiddenControls; }

	// Set the state indicating if hidden controls are to be displayed
	void SetShowHiddenControls( bool showHidden ) { m_bShowHiddenControls = showHidden; }


protected:

	void AddPreset( CDmeAnimationSet *pAnimSet, const char *pPresetGroupName, const char *pPresetName, bool bAnimated );

	virtual void GetAnimatedPresetTimeParameters( DmeTime_t &tHead, DmeTime_t &tStart, DmeTime_t &tEnd );


	// from CBaseAnimSetControlGroupPanel:

	void FireOverrideParentChangedListeners( CDmeDag *pChildDag );
	virtual void FireControlSelectionChangedListeners();
	virtual void FireRebuildControlHierarchyListeners();

	template< class T >
	void ApplyTransformSliderValue( CAttributeSlider *pSlider, CDmeTransformControl *pControl, bool bUsePreviewValue, bool bForce, bool &valuesChanged, AnimationControlType_t type );
	void ApplySliderValueWithDominance( CAttributeSlider *pSlider, int si, float flDomStart, float flDomValue, CDmElement *pControl, bool bUsePreviewValue, bool bForce, bool &valuesChanged, AnimationControlType_t type, const char *pChannelAttrName, const char *pValueAttrName );

	virtual SelectionInfo_t *FindSelectionInfoForControl( const CDmElement *pControl ) = 0;

	void EnsureCrossfadePresetControlValues( int nCount );

protected:
	// from CBaseAnimationSetEditor:
	CDmeHandle< CDmeFilmClip >			m_hFilmClip;
	CBaseAnimationSetEditor				*m_pEditor;

	// from CBaseAnimSetControlGroupPanel:
	CUtlLinkedList< SelectionInfo_t* >	m_SelectionHistory;
	CDmeHandle< CDmeDag >				m_hWorkCameraParent;
	CUtlVector< IOverrideParentChangedListener * > m_OverrideParentChangedListeners;
	CUtlVector< IAnimationSetControlSelectionChangedListener * > m_ControlSelectionChangedListeners;

	// from CBaseAnimSetAttributeSliderPanel:
	CUtlString							m_PreviousPresetSlider;
	float								m_flPreviousPresetAmount;
	bool								m_bPresetPreviouslyDragged : 1;
	bool								m_bPreviouslyHoldingPresetPreviewKey : 1;
	bool								m_bPresetSliderChanged : 1;
	vgui::DHANDLE< CAttributeSlider >	m_ActiveAttributeSlider;
	CUtlVector< AttributeValue_t >		m_DominantSliderStartValues; // Values of all sliders at start of drag of dominant slider
	int									m_nDominantSlider;

	// Flag indicating if hidden controls are to be displayed
	bool								m_bShowHiddenControls;

	CUtlVector< CDmElement* >			m_crossfadePresetControlValues;


	friend CBaseAnimationSetEditor;
	friend CNotifyAnimationSetControlSelectionChangedScopeGuard;
};

#endif // BASEANIMATIONSETEDITORCONTROLLER_H

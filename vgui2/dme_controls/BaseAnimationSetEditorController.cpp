//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/BaseAnimationSetEditorController.h"
#include "tier1/fmtstr.h"
#include "tier1/KeyValues.h"
#include "tier2/fileutils.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/perforcefilelistframe.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/attributeslider.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmetransformcontrol.h"

#include "vgui/IInput.h"

#include <windows.h>
#undef PostMessage
#undef MessageBox

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Blends flex values in left-right space instead of balance/value space
//-----------------------------------------------------------------------------
void BlendValues( bool bTransform, AttributeValue_t *pResult, const AttributeValue_t &src, const AttributeValue_t &dest, float flBlend, float flBalanceFilter = 0.5f );


DEFINE_FIXEDSIZE_ALLOCATOR( SelectionInfo_t, 256, CUtlMemoryPool::GROW_SLOW );


// TODO - we really need to create a solid way of notifying panels when things they care about changes
// right now, we sometimes send vgui messages, sometimes call methods, sometimes use NotifyDataChanged, OnPreviewChanged, FireXXXChangedListeners, etc.
class CNotifyAnimationSetControlSelectionChangedScopeGuard
{
public:
	CNotifyAnimationSetControlSelectionChangedScopeGuard( CBaseAnimationSetControl *pControl )
		: m_pControl( pControl ), m_selectionMode( SELECTION_REMOVE )
	{
	}
	CNotifyAnimationSetControlSelectionChangedScopeGuard( CBaseAnimationSetControl *pControl, ESelectionMode selectionMode )
		: m_pControl( pControl ), m_selectionMode( selectionMode )
	{
		++m_nDepth;
	}
	~CNotifyAnimationSetControlSelectionChangedScopeGuard()
	{
		Assert( m_nDepth >= 0 );
		if ( m_nDepth > 0 )
		{
			Finish();
		}
	}

	void Start( ESelectionMode selectionMode )
	{
		m_selectionMode = selectionMode;
		++m_nDepth;
	}
	void Finish()
	{
		Assert( m_nDepth > 0 );
		if ( --m_nDepth == 0 )
		{
			m_pControl->FireControlSelectionChangedListeners();
		}
	}

private:
	CBaseAnimationSetControl *m_pControl;
	ESelectionMode m_selectionMode;
	static int m_nDepth;
};
int CNotifyAnimationSetControlSelectionChangedScopeGuard::m_nDepth = 0;



CBaseAnimationSetControl::CBaseAnimationSetControl() :
	m_pEditor( NULL ),
	m_bShowHiddenControls( false ),
	m_PreviousPresetSlider( "" ),
	m_flPreviousPresetAmount( 0.0f ),
	m_bPresetPreviouslyDragged( false ),
	m_bPreviouslyHoldingPresetPreviewKey( false ),
	m_bPresetSliderChanged( false ),
	m_nDominantSlider( -1 )
{
}

CBaseAnimationSetControl::~CBaseAnimationSetControl()
{
	m_ControlSelectionChangedListeners.RemoveAll();

	ChangeAnimationSetClip( NULL );

	int nCount = m_crossfadePresetControlValues.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		DestroyElement( m_crossfadePresetControlValues[ i ], TD_SHALLOW );
	}
}

void CBaseAnimationSetControl::ChangeAnimationSetClip( CDmeFilmClip *pFilmClip )
{
	m_hFilmClip = pFilmClip;

	// Force recomputation
	m_nDominantSlider = -1;

	SetWorkCameraParent( NULL );
	ClearSelection();

	if ( CBaseAnimSetPresetFaderPanel *pPresetFader = m_pEditor->GetPresetFader() )
	{
		pPresetFader->PopulatePresetList( true );
	}
}

void CBaseAnimationSetControl::OnControlsAddedOrRemoved()
{
	// Force recomputation
	m_nDominantSlider = -1;

	bool bSelectionChanged = false;
	for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); )
	{
		SelectionInfo_t *psi = m_SelectionHistory[ i ];
		int iCurr = i;
		i = m_SelectionHistory.Next( i );

		CDmElement *pControl = psi->m_hControl.Get();
		if ( pControl && IsControlVisible( pControl ) )
			continue;

		psi->m_nComponentFlags = TRANSFORM_COMPONENT_NONE;
		m_SelectionHistory.Remove( iCurr );
		bSelectionChanged = true;
	}

	if ( bSelectionChanged )
	{
		CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, SELECTION_REMOVE );
	}

	m_pEditor->GetPresetFader()->PopulatePresetList( false );
}


void CBaseAnimationSetControl::SetWorkCameraParent( CDmeDag *pParent )
{
	m_hWorkCameraParent = pParent;
}


CDmeDag *CBaseAnimationSetControl::GetWorkCameraParent()
{
	return m_hWorkCameraParent;
}

void CBaseAnimationSetControl::AddOverrideParentChangedListener( IOverrideParentChangedListener *pListener )
{
	if ( m_OverrideParentChangedListeners.Find( pListener ) == m_OverrideParentChangedListeners.InvalidIndex() )
	{
		m_OverrideParentChangedListeners.AddToTail( pListener );
	}
}

void CBaseAnimationSetControl::RemoveOverrideParentChangedListener( IOverrideParentChangedListener *pListener )
{
	m_OverrideParentChangedListeners.FindAndRemove( pListener );
}

void CBaseAnimationSetControl::FireOverrideParentChangedListeners( CDmeDag *pChildDag )
{
	int nNumListeners = m_OverrideParentChangedListeners.Count();
	for ( int i = 0; i < nNumListeners; ++i )
	{
		m_OverrideParentChangedListeners[ i ]->OnOverrideParentChanged( pChildDag );
	}
}

void CBaseAnimationSetControl::AddControlSelectionChangedListener( IAnimationSetControlSelectionChangedListener *listener )
{
	if ( m_ControlSelectionChangedListeners.Find( listener ) == m_ControlSelectionChangedListeners.InvalidIndex() )
	{
		m_ControlSelectionChangedListeners.AddToTail( listener );
	}
}

void CBaseAnimationSetControl::RemoveControlSelectionChangedListener( IAnimationSetControlSelectionChangedListener *listener )
{
	m_ControlSelectionChangedListeners.FindAndRemove( listener );
}

void CBaseAnimationSetControl::FireControlSelectionChangedListeners()
{
	for ( int i = 0; i < m_ControlSelectionChangedListeners.Count(); ++i )
	{
		m_ControlSelectionChangedListeners[ i ]->OnControlSelectionChanged();
	}
}

void CBaseAnimationSetControl::FireRebuildControlHierarchyListeners()
{
	for ( int i = 0; i < m_ControlSelectionChangedListeners.Count(); ++i )
	{
		m_ControlSelectionChangedListeners[ i ]->OnRebuildControlHierarchy();
	}
}


CDmeFilmClip *CBaseAnimationSetControl::GetAnimationSetClip()
{
	return m_hFilmClip;
}

bool CBaseAnimationSetControl::IsControlSelected( CDmElement *pControl ) const
{
	return GetSelectionComponentFlags( pControl ) != 0;
}

void CBaseAnimationSetControl::ClearSelection()
{
	CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, SELECTION_REMOVE );

	for ( int i = m_SelectionHistory.Tail(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Previous( i ) )
	{
		m_SelectionHistory[ i ]->m_nComponentFlags = TRANSFORM_COMPONENT_NONE;
	}

	m_SelectionHistory.RemoveAll();
}

bool CBaseAnimationSetControl::SelectControl( const CDmElement *pControl, ESelectionMode selectionMode /*= SELECTION_ADD*/, TransformComponent_t nComponentFlags /*= TRANFORM_COMPONENT_ALL*/, bool bExpandTree /*= false*/ )
{
	CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, selectionMode );

	if ( !pControl )
		return false;

	// Check to see if the control is hidden, if it is hidden it may not be selected.
	if ( !IsControlVisible( pControl ) && ( selectionMode != SELECTION_REMOVE ) )
		return false;

	SelectionInfo_t *psi = FindSelectionInfoForControl( pControl );
	Assert( psi );
	if ( !psi )
		return false;

	if ( selectionMode == SELECTION_SET )
	{
		ClearSelection();
		psi->m_nComponentFlags = nComponentFlags;
	}
	else
	{
		if ( psi->m_nComponentFlags != 0 )
		{
			m_SelectionHistory.FindAndRemove( psi );
		}

		switch ( selectionMode )
		{
		case SELECTION_ADD:    psi->m_nComponentFlags |=  nComponentFlags; break;
		case SELECTION_REMOVE: psi->m_nComponentFlags &= ~nComponentFlags; break;
		case SELECTION_TOGGLE: psi->m_nComponentFlags ^=  nComponentFlags; break;
		}
	}

	if ( psi->m_nComponentFlags != 0 )
	{
		m_SelectionHistory.AddToTail( psi );

		if ( bExpandTree )
		{			
			for ( int i = 0; i < m_ControlSelectionChangedListeners.Count(); ++i )
			{
				m_ControlSelectionChangedListeners[ i ]->ExpandTreeToControl( pControl, nComponentFlags );
			}
		}
	}
	
	return true;
}


void CBaseAnimationSetControl::SaveSelection( CUtlVector< SelectionInfo_t > &selection ) const
{
	int nSelectionCount = m_SelectionHistory.Count();
	selection.RemoveAll();
	selection.EnsureCapacity( nSelectionCount );
	
	for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
	{
		const SelectionInfo_t *pSelection = m_SelectionHistory[ i ];
		if ( pSelection  )
		{
			selection.AddToTail( *pSelection );
		}
	}
}


void CBaseAnimationSetControl::RestoreSelection( const CUtlVector< SelectionInfo_t > &selection )
{
	CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, SELECTION_SET );

	ClearSelection();

	int nNumSelected = selection.Count();
	for ( int iSelected = 0; iSelected < nNumSelected; ++iSelected )
	{
		const SelectionInfo_t &selectionElement = selection[ iSelected ];
		
		CDmElement *pControl = g_pDataModel->GetElement( selectionElement.m_hControl );
		if ( pControl == NULL )
			continue;

		SelectionInfo_t *pSelectionInfo = FindSelectionInfoForControl( pControl );
		if ( pSelectionInfo == NULL )
			continue;

		pSelectionInfo->m_nComponentFlags = selectionElement.m_nComponentFlags;

		if ( pSelectionInfo->m_nComponentFlags != 0 )
		{
			m_SelectionHistory.AddToHead( pSelectionInfo );
		}
	}
}


void CBaseAnimationSetControl::DeselectHiddenControls()
{
	CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, SELECTION_REMOVE );

	// Find all of the selected controls which are hidden
	CUtlVector< const CDmElement* > hiddenControls( 0, m_SelectionHistory.Count() );
	for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
	{
		const SelectionInfo_t *pSelection = m_SelectionHistory[ i ];
		if ( pSelection  )
		{
			const CDmElement *pControl = pSelection->m_hControl;
			if ( pControl == NULL )
				continue;

			if ( IsControlVisible( pControl ) == false )
			{
				hiddenControls.AddToTail( pSelection->m_hControl );
			}
		}
	}

	// Remove the hidden controls from the selection
	int nNumHiddenControls = hiddenControls.Count();
	for ( int iControl = 0; iControl < nNumHiddenControls; ++iControl )
	{
		SelectControl( hiddenControls[ iControl ], SELECTION_REMOVE );
	}
}


void CBaseAnimationSetControl::SelectControlGroup( CDmeControlGroup *pGroup, ESelectionMode selectionMode /*= SELECTION_ADD*/ )
{
	CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, selectionMode );

	if ( !pGroup )
		return;

	CUtlVector< CDmElement * > list( 0, 32 );
	pGroup->GetControlsInGroup( list, true );

	for ( int j = list.Count() - 1; j >= 0 ; --j )
	{
		// Only set selection on the first control which was actually selected,
		// otherwise we'll de-select everything but the last control
		if ( SelectControl( list[ j ], selectionMode ) && ( selectionMode == SELECTION_SET ) )
		{
			selectionMode = SELECTION_ADD;
		}
	}
}


void CBaseAnimationSetControl::SelectControlForDag( const CDmeDag *pDag, ESelectionMode selectionMode )
{
	CDmeTransformControl *pTransformControl = pDag->FindTransformControl();
	if ( pTransformControl )
	{	
		SelectControl( pTransformControl, selectionMode, TRANSFORM_COMPONENT_ALL, true );
	}
}


void CBaseAnimationSetControl::SetRangeSelectionState( bool bInRangeSelection )
{
	static CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this );
	if ( bInRangeSelection )
	{
		sg.Start( SELECTION_SET );
	}
	else
	{
		sg.Finish();
	}
}


void CBaseAnimationSetControl::SelectAnimationSet( CDmeAnimationSet *pAnimSet, ESelectionMode selectionMode /*= SELECTION_ADD*/ )
{
	CNotifyAnimationSetControlSelectionChangedScopeGuard sg( this, selectionMode );

	SelectControlGroup( pAnimSet->GetRootControlGroup(), selectionMode );

	// Find all of the root of the aniamtion set
	CUtlVector< CDmeDag* > rootDagNodes;
	pAnimSet->FindRootDagNodes( rootDagNodes );
		
	// Make the fist selected root the primary selection
	// by moving it to the end of the selection list
	int nNumRoots = rootDagNodes.Count();
	for ( int iRoot = 0; iRoot < nNumRoots; ++iRoot )
	{		
		CDmeDag *pRootDag = rootDagNodes[ iRoot ];
		if ( pRootDag == NULL )
			continue;
		
		CDmeTransformControl *pControl = pRootDag->FindTransformControl();

		SelectionInfo_t *pSelectionInfo = FindSelectionInfoForControl( pControl );
		if ( ( pSelectionInfo ) && ( pSelectionInfo->m_nComponentFlags != 0 ) )
		{
			m_SelectionHistory.FindAndRemove( pSelectionInfo );
			m_SelectionHistory.AddToTail( pSelectionInfo );
			break;
		}
	}	
}


SelectionState_t CBaseAnimationSetControl::GetSelectionState( CDmeAnimationSet *pAnimSet ) const
{
	if ( !pAnimSet )
		return SEL_EMPTY;

	return GetSelectionState( pAnimSet->GetRootControlGroup() );
}

SelectionState_t CBaseAnimationSetControl::GetSelectionState( CDmeControlGroup *pControlGroup ) const
{
	if ( !pControlGroup )
		return SEL_EMPTY;

	SelectionState_t selection = SEL_EMPTY;

	const CDmaElementArray< CDmeControlGroup > &children = pControlGroup->Children();
	int nChildren = children.Count();
	for ( int i = 0; i < nChildren; ++i )
	{
		selection += GetSelectionState( children[ i ] );
		if ( selection == SEL_SOME )
			return SEL_SOME; // once we get to SEL_SOME, there we stay
	}

	const CDmaElementArray< CDmElement > &controls = pControlGroup->Controls();
	int nControls = controls.Count();
	for ( int i = 0; i < nControls; ++i )
	{
		selection += GetSelectionState( controls[ i ] );
		if ( selection == SEL_SOME )
			return SEL_SOME; // once we get to SEL_SOME, there we stay
	}

	return selection;
}

SelectionState_t CBaseAnimationSetControl::GetSelectionState( CDmElement *pControl, TransformComponent_t componentFlags /*= TRANSFORM_COMPONENT_ALL*/ ) const
{
	Assert( pControl );
	if ( ( pControl == NULL ) || !IsControlVisible( pControl ) )
		return SEL_EMPTY;

	TransformComponent_t nSelectionComponentFlags = GetSelectionComponentFlags( pControl );
	TransformComponent_t nCombinedComponentFlags = nSelectionComponentFlags & componentFlags;
	if ( nCombinedComponentFlags == 0 )
		return SEL_NONE;

	if ( nCombinedComponentFlags == componentFlags )
		return SEL_ALL;

	return SEL_SOME;
}


CDmElement *CBaseAnimationSetControl::GetMostRecentlySelectedControl()
{
	int i = m_SelectionHistory.Tail();
	if ( i == m_SelectionHistory.InvalidIndex() )
		return NULL;

	return m_SelectionHistory[ i ]->m_hControl;
}

TransformComponent_t CBaseAnimationSetControl::GetSelectionComponentFlags( CDmElement *pControl ) const
{
	if ( pControl == NULL )
		return TRANSFORM_COMPONENT_NONE;

	for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
	{
		const SelectionInfo_t *psi = m_SelectionHistory[ i ];
		if ( psi->m_hControl.Get() == pControl )
			return psi->m_nComponentFlags;
	}
	return TRANSFORM_COMPONENT_NONE;
}

void SetPresetFromControl( CDmePreset *pPreset, CDmElement *pControl )
{
	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pControl );

	if ( pTransformControl )
	{
		CDmElement *pControlValue = pPreset->FindOrAddControlValue( pControl->GetName() );
		if ( pControlValue == NULL )
			return;

		CDmeChannel *pPosChannel = pTransformControl->GetPositionChannel();
		if ( pPosChannel  )
		{
			pControlValue->SetValue< Vector >( "valuePosition", pTransformControl->GetPosition() );
		}

		CDmeChannel *pRotChannel = pTransformControl->GetOrientationChannel();
		if ( pRotChannel )
		{
			pControlValue->SetValue< Quaternion >( "valueOrientation", pTransformControl->GetOrientation() );
		}
	}
	else
	{
		// Stamp the control value
		CDmElement *pControlValue = pPreset->FindOrAddControlValue( pControl->GetName() );
		if ( IsStereoControl( pControl ) )
		{
			pControlValue->RemoveAttribute( "value" );
			pControlValue->SetValue< float >( "leftValue",  pControl->GetValue< float >( "leftValue" ) );
			pControlValue->SetValue< float >( "rightValue", pControl->GetValue< float >( "rightValue" ) );
		}
		else
		{
			pControlValue->SetValue< float >( "value", pControl->GetValue< float >( "value" ) );
			pControlValue->RemoveAttribute( "leftValue" );
			pControlValue->RemoveAttribute( "rightValue" );
		}
	}
}

template < class T >
void AddKeysToPreset( CDmePreset *pPreset, const char *pValuesAttrName, const char *pTimesAttrName, const CDmElement *pControl, const char *pChannelAttrName, DmeTime_t tHead, DmeTime_t tStart, DmeTime_t tEnd )
{
	if ( !pPreset || !pControl )
		return;

	CDmeChannel *pChannel = pControl->GetValueElement< CDmeChannel >( pChannelAttrName );
	if ( !pChannel )
		return;

	CDmElement *pControlValue = pPreset->FindOrAddControlValue( pControl->GetName() );

	CDmrArray< T >         values( pControlValue, pValuesAttrName, true );
	CDmrArray< DmeTime_t > times ( pControlValue, pTimesAttrName,  true );

	CDmeTypedLog< T > *pLog = CastElement< CDmeTypedLog< T > >( pChannel->GetLog() );
	CDmeChannelsClip *pChannelsClip = FindReferringElement< CDmeChannelsClip >( pChannel, "channels" );
	if ( !pLog || pLog->IsEmpty() || !pChannelsClip )
	{
		times.AddToTail( DMETIME_ZERO );

		T v;
		pChannel->GetInputValue( v );
		values.AddToTail( v );
		return;
	}

	DmeTime_t tLocalStart = pChannelsClip->ToChildMediaTime( tStart, false );
	DmeTime_t tLocalEnd   = pChannelsClip->ToChildMediaTime( tEnd,   false );

	bool bFirst = true;

	int nKeys = pLog->GetKeyCount();
	for ( int i = 0; i < nKeys; ++i )
	{
		DmeTime_t t = pLog->GetKeyTime( i );
		if ( t < tLocalStart )
			continue;

		if ( bFirst )
		{
			bFirst = false;
			times.AddToTail( tStart - tHead );
			values.AddToTail( pLog->GetValue( tLocalStart ) );
			if ( t == tLocalStart )
				continue;
		}

		if ( t >= tLocalEnd )
		{
			int nTimes = times.Count();
			DmeTime_t tLast = nTimes > 0 ? times[ nTimes - 1 ] : tEnd;
			if ( tLast <= tLocalEnd )
			{
				times.AddToTail( tEnd - tHead );
				values.AddToTail( pLog->GetValue( tLocalEnd ) );
			}
			break;
		}

		t = pChannelsClip->FromChildMediaTime( t, false );

		times.AddToTail( t - tHead );
		values.AddToTail( pLog->GetKeyValue( i ) );
	}
}

void SetPresetFromControlChannels( CDmePreset *pPreset, const CDmElement *pControl, DmeTime_t tHead, DmeTime_t tStart, DmeTime_t tEnd )
{
	if ( IsTransformControl( pControl ) )
	{
		AddKeysToPreset< Vector     >( pPreset, AS_VALUES_POSITION_ATTR,    AS_TIMES_POSITION_ATTR,    pControl, "positionChannel",    tHead, tStart, tEnd );
		AddKeysToPreset< Quaternion >( pPreset, AS_VALUES_ORIENTATION_ATTR, AS_TIMES_ORIENTATION_ATTR, pControl, "orientationChannel", tHead, tStart, tEnd );
	}

	if ( IsStereoControl( pControl ) )
	{
		AddKeysToPreset< float >( pPreset, AS_VALUES_LEFT_ATTR,  AS_TIMES_LEFT_ATTR,  pControl, "leftvaluechannel",  tHead, tStart, tEnd );
		AddKeysToPreset< float >( pPreset, AS_VALUES_RIGHT_ATTR, AS_TIMES_RIGHT_ATTR, pControl, "rightvaluechannel", tHead, tStart, tEnd );
	}
	else
	{
		AddKeysToPreset< float >( pPreset, AS_VALUES_ATTR, AS_TIMES_ATTR, pControl, "channel", tHead, tStart, tEnd );
	}
}

//-----------------------------------------------------------------------------
// Reads the current animation set control values, creates presets
//-----------------------------------------------------------------------------
void CBaseAnimationSetControl::SetPresetFromControls( const char *pPresetGroupName, const char *pPresetName )
{
	if ( !m_hFilmClip.Get() )
		return;

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Set Preset" );

	for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
	{
		SelectionInfo_t *psi = m_SelectionHistory[ i ];
		CDmeAnimationSet *pAnimSet = psi->m_hAnimSet;
		CDmElement *pControl = psi->m_hControl;
		if ( !pControl || !pAnimSet )
			continue;

		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		CDmePreset *pPreset = pPresetGroup->FindPreset( pPresetName );
		if ( !pPreset )
			continue;

		SetPresetFromControl( pPreset, pControl );
	}
}

void CBaseAnimationSetControl::AddPreset( const char *pPresetGroupName, const char *pPresetName, bool bAnimated )
{
	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Preset" );

	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		SelectionState_t selstate = GetSelectionState( pAnimSet );
		if ( selstate == SEL_EMPTY || selstate == SEL_NONE )
			continue;

		AddPreset( pAnimSet, pPresetGroupName, pPresetName, bAnimated );
	}
}

void CBaseAnimationSetControl::GetAnimatedPresetTimeParameters( DmeTime_t &tHead, DmeTime_t &tStart, DmeTime_t &tEnd )
{
	tHead  = DMETIME_ZERO;
	tStart = DMETIME_MINTIME / 2;
	tEnd   = DMETIME_MAXTIME / 2;
}

void CBaseAnimationSetControl::AddPreset( CDmeAnimationSet *pAnimSet, const char *pPresetGroupName, const char *pPresetName, bool bAnimated )
{
	CDmePresetGroup *pPresetGroup = pAnimSet->FindOrAddPresetGroup( pPresetGroupName );
	CDmePreset *pPreset = pPresetGroup->FindPreset( pPresetName );
	if ( pPreset )
	{
		Assert( 0 );
		return; // TODO - should this preset be skipped, deleted and re-added, or somehow merged? (merging gives undesirable results if the control selections differ)
	}

	pPreset = pPresetGroup->FindOrAddPreset( pPresetName );

	if ( bAnimated )
	{
		DmeTime_t tHead, tStart, tEnd;
		GetAnimatedPresetTimeParameters( tHead, tStart, tEnd );

		pPreset->SetValue( "animated", true );

		for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
		{
			SelectionInfo_t *psi = m_SelectionHistory[ i ];
			if ( psi->m_hAnimSet != pAnimSet )
				continue;

			// TODO - factor in componentFlags!!!

			SetPresetFromControlChannels( pPreset, psi->m_hControl, tHead, tStart, tEnd );
		}
	}
	else
	{
		for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
		{
			SelectionInfo_t *psi = m_SelectionHistory[ i ];
			if ( psi->m_hAnimSet != pAnimSet )
				continue;

			// TODO - factor in componentFlags!!!

			SetPresetFromControl( pPreset, psi->m_hControl );
		}
	}
}


//-----------------------------------------------------------------------------
// Can the control be snapped to
//-----------------------------------------------------------------------------
bool CBaseAnimationSetControl::IsControlSnapTarget( const CDmElement *pControl ) const
{
	CDmeControlGroup *pGroup = CDmeControlGroup::FindGroupContainingControl( pControl );
	if ( pGroup == NULL )
		return false;
	
	return pGroup->IsSnappable();
}


//-----------------------------------------------------------------------------
// Is the control selectable in the viewport
//-----------------------------------------------------------------------------
bool CBaseAnimationSetControl::IsControlSelectable( const CDmElement *pControl ) const
{
	CDmeControlGroup *pGroup = CDmeControlGroup::FindGroupContainingControl( pControl );
	if ( pGroup == NULL )
		return false;

	return ( pGroup->IsVisible() && pGroup->IsSelectable() );
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the specified control is hidden
//-----------------------------------------------------------------------------
bool CBaseAnimationSetControl::IsControlVisible( const CDmElement *pControl ) const
{
	if ( IsShowingHiddenControls() )
		return true;

	CDmeControlGroup *pGroup = CDmeControlGroup::FindGroupContainingControl( pControl );
	if ( pGroup )
	{
		return pGroup->IsVisible();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Determine if the specified control group is visible
//-----------------------------------------------------------------------------
bool CBaseAnimationSetControl::IsControlGroupVisible( const CDmeControlGroup *pGroup ) const
{
	if ( IsShowingHiddenControls() )
		return true;

	return pGroup->IsVisible();
}


void CBaseAnimationSetControl::UpdatePreviewSliderValues()
{
	CBaseAnimSetAttributeSliderPanel *pAttributeSlider = m_pEditor->GetAttributeSlider();
	if ( !pAttributeSlider )
		return;

	float flBalanceSliderValue = pAttributeSlider->GetBalanceSliderValue();

	CBaseAnimSetPresetFaderPanel *presets = m_pEditor->GetPresetFader();
	if ( !presets )
		return;

	FaderPreview_t fader;
	presets->GetPreviewFader( fader );

	bool nameChanged = fader.name && ( m_PreviousPresetSlider.IsEmpty() || Q_stricmp( m_PreviousPresetSlider.Get(), fader.name ) );
	bool beingDraggedChanged = fader.isbeingdragged != m_bPresetPreviouslyDragged;
	bool previewKeyChanged = fader.holdingPreviewKey != m_bPreviouslyHoldingPresetPreviewKey;
	bool bFaderChanged = nameChanged || beingDraggedChanged || previewKeyChanged;
	bool faderAmountChanged = fader.amount != m_flPreviousPresetAmount;

	// Update values for procedural presets, but not if we are already actively dragging, otherwise the 
	// target value of the head preset can change during the drag if the head is within the falloff region.
	if ( fader.holdingPreviewKey || ( fader.isbeingdragged && beingDraggedChanged ) || bFaderChanged )
	{
		presets->UpdateProceduralPresetSlider( fader.values );
	}

	// logic moved from CAnimSetAttributeSliderPanel for simplicity - another pass may remove a fair amount of it entirely
	bool previewKeyPressed = fader.holdingPreviewKey && previewKeyChanged;
	bool startedDrag = fader.isbeingdragged && beingDraggedChanged;
	bool newControl = nameChanged;
	m_bPresetSliderChanged = newControl || previewKeyPressed || startedDrag;

	m_PreviousPresetSlider = fader.name;
	m_flPreviousPresetAmount = fader.amount;
	m_bPresetPreviouslyDragged = fader.isbeingdragged;
	m_bPreviouslyHoldingPresetPreviewKey = fader.holdingPreviewKey;

	bool shiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );

	int c = pAttributeSlider->GetSliderCount();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = pAttributeSlider->GetSlider( i );
		if ( !slider->IsVisible() )
			continue;

		CDmElement *pControl = slider->GetControl();
		if ( !pControl )
			continue;

		bool bTransform = slider->IsTransform();

		if ( m_ActiveAttributeSlider.Get() == slider && !slider->IsDragging() && shiftDown )
		{
			// The preset stuff shouldn't be active when we're holding the preview key over the raw attribute sliders!!!
			Assert( !fader.isbeingdragged );

			AttributeValue_t dest;
			if ( !bTransform )
			{
				int x, y;
				input()->GetCursorPos( x, y );
				slider->ScreenToLocal( x, y );
				float flEstimatedValue = slider->EstimateValueAtPos( x, y );

				dest.m_pValue[ ANIM_CONTROL_VALUE ] = flEstimatedValue;
				dest.m_pValue[ ANIM_CONTROL_VALUE_LEFT ] = flEstimatedValue;
				dest.m_pValue[ ANIM_CONTROL_VALUE_RIGHT ] = flEstimatedValue;
			}
			else
			{
				slider->GetValue( ANIM_CONTROL_TXFORM_POSITION, dest.m_Vector );
				slider->GetValue( ANIM_CONTROL_TXFORM_ORIENTATION, dest.m_Quaternion );
			}

			// If we aren't over any of the preset sliders, then we need to be able to ramp down to the current value, too
			slider->SetPreview( dest, dest );
			continue;
		}

		if ( !fader.values )
			continue;

		bool simple = fader.isbeingdragged || !fader.holdingPreviewKey;
		if ( bFaderChanged || fader.isbeingdragged )
		{
			int idx = fader.values->Find( pControl->GetHandle() );
			const AttributeValue_t &previewin = idx == fader.values->InvalidIndex() ? slider->GetValue() : fader.values->Element( idx );
			const AttributeValue_t &current = slider->GetValue();
			AttributeValue_t preview;
			BlendValues( bTransform, &preview, current, previewin, 1.0f, flBalanceSliderValue );

			// If being dragged, slam to current value right away
			if ( simple )
			{
				slider->SetPreview( preview, preview );
			}
			else
			{
				// Apply the left-right balance to the target
				AttributeValue_t dest;
				BlendValues( bTransform, &dest, current, preview, fader.amount );
				slider->SetPreview( dest, preview );
			}
		}

		if ( faderAmountChanged || fader.isbeingdragged || fader.holdingPreviewKey )
		{
			slider->UpdateFaderAmount( fader.amount );
		}
	}
}

void CBaseAnimationSetControl::UpdatePreviewSliderTimes()
{
	CBaseAnimSetAttributeSliderPanel *pAttributeSlider = m_pEditor->GetAttributeSlider();
	if ( !pAttributeSlider )
		return;

	bool ctrlDown = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	bool shiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	if ( ctrlDown )
	{
		int mx, my;
		input()->GetCursorPos( mx, my );
		bool bInside = pAttributeSlider->IsWithin( mx, my );
		if ( !bInside )
		{
			shiftDown = false;
			ctrlDown = false;
		}

		VPANEL topMost = input()->GetMouseOver();
		if ( topMost && !ipanel()->HasParent( topMost, pAttributeSlider->GetVPanel() ) )
		{
			shiftDown = false;
			ctrlDown = false;
		}
	}

	bool previewing = ( m_bPreviouslyHoldingPresetPreviewKey ) || ( m_ActiveAttributeSlider.Get() && shiftDown );
	bool changingvalues = ( m_bPresetPreviouslyDragged ) || ( m_ActiveAttributeSlider.Get() && m_ActiveAttributeSlider->IsDragging() );

	// If control is being pressed or this slider is the currently active dominant slider set
	// this slider to be the new dominant slider. This will cause the dominant slider to be 
	// selected when you press control but not to be lost if the control key is released.
	int newDominantSlider = -1;
	if ( m_ActiveAttributeSlider.Get() && m_ActiveAttributeSlider->IsDragging() )
	{
		if ( ctrlDown || ( m_nDominantSlider >= 0 && pAttributeSlider->GetSlider( m_nDominantSlider ) == m_ActiveAttributeSlider.Get() ) )
		{
			newDominantSlider = pAttributeSlider->FindSliderIndexForControl( m_ActiveAttributeSlider->GetControl() );
		}
	}

	// If the dominant slider has changed update starting values that are used to determine what
	// values the other sliders should fade from as the value of the dominant slider increases.
	if ( newDominantSlider != m_nDominantSlider )
	{
		UpdateDominantSliderStartValues( newDominantSlider < 0 );
		m_nDominantSlider = newDominantSlider;
	}

	CAttributeSlider *dragSlider = m_ActiveAttributeSlider && m_ActiveAttributeSlider->IsDragging() ? m_ActiveAttributeSlider.Get() : NULL;
	pAttributeSlider->UpdateControlSetMode( changingvalues, previewing, dragSlider );
}

bool CBaseAnimationSetControl::IsPresetFaderBeingDragged() const
{
	return m_bPresetPreviouslyDragged;
}

float CBaseAnimationSetControl::GetDominantSliderStartValue( int nSliderIndex, AnimationControlType_t type )
{
	return m_DominantSliderStartValues[ nSliderIndex ].m_pValue[ type ];
}

//-----------------------------------------------------------------------------
// Purpose: Save the current values of the visible sliders to the dominant 
// slider start value or restore slider values to the value saved before if 
// the restoreSliderValues flag is true.
//-----------------------------------------------------------------------------
void CBaseAnimationSetControl::UpdateDominantSliderStartValues( bool restoreSliderValues )
{
	CBaseAnimSetAttributeSliderPanel *pAttributeSlider = m_pEditor->GetAttributeSlider();
	if ( !pAttributeSlider )
		return;

	int nSliders = pAttributeSlider->GetSliderCount();

	// If the start values have not been saved yet the array will need to be allocated.
	if ( m_DominantSliderStartValues.Count() != nSliders )
	{
		Assert( m_DominantSliderStartValues.Count() == 0 );
		m_DominantSliderStartValues.SetCount( nSliders );

		// Can't restore the values of the sliders to the 
		// start values if the start values have not been set.
		if ( restoreSliderValues )
		{
			Assert( restoreSliderValues == false );
			return;
		}
	}

	// Loop through all of the sliders and update the values of the visible sliders.
	for ( int i = 0; i < nSliders; ++i )
	{
		CAttributeSlider *pSlider = pAttributeSlider->GetSlider( i );
		if ( pSlider == NULL )
			continue;

		if ( !pSlider->IsVisible() )
			continue;

		if ( restoreSliderValues )
		{
			if ( pSlider->IsDragging() == false )
			{
				pSlider->SetValue( m_DominantSliderStartValues[ i ] );
			}
		}
		else
		{
			m_DominantSliderStartValues[ i ] = pSlider->GetValue();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the start and current values for the dominant slider based on
// the first active control.
//-----------------------------------------------------------------------------
void CBaseAnimationSetControl::GetDominantSliderValues( float &flDomStart, float &flDomValue )
{
	flDomStart = 0.0f;
	flDomValue = 0.0f;

	if ( m_nDominantSlider < 0 )
		return;

	CBaseAnimSetAttributeSliderPanel *pAttributeSlider = m_pEditor->GetAttributeSlider();
	if ( !pAttributeSlider )
		return;

	CAttributeSlider *pDominantSlider = pAttributeSlider->GetSlider( m_nDominantSlider );
	if ( pDominantSlider == NULL )
		return;

	if ( pDominantSlider->IsTransform() )
		return;

	AnimationControlType_t type = pDominantSlider->IsStereo() ? ANIM_CONTROL_VALUE_RIGHT : ANIM_CONTROL_VALUE;
	flDomStart = max( 0.0f, min( 1.0f, GetDominantSliderStartValue( m_nDominantSlider, type ) ) );
	flDomValue = max( 0.0f, min( 1.0f, pDominantSlider->GetValue( type ) ) );
}

void CBaseAnimationSetControl::ApplyPreset( float flScale, AttributeDict_t& values )
{
	CBaseAnimSetAttributeSliderPanel *pAttributeSlider = m_pEditor->GetAttributeSlider();
	if ( !pAttributeSlider )
		return;

	bool bChanged = false;
	int c = pAttributeSlider->GetSliderCount();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = pAttributeSlider->GetSlider( i );
		if ( !slider || !slider->IsVisible() )
			continue;

		int idx = values.Find( slider->GetControl()->GetHandle() );
		const AttributeValue_t &target = idx == values.InvalidIndex() ? slider->GetValue() : values[ idx ];
		const AttributeValue_t &current = slider->GetValue();

		// Apply the left-right balance to the target
		AttributeValue_t blend;
		BlendValues( slider->IsTransform(), &blend, current, target, flScale, pAttributeSlider->GetBalanceSliderValue() );
		slider->SetValue( blend );
		bChanged = true;
	}

	if ( bChanged )
	{
		pAttributeSlider->UpdatePreview( "ApplyPreset\n" );
	}
}

template< class T >
void CBaseAnimationSetControl::ApplyTransformSliderValue( CAttributeSlider *pSlider, CDmeTransformControl *pTranformControl, bool bUsePreviewValue, bool bForce, bool &valuesChanged, AnimationControlType_t type )
{
	CDmAttribute *pAttr = NULL;
	CDmeChannel *pChannel = NULL;
	if ( type == ANIM_CONTROL_TXFORM_POSITION )
	{
		pAttr = pTranformControl->GetPositionAttr();
		pChannel = pTranformControl->GetPositionChannel();
	}
	else if ( type == ANIM_CONTROL_TXFORM_ORIENTATION )
	{
		pAttr = pTranformControl->GetOrientationAttr();
		pChannel = pTranformControl->GetOrientationChannel();
	}

	Assert( pAttr );
	if ( !pAttr )
		return;
	
	Assert( pChannel );

	// Figure out what to do based on the channel's mode
	ChannelMode_t mode = pChannel ? pChannel->GetMode() : CM_PASS;
	bool bPushSlidersIntoScene = ( mode == CM_PASS || mode == CM_RECORD );
	bool bPullSlidersFromScene = ( mode == CM_PLAY );

	if ( bPullSlidersFromScene )
	{
		T value = pAttr->GetValue< T >();
		pChannel->GetCurrentPlaybackValue< T >( value );
		pSlider->SetValue( type, value );
		pAttr->SetValue( value );
	}
	else if ( bPushSlidersIntoScene )
	{
		T value;
		if ( bUsePreviewValue )
		{
			pSlider->GetPreview( type, value );
		}
		else
		{
			pSlider->GetValue( type, value );
		}
		if ( pAttr->GetValue< T >() != value || bForce )
		{
			// The txform manipulator drives the UpdatePreview call, so don't do it twice unless
			//  we are just dialing in a preset (bForce == true)
			valuesChanged = bForce;
	
			T currentValue = pAttr->GetValue< T >();
			T maskedValue = MaskValue( value, currentValue, pSlider->VisibleComponents() );
			pAttr->SetValue( maskedValue );
		}
	}
}

void CBaseAnimationSetControl::ApplySliderValueWithDominance( CAttributeSlider *pSlider, int si, float flDomStart, float flDomValue, CDmElement *pControl, bool bUsePreviewValue, bool bForce, bool &valuesChanged, AnimationControlType_t type, const char *pChannelAttrName, const char *pValueAttrName )
{
	CDmAttribute *pAttr = pControl->GetAttribute( pValueAttrName, AT_FLOAT );
	Assert( pAttr );
	if ( !pAttr )
		return;

	CDmeChannel *pChannel = pControl->GetValueElement< CDmeChannel >( pChannelAttrName );
	Assert( pChannel );

	// Figure out what to do based on the channel's mode
	ChannelMode_t mode = pChannel ? pChannel->GetMode() : CM_PASS;
	bool bPushSlidersIntoScene = ( mode == CM_PASS || mode == CM_RECORD );
	bool bPullSlidersFromScene = ( mode == CM_PLAY );

	if ( bPullSlidersFromScene )
	{
		// If it's actively being manipulated, the UI will be up to date
		if ( pSlider->IsDragging() )
			return;

		// Drive value setting based on the output data
		float flValue = pControl->GetValue< float >( DEFAULT_FLOAT_ATTR );
		if ( pChannel->GetCurrentPlaybackValue< float >( flValue ) )
		{
			pAttr->SetValue( flValue );
		}
		else
		{
			flValue = pAttr->GetValue< float >();
		}
		pSlider->SetValue( type, flValue );
	}
	else if ( bPushSlidersIntoScene )
	{
		float flValue = bUsePreviewValue ? pSlider->GetPreview( type ) : pSlider->GetValue( type );

		// If there is an active dominant slider then all other visible
		// sliders, should scale down based on the value of the dominant slider.
		if ( ( m_nDominantSlider >= 0 ) && pSlider->IsVisible() && !bUsePreviewValue )
		{
			if ( m_nDominantSlider != si )
			{
				float flSliderStart = GetDominantSliderStartValue( si, type );
				float flRange = 1.0f - flDomStart;
				float flScale = ( flRange > 0 ) ? ( max( 0.0f, flDomValue - flDomStart ) / flRange ) : 0.0f;
				float flTargetValue = pControl->GetValue< float >( DEFAULT_FLOAT_ATTR );
				flValue = flSliderStart * ( 1.0f - flScale ) + flTargetValue * flScale;
				pSlider->SetValue( type, flValue );
			}
		}

		if ( pAttr->GetValue< float >() != flValue || bForce )
		{
			valuesChanged = true;
			pAttr->SetValue( flValue );
		}
	}
}

bool CBaseAnimationSetControl::ApplySliderValues( bool bForce )
{
	CBaseAnimSetAttributeSliderPanel *pAttributeSlider = m_pEditor->GetAttributeSlider();
	if ( !pAttributeSlider )
		return false;

	if ( !bForce )
	{
		bForce = m_bPresetPreviouslyDragged;
	}

	CDisableUndoScopeGuard guard;

	float flDomStart = 0;
	float flDomValue = 0;
	GetDominantSliderValues( flDomStart, flDomValue );

	bool valuesChanged = false;

	int nSliders = pAttributeSlider->GetSliderCount();
	for ( int si = 0; si < nSliders; ++si )
	{
		CAttributeSlider *pSlider = pAttributeSlider->GetSlider( si );
		Assert( pSlider );
		if ( !pSlider || !pSlider->IsVisible() )
			continue;

		CDmElement *pControl = pSlider->GetControl();
		if ( !pControl )
			continue;

		bool shiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
		bool bPreviewingAttributeSlider = m_ActiveAttributeSlider.Get() == pSlider && !pSlider->IsDragging() && shiftDown;
		bool bMouseOverPresetSlider = m_pEditor->GetPresetFader()->GetActivePresetSlider() != NULL;
		bool bUsePreviewValue = m_bPreviouslyHoldingPresetPreviewKey || bPreviewingAttributeSlider || ( bForce && bMouseOverPresetSlider );

		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pControl );
		if ( pTransformControl )
		{
			if ( pSlider->IsOrientation() )
			{
				ApplyTransformSliderValue< Quaternion >( pSlider, pTransformControl, bUsePreviewValue, bForce, valuesChanged, ANIM_CONTROL_TXFORM_ORIENTATION );
			}
			else
			{
				ApplyTransformSliderValue< Vector >( pSlider, pTransformControl, bUsePreviewValue, bForce, valuesChanged, ANIM_CONTROL_TXFORM_POSITION );
			}
		}
		else if ( IsStereoControl( pControl ) )
		{
			ApplySliderValueWithDominance( pSlider, si, flDomStart, flDomValue, pControl, bUsePreviewValue, bForce, valuesChanged, ANIM_CONTROL_VALUE_LEFT,  "leftvaluechannel",  "leftValue" );
			ApplySliderValueWithDominance( pSlider, si, flDomStart, flDomValue, pControl, bUsePreviewValue, bForce, valuesChanged, ANIM_CONTROL_VALUE_RIGHT, "rightvaluechannel", "rightValue" );
		}
		else
		{
			ApplySliderValueWithDominance( pSlider, si, flDomStart, flDomValue, pControl, bUsePreviewValue, bForce, valuesChanged, ANIM_CONTROL_VALUE, "channel", "value" );
		}
	}

	guard.Release();

	return valuesChanged;
}

void CBaseAnimationSetControl::SetActiveAttributeSlider( CAttributeSlider *pSlider )
{
	m_ActiveAttributeSlider = pSlider;
}

void CBaseAnimationSetControl::EnsureCrossfadePresetControlValues( int nCount )
{
	m_crossfadePresetControlValues.RemoveAll();

	int nOldCount = m_crossfadePresetControlValues.Count();
	for ( int i = nOldCount; i < nCount; ++i )
	{
		m_crossfadePresetControlValues.AddToTail( CreateElement< CDmElement >( "procedural preset control value", DMFILEID_INVALID ) );
	}
}

void CBaseAnimationSetControl::ProceduralPreset_UpdateCrossfade( AttributeDict_t *pPresetValuesLookup, int nPresetType )
{
	switch ( nPresetType )
	{
	case PROCEDURAL_PRESET_HEAD_CROSSFADE:
	case PROCEDURAL_PRESET_IN_CROSSFADE:
	case PROCEDURAL_PRESET_OUT_CROSSFADE:
		return; // can only update these in the sfm (or an app that knows about current time (head) and time selection (in/out)
	}

	CDisableUndoScopeGuard guard;

	bool bIsDefaultPreset = nPresetType == PROCEDURAL_PRESET_DEFAULT_CROSSFADE;
	bool bSinglePreset = !bIsDefaultPreset; // the only other presets this code deals with are zero, half, one

	EnsureCrossfadePresetControlValues( bSinglePreset ? 1 : m_SelectionHistory.Count() );

	if ( bSinglePreset )
	{
		float flValue = 0.0f;
		switch ( nPresetType )
		{
		case PROCEDURAL_PRESET_ZERO_CROSSFADE:
			flValue = 0.0f;
			break;
		case PROCEDURAL_PRESET_HALF_CROSSFADE:
			flValue = 0.5f;
			break;
		case PROCEDRUAL_PRESET_ONE_CROSSFADE:
			flValue = 1.0f;
			break;
		}

		CDmElement *pPresetControlValue = m_crossfadePresetControlValues[ 0 ];
		pPresetControlValue->SetValue( "valuePosition", vec3_origin );
		pPresetControlValue->SetValue( "valueOrientation", quat_identity );
		pPresetControlValue->SetValue( "leftValue", flValue );
		pPresetControlValue->SetValue( "rightValue", flValue );
		pPresetControlValue->SetValue( "value", flValue );
	}

	pPresetValuesLookup->RemoveAll();

	int pcvi = -1;
	for ( int i = m_SelectionHistory.Head(); i != m_SelectionHistory.InvalidIndex(); i = m_SelectionHistory.Next( i ) )
	{
		SelectionInfo_t *psi = m_SelectionHistory[ i ];
		CDmElement *pControl = psi->m_hControl;
		if ( !pControl )
			continue;

		CDmElement *pPresetControlValue = m_crossfadePresetControlValues[ bSinglePreset ? 0 : ++pcvi ];

		DmElementHandle_t handle;
		handle = pControl->GetHandle();
		int idx = pPresetValuesLookup->Find( handle );
		if ( idx == pPresetValuesLookup->InvalidIndex() )
		{
			idx = pPresetValuesLookup->Insert( handle );
		}
		AnimationControlAttributes_t &val = pPresetValuesLookup->Element( idx );
		val.Clear();

		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pControl );
		if ( pTransformControl )
		{			
			if ( psi->AreAnyPositionComponentsSelected() )
			{
				CDmAttribute *pValueAttribute = pPresetControlValue->AddAttribute( "valuePosition", AT_VECTOR3 );
				Assert( pValueAttribute );
				if ( !pValueAttribute )
					continue;

				if ( !bSinglePreset )
				{
					pValueAttribute->SetValue( pTransformControl->GetDefaultPosition() );
				}

				val.m_pValueAttribute[ ANIM_CONTROL_TXFORM_POSITION ] = pValueAttribute;
				val.m_Vector = pValueAttribute->GetValue< Vector >();
			}

			if ( psi->AreAnyOrientationComponentsSelected() )
			{
				CDmAttribute *pValueAttribute = pPresetControlValue->AddAttribute( "valueOrientation", AT_QUATERNION );
				Assert( pValueAttribute );
				if ( !pValueAttribute )
					continue;

				if ( !bSinglePreset )
				{
					pValueAttribute->SetValue( pTransformControl->GetDefaultOrientation() );
				}

				val.m_pValueAttribute[ ANIM_CONTROL_TXFORM_ORIENTATION ] = pValueAttribute;
				val.m_Quaternion = pValueAttribute->GetValue< Quaternion >();
			}
		}
		else if ( IsStereoControl( pControl ) )
		{
			CDmAttribute *pLeftValueAttribute  = pPresetControlValue->AddAttribute( "leftValue",  AT_FLOAT );
			CDmAttribute *pRightValueAttribute = pPresetControlValue->AddAttribute( "rightValue", AT_FLOAT );
			Assert( pLeftValueAttribute && pRightValueAttribute );
			if ( !pLeftValueAttribute || !pRightValueAttribute )
				continue;

			if ( !bSinglePreset )
			{
				float flDefaultValue = pControl->GetValue< float >( DEFAULT_FLOAT_ATTR );
				pLeftValueAttribute ->SetValue( flDefaultValue );
				pRightValueAttribute->SetValue( flDefaultValue );
			}

			val.m_pValueAttribute[ ANIM_CONTROL_VALUE_LEFT  ] = pLeftValueAttribute;
			val.m_pValue         [ ANIM_CONTROL_VALUE_LEFT  ] = pLeftValueAttribute->GetValue< float >();
			val.m_pValueAttribute[ ANIM_CONTROL_VALUE_RIGHT ] = pRightValueAttribute;
			val.m_pValue         [ ANIM_CONTROL_VALUE_RIGHT ] = pRightValueAttribute->GetValue< float >();
		}
		else
		{
			CDmAttribute *pValueAttribute = pPresetControlValue->AddAttribute( "value", AT_FLOAT );
			Assert( pValueAttribute );
			if ( !pValueAttribute )
				continue;

			if ( !bSinglePreset )
			{
				pValueAttribute->SetValue( pControl->GetValue< float >( DEFAULT_FLOAT_ATTR ) );
			}

			val.m_pValueAttribute[ ANIM_CONTROL_VALUE ] = pValueAttribute;
			val.m_pValue         [ ANIM_CONTROL_VALUE ] = pValueAttribute->GetValue< float >();
		}
	}
}

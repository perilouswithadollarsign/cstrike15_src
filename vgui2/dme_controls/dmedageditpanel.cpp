//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "vstdlib/random.h"
#include "dme_controls/dmedageditpanel.h"
#include "dme_controls/dmedagrenderpanel.h"
#include "dme_controls/dmepanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/ScrollableEditablePanel.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/Image.h"
#include "vgui_controls/TextImage.h"
#include "vgui/isurface.h"
#include "vgui/ischeme.h"
#include "vgui/iinput.h"
#include "vgui/ivgui.h"
#include "vgui/cursor.h"
#include "movieobjects/dmemakefile.h"
#include "movieobjects/dmemdlmakefile.h"
#include "movieobjects/dmedccmakefile.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmeflexrules.h"

#include "tier1/keyvalues.h"
#include "materialsystem/imesh.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "dme_controls/BaseAnimationSetEditorController.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
//-----------------------------------------------------------------------------
//
// Hook into the dme panel editor system
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeDag, "DmeDagPreview2", "DmeDag Previewer 2", false );
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeSourceSkin, "DmeSourceSkinPreview2", "MDL Skin Previewer 2", false );
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeSourceAnimation, "DmeSourceAnimationPreview2", "MDL Animation Previewer 2", false );
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeDCCMakefile, "DmeMakeFileOutputPreview2", "DCC MakeFile Output Previewer 2", false );


//-----------------------------------------------------------------------------
// Slider panel
//-----------------------------------------------------------------------------
class CDmeAnimationListPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeAnimationListPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmeAnimationListPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeAnimationListPanel();

	void SetAnimationList( CDmeAnimationList *pList );
	void RefreshAnimationList();
	const char *GetSelectedAnimation() const;

private:
	// Called when the selection changes moves
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	MESSAGE_FUNC( OnItemDeselected, "ItemDeselected" );

	vgui::ListPanel *m_pAnimationList;
	CDmeHandle< CDmeAnimationList > m_hAnimationList;
};


static int __cdecl AnimationNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("name");
	const char *string2 = item2.kv->GetString("name");
	return Q_stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmeAnimationListPanel::CDmeAnimationListPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pAnimationList = new vgui::ListPanel( this, "AnimationList" );
	m_pAnimationList->AddColumnHeader( 0, "name", "name", 100, 0 );
	m_pAnimationList->AddActionSignalTarget( this );
	m_pAnimationList->SetSortFunc( 0, AnimationNameSortFunc );
	m_pAnimationList->SetSortColumn( 0 );
	m_pAnimationList->SetEmptyListText("No animations");
	m_pAnimationList->SetMultiselectEnabled( false );

	LoadControlSettingsAndUserConfig( "resource/dmeanimationlistpanel.res" );
}

CDmeAnimationListPanel::~CDmeAnimationListPanel()
{
}


//-----------------------------------------------------------------------------
// Sets the combination operator
//-----------------------------------------------------------------------------
void CDmeAnimationListPanel::SetAnimationList( CDmeAnimationList *pList )
{
	if ( pList != m_hAnimationList.Get() )
	{
		m_hAnimationList = pList;
		RefreshAnimationList();
	}
}


//-----------------------------------------------------------------------------
// Builds the list of animations
//-----------------------------------------------------------------------------
void CDmeAnimationListPanel::RefreshAnimationList()
{
	m_pAnimationList->RemoveAll();
	if ( !m_hAnimationList.Get() )
		return;

	int nCount = m_hAnimationList->GetAnimationCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeChannelsClip *pAnimation = m_hAnimationList->GetAnimation( i );
		KeyValues *pItemKeys = new KeyValues( "node", "name", pAnimation->GetName() );
		m_pAnimationList->AddItem( pItemKeys, 0, false, false );
	}

	m_pAnimationList->SortList();
}


//-----------------------------------------------------------------------------
// Returns the selected animation
//-----------------------------------------------------------------------------
const char *CDmeAnimationListPanel::GetSelectedAnimation() const
{
	if ( m_pAnimationList->GetSelectedItemsCount() == 0 )
		return "";

	int nIndex = m_pAnimationList->GetSelectedItem( 0 );
	KeyValues *pItemKeyValues = m_pAnimationList->GetItem( nIndex );
	return pItemKeyValues->GetString( "name" );
}


//-----------------------------------------------------------------------------
// Called when an animation is selected or deselected
//-----------------------------------------------------------------------------
void CDmeAnimationListPanel::OnItemSelected( )
{
	const char *pAnimationName = GetSelectedAnimation();
	PostActionSignal( new KeyValues( "AnimationSelected", "animationName", pAnimationName ) );
}

void CDmeAnimationListPanel::OnItemDeselected( )
{
	PostActionSignal( new KeyValues( "AnimationDeselected" ) );
}


class CCombinationOperatorControl : public CBaseAnimationSetControl
{
public:
	CCombinationOperatorControl() {}

	virtual void ChangeAnimationSetClip( CDmeFilmClip *pFilmClip )
	{
		RebuildControlList( m_FullControlList, pFilmClip );
		CBaseAnimationSetControl::ChangeAnimationSetClip( pFilmClip );
	}
	virtual void OnControlsAddedOrRemoved()
	{
		RemoveNullControls( m_FullControlList );
		AddMissingControls( m_FullControlList, m_hFilmClip );
		CBaseAnimationSetControl::OnControlsAddedOrRemoved();
	}

protected:
	virtual SelectionInfo_t *FindSelectionInfoForControl( const CDmElement *pControl )
	{
		return ::FindSelectionInfoForControl( m_FullControlList, pControl );
	}

	CUtlVector< SelectionInfo_t* > m_FullControlList;
};



class CDmeCombinationOperatorPanel : public CBaseAnimationSetEditor
{	
	DECLARE_CLASS_SIMPLE( CDmeCombinationOperatorPanel, CBaseAnimationSetEditor );
public:
	CDmeCombinationOperatorPanel( vgui::Panel *parent, const char *panelName, CBaseAnimationSetControl *pAnimationSetController );
	virtual ~CDmeCombinationOperatorPanel();

	virtual void OnTick();

	void SetCombinationOperator( CDmeCombinationOperator *pOp );
	void RefreshCombinationOperator();

private:
	void CreateFakeAnimationSet( );
	void DestroyFakeAnimationSet( );

	// Adds new controls to the animation set
	void AddNewAnimationSetControls();

	// Removes a controls from all presets
	void RemoveAnimationControlFromPresets( const char *pControlName );

	// Removes controls from the animation set that aren't used
	void RemoveUnusedAnimationSetControls();

	// Modify controls in the presets which have had stereo settings changed
	void ModifyExistingAnimationSetControls();

	// Modify controls in the presets which have had stereo settings changed
	void ModifyExistingAnimationSetPresets( CDmElement *pControlElement );

	// Modify controls which have had stereo settings changed
	void ModifyExistingAnimationSetControl( CDmElement *pControlElement );

	void RefreshAnimationSet();

	// Sort control names to match the combination controls
	void SortAnimationSetControls();

	CDmeHandle< CDmeCombinationOperator > m_hCombinationOperator;
	CUtlVector< IDmeOperator * > m_operatorList;
};


//-----------------------------------------------------------------------------
// Constructor/destructor
//-----------------------------------------------------------------------------
CDmeCombinationOperatorPanel::CDmeCombinationOperatorPanel( vgui::Panel *parent, const char *panelName, CBaseAnimationSetControl *pAnimationSetController ) :
	BaseClass( parent, panelName, pAnimationSetController )
{
	CreateFakeAnimationSet();
	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );
}

CDmeCombinationOperatorPanel::~CDmeCombinationOperatorPanel()
{
	DestroyFakeAnimationSet();
}


//-----------------------------------------------------------------------------
// Create, destroy fake animation sets
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::CreateFakeAnimationSet()
{
	CDmeFilmClip *pFilmClip = CreateElement< CDmeFilmClip >( "fakeFilmClip", DMFILEID_INVALID );
	CDmeAnimationSet *pAnimSet = CreateElement< CDmeAnimationSet >( "fakeAnimSet", DMFILEID_INVALID );
	pAnimSet->SetValue( "gameModel", DMELEMENT_HANDLE_INVALID );
	pFilmClip->GetAnimationSets().AddToTail( pAnimSet );
	g_pDataModel->DontAutoDelete( pFilmClip->GetHandle() );
	ChangeAnimationSetClip( pFilmClip );
}

void CDmeCombinationOperatorPanel::DestroyFakeAnimationSet( )
{
	DestroyElement( m_pController->GetAnimationSetClip(), TD_DEEP );
}


//-----------------------------------------------------------------------------
// Sets combination ops + animation sets
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::SetCombinationOperator( CDmeCombinationOperator *pOp )
{
	// The Animation Set Editor Doesn't Handle Passing The Same Animation Set With
	// Different Data In It Very Well
	DestroyFakeAnimationSet();
	CreateFakeAnimationSet();

	m_operatorList.RemoveAll();

	if ( pOp != m_hCombinationOperator.Get() )
	{
		m_hCombinationOperator = pOp;

		m_operatorList.AddToTail( m_hCombinationOperator );

		for ( int i = 0; i < pOp->GetOperationTargetCount(); ++i )
		{
			CDmeFlexRules *pDmeFlexRules = CastElement< CDmeFlexRules >( pOp->GetOperationTarget( i ) );
			if ( !pDmeFlexRules )
				continue;

			m_operatorList.AddToTail( pDmeFlexRules );
			for ( int j = 0; j < pDmeFlexRules->GetRuleCount(); ++j )
			{
				CDmeFlexRuleBase *pDmeFlexRule = pDmeFlexRules->GetRule( j );
				if ( !pDmeFlexRule )
					continue;

				m_operatorList.AddToTail( pDmeFlexRule );
			}
		}

		RefreshCombinationOperator();
	}
}


//-----------------------------------------------------------------------------
// Removes a controls from all presets
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::RemoveAnimationControlFromPresets( const char *pControlName )
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	CDmeAnimationSet *pAnimationSet = traversal.Next();
	Assert( !traversal.IsValid() );

	CDmrElementArray< CDmePresetGroup > presetGroupList = pAnimationSet->GetPresetGroups();

	int nPresetGroupCount = presetGroupList.Count();
	for ( int i = 0; i < nPresetGroupCount; ++i )
	{
		CDmePresetGroup *pPresetGroup = presetGroupList[ i ];

		CDmrElementArray< CDmePreset > presetList = pPresetGroup->GetPresets();
		int nPresetCount = presetList.Count();
		for ( int j = 0; j < nPresetCount; ++j )
		{
			CDmePreset *pPreset = presetList[ j ];
			CDmrElementArray<> controlValues = pPreset->GetControlValues();

			int nControlCount = controlValues.Count();
			for ( int k = 0; k < nControlCount; ++k )
			{
				CDmElement *v = controlValues[ k ];
				if ( !Q_stricmp( v->GetName(), pControlName ) )
				{
					controlValues.FastRemove( k );
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Removes controls from the animation set that aren't used
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::RemoveUnusedAnimationSetControls()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	CDmeAnimationSet *pAnimationSet = traversal.Next();
	Assert( !traversal.IsValid() );

	CDmrElementArray< > controls = pAnimationSet->GetControls();

	// Remove all controls in the animation set and in presets that don't exist in the combination system
	int nAnimSetCount = controls.Count();
	for ( int i = nAnimSetCount; --i >= 0; )
	{
		CDmElement *pControlElement = controls[i];
		if ( !pControlElement )
			continue;

		// Don't do any of this work for transforms
		if ( IsTransformControl( pControlElement ) )
			continue;

		const char *pControlName = pControlElement->GetName();

		// Look for a match
		if ( FindComboOpControlIndexForAnimSetControl( m_hCombinationOperator, pControlName ) >= 0 )
			continue;
	
		// No match, blow the control away.
		RemoveAnimationControlFromPresets( pControlName );
		pAnimationSet->RemoveControlFromGroups( pControlName );
		controls.FastRemove( i );
	}
}


//-----------------------------------------------------------------------------
// Modify controls in the presets which have had stereo settings changed
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::ModifyExistingAnimationSetControl( CDmElement *pControlElement )
{
	const char *pControlName = pControlElement->GetName();

	// Look for a match
	bool bIsMultiControl;
	ControlIndex_t nControlIndex = FindComboOpControlIndexForAnimSetControl( m_hCombinationOperator, pControlName, &bIsMultiControl );
	Assert( nControlIndex >= 0 );

	float flDefaultValue = m_hCombinationOperator->GetControlDefaultValue( nControlIndex );

	if ( bIsMultiControl )
	{
		// multi control can't (currently) be stereo, and so it either exists or it doesn't...
		pControlElement->SetValue< float >( "value", m_hCombinationOperator->GetMultiControlLevel( nControlIndex ) );
		pControlElement->SetValue( "defaultValue", 0.5f );
	}
	else
	{
		bool bIsStereoControl = m_hCombinationOperator->IsStereoControl( nControlIndex );
		bool bIsAnimControlStereo = IsStereoControl( pControlElement );
		if ( bIsAnimControlStereo != bIsStereoControl )
		{
			if ( !bIsStereoControl )
			{
				float flValue = m_hCombinationOperator->GetControlValue( nControlIndex );
				pControlElement->SetValue< float >( "value", flValue );

				pControlElement->RemoveAttribute( "leftValue" );
				pControlElement->RemoveAttribute( "rightValue" );
			}
			else
			{
				const Vector2D &value = m_hCombinationOperator->GetStereoControlValue( nControlIndex );
				pControlElement->SetValue< float >( "leftValue", value.x );
				pControlElement->SetValue< float >( "rightValue", value.y );

				pControlElement->RemoveAttribute( "value" );
			}
		}

		pControlElement->SetValue( "defaultValue", flDefaultValue );
	}
}


//-----------------------------------------------------------------------------
// Modify controls in the presets which have had stereo settings changed
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::ModifyExistingAnimationSetPresets( CDmElement *pControlElement )
{
	const char *pControlName = pControlElement->GetName();

	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	CDmeAnimationSet *pAnimationSet = traversal.Next();
	Assert( !traversal.IsValid() );

	const CDmrElementArray< CDmePresetGroup > &presetGroupList = pAnimationSet->GetPresetGroups();

	int nPresetGroupCount = presetGroupList.Count();
	for ( int g = 0; g < nPresetGroupCount; ++g )
	{
		CDmePresetGroup *pPresetGroup = presetGroupList[ g ];
		const CDmrElementArray< CDmePreset > &presetList = pPresetGroup->GetPresets();

		int nPresetCount = presetList.Count();
		for ( int i = 0; i < nPresetCount; ++i )
		{
			CDmePreset *pPreset = presetList[ i ];
			Assert( !pPreset->IsAnimated() ); // deal with this after GDC
			if ( pPreset->IsAnimated() )
				continue;

			const CDmrElementArray< CDmElement > &controlValues = pPreset->GetControlValues( );

			int nControlCount = controlValues.Count();
			for ( int j = 0; j < nControlCount; ++j )
			{
				CDmElement *v = controlValues[ j ];
				if ( Q_stricmp( v->GetName(), pControlName ) )
					continue;

				bool bIsAnimControlStereo = IsStereoControl( pControlElement );
				if ( bIsAnimControlStereo )
				{
					float flValue = v->GetValue< float >( "value" );
					v->RemoveAttribute( "value" );
					v->InitValue( "leftValue", flValue );
					v->InitValue( "rightValue", flValue );
				}
				else
				{
					float flLeftValue = v->GetValue< float >( "leftValue" );
					float flRightValue = v->GetValue< float >( "rightValue" );
					v->RemoveAttribute( "leftValue" );
					v->RemoveAttribute( "rightValue" );
					v->InitValue( "value", MAX( flLeftValue, flRightValue ) ); // TODO - should this be max or average?
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Modify controls which have had stereo settings changed
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::ModifyExistingAnimationSetControls()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	CDmeAnimationSet *pAnimationSet = traversal.Next();
	Assert( !traversal.IsValid() );

	const CDmrElementArray< CDmElement > &controls = pAnimationSet->GetControls();

	// Update the main controls; update defaults and add or remove combo + multi data
	int nAnimSetCount = controls.Count();
	for ( int i = nAnimSetCount; --i >= 0; )
	{
		CDmElement *pControlElement = controls[i];
		if ( !pControlElement )
			continue;

		// Don't do any of this work for transforms
		if ( IsTransformControl( pControlElement ) )
			continue;

		ModifyExistingAnimationSetControl( pControlElement );
		ModifyExistingAnimationSetPresets( pControlElement );
	}
}


//-----------------------------------------------------------------------------
// Adds new controls to the animation set
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::AddNewAnimationSetControls()
{
	if ( !m_hCombinationOperator.Get() )
		return;

	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	CDmeAnimationSet *pAnimationSet = traversal.Next();
	Assert( !traversal.IsValid() );

	CDmaElementArray< > &controls = pAnimationSet->GetControls();

	// Remove all controls in the animation set and in presets that don't exist in the combination system
	int nFirstControl = controls.Count();
	int nCombinationControlCount = m_hCombinationOperator->GetControlCount();
	int iCurrentAnimSetControl = 0;
	for ( int i = 0; i < nCombinationControlCount; ++i, ++iCurrentAnimSetControl )
	{
		const char *pControlName = m_hCombinationOperator->GetControlName( i );
		if ( !pAnimationSet->FindControl( pControlName ) )
		{
			bool bIsStereoControl = m_hCombinationOperator->IsStereoControl( i );
			float flDefaultValue = m_hCombinationOperator->GetControlDefaultValue( i );

			// Add the control to the controls group
			CDmElement *pControl = CreateElement< CDmElement >( pControlName, pAnimationSet->GetFileId() );
			Assert( pControl );
			controls.InsertBefore( iCurrentAnimSetControl, pControl );

			if ( bIsStereoControl )
			{ 
				const Vector2D &value = m_hCombinationOperator->GetStereoControlValue(i);
				pControl->SetValue( "leftValue", value.x );
				pControl->SetValue( "rightValue", value.y );
			}
			else
			{
				pControl->SetValue( "value", m_hCombinationOperator->GetControlValue(i) );
			}
			pControl->SetValue( "defaultValue", flDefaultValue );
		}

		bool bIsMultiControl = m_hCombinationOperator->IsMultiControl( i );
		if ( bIsMultiControl )
		{
			++iCurrentAnimSetControl;

			char pMultiControlName[ 256 ];
			V_snprintf( pMultiControlName, sizeof( pMultiControlName ), MULTI_CONTROL_FORMAT_STRING, pControlName );
			if ( !pAnimationSet->FindControl( pMultiControlName ) )
			{
				CDmElement *pMultiControl = CreateElement< CDmElement >( pMultiControlName, pAnimationSet->GetFileId() );
				Assert( pMultiControl );
				controls.InsertBefore( iCurrentAnimSetControl, pMultiControl );

				pMultiControl->SetValue( "value", m_hCombinationOperator->GetMultiControlLevel( i ) );
				pMultiControl->SetValue( "defaultValue", 0.5f );
			}
		}
	}

	int nLastControl = controls.Count();
	if ( nLastControl == nFirstControl )
		return;

	// Add new controls to the root group
	CDmElement *pGroup = pAnimationSet->FindOrAddControlGroup( NULL, "Root" );

	// Fill in members
	CDmrElementArray<> groups( pGroup, "controls" );
	for ( int i = nFirstControl; i < nLastControl; ++i )
	{
		groups.AddToTail( controls[ i ] );
	}
}


//-----------------------------------------------------------------------------
// Sort control names to match the combination controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::SortAnimationSetControls()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	CDmeAnimationSet *pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	// TODO: Figure out why this isn't called on load
	pAnimSet->OnElementUnserialized();

	CDmaElementArray<> &controls = pAnimSet->GetControls();
	int nControlCount = controls.Count();
	if ( nControlCount == 0 )
		return;

	int nCombinationControlCount = m_hCombinationOperator->GetControlCount();
	DmElementHandle_t *pElements = (DmElementHandle_t*)_alloca( nControlCount * sizeof(DmElementHandle_t) );
	int iCurrentControlIndex = 0;
	for ( int i = 0; i < nCombinationControlCount; ++i )
	{
		const char *pControlName = m_hCombinationOperator->GetControlName( i );
		CDmElement *pControl = pAnimSet->FindControl( pControlName );
		pElements[iCurrentControlIndex++] = pControl->GetHandle();

		if ( m_hCombinationOperator->IsMultiControl( i ) )
		{
			char pMultiControlName[ 256 ];
			V_snprintf( pMultiControlName, sizeof( pMultiControlName ), MULTI_CONTROL_FORMAT_STRING, pControlName );
			CDmElement *pMultiControl = pAnimSet->FindControl( pMultiControlName );
			pElements[iCurrentControlIndex++] = pMultiControl->GetHandle();
		}
	}

	Assert( nControlCount == iCurrentControlIndex );

	controls.SetMultiple( 0, nControlCount, pElements );
}


//-----------------------------------------------------------------------------
// Called when the animation set has to be made to match the combination operator
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::RefreshAnimationSet()
{
	if ( !m_pController->GetAnimationSetClip() )
		return;

	CDisableUndoScopeGuard sg;

	// Remove all controls in the animation set and in presets that don't exist in the combination system
	RemoveUnusedAnimationSetControls();

	// Modify controls in the presets which have had stereo settings changed
	ModifyExistingAnimationSetControls();
	
	// Add all controls not in the animation set but which do exist in the combination system
	AddNewAnimationSetControls();


	// Sort control names to match the combination controls
	SortAnimationSetControls();
}

void CDmeCombinationOperatorPanel::RefreshCombinationOperator()
{
	RefreshAnimationSet();
	ChangeAnimationSetClip( m_pController->GetAnimationSetClip() );
}


//-----------------------------------------------------------------------------
// Simulate
//-----------------------------------------------------------------------------
void CDmeCombinationOperatorPanel::OnTick()
{
	BaseClass::OnTick();

	if ( m_hCombinationOperator )
	{
		{
			CDisableUndoScopeGuard sg;

			int nCombinationControlCount = m_hCombinationOperator->GetControlCount();
			int iCurrentControlIndex = 0;
			for ( int i = 0; i < nCombinationControlCount; ++i )
			{
				AttributeValue_t value;
				if ( GetAttributeSlider()->GetSliderValues( &value, iCurrentControlIndex++ ) )
				{
					if ( m_hCombinationOperator->IsStereoControl( i ) )
					{
						m_hCombinationOperator->SetControlValue( i, value.m_pValue[ANIM_CONTROL_VALUE_LEFT], value.m_pValue[ANIM_CONTROL_VALUE_RIGHT] );
					}
					else
					{
						m_hCombinationOperator->SetControlValue( i, value.m_pValue[ANIM_CONTROL_VALUE] );
					}

				}

				if ( m_hCombinationOperator->IsMultiControl( i ) )
				{
					AttributeValue_t multiValue;
					if ( GetAttributeSlider()->GetSliderValues( &multiValue, iCurrentControlIndex++ ) )
					{
						m_hCombinationOperator->SetMultiControlLevel( i, multiValue.m_pValue[ANIM_CONTROL_VALUE] ); 
					}
				}
			}
		}

		if ( m_operatorList.Count() )
		{
			CDisableUndoScopeGuard guard;
			g_pDmElementFramework->SetOperators( m_operatorList );
			g_pDmElementFramework->Operate( true );
		}
		else
		{
			// FIXME: Shouldn't this happen at the application level?
			// run the machinery - apply, resolve, dependencies, operate, resolve
			CUtlVector< IDmeOperator* > operators;
			operators.AddToTail( m_hCombinationOperator );

			CDisableUndoScopeGuard guard;
			g_pDmElementFramework->SetOperators( operators );
			g_pDmElementFramework->Operate( true );
		}
	}

	// allow elements and attributes to be edited again
	g_pDmElementFramework->BeginEdit();
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeDagEditPanel::CDmeDagEditPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	m_pPropertiesSplitter = new vgui::Splitter( this, "PropertiesSplitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pPropertiesSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pPropertiesSplitter->GetChild( 1 );

	m_pDagRenderPanel = new CDmeDagRenderPanel( pSplitterRightSide, "DagRenderPanel" );

	m_pEditorSheet = new vgui::PropertySheet( pSplitterLeftSide, "EditorSheet" );
	m_pEditorSheet->AddActionSignalTarget( this );

	m_pAnimationPage = new vgui::PropertyPage( m_pEditorSheet, "AnimationPage" );
	m_pCombinationPage = new vgui::PropertyPage( m_pEditorSheet, "AnimationSetEditor" );
	m_pVertexAnimationPage = new vgui::PropertyPage( m_pEditorSheet, "VertexAnimationPage" );

	CCombinationOperatorControl *pAnimationSetController = new CCombinationOperatorControl();
	m_pCombinationPanel = new CDmeCombinationOperatorPanel( m_pCombinationPage, "AnimationSetEditorPanel", pAnimationSetController );
	m_pCombinationPanel->CreateToolsSubPanels();

	m_pAnimationListPanel = new CDmeAnimationListPanel( m_pAnimationPage, "AnimationListPanel" );
	m_pAnimationListPanel->AddActionSignalTarget( this );
	m_pVertexAnimationListPanel = new CDmeAnimationListPanel( m_pVertexAnimationPage, "VertexAnimationListPanel" );
	m_pVertexAnimationListPanel->AddActionSignalTarget( this );

	m_pCombinationPage->LoadControlSettingsAndUserConfig( "resource/dmedageditpanel_animationseteditorpage.res" );
	m_pAnimationPage->LoadControlSettingsAndUserConfig( "resource/dmedageditpanel_animationpage.res" );
	m_pVertexAnimationPage->LoadControlSettingsAndUserConfig( "resource/dmedageditpanel_vertexanimationpage.res" );

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettingsAndUserConfig( "resource/dmedageditpanel.res" );

	// NOTE: Page adding happens *after* LoadControlSettingsAndUserConfig
	// because the layout of the sheet is correct at this point.
	m_pEditorSheet->AddPage( m_pAnimationPage, "Animation" );
	m_pEditorSheet->AddPage( m_pCombinationPage, "Combination" );
	m_pEditorSheet->AddPage( m_pVertexAnimationPage, "Vertex Animation" );
	m_pEditorSheet->SetActivePage( m_pCombinationPage );
}

CDmeDagEditPanel::~CDmeDagEditPanel()
{
}


//-----------------------------------------------------------------------------
// Set the scene

//-----------------------------------------------------------------------------
void CDmeDagEditPanel::SetDmeElement( CDmeDag *pScene )
{
	m_pDagRenderPanel->SetDmeElement( pScene );
}


//-----------------------------------------------------------------------------
// Sets up the various panels in the dag editor
//-----------------------------------------------------------------------------
void CDmeDagEditPanel::SetMakefileRootElement( CDmElement *pRoot )
{
	CDmeAnimationList *pAnimationList = pRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	SetAnimationList( pAnimationList );

	CDmeAnimationList *pVertexAnimationList = pRoot->GetValueElement< CDmeAnimationList >( "vertexAnimationList" );
	SetVertexAnimationList( pVertexAnimationList );

	CDmeCombinationOperator *pComboOp = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	SetCombinationOperator( pComboOp );
}


//-----------------------------------------------------------------------------
// Other methods which hook into DmePanel
//-----------------------------------------------------------------------------
void CDmeDagEditPanel::SetDmeElement( CDmeSourceSkin *pSkin )
{
	m_pDagRenderPanel->SetDmeElement( pSkin );

	// First, try to grab the dependent makefile
	CDmeMakefile *pSourceMakefile = pSkin->GetDependentMakefile();
	if ( !pSourceMakefile )
		return;

	// Next, try to grab the output of that makefile
	CDmElement *pOutput = pSourceMakefile->GetOutputElement( true );
	if ( !pOutput )
		return;

	SetMakefileRootElement( pOutput );
}

void CDmeDagEditPanel::SetDmeElement( CDmeSourceAnimation *pAnimation )
{
	m_pDagRenderPanel->SetDmeElement( pAnimation );

	// First, try to grab the dependent makefile
	CDmeMakefile *pSourceMakefile = pAnimation->GetDependentMakefile();
	if ( !pSourceMakefile )
		return;

	// Next, try to grab the output of that makefile
	CDmElement *pOutput = pSourceMakefile->GetOutputElement( true );
	if ( !pOutput )
		return;

	SetMakefileRootElement( pOutput );

//	if ( pAnimationList->FindAnimation( pAnimation->m_SourceAnimationName ) < 0 )
//		return;
}

void CDmeDagEditPanel::SetDmeElement( CDmeDCCMakefile *pDCCMakefile )
{
	m_pDagRenderPanel->SetDmeElement( pDCCMakefile );

	// First, try to grab the dependent makefile
	CDmElement *pOutput = pDCCMakefile->GetOutputElement( true );
	if ( !pOutput )
		return;

	SetMakefileRootElement( pOutput );
}

CDmeDag *CDmeDagEditPanel::GetDmeElement()
{
	return m_pDagRenderPanel->GetDmeElement();
}


//-----------------------------------------------------------------------------
// paint it!
//-----------------------------------------------------------------------------
void CDmeDagEditPanel::Paint()
{
	BaseClass::Paint();
}


//-----------------------------------------------------------------------------
// Sets animation
//-----------------------------------------------------------------------------
void CDmeDagEditPanel::SetAnimationList( CDmeAnimationList *pAnimationList )
{
	m_pDagRenderPanel->SetAnimationList( pAnimationList );
	m_pDagRenderPanel->SelectAnimation( "" );
	m_pAnimationListPanel->SetAnimationList( pAnimationList );
}


void CDmeDagEditPanel::SetVertexAnimationList( CDmeAnimationList *pAnimationList )
{
	m_pDagRenderPanel->SetVertexAnimationList( pAnimationList );
	m_pDagRenderPanel->SelectVertexAnimation( "" );
	m_pVertexAnimationListPanel->SetAnimationList( pAnimationList );
}

void CDmeDagEditPanel::SetCombinationOperator( CDmeCombinationOperator *pComboOp )
{
	m_pCombinationPanel->SetCombinationOperator( pComboOp );
}

void CDmeDagEditPanel::RefreshCombinationOperator()
{
	m_pCombinationPanel->RefreshCombinationOperator();
}


//-----------------------------------------------------------------------------
// Called when the page changes
//-----------------------------------------------------------------------------
void CDmeDagEditPanel::OnPageChanged( )
{
	if ( m_pEditorSheet->GetActivePage() == m_pCombinationPage )
	{
		m_pDagRenderPanel->SelectAnimation( "" );
		m_pDagRenderPanel->SelectVertexAnimation( "" );
		return;
	}

	if ( m_pEditorSheet->GetActivePage() == m_pAnimationPage )
	{
		m_pDagRenderPanel->SelectAnimation( m_pAnimationListPanel->GetSelectedAnimation() );
		m_pDagRenderPanel->SelectVertexAnimation( "" );
		return;
	}

	if ( m_pEditorSheet->GetActivePage() == m_pVertexAnimationPage )
	{
		m_pDagRenderPanel->SelectAnimation( "" );
		m_pDagRenderPanel->SelectVertexAnimation( m_pVertexAnimationListPanel->GetSelectedAnimation() );
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when new animations are selected
//-----------------------------------------------------------------------------
void CDmeDagEditPanel::OnAnimationSelected( KeyValues *pKeyValues )
{
	vgui::Panel *pPanel = (vgui::Panel *)pKeyValues->GetPtr( "panel", NULL );
	const char *pAnimationName = pKeyValues->GetString( "animationName", "" );

	if ( pPanel == m_pAnimationListPanel )
	{
		m_pDagRenderPanel->SelectAnimation( pAnimationName );
		return;
	}

	if ( pPanel == m_pVertexAnimationListPanel )
	{
		m_pDagRenderPanel->SelectVertexAnimation( pAnimationName );
		return;
	}
}


void CDmeDagEditPanel::OnAnimationDeselected( KeyValues *pKeyValues )
{
	vgui::Panel *pPanel = (vgui::Panel *)pKeyValues->GetPtr( "panel", NULL );
	if ( pPanel == m_pAnimationListPanel )
	{
		m_pDagRenderPanel->SelectAnimation( "" );
		return;
	}

	if ( pPanel == m_pVertexAnimationListPanel )
	{
		m_pDagRenderPanel->SelectVertexAnimation( "" );
		return;
	}
}

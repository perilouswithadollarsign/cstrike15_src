//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "matsys_controls/sequencepicker.h"

#include "tier1/utldict.h"
#include "tier1/keyvalues.h"
#include "studio.h"
#include "bone_setup.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "matsys_controls/matsyscontrols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Sequence Picker
//
//-----------------------------------------------------------------------------


static const int POSE_PARAMETER_SLIDER_RANGE = 100;
static const int LAYER_WEIGHT_SLIDER_RANGE = 100;

//-----------------------------------------------------------------------------
// Sort by sequence name
//-----------------------------------------------------------------------------
static int __cdecl SequenceSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("sequence");
	const char *string2 = item2.kv->GetString("sequence");
	return stricmp( string1, string2 );
}

static int __cdecl ActivitySortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("activity");
	const char *string2 = item2.kv->GetString("activity");
	return stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSequencePicker::CSequencePicker( vgui::Panel *pParent, int nFlags ) : BaseClass( pParent, "SequencePicker" )
{
	int nAdvSequenceFlags = PICK_SEQUENCES | PICK_SEQUENCE_PARAMETERS;
	m_bSequenceParams = ( ( nFlags & nAdvSequenceFlags ) == nAdvSequenceFlags );
	m_hSelectedMDL = MDLHANDLE_INVALID;

	m_pRootMotionCheckBox = NULL;
	m_pPoseDefaultButton = NULL;
	V_memset( m_pLayerSequenceSelectors, 0, sizeof( m_pLayerSequenceSelectors ) );
	V_memset( m_pLayerSequenceSliders, 0, sizeof( m_pLayerSequenceSliders ) );
	V_memset( m_pPoseValueSliders, 0, sizeof( m_pPoseValueSliders ) );
	V_memset( m_pPoseValueEntries, 0, sizeof( m_pPoseValueEntries ) );
	V_memset( m_pPoseParameterName, 0, sizeof( m_pPoseParameterName ) );
	V_memset( m_SequenceLayers, 0, sizeof( m_SequenceLayers) );
	V_memset( m_PoseControlMap, -1, sizeof( m_PoseControlMap) );
	V_memset( m_PoseParameters, 0, sizeof( m_PoseParameters ) );

	// property sheet - revisions, changes, etc.
	m_pPreviewSplitter = new Splitter( this, "PreviewSplitter", SPLITTER_MODE_HORIZONTAL, 1 );

	vgui::Panel *pSplitterTopSide = m_pPreviewSplitter->GetChild( 0 );
	vgui::Panel *pSplitterBottomSide = m_pPreviewSplitter->GetChild( 1 );

	// MDL preview
	m_pMDLPreview = new CMDLPanel( pSplitterTopSide, "MDLPreview" );
	SetSkipChildDuringPainting( m_pMDLPreview );

	m_pViewsSheet = new vgui::PropertySheet( pSplitterBottomSide, "ViewsSheet" );
 	m_pViewsSheet->AddActionSignalTarget( this );

	// sequences
	m_pSequencesPage = NULL;
	m_pSequencesList = NULL;
	if ( nFlags & PICK_SEQUENCES )
	{		
		m_pSequencesPage = new PropertyPage( m_pViewsSheet, "SequencesPage" );
		m_pViewsSheet->AddPage( m_pSequencesPage, "Sequences" );
	
		if ( m_bSequenceParams )
		{	
			char controlName[ 64 ];
			const int nNumSeqLines = 32;
			const int nSeqSelectorWidth = 180;

			int yPos = 10;
			for ( int i = 0; i < NUM_SEQUENCE_LAYERS; ++i )
			{
				V_snprintf( controlName, sizeof( controlName ), "LayerSequence%i", i );
				m_pLayerSequenceSelectors[ i ] = new vgui::ComboBox( m_pSequencesPage, controlName, nNumSeqLines, false );
				m_pLayerSequenceSelectors[ i ]->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 10, yPos, 0, 0 );
				m_pLayerSequenceSelectors[ i ]->SetWide( nSeqSelectorWidth );
				m_pLayerSequenceSelectors[ i ]->AddActionSignalTarget( this );
				
				// No slider for the first sequence
				if ( i > 0 )
				{
					yPos += 24;
					V_snprintf( controlName, sizeof( controlName ), "LayerSequenceWeight%i", i );
					m_pLayerSequenceSliders[ i ] = new vgui::Slider( m_pSequencesPage, controlName );
					m_pLayerSequenceSliders[ i ]->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 10, yPos, 0, 0 );
					m_pLayerSequenceSliders[ i ]->SetWide( nSeqSelectorWidth );
					m_pLayerSequenceSliders[ i ]->SetRange( 0, LAYER_WEIGHT_SLIDER_RANGE );
					m_pLayerSequenceSliders[ i ]->AddActionSignalTarget( this );
				}
				yPos += 40;
			}

			// Create the pose parameter controls
			yPos = 10;
			for ( int i = 0; i < NUM_POSE_CONTROLS; ++i )
			{		
				V_snprintf( controlName, sizeof( controlName ), "PoseValueSlider%i", i );
				m_pPoseValueSliders[ i ] = new vgui::Slider( m_pSequencesPage, controlName );
				m_pPoseValueSliders[ i ]->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 210, yPos, 0, 0 );
				m_pPoseValueSliders[ i ]->SetWide( 120 );
				m_pPoseValueSliders[ i ]->SetRange( 0, POSE_PARAMETER_SLIDER_RANGE );
				m_pPoseValueSliders[ i ]->AddActionSignalTarget( this );

				V_snprintf( controlName, sizeof( controlName ), "PoseValueEntry%i", i );
				m_pPoseValueEntries[ i ] = new vgui::TextEntry( m_pSequencesPage, controlName );
				m_pPoseValueEntries[ i ]->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 330, yPos, 0, 0 );
				m_pPoseValueEntries[ i ]->SetWide( 40 );
				m_pPoseValueEntries[ i ]->SendNewLine( true );
				m_pPoseValueEntries[ i ]->AddActionSignalTarget( this );

				V_snprintf( controlName, sizeof( controlName ), "PoseParameterName%i", i );
				m_pPoseParameterName[ i ] = new vgui::ComboBox( m_pSequencesPage, controlName, 8, false );
				m_pPoseParameterName[ i ]->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 380, yPos, 0, 0 );
				m_pPoseParameterName[ i ]->SetWide( 120 );
				m_pPoseParameterName[ i ]->AddActionSignalTarget( this );

				yPos += 32;
			}

			// Add a button to reset the the pose parameters to their defaults
			m_pPoseDefaultButton = new vgui::Button( m_pSequencesPage, "DefaultsButton", "#SequencePicker_Defaults", this, "ResetToDefaults" );
			m_pPoseDefaultButton->SetContentAlignment( Label::a_center );
			m_pPoseDefaultButton->SetWide( 100 );
			m_pPoseDefaultButton->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 210, yPos, 0, 0 );
			m_pPoseDefaultButton->SetEnabled( false );

			// A check box for specifying if root motion is to be generated.
			m_pRootMotionCheckBox = new vgui::CheckButton( m_pSequencesPage, "RootMotionCheckBox", "#SequencePicker_RootMotion" );
			m_pRootMotionCheckBox->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 324, yPos, 0, 0 );
			m_pRootMotionCheckBox->SetWide( 150 );
			m_pRootMotionCheckBox->SetSelected( true );
		}
		else
		{
			m_pSequencesList = new ListPanel( m_pSequencesPage, "SequencesList" );
			m_pSequencesList->AddColumnHeader( 0, "sequence", "sequence", 52, 0 );
			m_pSequencesList->AddActionSignalTarget( this );
			m_pSequencesList->SetSelectIndividualCells( true );
			m_pSequencesList->SetEmptyListText(".MDL file contains no activities");
			m_pSequencesList->SetDragEnabled( true );
			m_pSequencesList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
			m_pSequencesList->SetSortFunc( 0, SequenceSortFunc );
			m_pSequencesList->SetSortColumn( 0 );
		}

	}

	// Activities
	m_pActivitiesPage = NULL;
	m_pActivitiesList = NULL;
	if ( nFlags & PICK_ACTIVITIES )
	{
		m_pActivitiesPage = new PropertyPage( m_pViewsSheet, "ActivitiesPage" );
		m_pViewsSheet->AddPage( m_pActivitiesPage, "Activities" );
		m_pActivitiesList = new ListPanel( m_pActivitiesPage, "ActivitiesList" );
 		m_pActivitiesList->AddColumnHeader( 0, "activity", "activity", 52, 0 );
		m_pActivitiesList->AddActionSignalTarget( this );
		m_pActivitiesList->SetSelectIndividualCells( true );
		m_pActivitiesList->SetEmptyListText( ".MDL file contains no activities" );
 		m_pActivitiesList->SetDragEnabled( true );
		m_pActivitiesList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
		m_pActivitiesList->SetSortFunc( 0, ActivitySortFunc );
		m_pActivitiesList->SetSortColumn( 0 );
	}

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettingsAndUserConfig( "resource/sequencepicker.res" );

	SETUP_PANEL( this );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSequencePicker::~CSequencePicker()
{
}


//-----------------------------------------------------------------------------
// Performs layout
//-----------------------------------------------------------------------------
void CSequencePicker::PerformLayout()
{
	// NOTE: This call should cause auto-resize to occur
	// which should fix up the width of the panels
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	// Layout the mdl splitter
	m_pPreviewSplitter->SetBounds( 0, 0, w, h );
}

	
int CompareSequenceDesc( mstudioseqdesc_t *const *pDescA, mstudioseqdesc_t *const *pDescB )
{
	if ( ( pDescA == NULL ) || ( pDescB == NULL ) )
		return 0;

	if ( ( *pDescA == NULL ) || ( *pDescB == NULL ) ) 
		return 0;

	const char *pNameA = (*pDescA)->pszLabel();
	const char *pNameB = (*pDescB)->pszLabel();
	
	if ( ( pNameA == NULL) || ( pNameB == NULL ) )
		return 0;

	return V_stricmp( pNameA, pNameB );
}

//-----------------------------------------------------------------------------
// Purpose: rebuilds the list of activities	+ sequences
//-----------------------------------------------------------------------------
void CSequencePicker::RefreshActivitiesAndSequencesList()
{
	if ( m_pActivitiesList )
	{
		m_pActivitiesList->RemoveAll();
	}

	if ( m_pSequencesList )
	{
		m_pSequencesList->RemoveAll();
	}

	for ( int i = 0; i < NUM_SEQUENCE_LAYERS; ++i )
	{
		if ( m_pLayerSequenceSelectors[ i ] )
		{
			m_pLayerSequenceSelectors[ i ]->RemoveAll();
		}
	}

	m_pMDLPreview->SetSequence( 0, false );

	if ( m_hSelectedMDL == MDLHANDLE_INVALID )
		return;

	studiohdr_t *hdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );

	CUtlDict<int, unsigned short> activityNames( true, 0, hdr->GetNumSeq() );
	    
	for (int j = 0; j < hdr->GetNumSeq(); j++)
	{
		if ( /*g_viewerSettings.showHidden ||*/ !(hdr->pSeqdesc(j).flags & STUDIO_HIDDEN))
		{
			const char *pActivityName = hdr->pSeqdesc(j).pszActivityName();
			if ( m_pActivitiesList && pActivityName && pActivityName[0] )
			{
				// Multiple sequences can have the same activity name; only add unique activity names
				if ( activityNames.Find( pActivityName ) == activityNames.InvalidIndex() )
				{
					KeyValues *pkv = new KeyValues("node", "activity", pActivityName );
					int nItemID = m_pActivitiesList->AddItem( pkv, 0, false, false );

					KeyValues *pDrag = new KeyValues( "drag", "text", pActivityName );
					pDrag->SetString( "texttype", "activityName" );
					pDrag->SetString( "mdl", vgui::MDLCache()->GetModelName( m_hSelectedMDL ) );
					m_pActivitiesList->SetItemDragData( nItemID, pDrag );

					activityNames.Insert( pActivityName, j );
				}
			}

			const char *pSequenceName = hdr->pSeqdesc(j).pszLabel();
			if ( m_pSequencesList && pSequenceName && pSequenceName[0] )
			{
				KeyValues *pkv = new KeyValues("node", "sequence", pSequenceName);
				int nItemID = m_pSequencesList->AddItem( pkv, 0, false, false );

				KeyValues *pDrag = new KeyValues( "drag", "text", pSequenceName );
				pDrag->SetString( "texttype", "sequenceName" );
				pDrag->SetString( "mdl", vgui::MDLCache()->GetModelName( m_hSelectedMDL ) );
				m_pSequencesList->SetItemDragData( nItemID, pDrag );
			}
		}
	}

	if ( m_pSequencesList )
	{
		m_pSequencesList->SortList();
	}

	if ( m_pActivitiesList )
	{
		m_pActivitiesList->SortList();
	}

	// If using the advanced sequence parameters update the 
	// sequence list for each of the sequence layer drop down boxes.
	if ( m_bSequenceParams )
	{
		int nNumSequences = hdr->GetNumSeq();
		CUtlVector< mstudioseqdesc_t * > sequenceList( 0, nNumSequences );

		for ( int i = 0; i < nNumSequences; ++i )
		{
			mstudioseqdesc_t *pSeqDesc = &hdr->pSeqdesc( i );
			if ( pSeqDesc->flags & STUDIO_HIDDEN ) 
				continue;

			sequenceList.AddToTail( pSeqDesc );
		}

		sequenceList.Sort( CompareSequenceDesc );

	
		// Clear the existing sequence lists.
		for ( int i = 0; i < NUM_SEQUENCE_LAYERS; ++i )
		{
			if ( m_pLayerSequenceSelectors[ i ] )
			{
				m_pLayerSequenceSelectors[ i ]->RemoveAll();
			}
		}

		// Add all of the sequences to each of the drop down lists
		for ( int i = 0; i < sequenceList.Count(); ++i )
		{
			mstudioseqdesc_t *pSeqDesc = sequenceList[ i ];

			const char *pSequenceName = pSeqDesc->pszLabel();
			if ( pSequenceName == NULL )
				continue;
			
			for ( int i = 0; i < NUM_SEQUENCE_LAYERS; ++i )
			{
				if ( m_pLayerSequenceSelectors[ i ] )
				{
					m_pLayerSequenceSelectors[ i ]->AddItem( pSequenceName, new KeyValues( "sequenceItem", "sequence", pSequenceName ) );
				}
			}
		}
	}
}



//-----------------------------------------------------------------------------
// Gets the selected activity/sequence
//-----------------------------------------------------------------------------
CSequencePicker::PickType_t CSequencePicker::GetSelectedSequenceType( )
{
	if ( m_pSequencesPage && ( m_pViewsSheet->GetActivePage() == m_pSequencesPage ) )
		return PICK_SEQUENCES;
	if ( m_pActivitiesPage && ( m_pViewsSheet->GetActivePage() == m_pActivitiesPage ) )
		return PICK_ACTIVITIES;
	return PICK_NONE;
}

const char *CSequencePicker::GetSelectedSequenceName()
{
	if ( m_pSequencesPage && ( m_pViewsSheet->GetActivePage() == m_pSequencesPage ) )
	{
		if ( m_pSequencesList )
		{		
			int nIndex = m_pSequencesList->GetSelectedItem( 0 );
			if ( nIndex >= 0 )
			{
				KeyValues *pkv = m_pSequencesList->GetItem( nIndex );
				return pkv->GetString( "sequence", NULL );
			}
		}
		else if ( m_bSequenceParams )
		{
			studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
			if ( pstudiohdr )
			{			
				CStudioHdr studioHdr( pstudiohdr );
				int nSeqIndex = m_SequenceLayers[ 0 ].m_nSequenceIndex;
				if ( ( nSeqIndex >= 0 ) && ( nSeqIndex < studioHdr.GetNumSeq() ) )
				{
					mstudioseqdesc_t &seqDesc = studioHdr.pSeqdesc( nSeqIndex );
					return seqDesc.pszLabel();
				}
			}
		}
		return NULL;
	}

	if ( m_pActivitiesPage && ( m_pViewsSheet->GetActivePage() == m_pActivitiesPage ) )
	{
		int nIndex = m_pActivitiesList->GetSelectedItem( 0 );
		if ( nIndex >= 0 )
		{
			KeyValues *pkv = m_pActivitiesList->GetItem( nIndex );
			return pkv->GetString( "activity", NULL );
		}
		return NULL;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Get the array of pose parameter values
//-----------------------------------------------------------------------------
void CSequencePicker::GetPoseParameters( CUtlVector< float > &poseParameters ) const
{
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	CStudioHdr studioHdr( pstudiohdr );

	int nNumParams = studioHdr.GetNumPoseParameters();
	for ( int i = 0; i < nNumParams; ++i )
	{
		poseParameters.AddToTail( m_PoseParameters[ i ] );
	}
}


//-----------------------------------------------------------------------------
// Get the array of sequence layers 
//-----------------------------------------------------------------------------
void CSequencePicker::GetSeqenceLayers( CUtlVector< MDLSquenceLayer_t > &sequenceLayers ) const
{
	// Add each of the layers to the array that has a valid sequence and 
	// a weight greater than 0. Skip the base layer, it is handled differently.
	for ( int i = 1; i < NUM_SEQUENCE_LAYERS; ++i )
	{
		const MDLSquenceLayer_t &seqLayer = m_SequenceLayers[ i ];
		if ( ( seqLayer.m_nSequenceIndex >= 0 ) && ( seqLayer.m_flWeight > 0.0f ) )
		{
			sequenceLayers.AddToTail( seqLayer );
		}
	}
}


//-----------------------------------------------------------------------------
// Get the root motion generation state
//-----------------------------------------------------------------------------
bool CSequencePicker::GetGenerateRootMotion() const
{
	if ( m_pRootMotionCheckBox == NULL )
		return false;

	return m_pRootMotionCheckBox->IsSelected();
}


//-----------------------------------------------------------------------------
// Plays the selected activity
//-----------------------------------------------------------------------------
void CSequencePicker::PlayActivity( const char *pActivityName )
{
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	for ( int i = 0; i < pstudiohdr->GetNumSeq(); i++ )
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
		if ( stricmp( seqdesc.pszActivityName(), pActivityName ) == 0 )
		{
			// FIXME: Add weighted sequence selection logic?
			m_pMDLPreview->SetSequence( i, false );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Plays the selected sequence
//-----------------------------------------------------------------------------
void CSequencePicker::UpdateActiveSequence( const char *pSequenceName )
{
	int seqIndex = FindSequence( pSequenceName );
	if ( seqIndex < 0 )
		return;

	if ( m_bSequenceParams )
	{
		SetSequenceLayer( 0, seqIndex, 1.0f );

		// Update the available set of pose parameters
		UpdateAvailablePoseParmeters();
	}


	// Play the sequence
	m_pMDLPreview->SetSequence( seqIndex, false );
}


//-----------------------------------------------------------------------------
// Find the index of the sequence with the specified name
//-----------------------------------------------------------------------------
int CSequencePicker::FindSequence( const char *pSequenceName ) const
{
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	for ( int i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
		if ( !Q_stricmp( seqdesc.pszLabel(), pSequenceName ) )
		{
			return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Update the set of available pose parameters 
//-----------------------------------------------------------------------------
void CSequencePicker::UpdateAvailablePoseParmeters()
{
	if ( m_bSequenceParams )
	{		
		studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
		CStudioHdr studioHdr( pstudiohdr );

		// Determine which of the pose parameters are active for the current sequence
		bool activePoseParameters[ MAXSTUDIOPOSEPARAM ] = { false };
		for ( int iLayer = 0; iLayer < NUM_SEQUENCE_LAYERS; ++iLayer )
		{
			int nSeqIndex = m_SequenceLayers[ iLayer ].m_nSequenceIndex;
			if ( ( nSeqIndex >= 0 ) && ( nSeqIndex < studioHdr.GetNumSeq() ) )
			{
				FindSequencePoseParameters( studioHdr, nSeqIndex, activePoseParameters, MAXSTUDIOPOSEPARAM );
			}
		}

		// Build a list of the active pose parameters
		int nNumMdlParameters = studioHdr.GetNumPoseParameters();
		int sequencePoseParameters[ MAXSTUDIOPOSEPARAM ];
		int nNumSequenceParameters = 0;

		V_memset( sequencePoseParameters, 0xff, sizeof( sequencePoseParameters ) );
		for ( int iParam = 0; iParam < nNumMdlParameters; ++iParam )
		{
			if ( activePoseParameters[ iParam ] )
			{
				sequencePoseParameters[ nNumSequenceParameters++ ] = iParam;
			}
		}

		// Assign the controls the parameters which are active for the current sequence
		int nNumActiveControls = MIN( NUM_POSE_CONTROLS, nNumSequenceParameters );

		for ( int iControl = 0; iControl < nNumActiveControls; ++iControl )
		{
			m_PoseControlMap[ iControl ] = sequencePoseParameters[ iControl ];
			m_pPoseValueSliders[ iControl ]->SetEnabled( true );
			m_pPoseValueEntries[ iControl ]->SetEnabled( true );

			vgui::ComboBox *pPoseNameComboBox = m_pPoseParameterName[ iControl ];
			pPoseNameComboBox->SetEnabled( true );
			pPoseNameComboBox->RemoveAll();
			pPoseNameComboBox->SetNumberOfEditLines( nNumSequenceParameters );

			// Add the parameters to the combo box list
			for ( int iParam = 0; iParam < nNumSequenceParameters; ++iParam )
			{
				int nParamIndex = sequencePoseParameters[ iParam ];
				const mstudioposeparamdesc_t &poseParameter = studioHdr.pPoseParameter( nParamIndex );
				int itemId = pPoseNameComboBox->AddItem( poseParameter.pszName(), new KeyValues( "ParameterItem", "paramIndex", nParamIndex ) );
				if ( iParam == iControl )
				{
					pPoseNameComboBox->ActivateItem( itemId );
				}
			}
		}

		// De-activate the controls for which there are not parameters
		for ( int iControl = nNumActiveControls; iControl < NUM_POSE_CONTROLS; ++iControl )
		{
			m_PoseControlMap[ iControl ] = -1;
			m_pPoseValueSliders[ iControl ]->SetEnabled( false );
			m_pPoseValueSliders[ iControl ]->SetValue( 0 );

			m_pPoseValueEntries[ iControl ]->SetEnabled( false );
			m_pPoseValueEntries[ iControl ]->SetText( "" );

			m_pPoseParameterName[ iControl ]->SetEnabled( false );
			m_pPoseParameterName[ iControl ]->RemoveAll();
			m_pPoseParameterName[ iControl ]->SetText( "" );
		}

		// Disable the reset to defaults button if there are no active controls
		m_pPoseDefaultButton->SetEnabled( ( nNumActiveControls > 0 ) );

		// Update the active controls to match the pose parameters
		UpdatePoseControlsFromParameters();
	}
}


//-----------------------------------------------------------------------------
// Update the value of the specified control control group
//-----------------------------------------------------------------------------
void CSequencePicker::SetPoseParameterValue( float flPoseParameterValue, int nParameterIndex )
{
	if ( ( nParameterIndex < 0  ) || ( nParameterIndex >= MAXSTUDIOPOSEPARAM ) )
		return;

	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	CStudioHdr studioHdr( pstudiohdr );

	// Update the the actual pose parameter value
	const mstudioposeparamdesc_t &poseParameter = studioHdr.pPoseParameter( nParameterIndex );
	m_PoseParameters[ nParameterIndex ] = flPoseParameterValue;
	m_pMDLPreview->SetPoseParameters( m_PoseParameters, MAXSTUDIOPOSEPARAM );

	for ( int iControl = 0; iControl < NUM_POSE_CONTROLS; ++iControl )
	{		
		if ( m_PoseControlMap[ iControl ] == nParameterIndex )
		{	
			// Compute the slider position, since the slider to parameter value mapping is 
			// exponential the parameter value must go through the inverse of the transform 
			// that is used to compute the parameter value from the slider value 
			float flRangeValue = flPoseParameterValue * 2.0f - 1.0f;
			float flNormalized = ( flRangeValue >= 0 ) ? sqrtf( flRangeValue ) : -( sqrtf( -flRangeValue ) );
			int nSliderValue = ( ( flNormalized * 0.5f + 0.5f ) * ( float )( POSE_PARAMETER_SLIDER_RANGE ) ) + 0.5f;
			m_pPoseValueSliders[ iControl ]->SetValue( nSliderValue, false );

			// Update the text entry using the the pose parameter range
			float flFullRangeValue = ( poseParameter.end - poseParameter.start ) * flPoseParameterValue  + poseParameter.start;
			char valueText[ 32 ];
			V_snprintf( valueText, sizeof( valueText ), "%0.2f", flFullRangeValue );
			m_pPoseValueEntries[ iControl ]->SetText( valueText );
		}
	}
}


//-----------------------------------------------------------------------------
// Set all pose parameters to their default values
//-----------------------------------------------------------------------------
void CSequencePicker::ResetPoseParametersToDefault()
{	
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	CStudioHdr studioHdr( pstudiohdr );

	// Re-compute the default pose parameters for the model, and update the
	// controls, clearing any changes that were made previously.
	Studio_CalcDefaultPoseParameters( &studioHdr, m_PoseParameters, MAXSTUDIOPOSEPARAM );
	UpdatePoseControlsFromParameters();
}


//-----------------------------------------------------------------------------
// Update the pose parameter controls using the stored pose parameter values
//-----------------------------------------------------------------------------
void CSequencePicker::UpdatePoseControlsFromParameters()
{
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	CStudioHdr studioHdr( pstudiohdr );

	int nNumParams = studioHdr.GetNumPoseParameters();
	for ( int iParam = 0; iParam < nNumParams; ++iParam )
	{
		float flParameterValue = m_PoseParameters[ iParam ];
		SetPoseParameterValue( flParameterValue, iParam );
	}
}

//-----------------------------------------------------------------------------
// Update the pose parameter controls to match the set of pose parameters for 
// the current mdl
//-----------------------------------------------------------------------------
void CSequencePicker::UpdatePoseParameterControlsForMdl()
{
	if ( m_bSequenceParams )
	{		
		// De-activate all pose controls 
		for ( int iControl = 0; iControl < NUM_POSE_CONTROLS; ++iControl )
		{
			m_PoseControlMap[ iControl ] = -1;
			m_pPoseValueSliders[ iControl ]->SetEnabled( false );
			m_pPoseValueEntries[ iControl ]->SetEnabled( false );
			m_pPoseParameterName[ iControl ]->SetEnabled( false );
		}
	}

	// Reset all pose parameters and controls to the default values
	ResetPoseParametersToDefault();
}


//-----------------------------------------------------------------------------
// Update the sequence and weighting of the specified layer
//-----------------------------------------------------------------------------
void CSequencePicker::SetSequenceLayer( int nLayerIndex, int nSequenceIndex, float flWeight )
{
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	if ( pstudiohdr == NULL )
		return;
	
	int nNumSequences = pstudiohdr->GetNumSeq();

	if ( ( nLayerIndex < 0 ) || ( nLayerIndex >= NUM_SEQUENCE_LAYERS ) || ( nSequenceIndex >= nNumSequences ) )
		return;
	
	m_SequenceLayers[ nLayerIndex ].m_nSequenceIndex = nSequenceIndex;
	m_SequenceLayers[ nLayerIndex ].m_flWeight = MIN( MAX( flWeight, 0.0f ), 1.0f );
	m_pMDLPreview->SetSequenceLayers( m_SequenceLayers + 1, NUM_SEQUENCE_LAYERS - 1 );

	// Update the associated sequence selection combo box to show the new sequence for the specified layer
	vgui::ComboBox *pSequenceSelector = m_pLayerSequenceSelectors[ nLayerIndex ];
	if ( pSequenceSelector )
	{
		// Find the item with the specified sequence
		if ( nSequenceIndex < 0 )
		{
			pSequenceSelector->SetText( "" );
		}
		else
		{
			mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( nSequenceIndex );
			const char *pSequenceName = seqdesc.pszLabel();

			// Find the item with the selected sequence
			int nItems = pSequenceSelector->GetItemCount();
			for ( int i = 0; i < nItems; ++i )
			{
				KeyValues *pItemData = pSequenceSelector->GetItemUserData( i );
				if ( pItemData )
				{
					const char *pItemSeqName = pItemData->GetString( "sequence", "invalidItem" );
					if ( V_stricmp( pSequenceName, pItemSeqName ) == 0 )
					{
						pSequenceSelector->ActivateItem( i );
					}
				}
			}
		}
	}

	// Update the slider for the layer to match the current value
	vgui::Slider *pWeightSlider = m_pLayerSequenceSliders[ nLayerIndex ];
	if ( pWeightSlider )
	{
		int nSliderValue = m_SequenceLayers[ nLayerIndex ].m_flWeight * LAYER_WEIGHT_SLIDER_RANGE;
		pWeightSlider->SetValue( nSliderValue, false );
	}

	// Update the available set of pose parameters
	UpdateAvailablePoseParmeters();
}


//-----------------------------------------------------------------------------
// Update the sequence layer controls to match the current layer data set
//-----------------------------------------------------------------------------
void CSequencePicker::UpdateLayerControls()
{
	for ( int iLayer = 0; iLayer < NUM_SEQUENCE_LAYERS; ++iLayer )
	{
		SetSequenceLayer( iLayer, m_SequenceLayers[ iLayer ].m_nSequenceIndex, m_SequenceLayers[ iLayer ].m_flWeight );
	}
}


//-----------------------------------------------------------------------------
// Clear all the layer information and update the controls
//-----------------------------------------------------------------------------
void CSequencePicker::ResetLayers()
{
	for ( int iLayer = 0; iLayer < NUM_SEQUENCE_LAYERS; ++iLayer )
	{
		m_SequenceLayers[ iLayer ].m_nSequenceIndex = ( iLayer == 0 ) ? 0 : -1;
		m_SequenceLayers[ iLayer ].m_flWeight = ( iLayer == 0 ) ? 1.0f : 0.0f;
	}
	UpdateLayerControls();
}


//-----------------------------------------------------------------------------
// Sets the MDL to select sequences in
//-----------------------------------------------------------------------------
void CSequencePicker::SetMDL( const char *pMDLName )
{
	m_hSelectedMDL = pMDLName ? vgui::MDLCache()->FindMDL( pMDLName ) : MDLHANDLE_INVALID;
	if ( vgui::MDLCache()->IsErrorModel( m_hSelectedMDL ) )
	{
		m_hSelectedMDL = MDLHANDLE_INVALID;
	}
	m_pMDLPreview->SetMDL( m_hSelectedMDL );
	m_pMDLPreview->LookAtMDL();

	ResetLayers();
	RefreshActivitiesAndSequencesList();
	UpdateLayerControls();
	UpdatePoseParameterControlsForMdl();
}


//-----------------------------------------------------------------------------
// Purpose: Called when a page is shown
//-----------------------------------------------------------------------------
void CSequencePicker::OnPageChanged( )
{
	if ( m_pSequencesPage && ( m_pViewsSheet->GetActivePage() == m_pSequencesPage ) )
	{
		const char *pSequenceName = GetSelectedSequenceName();
		if ( pSequenceName )
		{
			UpdateActiveSequence( pSequenceName );
			PostActionSignal( new KeyValues( "SequencePreviewChanged", "sequence", pSequenceName ) );
		}
		return;
	}

	if ( m_pActivitiesPage && ( m_pViewsSheet->GetActivePage() == m_pActivitiesPage ) )
	{
		const char *pActivityName = GetSelectedSequenceName();
		if ( pActivityName )
		{
			PlayActivity( pActivityName );
			PostActionSignal( new KeyValues( "SequencePreviewChanged", "activity", pActivityName ) );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: refreshes dialog on text changing
//-----------------------------------------------------------------------------
void CSequencePicker::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr("panel", NULL);
	if ( m_pSequencesList && (pPanel == m_pSequencesList ) )
	{
		const char *pSequenceName = GetSelectedSequenceName();
		if ( pSequenceName )
		{
			UpdateActiveSequence( pSequenceName );
			PostActionSignal( new KeyValues( "SequencePreviewChanged", "sequence", pSequenceName ) );
		}
		return;
	}

	if ( m_pActivitiesList && ( pPanel == m_pActivitiesList ) )
	{
		const char *pActivityName = GetSelectedSequenceName();
		if ( pActivityName )
		{
			PlayActivity( pActivityName );
			PostActionSignal( new KeyValues( "SequencePreviewChanged", "activity", pActivityName ) );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Process the slider moved message, determine which slider was moved and 
// update the corresponding pose parameter value.
//-----------------------------------------------------------------------------
void CSequencePicker::OnSliderMoved( KeyValues *pData )
{
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(pData)->GetPtr("panel") );

	for ( int i = 0; i < NUM_SEQUENCE_LAYERS; ++i )
	{
		if ( pPanel == m_pLayerSequenceSliders[ i ] )
		{
			int nValue = m_pLayerSequenceSliders[ i ]->GetValue();
			float flWeight = ( float )( nValue ) / ( float )( LAYER_WEIGHT_SLIDER_RANGE );
			SetSequenceLayer( i, m_SequenceLayers[ i ].m_nSequenceIndex, flWeight );
			return;
		}
	}


	for ( int i = 0; i < NUM_POSE_CONTROLS; ++i )
	{
		if ( pPanel == m_pPoseValueSliders[ i ] )
		{
			int nControlIndex = i;
			
			// Use an exponential mapping around which provides the
			// most precision around the 0.5 of the pose parameter
			int nValue = m_pPoseValueSliders[ nControlIndex ]->GetValue();
			float flHalfRange =  ( float )( POSE_PARAMETER_SLIDER_RANGE / 2 );
			float flScaledValue = ( ( float )( nValue ) - flHalfRange ) / flHalfRange;
			float flPoseParameterValue = ( flScaledValue * fabs( flScaledValue ) ) * 0.5f + 0.5f;

			int nParameterIndex = m_PoseControlMap[ nControlIndex ];
			SetPoseParameterValue( flPoseParameterValue, nParameterIndex );

			return;
		}
	}

}


//-----------------------------------------------------------------------------
// Process the text entry notification messages. Determine which text 
// entry was changed and update the value of the corresponding pose parameter.
//-----------------------------------------------------------------------------
void CSequencePicker::OnTextKillFocus( KeyValues *pData )
{
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(pData)->GetPtr("panel") );

	int nControlIndex = -1;
	for ( int i = 0; i < NUM_POSE_CONTROLS; ++i )
	{
		if ( pPanel == m_pPoseValueEntries[ i ] )
		{
			nControlIndex = i;
			break;
		}
	}

	if ( nControlIndex < 0 )
		return;

	// Get the value from the text entry and re-map using the pose parameter range
	float flValue = m_pPoseValueEntries[ nControlIndex ]->GetValueAsFloat();

	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	CStudioHdr studioHdr( pstudiohdr );

	int nParameterIndex = m_PoseControlMap[ nControlIndex ];
	const mstudioposeparamdesc_t &poseParameter = studioHdr.pPoseParameter( nParameterIndex );

	float flMinVal = MIN( poseParameter.start, poseParameter.end );
	float flMaxVal = MAX( poseParameter.start, poseParameter.end );
	float flRangeValue = MIN( flMaxVal, MAX( flMinVal, flValue ) );
	float flNormalized = ( flRangeValue - flMinVal ) / ( flMaxVal - flMinVal );

	SetPoseParameterValue( flNormalized, nParameterIndex );
}

void CSequencePicker::OnTextNewLine( KeyValues *pData )
{
	OnTextKillFocus( pData );
}


//-----------------------------------------------------------------------------
// Handle changes to the combo box selection
//-----------------------------------------------------------------------------
void CSequencePicker::OnTextChanged( KeyValues *pData )
{
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(pData)->GetPtr("panel") );
	
	// Check to see if a new sequence layer was selected
	for ( int i = 0; i < NUM_SEQUENCE_LAYERS; ++i )
	{
		if ( pPanel == m_pLayerSequenceSelectors[ i ] )
		{
			const char *pSequenceName = pData->GetString( "text" );

			if ( i == 0 )	
			{
				UpdateActiveSequence( pSequenceName );
			}
			else
			{
				int nSequenceIndex = FindSequence( pSequenceName );
				SetSequenceLayer( i, nSequenceIndex, m_SequenceLayers[ i ].m_flWeight );
			}
			return;
		}
	}

	// Determine which combo box has changed if any, this message will also be sent by the 
	// text entry but is ignored because it sent every time any change is made, even a just 
	// one character not necessarily a whole value change, so it is ignored.
	int nControlIndex = -1;
	for ( int i = 0; i < NUM_POSE_CONTROLS; ++i )
	{
		if ( pPanel == m_pPoseParameterName[ i ] )
		{
			nControlIndex = i;

			KeyValues *pItemData = m_pPoseParameterName[ nControlIndex ]->GetActiveItemUserData();
			int nParameterIndex = pItemData->GetInt( "paramIndex", -1 );
			if ( ( nParameterIndex >= 0 ) && ( nParameterIndex < MAXSTUDIOPOSEPARAM ) )
			{
				m_PoseControlMap[ nControlIndex ] = nParameterIndex;
				SetPoseParameterValue( m_PoseParameters[ nParameterIndex ], nParameterIndex );
			}
			return;
		}
	}


}


//-----------------------------------------------------------------------------
// Process command messages
//-----------------------------------------------------------------------------
void CSequencePicker::OnCommand( const char *pCommand )
{
	if ( !V_strcmp( "ResetToDefaults", pCommand ) )
	{
		ResetPoseParametersToDefault();
	}
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CSequencePickerFrame::CSequencePickerFrame( vgui::Panel *pParent, int nFlags ) : BaseClass( pParent, "SequencePickerFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pPicker = new CSequencePicker( this, nFlags );
	m_pPicker->AddActionSignalTarget( this );
	m_pOpenButton = new Button( this, "OpenButton", "#FileOpenDialog_Open", this, "Open" );
	m_pCancelButton = new Button( this, "CancelButton", "#FileOpenDialog_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/sequencepickerframe.res" );

	m_pOpenButton->SetEnabled( false );
}


//-----------------------------------------------------------------------------
// Purpose: Activate the dialog
//-----------------------------------------------------------------------------
void CSequencePickerFrame::DoModal( const char *pMDLName )
{
	m_pPicker->SetMDL( pMDLName );
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On mdl preview changed
//-----------------------------------------------------------------------------
void CSequencePickerFrame::OnSequencePreviewChanged( KeyValues *pKeyValues )
{
	const char *pSequence = pKeyValues->GetString( "sequence", NULL );
	const char *pActivity = pKeyValues->GetString( "activity", NULL );
	m_pOpenButton->SetEnabled( pSequence || pActivity );
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CSequencePickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Open" ) )
	{
		CSequencePicker::PickType_t type = m_pPicker->GetSelectedSequenceType( );
		if (( type == CSequencePicker::PICK_SEQUENCES ) || ( type == CSequencePicker::PICK_ACTIVITIES ))
		{
			const char *pSequenceName = m_pPicker->GetSelectedSequenceName();
			if ( pSequenceName )
			{
				if ( type == CSequencePicker::PICK_SEQUENCES )
				{
					// Create the key values with the sequence name
					KeyValues *pKV = new KeyValues( "SequenceSelected", "sequence", pSequenceName );

					// Add a set of keys descibing the sequence layers
					CUtlVector< MDLSquenceLayer_t > sequenceLayers;
					m_pPicker->GetSeqenceLayers( sequenceLayers );
					int nNumLayers = sequenceLayers.Count();
					pKV->SetInt( "numLayers", nNumLayers );

					char layerName[ 32 ];
					for ( int i = 0; i < nNumLayers; ++i )
					{
						V_snprintf( layerName, sizeof( layerName ), "layer%i", i );
						KeyValues *pLayerData = new KeyValues( layerName );
						pLayerData->SetInt( "sequence", sequenceLayers[ i ].m_nSequenceIndex );
						pLayerData->SetFloat( "weight", sequenceLayers[ i ].m_flWeight );
						pKV->AddSubKey( pLayerData );
					}

					// Add the pose parameters for the sequence, there is a key 
					// for the number of parameters and then a key for each parameter.
					CUtlVector< float > poseParameters( 0, MAXSTUDIOPOSEPARAM );
					m_pPicker->GetPoseParameters( poseParameters );
					int nNumParams = poseParameters.Count();
					pKV->SetInt( "numPoseParameters", nNumParams );

					char poseParameterName[ 32 ];
					for ( int i = 0; i < nNumParams; ++i )
					{
						V_snprintf( poseParameterName, sizeof( poseParameterName ), "poseParameter%i", i );
						pKV->SetFloat( poseParameterName, poseParameters[ i ] );
					}

					// Set the value indicating if root motion should be generated.
					bool bRootMoition = m_pPicker->GetGenerateRootMotion();
					pKV->SetBool( "rootMotion", bRootMoition );

					PostActionSignal( pKV );
				}
				else
				{
					PostActionSignal( new KeyValues("SequenceSelected", "activity", pSequenceName ) );
				}
				CloseModal();
				return;
			}
		}
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

	

//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "filesystem.h"
#include "matsys_controls/vmtpicker.h"
#include "matsys_controls/vmtpreviewpanel.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/Panel.h"
#include "matsys_controls/sheetsequencepanel.h"
#include "bitmap/psheet.h"
#include "keyvalues.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// VMT Preview Toolbar - controls for tweaking the VMT preview
//-----------------------------------------------------------------------------

CVMTPreviewToolbar::CVMTPreviewToolbar( vgui::Panel *parent, const char *panelName, CVMTPicker *parentpicker ) :
	BaseClass( parent, panelName ), m_pParentPicker( parentpicker )
{
	vgui::CBoxSizer* pSizer = new vgui::CBoxSizer( vgui::ESLD_HORIZONTAL );
	SetSizer( pSizer );

	// buttons + controls

	vgui::SizerAddArgs_t buttonAddArgs = vgui::SizerAddArgs_t().FixedSize(-1,18).Padding(2);

	pSizer->AddSpacer( vgui::SizerAddArgs_t().Padding( 15 ) );

	m_pPrevSeqButton = new vgui::Button( this, "PrevSeqButton", "<", this );
	m_pPrevSeqButton->SetCommand( new KeyValues( "OnPrevSequence" ) );
	pSizer->AddPanel( m_pPrevSeqButton, buttonAddArgs );

	m_pSequenceSelection = new vgui::MenuButton( this, "SequenceSelection", "-" );
	pSizer->AddPanel( m_pSequenceSelection, vgui::SizerAddArgs_t().FixedSize(120,18).Padding(2) );

	m_pNextSeqButton = new vgui::Button( this, "NextSeqButton", ">", this );
	m_pNextSeqButton->SetCommand( new KeyValues( "OnNextSequence" ) );
	pSizer->AddPanel( m_pNextSeqButton, buttonAddArgs );

	m_pSequenceSelection_Second = new vgui::MenuButton( this, "SequenceSelection", "(Color)" );
	pSizer->AddPanel( m_pSequenceSelection_Second, buttonAddArgs );

	m_pSheetPreviewSpeed = new vgui::Slider( this, "SheetPreviewSpeed" );
	m_pSheetPreviewSpeed->SetRange( 5, 3000 );
	m_pSheetPreviewSpeed->SetValue( 750 );
	m_pSheetPreviewSpeed->AddActionSignalTarget( this );
	pSizer->AddPanel( m_pSheetPreviewSpeed, vgui::SizerAddArgs_t().FixedSize(150,18).Padding(2) );

	// preview panels

	m_pSheetPanel = new CSheetSequencePanel(this, "sheetpanel");
	m_pSheetPanel->AddActionSignalTarget( this );
	m_pSequenceSelection->SetMenu( m_pSheetPanel );

	m_pSheetPanel_Second = new CSheetSequencePanel(this, "sheetpanel_second");
	m_pSheetPanel_Second->SetSecondSequenceView( true );
	m_pSheetPanel_Second->AddActionSignalTarget( this );
	m_pSequenceSelection_Second->SetMenu( m_pSheetPanel_Second );

	UpdateToolbarGUI();
}


void CVMTPreviewToolbar::OnNextSequence( )
{
	int nSeq = m_pParentPicker->GetCurrentSequence();
	nSeq = (nSeq + 1) % m_pParentPicker->GetSheetSequenceCount();
	
	m_pParentPicker->SetSelectedSequence( nSeq );
	UpdateToolbarGUI();
}

void CVMTPreviewToolbar::OnPrevSequence( )
{
	int nSeq = m_pParentPicker->GetCurrentSequence();

	if ( nSeq == 0 )
	{
		nSeq = m_pParentPicker->GetSheetSequenceCount()-1;
	}
	else
	{
		nSeq = (nSeq - 1) % m_pParentPicker->GetSheetSequenceCount();
	}

	m_pParentPicker->SetSelectedSequence( nSeq );
	UpdateToolbarGUI();
}

void CVMTPreviewToolbar::OnSliderMoved( KeyValues *pData )
{
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(pData)->GetPtr("panel") );

	if ( pPanel == m_pSheetPreviewSpeed )
	{
		m_pParentPicker->SetSheetPreviewSpeed( m_pSheetPreviewSpeed->GetValue() );
	}
}

void CVMTPreviewToolbar::PopulateSequenceMenu( vgui::Menu *menu )
{
	menu->DeleteAllItems();

	int nSequences = m_pParentPicker->GetSheetSequenceCount();
	for ( int i = 0; i < nSequences; ++i )
	{
		char sz[64];
		Q_snprintf( sz, sizeof( sz ), "Sequence %d", i );
		menu->AddMenuItem( "seqitem", sz, new KeyValues( "OnSelectSequence", "nSequenceNumber", i ), this );
	}
}

int CVMTPreviewToolbar::GetSequenceMenuItemCount( )
{
	return m_pParentPicker->GetSheetSequenceCount();
}

void CVMTPreviewToolbar::OnSelectSequence( int nSequenceNumber )
{
	m_pParentPicker->SetSelectedSequence(nSequenceNumber);
	UpdateToolbarGUI();
}

void CVMTPreviewToolbar::OnSheetSequenceSelected( KeyValues *pData )
{
	if ( pData->GetBool("bIsSecondSequence") )
	{
		m_pParentPicker->SetSelectedSecondarySequence( pData->GetInt("nSequenceNumber") );
	}
	else
	{
		m_pParentPicker->SetSelectedSequence( pData->GetInt("nSequenceNumber") );
	}
	UpdateToolbarGUI();
}

void CVMTPreviewToolbar::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pSequenceSelection->SetFont( pScheme->GetFont( "DefaultVerySmall" ) );
}

void CVMTPreviewToolbar::UpdateToolbarGUI()
{
	int nSequences = m_pParentPicker->GetSheetSequenceCount();
	int nCurrentSequence = m_pParentPicker->GetCurrentSequence();
	int nRealSeqNumber = m_pParentPicker->GetRealSequenceNumber();
	bool bSecondPicker = CSheetExtended::IsMaterialSeparateAlphaColorMaterial( m_pParentPicker->GetMaterial() );

	if ( nSequences == 0 )
	{
		m_pSequenceSelection->SetText( "No Sequences" );
		m_pSequenceSelection->SetEnabled(false);
		m_pNextSeqButton->SetEnabled(false);
		m_pPrevSeqButton->SetEnabled(false);
		m_pSequenceSelection_Second->SetVisible(false);

		m_pSheetPanel->SetFromMaterial( NULL );
		m_pSheetPanel_Second->SetFromMaterial( NULL );
	}
	else
	{
		char sz[64];
		Q_snprintf( sz, sizeof( sz ), "%d/%d Sequences (#%d)", nCurrentSequence+1, nSequences, nRealSeqNumber );
		m_pSequenceSelection->SetText( sz );
		m_pSequenceSelection->SetEnabled(true);
		m_pSequenceSelection_Second->SetVisible(bSecondPicker);

		m_pNextSeqButton->SetEnabled(true);
		m_pPrevSeqButton->SetEnabled(true);

		m_pSheetPanel->SetFromMaterial( m_pParentPicker->GetMaterial() );
		m_pSheetPanel_Second->SetFromMaterial( m_pParentPicker->GetMaterial() );
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVMTPicker::CVMTPicker( vgui::Panel *pParent, bool bAllowMultiselect ) : 
	BaseClass( pParent, "VMT Files", "vmt", "materials", "vmtName" )
{
	// Horizontal splitter for preview
	m_pPreviewSplitter = new vgui::Splitter( this, "PreviewSplitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pPreviewSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pPreviewSplitter->GetChild( 1 );

	m_p2D3DSplitter = new vgui::Splitter( pSplitterRightSide, "2D3DSplitter", vgui::SPLITTER_MODE_HORIZONTAL, 1 );
 	vgui::Panel *pSplitterTopSide = m_p2D3DSplitter->GetChild( 0 );
	vgui::Panel *pSplitterBottomSide = m_p2D3DSplitter->GetChild( 1 );

	// VMT preview
	m_pVMTPreview2D = new CVMTPreviewPanel( pSplitterTopSide, "VMTPreview2D" );
	m_pVMTPreview3D = new CVMTPreviewPanel( pSplitterBottomSide, "VMTPreview3D" );
	m_pVMTPreview3D->DrawIn3DMode( true );

	m_pVMTPreviewToolbar = new CVMTPreviewToolbar( pSplitterBottomSide, "PreviewToolbar", this );

	// Standard browser controls
 	CreateStandardControls( pSplitterLeftSide, bAllowMultiselect );

	LoadControlSettingsAndUserConfig( "resource/vmtpicker.res" );
}

CVMTPicker::~CVMTPicker()
{
}

void CVMTPicker::SetSheetPreviewSpeed( float flPreviewSpeed )
{
	m_pVMTPreview2D->SetSheetPreviewSpeed( flPreviewSpeed );
	m_pVMTPreview3D->SetSheetPreviewSpeed( flPreviewSpeed );
}

void CVMTPicker::SetSelectedSequence( int nSequence )
{
	m_pVMTPreview2D->SetSheetSequence( nSequence );
	m_pVMTPreview3D->SetSheetSequence( nSequence );
}

void CVMTPicker::SetSelectedSecondarySequence( int nSequence )
{
	m_pVMTPreview2D->SetSecondarySheetSequence( nSequence );
	m_pVMTPreview3D->SetSecondarySheetSequence( nSequence );
}

int CVMTPicker::GetSheetSequenceCount()
{
	return m_pVMTPreview3D->GetSheetSequenceCount();
}

int CVMTPicker::GetCurrentSequence()
{
	return m_pVMTPreview3D->GetCurrentSequence();
}

int CVMTPicker::GetCurrentSecondarySequence()
{
	return m_pVMTPreview3D->GetCurrentSecondarySequence();
}

int CVMTPicker::GetRealSequenceNumber()
{
	return m_pVMTPreview3D->GetRealSequenceNumber();
}

void CVMTPicker::CustomizeSelectionMessage( KeyValues *pKeyValues )
{
	BaseClass::CustomizeSelectionMessage(pKeyValues);

	pKeyValues->SetInt( "sheet_sequence_count", GetSheetSequenceCount() );
	pKeyValues->SetInt( "sheet_sequence_number", GetCurrentSequence() );
	pKeyValues->SetInt( "sheet_sequence_secondary_number", GetCurrentSecondarySequence() );
}

CSheetExtended* CVMTPicker::GetSheet()
{
	return m_pVMTPreview3D->GetSheet();
}

IMaterial* CVMTPicker::GetMaterial()
{
	return m_pVMTPreview3D->GetMaterial();
}

//-----------------------------------------------------------------------------
// Derived classes have this called when the previewed asset changes
//-----------------------------------------------------------------------------
void CVMTPicker::OnSelectedAssetPicked( const char *pAssetName )
{
	m_pVMTPreview2D->SetVMT( pAssetName );
	m_pVMTPreview3D->SetVMT( pAssetName );
	m_pVMTPreviewToolbar->UpdateToolbarGUI();
}

//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CVMTPickerFrame::CVMTPickerFrame( vgui::Panel *pParent, const char *pTitle, bool bAllowMultiselect ) : 
	BaseClass( pParent )
{
	SetAssetPicker( new CVMTPicker( this, bAllowMultiselect ) );
	LoadControlSettingsAndUserConfig( "resource/vmtpickerframe.res" );
	SetTitle( pTitle, false );
}


//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "matsys_controls/mdlpicker.h"
#include "tier1/keyvalues.h"
#include "tier1/utldict.h"
#include "filesystem.h"
#include "studio.h"
#include "matsys_controls/matsyscontrols.h"
#include "matsys_controls/mdlpanel.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/DirectorySelectDialog.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/Cursor.h"
#include "matsys_controls/assetpicker.h"
#include "matsys_controls/colorpickerpanel.h"
#include "dmxloader/dmxloader.h"
#include "utlbuffer.h"
#include "bitmap/tgawriter.h"
#include "tier3/tier3.h"
#include "istudiorender.h"
#include "../vgui2/src/VPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// MDL Picker
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Sort by MDL name
//-----------------------------------------------------------------------------
static int __cdecl MDLBrowserSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("mdl");
	const char *string2 = item2.kv->GetString("mdl");
	return stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMDLPicker::CMDLPicker( vgui::Panel *pParent, int nFlags ) : 
	BaseClass( pParent, "MDL Files", "mdl", "models", "mdlName" )
{
	m_hSelectedMDL = MDLHANDLE_INVALID;

	m_nFlags = nFlags;	// remember what we show and what not

	m_pRenderPage = NULL;
	m_pSequencesPage = NULL;
	m_pActivitiesPage = NULL;
	m_pSkinsPage = NULL;
	m_pInfoPage = NULL;

	m_pSequencesList = NULL;
	m_pActivitiesList = NULL;

	// Horizontal splitter for mdls
	m_pFileBrowserSplitter = new Splitter( this, "FileBrowserSplitter", SPLITTER_MODE_VERTICAL, 1 );

	float flFractions[] = { 0.33f, 0.67f };

	m_pFileBrowserSplitter->RespaceSplitters( flFractions );

	vgui::Panel *pSplitterLeftSide = m_pFileBrowserSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pFileBrowserSplitter->GetChild( 1 );

	// Standard browser controls
	pSplitterLeftSide->RequestFocus();
	CreateStandardControls( pSplitterLeftSide, false );

	// property sheet - revisions, changes, etc.
	m_pPreviewSplitter = new Splitter( pSplitterRightSide, "PreviewSplitter", SPLITTER_MODE_HORIZONTAL, 1 );

	vgui::Panel *pSplitterTopSide = m_pPreviewSplitter->GetChild( 0 );
	vgui::Panel *pSplitterBottomSide = m_pPreviewSplitter->GetChild( 1 );

	// MDL preview
	m_pMDLPreview = new CMDLPanel( pSplitterTopSide, "MDLPreview" );
	SetSkipChildDuringPainting( m_pMDLPreview );

	m_pViewsSheet = new vgui::PropertySheet( pSplitterBottomSide, "ViewsSheet" );
	m_pViewsSheet->AddActionSignalTarget( this );

	// now add wanted features
	if ( nFlags & PAGE_RENDER )
	{
		m_pRenderPage = new vgui::PropertyPage( m_pViewsSheet, "RenderPage" );

		m_pRenderPage->AddActionSignalTarget( this );

        m_pRenderPage->LoadControlSettingsAndUserConfig( "resource/mdlpickerrender.res" );

		RefreshRenderSettings();

		// ground
		Button *pSelectProbe = (Button*)m_pRenderPage->FindChildByName( "ChooseLightProbe" );
		pSelectProbe->AddActionSignalTarget( this );
	}

	if ( nFlags & PAGE_SEQUENCES )
	{
		m_pSequencesPage = new vgui::PropertyPage( m_pViewsSheet, "SequencesPage" );

		m_pSequencesList = new vgui::ListPanel( m_pSequencesPage, "SequencesList" );
		m_pSequencesList->AddColumnHeader( 0, "sequence", "sequence", 52, 0 );
		m_pSequencesList->AddActionSignalTarget( this );
		m_pSequencesList->SetSelectIndividualCells( true );
		m_pSequencesList->SetEmptyListText("No .MDL file currently selected.");
		m_pSequencesList->SetDragEnabled( true );
		m_pSequencesList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -6, -6 );
	}

	if ( nFlags & PAGE_ACTIVITIES )
	{
		m_pActivitiesPage = new vgui::PropertyPage( m_pViewsSheet, "ActivitiesPage" );

		m_pActivitiesList = new vgui::ListPanel( m_pActivitiesPage, "ActivitiesList" );
		m_pActivitiesList->AddColumnHeader( 0, "activity", "activity", 52, 0 );
		m_pActivitiesList->AddActionSignalTarget( this );
		m_pActivitiesList->SetSelectIndividualCells( true );
		m_pActivitiesList->SetEmptyListText( "No .MDL file currently selected." );
		m_pActivitiesList->SetDragEnabled( true );
		m_pActivitiesList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -6, -6 );
	}

	if ( nFlags & PAGE_SKINS )
	{
		m_pSkinsPage = new vgui::PropertyPage( m_pViewsSheet, "SkinsPage" );

		m_pSkinsList = new vgui::ListPanel( m_pSkinsPage, "SkinsList" );
		m_pSkinsList->AddColumnHeader( 0, "skin", "skin", 52, 0 );
		m_pSkinsList->AddActionSignalTarget( this );
		m_pSkinsList->SetSelectIndividualCells( true );
		m_pSkinsList->SetEmptyListText( "No .MDL file currently selected." );
		m_pSkinsList->SetDragEnabled( true );
		m_pSkinsList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -6, -6 );		
	}

	if ( nFlags & PAGE_INFO )
	{
		m_pInfoPage = new vgui::PropertyPage( m_pViewsSheet, "InfoPage" );

		m_pInfoPage->AddActionSignalTarget( this );

		m_pInfoPage->LoadControlSettingsAndUserConfig( "resource/mdlpickerinfo.res" );

		CheckButton * pTempCheck = (CheckButton *)m_pInfoPage->FindChildByName( "PhysicsObject" );
		pTempCheck->SetDisabledFgColor1( pTempCheck->GetFgColor());
		pTempCheck->SetDisabledFgColor2( pTempCheck->GetFgColor());
		pTempCheck = (CheckButton *)m_pInfoPage->FindChildByName( "StaticObject" );
		pTempCheck->SetDisabledFgColor1( pTempCheck->GetFgColor());
		pTempCheck->SetDisabledFgColor2( pTempCheck->GetFgColor());
		pTempCheck = (CheckButton *)m_pInfoPage->FindChildByName( "DynamicObject" );
		pTempCheck->SetDisabledFgColor1( pTempCheck->GetFgColor());
		pTempCheck->SetDisabledFgColor2( pTempCheck->GetFgColor());

		m_pPropDataList = new vgui::ListPanel( m_pInfoPage, "PropData" );
		m_pPropDataList->AddColumnHeader( 0, "key", "key", 250, ListPanel::COLUMN_FIXEDSIZE );		
		m_pPropDataList->AddColumnHeader( 1, "value", "value", 52, 0 );
		m_pPropDataList->AddActionSignalTarget( this );
		m_pPropDataList->SetSelectIndividualCells( false );
		m_pPropDataList->SetEmptyListText( "No prop_data available." );
		m_pPropDataList->SetDragEnabled( true );
		m_pPropDataList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 6, 72, -6, -6 );

		RefreshRenderSettings();
	}

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettingsAndUserConfig( "resource/mdlpicker.res" );

	// Pages must be added after control settings are set up
	if ( m_pRenderPage )
	{
		m_pViewsSheet->AddPage( m_pRenderPage, "Render" );
	}
	if ( m_pSequencesPage )
	{
		m_pViewsSheet->AddPage( m_pSequencesPage, "Sequences" );
	}
	if ( m_pActivitiesPage )
	{
		m_pViewsSheet->AddPage( m_pActivitiesPage, "Activities" );
	}
	if ( m_pSkinsPage )
	{
		m_pViewsSheet->AddPage( m_pSkinsPage, "Skins" );
	}
	if ( m_pInfoPage )
	{
		m_pViewsSheet->AddPage( m_pInfoPage, "Info" );
	}
}

void CMDLPicker::RefreshRenderSettings()
{
	vgui::CheckButton *pToggle;

	if ( !m_pRenderPage )
		return;

	// ground
	pToggle = (vgui::CheckButton*)m_pRenderPage->FindChildByName("NoGround");
	pToggle->AddActionSignalTarget( this );
	m_pMDLPreview->SetGroundGrid( !pToggle->IsSelected() );

	// collision
	pToggle = (vgui::CheckButton*)m_pRenderPage->FindChildByName("Collision");
	pToggle->AddActionSignalTarget( this );
	m_pMDLPreview->SetCollsionModel( pToggle->IsSelected() );

	// wireframe
	pToggle = (vgui::CheckButton*)m_pRenderPage->FindChildByName("Wireframe");
	pToggle->AddActionSignalTarget( this );
	m_pMDLPreview->SetWireFrame( pToggle->IsSelected() );

	// lockview
	pToggle = (vgui::CheckButton*)m_pRenderPage->FindChildByName("LockView");
	pToggle->AddActionSignalTarget( this );
	m_pMDLPreview->SetLockView( pToggle->IsSelected() );

	// look at camera
	pToggle = (vgui::CheckButton*)m_pRenderPage->FindChildByName("LookAtCamera");
	pToggle->AddActionSignalTarget( this );
	m_pMDLPreview->SetLookAtCamera( pToggle->IsSelected() );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMDLPicker::~CMDLPicker()
{
}


//-----------------------------------------------------------------------------
// Performs layout
//-----------------------------------------------------------------------------
void CMDLPicker::PerformLayout()
{
	// NOTE: This call should cause auto-resize to occur
	// which should fix up the width of the panels
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	// Layout the mdl splitter
	m_pFileBrowserSplitter->SetBounds( 0, 0, w, h );
}


//-----------------------------------------------------------------------------
// Buttons on various pages
//-----------------------------------------------------------------------------
void CMDLPicker::OnAssetSelected( KeyValues *pParams )
{
	const char *pAsset = pParams->GetString( "asset" );

	char pProbeBuf[MAX_PATH];
	Q_snprintf( pProbeBuf, sizeof(pProbeBuf), "materials/lightprobes/%s", pAsset );

	BeginDMXContext();
	CDmxElement *pLightProbe = NULL; 
	bool bOk = UnserializeDMX( pProbeBuf, "GAME", true, &pLightProbe );
	if ( !pLightProbe || !bOk )
	{
		char pBuf[1024];
		Q_snprintf( pBuf, sizeof(pBuf), "Error loading lightprobe file '%s'!\n", pProbeBuf ); 
		vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Error Loading File!\n", pBuf, GetParent() );
		pMessageBox->DoModal( );

		EndDMXContext( true );
		return;
	}

	m_pMDLPreview->SetLightProbe( pLightProbe );
	EndDMXContext( true );
}


//-----------------------------------------------------------------------------
// Buttons on various pages
//-----------------------------------------------------------------------------
void CMDLPicker::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "ChooseLightProbe" ) )
	{
		CAssetPickerFrame *pPicker = new CAssetPickerFrame( this, "Select Light Probe (.prb) File",
			"Light Probe", "prb", "materials/lightprobes", "lightprobe" );
		pPicker->DoModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Purpose: rebuilds the list of activities
//-----------------------------------------------------------------------------
void CMDLPicker::RefreshActivitiesAndSequencesList()
{
	m_pActivitiesList->RemoveAll();
	m_pSequencesList->RemoveAll();
	m_pMDLPreview->SetSequence( 0, false );

	if ( m_hSelectedMDL == MDLHANDLE_INVALID )
	{
		m_pActivitiesList->SetEmptyListText("No .MDL file currently selected");
		m_pSequencesList->SetEmptyListText("No .MDL file currently selected");
		return;
	}

	m_pActivitiesList->SetEmptyListText(".MDL file contains no activities");
	m_pSequencesList->SetEmptyListText(".MDL file contains no sequences");

	studiohdr_t *hdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	
	CUtlDict<int, unsigned short> activityNames( true, 0, hdr->GetNumSeq() );

	for (int j = 0; j < hdr->GetNumSeq(); j++)
	{
		if ( /*g_viewerSettings.showHidden ||*/ !(hdr->pSeqdesc(j).flags & STUDIO_HIDDEN))
		{
			const char *pActivityName = hdr->pSeqdesc(j).pszActivityName();
			if ( pActivityName && pActivityName[0] )
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
			if ( pSequenceName && pSequenceName[0] )
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
}

//-----------------------------------------------------------------------------
// A MDL was selected
//-----------------------------------------------------------------------------
void CMDLPicker::OnSelectedAssetPicked( const char *pMDLName )
{
	char pRelativePath[MAX_PATH];
	if ( pMDLName )
	{
		Q_snprintf( pRelativePath, sizeof(pRelativePath), "models\\%s", pMDLName );
		SelectMDL( pRelativePath );
	}
	else
	{
		SelectMDL( NULL );
	}
}


//-----------------------------------------------------------------------------
// Allows external apps to select a MDL
//-----------------------------------------------------------------------------
void CMDLPicker::SelectMDL( const char *pRelativePath )
{
	MDLHandle_t hSelectedMDL = pRelativePath ? vgui::MDLCache()->FindMDL( pRelativePath ) : MDLHANDLE_INVALID;
	 
	// We didn't change models after all...
	if ( hSelectedMDL == m_hSelectedMDL )
	{
		// vgui::MDLCache()->FindMDL adds a reference by default we don't use, release it again
		if ( hSelectedMDL != MDLHANDLE_INVALID )
		{
			vgui::MDLCache()->Release( hSelectedMDL );
		}
		return;
	}

	m_hSelectedMDL = hSelectedMDL;

	if ( vgui::MDLCache()->IsErrorModel( m_hSelectedMDL ) )
	{
		m_hSelectedMDL = MDLHANDLE_INVALID;
	}
	m_pMDLPreview->SetMDL( m_hSelectedMDL );

	m_pMDLPreview->LookAtMDL();


	if ( m_nFlags & ( PAGE_SKINS ) )
	{
		UpdateSkinsList();
	}

	if ( m_nFlags & ( PAGE_INFO ) )
	{
		UpdateInfoTab();
	}

	if ( m_nFlags & (PAGE_ACTIVITIES|PAGE_SEQUENCES) )
	{
		RefreshActivitiesAndSequencesList();
	}

	// vgui::MDLCache()->FindMDL adds a reference by default we don't use, release it again
	if ( hSelectedMDL != MDLHANDLE_INVALID )
	{
		vgui::MDLCache()->Release( hSelectedMDL );
	}

	PostActionSignal( new KeyValues( "MDLPreviewChanged", "mdl", pRelativePath ? pRelativePath : "" ) );
}


//-----------------------------------------------------------------------------
// Purpose: updates revision view on a file being selected
//-----------------------------------------------------------------------------
void CMDLPicker::OnCheckButtonChecked(KeyValues *kv)
{
//    RefreshMDLList();
	BaseClass::OnCheckButtonChecked( kv );
	RefreshRenderSettings();
}


void CMDLPicker::GetSelectedMDLName( char *pBuffer, int nMaxLen )
{
	Assert( nMaxLen > 0 );
	if ( GetSelectedAssetCount() > 0 )
	{
		Q_snprintf( pBuffer, nMaxLen, "models\\%s", GetSelectedAsset( ) );
	}
	else
	{
		pBuffer[0] = 0;
	}
}
	
//-----------------------------------------------------------------------------
// Gets the selected activity/sequence
//-----------------------------------------------------------------------------
int CMDLPicker::GetSelectedPage( )
{
	if ( m_pSequencesPage && ( m_pViewsSheet->GetActivePage() == m_pSequencesPage ) )
		return PAGE_SEQUENCES;

	if ( m_pActivitiesPage && ( m_pViewsSheet->GetActivePage() == m_pActivitiesPage ) )
		return PAGE_ACTIVITIES;

	return PAGE_NONE;
}

const char *CMDLPicker::GetSelectedSequenceName()
{
	if ( !m_pSequencesPage  )
		return NULL;

	int nIndex = m_pSequencesList->GetSelectedItem( 0 );
	if ( nIndex >= 0 )
	{
		KeyValues *pkv = m_pSequencesList->GetItem( nIndex );
		return pkv->GetString( "sequence", NULL );
	}

	return NULL;
}

const char *CMDLPicker::GetSelectedActivityName()
{
	if ( !m_pActivitiesPage  )
		return NULL;

	int nIndex = m_pActivitiesList->GetSelectedItem( 0 );
	if ( nIndex >= 0 )
	{
		KeyValues *pkv = m_pActivitiesList->GetItem( nIndex );
		return pkv->GetString( "activity", NULL );
	}
	return NULL;
}

int	CMDLPicker::GetSelectedSkin()
{
	if ( !m_pSkinsPage )
		return NULL;

	int nIndex = m_pSkinsList->GetSelectedItem( 0 );
	if ( nIndex >= 0 )
	{
		return nIndex;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Plays the selected activity
//-----------------------------------------------------------------------------
void CMDLPicker::SelectActivity( const char *pActivityName )
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

	PostActionSignal( new KeyValues( "SequenceSelectionChanged", "activity", pActivityName ) );
}


//-----------------------------------------------------------------------------
// Plays the selected sequence
//-----------------------------------------------------------------------------
void CMDLPicker::SelectSequence( const char *pSequenceName )
{
	studiohdr_t *pstudiohdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	for (int i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
		if ( !Q_stricmp( seqdesc.pszLabel(), pSequenceName ) )
		{
			m_pMDLPreview->SetSequence( i, false );
			break;
		}
	}

	PostActionSignal( new KeyValues( "SequenceSelectionChanged", "sequence", pSequenceName ) );
}

void CMDLPicker::SelectSkin( int nSkin )
{
	m_pMDLPreview->SetSkin( nSkin );
	PostActionSignal( new KeyValues( "SkinSelectionChanged", "skin", nSkin));
}

	
//-----------------------------------------------------------------------------
// Purpose: Updates preview when an item is selected
//-----------------------------------------------------------------------------
void CMDLPicker::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr("panel", NULL);
	if ( m_pSequencesList && (pPanel == m_pSequencesList ) )
	{
		const char *pSequenceName = GetSelectedSequenceName();
		if ( pSequenceName )
		{
			SelectSequence( pSequenceName );
		}
		return;
	}

	if ( m_pActivitiesList && ( pPanel == m_pActivitiesList ) )
	{
		const char *pActivityName = GetSelectedActivityName();
		if ( pActivityName )
		{
			SelectActivity( pActivityName );
		}
		return;
	}

	if ( m_pSkinsList && ( pPanel == m_pSkinsList ) )
	{
		int nSelectedSkin = GetSelectedSkin();
		SelectSkin( nSelectedSkin );
	
		return;
	}

	BaseClass::OnItemSelected( kv );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a page is shown
//-----------------------------------------------------------------------------
void CMDLPicker::OnPageChanged( )
{
	if ( m_pSequencesPage && ( m_pViewsSheet->GetActivePage() == m_pSequencesPage ) )
	{
		m_pSequencesList->RequestFocus();

		const char *pSequenceName = GetSelectedSequenceName();

		if ( pSequenceName )
		{
			SelectSequence( pSequenceName );
		}
		return;
	}
	
	if ( m_pActivitiesPage && ( m_pViewsSheet->GetActivePage() == m_pActivitiesPage ) )
	{
		m_pActivitiesList->RequestFocus();

		const char *pActivityName = GetSelectedActivityName();

		if ( pActivityName )
		{
			SelectActivity( pActivityName );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CMDLPickerFrame::CMDLPickerFrame( vgui::Panel *pParent, const char *pTitle, int nFlags ) : 
	BaseClass( pParent )
{
	SetAssetPicker( new CMDLPicker( this, nFlags ) );
	LoadControlSettingsAndUserConfig( "resource/mdlpickerframe.res" );
	SetTitle( pTitle, false );
}

CMDLPickerFrame::~CMDLPickerFrame()
{
}


//-----------------------------------------------------------------------------
// Allows external apps to select a MDL
//-----------------------------------------------------------------------------
void CMDLPickerFrame::SelectMDL( const char *pRelativePath )
{
	static_cast<CMDLPicker*>( GetAssetPicker() )->SelectMDL( pRelativePath );
}

int CMDLPicker::UpdateSkinsList()
{
	int nNumSkins = 0;

	if ( m_pSkinsList )
	{
		m_pSkinsList->RemoveAll();

		studiohdr_t *hdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
		if ( hdr )
		{
			nNumSkins = hdr->numskinfamilies;
			for ( int i = 0; i < nNumSkins; i++ )
			{
				char skinText[25] = "";
				sprintf( skinText, "skin%i", i );
				KeyValues *pkv = new KeyValues("node", "skin", skinText );
				m_pSkinsList->AddItem( pkv, 0, false, false );
			}
		}
	}
		
	return nNumSkins;
}

void CMDLPicker::UpdateInfoTab()
{
	studiohdr_t *hdr = vgui::MDLCache()->GetStudioHdr( m_hSelectedMDL );
	if ( !hdr )
		return;
	
	int nMass = hdr->mass;
	Panel *pTempPanel = m_pInfoPage->FindChildByName("MassValue");
	char massBuff[10];
	Q_snprintf( massBuff, 10, "%d", nMass );
	((vgui::Label *)pTempPanel)->SetText( massBuff );
	bool bIsStatic = ( hdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) ? true : false;
	bool bIsPhysics = false;

	Label *pTempLabel = (Label *)m_pInfoPage->FindChildByName("StaticText");
	pTempLabel->SetVisible( false );

	KeyValues *pkvModelKeys = new KeyValues( "modelkeys" );
	pkvModelKeys->LoadFromBuffer( "modelkeys", hdr->KeyValueText() );

	KeyValues *kvPropData = pkvModelKeys->FindKey( "prop_data" );
	if ( kvPropData )
	{
		int iPropDataCount = UpdatePropDataList( kvPropData, bIsStatic );
		if( iPropDataCount )
		{
			bIsPhysics = true;
		}
	}
	else
	{
		m_pPropDataList->RemoveAll();
	}

	pkvModelKeys->deleteThis();
	
	CheckButton * pTempCheck = (CheckButton *)m_pInfoPage->FindChildByName("StaticObject");
	pTempCheck->SetCheckButtonCheckable( true );
	pTempCheck->SetSelected( bIsStatic );
	pTempCheck->SetCheckButtonCheckable( false );
	pTempCheck = (CheckButton *)m_pInfoPage->FindChildByName("PhysicsObject");
	pTempCheck->SetCheckButtonCheckable( true );
	pTempCheck->SetSelected( bIsPhysics );
	pTempCheck->SetCheckButtonCheckable( false );
	pTempCheck = (CheckButton *)m_pInfoPage->FindChildByName("DynamicObject");
	pTempCheck->SetCheckButtonCheckable( true );
	pTempCheck->SetSelected( !bIsPhysics );
	pTempCheck->SetCheckButtonCheckable( false );
}


int CMDLPicker::UpdatePropDataList( KeyValues *pkvPropData, bool &bIsStatic )
{
	int iCount = 0;  

	if ( m_pPropDataList )
	{
		m_pPropDataList->RemoveAll();

		KeyValues *kvItem = pkvPropData->GetFirstSubKey();
		while ( kvItem )
		{
			if ( kvItem->GetDataType() != KeyValues::TYPE_NONE )
			{
				// Special handling for some keys
				if ( !Q_strcmp( kvItem->GetName(), "allowstatic" ) && !Q_strcmp( kvItem->GetString() , "1" ) )
				{
					if ( !bIsStatic )
					{					
						Label *pTempLabel = (Label *)m_pInfoPage->FindChildByName( "StaticText" );
						pTempLabel->SetVisible( true );
					}
					bIsStatic &= true;
				}

				KeyValues *pkv = new KeyValues("node", "key", kvItem->GetName(), "value", kvItem->GetString() );
				m_pPropDataList->AddItem( pkv, 0, false, false );
				iCount++;
			}
			
			kvItem = kvItem->GetNextKey();
		}
	}
	
	return iCount;
}

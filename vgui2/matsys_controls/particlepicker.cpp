//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "matsys_controls/particlepicker.h"
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
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ScrollableEditablePanel.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_controls/Tooltip.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/Cursor.h"
#include "matsys_controls/assetpicker.h"
#include "dmxloader/dmxloader.h"
#include "particles/particles.h"

#include "vstdlib/jobthread.h"

#include "dme_controls/particlesystempanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------

struct CachedParticleInfo_t
{
	CachedAssetInfo_t m_AssetInfo;
	CUtlString m_FileName;
};

struct PCFToLoad_t
{
	CUtlString m_FileName;
	int m_ModId;
};

static CUtlVector<PCFToLoad_t> sCacheUnloadedPCFs;
static CUtlVector<CachedParticleInfo_t> sCacheParticleList;

//-----------------------------------------------------------------------------

class CParticleSnapshotPanel: public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSnapshotPanel, vgui::EditablePanel );

public:
	CParticleSnapshotPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CParticleSnapshotPanel( );
	MESSAGE_FUNC( OnSetFocus, "SetFocus" );
	MESSAGE_FUNC_CHARPTR( OnParticleSystemSelected, "ParticleSystemSelected", SystemName );

	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);

	virtual void ApplySchemeSettings(IScheme *pScheme);

	virtual void Paint();

	void SetParticleSystem( const char *szSystemName, int nId );
	void Simulate();
	void SetSelected( bool bSelected );
	const char *GetSystemName() const;
	bool IsSelected();
	CParticleCollection* GetSystem();
	int GetId();

	void SetPreviewEnabled( bool bEnabled );

	void UpdateRelatives( IImage *pIcon, CUtlVector<CParticleSnapshotGrid::PSysRelativeInfo_t>& sysParents, CUtlVector<CParticleSnapshotGrid::PSysRelativeInfo_t>& sysChildren );

protected:
	CParticleSystemPanel *m_pParticlePanel;
	MenuButton *m_pParentsButton;
	MenuButton *m_pChildrenButton;
	vgui::Label *m_pLabel;
	int m_nSystemIndex;
	CUtlString m_SystemName;
	bool m_bSelected;
	int m_nSystemId;
	bool m_bPreviewEnabled;

	Color m_SelectedBgColor;
	Color m_SelectedTextColor;
};

CParticleSnapshotPanel::CParticleSnapshotPanel( vgui::Panel *pParent, const char *pName ):
	BaseClass(pParent,pName)
{
	m_bSelected = false;
	m_bPreviewEnabled = true;

	m_pParticlePanel = new CParticleSystemPanel( this, pName );
	m_pParticlePanel->SetSelfSimulation(false);
	m_pParticlePanel->SetGridColor( 128, 128, 128 );
	m_pParticlePanel->RenderGrid(true);
	m_pParticlePanel->SetParentMouseNotify(true);
	m_pParticlePanel->EnableAutoViewing(true);

	m_pLabel = new Label( this, "SystemLabel", "Unnamed" );
	m_pLabel->SetContentAlignment( Label::a_center );
	m_pLabel->SetPaintBackgroundEnabled( false );
	m_pLabel->SetMouseInputEnabled( false );
	
	CBoxSizer *pSizer = new CBoxSizer(ESLD_VERTICAL);
	pSizer->AddPanel( m_pParticlePanel, SizerAddArgs_t().Expand( 1.0f ).Padding( 5 ) );

	{
		CBoxSizer *pRow = new CBoxSizer(ESLD_HORIZONTAL);
		
		m_pParentsButton = new MenuButton( this, "ParentsButton", "P" );
		pRow->AddPanel( m_pParentsButton, SizerAddArgs_t().Expand( 0.0f ).Padding( 0 ) );

		m_pChildrenButton = new MenuButton( this, "ChildrenButton", "C" );
		pRow->AddPanel( m_pChildrenButton, SizerAddArgs_t().Expand( 0.0f ).Padding( 0 ) );

		pRow->AddPanel( m_pLabel, SizerAddArgs_t().Expand( 0.0f ).Padding( 2 ) );

		pSizer->AddSizer( pRow, SizerAddArgs_t().Expand( 0.0f ).Padding( 5 ) );
	}

	pSizer->AddSpacer( SizerAddArgs_t().Padding( 5 ) );
	SetSizer( pSizer );
	InvalidateLayout(true);

	m_pParticlePanel->ResetView();
	m_pParticlePanel->LookAt( 50.f );
}

void CParticleSnapshotPanel::SetPreviewEnabled( bool bEnabled )
{
	m_bPreviewEnabled = bEnabled;
	m_pParticlePanel->SetVisible(m_bPreviewEnabled);
	InvalidateLayout();
}

CParticleSnapshotPanel::~CParticleSnapshotPanel( )
{

}

void CParticleSnapshotPanel::OnParticleSystemSelected( const char *SystemName )
{
	// hand the signal up
	PostActionSignal( new KeyValues( "ParticleSystemSelected", "SystemName", SystemName ) );
}

int CParticleSnapshotPanel::GetId()
{
	return m_nSystemId;
}

CParticleCollection* CParticleSnapshotPanel::GetSystem()
{
	return m_pParticlePanel->GetParticleSystem();
}

void CParticleSnapshotPanel::OnSetFocus()
{
	BaseClass::OnSetFocus();
}

void CParticleSnapshotPanel::UpdateRelatives( IImage *pIcon, CUtlVector<CParticleSnapshotGrid::PSysRelativeInfo_t>& sysParents, CUtlVector<CParticleSnapshotGrid::PSysRelativeInfo_t>& sysChildren )
{
	char countBuf[8];

	// parents
	if ( sysParents.Count() )
	{
		Menu *pParentsMenu = new Menu(GetParent()->GetParent(), "ParentsMenu");

		for ( int i = 0; i < sysParents.Count(); ++i )
		{
			int nItemIdx = pParentsMenu->AddMenuItem( sysParents[i].relName, new KeyValues( "ParticleSystemSelected", "SystemName", sysParents[i].relName ), this );
			pParentsMenu->GetMenuItem(nItemIdx)->SetEnabled( sysParents[i].bVisibleInCurrentView );
		}

		m_pParentsButton->SetEnabled( true );

		V_snprintf( countBuf, sizeof(countBuf), "%d", sysParents.Count() );
		m_pParentsButton->SetText( countBuf );

		m_pParentsButton->SetMenu( pParentsMenu );
	}
	else
	{
		m_pParentsButton->SetEnabled( false );
		m_pParentsButton->SetText( "P" );
		m_pParentsButton->SetMenu( NULL );
	}

	// children
	if ( sysChildren.Count() )
	{
		Menu *pChildrenMenu = new Menu(GetParent()->GetParent(), "ChildrenMenu");

		for ( int i = 0; i < sysChildren.Count(); ++i )
		{
			int nItemIdx = pChildrenMenu->AddMenuItem( sysChildren[i].relName, new KeyValues( "ParticleSystemSelected", "SystemName", sysChildren[i].relName ), this );
			pChildrenMenu->GetMenuItem(nItemIdx)->SetEnabled( sysChildren[i].bVisibleInCurrentView );
		}

		m_pChildrenButton->SetEnabled( true );

		V_snprintf( countBuf, sizeof(countBuf), "%d", sysChildren.Count() );
		m_pChildrenButton->SetText( countBuf );

		m_pChildrenButton->SetMenu( pChildrenMenu );
	}
	else
	{
		m_pChildrenButton->SetEnabled( false );
		m_pChildrenButton->SetText( "C" );
		m_pChildrenButton->SetMenu( NULL );
	}
}

void CParticleSnapshotPanel::OnMousePressed( MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( code == MOUSE_LEFT )
	{
		if ( input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT) )
		{
			PostActionSignal( new KeyValues( "ParticleSystemShiftSelected", "SystemName", m_SystemName ) );
		}
		else if ( input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL) )
		{
			PostActionSignal( new KeyValues( "ParticleSystemCtrlSelected", "SystemName", m_SystemName ) );
		}
		else
		{
			PostActionSignal( new KeyValues( "ParticleSystemSelected", "SystemName", m_SystemName ) );
		}
	}
}

void CParticleSnapshotPanel::OnMouseDoublePressed( MouseCode code )
{
	BaseClass::OnMouseDoublePressed( code );

	if ( code == MOUSE_LEFT )
	{
		PostActionSignal( new KeyValues( "ParticleSystemPicked", "SystemName", m_SystemName ) );
	}
}

void CParticleSnapshotPanel::SetParticleSystem( const char *szSystemName, int nId )
{
	bool bSameSystem = !V_strcmp( m_SystemName, szSystemName );

	m_SystemName = szSystemName;
	m_nSystemId = nId;

	if ( !bSameSystem || m_pParticlePanel->GetParticleSystem() == NULL )
	{
		m_pParticlePanel->SetParticleSystem(szSystemName);
		m_pParticlePanel->SetControlPointValue( 0, Vector(0.f,0.f,0.f) );
		m_pParticlePanel->SetControlPointValue( 1, Vector(100.f,0.f,0.f) );
		m_pParticlePanel->LookAt( 50.f );

		m_pLabel->SetText( m_SystemName );

		m_pParticlePanel->ResetView();
	}

	if ( !m_pParticlePanel->GetParticleSystem() )
	{
		CUtlString errStr = "ERROR: ";
		errStr += m_SystemName;
		m_pLabel->SetText( errStr );
		m_pParticlePanel->SetBackgroundColor( 64,0,0 );
	}
	else
	{
		m_pParticlePanel->SetBackgroundColor( 32, 32, 32 );
	}

	InvalidateLayout();
}

void CParticleSnapshotPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_SelectedBgColor = pScheme->GetColor( "ListPanel.SelectedBgColor", Color(255, 255, 255, 0) );
	m_SelectedTextColor = pScheme->GetColor( "ListPanel.SelectedTextColor", Color(255, 255, 255, 0) );

	// override the colors because alpha = 0 is bad
	Color btnFgColor = pScheme->GetColor("Button.TextColor", Color(255, 255, 255, 255) );
	Color btnBgColor = pScheme->GetColor("ListPanel.BgColor", Color(128, 128, 128, 255) );

	m_pChildrenButton->SetDefaultColor( btnFgColor, btnBgColor );
	m_pChildrenButton->SetArmedColor( btnFgColor, btnBgColor );
	m_pChildrenButton->SetDepressedColor( btnFgColor, btnBgColor );

	m_pParentsButton->SetDefaultColor( btnFgColor, btnBgColor );
	m_pParentsButton->SetArmedColor( btnFgColor, btnBgColor );
	m_pParentsButton->SetDepressedColor( btnFgColor, btnBgColor );
}

void CParticleSnapshotPanel::Simulate()
{
	if( !m_bPreviewEnabled )
	{
		return;
	}

	Panel* pParent = GetParent();

	int x0,y0,x1, y1;
	GetClipRect( x0, y0, x1, y1 );

	int nPX0, nPY0, nPX1, nPY1;
	pParent->GetClipRect( nPX0, nPY0, nPX1, nPY1 );

	if( x1 < nPX0 || x0 > nPX1 ||
		y1 < nPY0 || y0 > nPY1 )
	{
		// out of bounds
		return;
	}

	m_pParticlePanel->Simulate();
}

void CParticleSnapshotPanel::Paint()
{
	BaseClass::Paint();

	if( m_bSelected )
	{
		DrawBox( 1, 1, GetWide()-2, GetTall()-2, m_SelectedBgColor, 1.0f );
	}

	if( m_bPreviewEnabled )
	{
		// black border around the preview
		int x, y, w, t;
		m_pParticlePanel->GetBounds( x, y, w, t );
		surface()->DrawSetColor( Color(0,0,0,255) );
		surface()->DrawOutlinedRect( x-1, y-1, x+w+1, y+t+1 );
	}
}

const char *CParticleSnapshotPanel::GetSystemName() const
{
	return m_SystemName;
}

void CParticleSnapshotPanel::SetSelected( bool bSelected )
{
	m_bSelected = bSelected;
}

bool CParticleSnapshotPanel::IsSelected( )
{
	return m_bSelected;
}




//-----------------------------------------------------------------------------
//
// Particle Snapshot Grid
//
//-----------------------------------------------------------------------------

const int MIN_PANEL_SIZE = 200;
const int MAX_PANEL_SIZE = 250;
const int MIN_NUM_COLS = 1;
const int OFFSET = 4;
const int TOOLBAR_HEIGHT = 30;

CParticleSnapshotGrid::CParticleSnapshotGrid( vgui::Panel *pParent, const char *pName ):
	BaseClass(pParent,pName)
{
	m_pRelativesImgNeither = scheme()->GetImage( "tools/particles/icon_particles_rel_neither", false );
	m_pRelativesImgPOnly = scheme()->GetImage( "tools/particles/icon_particles_rel_ponly", false );
	m_pRelativesImgCOnly = scheme()->GetImage( "tools/particles/icon_particles_rel_conly", false );
	m_pRelativesImgBoth = scheme()->GetImage( "tools/particles/icon_particles_rel_both", false );

	m_pScrollBar = new ScrollBar( this, "ScrollBar", true );
	m_pScrollBar->SetWide( 16 );
	m_pScrollBar->SetAutoResize( PIN_TOPRIGHT, AUTORESIZE_DOWN, 0, TOOLBAR_HEIGHT, -16, 0 );
	m_pScrollBar->AddActionSignalTarget( this );

	m_pScrollPanel = new Panel( this, "ScrollPanel" );
	m_pScrollPanel->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, TOOLBAR_HEIGHT, -16, 0 );
	m_pScrollPanel->DisableMouseInputForThisPanel(true);

	m_pToolPanel = new Panel( this, "ToolPanel" );
	m_pToolPanel->SetTall(TOOLBAR_HEIGHT);
	m_pToolPanel->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_RIGHT, 0, 0, -16, 0 );

	m_pToolPanel->SetSizer( new CBoxSizer(ESLD_HORIZONTAL) );
	m_pPreviewCheckbox = new CheckButton( m_pToolPanel, "PreviewCheckbox", "Show Previews" );
	m_pPreviewCheckbox->SetSelected(true);
	m_pPreviewCheckbox->AddActionSignalTarget( this );
	m_pToolPanel->GetSizer()->AddPanel( m_pPreviewCheckbox, SizerAddArgs_t() );

	m_pNoSystemsLabel = new Label( m_pScrollPanel, "NoSystemsLabel", "<No Systems>" );

	m_nCurrentColCount = MIN_NUM_COLS;
	m_nMostRecentSelectedIndex = -1;

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	SetKeyBoardInputEnabled(true);
}

void CParticleSnapshotGrid::MapSystemRelatives( )
{
	// somewhat slow, but doesn't get called frequently or with large enough data sets to optimize

	m_ParentsMap.RemoveAll();
	m_ParentsMap.AddMultipleToTail( m_Panels.Count() );
	m_ChildrenMap.RemoveAll();
	m_ChildrenMap.AddMultipleToTail( m_Panels.Count() );

	for ( int i = 0; i < g_pParticleSystemMgr->GetParticleSystemCount(); i++ )
	{
		const char *pPotentialParentName = g_pParticleSystemMgr->GetParticleSystemNameFromIndex(i);
		CParticleSystemDefinition *pPotentialParent = g_pParticleSystemMgr->FindParticleSystem( pPotentialParentName );

		//////////////////////
		// see if this system is visible
		int nParentVisibileIndex = -1;
		for ( int p = 0; p < m_Panels.Count(); ++p )
		{
			CParticleCollection *pVisible = m_Panels[p]->GetSystem();

			if( pVisible == NULL )
				continue;

			if ( pVisible->m_pDef == pPotentialParent )
			{
				nParentVisibileIndex = p;
				break;
			}
		}

		//////////////////////
		// see if this system is a parent of any visible system; check all its children
		for ( int c = 0; c < pPotentialParent->m_Children.Count(); ++c )
		{
			CParticleSystemDefinition *pChild = NULL;

			if ( pPotentialParent->m_Children[c].m_bUseNameBasedLookup )
			{
				pChild = g_pParticleSystemMgr->FindParticleSystem( pPotentialParent->m_Children[c].m_Name );
			}
			else
			{
				pChild = g_pParticleSystemMgr->FindParticleSystem( pPotentialParent->m_Children[c].m_Id );
			}

			if ( pChild == NULL )
				continue;

			//////////////////////
			// is the child visible?
			int nChildVisibleIndex = -1;

			for ( int p = 0; p < m_Panels.Count(); ++p )
			{
				CParticleCollection *pVisible = m_Panels[p]->GetSystem();

				if( pVisible == NULL )
					continue;

				if ( pVisible->m_pDef == pChild )
				{
					// this child is visible; add entry, and mark parent if visible
					PSysRelativeInfo_t parentInfo;
					parentInfo.relName = pPotentialParent->GetName();
					parentInfo.bVisibleInCurrentView = ( nParentVisibileIndex != -1 );
					m_ParentsMap[p].AddToTail( parentInfo );

					nChildVisibleIndex = p;
					break;
				}
			}

			if ( nParentVisibileIndex != -1 )
			{
				// this parent is visible; add entry, and mark child if visible
				PSysRelativeInfo_t childInfo;
				childInfo.relName = pChild->GetName();
				childInfo.bVisibleInCurrentView = ( nChildVisibleIndex != -1 );
				m_ChildrenMap[nParentVisibileIndex].AddToTail( childInfo );
			}
		}
	}
}


void CParticleSnapshotGrid::UpdatePanelRelatives( int nIndex )
{
	CParticleSnapshotPanel* pPanel = m_Panels[nIndex];
	CUtlVector<PSysRelativeInfo_t>& sysParents = m_ParentsMap[nIndex];
	CUtlVector<PSysRelativeInfo_t>& sysChildren = m_ChildrenMap[nIndex];

	IImage *pImg = m_pRelativesImgNeither;

	if ( sysParents.Count() && sysChildren.Count() )
	{
		pImg = m_pRelativesImgBoth;
	}
	else if ( sysParents.Count() )
	{
		pImg = m_pRelativesImgPOnly;
	}
	else if ( sysChildren.Count() )
	{
		pImg = m_pRelativesImgCOnly;
	}

	pPanel->UpdateRelatives( pImg, sysParents, sysChildren );
}

void CParticleSnapshotGrid::OnScrollBarSliderMoved()
{
	LayoutScrolled();
}

void CParticleSnapshotGrid::OnCheckButtonChecked( KeyValues *kv )
{
	SetAllPreviewEnabled( m_pPreviewCheckbox->IsSelected() );
}

void CParticleSnapshotGrid::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CParticleSnapshotGrid::UpdateAllRelatives()
{
	MapSystemRelatives();

	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		UpdatePanelRelatives( i );
	}
}

void CParticleSnapshotGrid::OnMousePressed(MouseCode code)
{
	BaseClass::OnMousePressed( code );

	SelectSystem("",false,false);
	PostActionSignal( new KeyValues( "ParticleSystemSelectionChanged" ) );
}

static int PanelSortHelperI( CParticleSnapshotPanel *const *a, CParticleSnapshotPanel * const *b )
{
	return V_stricmp((*a)->GetSystemName(),(*b)->GetSystemName());
}

void CParticleSnapshotGrid::SetParticleList( const CUtlVector<const char *>& ParticleNames )
{
	bool bPreviewEnabled = m_pPreviewCheckbox->IsSelected();

	// might have too many panels
	while ( m_Panels.Count() > ParticleNames.Count() )
	{
		delete m_Panels.Tail();
		m_Panels.RemoveMultipleFromTail(1);
	}

	// might have too few panels
	while ( m_Panels.Count() < ParticleNames.Count() )
	{
		char szPanelName[32];
		V_snprintf( szPanelName, sizeof(szPanelName), "ParticlePanel%d", m_Panels.Count() );
		CParticleSnapshotPanel *pPanel = new CParticleSnapshotPanel( m_pScrollPanel, szPanelName );
		pPanel->SetPreviewEnabled(bPreviewEnabled);
		pPanel->AddActionSignalTarget(this);

		m_Panels.AddToTail( pPanel );
	}

	// reinit them all, with the new system names
	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		if( i < ParticleNames.Count() )
		{
			m_Panels[i]->SetParticleSystem( ParticleNames[i], i );
		}
		else
		{
			m_Panels[i]->SetParticleSystem( "", i );
		}

		m_Panels[i]->SetSelected(false);
	}

	// now sort the panels however we want
	m_Panels.Sort( PanelSortHelperI );

	UpdateAllRelatives();

	m_nMostRecentSelectedIndex = -1;

	InvalidateLayout();
}

int CParticleSnapshotGrid::GetPanelWide()
{
	int nWide = GetWide() - OFFSET*2 - m_pScrollBar->GetWide();
	return MIN(MAX_PANEL_SIZE,nWide/m_nCurrentColCount);
}

int CParticleSnapshotGrid::GetPanelTall()
{
	if ( m_pPreviewCheckbox->IsSelected() )
	{
		return GetPanelWide();
	}
	else
	{
		return 30; // TODO: This shouldn't be hard-coded :|
	}
}

void CParticleSnapshotGrid::PerformLayout()
{
	BaseClass::PerformLayout();

	int nWide = GetWide() - OFFSET*2 - m_pScrollBar->GetWide();
	m_nCurrentColCount = MAX(MIN_NUM_COLS,nWide/MIN_PANEL_SIZE);

	LayoutScrolled();
}

void CParticleSnapshotGrid::SetAllPreviewEnabled( bool bEnabled )
{
	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		m_Panels[i]->SetPreviewEnabled(bEnabled);
	}

	InvalidateLayout(true);
	LayoutScrolled();
}

void CParticleSnapshotGrid::LayoutScrolled()
{
//	int nWide = GetWide() - m_pScrollBar->GetWide();
	int nPanels = m_Panels.Count();
	int nRows = (nPanels+m_nCurrentColCount-1)/m_nCurrentColCount; // panels/cols rounded up
//	int nRemainder = (nWide - m_nCurrentColCount*m_nCurrentPanelSize - OFFSET*2);
	int nScrollPos = m_pScrollBar->GetValue();

	int nPanelW = GetPanelWide();
	int nPanelT = GetPanelTall();

	m_pScrollBar->SetRange( 0, nRows*nPanelT+OFFSET*2 );
	m_pScrollBar->SetRangeWindow( m_pScrollPanel->GetTall() );

	for( int r = 0; r < nRows; ++r )
	{
		for( int c = 0; c < m_nCurrentColCount; ++c )
		{
			int i = c + r*m_nCurrentColCount;
			if( i >= nPanels ) continue;

			if( IsSystemVisible( i ) )
			{
				m_Panels[i]->SetSize(nPanelW,nPanelT);
				m_Panels[i]->SetPos(OFFSET+c*nPanelW,OFFSET-nScrollPos+r*nPanelT);
				m_Panels[i]->SetVisible(true);
			}
			else
			{
				m_Panels[i]->SetSize(1,1);
				m_Panels[i]->SetPos(-10,-10);
				m_Panels[i]->SetVisible(false);
			}
		}
	}

	if ( nPanels == 0 )
	{
		m_pNoSystemsLabel->SetVisible( true );

		int w,t;
		m_pNoSystemsLabel->GetContentSize(w,t);

		w *= 2;
		m_pNoSystemsLabel->SetWide( w );
		m_pNoSystemsLabel->SetPos( (m_pScrollPanel->GetWide()-w)/2, (m_pScrollPanel->GetTall()-t)/2 );
	}
	else
	{
		m_pNoSystemsLabel->SetVisible( false );
	}
}

void CParticleSnapshotGrid::OnMouseWheeled(int delta)
{
	int val = m_pScrollBar->GetValue();
	val -= (delta * 30);
	m_pScrollBar->SetValue(val);
	RequestFocus();
}

bool CParticleSnapshotGrid::IsSystemVisible( int nIndex )
{
	int nViewTall = m_pScrollPanel->GetTall();
	int nRow = (nIndex / m_nCurrentColCount);
	int nTopOfPanel = nRow * GetPanelTall();
	int nBottomOfPanel = nTopOfPanel + GetPanelTall();
	int nCurrentScrollValue = m_pScrollBar->GetValue();

	return ( nTopOfPanel < nCurrentScrollValue+nViewTall &&
			 nBottomOfPanel > nCurrentScrollValue );
}

void CParticleSnapshotGrid::SelectIndex( int nIndex, bool bAddToSelection, bool bToggle )
{
	// not the most efficient, but easy
	SelectSystem( m_Panels[nIndex]->GetSystemName(), bAddToSelection, bToggle );
}

void CParticleSnapshotGrid::SelectId( int nId, bool bAddToSelection, bool bToggle )
{
	// not the most efficient, but easy
	SelectSystem( GetSystemName(nId), bAddToSelection, bToggle );
}

void CParticleSnapshotGrid::DeselectAll()
{
	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		m_Panels[i]->SetSelected(false);
	}

	m_nMostRecentSelectedIndex = -1;
}

int CParticleSnapshotGrid::GetSelectedSystemCount()
{
	int nSelected = 0;

	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		if( m_Panels[i]->IsSelected() )
		{
			nSelected++;
		}
	}

	return nSelected;
}

const char *CParticleSnapshotGrid::GetSystemName( int nSystemId )
{
	int nInternalIndex = IdToIndex( nSystemId );

	if ( nInternalIndex < 0 || nInternalIndex >= m_Panels.Count() )
	{
		return "";
	}
	else
	{
		return m_Panels[nInternalIndex]->GetSystemName();
	}
}

int CParticleSnapshotGrid::GetSelectedSystemId( int nSelectionIndex )
{
	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		if( m_Panels[i]->IsSelected() )
		{
			nSelectionIndex--;

			if( nSelectionIndex < 0 )
			{
				return m_Panels[i]->GetId();
			}
		}
	}

	return -1;
}

void CParticleSnapshotGrid::SelectSystem( const char *pSystemName, bool bAddToSelection, bool bToggle )
{
	int nSelectedIndex = -1;

	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		if ( !V_strcmp(m_Panels[i]->GetSystemName(),pSystemName) )
		{
			if ( m_Panels[i]->IsSelected() )
			{
				if ( bAddToSelection && bToggle )
				{
					m_Panels[i]->SetSelected( false );
				}
			}
			else
			{
				nSelectedIndex = i;
				m_Panels[i]->SetSelected( true );
				m_Panels[i]->RequestFocus();
			}

			m_nMostRecentSelectedIndex = i;
		}
		else if( !bAddToSelection )
		{
			m_Panels[i]->SetSelected(false);
		}
	}

	if ( nSelectedIndex != -1 )
	{
		int nViewTall = m_pScrollPanel->GetTall();
		int nRow = (nSelectedIndex / m_nCurrentColCount);
		int nTopOfPanel = nRow * GetPanelTall() + OFFSET;
		int nBottomOfPanel = nTopOfPanel + GetPanelTall() + OFFSET;
		int nCurrentScrollValue = m_pScrollBar->GetValue();

		if( nTopOfPanel < nCurrentScrollValue )
		{
			m_pScrollBar->SetValue(nTopOfPanel);
		}
		else if( nBottomOfPanel > nCurrentScrollValue+nViewTall )
		{
			m_pScrollBar->SetValue(nBottomOfPanel-nViewTall);
		}
	}

	Repaint();
}
/*
static void ProcessPSystem( CParticleSnapshotPanel *&pPanel )
{
	pPanel->Simulate();
}
*/
void CParticleSnapshotGrid::OnTick()
{
//	ParallelProcess( m_Panels.Base(), m_Panels.Count(), ProcessPSystem );

	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		m_Panels[i]->Simulate();
	}
}

void CParticleSnapshotGrid::OnParticleSystemSelected( const char *SystemName )
{
	SelectSystem( SystemName, false, false );
	PostActionSignal( new KeyValues( "ParticleSystemSelectionChanged" ) );
}

void CParticleSnapshotGrid::OnParticleSystemCtrlSelected( const char *SystemName )
{
	SelectSystem( SystemName, true, true );
	PostActionSignal( new KeyValues( "ParticleSystemSelectionChanged" ) );
}

void CParticleSnapshotGrid::OnParticleSystemShiftSelected( const char *SystemName )
{
	int nIdx = InternalFindSystemIndexByName( SystemName );

	if ( nIdx != -1 && m_nMostRecentSelectedIndex != -1 )
	{
		int nFrom = MIN( nIdx, m_nMostRecentSelectedIndex );
		int nTo = MAX( nIdx, m_nMostRecentSelectedIndex );

		for ( int i = nFrom; i <= nTo; ++i )
		{
			SelectIndex( i, true, false );
		}

		PostActionSignal( new KeyValues( "ParticleSystemSelectionChanged" ) );
	}
}

int CParticleSnapshotGrid::InternalFindSystemIndexByName( const char *pSystemName )
{
	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		if ( !V_strcmp(m_Panels[i]->GetSystemName(),pSystemName) )
		{
			return i;
		}
	}

	return -1;
}

void CParticleSnapshotGrid::OnParticleSystemPicked( const char *SystemName )
{
	PostActionSignal( new KeyValues( "ParticleSystemPicked", "SystemName", SystemName ) );
}

int CParticleSnapshotGrid::IdToIndex( int nId )
{
	for ( int i = 0; i < m_Panels.Count(); ++i )
	{
		if ( m_Panels[i]->GetId() == nId )
		{
			return i;
		}
	}

	return -1;
}

void CParticleSnapshotGrid::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_UP ||
		 code == KEY_DOWN ||
		 code == KEY_LEFT ||
		 code == KEY_RIGHT ) 
	{
		if ( m_nMostRecentSelectedIndex != -1 )
		{
			int nNextIndex = m_nMostRecentSelectedIndex;

			if ( code == KEY_UP )
			{
				nNextIndex -= m_nCurrentColCount;
			}
			else if ( code == KEY_DOWN )
			{
				nNextIndex += m_nCurrentColCount;
			}
			else if ( code == KEY_LEFT )
			{
				nNextIndex--;
			}
			else if ( code == KEY_RIGHT )
			{
				nNextIndex++;
			}

			int nPanels = m_Panels.Count();
			int nCurrentRowCount = (nPanels+m_nCurrentColCount-1)/m_nCurrentColCount;

			if ( nNextIndex/m_nCurrentColCount >= nCurrentRowCount-1 &&
				 nNextIndex >= nPanels )
			{
				// if you're not on the last row and hit down,
				// but nothing is below you, still pop to the last
				// item in the list
				nNextIndex = nPanels-1;
			}

			if( nNextIndex >= 0 && nNextIndex < nPanels )
			{
				SelectIndex( nNextIndex, false, false );
				PostActionSignal( new KeyValues( "ParticleSystemSelectionChanged" ) );
				RequestFocus();
			}
		}
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

//-----------------------------------------------------------------------------
//
// Particle Picker
//
//-----------------------------------------------------------------------------

static int StringSortHelperI( const char * const *a, const char * const *b )
{
	return V_stricmp(*a,*b);
}

void CParticlePicker::OnAssetListChanged( )
{
	CUtlVector<const char*> assetNames;

	int nCount = GetAssetCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsAssetVisible( i ) )
		{
			assetNames.AddToTail( GetAssetName(i) );
		}
	}

	assetNames.Sort( StringSortHelperI );

	m_pSnapshotGrid->SetParticleList( assetNames );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CParticlePicker::CParticlePicker( vgui::Panel *pParent ) : 
	BaseClass( pParent, "Particle Systems", "pcf", "particles", "pcfName" )
{
	m_pFileBrowserSplitter = new Splitter( this, "FileBrowserSplitter", SPLITTER_MODE_VERTICAL, 1 );

	float flFractions[] = { 0.33f, 0.67f };

	m_pFileBrowserSplitter->RespaceSplitters( flFractions );

	vgui::Panel *pSplitterLeftSide = m_pFileBrowserSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pFileBrowserSplitter->GetChild( 1 );

	pSplitterLeftSide->RequestFocus();
	CreateStandardControls( pSplitterLeftSide, false );
	AutoLayoutStandardControls();

	m_pSnapshotGrid = new CParticleSnapshotGrid( pSplitterRightSide, "ParticleSystemPanel" );
	m_pSnapshotGrid->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	m_pSnapshotGrid->AddActionSignalTarget(this);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CParticlePicker::~CParticlePicker()
{
}


//-----------------------------------------------------------------------------
// Performs layout
//-----------------------------------------------------------------------------
void CParticlePicker::PerformLayout()
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
void CParticlePicker::OnAssetSelected( KeyValues *pParams )
{
	
}

//-----------------------------------------------------------------------------
// A particle system was selected
//-----------------------------------------------------------------------------
void CParticlePicker::OnSelectedAssetPicked( const char *pParticleSysName )
{
	SelectParticleSys( pParticleSysName );

	m_pSnapshotGrid->SelectSystem( pParticleSysName, false, false );
}


void CParticlePicker::OnMousePressed(MouseCode code)
{
	SelectParticleSys( "" );
	m_pSnapshotGrid->SelectSystem( "", false, false );
}

//-----------------------------------------------------------------------------
// Allows external apps to select a particle system
//-----------------------------------------------------------------------------
void CParticlePicker::SelectParticleSys( const char *pRelativePath )
{
	PostActionSignal( new KeyValues( "SelectedParticleSysChanged", "particle", pRelativePath ? pRelativePath : "" ) );
}

void CParticlePicker::GetSelectedParticleSysName( char *pBuffer, int nMaxLen )
{
	Assert( nMaxLen > 0 );

	if ( GetSelectedAssetCount() > 0 )
	{
		Q_snprintf( pBuffer, nMaxLen, "%s", GetSelectedAsset() );
	}
	else
	{
		pBuffer[0] = 0;
	}
}
	
void CParticlePicker::OnParticleSystemSelectionChanged()
{
	const char *SystemName = m_pSnapshotGrid->GetSystemName( m_pSnapshotGrid->GetSelectedSystemId(0) );
	SetSelection( SystemName );
}

void CParticlePicker::OnParticleSystemPicked( const char *SystemName )
{
	OnKeyCodeTyped(KEY_ENTER);
}

int CParticlePicker::GetAssetCount()
{
	return sCacheParticleList.Count();
}

const char *CParticlePicker::GetAssetName( int nAssetIndex )
{
	return sCacheParticleList[nAssetIndex].m_AssetInfo.m_AssetName;
}

const CachedAssetInfo_t& CParticlePicker::GetCachedAsset( int nAssetIndex )
{
	return sCacheParticleList[nAssetIndex].m_AssetInfo;
}

int CParticlePicker::GetCachedAssetCount()
{
	return sCacheParticleList.Count();
}

void CParticlePicker::CachePCFInfo( int nModIndex, const char *pFileName  )
{
	const CacheModInfo_t& modInfo = ModInfo(nModIndex);

	if ( pFileName[0] == '!' )
	{
		++pFileName;
	}

	CUtlBuffer buf;

	CUtlString pcfPath = modInfo.m_Path;
	pcfPath += "/";
	pcfPath += pFileName;

	// cache the particles into the manager
	g_pParticleSystemMgr->ReadParticleConfigFile( pcfPath, true );

	// and read it ourselves to see what came out
	if ( !g_pFullFileSystem->ReadFile( pcfPath, modInfo.m_Path, buf ) )
	{
		return;
	}

	DECLARE_DMX_CONTEXT_DECOMMIT( true );

	CDmxElement *pRoot;
	if ( !UnserializeDMX( buf, &pRoot, pFileName ) )
	{
		return;
	}

	if ( !Q_stricmp( pRoot->GetTypeString(), "DmeParticleSystemDefinition" ) )
	{
		CachedParticleInfo_t &info = sCacheParticleList[sCacheParticleList.AddToTail()];
		info.m_AssetInfo.m_AssetName = pRoot->GetName();
		info.m_AssetInfo.m_nModIndex = nModIndex;
		info.m_FileName = pFileName;
	
		return;
	}

	const CDmxAttribute *pDefinitions = pRoot->GetAttribute( "particleSystemDefinitions" );
	if ( !pDefinitions || pDefinitions->GetType() != AT_ELEMENT_ARRAY )
	{
		CleanupDMX( pRoot );
		return;
	}

	const CUtlVector< CDmxElement* >& definitions = pDefinitions->GetArray<CDmxElement*>( );
	int nCount = definitions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CachedParticleInfo_t &info = sCacheParticleList[sCacheParticleList.AddToTail()];
		info.m_AssetInfo.m_AssetName = definitions[i]->GetName();
		info.m_AssetInfo.m_nModIndex = nModIndex;
		info.m_FileName = pFileName;
	}

	CleanupDMX( pRoot );
}

void CParticlePicker::HandleModParticles( int nModIndex )
{
	const CacheModInfo_t &modInfo = ModInfo(nModIndex);

	CUtlString manifestPath = modInfo.m_Path;
	manifestPath += "/particles/particles_manifest.txt" ;

	CUtlVector<CUtlString> pcfList;
	GetParticleManifest( pcfList, manifestPath );

	int nCount = pcfList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		PCFToLoad_t &p = sCacheUnloadedPCFs[sCacheUnloadedPCFs.AddToTail()];
		p.m_FileName = pcfList[i];
		p.m_ModId = nModIndex;
	}
}

bool CParticlePicker::BeginCacheAssets( bool bForceRecache )
{
	if ( bForceRecache )
	{
		sCacheParticleList.RemoveAll();
		sCacheUnloadedPCFs.RemoveAll();
	}

	if ( sCacheParticleList.Count() )
	{
		return true;
	}

	int nCount = ModCount();
	for ( int i = 0; i < nCount; ++i )
	{
		HandleModParticles( i );
	}

	return false;
}


bool CParticlePicker::IncrementalCacheAssets( float flTimeAllowed )
{
	float flStartTime = Plat_FloatTime();

	while ( sCacheUnloadedPCFs.Count() )
	{
		PCFToLoad_t &p = sCacheUnloadedPCFs.Tail();

		CachePCFInfo( p.m_ModId, p.m_FileName );

		sCacheUnloadedPCFs.RemoveMultipleFromTail(1);

		// might have run out of time
		if ( Plat_FloatTime() - flStartTime >= flTimeAllowed )
			break;
	}

	return (sCacheUnloadedPCFs.Count() == 0);
}

CUtlString CParticlePicker::GetSelectedAssetFullPath( int nSelectionIndex )
{
	int nAssetIndex = GetSelectedAssetIndex( nSelectionIndex );

	if( nAssetIndex < 0 || nAssetIndex >= sCacheParticleList.Count() )
	{
		return CUtlString("ERROR");
	}

	CachedParticleInfo_t &p = sCacheParticleList[nAssetIndex];
	const CacheModInfo_t &m = ModInfo( p.m_AssetInfo.m_nModIndex );

	char pBuf[MAX_PATH+128];
	Q_snprintf( pBuf, sizeof(pBuf), "%s\\%s::%s", 
		m.m_Path.Get(), p.m_FileName.Get(), p.m_AssetInfo.m_AssetName.Get() );
	Q_FixSlashes( pBuf );

	return CUtlString(pBuf);
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CParticlePickerFrame::CParticlePickerFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent )
{
	SetAssetPicker( new CParticlePicker( this ) );
	LoadControlSettingsAndUserConfig( "resource/mdlpickerframe.res" );
	SetTitle( pTitle, false );
}

CParticlePickerFrame::~CParticlePickerFrame()
{
}

void CParticlePickerFrame::SelectParticleSys( const char *pRelativePath )
{
	static_cast<CParticlePicker*>( GetAssetPicker() )->SelectParticleSys( pRelativePath );
}

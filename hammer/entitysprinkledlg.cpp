//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the entity/prefab placement tool.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "entitysprinkledlg.h"
#include "mapdoc.h"
#include "KeyValues.h"
#include "toolmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// CEntitySprinkleDlg dialog

IMPLEMENT_DYNAMIC(CEntitySprinkleDlg, CPropertyPage)


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CEntitySprinkleDlg::CEntitySprinkleDlg()
	: CDialog(CEntitySprinkleDlg::IDD)
{

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CEntitySprinkleDlg::~CEntitySprinkleDlg()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SPRINKLE_MODE, m_SprinkleModeControl);
	DDX_Control(pDX, IDC_SPRINKLE_DENSITY, m_SprinkleDensityControl);
	DDX_Control(pDX, IDC_SPRINKLE_GRID_OFFSET_X, m_GridOffsetXControl);
	DDX_Control(pDX, IDC_SPRINKLE_GRID_OFFSET_Y, m_GridOffsetYControl);
	DDX_Control(pDX, IDC_SPRINKLE_GRID_SIZE_X, m_GridSizeXControl);
	DDX_Control(pDX, IDC_SPRINKLE_GRID_SIZE_Y, m_GridSizeYControl);
	DDX_Control(pDX, IDC_SPRINKLE_TYPE, m_SprinkleTypeControl);
	DDX_Control(pDX, IDC_SPRINKLE_DENSITY_DISPLAY, m_SprinkleDensityDisplayControl);
	DDX_Control(pDX, IDC_SPRINKLE_DEFINITION_GRID_SIZE, m_DefinitionGridSizeControl);
	DDX_Control(pDX, IDC_SPRINKLE_RANDOM_YAW, m_RandomYawControl);
}


BEGIN_MESSAGE_MAP(CEntitySprinkleDlg, CDialog)
	ON_CBN_SELCHANGE(IDC_SPRINKLE_MODE, &CEntitySprinkleDlg::OnCbnSelchangeSprinkleMode)
	ON_BN_CLICKED(IDC_SPRINKLE_USE_GRID, &CEntitySprinkleDlg::OnBnClickedSprinkleUseGrid)
	ON_BN_CLICKED(IDC_SPRINKLE_DEFINITION_GRID_SIZE, &CEntitySprinkleDlg::OnBnClickedSprinkleDefinitionGridSize)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SPRINKLE_DENSITY, &CEntitySprinkleDlg::OnNMCustomdrawSprinkleDensity)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::OnCbnSelchangeSprinkleMode()
{
	// TODO: Add your control notification handler code here
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CEntitySprinkleDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_SprinkleModeControl.AddString( "Additive" );
	m_SprinkleModeControl.AddString( "Subtractive" );
	m_SprinkleModeControl.AddString( "Replace" );
	m_SprinkleModeControl.AddString( "Overwrite" );
	m_SprinkleModeControl.SetCurSel( 0 );

	m_SprinkleDensityControl.SetRange( 0, 100 );
	m_SprinkleDensityControl.SetTicFreq( 10 );
	m_SprinkleDensityControl.SetLineSize( 10 );
	m_SprinkleDensityControl.SetPageSize( 10 );
	m_SprinkleDensityControl.SetPos( 25 );

	m_DefinitionGridSizeControl.SetCheck( BST_CHECKED );

	OnBnClickedSprinkleUseGrid();
	OnBnClickedSprinkleDefinitionGridSize();

	return TRUE;  // return TRUE unless you set the focus to a control
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::OnBnClickedSprinkleUseGrid()
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	
	if ( !pDoc )
	{
		return;
	}
	
	char temp[ 128 ];

	sprintf( temp, "%d", pDoc->GetGridSpacing() );

	m_GridSizeXControl.SetWindowText( temp );
	m_GridSizeYControl.SetWindowText( temp );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::SetSprinkleTypes( KeyValues *pSprinkleInfo )
{
	int nSelection = m_SprinkleTypeControl.GetCurSel();
	int nCount = m_SprinkleTypeControl.GetCount();

	m_SprinkleTypeControl.ResetContent();

	for ( KeyValues *pSub = pSprinkleInfo->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
	{
		int nIndex = m_SprinkleTypeControl.AddString( pSub->GetName() );
		m_SprinkleTypeControl.SetItemDataPtr( nIndex, pSub );
	}

	if ( nCount == 0 )
	{
		m_SprinkleTypeControl.SetCurSel( 0 );
	}
	else
	{
		m_SprinkleTypeControl.SetCurSel( nSelection );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::GetGridSize( float &flGridXSize, float &flGridYSize )
{
	CString		Text;

	m_GridSizeXControl.GetWindowText( Text );
	flGridXSize = atof( Text );
	m_GridSizeYControl.GetWindowText( Text );
	flGridYSize = atof( Text );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
KeyValues *CEntitySprinkleDlg::GetSprinkleType( )
{
	int nIndex = m_SprinkleTypeControl.GetCurSel();
	
	if ( nIndex == CB_ERR )
	{
		return NULL;
	}

	return ( KeyValues * )( m_SprinkleTypeControl.GetItemDataPtr( nIndex ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
int CEntitySprinkleDlg::GetSprinkleMode( )
{
	return m_SprinkleModeControl.GetCurSel();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
int CEntitySprinkleDlg::GetSprinkleDensity( )
{
	return m_SprinkleDensityControl.GetPos();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::OnBnClickedSprinkleDefinitionGridSize()
{
	if ( m_DefinitionGridSizeControl.GetCheck() == BST_CHECKED )
	{
		m_GridSizeXControl.EnableWindow( FALSE );
		m_GridSizeYControl.EnableWindow( FALSE );
	}
	else
	{
		m_GridSizeXControl.EnableWindow( TRUE );
		m_GridSizeYControl.EnableWindow( TRUE );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::OnNMCustomdrawSprinkleDensity(NMHDR *pNMHDR, LRESULT *pResult)
{
//	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

	char temp[ 128 ];

	sprintf( temp, "%d%%", m_SprinkleDensityControl.GetPos() );
	m_SprinkleDensityDisplayControl.SetWindowText( temp );
	
	*pResult = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CEntitySprinkleDlg::UseDefinitionGridSize( )
{
	return ( m_DefinitionGridSizeControl.GetCheck() == BST_CHECKED );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CEntitySprinkleDlg::OnClose()
{
	ToolManager()->SetTool( TOOL_PICK_ENTITY );

	CDialog::OnClose();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CEntitySprinkleDlg::UseRandomYaw( )
{
	return ( m_RandomYawControl.GetCheck() == BST_CHECKED );
}

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgoff.h>

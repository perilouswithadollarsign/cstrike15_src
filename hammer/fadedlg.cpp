//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include <tier2/tier2.h>
#include "FadeDlg.h"
#include "Options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================
//
// Fade Dialog Functions
//
BEGIN_MESSAGE_MAP( CFadeDlg, CDialog)
	//{{AFX_MSG_MAP(CFadeDlg)
	ON_BN_CLICKED( IDC_BUTTON_FADE_LOW, OnButtonFadeLow )
	ON_BN_CLICKED( IDC_BUTTON_FADE_MED, OnButtonFadeMed )
	ON_BN_CLICKED( IDC_BUTTON_FADE_HIGH, OnButtonFadeHigh )
	ON_BN_CLICKED( IDC_BUTTON_FADE_360, OnButtonFade360 )
	ON_BN_CLICKED( IDC_BUTTON_FADE_LEVEL, OnButtonFadeLevel )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFadeDlg::CFadeDlg( CWnd *pParent ) : CDialog( CFadeDlg::IDD, pParent )
{
	Options.view3d.nFadeMode = FADE_MODE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFadeDlg::OnInitDialog()
{
	if ( m_pParentWnd )
	{
		CRect dialogRect;
		GetWindowRect( &dialogRect );

		CRect toolbarRect;
		CToolBar *pToolBar = static_cast<CToolBar*>( m_pParentWnd );
		pToolBar->GetWindowRect( &toolbarRect );

		CRect buttonRect;
		pToolBar->GetToolBarCtrl().GetRect( ID_VIEW_PREVIEW_MODEL_FADE, &buttonRect );

		int nLeft = toolbarRect.left + buttonRect.left + ( buttonRect.Width() / 2 );
		int nTop = toolbarRect.top + buttonRect.top + ( buttonRect.Height() / 2 ) ;

		::SetWindowPos( m_hWnd, HWND_TOP, nLeft, nTop, dialogRect.Width(), dialogRect.Height(), SWP_NOCOPYBITS );
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFadeDlg::OnButtonFadeLow( void )
{
	Options.view3d.nFadeMode = FADE_MODE_LOW;
	OnOK();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFadeDlg::OnButtonFadeMed( void )
{
	Options.view3d.nFadeMode = FADE_MODE_MED;
	OnOK();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFadeDlg::OnButtonFadeHigh( void )
{
	Options.view3d.nFadeMode = FADE_MODE_HIGH;
	OnOK();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFadeDlg::OnButtonFade360( void )
{
	Options.view3d.nFadeMode = FADE_MODE_360;
	OnOK();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFadeDlg::OnButtonFadeLevel( void )
{
	Options.view3d.nFadeMode = FADE_MODE_LEVEL;
	OnOK();
}

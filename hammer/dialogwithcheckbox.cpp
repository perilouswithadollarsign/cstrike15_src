//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// syncfiledialog.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "dialogwithcheckbox.h"
#include "mapdoc.h"
#include "p4lib/ip4.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_DYNAMIC(CDialogWithCheckbox, CDialog)

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CDialogWithCheckbox::CDialogWithCheckbox( const char *pszTitleText, const char *pszDialogText, const char *pszCheckboxText, bool bCheckState, bool bDisabled, CWnd* pParent /*=NULL*/ )
	: CDialog(CDialogWithCheckbox::IDD, pParent)
	, m_strCheckboxText(_T(""))
	, m_strDialogText(_T(""))
{
	m_bDefaultCheckState = bCheckState;
	m_bCheckMark = false;
	m_bClickedOk = false;
	m_bCheckMarkDisabled = bDisabled;


	m_strTitleText = pszTitleText;
	m_strDialogText = pszDialogText;
	m_strCheckboxText = pszCheckboxText;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CDialogWithCheckbox::~CDialogWithCheckbox()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CDialogWithCheckbox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DIALOG_ICON, m_IconControl);
	DDX_Control(pDX, IDC_CHECKMARK_CONTROL, m_CheckmarkControl);

	DDX_Text(pDX, IDC_DIALOG_TEXT, m_strDialogText);
	DDX_Text(pDX, IDC_CHECKMARK_TEXT, m_strCheckboxText);
}


BEGIN_MESSAGE_MAP(CDialogWithCheckbox, CDialog)
	ON_BN_CLICKED(IDOK, &CDialogWithCheckbox::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CDialogWithCheckbox::OnBnClickedCancel)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CDialogWithCheckbox::OnBnClickedOk()
{
	m_bCheckMark = ( m_CheckmarkControl.GetCheck() ? true : false );
	m_bClickedOk = true;

	OnOK();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CDialogWithCheckbox::OnBnClickedCancel()
{
	m_bCheckMark = false;
	m_bClickedOk = false;

	OnCancel();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CDialogWithCheckbox::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText( m_strTitleText );

	m_CheckmarkControl.SetCheck( m_bDefaultCheckState );
	
	if ( m_bCheckMarkDisabled )
	{
		m_CheckmarkControl.EnableWindow( false );
		
		// also disable the caption next to the checkbox
		GetDlgItem( IDC_CHECKMARK_TEXT )->EnableWindow( false );
	}

	HICON Icon = ::LoadIcon( NULL, MAKEINTRESOURCE( IDI_ERROR ) );
	m_IconControl.SetIcon( Icon );

	return TRUE; 
}

bool CDialogWithCheckbox::IsCheckboxChecked()
{
	if ( m_bCheckMark )
	{
		return true;
	}

	return false;
}

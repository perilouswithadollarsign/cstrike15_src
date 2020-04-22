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
#include "syncfiledialog.h"
#include "mapdoc.h"
#include "p4lib/ip4.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// CSyncFileDialog dialog

bool CSyncFileDialog::m_bRepeatOperation = false;
bool CSyncFileDialog::m_bDoSync = false;

IMPLEMENT_DYNAMIC(CSyncFileDialog, CDialog)


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CSyncFileDialog::CSyncFileDialog( P4File_t *pFileInfo, CWnd* pParent /*=NULL*/ )
	: CDialog(CSyncFileDialog::IDD, pParent)
{
	m_pFileInfo = pFileInfo;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CSyncFileDialog::~CSyncFileDialog()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CSyncFileDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILENAME, m_FileNameControl);
	DDX_Control(pDX, IDC_REVISION, m_RevisionControl);
	DDX_Control(pDX, IDC_DO_OPERATION, m_DoOperationControl);
	DDX_Control(pDX, IDC_SYNC_ICON, m_IconControl);
}


BEGIN_MESSAGE_MAP(CSyncFileDialog, CDialog)
	ON_BN_CLICKED(IDOK, &CSyncFileDialog::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CSyncFileDialog::OnBnClickedCancel)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CSyncFileDialog::OnBnClickedOk()
{
	m_bRepeatOperation = ( m_DoOperationControl.GetCheck() ? true : false );
	m_bDoSync = true;

	OnOK();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CSyncFileDialog::OnBnClickedCancel()
{
	m_bRepeatOperation = ( m_DoOperationControl.GetCheck() ? true : false );
	m_bDoSync = false;

	OnCancel();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CSyncFileDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	char temp[ 256 ];

	m_FileNameControl.SetWindowText( p4->String( m_pFileInfo->m_sLocalFile ) );

	sprintf( temp, "Local Revision: %d  Head Revision: %d", m_pFileInfo->m_iHaveRevision, m_pFileInfo->m_iHeadRevision );
	m_RevisionControl.SetWindowText( temp );

	m_DoOperationControl.SetCheck( m_bRepeatOperation ? TRUE : FALSE );

	HICON	Icon = ::LoadIcon( NULL, MAKEINTRESOURCE( IDI_ERROR ) );
	m_IconControl.SetIcon( Icon );

	return TRUE; 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CheckForFileSync( const char *pszFileName, bool bClearRepeat )
{
	if ( bClearRepeat == true )
	{
		CSyncFileDialog::m_bRepeatOperation = false;
	}

	if ( p4 && Options.general.bEnablePerforceIntegration == TRUE )
	{
		P4File_t	FileInfo;

		if ( p4->GetFileInfo( pszFileName, &FileInfo ) == true )
		{
			if ( FileInfo.m_iHeadRevision != FileInfo.m_iHaveRevision )
			{
				if ( bClearRepeat == true || CSyncFileDialog::m_bRepeatOperation == false )
				{
					CSyncFileDialog	SyncFileDialog( &FileInfo );

					SyncFileDialog.DoModal();
				}

				if ( CSyncFileDialog::m_bDoSync == true )
				{
					if ( p4->SyncFile( pszFileName ) == false )
					{
						AfxMessageBox( "Sync operation was NOT successful!", MB_OK ) ;
					}
				}
			}
		}
	}
}

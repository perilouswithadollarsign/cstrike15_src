//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TransformDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "versioncontroldialog.h"
#include "mapdoc.h"
#include "p4lib/ip4.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// CMapDocCheckin dialog

IMPLEMENT_DYNAMIC(CMapDocCheckin, CDialog)

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CMapDocCheckin::CMapDocCheckin(CWnd* pParent /*=NULL*/)
: CDialog(CMapDocCheckin::IDD, pParent)
{

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CMapDocCheckin::~CMapDocCheckin()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocCheckin::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECKIN_LIST, m_CheckinListCtrl);
	DDX_Control(pDX, IDC_CHECKIN_DESCRIPTION, m_DescriptionCtrl);
	DDX_Control(pDX, IDC_CHECKIN_STATUS, m_CheckInStatusControl);
	DDX_Control(pDX, ID_SUBMIT, m_SubmitButtonControl);
}


BEGIN_MESSAGE_MAP(CMapDocCheckin, CDialog)
	ON_BN_CLICKED(ID_SUBMIT, &CMapDocCheckin::OnBnClickedSubmit)
	ON_NOTIFY(NM_RCLICK, IDC_CHECKIN_LIST, &CMapDocCheckin::OnNMRclickCheckinList)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocCheckin::AddFileToList( CMapDoc *pMapDoc, P4File_t *FileInfo )
{
	int nIndex = m_CheckinListCtrl.InsertItem( m_CheckinListCtrl.GetItemCount(), "" );

//	nCount++;
	m_CheckinListCtrl.SetItemData( nIndex, ( DWORD_PTR )pMapDoc );
	switch( FileInfo->m_eOpenState )
	{	
		case P4FILE_OPENED_FOR_ADD:
			m_CheckinListCtrl.SetItemText( nIndex, 1, "Add" );
			break;

		case P4FILE_OPENED_FOR_EDIT:
			m_CheckinListCtrl.SetItemText( nIndex, 1, "Edit" );
			break;
	}
	m_CheckinListCtrl.SetItemText( nIndex, 2, p4->String( FileInfo->m_sName ) );
	m_CheckinListCtrl.SetItemText( nIndex, 3, p4->String( FileInfo->m_sPath ) );
	m_FileList.AddToTail( FileInfo->m_sLocalFile );

	if ( pMapDoc != NULL && pMapDoc->IsDefaultCheckIn() )
	{
		ListView_SetItemState( m_CheckinListCtrl.m_hWnd, nIndex, INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ), LVIS_STATEIMAGEMASK );
		pMapDoc->ClearDefaultCheckIn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CMapDocCheckin::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_CheckinListCtrl.SetExtendedStyle( m_CheckinListCtrl.GetExtendedStyle() | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT );

	m_CheckinListCtrl.InsertColumn( 0, "", LVCFMT_LEFT, 30, -1 );
	m_CheckinListCtrl.InsertColumn( 1, "Status", LVCFMT_LEFT, 50, -1 );
	m_CheckinListCtrl.InsertColumn( 2, "Name", LVCFMT_LEFT, 180, -1 );
	m_CheckinListCtrl.InsertColumn( 3, "Folder", LVCFMT_LEFT, 360, -1 );

	if ( p4 == NULL ) 
	{
		return TRUE;
	}

	P4File_t				FileInfo;
	CUtlVector< P4File_t >	FileList;

	p4->GetOpenedFileList( FileList, true );

	POSITION pos = APP()->pMapDocTemplate->GetFirstDocPosition();
	while( pos != NULL )
	{
		CDocument *pDoc = APP()->pMapDocTemplate->GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc )
		{
			if ( pMapDoc->IsCheckedOut() )
			{
				if ( p4->GetFileInfo( pMapDoc->GetPathName(), &FileInfo ) == true )
				{
					for( int i = 0; i < FileList.Count(); i++ )
					{
						if ( FileList[ i ].m_sClientFile == FileInfo.m_sClientFile )
						{
							FileList.Remove( i );
							break;
						}
					}

					AddFileToList( pMapDoc, &FileInfo );
				}
			}
		}
	}

	for( int i = 0; i < FileList.Count(); i++ )
	{
		AddFileToList( NULL, &FileList[ i ] );
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocCheckin::OnOK()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocCheckin::OnBnClickedSubmit()
{
	int		nFileCount = 0;
	char	temp[ 2048 ];

	for( int i = 0; i < m_CheckinListCtrl.GetItemCount(); i++ )
	{
		if ( m_CheckinListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
		{
			nFileCount++;
		}
	}

	if ( nFileCount > 0 )
	{
		CString	Description;
		m_DescriptionCtrl.GetWindowText( Description );
		if ( Description.GetLength() < 2 )
		{
			m_CheckInStatusControl.SetWindowText( "Checkin FAILED!" );
			AfxMessageBox( "Please put in something descriptive for the description.  I took the time to type this dialog, the least you could do is type something!", MB_ICONHAND | MB_OK );
			return;
		}
		if ( Description.GetLength() >= P4_MAX_INPUT_BUFFER_SIZE )
		{
			m_CheckInStatusControl.SetWindowText( "Checkin FAILED!" );
			sprintf( temp, "Your description is too long.  Please shorten it down by %d characters.", Description.GetLength() - P4_MAX_INPUT_BUFFER_SIZE + 1 );
			AfxMessageBox( temp, MB_ICONHAND | MB_OK );
			return;
		}

		m_SubmitButtonControl.EnableWindow( FALSE );
		sprintf( temp, "Checking in %d file(s).  Please wait...", nFileCount );
		m_CheckInStatusControl.SetWindowText( temp );

		const char **ppFileNames = ( const char** )stackalloc( nFileCount * sizeof( char * ) );

		nFileCount = 0;
		for( int i = 0; i < m_CheckinListCtrl.GetItemCount(); i++ )
		{
			if ( m_CheckinListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CMapDoc	*pMapDoc = ( CMapDoc * )m_CheckinListCtrl.GetItemData( i );
				const char *pszFileName = p4->String( m_FileList[ i ] );

				if ( pMapDoc != NULL )
				{
					ppFileNames[ nFileCount ] = pMapDoc->GetPathName();
					pMapDoc->OnSaveDocument( pMapDoc->GetPathName() );
				}
				else
				{
					ppFileNames[ nFileCount ] = pszFileName;
				}
				nFileCount++;
			}
		}

		// we need to replace \r\n with \t\n to make multi-line p4 changelist descriptions happy
		Description.Replace( '\n', '\t' );	
		Description.Replace( '\r', '\n' );

		if ( p4->SubmitFiles( nFileCount, ppFileNames, Description ) == false )
		{
			m_CheckInStatusControl.SetWindowText( "Checkin FAILED!" );
			m_SubmitButtonControl.EnableWindow( TRUE );

			sprintf( temp, "Could not check in map(s): %s", p4->GetLastError() );
			AfxMessageBox( temp, MB_ICONHAND | MB_OK );

			return;
		}

		for( int i = 0; i < m_CheckinListCtrl.GetItemCount(); i++ )
		{
			if ( m_CheckinListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CMapDoc	*pMapDoc = ( CMapDoc * )m_CheckinListCtrl.GetItemData( i );
				
				if ( pMapDoc != NULL )
				{
					pMapDoc->CheckFileStatus();
				}
			}
		}

		m_SubmitButtonControl.EnableWindow( TRUE );
		m_CheckInStatusControl.SetWindowText( "" );
	}

	EndDialog( IDOK );
}


void CMapDocCheckin::OnNMRclickCheckinList(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;

	for( int i = 0; i < m_CheckinListCtrl.GetItemCount(); i++ )
	{
		if ( m_bSelectAll == true )
		{
			m_CheckinListCtrl.SetItemState( i, ( unsigned int )INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ), LVIS_STATEIMAGEMASK );
		}
		else
		{
			m_CheckinListCtrl.SetItemState( i, ( unsigned int )INDEXTOSTATEIMAGEMASK( LVIS_FOCUSED ), LVIS_STATEIMAGEMASK );
		}
	}

	m_bSelectAll = !m_bSelectAll;
}


void CMapDocCheckin::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	m_bSelectAll = true;
}


// CMapDocStatus dialog


IMPLEMENT_DYNAMIC(CMapDocStatus, CDialog)


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CMapDocStatus::CMapDocStatus(CWnd* pParent /*=NULL*/)
	: CDialog(CMapDocStatus::IDD, pParent)
{

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CMapDocStatus::~CMapDocStatus()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocStatus::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILE_LIST, m_FileListCtrl);
	DDX_Control(pDX, IDC_STATUS_TEXT, m_StatusTextControl);
	DDX_Control(pDX, IDSYNC, m_SyncControl);
	DDX_Control(pDX, IDADD, m_AddControl);
	DDX_Control(pDX, IDCHECKOUT, m_CheckOutControl);
	DDX_Control(pDX, IDCANCEL, m_DoneControl);
	DDX_Control(pDX, IDREVERT, m_RevertControl);
}


BEGIN_MESSAGE_MAP(CMapDocStatus, CDialog)
	ON_BN_CLICKED(IDCHECKOUT, &CMapDocStatus::OnBnClickedCheckout)
	ON_BN_CLICKED(IDADD, &CMapDocStatus::OnBnClickedAdd)
	ON_BN_CLICKED(IDSYNC, &CMapDocStatus::OnBnClickedSync)
	ON_BN_CLICKED(IDREVERT, &CMapDocStatus::OnBnClickedRevert)
	ON_NOTIFY(NM_RCLICK, IDC_FILE_LIST, &CMapDocStatus::OnNMRclickFileList)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CMapDocStatus::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_FileListCtrl.SetExtendedStyle( m_FileListCtrl.GetExtendedStyle() | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT );

	m_FileListCtrl.InsertColumn( 0, "", LVCFMT_LEFT, 30, -1 );
	m_FileListCtrl.InsertColumn( 1, "Status", LVCFMT_LEFT, 80, -1 );
	m_FileListCtrl.InsertColumn( 2, "Revision", LVCFMT_LEFT, 70, -1 );
	m_FileListCtrl.InsertColumn( 3, "Name", LVCFMT_LEFT, 150, -1 );
	m_FileListCtrl.InsertColumn( 4, "Folder", LVCFMT_LEFT, 300, -1 );

	SetControls( false, "" );

	UpdateMapList();

	if ( Options.general.bEnablePerforceIntegration == FALSE )
	{
		GetDlgItem( IDCHECKOUT )->EnableWindow( false );
		GetDlgItem( IDADD )->EnableWindow( false );
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocStatus::UpdateMapList( bool RedoList )
{
	P4File_t	FileInfo;

	m_FileListCtrl.DeleteAllItems();

	int nCount = 0;

	POSITION pos = APP()->pMapDocTemplate->GetFirstDocPosition();
	while( pos != NULL )
	{
		CDocument *pDoc = APP()->pMapDocTemplate->GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		pMapDoc->CheckFileStatus();

		int nIndex = m_FileListCtrl.InsertItem( nCount, "" );
		nCount++;
		m_FileListCtrl.SetItemData( nIndex, ( DWORD_PTR )pMapDoc );

		if ( p4 && Options.general.bEnablePerforceIntegration == TRUE && p4->GetFileInfo( pMapDoc->GetPathName(), &FileInfo ) == true )
		{
			switch( FileInfo.m_eOpenState )
			{	
				case P4FILE_UNOPENED:
					if ( pMapDoc->IsReadOnly() )
					{
						m_FileListCtrl.SetItemText( nIndex, 1, "Read Only" );
					}
					else
					{
						m_FileListCtrl.SetItemText( nIndex, 1, "Writeable" );
					}
					break;

				case P4FILE_OPENED_FOR_ADD:
					m_FileListCtrl.SetItemText( nIndex, 1, "Add" );
					break;

				case P4FILE_OPENED_FOR_EDIT:
					m_FileListCtrl.SetItemText( nIndex, 1, "Edit" );
					break;
			}

			if ( FileInfo.m_iHaveRevision == FileInfo.m_iHeadRevision )
			{
				char temp[ 128 ];
				
				sprintf( temp, "%d", FileInfo.m_iHaveRevision );
				m_FileListCtrl.SetItemText( nIndex, 2, temp );
			}
			else
			{
				char temp[ 128 ];

				sprintf( temp, "%d / %d", FileInfo.m_iHaveRevision, FileInfo.m_iHeadRevision );
				m_FileListCtrl.SetItemText( nIndex, 2, temp );
			}

			m_FileListCtrl.SetItemText( nIndex, 3, p4->String( FileInfo.m_sName ) );
			m_FileListCtrl.SetItemText( nIndex, 4, p4->String( FileInfo.m_sPath ) );
		}
		else
		{
			if ( pMapDoc->IsReadOnly() )
			{
				m_FileListCtrl.SetItemText( nIndex, 1, "Read Only" );
			}
			else
			{
				m_FileListCtrl.SetItemText( nIndex, 1, "Writeable" );
			}

			CString strMapFilename = pMapDoc->GetPathName(); 

			if ( strMapFilename.IsEmpty() ) 
			{
				m_FileListCtrl.SetItemText( nIndex, 3, "not saved" );
				m_FileListCtrl.SetItemText( nIndex, 4, "" );
			}
			// the map already has a filename 
			else 
			{
				int nFilenameBeginOffset = strMapFilename.ReverseFind( '\\' ) + 1;
				int nFilenameEndOffset = strMapFilename.Find( '.' );

				m_FileListCtrl.SetItemText( nIndex, 3, strMapFilename.Mid( nFilenameBeginOffset, nFilenameEndOffset - nFilenameBeginOffset ) );
				m_FileListCtrl.SetItemText( nIndex, 4, strMapFilename.Mid( 0, nFilenameBeginOffset - 1 ) );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocStatus::OnBnClickedCheckout()
{
	if ( !p4 || Options.general.bEnablePerforceIntegration == FALSE )
	{
		return;
	}

	char	temp[ 2048 ];
	int		nFileCount = 0;

	for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
	{
		if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
		{
			nFileCount++;
		}
	}

	if ( nFileCount > 0 )
	{
		sprintf( temp, "Checking out %d file(s).  Please wait...", nFileCount );
		SetControls( true, temp );

		const char **ppFileNames = ( const char** )stackalloc( nFileCount * sizeof( char * ) );

		nFileCount = 0;
		for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
		{
			if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CMapDoc	*pMapDoc = ( CMapDoc * )m_FileListCtrl.GetItemData( i );

				ppFileNames[ nFileCount ] = pMapDoc->GetPathName();
				nFileCount++;
			}
		}

		if ( p4->OpenFilesForEdit( nFileCount, ppFileNames ) == false )
		{
			SetControls( false, "Checkout FAILED." );

			sprintf( temp, "Could not check out map(s): %s", p4->GetLastError() );
			AfxMessageBox( temp, MB_ICONHAND | MB_OK );

			return;
		}

		UpdateMapList();

		sprintf( temp, "Checked out %d file(s).", nFileCount );
		SetControls( false, temp );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDocStatus::OnBnClickedAdd()
{
	if ( !p4 || Options.general.bEnablePerforceIntegration == FALSE )
	{
		return;
	}

	int		nFileCount = 0;
	char	temp[ 2048 ];

	for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
	{
		if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
		{
			nFileCount++;
		}
	}

	if ( nFileCount > 0 )
	{
		sprintf( temp, "Adding %d file(s).  Please wait...", nFileCount );
		SetControls( true, temp );

		const char **ppFileNames = ( const char** )stackalloc( nFileCount * sizeof( char * ) );

		nFileCount = 0;
		for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
		{
			if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CMapDoc	*pMapDoc = ( CMapDoc * )m_FileListCtrl.GetItemData( i );

				ppFileNames[ nFileCount ] = pMapDoc->GetPathName();
				nFileCount++;
			}
		}

		if ( p4->OpenFilesForAdd( nFileCount, ppFileNames ) == false )
		{
			SetControls( false, "Adding FAILED." );
			sprintf( temp, "Could not add map(s): %s", p4->GetLastError() );
			AfxMessageBox( temp, MB_ICONHAND | MB_OK );

			return;
		}

		UpdateMapList();

		sprintf( temp, "Added %d file(s).", nFileCount );
		SetControls( false, temp );
	}
}

void CMapDocStatus::OnBnClickedSync()
{
	if ( !p4 || Options.general.bEnablePerforceIntegration == FALSE )
	{
		return;
	}

	int		nFileCount = 0;
	char	temp[ 2048 ];

	for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
	{
		if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
		{
			nFileCount++;
		}
	}

	if ( nFileCount > 0 )
	{
		sprintf( temp, "Syncing %d file(s).  Please wait...", nFileCount );
		SetControls( true, temp );

		int nSyncFileCount = 0;
		for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
		{
			if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CMapDoc	*pMapDoc = ( CMapDoc * )m_FileListCtrl.GetItemData( i );

				if ( pMapDoc->SyncToHeadRevision() == true )
				{
					nSyncFileCount++;
				}
			}
		}

		UpdateMapList();

		if ( CMapDoc::GetActiveMapDoc() != NULL )
		{
			CMapDoc::GetActiveMapDoc()->UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
		}

		if ( nSyncFileCount == nFileCount )
		{
			sprintf( temp, "Synced %d file(s).", nSyncFileCount );
		}
		else
		{
			sprintf( temp, "Synced %d file(s).  %d file(s) were not synced!", nSyncFileCount, nFileCount - nSyncFileCount );
		}
		SetControls( false, temp );
	}
}

void CMapDocStatus::SetControls( bool bDisable, char *pszMessage )
{
	BOOL bEnable = ( bDisable ? FALSE : TRUE );

	m_StatusTextControl.SetWindowText( pszMessage );

	m_SyncControl.EnableWindow( bEnable );
	m_AddControl.EnableWindow( bEnable );
	m_CheckOutControl.EnableWindow( bEnable );
	m_RevertControl.EnableWindow( bEnable );
	m_DoneControl.EnableWindow( bEnable );
}

void CMapDocStatus::OnBnClickedRevert()
{
	if ( !p4 || Options.general.bEnablePerforceIntegration == FALSE )
	{
		return;
	}

	int		nFileCount = 0;
	char	temp[ 2048 ];

	for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
	{
		if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
		{
			nFileCount++;
		}
	}

	if ( nFileCount > 0 )
	{
		if ( AfxMessageBox( "Are you sure you want to revert these file(s)?", MB_ICONQUESTION | MB_YESNO ) == IDNO )
		{
			return;
		}

		sprintf( temp, "Reverting %d file(s).  Please wait...", nFileCount );
		SetControls( true, temp );

		int nRevertFileCount = 0;
		for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
		{
			if ( m_FileListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CMapDoc	*pMapDoc = ( CMapDoc * )m_FileListCtrl.GetItemData( i );

				if ( pMapDoc->Revert() == true )
				{
					nRevertFileCount++;
				}
			}
		}

		UpdateMapList();

		if ( CMapDoc::GetActiveMapDoc() != NULL )
		{
			CMapDoc::GetActiveMapDoc()->UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
		}

		if ( nRevertFileCount == nFileCount )
		{
			sprintf( temp, "Reverted %d file(s).", nRevertFileCount );
		}
		else
		{
			sprintf( temp, "Reverted %d file(s).  %d file(s) were not reverted!", nRevertFileCount, nFileCount - nRevertFileCount );
		}
		SetControls( false, temp );
	}
}

void CMapDocStatus::OnNMRclickFileList(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;

	for( int i = 0; i < m_FileListCtrl.GetItemCount(); i++ )
	{
		if ( m_bSelectAll == true )
		{
			m_FileListCtrl.SetItemState( i, ( unsigned int )INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ), LVIS_STATEIMAGEMASK );
		}
		else
		{
			m_FileListCtrl.SetItemState( i, ( unsigned int )INDEXTOSTATEIMAGEMASK( LVIS_FOCUSED ), LVIS_STATEIMAGEMASK );
		}
	}

	m_bSelectAll = !m_bSelectAll;
}

void CMapDocStatus::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	m_bSelectAll = true;
}

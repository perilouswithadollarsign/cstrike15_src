//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "filesystem.h"
#include "mapclass.h"
#include "mapentity.h"
#include "history.h"
#include "dlglistmanage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP( CDlgListManage, CDialog )
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_SCRIPT_LIST_ADD, &CDlgListManage::OnBnClickedScriptListAdd)
	ON_BN_CLICKED(IDC_SCRIPT_LIST_REMOVE, &CDlgListManage::OnBnClickedScriptListRemove)
	ON_BN_CLICKED(IDC_SCRIPT_LIST_EDIT, &CDlgListManage::OnBnClickedScriptListEdit)
END_MESSAGE_MAP()

CDlgListManage::CDlgListManage(CWnd *pParent, IDlgListManageBrowse *pBrowseImpl, const CMapObjectList *pObjectList)
	: CDialog(CDlgListManage::IDD, pParent)
{
	m_pBrowseImpl = pBrowseImpl;
	m_rcDialog.SetRectEmpty();
	m_pObjectList = pObjectList;
}

static bool StringLessFunc( CString const &a, CString const &b )
{
	return (a < b);
}

void CDlgListManage::PopulateScriptList( void )
{
	// Clear us out!
	m_ScriptList.ResetContent();

	// Populate the list
	CUtlMap<CString, int> mapStrings( StringLessFunc );	// A map of strings to their counts

	// Iterate through all map objects currently being edited
	FOR_EACH_OBJ( *m_pObjectList, pos )
	{
		const CMapClass *pObject = m_pObjectList->Element( pos );
		if ( (pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity))) )
		{
			CMapEntity *pEntity = (CMapEntity *)pObject;
			CString strScriptName = pEntity->GetKeyValue( "vscripts" );

			// Only consider this if we have strings here!
			if ( strScriptName.IsEmpty() )
				continue;

			int nStart = 0;
			CString strToken = strScriptName.Tokenize( " ", nStart );

			// Now, iterate through all the listed scripts in the entity and check their counts
			while( strToken != "" )
			{
				// If we've removed this, filter it out
				if ( m_vSubtractions.Find( strToken ) != m_vSubtractions.InvalidIndex() )
				{
					strToken = strScriptName.Tokenize( " ", nStart );
					continue;
				}

				// Also, keep a map into the global scripts we're adding for later!
				int nIndex = mapStrings.Find( strToken );
				if ( nIndex != mapStrings.InvalidIndex() )
				{
					mapStrings[nIndex]++;
				}
				else
				{
					int nIndex = mapStrings.Insert( strToken );
					mapStrings[nIndex] = 1;
				}					

				// Continue to the next token
				strToken = strScriptName.Tokenize( " ", nStart );
			}
		}
	}

	// Include new additions
	FOR_EACH_VEC( m_vAdditions, itr )
	{
		int nIndex = mapStrings.Insert( m_vAdditions[itr] );
		mapStrings[nIndex] = m_pObjectList->Count();			// Always added to all edited members
	}

	// Now go through and add things to the list with proper coloring
	FOR_EACH_MAP( mapStrings, itr )
	{
		// If it's the same for every object, it's bold
		if ( mapStrings[itr] == m_pObjectList->Count() )
		{
			if ( m_pObjectList->Count() == 1 )
			{
				// Normal (single instance)
				m_ScriptList.AddItemText( mapStrings.Key( itr ), LISTFONT_NORMAL );
			}
			else
			{
				// Bold (multiple instances)
				m_ScriptList.AddItemText( mapStrings.Key( itr ), LISTFONT_BOLD );
			}
		}
		else
		{
			// Red (different)
			m_ScriptList.AddItemText( mapStrings.Key( itr ), LISTFONT_DUPLICATE );
		}
	}
}

void CDlgListManage::UpdateScriptChanges( void )
{
	// Nothing to do currently!
}

void CDlgListManage::SaveScriptChanges( void )
{
	GetHistory()->MarkUndoPosition( NULL, "VScript update" );

	// Iterate through all map objects currently being edited
	FOR_EACH_OBJ( *m_pObjectList, pos )
	{
		const CMapClass *pObject = m_pObjectList->Element( pos );
		if ( (pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity))) )
		{
			CMapEntity *pEntity = (CMapEntity *)pObject;
			CString strScriptName = pEntity->GetKeyValue( "vscripts" );

			GetHistory()->Keep( pEntity );

			// Add in all new additions
			FOR_EACH_VEC( m_vAdditions, itr )
			{
				// Do we already have this in the list somehow?
				if ( strScriptName.Find( m_vAdditions[itr], 0 ) != -1 )
					continue;

				// Append it
				strScriptName += " " + m_vAdditions[itr];
			}
			
			// Subtract things we deleted
			FOR_EACH_VEC( m_vSubtractions, itr )
			{
				// Find an occurence of this string in our base strings
				int nIndex = strScriptName.Find( m_vSubtractions[itr], 0 );
				if ( nIndex == -1 )
					continue;

				// Kill it!
				int nEndIndex = strScriptName.Find( ' ', nIndex );
				if ( nEndIndex == -1 )
				{
					nEndIndex = strlen( strScriptName ); // Really?  You can't ask a CString its length?
				}

				strScriptName.Delete( nIndex, (nEndIndex-nIndex) );
			}
			
			// Done!
			pEntity->SetKeyValue( "vscripts", strScriptName );
		}
	}

	// Clear it
	m_vAdditions.RemoveAll();
	m_vSubtractions.RemoveAll();
}

void CDlgListManage::DoDataExchange( CDataExchange* pDX )
{
	DDX_Control( pDX, IDC_SCRIPT_LIST, m_ScriptList );

	CDialog::DoDataExchange(pDX);

	if ( pDX->m_bSaveAndValidate )
	{
		UpdateScriptChanges();
	}
	else
	{
		PopulateScriptList();
	}
}

BOOL CDlgListManage::OnInitDialog()
{
	BOOL bResult = CDialog::OnInitDialog();

	GetWindowRect( m_rcDialog );

	int arrCtlIds[] = { IDOK, IDCANCEL, IDC_SCRIPT_LIST, IDC_BROWSE };
	int riFlags[] = { ResizeInfo_t::RI_TOP_AND_LEFT, ResizeInfo_t::RI_TOP_AND_LEFT, ResizeInfo_t::RI_WIDTH_AND_HEIGHT, ResizeInfo_t::RI_TOP };
	for ( int k = 0; k < ARRAYSIZE( arrCtlIds ); ++ k )
	{
		CWnd *pWnd = GetDlgItem( arrCtlIds[ k ] );
		if ( !pWnd )
			continue;

		ResizeInfo_t ri;
		ri.flags = riFlags[k];
		pWnd->GetWindowRect( ri.rc );
		this->ScreenToClient( ri.rc );
		
		m_ctlInfo[ arrCtlIds[ k ] ] = ri;
	}

	if ( !m_pBrowseImpl )
	{
		if ( CWnd *pWnd = GetDlgItem( IDC_BROWSE ) )
			pWnd->ShowWindow( SW_HIDE );
	}

	return bResult;
}

void CDlgListManage::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize( nType, cx, cy );

	CRect rcDialog;
	GetWindowRect( rcDialog );

	int dx = rcDialog.Width() - m_rcDialog.Width();
	int dy = rcDialog.Height() - m_rcDialog.Height();

	for ( POSITION pos = m_ctlInfo.GetStartPosition(); pos; )
	{
		int ctlId;
		ResizeInfo_t ri;
		m_ctlInfo.GetNextAssoc( pos, ctlId, ri );

		CWnd *pWnd = GetDlgItem( ctlId );
		if ( !pWnd )
			continue;

		CRect rcWnd = ri.rc;
		if ( ri.flags & ResizeInfo_t::RI_HEIGHT )
			rcWnd.bottom += dy;
		if ( ri.flags & ResizeInfo_t::RI_WIDTH )
			rcWnd.right += dx;
		if ( ri.flags & ResizeInfo_t::RI_LEFT )
			rcWnd.OffsetRect( dx, 0 );
		if ( ri.flags & ResizeInfo_t::RI_TOP )
			rcWnd.OffsetRect( 0, dy );

		pWnd->MoveWindow( rcWnd );
	}
}

void CDlgListManage::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CDialog::OnGetMinMaxInfo( lpMMI );

	if ( !m_rcDialog.IsRectEmpty() )
	{
		lpMMI->ptMinTrackSize.x = m_rcDialog.Width();
		lpMMI->ptMinTrackSize.y = m_rcDialog.Height();
	}
}

void CDlgListManage::OnBnClickedScriptListAdd()
{
	// Launch a browse window
	CStringList lstBrowse;
	if ( m_pBrowseImpl == NULL || m_pBrowseImpl->HandleBrowse( lstBrowse ) == false )
		return;

	// Nothing was added!
	if ( lstBrowse.IsEmpty() )
		return;
	
	// Operate on all strings we got back from the browser
	for ( POSITION pos = lstBrowse.GetHeadPosition(); pos; )
	{
		const CString &strTemp = lstBrowse.GetNext( pos );

		// Update our change lists
		m_vSubtractions.FindAndRemove( strTemp );

		// Don't add twice!
		if ( m_vAdditions.Find( strTemp ) == -1 )
		{
			m_vAdditions.AddToTail( strTemp );
		}
	}

	UpdateData();
	UpdateData( FALSE );
}

void CDlgListManage::OnBnClickedScriptListRemove()
{
	// Remove the current selection
	int nSelectedItem = m_ScriptList.GetCurSel();
	if ( nSelectedItem != LB_ERR )
	{
		CString strOldText;
		m_ScriptList.GetText( nSelectedItem, strOldText );

		// Update our change lists
		m_vAdditions.FindAndRemove( strOldText );

		// Don't add twice!
		if ( m_vSubtractions.Find( strOldText ) == -1 )
		{
			m_vSubtractions.AddToTail( strOldText );
		}

		// Kill the line
		m_ScriptList.DeleteString( nSelectedItem );
		
		// Move our current selection around based on where we are in the list
		if ( nSelectedItem >= m_ScriptList.GetCount() )
		{
			nSelectedItem = m_ScriptList.GetCount() - 1;
		}

		m_ScriptList.SetCurSel( nSelectedItem );	// Don't lose selection (annoying for multi-delete)
	}

	// Now update to reflect the change
	UpdateData();
}

void CDlgListManage::OnBnClickedScriptListEdit()
{
	// Get what they've currently selected
	int nSelectedItem = m_ScriptList.GetCurSel();
	if ( nSelectedItem == LB_ERR )
		return;

	// Get the file they've selected
	CString strSelectedText;
	m_ScriptList.GetText( nSelectedItem, strSelectedText );

	// Cobble together a complete filename
	CString strFilename = "scripts\\vscripts\\" + strSelectedText;

	char pFullPath[MAX_PATH];
	if ( g_pFullFileSystem->GetLocalPath( strFilename, pFullPath, MAX_PATH ) )
	{
		ShellExecute( NULL, "open", pFullPath, NULL, NULL, SW_SHOWNORMAL );
	}
}

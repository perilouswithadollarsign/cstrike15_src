//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "resource.h"
#include "MessageBoxWithCheckBox.h"
#include "ChoreoView.h"
#include "choreoactor.h"
#include "mdlviewer.h"

static CMessageBoxWithCheckBoxParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// MessageBoxWithCheckBox  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK MessageBoxWithCheckBoxPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_STATIC_PROMPT, g_Params.m_szPrompt );
			SetDlgItemText( hwndDlg, IDC_MESSAGEBOX_CHECKBOX, g_Params.m_szCheckBoxText );
			
			SendMessage( GetDlgItem( hwndDlg, IDC_MESSAGEBOX_CHECKBOX ), BM_SETCHECK, ( WPARAM )g_Params.m_bChecked ? BST_CHECKED : BST_UNCHECKED, (LPARAM)0 );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );
		}
		return FALSE;  

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				g_Params.m_bChecked = SendMessage( GetDlgItem( hwndDlg, IDC_MESSAGEBOX_CHECKBOX ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
				EndDialog( hwndDlg, 1 );
			}
			break;
		case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		}
		return TRUE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// MessageBoxWithCheckBox  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int MessageBoxWithCheckBox( CMessageBoxWithCheckBoxParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_MESSAGEBOX_WITHCHECKBOX ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)MessageBoxWithCheckBoxPropertiesDialogProc );

	*params = g_Params;

	return retval;
}
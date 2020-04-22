//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef __DIALOG_WITH_CHECKBOX_H
#define __DIALOG_WITH_CHECKBOX_H
#pragma once

#include "afxwin.h"

struct P4File_t;


// CDialogWithCheckbox dialog

class CDialogWithCheckbox : public CDialog
{
	DECLARE_DYNAMIC(CDialogWithCheckbox)

public:
	CDialogWithCheckbox( const char *pszTitleText, const char *pszDialogText, const char *pszCheckboxText, bool bCheckState = false, bool bDisabled = false, CWnd* pParent = NULL );   // constructor
	virtual ~CDialogWithCheckbox();

// Dialog Data
	enum { IDD = IDD_DIALOG_WITH_CHECKBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_DialogTextControl;
	CButton m_CheckmarkControl;
	CStatic m_IconControl;	

	CString m_strTitleText;
	CString m_strDialogText;
	CString m_strCheckboxText;

	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	virtual BOOL OnInitDialog();
	bool IsCheckboxChecked();

	int m_bDefaultCheckState;
	bool m_bCheckMark;
	bool m_bClickedOk;
	bool m_bCheckMarkDisabled;
};


#endif // __SYNC_FILE_DIALOG_H

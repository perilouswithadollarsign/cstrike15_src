//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef __SYNC_FILE_DIALOG_H
#define __SYNC_FILE_DIALOG_H
#pragma once

#include "afxwin.h"

struct P4File_t;


// CSyncFileDialog dialog

class CSyncFileDialog : public CDialog
{
	DECLARE_DYNAMIC(CSyncFileDialog)

public:
	CSyncFileDialog( P4File_t *pFileInfo, CWnd* pParent = NULL );   // standard constructor
	virtual ~CSyncFileDialog();

// Dialog Data
	enum { IDD = IDD_DIALOG_SYNC_FILE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	P4File_t	*m_pFileInfo;

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_FileNameControl;
	CStatic m_RevisionControl;
	CButton m_DoOperationControl;
	CStatic m_IconControl;
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	virtual BOOL OnInitDialog();

	static bool m_bRepeatOperation;
	static bool m_bDoSync;
};

void CheckForFileSync( const char *pszFileName, bool bClearRepeat );

#endif // __SYNC_FILE_DIALOG_H

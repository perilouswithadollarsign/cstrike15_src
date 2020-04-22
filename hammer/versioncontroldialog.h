//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef __VERSION_CONTROL_DIALOG_H
#define __VERSION_CONTROL_DIALOG_H
#pragma once

#include "utlsymbol.h"
#include "afxwin.h"


struct P4File_t;


class CMapDocCheckin : public CDialog
{
	DECLARE_DYNAMIC(CMapDocCheckin)

public:
	CMapDocCheckin(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMapDocCheckin();

	// Dialog Data
	enum { IDD = IDD_MAPDOC_CHECKIN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void AddFileToList( CMapDoc *pMapDoc, P4File_t *FileInfo );

	bool	m_bSelectAll;

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnClickedSubmit();

	CUtlVector< CUtlSymbol >	m_FileList;
	CListCtrl m_CheckinListCtrl;
	CEdit m_DescriptionCtrl;
	CStatic m_CheckInStatusControl;
	CButton m_SubmitButtonControl;
	afx_msg void OnNMRclickCheckinList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};


class CMapDocStatus : public CDialog
{
	DECLARE_DYNAMIC(CMapDocStatus)

public:
	CMapDocStatus(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMapDocStatus();

// Dialog Data
	enum { IDD = IDD_MAPDOC_STATUS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void UpdateMapList( bool RedoList = false );
	void SetControls( bool bDisable, char *pszMessage );

	bool	m_bSelectAll;

	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL OnInitDialog();

	CListCtrl m_FileListCtrl;
	afx_msg void OnBnClickedCheckout();
	afx_msg void OnBnClickedAdd();
	CStatic m_StatusTextControl;
	CButton m_SyncControl;
	CButton m_AddControl;
	CButton m_CheckOutControl;
	afx_msg void OnBnClickedSync();
	CButton m_DoneControl;
	CButton m_RevertControl;
	afx_msg void OnBnClickedRevert();
	afx_msg void OnNMRclickFileList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};

#endif // __VERSION_CONTROL_DIALOG_H

// MFC_DEMO.h : main header file for the MFC_DEMO application
//

#if !defined(AFX_MFC_DEMO_H__B7D0AFE6_20A7_11D2_B1B0_0040053C38B6__INCLUDED_)
#define AFX_MFC_DEMO_H__B7D0AFE6_20A7_11D2_B1B0_0040053C38B6__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMOApp:
// See MFC_DEMO.cpp for the implementation of this class
//

class CMFC_DEMOApp : public CWinApp
{
public:
	CMFC_DEMOApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMFC_DEMOApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CMFC_DEMOApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MFC_DEMO_H__B7D0AFE6_20A7_11D2_B1B0_0040053C38B6__INCLUDED_)

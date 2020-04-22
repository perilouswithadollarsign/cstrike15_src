// MFC_DEMOView.h : interface of the CMFC_DEMOView class
//
/////////////////////////////////////////////////////////////////////////////


#if !defined(AFX_MFC_DEMOVIEW_H__B7D0AFEE_20A7_11D2_B1B0_0040053C38B6__INCLUDED_)
#define      AFX_MFC_DEMOVIEW_H__B7D0AFEE_20A7_11D2_B1B0_0040053C38B6__INCLUDED_

#include <afxmt.h>
#include <windows.h>
#include <wintab.h>

#include "point.h"

using namespace std;

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CMFC_DEMOView : public CView
{
	CMutex *pWTMutex;
	POINT csr;
	HCTX hCtx;
	unsigned prev_pkButtons;
	LOGCONTEXT lc;

protected: // create from serialization only
	CMFC_DEMOView();
	DECLARE_DYNCREATE(CMFC_DEMOView)

// Attributes
public:
	CMFC_DEMODoc* GetDocument();

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMFC_DEMOView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMFC_DEMOView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	afx_msg LRESULT OnWTPacket(WPARAM, LPARAM);

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CMFC_DEMOView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnCancelMode();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in MFC_DEMOView.cpp
inline CMFC_DEMODoc* CMFC_DEMOView::GetDocument()
   { return (CMFC_DEMODoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MFC_DEMOVIEW_H__B7D0AFEE_20A7_11D2_B1B0_0040053C38B6__INCLUDED_)

// MFC_DEMODoc.h : interface of the CMFC_DEMODoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MFC_DEMODOC_H__B7D0AFEC_20A7_11D2_B1B0_0040053C38B6__INCLUDED_)
#define AFX_MFC_DEMODOC_H__B7D0AFEC_20A7_11D2_B1B0_0040053C38B6__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <list>

#include "point.H"


using namespace std;

class CMFC_DEMODoc : public CDocument
{
	list<point> * pt_lst;

protected: // create from serialization only
	CMFC_DEMODoc();
	DECLARE_DYNCREATE(CMFC_DEMODoc)

// Attributes
public:
	list<point> * GetLst( void ) { return pt_lst; };

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMFC_DEMODoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMFC_DEMODoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CMFC_DEMODoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MFC_DEMODOC_H__B7D0AFEC_20A7_11D2_B1B0_0040053C38B6__INCLUDED_)

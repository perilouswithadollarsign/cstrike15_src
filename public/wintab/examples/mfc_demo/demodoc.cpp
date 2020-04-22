// MFC_DEMODoc.cpp : implementation of the CMFC_DEMODoc class
//

#include "stdafx.h"
#include "MFC_DEMO.h"
#include "point.h"

#include "DEMODoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMODoc

IMPLEMENT_DYNCREATE(CMFC_DEMODoc, CDocument)

BEGIN_MESSAGE_MAP(CMFC_DEMODoc, CDocument)
	//{{AFX_MSG_MAP(CMFC_DEMODoc)

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMODoc construction/destruction

CMFC_DEMODoc::CMFC_DEMODoc()
{
	pt_lst = new list<point>;
}

CMFC_DEMODoc::~CMFC_DEMODoc()
{
	delete pt_lst;
}

BOOL CMFC_DEMODoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// Clear the point list
	delete pt_lst;
	pt_lst = new list<point>;

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMODoc serialization

void CMFC_DEMODoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMODoc diagnostics

#ifdef _DEBUG
void CMFC_DEMODoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CMFC_DEMODoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMODoc commands

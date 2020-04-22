// MFC_DEMOView.cpp : implementation of the CMFC_DEMOView class
//

#include "stdafx.h"
#include "MFC_DEMO.h"

#include "DEMODoc.h"
#include "DEMOView.h"

#include <wintab.h>

#define PACKETDATA	PK_X | PK_Y | PK_BUTTONS
#define PACKETMODE	0
#include <pktdef.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMOView

IMPLEMENT_DYNCREATE(CMFC_DEMOView, CView)

BEGIN_MESSAGE_MAP(CMFC_DEMOView, CView)
	ON_MESSAGE(WT_PACKET, OnWTPacket)
	//{{AFX_MSG_MAP(CMFC_DEMOView)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMOView construction/destruction

CMFC_DEMOView::CMFC_DEMOView()
{
	csr.x = -1;
	prev_pkButtons = 0;
	pWTMutex = new CMutex( TRUE, NULL, NULL );
	hCtx = 0;
}

CMFC_DEMOView::~CMFC_DEMOView()
{
	delete pWTMutex;
	if( hCtx )
		WTClose( hCtx );
}

BOOL CMFC_DEMOView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMOView drawing

void CMFC_DEMOView::OnDraw(CDC* pDC)
{
	CMFC_DEMODoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	csr.x = -1;

	list<point> * lst = pDoc->GetLst();
	list<point>::iterator i = lst->begin();
	while( i != lst->end() ) {
		if( i->x >= 0 )
			pDC->LineTo(i->x,i->y);
		else
			pDC->MoveTo(abs(i->x),abs(i->y));
		i++;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMOView diagnostics

#ifdef _DEBUG
void CMFC_DEMOView::AssertValid() const
{
	CView::AssertValid();
}

void CMFC_DEMOView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMFC_DEMODoc* CMFC_DEMOView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMFC_DEMODoc)));
	return (CMFC_DEMODoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMFC_DEMOView message handlers

LRESULT CMFC_DEMOView::OnWTPacket(WPARAM wSerial, LPARAM hCtx)
{
	// Read the packet
	PACKET pkt;
	WTPacket( (HCTX)hCtx, wSerial, &pkt );

	// Process packets in order, one at a time
	CSingleLock lock( pWTMutex, TRUE );

	CDC *pDC = GetDC();
	
	// Get window size
	RECT window_rect;
	GetWindowRect( &window_rect );
	POINT size;
	size.x = window_rect.right - window_rect.left;
	size.y = window_rect.bottom - window_rect.top;

	// Erase the old cursor
	if( csr.x >= 0 ) {
		CRgn r;
		r.CreateRectRgn( csr.x - 2, csr.y - 2, csr.x + 2, csr.y + 2 );
		pDC->InvertRgn( &r );
	}

	csr.x = (size.x * pkt.pkX) / lc.lcInExtX;
	csr.y = size.y - (size.y * pkt.pkY) / lc.lcInExtY;

	if( pkt.pkButtons ) {
		CMFC_DEMODoc *pDoc = GetDocument();
		list<point> * lst = pDoc->GetLst();

		if( prev_pkButtons ) {

			list<point>::iterator i = lst->end();
			i--;
			pDC->MoveTo(abs(i->x),abs(i->y));

			lst->push_back(csr);
			pDC->LineTo(csr);
		} else {
			POINT pt;
			pt.x = -csr.x;
			pt.y = -csr.y;
			lst->push_back(pt);
		}
	}

	prev_pkButtons = pkt.pkButtons;

	// Draw a new cursor
	CRgn r;
	r.CreateRectRgn( csr.x - 2, csr.y - 2, csr.x + 2, csr.y + 2 );
	pDC->InvertRgn( &r );

	ReleaseDC( pDC );

	return TRUE;
}

int CMFC_DEMOView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// Open a Wintab context

	// Get default context information
	WTInfo( WTI_DEFCONTEXT, 0, &lc );

	// Open the context
	lc.lcPktData = PACKETDATA;
	lc.lcPktMode = PACKETMODE;
	lc.lcOptions = CXO_MESSAGES;
	//hCtx = WTOpen( m_hWnd, &lc, TRUE );
	hCtx = WTOpen( m_hWnd, &lc, TRUE );
	return 0;
}

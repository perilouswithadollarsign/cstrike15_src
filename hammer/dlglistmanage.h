//====== Copyright c 1996-2009, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//

#ifndef DLGLISTMANAGE_H
#define DLGLISTMANAGE_H
#ifdef _WIN32
#pragma once
#endif

enum ListFontType_t
{
	LISTFONT_NORMAL,
	LISTFONT_DUPLICATE,
	LISTFONT_BOLD
};

class IDlgListManageBrowse
{
public:
	virtual bool HandleBrowse( CStringList &lstResult ) = 0;
};

class CColorListBox : public CListBox
{
public:

	CColorListBox( void ) { }

	void AddItemText( const char *lpszText, ListFontType_t type )
	{
		int nIndex = AddString( lpszText );
		if( CB_ERR != nIndex )
		{
			SetItemData( nIndex, type );
		}
	}

	void CreateFonts( CDC *pDC )
	{
		if ( m_NormalFont.m_hObject == NULL )
		{
			// Describe a 16-point truetype font of normal weight
			LOGFONT lf;
			memset(&lf, 0, sizeof(lf));
			lf.lfHeight = 16;
			lf.lfWeight = FW_NORMAL;
			lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
			
			if (!m_NormalFont.CreateFontIndirect(&lf))
				return;
		}

		// Create a bold font
		if ( m_BoldFont.m_hObject == NULL )
		{
			if ( m_NormalFont.m_hObject )
			{
				LOGFONT LogFont;
				m_NormalFont.GetLogFont(&LogFont);
				LogFont.lfWeight = FW_BOLD;
				m_BoldFont.CreateFontIndirect(&LogFont);
			}
		}
	}

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDIS ) 
	{

		CDC			dc;
		CRect		rcItem(lpDIS->rcItem);
		UINT		nIndex = lpDIS->itemID;
		COLORREF	rgbBkgnd = ::GetSysColor( (lpDIS->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW);		

		dc.Attach( lpDIS->hDC );
		
		// Blah
		CreateFonts( &dc );

		CBrush br( rgbBkgnd );
		dc.FillRect( rcItem, &br );
		if( lpDIS->itemState & ODS_FOCUS )
		{
			dc.DrawFocusRect( rcItem );
		}

		if ( nIndex != (UINT)-1 )
		{
			// The text color is stored as the item data.
			ListFontType_t type = (ListFontType_t) GetItemData( nIndex );
			CString str;
			GetText( nIndex, str );
			dc.SetBkColor( rgbBkgnd );
			dc.SetTextColor( RGB(0,0,0) );
			
			CFont* pOldFont = NULL;
			if ( type == LISTFONT_BOLD )
			{
				pOldFont = dc.SelectObject( &m_BoldFont );
			}
			else
			{
				pOldFont = dc.SelectObject( &m_NormalFont );

				if ( type == LISTFONT_DUPLICATE )
				{
					// TODO: We don't color this anymore!
					// dc.SetTextColor(RGB(255,0,0));
				}
			}

			dc.TextOut(rcItem.left + 2, rcItem.top + 2, str);
			dc.SelectObject(pOldFont);
		}
		dc.Detach();
	}

	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMIS ) 
	{
		// Set the item height. Get the DC, select the font for the
		// list box, and compute the average height.
		CClientDC   dc(this);
		TEXTMETRIC   tm;
		CFont* pFont = GetFont();
		CFont* pOldFont = dc.SelectObject(pFont);
		dc.GetTextMetrics(&tm);
		dc.SelectObject(pOldFont);
		lpMIS->itemHeight = tm.tmHeight + 4;   
	}

private:
	CFont	m_BoldFont;
	CFont	m_NormalFont;
};

class CDlgListManage : public CDialog
{
	// Construction
public:
	CDlgListManage( CWnd* pParent = NULL, IDlgListManageBrowse *pBrowseImpl = NULL, const CMapObjectList *pObjectList = NULL );   // standard constructor

	// Dialog Data
	enum { IDD = IDD_MANAGE_LIST_DIALOG };

	void SaveScriptChanges( void );

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void OnSize(UINT nType, int cx, int cy);
	void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	void OnBrowse();
	DECLARE_MESSAGE_MAP()

protected:
	CRect m_rcDialog;
	
	void PopulateScriptList( void );
	void UpdateScriptChanges( void );

	struct ResizeInfo_t
	{
		enum {
			RI_WIDTH	= 1 << 0,
			RI_HEIGHT	= 1 << 1,
			RI_LEFT		= 1 << 2,
			RI_TOP		= 1 << 3,

			// Combinations
			RI_WIDTH_AND_HEIGHT = RI_WIDTH | RI_HEIGHT,
			RI_TOP_AND_LEFT = RI_TOP | RI_LEFT
		};

		int flags;
		CRect rc;
	};
	CMap< int, int, ResizeInfo_t, ResizeInfo_t > m_ctlInfo;

	IDlgListManageBrowse *m_pBrowseImpl;
	
	CColorListBox m_ScriptList;
	
	const CMapObjectList *m_pObjectList;

	CUtlVector<CString>	m_vAdditions;
	CUtlVector<CString> m_vSubtractions;

public:
	afx_msg void OnBnClickedScriptListAdd();
	afx_msg void OnBnClickedScriptListRemove();
	afx_msg void OnBnClickedScriptListEdit();
};


#endif // #ifndef DLGLISTMANAGE_H

//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FADEDLG_H
#define FADEDLG_H
#pragma once

#include "resource.h"
#include "IconComboBox.h"

//=============================================================================
//
// Fade Create Dialog
//
class CFadeDlg : public CDialog
{
public:

	CFadeDlg( CWnd *pParent = NULL );

	//{{AFX_DATA( CFadeDlg )
	enum { IDD = IDD_FADE_DIALOG };
	unsigned int	m_nFadeMode;
	//}}AFX_DATA

	virtual BOOL OnInitDialog();

	//{{AFX_VIRTUAL( CFadeDlg )
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG( CFadeDlg )
	afx_msg void OnButtonFadeLow( void );
	afx_msg void OnButtonFadeMed( void );
	afx_msg void OnButtonFadeHigh( void );
	afx_msg void OnButtonFade360( void );
	afx_msg void OnButtonFadeLevel( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

#endif // FADEDLG_H

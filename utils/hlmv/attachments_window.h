//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ATTACHMENTS_WINDOW_H
#define ATTACHMENTS_WINDOW_H
#ifdef _WIN32
#pragma once
#endif


#ifndef INCLUDED_MXWINDOW
#include <mxtk/mxWindow.h>
#endif
#include <mxtk/mx.h>
#include "mxLineEdit2.h"
#include "mathlib/vector.h"


class ControlPanel;


class CAttachmentsWindow : public mxWindow
{
public:
	CAttachmentsWindow( ControlPanel* pParent );
	void Init( );

	void OnLoadModel();
	
	void OnTabSelected();
	void OnTabUnselected();
	
	virtual int handleEvent( mxEvent *event );


private:

	void OnSelChangeAttachmentList();

	void PopulateAttachmentsList();
	void PopulateBoneList();
	void UpdateStrings( bool bUpdateQC=true, bool bUpdateTranslation=true, bool bUpdateRotation=true );

	Vector GetCurrentTranslation();
	Vector GetCurrentRotation();


private:

	ControlPanel *m_pControlPanel;
	mxListBox *m_cAttachmentList;
	mxListBox *m_cBoneList;
	
	mxLineEdit2 *m_cTranslation;
	mxLineEdit2 *m_cRotation;
	mxLineEdit2 *m_cQCString;

	mxButton *m_bTranslateXSmallPlus;
	mxButton *m_bTranslateYSmallPlus;
	mxButton *m_bTranslateZSmallPlus;
	mxButton *m_bTranslateXSmallMinus;
	mxButton *m_bTranslateYSmallMinus;
	mxButton *m_bTranslateZSmallMinus;
	mxButton *m_bTranslateXLargePlus;
	mxButton *m_bTranslateYLargePlus;
	mxButton *m_bTranslateZLargePlus;
	mxButton *m_bTranslateXLargeMinus;
	mxButton *m_bTranslateYLargeMinus;
	mxButton *m_bTranslateZLargeMinus;

	mxButton *m_bRotateXSmallPlus;
	mxButton *m_bRotateYSmallPlus;
	mxButton *m_bRotateZSmallPlus;
	mxButton *m_bRotateXSmallMinus;
	mxButton *m_bRotateYSmallMinus;
	mxButton *m_bRotateZSmallMinus;
	mxButton *m_bRotateXLargePlus;
	mxButton *m_bRotateYLargePlus;
	mxButton *m_bRotateZLargePlus;
	mxButton *m_bRotateXLargeMinus;
	mxButton *m_bRotateYLargeMinus;
	mxButton *m_bRotateZLargeMinus;

};


#endif // ATTACHMENTS_WINDOW_H

//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: Defines the interface to the entity sprinkle tool.
//
//=============================================================================

#ifndef __TOOL_SPRINKLE_DLG_H
#define __TOOL_SPRINKLE_DLG_H
#pragma once


class KeyValues;


enum
{
	SPRINKLE_MODE_ADDITIVE = 0,		// adds to existing objects
	SPRINKLE_MODE_SUBTRACTIVE,		// will remove from existing objects
	SPRINKLE_MODE_REPLACE,			// nukes entire objects from area
	SPRINKLE_MODE_OVERWRITE,		// overwrites existing objects
};


// CEntitySprinkleDlg dialog

class CEntitySprinkleDlg : public CDialog
{
	DECLARE_DYNAMIC(CEntitySprinkleDlg)

public:
	CEntitySprinkleDlg();
	virtual ~CEntitySprinkleDlg();

	void SetSprinkleTypes( KeyValues *pSprinkleInfo );
	void GetGridSize( float &flGridXSize, float &flGridYSize );
	KeyValues *GetSprinkleType( );
	int GetSprinkleMode( );
	int GetSprinkleDensity( );
	bool UseDefinitionGridSize( );
	bool UseRandomYaw( );

// Dialog Data
	enum { IDD = IDD_ENTITY_SPRINKLE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnCbnSelchangeSprinkleMode();
	CComboBox m_SprinkleModeControl;
	CSliderCtrl m_SprinkleDensityControl;
	virtual BOOL OnInitDialog();
	CEdit m_GridOffsetXControl;
	CEdit m_GridOffsetYControl;
	CEdit m_GridSizeXControl;
	CEdit m_GridSizeYControl;
	afx_msg void OnBnClickedSprinkleUseGrid();
	CComboBox m_SprinkleTypeControl;
	CStatic m_SprinkleDensityDisplayControl;
	CButton m_DefinitionGridSizeControl;
	afx_msg void OnBnClickedSprinkleDefinitionGridSize();
	afx_msg void OnNMCustomdrawSprinkleDensity(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnClose();
	CButton m_RandomYawControl;
};


#endif // __TOOL_SPRINKLE_DLG_H
#pragma once

#include "resource.h"
#include "utlvector.h"
#include "VGuiWnd.h"
#include "matsys_controls\baseassetpicker.h"


namespace vgui
{
	class TextEntry;
	class Splitter;
	class Button;
}

class CModelBrowserPanel;
class CMDLPicker;


#define ID_FIND_ASSET	100


class CModelBrowser : public CDialog
{
	DECLARE_DYNAMIC(CModelBrowser)

public:
	CModelBrowser(CWnd* pParent = NULL);   // standard constructor
	virtual ~CModelBrowser();

	void SetUsedModelList( CUtlVector<AssetUsageInfo_t> &usedModels );

	void	SetModelName( const char *pModelName );
	void	GetModelName( char *pModelName, int length );
	void	GetSkin( int &nSkin );
	void	SetSkin( int nSkin );

// Dialog Data
	enum { IDD = IDD_MODEL_BROWSER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreTranslateMessage( MSG* pMsg ); 


	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	virtual BOOL OnInitDialog();

	void UpdateStatusLine();
	void SaveLoadSettings( bool bSave ); 
	void Resize( void );

	CVGuiPanelWnd	m_VGuiWindow;

	CMDLPicker		*m_pPicker;
	vgui::Button	*m_pButtonOK;
	vgui::Button	*m_pButtonCancel;
	vgui::TextEntry	*m_pStatusLine;

	void Show();
	void Hide();
	
};

#pragma once

#include "resource.h"
#include "VGuiWnd.h"

// CParticleBrowser dialog

namespace vgui
{
	class TextEntry;
	class Splitter;
	class Button;
}

class CParticleBrowserPanel;
class CParticlePicker;


class CParticleBrowser : public CDialog
{
	DECLARE_DYNAMIC(CParticleBrowser)

public:
	CParticleBrowser(CWnd* pParent = NULL);   // standard constructor
	virtual ~CParticleBrowser();

	void	SetParticleSysName( const char *pParticleName );
	void	GetParticleSysName( char *pParticleName, int length );

// Dialog Data
	enum { IDD = IDD_PARTICLE_BROWSER };

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

	CParticlePicker	*m_pPicker;
	vgui::Button	*m_pButtonOK;
	vgui::Button	*m_pButtonCancel;
	vgui::TextEntry	*m_pStatusLine;

	void Show();
	void Hide();
};
